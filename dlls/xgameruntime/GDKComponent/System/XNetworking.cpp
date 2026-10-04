/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XNetworking
 * 
 * Written by Weather
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "../../private.h"

#include <cstring>
#include <atomic>
#include <winhttp.h>
#include <new>
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdkc);
WINE_DECLARE_DEBUG_CHANNEL(gdk_session);

/* The host networking stack is available before the title starts. Xbox account
 * authentication is independent of network initialization. Keep stable storage
 * for middleware which retains the hint passed to its notification callback. */
static XNetworkingConnectivityHint g_connectivity_hint =
{
    XNetworkingConnectivityLevelHint::InternetAccess,
    XNetworkingConnectivityCostHint::Unrestricted,
    0,       /* ianaInterfaceType */
    TRUE,    /* networkInitialized */
    FALSE,   /* approachingDataLimit */
    FALSE,   /* overDataLimit */
    FALSE,   /* roaming */
};

static std::atomic_uint64_t g_connectivity_hint_token{1};
static std::atomic_uint64_t g_preferred_udp_token{1};

struct connectivity_registration
{
    struct list entry;
    LONG refs;
    UINT64 token;
    XNetworkingConnectivityHintChangedCallback *callback;
    void *context;
    IXThreadingImpl *threading;
    XTaskQueueHandle queue;
    bool removed, running;
    DWORD callback_thread;
};

static SRWLOCK connectivity_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE connectivity_done = CONDITION_VARIABLE_INIT;
static struct list connectivity_registrations = LIST_INIT(connectivity_registrations);

static void release_connectivity_registration( connectivity_registration *registration )
{
    if (InterlockedDecrement( &registration->refs )) return;
    if (registration->queue) registration->threading->XTaskQueueCloseHandle( registration->queue );
    if (registration->threading) registration->threading->Release();
    delete registration;
}

static void WINAPI connectivity_callback( void *context, BOOLEAN canceled )
{
    auto registration = static_cast<connectivity_registration *>(context);
    AcquireSRWLockExclusive( &connectivity_lock );
    TRACE_(gdk_session)( "Connectivity callback token %I64u, canceled %u, removed %u.\n",
                        registration->token, (unsigned)canceled, (unsigned)registration->removed );
    if (!canceled && !registration->removed)
    {
        registration->running = true;
        registration->callback_thread = GetCurrentThreadId();
        ReleaseSRWLockExclusive( &connectivity_lock );
        /* Keep the hint address stable for middleware which retains it. */
        registration->callback( registration->context, &g_connectivity_hint );
        AcquireSRWLockExclusive( &connectivity_lock );
        registration->running = false;
        WakeAllConditionVariable( &connectivity_done );
    }
    ReleaseSRWLockExclusive( &connectivity_lock );
    release_connectivity_registration( registration );
}

static void CALLBACK connectivity_threadpool_callback( PTP_CALLBACK_INSTANCE, void *context )
{
    connectivity_callback( context, FALSE );
}

static HRESULT submit_connectivity_callback( connectivity_registration *registration )
{
    HRESULT hr;
    TRACE_(gdk_session)( "Queue connectivity callback token %I64u.\n", registration->token );
    if (registration->queue)
        hr = registration->threading->XTaskQueueSubmitCallback( registration->queue, XTaskQueuePort::Completion,
                                                               registration, connectivity_callback );
    else
        hr = TrySubmitThreadpoolCallback( connectivity_threadpool_callback, registration, nullptr ) ?
                S_OK : HRESULT_FROM_WIN32(GetLastError());
    if (FAILED(hr)) release_connectivity_registration( registration );
    return hr;
}

static void fill_connectivity_hint( XNetworkingConnectivityHint *out )
{
    AcquireSRWLockShared( &connectivity_lock );
    *out = g_connectivity_hint;
    ReleaseSRWLockShared( &connectivity_lock );
}

static HRESULT WINAPI security_information_provider( XAsyncOp op, const XAsyncProviderData *data )
{
    IXThreadingImpl *threading;
    HRESULT hr = E_NOTIMPL;

    TRACE( "op %u, asyncBlock %p, bufferSize %Iu, buffer %p.\n",
            static_cast<unsigned int>(op), data->async, data->bufferSize, data->buffer );
    if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, IID_IXThreadingImpl, (void **)&threading )))
        return hr;

    switch (op)
    {
        case XAsyncOp::Begin:
            hr = threading->XAsyncSchedule( data->async, 0 );
            break;
        case XAsyncOp::DoWork:
            threading->XAsyncComplete( data->async, S_OK, sizeof(XNetworkingSecurityInformation) );
            hr = S_OK;
            break;
        case XAsyncOp::GetResult:
        {
            if (!data->buffer || data->bufferSize < sizeof(XNetworkingSecurityInformation))
            {
                hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
                break;
            }
            auto *securityInformation = static_cast<XNetworkingSecurityInformation *>(data->buffer);
            securityInformation->enabledHttpSecurityProtocolFlags = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1
                    | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
            securityInformation->thumbprintCount = 0;
            securityInformation->thumbprints = nullptr;
            hr = S_OK;
            break;
        }
        case XAsyncOp::Cancel:
        case XAsyncOp::Cleanup:
            hr = S_OK;
            break;
    }

    threading->Release();
    return hr;
}

static HRESULT WINAPI preferred_udp_port_provider( XAsyncOp op, const XAsyncProviderData *data )
{
    IXThreadingImpl *threading;
    HRESULT hr;

    TRACE( "op %u, asyncBlock %p, bufferSize %Iu, buffer %p.\n",
            static_cast<unsigned int>(op), data->async, data->bufferSize, data->buffer );
    if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, IID_IXThreadingImpl, (void **)&threading )))
        return hr;

    switch (op)
    {
        case XAsyncOp::Begin:
            hr = threading->XAsyncSchedule( data->async, 0 );
            break;
        case XAsyncOp::DoWork:
            threading->XAsyncComplete( data->async, S_OK, sizeof(UINT16) );
            hr = S_OK;
            break;
        case XAsyncOp::GetResult:
        {
            auto *port = static_cast<UINT16 *>(data->buffer);
            *port = 3074;
            hr = S_OK;
            break;
        }
        case XAsyncOp::Cancel:
        case XAsyncOp::Cleanup:
            hr = S_OK;
            break;
    }

    threading->Release();
    return hr;
}


class XNetworkingImpl :
    public IXNetworkingImpl2
{
public:
    HRESULT WINAPI QueryInterface( REFIID iid, void **out )
    {
        TRACE( "iface %p, iid %s, out %p.\n", this, debugstr_guid( &iid ), out );

        if (!out) return E_POINTER;
        *out = nullptr;

        if ( iid == __uuidof( IUnknown ) ||
             iid == __uuidof( IInspectable ) ||
             iid == __uuidof( IAgileObject ) ||
             iid == __uuidof( IXNetworkingImpl ) ||
             iid == __uuidof( IXNetworkingImpl2 ) )
        {
            AddRef();
            *out = static_cast<IXNetworkingImpl2 *>(this);
            return S_OK;
        }

        FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( &iid ) );
        *out = nullptr;
        return E_NOINTERFACE;
    }

    ULONG WINAPI
    AddRef() noexcept override
    {
        ULONG curr = static_cast<ULONG>(++ref);
        TRACE( "iface %p increasing refcount to %lu.\n", this, curr );
        return curr;
    }

    ULONG WINAPI
    Release() noexcept override
    {
        ULONG curr = static_cast<ULONG>(--ref);
        TRACE( "iface %p decreasing refcount to %lu.\n", this, curr );
        return curr;
    }

    HRESULT WINAPI XNetworkingQueryPreferredLocalUdpMultiplayerPort( UINT16 *preferredLocalUdpMultiplayerPort ) override
    {
        TRACE( "preferredLocalUdpMultiplayerPort %p\n", preferredLocalUdpMultiplayerPort );
        if (!preferredLocalUdpMultiplayerPort) return E_POINTER;
        /* Well-known Xbox Live multiplayer UDP port (also used on Windows/PC GDK). */
        *preferredLocalUdpMultiplayerPort = 3074;
        return S_OK;
    }

    HRESULT WINAPI XNetworkingQueryPreferredLocalUdpMultiplayerPortAsync( XAsyncBlock *asyncBlock ) override
    {
        IXThreadingImpl *threading;
        HRESULT hr;

        TRACE( "asyncBlock %p\n", asyncBlock );
        if (!asyncBlock) return E_POINTER;
        if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, IID_IXThreadingImpl, (void **)&threading )))
            return hr;
        hr = threading->XAsyncBegin( asyncBlock, nullptr, nullptr,
                "XNetworkingQueryPreferredLocalUdpMultiplayerPortAsync", preferred_udp_port_provider );
        threading->Release();
        return hr;
    }

    HRESULT WINAPI XNetworkingQueryPreferredLocalUdpMultiplayerPortAsyncResult( XAsyncBlock *asyncBlock, UINT16 *preferredLocalUdpMultiplayerPort ) override
    {
        IXThreadingImpl *threading;
        HRESULT hr;

        TRACE( "asyncBlock %p, preferredLocalUdpMultiplayerPort %p\n", asyncBlock, preferredLocalUdpMultiplayerPort );
        if (!asyncBlock || !preferredLocalUdpMultiplayerPort) return E_POINTER;
        if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, IID_IXThreadingImpl, (void **)&threading )))
            return hr;
        hr = threading->XAsyncGetResult( asyncBlock, nullptr, sizeof(UINT16), preferredLocalUdpMultiplayerPort, nullptr );
        threading->Release();
        return hr;
    }

    HRESULT WINAPI XNetworkingRegisterPreferredLocalUdpMultiplayerPortChanged( XTaskQueueHandle queue, PVOID context, XNetworkingPreferredLocalUdpMultiplayerPortChangedCallback *callback, XTaskQueueRegistrationToken *token ) override
    {
        TRACE( "queue %p, context %p, callback %p, token %p\n", queue, context, callback, token );
        if (!callback || !token) return E_POINTER;
        token->token = g_preferred_udp_token.fetch_add( 1 );
        /* Port is fixed on this stub; fire once so titles that wait on the callback proceed. */
        callback( context, (UINT64)3074 );
        return S_OK;
    }

    BOOLEAN WINAPI XNetworkingUnregisterPreferredLocalUdpMultiplayerPortChanged( XTaskQueueRegistrationToken token, BOOLEAN wait ) override
    {
        TRACE( "token %I64u, wait %d\n", (UINT64)token.token, wait );
        return TRUE;
    }

    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlAsync( LPCSTR url, XAsyncBlock *asyncBlock ) override
    {
        IXThreadingImpl *threading;
        HRESULT hr;

        TRACE( "url %s, asyncBlock %p.\n", debugstr_a( url ), asyncBlock );
        if (!url || !asyncBlock) return E_POINTER;
        if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, IID_IXThreadingImpl, (void **)&threading )))
            return hr;
        hr = threading->XAsyncBegin( asyncBlock, nullptr, nullptr,
                "XNetworkingQuerySecurityInformationForUrlAsync", security_information_provider );
        threading->Release();
        return hr;
    }

    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlAsyncResultSize( XAsyncBlock *asyncBlock, SIZE_T *securityInformationBufferByteCount ) override
    {
        IXThreadingImpl *threading;
        HRESULT hr;

        TRACE( "asyncBlock %p, securityInformationBufferByteCount %p.\n", asyncBlock, securityInformationBufferByteCount );
        if (!asyncBlock || !securityInformationBufferByteCount) return E_POINTER;
        if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, IID_IXThreadingImpl, (void **)&threading )))
            return hr;
        hr = threading->XAsyncGetResultSize( asyncBlock, securityInformationBufferByteCount );
        threading->Release();
        return hr;
    }

    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlAsyncResult( XAsyncBlock *asyncBlock, SIZE_T securityInformationBufferByteCount, SIZE_T *securityInformationBufferByteCountUsed, UINT8 *securityInformationBuffer, XNetworkingSecurityInformation **securityInformation ) override
    {
        IXThreadingImpl *threading;
        HRESULT hr;

        TRACE( "asyncBlock %p, securityInformationBufferByteCount %Iu, securityInformationBufferByteCountUsed %p, securityInformationBuffer %p, securityInformation %p.\n",
                asyncBlock, securityInformationBufferByteCount, securityInformationBufferByteCountUsed, securityInformationBuffer, securityInformation );
        if (!securityInformation) return E_POINTER;
        *securityInformation = nullptr;
        if (!asyncBlock || !securityInformationBuffer) return E_POINTER;
        if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, IID_IXThreadingImpl, (void **)&threading )))
            return hr;
        if (SUCCEEDED(hr = threading->XAsyncGetResult( asyncBlock, nullptr, securityInformationBufferByteCount,
                securityInformationBuffer, securityInformationBufferByteCountUsed )))
            *securityInformation = reinterpret_cast<XNetworkingSecurityInformation *>(securityInformationBuffer);
        threading->Release();
        return hr;
    }

    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlUtf16Async( LPCWSTR url, XAsyncBlock *asyncBlock ) override
    {
        IXThreadingImpl *threading;
        HRESULT hr;

        TRACE( "url %s, asyncBlock %p.\n", debugstr_w( url ), asyncBlock );
        if (!url || !asyncBlock) return E_POINTER;
        if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, IID_IXThreadingImpl, (void **)&threading )))
            return hr;
        hr = threading->XAsyncBegin( asyncBlock, nullptr, nullptr,
                "XNetworkingQuerySecurityInformationForUrlUtf16Async", security_information_provider );
        threading->Release();
        return hr;
    }

    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlUtf16AsyncResultSize( XAsyncBlock *asyncBlock, SIZE_T *securityInformationBufferByteCount ) override
    {
        return XNetworkingQuerySecurityInformationForUrlAsyncResultSize( asyncBlock, securityInformationBufferByteCount );
    }

    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlUtf16AsyncResult( XAsyncBlock *asyncBlock, SIZE_T securityInformationBufferByteCount, SIZE_T *securityInformationBufferByteCountUsed, UINT8 *securityInformationBuffer, XNetworkingSecurityInformation **securityInformation ) override
    {
        return XNetworkingQuerySecurityInformationForUrlAsyncResult( asyncBlock, securityInformationBufferByteCount,
                securityInformationBufferByteCountUsed, securityInformationBuffer, securityInformation );
    }

    HRESULT WINAPI XNetworkingVerifyServerCertificate( PVOID requestHandle, const XNetworkingSecurityInformation *securityInformation ) override
    {
        TRACE( "requestHandle %p, securityInformation %p\n", requestHandle, securityInformation );
        return S_OK;
    }

    HRESULT WINAPI XNetworkingGetConnectivityHint( XNetworkingConnectivityHint *connectivityHint ) override
    {
        if (!connectivityHint) return E_POINTER;
        fill_connectivity_hint( connectivityHint );
        TRACE( "connectivityHint %p -> level=%u cost=%u initialized=%u\n",
               connectivityHint, (unsigned)connectivityHint->connectivityLevel,
               (unsigned)connectivityHint->connectivityCost, (unsigned)connectivityHint->networkInitialized );
        return S_OK;
    }

    HRESULT WINAPI XNetworkingRegisterConnectivityHintChanged( XTaskQueueHandle queue, PVOID context, XNetworkingConnectivityHintChangedCallback *callback, XTaskQueueRegistrationToken *token ) override
    {
        TRACE( "queue %p, context %p, callback %p, token %p (initialized=%u)\n",
               queue, context, callback, token, (unsigned)g_connectivity_hint.networkInitialized );

        if (!callback || !token) return E_POINTER;

        token->token = 0;
        auto registration = new (std::nothrow) connectivity_registration{};
        if (!registration) return E_OUTOFMEMORY;
        registration->refs = 1;
        registration->token = g_connectivity_hint_token.fetch_add( 1 );
        registration->callback = callback;
        registration->context = context;
        if (queue)
        {
            HRESULT hr = QueryApiImpl( &CLSID_XThreadingImpl, IID_IXThreadingImpl,
                                      (void **)&registration->threading );
            if (SUCCEEDED(hr)) hr = registration->threading->XTaskQueueDuplicateHandle( queue, &registration->queue );
            if (FAILED(hr)) { release_connectivity_registration( registration ); return hr; }
        }
        AcquireSRWLockExclusive( &connectivity_lock );
        list_add_tail( &connectivity_registrations, &registration->entry );
        /* Each registration receives an initial notification on its queue. */
        InterlockedIncrement( &registration->refs );
        token->token = registration->token;
        ReleaseSRWLockExclusive( &connectivity_lock );
        HRESULT hr = submit_connectivity_callback( registration );
        if (FAILED(hr))
        {
            XNetworkingUnregisterConnectivityHintChanged( *token, TRUE );
            token->token = 0;
            return hr;
        }
        return S_OK;
    }

    BOOLEAN WINAPI XNetworkingUnregisterConnectivityHintChanged( XTaskQueueRegistrationToken token, BOOLEAN wait ) override
    {
        connectivity_registration *registration, *found = nullptr;
        TRACE( "token %I64u, wait %d\n", (UINT64)token.token, wait );
        AcquireSRWLockExclusive( &connectivity_lock );
        LIST_FOR_EACH_ENTRY( registration, &connectivity_registrations, connectivity_registration, entry )
            if (registration->token == token.token) { found = registration; break; }
        if (found)
        {
            list_remove( &found->entry );
            found->removed = true;
            /* A callback may unregister itself without waiting on its own return. */
            while (wait && found->running && found->callback_thread != GetCurrentThreadId())
                SleepConditionVariableSRW( &connectivity_done, &connectivity_lock, INFINITE, 0 );
        }
        ReleaseSRWLockExclusive( &connectivity_lock );
        if (found) release_connectivity_registration( found );
        return found != nullptr;
    }

    HRESULT WINAPI XNetworkingQueryConfigurationSetting( XNetworkingConfigurationSetting configurationSetting, UINT64 *value ) override
    {
        TRACE( "configurationSetting %u, value %p\n", (unsigned)configurationSetting, value );
        if (!value) return E_POINTER;
        /* Sensible GDK defaults (1 MiB title TCP queued receive buffer). */
        switch (configurationSetting)
        {
        case XNetworkingConfigurationSetting::MaxTitleTcpQueuedReceiveBufferSize:
        case XNetworkingConfigurationSetting::MaxSystemTcpQueuedReceiveBufferSize:
        case XNetworkingConfigurationSetting::MaxToolsTcpQueuedReceiveBufferSize:
            *value = 1024ull * 1024ull;
            return S_OK;
        default:
            *value = 0;
            return E_INVALIDARG;
        }
    }

    HRESULT WINAPI XNetworkingSetConfigurationSetting( XNetworkingConfigurationSetting configurationParameter, UINT64 value ) override
    {
        TRACE( "configurationParameter %u, value %I64u\n", (unsigned)configurationParameter, value );
        return S_OK;
    }

    HRESULT WINAPI XNetworkingQueryStatistics( XNetworkingStatisticsType statisticsType, XNetworkingStatisticsBuffer *statisticsBuffer ) override
    {
        TRACE( "statisticsType %u, statisticsBuffer %p\n", (unsigned)statisticsType, statisticsBuffer );
        if (!statisticsBuffer) return E_POINTER;
        memset( statisticsBuffer, 0, sizeof(*statisticsBuffer) );
        return S_OK;
    }

private:
    std::atomic_long ref{ 1 };
};

static XNetworkingImpl g_x_networking;

IXNetworkingImpl *x_networking = static_cast<IXNetworkingImpl*>(&g_x_networking);
