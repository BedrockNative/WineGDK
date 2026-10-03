/*
 * Xbox Game runtime Library
 *  Xodus Interopability Layer -> XodusService
 * 
 * Written by Weather
 * Copyright 2026 Olivia Ryan
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
#include "../../WineCoreUAP/Foundation/IWineAsync.hpp"
#include "Structs.h"

#include "robuffer.h"
#include <atomic>
#include <new>

WINE_DEFAULT_DEBUG_CHANNEL(xodus);

using namespace ABI;
using namespace ABI::Xodus;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;

class ABI::Xodus::XodusService :
    public IXodusService
{
public:
    /* IUnknown Methods */
    HRESULT WINAPI 
    QueryInterface( REFIID iid, void **out )
    {
        TRACE( "iface %p, iid %s, out %p.\n", this, debugstr_guid( &iid ), out );

        if (!out) return E_POINTER;
        *out = nullptr;

        if ( iid == __uuidof( IUnknown ) ||
             iid == __uuidof( IInspectable ) ||
             iid == __uuidof( IAgileObject ) ||
             iid == __uuidof( IXodusService ) )
        {
            AddRef();
            *out = static_cast<IXodusService *>(this);
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

        // Polymorphic classes should not be deleted.
        /*
        if ( !curr )
            delete this;
        */

        return curr;
    }

    /* IInspectable Methods */
    HRESULT WINAPI
    GetIids( ULONG *iidCount, IID **iids ) override
    {
        FIXME("iface %p, iidCount %p, iids %p stub!\n", this, iidCount, iids);
        return E_NOTIMPL;
    }

    HRESULT WINAPI
    GetRuntimeClassName( HSTRING *className ) override 
    {
        FIXME("iface %p, className %p stub!\n", this, className);
        return E_NOTIMPL;
    }

    HRESULT WINAPI
    GetTrustLevel( TrustLevel *trustLevel ) override
    {
        FIXME("iface %p, trustLevel %p stub!\n", this, trustLevel);
        return E_NOTIMPL;
    }

    /* IXodusService Methods */
    HRESULT WINAPI
    Ping( IAsyncAction **operation ) override
    {
        TRACE("operation %p.\n", operation);
        if (!operation) return E_POINTER;
        *operation = nullptr;
        return AsyncAction::Create( static_cast<IUnknown *>(this), nullptr, PingAsync, operation );
    }

    HRESULT WINAPI
    MsaTokenRequest( const char *clientId, boolean allowUi, boolean fullTrust, IAsyncOperation<IMsaTokenResponse *> **operation ) override
    {
        HRESULT hr;
        MsaTokenRequestContext *ctx;

        TRACE( "clientId %s, operation %p.\n", debugstr_a( clientId ), operation );
        if (!operation) return E_POINTER;
        *operation = nullptr;
        if (!clientId) return E_INVALIDARG;
        if (!(ctx = new (std::nothrow) MsaTokenRequestContext{})) return E_OUTOFMEMORY;
        if (!(ctx->clientId = strdup( clientId )))
        {
            delete ctx;
            return E_OUTOFMEMORY;
        }
        ctx->allowUi = allowUi;
        ctx->fullTrust = fullTrust;
        hr = AsyncOperation<IMsaTokenResponse *>::Create( static_cast<IUnknown *>(this), ctx,
                                                         MsaTokenRequestAsync, operation );
        if (FAILED(hr))
        {
            free( ctx->clientId );
            delete ctx;
        }
        return hr;
    }

private:

    static HRESULT WINAPI
    PingAsync( IUnknown *invoker, PVOID param, PROPVARIANT *result )
    {
        DWORD ret;
        UINT16 messageType;
        HRESULT status;
        HSTRING bufferClass = nullptr;

        IXodusIPCPacket *xodusPacket = nullptr;
        IBuffer *message = nullptr;
        IBufferFactory *bufferFactory = nullptr;
        IAsyncOperation<IXodusIPCPacket *> *response = nullptr;

        TRACE("invoker %p, param %p, result %p\n", invoker, param, result);

        status = WindowsCreateString( RuntimeClass_Windows_Storage_Streams_Buffer, lstrlenW( RuntimeClass_Windows_Storage_Streams_Buffer ), &bufferClass );
        if ( FAILED( status ) ) goto _CLEANUP;

        status = RoGetActivationFactory( bufferClass, __uuidof( IBufferFactory ), (void **)&bufferFactory );
        if ( FAILED( status ) ) goto _CLEANUP;

        status = bufferFactory->Create( 1, &message );
        if ( FAILED( status ) ) goto _CLEANUP;

        // Construct a new IPC Packet
        xodusPacket = new (std::nothrow) XodusIPCPacket(
            MagicHeaderType::XML,
            1 /* PING */,
            message
        );

        if (!xodusPacket)
        {
            status = E_OUTOFMEMORY;
            goto _CLEANUP;
        }
        status = xodus_ipclayer->SendRequestAsync( xodusPacket, &response );
        if (FAILED(status)) goto _CLEANUP;

        ret = AsyncOperationCompletedHandler<IXodusIPCPacket *>::await_AsyncOperation( response, INFINITE );
        if ( ret )
        {
            status = E_FAIL;
            goto _CLEANUP;
        }

        xodusPacket->Release();
        xodusPacket = nullptr;

        message->Release();
        message = nullptr;

        // confirm that we actually PONGed
        status = response->GetResults( &xodusPacket );
        if (FAILED(status)) goto _CLEANUP;
        status = xodusPacket->get_MessageType( &messageType );
        if (SUCCEEDED(status) && messageType != 2 /* PONG */)
            status = E_INVALIDARG;
        
_CLEANUP:
        if ( bufferClass ) WindowsDeleteString( bufferClass );
        if ( bufferFactory ) bufferFactory->Release();
        if ( message ) message->Release();
        if ( response ) response->Release();
        if ( xodusPacket ) xodusPacket->Release();

        return status;
    }

    static HRESULT WINAPI
    MsaTokenRequestAsync( IUnknown *invoker, PVOID param, PROPVARIANT *result )
    {
        IBufferByteAccess *requestByteAccess = nullptr, *responseByteAccess = nullptr;
        IXodusIPCPacket *requestPacket = nullptr, *responsePacket = nullptr;
        IBuffer *requestMessage = nullptr, *responseMessage = nullptr;
        auto ctx = static_cast<MsaTokenRequestContext *>(param);
        IAsyncOperation<IXodusIPCPacket *> *asyncop = nullptr;
        HSTRING_HEADER classNameHeader;
        IMsaTokenResponse *token = nullptr;
        IBufferFactory *factory = nullptr;
        UINT16 messageType = 3; /* MSA_TOKEN_REQUEST */
        char *xml = nullptr;
        HSTRING className;
        BYTE *buffer;
        HRESULT hr;

        if (FAILED(hr = WindowsCreateStringReference( RuntimeClass_Windows_Storage_Streams_Buffer,
                                                      wcslen( RuntimeClass_Windows_Storage_Streams_Buffer ),
                                                      &classNameHeader, &className ))) goto cleanup;

        if (FAILED(hr = RoGetActivationFactory( className, __uuidof( IBufferFactory ), (void **)&factory ))) goto cleanup;
        if (FAILED(hr = xodus_xml_builder->BuildMsaTokenRequestXml( ctx->clientId, ctx->allowUi, ctx->fullTrust, &xml ))) goto cleanup;
        if (FAILED(hr = factory->Create( strlen( xml ) + 1, &requestMessage ))) goto cleanup;
        if (FAILED(hr = requestMessage->QueryInterface<IBufferByteAccess>( &requestByteAccess ))) goto cleanup;
        if (FAILED(hr = requestByteAccess->Buffer( &buffer ))) goto cleanup;
        if (FAILED(hr = requestMessage->put_Length( strlen( xml ) + 1 ))) goto cleanup;
        memcpy( buffer, xml, strlen( xml ) + 1 );

        /* Construct a new IPC Packet */
        requestPacket = new XodusIPCPacket( MagicHeaderType::XML, messageType, requestMessage );
        if (FAILED(hr = xodus_ipclayer->SendRequestAsync( requestPacket, &asyncop )) || !asyncop)
        {
            if (SUCCEEDED(hr)) hr = E_FAIL;
            goto cleanup;
        }

        if (AsyncOperationCompletedHandler<IXodusIPCPacket *>::await_AsyncOperation( asyncop, INFINITE ))
        {
            hr = E_FAIL;
            goto cleanup;
        }

        if (FAILED(hr = asyncop->GetResults( &responsePacket ))) goto cleanup;
        /* OrionBE: a timed out request completes with S_OK and a NULL packet — don't dereference it. */
        if (!responsePacket)
        {
            WARN( "Xodus MSA token request returned no response (timed out?).\n" );
            hr = HRESULT_FROM_NT( STATUS_TIMEOUT );
            goto cleanup;
        }
        responsePacket->get_MessageType( &messageType );
        if (messageType != 4 /* MSA_TOKEN_RESPONSE */)
        {
            hr = E_FAIL;
            goto cleanup;
        }

        if (FAILED(hr = responsePacket->get_Message( &responseMessage ))) goto cleanup;
        if (FAILED(hr = responseMessage->QueryInterface<IBufferByteAccess>( &responseByteAccess ))) goto cleanup;
        if (FAILED(hr = responseByteAccess->Buffer( &buffer ))) goto cleanup;
        if (FAILED(hr = xodus_xml_builder->FromMsaTokenResponseXml( reinterpret_cast<char *>(buffer), &token ))) goto cleanup;

        result->vt = VT_UNKNOWN;
        result->punkVal = token;

    cleanup:
        if (responseByteAccess) responseByteAccess->Release();
        if (requestByteAccess) requestByteAccess->Release();
        if (responseMessage) responseMessage->Release();
        if (requestMessage) requestMessage->Release();
        if (responsePacket) responsePacket->Release();
        if (requestPacket) requestPacket->Release();
        if (asyncop) asyncop->Release();
        if (xml) free( xml );
        if (factory) factory->Release();
        free( ctx->clientId );
        delete ctx;
        return hr;
    }

    std::atomic_long ref{ 1 };
};

static XodusService g_xodus_service;
IXodusService *xodus_service = static_cast<IXodusService*>(&g_xodus_service);
