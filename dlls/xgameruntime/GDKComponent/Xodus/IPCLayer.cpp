/*
 * Xbox Game runtime Library
 *  Xodus Interopability Layer -> IPCLayer
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
#include "../../WineCoreUAP/Foundation/IWineAsync.hpp"
#include "Structs.h"

#include <wine/list.h>
#include "ntstatus.h"
#include "robuffer.h"

#include <atomic>
#include <new>

WINE_DEFAULT_DEBUG_CHANNEL(xodus);

using namespace ABI;
using namespace ABI::Xodus;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;

class ABI::Xodus::IPCLayer :
    public IIPCLayer
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
             iid == __uuidof( IIPCLayer ) )
        {
            AddRef();
            *out = static_cast<IIPCLayer *>(this);
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

    /* IIPCLayer Methods */
    HRESULT WINAPI InitializeSocket() override
    {
        IAsyncAction *operation = nullptr;
        HRESULT hr = AsyncAction::Create( static_cast<IUnknown *>(this), nullptr,
                                           InitializeSocketThread, &operation );
        if (operation) operation->Release();
        return hr;
    }

    HRESULT WINAPI SendRequestAsync( IXodusIPCPacket *packet,
                                     IAsyncOperation<IXodusIPCPacket *> **operation ) override
    {
        HRESULT hr;
        if (!operation) return E_POINTER;
        *operation = nullptr;
        if (!packet) return E_INVALIDARG;
        packet->AddRef();
        hr = AsyncOperation<IXodusIPCPacket *>::Create( static_cast<IUnknown *>(this),
                                                       packet, SendRequest, operation );
        if (FAILED(hr)) packet->Release();
        return hr;
    }

    HRESULT WINAPI add_ResponseReceived( IIPCResponseHandler *handler, EventRegistrationToken *token ) override
    {
        response_received_callback *callback;
        if (!handler || !token) return E_POINTER;
        if (!(callback = new (std::nothrow) response_received_callback{})) return E_OUTOFMEMORY;
        callback->handler = handler;
        handler->AddRef();
        callback->token = token->value = m_NextEventToken++;
        AcquireSRWLockExclusive( &m_CallbackLock );
        list_add_head( &m_Callbacks, &callback->entry );
        ReleaseSRWLockExclusive( &m_CallbackLock );
        return S_OK;
    }

    HRESULT WINAPI remove_ResponseReceived( EventRegistrationToken token ) override
    {
        response_received_callback *callback;
        AcquireSRWLockExclusive( &m_CallbackLock );
        LIST_FOR_EACH_ENTRY( callback, &m_Callbacks, response_received_callback, entry )
        {
            if (callback->token != token.value) continue;
            list_remove( &callback->entry );
            ReleaseSRWLockExclusive( &m_CallbackLock );
            callback->handler->Release();
            delete callback;
            return S_OK;
        }
        ReleaseSRWLockExclusive( &m_CallbackLock );
        return E_BOUNDS;
    }

private:
    struct IPCHeader_CTYPE
    {
        MagicHeaderType Magic;
        UINT16 Message_Type;
        UINT16 MessageLength;
    };

    struct IPCFrame
    {
        UINT32 frameSize;
        BYTE *frame;
    };

    struct SendRequestContext
    {
        HANDLE event;
        IXodusIPCPacket *response;
        UINT16 expectedType;
    };

    static HRESULT WINAPI SendRequest( IUnknown *invoker, PVOID param, PROPVARIANT *result )
    {
        auto iface = static_cast<IPCLayer *>(invoker);
        auto packet = static_cast<IXodusIPCPacket *>(param);
        IBuffer *message = nullptr;
        IBufferByteAccess *access = nullptr;
        IPCResponseHandler *handler = nullptr;
        BYTE *messageBuffer = nullptr;
        IPCFrame frame{};
        IPCHeader_CTYPE header{};
        EventRegistrationToken token{};
        SendRequestContext context{};
        UINT32 length;
        DWORD wait;
        NTSTATUS status;
        HRESULT hr;
        bool registered = false;

        /* The protocol has no request IDs. Keep at most one request in flight. */
        AcquireSRWLockExclusive( &iface->m_RequestLock );
        if (FAILED(hr = packet->get_Magic( &header.Magic ))) goto cleanup;
        if (FAILED(hr = packet->get_MessageType( &header.Message_Type ))) goto cleanup;
        if (header.Message_Type != 1 && header.Message_Type != 3)
        {
            hr = E_INVALIDARG;
            goto cleanup;
        }
        context.expectedType = header.Message_Type + 1;
        if (FAILED(hr = packet->get_Message( &message ))) goto cleanup;
        if (!message)
        {
            hr = E_INVALIDARG;
            goto cleanup;
        }
        if (FAILED(hr = message->get_Length( &length ))) goto cleanup;
        if (length > 0xffff)
        {
            hr = HRESULT_FROM_WIN32( ERROR_INSUFFICIENT_BUFFER );
            goto cleanup;
        }
        if (FAILED(hr = message->QueryInterface<IBufferByteAccess>( &access ))) goto cleanup;
        if (FAILED(hr = access->Buffer( &messageBuffer ))) goto cleanup;
        if (length && !messageBuffer)
        {
            hr = E_INVALIDARG;
            goto cleanup;
        }
        header.MessageLength = length;
        frame.frameSize = sizeof(header) + length;
        if (!(frame.frame = static_cast<BYTE *>(CoTaskMemAlloc( frame.frameSize ))))
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }
        memcpy( frame.frame, &header, sizeof(header) );
        if (length) memcpy( frame.frame + sizeof(header), messageBuffer, length );
        if (!(context.event = CreateEventW( nullptr, TRUE, FALSE, nullptr )))
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            goto cleanup;
        }
        if (!(handler = new (std::nothrow) IPCResponseHandler( SendRequestResponseHandler, &context )))
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }
        if (FAILED(hr = iface->add_ResponseReceived( handler, &token ))) goto cleanup;
        registered = true;
        status = __wine_unix_call( unixhandle, send_frame, &frame );
        if (status)
        {
            hr = HRESULT_FROM_NT( status );
            goto cleanup;
        }
        wait = WaitForSingleObject( context.event, IPC_REQUEST_TIMEOUT_MS );
        if (wait == WAIT_OBJECT_0) hr = S_OK;
        else if (wait == WAIT_TIMEOUT) hr = HRESULT_FROM_WIN32( ERROR_TIMEOUT );
        else hr = HRESULT_FROM_WIN32( GetLastError() );

    cleanup:
        /* Removal waits for any running callback before its stack context disappears. */
        if (registered) iface->remove_ResponseReceived( token );
        if (SUCCEEDED(hr))
        {
            if (context.response)
            {
                result->vt = VT_UNKNOWN;
                result->punkVal = context.response;
                context.response = nullptr;
            }
            else hr = E_UNEXPECTED;
        }
        if (context.response) context.response->Release();
        if (handler) handler->Release();
        if (context.event) CloseHandle( context.event );
        CoTaskMemFree( frame.frame );
        if (access) access->Release();
        if (message) message->Release();
        packet->Release();
        ReleaseSRWLockExclusive( &iface->m_RequestLock );
        return hr;
    }

    static HRESULT WINAPI SendRequestResponseHandler( PVOID context, IXodusIPCPacket *packet )
    {
        auto ctx = static_cast<SendRequestContext *>(context);
        UINT16 type;
        HRESULT hr = packet->get_MessageType( &type );
        if (FAILED(hr)) return hr;
        if (type != ctx->expectedType || ctx->response) return S_OK;
        packet->AddRef();
        ctx->response = packet;
        SetEvent( ctx->event );
        return S_OK;
    }

    static HRESULT WINAPI InitializeSocketThread( IUnknown *invoker, PVOID param, PROPVARIANT *result )
    {
        auto iface = static_cast<IPCLayer *>(invoker);
        HSTRING_HEADER classHeader;
        HSTRING className;
        IBufferFactory *factory = nullptr;
        IBuffer *message = nullptr;
        IBufferByteAccess *access = nullptr;
        IXodusIPCPacket *packet = nullptr;
        POLL_SOCKET_ARGS poll{};
        response_received_callback *callback;
        BYTE *buffer;
        HRESULT hr;
        NTSTATUS status;
        SIZE_T offset;

        if (FAILED(hr = WindowsCreateStringReference( RuntimeClass_Windows_Storage_Streams_Buffer,
                wcslen( RuntimeClass_Windows_Storage_Streams_Buffer ), &classHeader, &className ))) return hr;
        if (FAILED(hr = RoGetActivationFactory( className, __uuidof(IBufferFactory), (void **)&factory ))) return hr;
        for (;;)
        {
            if ((status = __wine_unix_call( unixhandle, poll_socket, &poll )))
            {
                hr = HRESULT_FROM_NT( status );
                goto cleanup;
            }
            offset = 0;
            while (poll.curr_buffer_size - offset >= sizeof(IPCHeader_CTYPE))
            {
                IPCHeader_CTYPE header;
                memcpy( &header, poll.curr_buffer + offset, sizeof(header) );
                if (header.Magic != MagicHeaderType::XML)
                {
                    hr = E_INVALIDARG;
                    goto cleanup;
                }
                if (poll.curr_buffer_size - offset < sizeof(header) + header.MessageLength) break;
                if (FAILED(hr = factory->Create( header.MessageLength + 1, &message ))) goto cleanup;
                if (FAILED(hr = message->QueryInterface<IBufferByteAccess>( &access ))) goto cleanup;
                if (FAILED(hr = access->Buffer( &buffer ))) goto cleanup;
                if (!buffer)
                {
                    hr = E_OUTOFMEMORY;
                    goto cleanup;
                }
                memcpy( buffer, poll.curr_buffer + offset + sizeof(header), header.MessageLength );
                buffer[header.MessageLength] = 0;
                if (FAILED(hr = message->put_Length( header.MessageLength ))) goto cleanup;
                offset += sizeof(header) + header.MessageLength;
                if (!(packet = new (std::nothrow) XodusIPCPacket( header.Magic, header.Message_Type, message )))
                {
                    hr = E_OUTOFMEMORY;
                    goto cleanup;
                }
                /* Keep callback context alive until dispatch finishes. */
                AcquireSRWLockShared( &iface->m_CallbackLock );
                LIST_FOR_EACH_ENTRY( callback, &iface->m_Callbacks, response_received_callback, entry )
                    callback->handler->Invoke( packet );
                ReleaseSRWLockShared( &iface->m_CallbackLock );
                packet->Release();
                packet = nullptr;
                access->Release();
                access = nullptr;
                message->Release();
                message = nullptr;
            }
            if (offset)
            {
                memmove( poll.curr_buffer, poll.curr_buffer + offset, poll.curr_buffer_size - offset );
                poll.curr_buffer_size -= offset;
            }
        }

    cleanup:
        if (packet) packet->Release();
        if (access) access->Release();
        if (message) message->Release();
        factory->Release();
        return hr;
    }

    struct response_received_callback
    {
        struct list entry;
        IIPCResponseHandler *handler;
        INT64 token;
    };

    struct list m_Callbacks = LIST_INIT( m_Callbacks );
    SRWLOCK m_CallbackLock = SRWLOCK_INIT;
    SRWLOCK m_RequestLock = SRWLOCK_INIT;
    std::atomic<INT64> m_NextEventToken{ 0 };
    std::atomic_long ref{ 1 };
};

static IPCLayer g_xodus_ipclayer;
IIPCLayer *xodus_ipclayer = static_cast<IIPCLayer*>(&g_xodus_ipclayer);
