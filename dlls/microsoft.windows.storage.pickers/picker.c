/* WinRT Microsoft.Windows.Storage.Pickers (Windows App SDK) implementation
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

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(pickers);

static HRESULT return_class_name( const WCHAR *name, HSTRING *class_name )
{
    if (!class_name) return E_POINTER;
    return WindowsCreateString( name, wcslen( name ), class_name );
}

static HRESULT get_hstring( const WCHAR *str, HSTRING *value )
{
    if (!value) return E_POINTER;
    if (!str) { *value = NULL; return S_OK; }
    return WindowsCreateString( str, wcslen( str ), value );
}

static HRESULT set_hstring( WCHAR **str, HSTRING value )
{
    free( *str );
    *str = hstring_dup( value );
    return S_OK;
}

static WCHAR *wcsdup_or_null( const WCHAR *str )
{
    return str ? wcsdup( str ) : NULL;
}

/*
 * PickFileResult / PickFolderResult
 */

struct pick_result
{
    IPickFileResult IPickFileResult_iface;
    IPickFolderResult IPickFolderResult_iface;
    BOOL folder;
    LONG ref;
    WCHAR *path;
};

static inline struct pick_result *impl_from_IPickFileResult( IPickFileResult *iface )
{
    return CONTAINING_RECORD( iface, struct pick_result, IPickFileResult_iface );
}

static HRESULT WINAPI file_result_QueryInterface( IPickFileResult *iface, REFIID iid, void **out )
{
    struct pick_result *impl = impl_from_IPickFileResult( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        (!impl->folder && IsEqualGUID( iid, &IID_IPickFileResult )))
    {
        *out = &impl->IPickFileResult_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }
    if (impl->folder && IsEqualGUID( iid, &IID_IPickFolderResult ))
    {
        *out = &impl->IPickFolderResult_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI file_result_AddRef( IPickFileResult *iface )
{
    struct pick_result *impl = impl_from_IPickFileResult( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI file_result_Release( IPickFileResult *iface )
{
    struct pick_result *impl = impl_from_IPickFileResult( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref)
    {
        free( impl->path );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI file_result_GetIids( IPickFileResult *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI file_result_GetRuntimeClassName( IPickFileResult *iface, HSTRING *class_name )
{
    struct pick_result *impl = impl_from_IPickFileResult( iface );
    return return_class_name( impl->folder ? RuntimeClass_Microsoft_Windows_Storage_Pickers_PickFolderResult
                                           : RuntimeClass_Microsoft_Windows_Storage_Pickers_PickFileResult, class_name );
}

static HRESULT WINAPI file_result_GetTrustLevel( IPickFileResult *iface, TrustLevel *trust_level )
{
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI file_result_get_Path( IPickFileResult *iface, HSTRING *value )
{
    struct pick_result *impl = impl_from_IPickFileResult( iface );
    TRACE( "iface %p, value %p -> %s.\n", iface, value, debugstr_w( impl->path ) );
    return get_hstring( impl->path, value );
}

static const struct IPickFileResultVtbl file_result_vtbl =
{
    file_result_QueryInterface,
    file_result_AddRef,
    file_result_Release,
    /* IInspectable methods */
    file_result_GetIids,
    file_result_GetRuntimeClassName,
    file_result_GetTrustLevel,
    /* IPickFileResult methods */
    file_result_get_Path,
};

DEFINE_IINSPECTABLE( folder_result, IPickFolderResult, struct pick_result, IPickFileResult_iface )

static HRESULT WINAPI folder_result_get_Path( IPickFolderResult *iface, HSTRING *value )
{
    struct pick_result *impl = impl_from_IPickFolderResult( iface );
    TRACE( "iface %p, value %p -> %s.\n", iface, value, debugstr_w( impl->path ) );
    return get_hstring( impl->path, value );
}

static const struct IPickFolderResultVtbl folder_result_vtbl =
{
    folder_result_QueryInterface,
    folder_result_AddRef,
    folder_result_Release,
    /* IInspectable methods */
    folder_result_GetIids,
    folder_result_GetRuntimeClassName,
    folder_result_GetTrustLevel,
    /* IPickFolderResult methods */
    folder_result_get_Path,
};

static HRESULT pick_result_create( const WCHAR *path, BOOL folder, struct pick_result **out )
{
    struct pick_result *impl;

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IPickFileResult_iface.lpVtbl = &file_result_vtbl;
    impl->IPickFolderResult_iface.lpVtbl = &folder_result_vtbl;
    impl->folder = folder;
    impl->ref = 1;
    if (!(impl->path = wcsdup( path )))
    {
        free( impl );
        return E_OUTOFMEMORY;
    }
    *out = impl;
    return S_OK;
}

HRESULT pick_file_result_create( const WCHAR *path, IPickFileResult **out )
{
    struct pick_result *impl;
    HRESULT hr;
    if (FAILED(hr = pick_result_create( path, FALSE, &impl ))) return hr;
    *out = &impl->IPickFileResult_iface;
    return S_OK;
}

HRESULT pick_folder_result_create( const WCHAR *path, IPickFolderResult **out )
{
    struct pick_result *impl;
    HRESULT hr;
    if (FAILED(hr = pick_result_create( path, TRUE, &impl ))) return hr;
    *out = &impl->IPickFolderResult_iface;
    return S_OK;
}

/*
 * async callbacks (run on a thread pool thread, never on the caller's UI thread)
 */

static HRESULT pick_single_async( IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async )
{
    struct picker_request *request = CONTAINING_RECORD( param, struct picker_request, IUnknown_iface );
    IPickFolderResult *folder;
    IPickFileResult *file;
    WCHAR *paths;
    HRESULT hr = S_OK;

    if (!called_async) return STATUS_PENDING;

    if (!(paths = picker_run_dialog( request )))
    {
        /* cancelled (or no backend): completes with a null result, like Windows */
        result->vt = VT_EMPTY;
        return S_OK;
    }

    if (request->kind == PICKER_KIND_FOLDER)
    {
        if (SUCCEEDED(hr = pick_folder_result_create( paths, &folder )))
        {
            result->vt = VT_UNKNOWN;
            result->punkVal = (IUnknown *)folder;
        }
    }
    else if (SUCCEEDED(hr = pick_file_result_create( paths, &file )))
    {
        result->vt = VT_UNKNOWN;
        result->punkVal = (IUnknown *)file;
    }
    free( paths );
    return hr;
}

static HRESULT pick_multiple_async( IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async )
{
    struct picker_request *request = CONTAINING_RECORD( param, struct picker_request, IUnknown_iface );
    struct vector_iids iids =
    {
        .iterable = &IID_IIterable_PickFileResult,
        .iterator = &IID_IIterator_PickFileResult,
        .vector = &IID_IVector_PickFileResult,
        .view = &IID_IVectorView_PickFileResult,
    };
    IVectorView_IInspectable *view;
    IVector_IInspectable *vector;
    IPickFileResult *file;
    WCHAR *paths, *p;
    HRESULT hr;

    if (!called_async) return STATUS_PENDING;

    if (FAILED(hr = vector_inspectable_create( &iids, &vector ))) return hr;

    /* a cancel yields an empty list */
    if ((paths = picker_run_dialog( request )))
    {
        for (p = paths; *p; p += wcslen( p ) + 1)
        {
            if (FAILED(hr = pick_file_result_create( p, &file ))) break;
            hr = IVector_IInspectable_Append( vector, (IInspectable *)file );
            IPickFileResult_Release( file );
            if (FAILED(hr)) break;
        }
        free( paths );
    }

    if (SUCCEEDED(hr)) hr = IVector_IInspectable_GetView( vector, &view );
    IVector_IInspectable_Release( vector );
    if (FAILED(hr)) return hr;

    result->vt = VT_UNKNOWN;
    result->punkVal = (IUnknown *)view;
    return S_OK;
}

/* FileTypeFilter entries (".png", "png", "*") -> "Label\t*.png;*.zip\n" lines */
static WCHAR *build_open_filters( IVector_HSTRING *filter )
{
    WCHAR *patterns = NULL, *label = NULL, *ret = NULL, *tmp;
    size_t patterns_len = 0, label_len = 0;
    BOOL all = FALSE;
    UINT32 i, size = 0;
    HSTRING str;

    if (!filter || FAILED(IVector_HSTRING_get_Size( filter, &size )) || !size) return NULL;

    for (i = 0; i < size; i++)
    {
        const WCHAR *buf;
        UINT32 len;

        if (FAILED(IVector_HSTRING_GetAt( filter, i, &str ))) continue;
        buf = WindowsGetStringRawBuffer( str, &len );
        if (len >= 2 && buf[0] == '*' && buf[1] == '.') { buf += 2; len -= 2; }
        else if (len >= 1 && buf[0] == '.') { buf++; len--; }
        if (!len || (len == 1 && *buf == '*'))
        {
            all = TRUE;
            WindowsDeleteString( str );
            continue;
        }
        /* "*.png;" + label "*.png, " */
        if (!(tmp = realloc( patterns, (patterns_len + len + 4) * sizeof(WCHAR) ))) break;
        patterns = tmp;
        patterns_len += swprintf( patterns + patterns_len, len + 4, L"%s*.%.*s", patterns_len ? L";" : L"", len, buf );
        if (!(tmp = realloc( label, (label_len + len + 5) * sizeof(WCHAR) ))) break;
        label = tmp;
        label_len += swprintf( label + label_len, len + 5, L"%s*.%.*s", label_len ? L", " : L"", len, buf );
        WindowsDeleteString( str );
    }

    if (patterns_len)
    {
        size_t len = label_len + patterns_len + 64;
        if ((ret = malloc( len * sizeof(WCHAR) )))
            swprintf( ret, len, L"Supported files (%s)\t%s\n%s", label, patterns, all ? L"All files\t*\n" : L"" );
    }
    else if (all) ret = wcsdup( L"All files\t*\n" );

    free( patterns );
    free( label );
    return ret;
}

/*
 * FileOpenPicker
 */

struct file_open_picker
{
    IFileOpenPicker IFileOpenPicker_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    UINT64 window_id;
    PickerViewMode view_mode;
    PickerLocationId start_location;
    WCHAR *commit_text;
    IVector_HSTRING *filter;
};

static inline struct file_open_picker *impl_from_IFileOpenPicker( IFileOpenPicker *iface )
{
    return CONTAINING_RECORD( iface, struct file_open_picker, IFileOpenPicker_iface );
}

static HRESULT WINAPI open_picker_QueryInterface( IFileOpenPicker *iface, REFIID iid, void **out )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IFileOpenPicker ))
    {
        *out = &impl->IFileOpenPicker_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI open_picker_AddRef( IFileOpenPicker *iface )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI open_picker_Release( IFileOpenPicker *iface )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref)
    {
        IVector_HSTRING_Release( impl->filter );
        free( impl->commit_text );
        impl->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI open_picker_GetIids( IFileOpenPicker *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI open_picker_GetRuntimeClassName( IFileOpenPicker *iface, HSTRING *class_name )
{
    return return_class_name( RuntimeClass_Microsoft_Windows_Storage_Pickers_FileOpenPicker, class_name );
}

static HRESULT WINAPI open_picker_GetTrustLevel( IFileOpenPicker *iface, TrustLevel *trust_level )
{
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI open_picker_get_ViewMode( IFileOpenPicker *iface, PickerViewMode *value )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = impl->view_mode;
    return S_OK;
}

static HRESULT WINAPI open_picker_put_ViewMode( IFileOpenPicker *iface, PickerViewMode value )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    TRACE( "iface %p, value %d.\n", iface, value );
    if (value != PickerViewMode_List && value != PickerViewMode_Thumbnail) return E_INVALIDARG;
    impl->view_mode = value;
    return S_OK;
}

static HRESULT WINAPI open_picker_get_SuggestedStartLocation( IFileOpenPicker *iface, PickerLocationId *value )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = impl->start_location;
    return S_OK;
}

static HRESULT WINAPI open_picker_put_SuggestedStartLocation( IFileOpenPicker *iface, PickerLocationId value )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    TRACE( "iface %p, value %d.\n", iface, value );
    impl->start_location = value;
    return S_OK;
}

static HRESULT WINAPI open_picker_get_CommitButtonText( IFileOpenPicker *iface, HSTRING *value )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    HRESULT hr;
    TRACE( "iface %p, value %p.\n", iface, value );
    EnterCriticalSection( &impl->cs );
    hr = get_hstring( impl->commit_text, value );
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI open_picker_put_CommitButtonText( IFileOpenPicker *iface, HSTRING value )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    HRESULT hr;
    TRACE( "iface %p, value %s.\n", iface, debugstr_hstring( value ) );
    EnterCriticalSection( &impl->cs );
    hr = set_hstring( &impl->commit_text, value );
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI open_picker_get_FileTypeFilter( IFileOpenPicker *iface, IVector_HSTRING **value )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    IVector_HSTRING_AddRef( (*value = impl->filter) );
    return S_OK;
}

static HRESULT open_picker_make_request( struct file_open_picker *impl, enum picker_kind kind, struct picker_request **out )
{
    struct picker_request *request;
    HRESULT hr;

    if (FAILED(hr = picker_request_create( kind, impl->window_id, &request ))) return hr;
    EnterCriticalSection( &impl->cs );
    request->start_location = impl->start_location;
    request->accept_label = wcsdup_or_null( impl->commit_text );
    LeaveCriticalSection( &impl->cs );
    request->filters = build_open_filters( impl->filter );
    TRACE( "filters %s\n", debugstr_w( request->filters ) );
    *out = request;
    return S_OK;
}

static HRESULT WINAPI open_picker_PickSingleFileAsync( IFileOpenPicker *iface, IAsyncOperation_PickFileResult **operation )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    struct picker_request *request;
    HRESULT hr;

    TRACE( "iface %p, operation %p.\n", iface, operation );

    if (!operation) return E_POINTER;
    if (FAILED(hr = open_picker_make_request( impl, PICKER_KIND_OPEN_SINGLE, &request ))) return hr;
    hr = async_operation_inspectable_create( &IID_IAsyncOperation_PickFileResult, (IUnknown *)iface, &request->IUnknown_iface,
                                             pick_single_async, (IAsyncOperation_IInspectable **)operation );
    IUnknown_Release( &request->IUnknown_iface );
    return hr;
}

static HRESULT WINAPI open_picker_PickMultipleFilesAsync( IFileOpenPicker *iface, IAsyncOperation_IVectorView_PickFileResult **operation )
{
    struct file_open_picker *impl = impl_from_IFileOpenPicker( iface );
    struct picker_request *request;
    HRESULT hr;

    TRACE( "iface %p, operation %p.\n", iface, operation );

    if (!operation) return E_POINTER;
    if (FAILED(hr = open_picker_make_request( impl, PICKER_KIND_OPEN_MULTIPLE, &request ))) return hr;
    hr = async_operation_inspectable_create( &IID_IAsyncOperation_IVectorView_PickFileResult, (IUnknown *)iface,
                                             &request->IUnknown_iface, pick_multiple_async,
                                             (IAsyncOperation_IInspectable **)operation );
    IUnknown_Release( &request->IUnknown_iface );
    return hr;
}

static const struct IFileOpenPickerVtbl open_picker_vtbl =
{
    open_picker_QueryInterface,
    open_picker_AddRef,
    open_picker_Release,
    /* IInspectable methods */
    open_picker_GetIids,
    open_picker_GetRuntimeClassName,
    open_picker_GetTrustLevel,
    /* IFileOpenPicker methods */
    open_picker_get_ViewMode,
    open_picker_put_ViewMode,
    open_picker_get_SuggestedStartLocation,
    open_picker_put_SuggestedStartLocation,
    open_picker_get_CommitButtonText,
    open_picker_put_CommitButtonText,
    open_picker_get_FileTypeFilter,
    open_picker_PickSingleFileAsync,
    open_picker_PickMultipleFilesAsync,
};

/*
 * FileSavePicker
 */

struct file_save_picker
{
    IFileSavePicker IFileSavePicker_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    UINT64 window_id;
    PickerLocationId start_location;
    WCHAR *commit_text;
    WCHAR *default_ext;
    WCHAR *suggested_name;
    WCHAR *suggested_folder;
    IMap_HSTRING_IVector_HSTRING *choices;
};

static inline struct file_save_picker *impl_from_IFileSavePicker( IFileSavePicker *iface )
{
    return CONTAINING_RECORD( iface, struct file_save_picker, IFileSavePicker_iface );
}

static HRESULT WINAPI save_picker_QueryInterface( IFileSavePicker *iface, REFIID iid, void **out )
{
    struct file_save_picker *impl = impl_from_IFileSavePicker( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IFileSavePicker ))
    {
        *out = &impl->IFileSavePicker_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI save_picker_AddRef( IFileSavePicker *iface )
{
    struct file_save_picker *impl = impl_from_IFileSavePicker( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI save_picker_Release( IFileSavePicker *iface )
{
    struct file_save_picker *impl = impl_from_IFileSavePicker( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref)
    {
        IMap_HSTRING_IVector_HSTRING_Release( impl->choices );
        free( impl->commit_text );
        free( impl->default_ext );
        free( impl->suggested_name );
        free( impl->suggested_folder );
        impl->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI save_picker_GetIids( IFileSavePicker *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI save_picker_GetRuntimeClassName( IFileSavePicker *iface, HSTRING *class_name )
{
    return return_class_name( RuntimeClass_Microsoft_Windows_Storage_Pickers_FileSavePicker, class_name );
}

static HRESULT WINAPI save_picker_GetTrustLevel( IFileSavePicker *iface, TrustLevel *trust_level )
{
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI save_picker_get_SuggestedStartLocation( IFileSavePicker *iface, PickerLocationId *value )
{
    struct file_save_picker *impl = impl_from_IFileSavePicker( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = impl->start_location;
    return S_OK;
}

static HRESULT WINAPI save_picker_put_SuggestedStartLocation( IFileSavePicker *iface, PickerLocationId value )
{
    struct file_save_picker *impl = impl_from_IFileSavePicker( iface );
    TRACE( "iface %p, value %d.\n", iface, value );
    impl->start_location = value;
    return S_OK;
}

#define SAVE_PICKER_STRING_PROPERTY( name, field )                                                 \
    static HRESULT WINAPI save_picker_get_##name( IFileSavePicker *iface, HSTRING *value )        \
    {                                                                                              \
        struct file_save_picker *impl = impl_from_IFileSavePicker( iface );                        \
        HRESULT hr;                                                                                \
        TRACE( "iface %p, value %p.\n", iface, value );                                            \
        EnterCriticalSection( &impl->cs );                                                         \
        hr = get_hstring( impl->field, value );                                                    \
        LeaveCriticalSection( &impl->cs );                                                         \
        return hr;                                                                                 \
    }                                                                                              \
    static HRESULT WINAPI save_picker_put_##name( IFileSavePicker *iface, HSTRING value )         \
    {                                                                                              \
        struct file_save_picker *impl = impl_from_IFileSavePicker( iface );                        \
        HRESULT hr;                                                                                \
        TRACE( "iface %p, value %s.\n", iface, debugstr_hstring( value ) );                        \
        EnterCriticalSection( &impl->cs );                                                         \
        hr = set_hstring( &impl->field, value );                                                   \
        LeaveCriticalSection( &impl->cs );                                                         \
        return hr;                                                                                 \
    }

SAVE_PICKER_STRING_PROPERTY( CommitButtonText, commit_text )
SAVE_PICKER_STRING_PROPERTY( DefaultFileExtension, default_ext )
SAVE_PICKER_STRING_PROPERTY( SuggestedFileName, suggested_name )
SAVE_PICKER_STRING_PROPERTY( SuggestedFolder, suggested_folder )

static HRESULT WINAPI save_picker_get_FileTypeChoices( IFileSavePicker *iface, IMap_HSTRING_IVector_HSTRING **value )
{
    struct file_save_picker *impl = impl_from_IFileSavePicker( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    IMap_HSTRING_IVector_HSTRING_AddRef( (*value = impl->choices) );
    return S_OK;
}

static HRESULT WINAPI save_picker_PickSaveFileAsync( IFileSavePicker *iface, IAsyncOperation_PickFileResult **operation )
{
    struct file_save_picker *impl = impl_from_IFileSavePicker( iface );
    struct picker_request *request;
    HRESULT hr;

    TRACE( "iface %p, operation %p.\n", iface, operation );

    if (!operation) return E_POINTER;
    if (FAILED(hr = picker_request_create( PICKER_KIND_SAVE, impl->window_id, &request ))) return hr;
    EnterCriticalSection( &impl->cs );
    request->start_location = impl->start_location;
    request->accept_label = wcsdup_or_null( impl->commit_text );
    request->current_name = wcsdup_or_null( impl->suggested_name );
    request->current_folder = wcsdup_or_null( impl->suggested_folder );
    request->default_ext = wcsdup_or_null( impl->default_ext );
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

    hr = async_operation_inspectable_create( &IID_IAsyncOperation_PickFileResult, (IUnknown *)iface, &request->IUnknown_iface,
                                             pick_single_async, (IAsyncOperation_IInspectable **)operation );
    IUnknown_Release( &request->IUnknown_iface );
    return hr;
}

static const struct IFileSavePickerVtbl save_picker_vtbl =
{
    save_picker_QueryInterface,
    save_picker_AddRef,
    save_picker_Release,
    /* IInspectable methods */
    save_picker_GetIids,
    save_picker_GetRuntimeClassName,
    save_picker_GetTrustLevel,
    /* IFileSavePicker methods */
    save_picker_get_SuggestedStartLocation,
    save_picker_put_SuggestedStartLocation,
    save_picker_get_CommitButtonText,
    save_picker_put_CommitButtonText,
    save_picker_get_FileTypeChoices,
    save_picker_get_DefaultFileExtension,
    save_picker_put_DefaultFileExtension,
    save_picker_get_SuggestedFileName,
    save_picker_put_SuggestedFileName,
    save_picker_get_SuggestedFolder,
    save_picker_put_SuggestedFolder,
    save_picker_PickSaveFileAsync,
};

/*
 * FolderPicker
 */

struct folder_picker
{
    IFolderPicker IFolderPicker_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    UINT64 window_id;
    PickerViewMode view_mode;
    PickerLocationId start_location;
    WCHAR *commit_text;
};

static inline struct folder_picker *impl_from_IFolderPicker( IFolderPicker *iface )
{
    return CONTAINING_RECORD( iface, struct folder_picker, IFolderPicker_iface );
}

static HRESULT WINAPI folder_picker_QueryInterface( IFolderPicker *iface, REFIID iid, void **out )
{
    struct folder_picker *impl = impl_from_IFolderPicker( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IFolderPicker ))
    {
        *out = &impl->IFolderPicker_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI folder_picker_AddRef( IFolderPicker *iface )
{
    struct folder_picker *impl = impl_from_IFolderPicker( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI folder_picker_Release( IFolderPicker *iface )
{
    struct folder_picker *impl = impl_from_IFolderPicker( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref)
    {
        free( impl->commit_text );
        impl->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI folder_picker_GetIids( IFolderPicker *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI folder_picker_GetRuntimeClassName( IFolderPicker *iface, HSTRING *class_name )
{
    return return_class_name( RuntimeClass_Microsoft_Windows_Storage_Pickers_FolderPicker, class_name );
}

static HRESULT WINAPI folder_picker_GetTrustLevel( IFolderPicker *iface, TrustLevel *trust_level )
{
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI folder_picker_get_ViewMode( IFolderPicker *iface, PickerViewMode *value )
{
    struct folder_picker *impl = impl_from_IFolderPicker( iface );
    if (!value) return E_POINTER;
    *value = impl->view_mode;
    return S_OK;
}

static HRESULT WINAPI folder_picker_put_ViewMode( IFolderPicker *iface, PickerViewMode value )
{
    struct folder_picker *impl = impl_from_IFolderPicker( iface );
    if (value != PickerViewMode_List && value != PickerViewMode_Thumbnail) return E_INVALIDARG;
    impl->view_mode = value;
    return S_OK;
}

static HRESULT WINAPI folder_picker_get_SuggestedStartLocation( IFolderPicker *iface, PickerLocationId *value )
{
    struct folder_picker *impl = impl_from_IFolderPicker( iface );
    if (!value) return E_POINTER;
    *value = impl->start_location;
    return S_OK;
}

static HRESULT WINAPI folder_picker_put_SuggestedStartLocation( IFolderPicker *iface, PickerLocationId value )
{
    struct folder_picker *impl = impl_from_IFolderPicker( iface );
    impl->start_location = value;
    return S_OK;
}

static HRESULT WINAPI folder_picker_get_CommitButtonText( IFolderPicker *iface, HSTRING *value )
{
    struct folder_picker *impl = impl_from_IFolderPicker( iface );
    HRESULT hr;
    EnterCriticalSection( &impl->cs );
    hr = get_hstring( impl->commit_text, value );
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI folder_picker_put_CommitButtonText( IFolderPicker *iface, HSTRING value )
{
    struct folder_picker *impl = impl_from_IFolderPicker( iface );
    HRESULT hr;
    TRACE( "iface %p, value %s.\n", iface, debugstr_hstring( value ) );
    EnterCriticalSection( &impl->cs );
    hr = set_hstring( &impl->commit_text, value );
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI folder_picker_PickSingleFolderAsync( IFolderPicker *iface, IAsyncOperation_PickFolderResult **operation )
{
    struct folder_picker *impl = impl_from_IFolderPicker( iface );
    struct picker_request *request;
    HRESULT hr;

    TRACE( "iface %p, operation %p.\n", iface, operation );

    if (!operation) return E_POINTER;
    if (FAILED(hr = picker_request_create( PICKER_KIND_FOLDER, impl->window_id, &request ))) return hr;
    EnterCriticalSection( &impl->cs );
    request->start_location = impl->start_location;
    request->accept_label = wcsdup_or_null( impl->commit_text );
    LeaveCriticalSection( &impl->cs );

    hr = async_operation_inspectable_create( &IID_IAsyncOperation_PickFolderResult, (IUnknown *)iface, &request->IUnknown_iface,
                                             pick_single_async, (IAsyncOperation_IInspectable **)operation );
    IUnknown_Release( &request->IUnknown_iface );
    return hr;
}

static const struct IFolderPickerVtbl folder_picker_vtbl =
{
    folder_picker_QueryInterface,
    folder_picker_AddRef,
    folder_picker_Release,
    /* IInspectable methods */
    folder_picker_GetIids,
    folder_picker_GetRuntimeClassName,
    folder_picker_GetTrustLevel,
    /* IFolderPicker methods */
    folder_picker_get_ViewMode,
    folder_picker_put_ViewMode,
    folder_picker_get_SuggestedStartLocation,
    folder_picker_put_SuggestedStartLocation,
    folder_picker_get_CommitButtonText,
    folder_picker_put_CommitButtonText,
    folder_picker_PickSingleFolderAsync,
};

/*
 * Activation factories
 */

static void init_cs( CRITICAL_SECTION *cs, const char *name )
{
    InitializeCriticalSectionEx( cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO );
    cs->DebugInfo->Spare[0] = (DWORD_PTR)name;
}

static HRESULT create_file_open_picker( WindowId window_id, IFileOpenPicker **value )
{
    struct file_open_picker *impl;
    HRESULT hr;

    TRACE( "window_id %s, value %p.\n", wine_dbgstr_longlong( window_id.Value ), value );

    if (!value) return E_POINTER;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IFileOpenPicker_iface.lpVtbl = &open_picker_vtbl;
    impl->ref = 1;
    impl->window_id = window_id.Value;
    impl->view_mode = PickerViewMode_List;
    impl->start_location = PickerLocationId_Unspecified;
    if (FAILED(hr = vector_hstring_create( &impl->filter )))
    {
        free( impl );
        return hr;
    }
    init_cs( &impl->cs, __FILE__ ": file_open_picker.cs" );
    *value = &impl->IFileOpenPicker_iface;
    TRACE( "created FileOpenPicker %p\n", *value );
    return S_OK;
}

static HRESULT create_file_save_picker( WindowId window_id, IFileSavePicker **value )
{
    struct file_save_picker *impl;
    HRESULT hr;

    TRACE( "window_id %s, value %p.\n", wine_dbgstr_longlong( window_id.Value ), value );

    if (!value) return E_POINTER;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IFileSavePicker_iface.lpVtbl = &save_picker_vtbl;
    impl->ref = 1;
    impl->window_id = window_id.Value;
    impl->start_location = PickerLocationId_Unspecified;
    if (FAILED(hr = file_type_choices_create( &impl->choices )))
    {
        free( impl );
        return hr;
    }
    init_cs( &impl->cs, __FILE__ ": file_save_picker.cs" );
    *value = &impl->IFileSavePicker_iface;
    TRACE( "created FileSavePicker %p\n", *value );
    return S_OK;
}

static HRESULT create_folder_picker( WindowId window_id, IFolderPicker **value )
{
    struct folder_picker *impl;

    TRACE( "window_id %s, value %p.\n", wine_dbgstr_longlong( window_id.Value ), value );

    if (!value) return E_POINTER;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IFolderPicker_iface.lpVtbl = &folder_picker_vtbl;
    impl->ref = 1;
    impl->window_id = window_id.Value;
    impl->view_mode = PickerViewMode_List;
    impl->start_location = PickerLocationId_Unspecified;
    init_cs( &impl->cs, __FILE__ ": folder_picker.cs" );
    *value = &impl->IFolderPicker_iface;
    TRACE( "created FolderPicker %p\n", *value );
    return S_OK;
}

struct picker_factory
{
    IActivationFactory IActivationFactory_iface;
    union
    {
        IFileOpenPickerFactory IFileOpenPickerFactory_iface;
        IFileSavePickerFactory IFileSavePickerFactory_iface;
        IFolderPickerFactory IFolderPickerFactory_iface;
        IInspectable IInspectable_factory;
    };
    const GUID *factory_iid;
    const WCHAR *class_name;
    LONG ref;
};

static inline struct picker_factory *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct picker_factory, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct picker_factory *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, impl->factory_iid ))
    {
        *out = &impl->IInspectable_factory;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s: %s not implemented, returning E_NOINTERFACE.\n", debugstr_w( impl->class_name ), debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct picker_factory *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct picker_factory *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    struct picker_factory *impl = impl_from_IActivationFactory( iface );
    return return_class_name( impl->class_name, class_name );
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct picker_factory *impl = impl_from_IActivationFactory( iface );
    /* pickers have no default constructor (they need a WindowId) */
    FIXME( "%s: iface %p, instance %p: no default constructor\n", debugstr_w( impl->class_name ), iface, instance );
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    /* IInspectable methods */
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    /* IActivationFactory methods */
    factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( open_factory, IFileOpenPickerFactory, struct picker_factory, IActivationFactory_iface )

static HRESULT WINAPI open_factory_CreateInstance( IFileOpenPickerFactory *iface, WindowId window_id, IFileOpenPicker **value )
{
    return create_file_open_picker( window_id, value );
}

static const struct IFileOpenPickerFactoryVtbl open_factory_vtbl =
{
    open_factory_QueryInterface,
    open_factory_AddRef,
    open_factory_Release,
    /* IInspectable methods */
    open_factory_GetIids,
    open_factory_GetRuntimeClassName,
    open_factory_GetTrustLevel,
    /* IFileOpenPickerFactory methods */
    open_factory_CreateInstance,
};

DEFINE_IINSPECTABLE( save_factory, IFileSavePickerFactory, struct picker_factory, IActivationFactory_iface )

static HRESULT WINAPI save_factory_CreateInstance( IFileSavePickerFactory *iface, WindowId window_id, IFileSavePicker **value )
{
    return create_file_save_picker( window_id, value );
}

static const struct IFileSavePickerFactoryVtbl save_factory_vtbl =
{
    save_factory_QueryInterface,
    save_factory_AddRef,
    save_factory_Release,
    /* IInspectable methods */
    save_factory_GetIids,
    save_factory_GetRuntimeClassName,
    save_factory_GetTrustLevel,
    /* IFileSavePickerFactory methods */
    save_factory_CreateInstance,
};

DEFINE_IINSPECTABLE( folder_factory, IFolderPickerFactory, struct picker_factory, IActivationFactory_iface )

static HRESULT WINAPI folder_factory_CreateInstance( IFolderPickerFactory *iface, WindowId window_id, IFolderPicker **value )
{
    return create_folder_picker( window_id, value );
}

static const struct IFolderPickerFactoryVtbl folder_factory_vtbl =
{
    folder_factory_QueryInterface,
    folder_factory_AddRef,
    folder_factory_Release,
    /* IInspectable methods */
    folder_factory_GetIids,
    folder_factory_GetRuntimeClassName,
    folder_factory_GetTrustLevel,
    /* IFolderPickerFactory methods */
    folder_factory_CreateInstance,
};

static struct picker_factory open_factory =
{
    {&factory_vtbl},
    {.IFileOpenPickerFactory_iface = {&open_factory_vtbl}},
    &IID_IFileOpenPickerFactory,
    RuntimeClass_Microsoft_Windows_Storage_Pickers_FileOpenPicker,
    1,
};

static struct picker_factory save_factory =
{
    {&factory_vtbl},
    {.IFileSavePickerFactory_iface = {&save_factory_vtbl}},
    &IID_IFileSavePickerFactory,
    RuntimeClass_Microsoft_Windows_Storage_Pickers_FileSavePicker,
    1,
};

static struct picker_factory folder_factory =
{
    {&factory_vtbl},
    {.IFolderPickerFactory_iface = {&folder_factory_vtbl}},
    &IID_IFolderPickerFactory,
    RuntimeClass_Microsoft_Windows_Storage_Pickers_FolderPicker,
    1,
};

IActivationFactory *file_open_picker_factory = &open_factory.IActivationFactory_iface;
IActivationFactory *file_save_picker_factory = &save_factory.IActivationFactory_iface;
IActivationFactory *folder_picker_factory = &folder_factory.IActivationFactory_iface;
