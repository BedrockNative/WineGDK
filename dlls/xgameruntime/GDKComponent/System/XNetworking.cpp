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
#include <thread>
#include <chrono>
#include <winhttp.h>

WINE_DEFAULT_DEBUG_CHANNEL(gdkc);

/* Stable storage: PlayFab Multiplayer may keep the connectivityHint pointer from
 * RegisterConnectivityHintChanged after the register call returns. */
static XNetworkingConnectivityHint g_connectivity_hint =
{
    XNetworkingConnectivityLevelHint::Unknown,
    XNetworkingConnectivityCostHint::Unknown,
    0,       /* ianaInterfaceType */
    FALSE,   /* networkInitialized — must stay false until Xbox user auth is up */
    FALSE,   /* approachingDataLimit */
    FALSE,   /* overDataLimit */
    FALSE,   /* roaming */
};

static std::atomic_uint64_t g_connectivity_hint_token{1};
static std::atomic_uint64_t g_preferred_udp_token{1};
static std::atomic_bool g_connectivity_bringup_started{false};
static std::atomic_bool g_connectivity_initialized{false};

static XNetworkingConnectivityHintChangedCallback *g_connectivity_callback;
static void *g_connectivity_callback_context;
static UINT64 g_connectivity_callback_token;

static void fill_connectivity_hint( XNetworkingConnectivityHint *out )
{
    *out = g_connectivity_hint;
}

static void mark_network_initialized_and_notify( void )
{
    g_connectivity_hint.connectivityLevel = XNetworkingConnectivityLevelHint::InternetAccess;
    g_connectivity_hint.connectivityCost = XNetworkingConnectivityCostHint::Unrestricted;
    g_connectivity_hint.networkInitialized = TRUE;
    g_connectivity_initialized.store( true );

    XNetworkingConnectivityHintChangedCallback *callback = g_connectivity_callback;
    void *context = g_connectivity_callback_context;
    TRACE( "networkInitialized -> TRUE, callback %p token %I64u\n", callback, g_connectivity_callback_token );
    if (callback)
        callback( context, &g_connectivity_hint );
}

/* Defer bring-up so titles finish XUserAdd / XSTS before PeopleHub/RTA start.
 * Firing initialized=TRUE synchronously during RegisterConnectivityHintChanged made
 * Minecraft hit peoplehub before auth and get HTTP 500 (friends list empty). */
static void ensure_connectivity_bringup( void )
{
    bool expected = false;
    if (!g_connectivity_bringup_started.compare_exchange_strong( expected, true ))
        return;

    std::thread( []
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 2500 ) );
        mark_network_initialized_and_notify();
    } ).detach();
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
        TRACE( "connectivityHint %p -> level=%u cost=%u initialized=%u\n",
               connectivityHint,
               (unsigned)g_connectivity_hint.connectivityLevel,
               (unsigned)g_connectivity_hint.connectivityCost,
               (unsigned)g_connectivity_hint.networkInitialized );

        if (!connectivityHint) return E_POINTER;
        ensure_connectivity_bringup();
        fill_connectivity_hint( connectivityHint );
        return S_OK;
    }

    HRESULT WINAPI XNetworkingRegisterConnectivityHintChanged( XTaskQueueHandle queue, PVOID context, XNetworkingConnectivityHintChangedCallback *callback, XTaskQueueRegistrationToken *token ) override
    {
        TRACE( "queue %p, context %p, callback %p, token %p (initialized=%u)\n",
               queue, context, callback, token, (unsigned)g_connectivity_hint.networkInitialized );

        if (!callback || !token) return E_POINTER;

        token->token = g_connectivity_hint_token.fetch_add( 1 );
        g_connectivity_callback = callback;
        g_connectivity_callback_context = context;
        g_connectivity_callback_token = token->token;

        ensure_connectivity_bringup();

        /* If bring-up already finished, notify now with the stable global hint. */
        if (g_connectivity_initialized.load())
            callback( context, &g_connectivity_hint );

        return S_OK;
    }

    BOOLEAN WINAPI XNetworkingUnregisterConnectivityHintChanged( XTaskQueueRegistrationToken token, BOOLEAN wait ) override
    {
        TRACE( "token %I64u, wait %d\n", (UINT64)token.token, wait );
        if (g_connectivity_callback_token == token.token)
        {
            g_connectivity_callback = nullptr;
            g_connectivity_callback_context = nullptr;
            g_connectivity_callback_token = 0;
        }
        return TRUE;
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

    HRESULT WINAPI XNetworkingQueryConfigurationSetting(XNetworkingConfigurationSetting setting, UINT64 *value) override
    {
        if (!value) return E_POINTER;
        *value = 0;
        return E_NOTIMPL;
    }

    HRESULT WINAPI XNetworkingSetConfigurationSetting(XNetworkingConfigurationSetting setting, UINT64 value) override
    {
        return E_NOTIMPL;
    }

    HRESULT WINAPI XNetworkingQueryStatistics(XNetworkingStatisticsType type, XNetworkingStatisticsBuffer *buffer) override
    {
        if (!buffer) return E_POINTER;
        memset(buffer, 0, sizeof(*buffer));
        return E_NOTIMPL;
    }

private:
    std::atomic_long ref{ 1 };
};

static XNetworkingImpl g_x_networking;

IXNetworkingImpl *x_networking = static_cast<IXNetworkingImpl*>(&g_x_networking);
