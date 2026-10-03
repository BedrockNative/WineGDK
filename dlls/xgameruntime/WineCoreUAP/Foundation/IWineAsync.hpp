/* CryptoWinRT Implementation
 *
 * Copyright 2022 Bernhard Kölbl for CodeWeavers
 * Copyright 2022 Rémi Bernon for CodeWeavers
 * C++ port was done by Weather.
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
#include "provider.h"

#include <atomic>
#include <mutex>
#include <new>

#ifndef IWINEASYNC_HPP
#define IWINEASYNC_HPP

using namespace ABI::Windows::Foundation;
using namespace ABI::XGameRuntime;

class AsyncInfo final
    : public IAsyncInfo
    , public IWineAsyncInfoImpl
{
public:
    AsyncInfo() = default;
    virtual ~AsyncInfo() = default;

    /* IUnknown Methods (Shared) */
    HRESULT WINAPI
    QueryInterface( REFIID iid, void** out ) noexcept override;

    ULONG WINAPI
    AddRef() noexcept override;

    ULONG WINAPI
    Release() noexcept override;

    /* IInspectable Methods (WineAsyncInfoImpl) */
    HRESULT WINAPI
    GetIids( ULONG *iid_count, IID **iids ) noexcept override;

    HRESULT WINAPI
    GetRuntimeClassName( HSTRING *class_name ) noexcept override;

    HRESULT WINAPI
    GetTrustLevel( TrustLevel *trust_level ) noexcept override;

    /* IWineAsyncOperationCompletedHandler Methods */
    HRESULT WINAPI
    put_Completed( IWineAsyncOperationCompletedHandler *handler ) noexcept override;

    HRESULT WINAPI
    get_Completed( IWineAsyncOperationCompletedHandler **handler ) noexcept override;

    HRESULT WINAPI
    get_Result( PROPVARIANT *result ) noexcept override;

    HRESULT WINAPI
    Start() noexcept override;

    /* IAsyncInfo Methods */
    HRESULT WINAPI
    get_Id( UINT32 *id ) noexcept override;

    HRESULT WINAPI
    get_Status( AsyncStatus *status ) noexcept override;

    HRESULT WINAPI
    get_ErrorCode( HRESULT *error_code ) noexcept override;

    HRESULT WINAPI
    Cancel() noexcept override;

    HRESULT WINAPI
    Close() noexcept override;

    /* Internal methods */
    static HRESULT WINAPI
    Create( IUnknown *invoker, PVOID param, async_operation_callback callback,
                                  IInspectable *outer, IWineAsyncInfoImpl **out ) noexcept;

private:
    static void CALLBACK
    async_info_callback( TP_CALLBACK_INSTANCE *instance, void *iface, TP_WORK *work );

    std::atomic_long ref{ 1 };
    std::mutex mutex;

    IWineAsyncOperationCompletedHandler *handler = nullptr;
    IInspectable *IInspectable_outer = nullptr;
    IErrorInfo *errorInfo = nullptr;
    IUnknown *invoker = nullptr;

    async_operation_callback callback = nullptr;
    AsyncStatus status = Started;
    bool closed = false;
    PROPVARIANT result{};
    HRESULT hr = S_OK;
    TP_WORK *async_run_work = nullptr;
    PVOID param = nullptr;
};

template<typename T>
class AsyncOperation
    : public IAsyncOperation<T>
{
public:
    AsyncOperation() = default;
    virtual ~AsyncOperation() { if (info) info->Release(); }

        /* IUnknown Methods */
    HRESULT WINAPI
    QueryInterface( REFIID iid, void** out ) noexcept override;

    ULONG WINAPI
    AddRef() noexcept override;

    ULONG WINAPI
    Release() noexcept override;

    /* IInspectable Methods */
    HRESULT WINAPI
    GetIids( ULONG *iid_count, IID **iids ) noexcept override;

    HRESULT WINAPI
    GetRuntimeClassName( HSTRING *class_name ) noexcept override;

    HRESULT WINAPI
    GetTrustLevel( TrustLevel *trust_level ) noexcept override;

    /* IAsyncOperation<TResult> methods */
    HRESULT WINAPI
    put_Completed( IAsyncOperationCompletedHandler<T> *inspectable_handler ) noexcept override;

    HRESULT WINAPI
    get_Completed( IAsyncOperationCompletedHandler<T> **inspectable_handler ) noexcept override;

    HRESULT WINAPI
    GetResults( T *results ) noexcept override;

    /* Internal methods */
    static HRESULT WINAPI
    Create( IUnknown *invoker, PVOID param, async_operation_callback callback,
                IAsyncOperation<T> **out );

private:
    std::atomic_long ref{ 1 };
    IWineAsyncInfoImpl *info = nullptr;
};

class AsyncAction final
    : public IAsyncAction
{
public:
    AsyncAction() = default;
    virtual ~AsyncAction() { if (info) info->Release(); }

    /* IUnknown Methods */
    HRESULT WINAPI
    QueryInterface( REFIID iid, void** out ) noexcept override;

    ULONG WINAPI
    AddRef() noexcept override;

    ULONG WINAPI
    Release() noexcept override;

    /* IInspectable Methods */
    HRESULT WINAPI
    GetIids( ULONG *iid_count, IID **iids ) noexcept override;

    HRESULT WINAPI
    GetRuntimeClassName( HSTRING *class_name ) noexcept override;

    HRESULT WINAPI
    GetTrustLevel( TrustLevel *trust_level ) noexcept override;

    /* IAsyncOperation<TResult> methods */
    HRESULT WINAPI
    put_Completed( IAsyncActionCompletedHandler *inspectable_handler ) noexcept override;

    HRESULT WINAPI
    get_Completed( IAsyncActionCompletedHandler **inspectable_handler ) noexcept override;

    HRESULT WINAPI
    GetResults() noexcept override;

    /* Internal methods */
    static HRESULT WINAPI
    Create( IUnknown *invoker, PVOID param, async_operation_callback callback,
                                        IAsyncAction **out );

private:
    std::atomic_long ref{ 1 };
    IWineAsyncInfoImpl *info = nullptr;
};

// NOTE: Do not create a non-static instance of this object.
class AsyncActionCompletedHandler final
    : public IAsyncActionCompletedHandler
{
public:
    virtual ~AsyncActionCompletedHandler() { if (event) CloseHandle( event ); }
    
    /* IUnknown Methods */
    HRESULT WINAPI
    QueryInterface( REFIID iid, void** out ) noexcept override
    {
        if (!out) return E_POINTER;
        *out = nullptr;

        if ( iid == __uuidof( IUnknown ) ||
             iid == __uuidof( IInspectable ) ||
             iid == __uuidof( IAgileObject ) ||
             iid == __uuidof( IAsyncActionCompletedHandler ) )
        {
            AddRef();
            *out = static_cast<IAsyncActionCompletedHandler *>(this);
            return S_OK;
        }

        *out = NULL;
        return E_NOINTERFACE;
    }

    ULONG WINAPI
    AddRef() noexcept override
    {
        ULONG curr = static_cast<ULONG>(++ref);
        return curr;
    }

    ULONG WINAPI
    Release() noexcept override
    {
        ULONG curr = static_cast<ULONG>(--ref);

        if ( !curr )
        {
            delete this;
        }

        return curr;
    }

    HRESULT WINAPI
    Invoke( IAsyncAction *invoker, AsyncStatus status ) override
    {
        if ( event ) SetEvent( event );
        return S_OK;
    }

    /* Internal methods */
    static DWORD await_AsyncAction( IAsyncAction *async, DWORD timeout )
    {
        if (!async) return E_POINTER;
        auto handler = new (std::nothrow) AsyncActionCompletedHandler();
        if (!handler) return E_OUTOFMEMORY;
        if (!(handler->event = CreateEventW( nullptr, FALSE, FALSE, nullptr )))
        {
            DWORD error = GetLastError();
            handler->Release();
            return HRESULT_FROM_WIN32( error );
        }
        HRESULT hr = async->put_Completed( handler );
        DWORD ret = FAILED(hr) ? static_cast<DWORD>(hr) : WaitForSingleObject( handler->event, timeout );
        handler->Release();
        return ret;
    }

    static DWORD await_CancellableAsyncAction( IAsyncAction *async, HANDLE event, DWORD timeout )
    {
        if (!async || !event) return E_POINTER;
        auto handler = new (std::nothrow) AsyncActionCompletedHandler();
        if (!handler) return E_OUTOFMEMORY;
        if (!DuplicateHandle( GetCurrentProcess(), event, GetCurrentProcess(), &handler->event,
                              0, FALSE, DUPLICATE_SAME_ACCESS ))
        {
            DWORD error = GetLastError();
            handler->Release();
            return HRESULT_FROM_WIN32( error );
        }
        HRESULT hr = async->put_Completed( handler );
        DWORD ret = FAILED(hr) ? static_cast<DWORD>(hr) : WaitForSingleObject( handler->event, timeout );
        handler->Release();
        return ret;
    }

private:
    HANDLE event = nullptr;
    std::atomic_long ref{ 1 };
};

// NOTE: Do not create a non-static instance of this object.
template<typename T>
class AsyncOperationCompletedHandler final
    : public IAsyncOperationCompletedHandler<T>
{
public:
    virtual ~AsyncOperationCompletedHandler() { if (event) CloseHandle( event ); }
    
    /* IUnknown Methods */
    HRESULT WINAPI
    QueryInterface( REFIID iid, void** out ) noexcept override
    {
        if (!out) return E_POINTER;
        *out = nullptr;

        if ( iid == __uuidof( IUnknown ) ||
             iid == __uuidof( IInspectable ) ||
             iid == __uuidof( IAgileObject ) ||
             iid == __uuidof( IAsyncOperationCompletedHandler<T> ) )
        {
            AddRef();
            *out = static_cast<IAsyncOperationCompletedHandler<T> *>(this);
            return S_OK;
        }

        *out = NULL;
        return E_NOINTERFACE;
    }

    ULONG WINAPI
    AddRef() noexcept override
    {
        ULONG curr = static_cast<ULONG>(++ref);
        return curr;
    }

    ULONG WINAPI
    Release() noexcept override
    {
        ULONG curr = static_cast<ULONG>(--ref);

        if ( !curr )
        {
            delete this;
        }

        return curr;
    }

    HRESULT WINAPI
    Invoke( IAsyncOperation<T> *invoker, AsyncStatus status ) override
    {
        if ( event ) SetEvent( event );
        return S_OK;
    }

    /* Internal methods */
    static DWORD await_AsyncOperation( IAsyncOperation<T> *async, DWORD timeout )
    {
        if (!async) return E_POINTER;
        auto handler = new (std::nothrow) AsyncOperationCompletedHandler<T>();
        if (!handler) return E_OUTOFMEMORY;
        if (!(handler->event = CreateEventW( nullptr, FALSE, FALSE, nullptr )))
        {
            DWORD error = GetLastError();
            handler->Release();
            return HRESULT_FROM_WIN32( error );
        }
        HRESULT hr = async->put_Completed( handler );
        DWORD ret = FAILED(hr) ? static_cast<DWORD>(hr) : WaitForSingleObject( handler->event, timeout );
        handler->Release();
        return ret;
    }

    static DWORD await_CancellableAsyncOperation( IAsyncOperation<T> *async, HANDLE event, DWORD timeout )
    {
        if (!async || !event) return E_POINTER;
        auto handler = new (std::nothrow) AsyncOperationCompletedHandler<T>();
        if (!handler) return E_OUTOFMEMORY;
        if (!DuplicateHandle( GetCurrentProcess(), event, GetCurrentProcess(), &handler->event,
                              0, FALSE, DUPLICATE_SAME_ACCESS ))
        {
            DWORD error = GetLastError();
            handler->Release();
            return HRESULT_FROM_WIN32( error );
        }
        HRESULT hr = async->put_Completed( handler );
        DWORD ret = FAILED(hr) ? static_cast<DWORD>(hr) : WaitForSingleObject( handler->event, timeout );
        handler->Release();
        return ret;
    }

private:
    HANDLE event = nullptr;
    std::atomic_long ref{ 1 };
};

#endif