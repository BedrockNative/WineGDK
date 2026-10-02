/* Windows.Storage.Pickers.FileSavePicker (UWP flavour)
 *
 * Minecraft's export flow (structures, worlds...) activates the UWP Windows.Storage.Pickers.FileSavePicker, not the
 * Windows App SDK one, and then writes through the returned StorageFile. Wine has no such class, so the game waited
 * forever after an activation failure. This reuses the launcher-brokered dialog of the Microsoft.Windows.* pickers.
 *
 * Copyright 2026 OrionBE contributors
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

#include <stdlib.h>

#include "private.h"

#define WIDL_using_Windows_Storage
#include "windows.storage.h"

#include "initguid.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(pickers);

/* Windows.Storage.Pickers.IFileSavePicker {3286ffcb-617f-4cc5-af6a-b3fdf29ad145} */
DEFINE_GUID( IID_IOrionUwpFileSavePicker, 0x3286ffcb, 0x617f, 0x4cc5, 0xaf, 0x6a, 0xb3, 0xfd, 0xf2, 0x9a, 0xd1, 0x45 );

typedef struct IOrionUwpFileSavePicker IOrionUwpFileSavePicker;

typedef struct IOrionUwpFileSavePickerVtbl
{
    BEGIN_INTERFACE
    HRESULT (WINAPI *QueryInterface)( IOrionUwpFileSavePicker *iface, REFIID iid, void **out );
    ULONG (WINAPI *AddRef)( IOrionUwpFileSavePicker *iface );
    ULONG (WINAPI *Release)( IOrionUwpFileSavePicker *iface );
    HRESULT (WINAPI *GetIids)( IOrionUwpFileSavePicker *iface, ULONG *iid_count, IID **iids );
    HRESULT (WINAPI *GetRuntimeClassName)( IOrionUwpFileSavePicker *iface, HSTRING *class_name );
    HRESULT (WINAPI *GetTrustLevel)( IOrionUwpFileSavePicker *iface, TrustLevel *trust_level );
    HRESULT (WINAPI *get_SettingsIdentifier)( IOrionUwpFileSavePicker *iface, HSTRING *value );
    HRESULT (WINAPI *put_SettingsIdentifier)( IOrionUwpFileSavePicker *iface, HSTRING value );
    HRESULT (WINAPI *get_SuggestedStartLocation)( IOrionUwpFileSavePicker *iface, int *value );
    HRESULT (WINAPI *put_SuggestedStartLocation)( IOrionUwpFileSavePicker *iface, int value );
    HRESULT (WINAPI *get_CommitButtonText)( IOrionUwpFileSavePicker *iface, HSTRING *value );
    HRESULT (WINAPI *put_CommitButtonText)( IOrionUwpFileSavePicker *iface, HSTRING value );
    HRESULT (WINAPI *get_FileTypeChoices)( IOrionUwpFileSavePicker *iface, IMap_HSTRING_IVector_HSTRING **value );
    HRESULT (WINAPI *get_DefaultFileExtension)( IOrionUwpFileSavePicker *iface, HSTRING *value );
    HRESULT (WINAPI *put_DefaultFileExtension)( IOrionUwpFileSavePicker *iface, HSTRING value );
    HRESULT (WINAPI *get_SuggestedSaveFile)( IOrionUwpFileSavePicker *iface, IStorageFile **value );
    HRESULT (WINAPI *put_SuggestedSaveFile)( IOrionUwpFileSavePicker *iface, IStorageFile *value );
    HRESULT (WINAPI *get_SuggestedFileName)( IOrionUwpFileSavePicker *iface, HSTRING *value );
    HRESULT (WINAPI *put_SuggestedFileName)( IOrionUwpFileSavePicker *iface, HSTRING value );
    HRESULT (WINAPI *PickSaveFileAsync)( IOrionUwpFileSavePicker *iface, IAsyncOperation_StorageFile **operation );
    END_INTERFACE
} IOrionUwpFileSavePickerVtbl;

struct IOrionUwpFileSavePicker
{
    const IOrionUwpFileSavePickerVtbl *lpVtbl;
};

struct uwp_save_picker
{
    IOrionUwpFileSavePicker iface;
    LONG ref;
    CRITICAL_SECTION cs;
    int start_location;
    WCHAR *settings_id;
    WCHAR *commit_text;
    WCHAR *default_ext;
    WCHAR *suggested_name;
    IMap_HSTRING_IVector_HSTRING *choices;
    IStorageFile *suggested_file;
};

static inline struct uwp_save_picker *impl_from_picker( IOrionUwpFileSavePicker *iface )
{
    return CONTAINING_RECORD( iface, struct uwp_save_picker, iface );
}

static HRESULT get_hstring_w( const WCHAR *str, HSTRING *value )
{
    if (!value) return E_POINTER;
    if (!str) { *value = NULL; return S_OK; }
    return WindowsCreateString( str, wcslen( str ), value );
}

static HRESULT WINAPI uwp_save_QueryInterface( IOrionUwpFileSavePicker *iface, REFIID iid, void **out )
{
    struct uwp_save_picker *impl = impl_from_picker( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IOrionUwpFileSavePicker ))
    {
        *out = &impl->iface;
        IUnknown_AddRef( (IUnknown *)*out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI uwp_save_AddRef( IOrionUwpFileSavePicker *iface )
{
    struct uwp_save_picker *impl = impl_from_picker( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI uwp_save_Release( IOrionUwpFileSavePicker *iface )
{
    struct uwp_save_picker *impl = impl_from_picker( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref)
    {
        if (impl->choices) IMap_HSTRING_IVector_HSTRING_Release( impl->choices );
        if (impl->suggested_file) IStorageFile_Release( impl->suggested_file );
        free( impl->settings_id );
        free( impl->commit_text );
        free( impl->default_ext );
        free( impl->suggested_name );
        impl->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI uwp_save_GetIids( IOrionUwpFileSavePicker *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI uwp_save_GetRuntimeClassName( IOrionUwpFileSavePicker *iface, HSTRING *class_name )
{
    if (!class_name) return E_POINTER;
    return WindowsCreateString( L"Windows.Storage.Pickers.FileSavePicker", 38, class_name );
}

static HRESULT WINAPI uwp_save_GetTrustLevel( IOrionUwpFileSavePicker *iface, TrustLevel *trust_level )
{
    *trust_level = BaseTrust;
    return S_OK;
}

#define UWP_SAVE_STRING_PROPERTY( name, field )                                                    \
    static HRESULT WINAPI uwp_save_get_##name( IOrionUwpFileSavePicker *iface, HSTRING *value )    \
    {                                                                                              \
        struct uwp_save_picker *impl = impl_from_picker( iface );                                  \
        HRESULT hr;                                                                                \
        TRACE( "iface %p, value %p.\n", iface, value );                                            \
        EnterCriticalSection( &impl->cs );                                                         \
        hr = get_hstring_w( impl->field, value );                                                  \
        LeaveCriticalSection( &impl->cs );                                                         \
        return hr;                                                                                 \
    }                                                                                              \
    static HRESULT WINAPI uwp_save_put_##name( IOrionUwpFileSavePicker *iface, HSTRING value )     \
    {                                                                                              \
        struct uwp_save_picker *impl = impl_from_picker( iface );                                  \
        TRACE( "iface %p, value %s.\n", iface, debugstr_hstring( value ) );                        \
        EnterCriticalSection( &impl->cs );                                                         \
        free( impl->field );                                                                       \
        impl->field = hstring_dup( value );                                                        \
        LeaveCriticalSection( &impl->cs );                                                         \
        return S_OK;                                                                               \
    }

UWP_SAVE_STRING_PROPERTY( SettingsIdentifier, settings_id )
UWP_SAVE_STRING_PROPERTY( CommitButtonText, commit_text )
UWP_SAVE_STRING_PROPERTY( DefaultFileExtension, default_ext )
UWP_SAVE_STRING_PROPERTY( SuggestedFileName, suggested_name )

static HRESULT WINAPI uwp_save_get_SuggestedStartLocation( IOrionUwpFileSavePicker *iface, int *value )
{
    struct uwp_save_picker *impl = impl_from_picker( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = impl->start_location;
    return S_OK;
}

static HRESULT WINAPI uwp_save_put_SuggestedStartLocation( IOrionUwpFileSavePicker *iface, int value )
{
    struct uwp_save_picker *impl = impl_from_picker( iface );
    TRACE( "iface %p, value %d.\n", iface, value );
    impl->start_location = value;
    return S_OK;
}

static HRESULT WINAPI uwp_save_get_FileTypeChoices( IOrionUwpFileSavePicker *iface, IMap_HSTRING_IVector_HSTRING **value )
{
    struct uwp_save_picker *impl = impl_from_picker( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    IMap_HSTRING_IVector_HSTRING_AddRef( (*value = impl->choices) );
    return S_OK;
}

static HRESULT WINAPI uwp_save_get_SuggestedSaveFile( IOrionUwpFileSavePicker *iface, IStorageFile **value )
{
    struct uwp_save_picker *impl = impl_from_picker( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    EnterCriticalSection( &impl->cs );
    if ((*value = impl->suggested_file)) IStorageFile_AddRef( *value );
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI uwp_save_put_SuggestedSaveFile( IOrionUwpFileSavePicker *iface, IStorageFile *value )
{
    struct uwp_save_picker *impl = impl_from_picker( iface );
    IStorageFile *old;

    TRACE( "iface %p, value %p.\n", iface, value );
    if (value) IStorageFile_AddRef( value );
    EnterCriticalSection( &impl->cs );
    old = impl->suggested_file;
    impl->suggested_file = value;
    LeaveCriticalSection( &impl->cs );
    if (old) IStorageFile_Release( old );
    return S_OK;
}

/* async: run the dialog, make sure the chosen file exists (Windows creates it), wrap it in a StorageFile */
static HRESULT uwp_pick_save_async( IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async )
{
    struct picker_request *request = CONTAINING_RECORD( param, struct picker_request, IUnknown_iface );
    IUnknown *file;
    HANDLE handle;
    WCHAR *paths;
    HRESULT hr;

    if (!called_async) return STATUS_PENDING;

    if (!(paths = picker_run_dialog( request )))
    {
        result->vt = VT_EMPTY; /* cancelled: null StorageFile, like Windows */
        return S_OK;
    }

    handle = CreateFileW( paths, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, NULL );
    if (handle != INVALID_HANDLE_VALUE) CloseHandle( handle );
    else WARN( "could not create %s, error %lu.\n", debugstr_w( paths ), GetLastError() );

    hr = storage_file_create_object( paths, &file );
    free( paths );
    if (FAILED(hr)) return hr;
    result->vt = VT_UNKNOWN;
    result->punkVal = file;
    return S_OK;
}

static HRESULT WINAPI uwp_save_PickSaveFileAsync( IOrionUwpFileSavePicker *iface, IAsyncOperation_StorageFile **operation )
{
    struct uwp_save_picker *impl = impl_from_picker( iface );
    struct picker_request *request;
    HRESULT hr;

    TRACE( "iface %p, operation %p.\n", iface, operation );

    if (!operation) return E_POINTER;
    if (FAILED(hr = picker_request_create( PICKER_KIND_SAVE, 0, &request ))) return hr;
    EnterCriticalSection( &impl->cs );
    request->start_location = impl->start_location;
    request->accept_label = impl->commit_text ? wcsdup( impl->commit_text ) : NULL;
    request->current_name = impl->suggested_name ? wcsdup( impl->suggested_name ) : NULL;
    request->default_ext = impl->default_ext ? wcsdup( impl->default_ext ) : NULL;
    LeaveCriticalSection( &impl->cs );
    request->filters = file_type_choices_to_filters( impl->choices );
    if (request->current_name && request->default_ext && !wcschr( request->current_name, '.' ))
    {
        size_t len = wcslen( request->current_name ) + wcslen( request->default_ext ) + 2;
        WCHAR *name = malloc( len * sizeof(WCHAR) );
        if (name)
        {
            swprintf( name, len, L"%s%s%s", request->current_name, request->default_ext[0] == '.' ? L"" : L".",
                      request->default_ext );
            free( request->current_name );
            request->current_name = name;
        }
    }
    TRACE( "filters %s, name %s\n", debugstr_w( request->filters ), debugstr_w( request->current_name ) );

    hr = async_operation_inspectable_create( &IID_IAsyncOperation_StorageFile, (IUnknown *)iface, &request->IUnknown_iface,
                                             uwp_pick_save_async, (IAsyncOperation_IInspectable **)operation );
    IUnknown_Release( &request->IUnknown_iface );
    return hr;
}

static const IOrionUwpFileSavePickerVtbl uwp_save_vtbl =
{
    uwp_save_QueryInterface,
    uwp_save_AddRef,
    uwp_save_Release,
    /* IInspectable methods */
    uwp_save_GetIids,
    uwp_save_GetRuntimeClassName,
    uwp_save_GetTrustLevel,
    /* IFileSavePicker methods */
    uwp_save_get_SettingsIdentifier,
    uwp_save_put_SettingsIdentifier,
    uwp_save_get_SuggestedStartLocation,
    uwp_save_put_SuggestedStartLocation,
    uwp_save_get_CommitButtonText,
    uwp_save_put_CommitButtonText,
    uwp_save_get_FileTypeChoices,
    uwp_save_get_DefaultFileExtension,
    uwp_save_put_DefaultFileExtension,
    uwp_save_get_SuggestedSaveFile,
    uwp_save_put_SuggestedSaveFile,
    uwp_save_get_SuggestedFileName,
    uwp_save_put_SuggestedFileName,
    uwp_save_PickSaveFileAsync,
};

/*
 * activation factory (default constructor)
 */

static HRESULT WINAPI uwp_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = iface;
        IUnknown_AddRef( (IUnknown *)iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI uwp_factory_AddRef( IActivationFactory *iface ) { return 2; }
static ULONG WINAPI uwp_factory_Release( IActivationFactory *iface ) { return 1; }

static HRESULT WINAPI uwp_factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI uwp_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    if (!class_name) return E_POINTER;
    return WindowsCreateString( L"Windows.Storage.Pickers.FileSavePicker", 38, class_name );
}

static HRESULT WINAPI uwp_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI uwp_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct uwp_save_picker *impl;
    HRESULT hr;

    TRACE( "iface %p, instance %p.\n", iface, instance );

    if (!instance) return E_POINTER;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl = &uwp_save_vtbl;
    impl->ref = 1;
    impl->start_location = 9; /* PickerLocationId::Unspecified */
    InitializeCriticalSectionEx( &impl->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO );
    impl->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": uwp_save_picker.cs");
    if (FAILED(hr = file_type_choices_create( &impl->choices )))
    {
        impl->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection( &impl->cs );
        free( impl );
        return hr;
    }

    *instance = (IInspectable *)&impl->iface;
    return S_OK;
}

static const IActivationFactoryVtbl uwp_factory_vtbl =
{
    uwp_factory_QueryInterface,
    uwp_factory_AddRef,
    uwp_factory_Release,
    /* IInspectable methods */
    uwp_factory_GetIids,
    uwp_factory_GetRuntimeClassName,
    uwp_factory_GetTrustLevel,
    /* IActivationFactory methods */
    uwp_factory_ActivateInstance,
};

static IActivationFactory uwp_save_factory_impl = { &uwp_factory_vtbl };
IActivationFactory *uwp_save_picker_factory = &uwp_save_factory_impl;
