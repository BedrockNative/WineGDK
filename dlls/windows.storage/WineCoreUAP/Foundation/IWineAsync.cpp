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

#include <atomic>
#include <mutex>

#include "IWineAsync.hpp"
#include "roapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(wineasync);

#define Closed 4
#define HANDLER_NOT_SET ((void *)~(ULONG_PTR)0)

HRESULT WINAPI
AsyncInfo::QueryInterface( REFIID iid, void** out ) noexcept
{
    TRACE( "iface %p, iid %s, out %p.\n", this, debugstr_guid( &iid ), out );

    if (!out) return E_POINTER;
    *out = nullptr;

    if ( iid == __uuidof( IUnknown ) ||
         iid == __uuidof( IInspectable ) ||
         iid == __uuidof( IAgileObject ) ||
         iid == __uuidof( IWineAsyncInfoImpl ) )
    {
        AddRef();
        *out = static_cast<IWineAsyncInfoImpl *>(this);
        return S_OK;
    }

    if ( iid == __uuidof(IAsyncInfo) )
    {
        AddRef();
        *out = static_cast<IAsyncInfo *>(this);
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( &iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

ULONG WINAPI
AsyncInfo::AddRef() noexcept
{
    ULONG curr = static_cast<ULONG>(++ref);
    TRACE( "iface %p increasing refcount to %lu.\n", this, curr );
    return curr;
}

ULONG WINAPI
AsyncInfo::Release() noexcept
{
    ULONG curr = static_cast<ULONG>(--ref);
    TRACE( "iface %p decreasing refcount to %lu.\n", this, curr );

    if ( !curr )
    {
        if ( handler && handler != HANDLER_NOT_SET )
            handler->Release();
        Close();

        if ( invoker ) invoker->Release();
        if (errorInfo) errorInfo->Release();
        PropVariantClear(&result);

        delete this;
    }

    return curr;
}

/* IInspectable Methods (WineAsyncInfoImpl) */
HRESULT WINAPI
AsyncInfo::GetIids( ULONG *iid_count, IID **iids ) noexcept
{
    TRACE( "iface %p, iid_count %p, iids %p\n", this, iid_count, iids );

    if ( !iid_count || !iids )
        return E_POINTER;

    *iid_count = 2;
    IID* allocated = static_cast<IID*>( CoTaskMemAlloc( sizeof(IID) * (*iid_count) ) );

    if ( !allocated )
        return E_OUTOFMEMORY;

    allocated[0] = __uuidof( IWineAsyncInfoImpl );
    allocated[1] = __uuidof( IAsyncInfo );

    *iids = allocated;
    return S_OK;
}

HRESULT WINAPI
AsyncInfo::GetRuntimeClassName( HSTRING *class_name ) noexcept
{
    TRACE( "iface %p, class_name %p\n", this, class_name );
    return WindowsCreateString( (LPCWSTR)L"Windows.Storage.WineAsyncInfoImpl", 34, class_name );
}

HRESULT WINAPI
AsyncInfo::GetTrustLevel( TrustLevel *trust_level ) noexcept
{
    FIXME( "iface %p, trust_level %p stub!\n", this, trust_level );
    return E_NOTIMPL;
}

/* IWineAsyncOperationCompletedHandler Methods */
HRESULT WINAPI
AsyncInfo::put_Completed( IWineAsyncOperationCompletedHandler *handler ) noexcept
{
    AsyncStatus completed_status = Started;
    if (!handler) return E_INVALIDARG;
    {
        const std::lock_guard<std::mutex> lock( mutex );
        if (status == Closed) return E_ILLEGAL_METHOD_CALL;
        if (this->handler != HANDLER_NOT_SET) return E_ILLEGAL_DELEGATE_ASSIGNMENT;
        handler->AddRef();
        this->handler = handler;
        if (status != Started)
        {
            completed_status = status;
            this->handler = nullptr;
        }
    }
    if (completed_status != Started)
    {
        handler->Invoke(IInspectable_outer, completed_status);
        handler->Release();
    }
    return S_OK;
}

HRESULT WINAPI
AsyncInfo::get_Completed( IWineAsyncOperationCompletedHandler **handler ) noexcept
{
    TRACE( "iface %p, handler %p.\n", this, handler );

    {
        const std::lock_guard<std::mutex> lock( mutex );

        if ( status == Closed )
            return E_ILLEGAL_METHOD_CALL;

        if (this->handler == NULL || this->handler == HANDLER_NOT_SET )
            *handler = NULL;
        else
        {
            this->handler->AddRef();
            *handler = this->handler;
        }
    }

    return S_OK;
}

HRESULT WINAPI
AsyncInfo::get_Result( PROPVARIANT *result ) noexcept
{
    if (!result) return E_POINTER;
    const std::lock_guard<std::mutex> lock( mutex );
    if (status == Closed || status == Started) return E_ILLEGAL_METHOD_CALL;
    if (status == Canceled) return E_ABORT;
    if (status == Error)
    {
        if (errorInfo) SetErrorInfo(0, errorInfo);
        return hr;
    }
    return PropVariantCopy(result, &this->result);
}

HRESULT WINAPI
AsyncInfo::Start() noexcept
{
    TRACE( "iface %p.\n", this );

    IInspectable_outer->AddRef();
    SubmitThreadpoolWork( async_run_work );

    return S_OK;
}

/* IAsyncInfo Methods */
HRESULT WINAPI
AsyncInfo::get_Id( UINT32 *id ) noexcept
{
    TRACE( "iface %p, id %p.\n", this, id );

    {
        const std::lock_guard<std::mutex> lock( mutex );

        if ( status == Closed )
            return E_ILLEGAL_METHOD_CALL;
        *id = 1;
    }

    return S_OK;
}

HRESULT WINAPI
AsyncInfo::get_Status( AsyncStatus *status ) noexcept
{
    TRACE( "iface %p, status %p.\n", this, status );

    {
        const std::lock_guard<std::mutex> lock( mutex );

        if ( this->status == Closed )
            return E_ILLEGAL_METHOD_CALL;
        *status = this->status;
    }

    return S_OK;
}

HRESULT WINAPI
AsyncInfo::get_ErrorCode( HRESULT *error_code ) noexcept
{
    TRACE( "iface %p, error_code %p.\n", this, error_code );

    {
        const std::lock_guard<std::mutex> lock( mutex );

        if ( status == Closed )
            return *error_code = E_ILLEGAL_METHOD_CALL;
        *error_code = hr;
    }

    return S_OK;
}

HRESULT WINAPI
AsyncInfo::Cancel() noexcept
{
    TRACE( "iface %p.\n", this );

    {
        const std::lock_guard<std::mutex> lock( mutex );

        if ( status == Closed )
            return E_ILLEGAL_METHOD_CALL;

        if ( status == Started ) status = Canceled;
    }

    return S_OK;
}

HRESULT WINAPI
AsyncInfo::Close() noexcept
{
    TRACE( "iface %p.\n", this );

    {
        const std::lock_guard<std::mutex> lock( mutex );

        if ( status == Started )
            return E_ILLEGAL_STATE_CHANGE;

        if ( status != Closed )
        {
            CloseThreadpoolWork( async_run_work );
            async_run_work = NULL;
            status = static_cast<AsyncStatus>(Closed);
        }
    }

    return S_OK;
}

/* Internal methods */
HRESULT WINAPI
AsyncInfo::Create( IUnknown *invoker, PVOID param, async_operation_callback callback,
                                  IInspectable *outer, IWineAsyncInfoImpl **out ) noexcept
{
    AsyncInfo *impl = new AsyncInfo();
    HRESULT hr;

    impl->IInspectable_outer = outer;
    impl->callback = callback;
    impl->handler = static_cast<IWineAsyncOperationCompletedHandler *>(HANDLER_NOT_SET);
    impl->status = Started;

    if ( !( impl->async_run_work = CreateThreadpoolWork( async_info_callback, impl, NULL ) ) )
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        delete impl;
        return hr;
    }

    if ( (impl->invoker = invoker) ) impl->invoker->AddRef();
    impl->param = param;

    *out = impl;
    return S_OK;
}

void CALLBACK
AsyncInfo::async_info_callback( TP_CALLBACK_INSTANCE *instance, void *iface, TP_WORK *work )
{
    AsyncInfo *impl = static_cast<AsyncInfo *>(iface);
    PROPVARIANT result = {};
    IInspectable *operation = impl->IInspectable_outer;
    IWineAsyncOperationCompletedHandler *handler = nullptr;
    AsyncStatus status;
    HRESULT init_hr = RoInitialize(RO_INIT_MULTITHREADED);
    HRESULT hr = impl->callback(impl->invoker, impl->param, &result);
    {
        const std::lock_guard<std::mutex> lock(impl->mutex);
        if (impl->status == Started) impl->status = FAILED(hr) ? Error : Completed;
        if (impl->status == Error) GetErrorInfo(0, &impl->errorInfo);
        impl->hr = hr;
        impl->result = result;
        result.vt = VT_EMPTY;
        status = impl->status;
        if (impl->handler && impl->handler != HANDLER_NOT_SET)
        {
            handler = impl->handler;
            impl->handler = nullptr;
        }
    }
    if (handler)
    {
        handler->Invoke(operation, status);
        handler->Release();
    }
    operation->Release();
    if (SUCCEEDED(init_hr)) RoUninitialize();
}

HRESULT WINAPI
AsyncOperation::Inspectable::QueryInterface( REFIID iid, void** out ) noexcept
{
    TRACE( "iface %p, iid %s, out %p.\n", this, debugstr_guid( &iid ), out );

    if (!out) return E_POINTER;
    *out = nullptr;

    if ( iid == __uuidof( IUnknown ) ||
         iid == __uuidof( IInspectable ) ||
         iid == __uuidof( IAgileObject ) ||
         iid == __uuidof( IAsyncOperation<IInspectable *> ) )
    {
        AddRef();
        *out = static_cast<IAsyncOperation<IInspectable *> *>(this);
        return S_OK;
    }

    if ( IsEqualGUID( iid, iids.operation ) )
    {
        AddRef();
        *out = static_cast<IAsyncOperation<IInspectable *> *>(this);
        return S_OK;
    }

    return info->QueryInterface( iid, out );
}

ULONG WINAPI
AsyncOperation::Inspectable::AddRef() noexcept
{
    ULONG curr = static_cast<ULONG>(++ref);
    TRACE( "iface %p increasing refcount to %lu.\n", this, curr );
    return curr;
}

ULONG WINAPI
AsyncOperation::Inspectable::Release() noexcept
{
    ULONG curr = static_cast<ULONG>(--ref);
    TRACE( "iface %p decreasing refcount to %lu.\n", this, curr );

    if ( !curr )
        delete this;

    return curr;
}

/* IInspectable Methods */
HRESULT WINAPI
AsyncOperation::Inspectable::GetIids( ULONG *iid_count, IID **iids ) noexcept
{
    TRACE( "iface %p, iid_count %p, iids %p\n", this, iid_count, iids );

    if ( !iid_count || !iids )
        return E_POINTER;

    *iid_count = 2;
    IID* allocated = static_cast<IID*>( CoTaskMemAlloc( sizeof(IID) * (*iid_count) ) );

    if ( !allocated )
        return E_OUTOFMEMORY;

    allocated[0] = __uuidof( IAsyncOperation<IInspectable *> );
    allocated[1] = __uuidof( IAsyncInfo );

    *iids = allocated;
    return S_OK;
}

HRESULT WINAPI
AsyncOperation::Inspectable::GetRuntimeClassName( HSTRING *class_name ) noexcept
{
    TRACE( "iface %p, class_name %p\n", this, class_name );
    return WindowsCreateString( (LPCWSTR)L"Windows.Foundation.IAsyncAction`1<IInspectable>", 48, class_name );
}

HRESULT WINAPI
AsyncOperation::Inspectable::GetTrustLevel( TrustLevel *trust_level ) noexcept
{
    FIXME( "iface %p, trust_level %p stub!\n", this, trust_level );
    return E_NOTIMPL;
}

/* IAsyncOperation<TResult> methods */
HRESULT WINAPI
AsyncOperation::Inspectable::put_Completed( IAsyncOperationCompletedHandler<IInspectable *> *inspectable_handler ) noexcept
{
    IWineAsyncOperationCompletedHandler *handler = reinterpret_cast<IWineAsyncOperationCompletedHandler *>(inspectable_handler);
    TRACE( "iface %p, handler %p.\n", this, handler );
    return info->put_Completed( handler );
}

HRESULT WINAPI
AsyncOperation::Inspectable::get_Completed( IAsyncOperationCompletedHandler<IInspectable *> **inspectable_handler ) noexcept
{
    IWineAsyncOperationCompletedHandler **handler = reinterpret_cast<IWineAsyncOperationCompletedHandler **>(inspectable_handler);
    TRACE( "iface %p, handler %p.\n", this, handler );
    return info->get_Completed( handler );
}

HRESULT WINAPI
AsyncOperation::Inspectable::GetResults( IInspectable **results ) noexcept
{
    TRACE( "iface %p.\n", this );

    PROPVARIANT result = {.vt = VT_NULL};
    HRESULT hr;

    hr = info->get_Result( &result );

    if (!results) return E_POINTER;
    *results = nullptr;
    if (SUCCEEDED(hr) && result.vt == VT_UNKNOWN)
    {
        *results = static_cast<IInspectable *>(result.punkVal);
        result.vt = VT_EMPTY; /* Transfer the reference acquired by get_Result. */
    }
    PropVariantClear( &result );

    return hr;
}

HRESULT WINAPI
AsyncOperation::Inspectable::Create( IUnknown *invoker, PVOID param, async_operation_callback callback,
                            struct async_operation_iids iids, IAsyncOperation<IInspectable *> **out )
{
    AsyncOperation::Inspectable *impl = new AsyncOperation::Inspectable();
    HRESULT hr;

    impl->iids = iids;

    if ( FAILED( hr = AsyncInfo::Create( invoker, param, callback, static_cast<IInspectable *>(impl), &impl->info ) ) ||
         FAILED( hr = impl->info->Start() ) )
    {
        impl->info->Release();
        delete impl;
        return hr;
    }

    *out = impl;
    TRACE( "created AsyncAction %p\n", *out );
    return S_OK;
}

HRESULT WINAPI
AsyncAction::QueryInterface( REFIID iid, void** out ) noexcept
{
    TRACE( "iface %p, iid %s, out %p.\n", this, debugstr_guid( &iid ), out );

    if (!out) return E_POINTER;
    *out = nullptr;

    if ( iid == __uuidof( IUnknown ) ||
         iid == __uuidof( IInspectable ) ||
         iid == __uuidof( IAgileObject ) ||
         iid == __uuidof( IAsyncAction ) )
    {
        AddRef();
        *out = static_cast<IAsyncAction *>(this);
        return S_OK;
    }

    return info->QueryInterface( iid, out );
}

ULONG WINAPI
AsyncAction::AddRef() noexcept
{
    ULONG curr = static_cast<ULONG>(++ref);
    TRACE( "iface %p increasing refcount to %lu.\n", this, curr );
    return curr;
}

ULONG WINAPI
AsyncAction::Release() noexcept
{
    ULONG curr = static_cast<ULONG>(--ref);
    TRACE( "iface %p decreasing refcount to %lu.\n", this, curr );

    if ( !curr )
        delete this;

    return curr;
}

/* IInspectable Methods */
HRESULT WINAPI
AsyncAction::GetIids( ULONG *iid_count, IID **iids ) noexcept
{
    TRACE( "iface %p, iid_count %p, iids %p\n", this, iid_count, iids );

    if ( !iid_count || !iids )
        return E_POINTER;

    *iid_count = 2;
    IID* allocated = static_cast<IID*>( CoTaskMemAlloc( sizeof(IID) * (*iid_count) ) );

    if ( !allocated )
        return E_OUTOFMEMORY;

    allocated[0] = __uuidof( IAsyncAction );
    allocated[1] = __uuidof( IAsyncInfo );

    *iids = allocated;
    return S_OK;
}

HRESULT WINAPI
AsyncAction::GetRuntimeClassName( HSTRING *class_name ) noexcept
{
    TRACE( "iface %p, class_name %p\n", this, class_name );
    return WindowsCreateString( (LPCWSTR)L"Windows.Foundation.IAsyncAction`1<IInspectable>", 48, class_name );
}

HRESULT WINAPI
AsyncAction::GetTrustLevel( TrustLevel *trust_level ) noexcept
{
    FIXME( "iface %p, trust_level %p stub!\n", this, trust_level );
    return E_NOTIMPL;
}

/* IAsyncOperation<TResult> methods */
HRESULT WINAPI
AsyncAction::put_Completed( IAsyncActionCompletedHandler *inspectable_handler ) noexcept
{
    IWineAsyncOperationCompletedHandler *handler = reinterpret_cast<IWineAsyncOperationCompletedHandler *>(inspectable_handler);
    TRACE( "iface %p, handler %p.\n", this, handler );
    return info->put_Completed( handler );
}

HRESULT WINAPI
AsyncAction::get_Completed( IAsyncActionCompletedHandler **inspectable_handler ) noexcept
{
    IWineAsyncOperationCompletedHandler **handler = reinterpret_cast<IWineAsyncOperationCompletedHandler **>(inspectable_handler);
    TRACE( "iface %p, handler %p.\n", this, handler );
    return info->get_Completed( handler );
}

HRESULT WINAPI
AsyncAction::GetResults() noexcept
{
    TRACE( "iface %p.\n", this );

    PROPVARIANT result = {.vt = VT_NULL};
    HRESULT hr;

    hr = info->get_Result( &result );

    return hr;
}

/* Internal methods */
HRESULT WINAPI
AsyncAction::Create( IUnknown *invoker, PVOID param, async_operation_callback callback,
                                        IAsyncAction **out )
{
    AsyncAction *impl = new AsyncAction();
    HRESULT hr;

    if ( FAILED( hr = AsyncInfo::Create( invoker, param, callback, static_cast<IInspectable *>(impl), &impl->info ) ) ||
         FAILED( hr = impl->info->Start() ) )
    {
        impl->info->Release();
        delete impl;
        return hr;
    }

    *out = impl;
    TRACE( "created AsyncAction %p\n", *out );
    return S_OK;
}