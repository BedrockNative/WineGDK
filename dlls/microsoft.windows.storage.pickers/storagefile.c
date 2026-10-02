/* Windows.Storage.StorageFile (minimal, backed by Win32 files)
 *
 * Minecraft's file import flow gets a path from Microsoft.Windows.Storage.Pickers and then calls
 * Windows.Storage.StorageFile.GetFileFromPathAsync() to wrap it. Wine's windows.storage has no StorageFile class
 * (activation returned CLASS_E_CLASSNOTAVAILABLE, which the game's C++/WinRT code turns into a fail-fast), so this
 * provides the statics and a read-only file object. Stream/copy/move/delete operations return E_NOTIMPL.
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

#include "initguid.h"
#define WIDL_using_Windows_Storage
#define WIDL_using_Windows_Storage_Streams
#include "windows.storage.h"
#include "windows.storage.streams.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(pickers);

/* IStorageFileStatics {5984c710-daf2-43c8-8bb4-a4d3eacfd03f} (not defined in Wine's windows.storage.idl) */
DEFINE_GUID( IID_IOrionStorageFileStatics, 0x5984c710, 0xdaf2, 0x43c8, 0x8b, 0xb4, 0xa4, 0xd3, 0xea, 0xcf, 0xd0, 0x3f );

typedef struct IOrionStorageFileStatics IOrionStorageFileStatics;

typedef struct IOrionStorageFileStaticsVtbl
{
    BEGIN_INTERFACE
    HRESULT (WINAPI *QueryInterface)( IOrionStorageFileStatics *iface, REFIID iid, void **out );
    ULONG (WINAPI *AddRef)( IOrionStorageFileStatics *iface );
    ULONG (WINAPI *Release)( IOrionStorageFileStatics *iface );
    HRESULT (WINAPI *GetIids)( IOrionStorageFileStatics *iface, ULONG *iid_count, IID **iids );
    HRESULT (WINAPI *GetRuntimeClassName)( IOrionStorageFileStatics *iface, HSTRING *class_name );
    HRESULT (WINAPI *GetTrustLevel)( IOrionStorageFileStatics *iface, TrustLevel *trust_level );
    HRESULT (WINAPI *GetFileFromPathAsync)( IOrionStorageFileStatics *iface, HSTRING path, IAsyncOperation_StorageFile **operation );
    HRESULT (WINAPI *GetFileFromApplicationUriAsync)( IOrionStorageFileStatics *iface, void *uri, IAsyncOperation_StorageFile **operation );
    HRESULT (WINAPI *CreateStreamedFileAsync)( IOrionStorageFileStatics *iface, HSTRING name, void *handler, void *thumbnail, IAsyncOperation_StorageFile **operation );
    HRESULT (WINAPI *ReplaceWithStreamedFileAsync)( IOrionStorageFileStatics *iface, void *file, void *handler, void *thumbnail, IAsyncOperation_StorageFile **operation );
    HRESULT (WINAPI *CreateStreamedFileFromUriAsync)( IOrionStorageFileStatics *iface, HSTRING name, void *uri, void *thumbnail, IAsyncOperation_StorageFile **operation );
    HRESULT (WINAPI *ReplaceWithStreamedFileFromUriAsync)( IOrionStorageFileStatics *iface, void *file, void *uri, void *thumbnail, IAsyncOperation_StorageFile **operation );
    END_INTERFACE
} IOrionStorageFileStaticsVtbl;

struct IOrionStorageFileStatics
{
    const IOrionStorageFileStaticsVtbl *lpVtbl;
};

#define NOTIMPL_STUB( name )                                              \
    static HRESULT WINAPI stub_##name( void )                             \
    {                                                                     \
        FIXME( "%s is not implemented, returning E_NOTIMPL.\n", #name ); \
        return E_NOTIMPL;                                                 \
    }

NOTIMPL_STUB( OpenAsync )
NOTIMPL_STUB( OpenTransactedWriteAsync )
NOTIMPL_STUB( CopyOverloadDefaultNameAndOptions )
NOTIMPL_STUB( CopyOverloadDefaultOptions )
NOTIMPL_STUB( CopyOverload )
NOTIMPL_STUB( CopyAndReplaceAsync )
NOTIMPL_STUB( MoveOverloadDefaultNameAndOptions )
NOTIMPL_STUB( MoveOverloadDefaultOptions )
NOTIMPL_STUB( MoveOverload )
NOTIMPL_STUB( MoveAndReplaceAsync )
NOTIMPL_STUB( RenameAsyncOverloadDefaultOptions )
NOTIMPL_STUB( RenameAsync )
NOTIMPL_STUB( DeleteAsyncOverloadDefaultOptions )
NOTIMPL_STUB( DeleteAsync )
NOTIMPL_STUB( GetBasicPropertiesAsync )
NOTIMPL_STUB( OpenReadAsync )
NOTIMPL_STUB( OpenSequentialReadAsync )
NOTIMPL_STUB( GetFileFromApplicationUriAsync )
NOTIMPL_STUB( CreateStreamedFileAsync )
NOTIMPL_STUB( ReplaceWithStreamedFileAsync )
NOTIMPL_STUB( CreateStreamedFileFromUriAsync )
NOTIMPL_STUB( ReplaceWithStreamedFileFromUriAsync )

static HRESULT return_class_name( const WCHAR *name, HSTRING *class_name )
{
    if (!class_name) return E_POINTER;
    return WindowsCreateString( name, wcslen( name ), class_name );
}

/*
 * StorageFile
 */

struct storage_file
{
    IStorageFile IStorageFile_iface;
    IStorageItem IStorageItem_iface;
    IRandomAccessStreamReference IRandomAccessStreamReference_iface;
    IInputStreamReference IInputStreamReference_iface;
    LONG ref;
    WCHAR *path;
};

static inline struct storage_file *impl_from_IStorageFile( IStorageFile *iface )
{
    return CONTAINING_RECORD( iface, struct storage_file, IStorageFile_iface );
}

static HRESULT WINAPI file_QueryInterface( IStorageFile *iface, REFIID iid, void **out )
{
    struct storage_file *impl = impl_from_IStorageFile( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IStorageFile ))
        *out = &impl->IStorageFile_iface;
    else if (IsEqualGUID( iid, &IID_IStorageItem ))
        *out = &impl->IStorageItem_iface;
    else if (IsEqualGUID( iid, &IID_IRandomAccessStreamReference ))
        *out = &impl->IRandomAccessStreamReference_iface;
    else if (IsEqualGUID( iid, &IID_IInputStreamReference ))
        *out = &impl->IInputStreamReference_iface;

    if (*out)
    {
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    return E_NOINTERFACE;
}

static ULONG WINAPI file_AddRef( IStorageFile *iface )
{
    struct storage_file *impl = impl_from_IStorageFile( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI file_Release( IStorageFile *iface )
{
    struct storage_file *impl = impl_from_IStorageFile( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref)
    {
        free( impl->path );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI file_GetIids( IStorageFile *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI file_GetRuntimeClassName( IStorageFile *iface, HSTRING *class_name )
{
    return return_class_name( L"Windows.Storage.StorageFile", class_name );
}

static HRESULT WINAPI file_GetTrustLevel( IStorageFile *iface, TrustLevel *trust_level )
{
    *trust_level = BaseTrust;
    return S_OK;
}

static const WCHAR *path_extension( const WCHAR *path )
{
    const WCHAR *name = wcsrchr( path, '\\' ), *ext;
    const WCHAR *slash = wcsrchr( path, '/' );

    if (slash && (!name || slash > name)) name = slash;
    name = name ? name + 1 : path;
    ext = wcsrchr( name, '.' );
    return ext ? ext : name + wcslen( name );
}

static HRESULT WINAPI file_get_FileType( IStorageFile *iface, HSTRING *value )
{
    struct storage_file *impl = impl_from_IStorageFile( iface );
    const WCHAR *ext = path_extension( impl->path );

    TRACE( "iface %p, value %p -> %s.\n", iface, value, debugstr_w( ext ) );
    if (!value) return E_POINTER;
    return WindowsCreateString( ext, wcslen( ext ), value );
}

static HRESULT WINAPI file_get_ContentType( IStorageFile *iface, HSTRING *value )
{
    struct storage_file *impl = impl_from_IStorageFile( iface );
    const WCHAR *ext = path_extension( impl->path ), *type = L"application/octet-stream";

    if (!value) return E_POINTER;
    if (!wcsicmp( ext, L".png" )) type = L"image/png";
    else if (!wcsicmp( ext, L".jpg" ) || !wcsicmp( ext, L".jpeg" )) type = L"image/jpeg";
    else if (!wcsicmp( ext, L".json" )) type = L"application/json";
    else if (!wcsicmp( ext, L".zip" ) || !wcsicmp( ext, L".mcpack" ) || !wcsicmp( ext, L".mcworld" )) type = L"application/zip";
    TRACE( "iface %p, value %p -> %s.\n", iface, value, debugstr_w( type ) );
    return WindowsCreateString( type, wcslen( type ), value );
}

static const IStorageFileVtbl storage_file_vtbl =
{
    file_QueryInterface,
    file_AddRef,
    file_Release,
    /* IInspectable methods */
    file_GetIids,
    file_GetRuntimeClassName,
    file_GetTrustLevel,
    /* IStorageFile methods */
    file_get_FileType,
    file_get_ContentType,
    (void *)stub_OpenAsync,
    (void *)stub_OpenTransactedWriteAsync,
    (void *)stub_CopyOverloadDefaultNameAndOptions,
    (void *)stub_CopyOverloadDefaultOptions,
    (void *)stub_CopyOverload,
    (void *)stub_CopyAndReplaceAsync,
    (void *)stub_MoveOverloadDefaultNameAndOptions,
    (void *)stub_MoveOverloadDefaultOptions,
    (void *)stub_MoveOverload,
    (void *)stub_MoveAndReplaceAsync,
};

DEFINE_IINSPECTABLE( item, IStorageItem, struct storage_file, IStorageFile_iface )

static HRESULT WINAPI item_get_Name( IStorageItem *iface, HSTRING *value )
{
    struct storage_file *impl = impl_from_IStorageItem( iface );
    const WCHAR *name = wcsrchr( impl->path, '\\' ), *slash = wcsrchr( impl->path, '/' );

    if (slash && (!name || slash > name)) name = slash;
    name = name ? name + 1 : impl->path;
    TRACE( "iface %p, value %p -> %s.\n", iface, value, debugstr_w( name ) );
    if (!value) return E_POINTER;
    return WindowsCreateString( name, wcslen( name ), value );
}

static HRESULT WINAPI item_get_Path( IStorageItem *iface, HSTRING *value )
{
    struct storage_file *impl = impl_from_IStorageItem( iface );
    TRACE( "iface %p, value %p -> %s.\n", iface, value, debugstr_w( impl->path ) );
    if (!value) return E_POINTER;
    return WindowsCreateString( impl->path, wcslen( impl->path ), value );
}

static HRESULT WINAPI item_get_Attributes( IStorageItem *iface, FileAttributes *value )
{
    struct storage_file *impl = impl_from_IStorageItem( iface );
    DWORD attrs = GetFileAttributesW( impl->path );

    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = FileAttributes_Normal;
    if (attrs == INVALID_FILE_ATTRIBUTES) return S_OK;
    if (attrs & FILE_ATTRIBUTE_READONLY) *value |= FileAttributes_ReadOnly;
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) *value |= FileAttributes_Directory;
    if (attrs & FILE_ATTRIBUTE_ARCHIVE) *value |= FileAttributes_Archive;
    if (attrs & FILE_ATTRIBUTE_TEMPORARY) *value |= FileAttributes_Temporary;
    return S_OK;
}

static HRESULT WINAPI item_get_DateCreated( IStorageItem *iface, DateTime *value )
{
    struct storage_file *impl = impl_from_IStorageItem( iface );
    WIN32_FILE_ATTRIBUTE_DATA data;

    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    value->UniversalTime = 0;
    if (GetFileAttributesExW( impl->path, GetFileExInfoStandard, &data ))
        value->UniversalTime = ((INT64)data.ftCreationTime.dwHighDateTime << 32) | data.ftCreationTime.dwLowDateTime;
    return S_OK;
}

static HRESULT WINAPI item_IsOfType( IStorageItem *iface, StorageItemTypes type, boolean *value )
{
    struct storage_file *impl = impl_from_IStorageItem( iface );
    DWORD attrs = GetFileAttributesW( impl->path );
    BOOL is_dir = attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);

    TRACE( "iface %p, type %#x, value %p.\n", iface, type, value );
    if (!value) return E_POINTER;
    *value = (!is_dir && (type & StorageItemTypes_File)) || (is_dir && (type & StorageItemTypes_Folder));
    return S_OK;
}

static const IStorageItemVtbl storage_item_vtbl =
{
    item_QueryInterface,
    item_AddRef,
    item_Release,
    /* IInspectable methods */
    item_GetIids,
    item_GetRuntimeClassName,
    item_GetTrustLevel,
    /* IStorageItem methods */
    (void *)stub_RenameAsyncOverloadDefaultOptions,
    (void *)stub_RenameAsync,
    (void *)stub_DeleteAsyncOverloadDefaultOptions,
    (void *)stub_DeleteAsync,
    (void *)stub_GetBasicPropertiesAsync,
    item_get_Name,
    item_get_Path,
    item_get_Attributes,
    item_get_DateCreated,
    item_IsOfType,
};

DEFINE_IINSPECTABLE( rasref, IRandomAccessStreamReference, struct storage_file, IStorageFile_iface )

static const IRandomAccessStreamReferenceVtbl random_access_stream_reference_vtbl =
{
    rasref_QueryInterface,
    rasref_AddRef,
    rasref_Release,
    /* IInspectable methods */
    rasref_GetIids,
    rasref_GetRuntimeClassName,
    rasref_GetTrustLevel,
    /* IRandomAccessStreamReference methods */
    (void *)stub_OpenReadAsync,
};

DEFINE_IINSPECTABLE( isref, IInputStreamReference, struct storage_file, IStorageFile_iface )

static const IInputStreamReferenceVtbl input_stream_reference_vtbl =
{
    isref_QueryInterface,
    isref_AddRef,
    isref_Release,
    /* IInspectable methods */
    isref_GetIids,
    isref_GetRuntimeClassName,
    isref_GetTrustLevel,
    /* IInputStreamReference methods */
    (void *)stub_OpenSequentialReadAsync,
};

static HRESULT storage_file_create( const WCHAR *path, struct storage_file **out )
{
    struct storage_file *impl;

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IStorageFile_iface.lpVtbl = &storage_file_vtbl;
    impl->IStorageItem_iface.lpVtbl = &storage_item_vtbl;
    impl->IRandomAccessStreamReference_iface.lpVtbl = &random_access_stream_reference_vtbl;
    impl->IInputStreamReference_iface.lpVtbl = &input_stream_reference_vtbl;
    impl->ref = 1;
    if (!(impl->path = wcsdup( path )))
    {
        free( impl );
        return E_OUTOFMEMORY;
    }
    *out = impl;
    return S_OK;
}

/*
 * IStorageFileStatics factory
 */

struct path_request
{
    IUnknown IUnknown_iface;
    LONG ref;
    WCHAR *path;
};

static inline struct path_request *impl_from_request( IUnknown *iface )
{
    return CONTAINING_RECORD( iface, struct path_request, IUnknown_iface );
}

static HRESULT WINAPI path_request_QueryInterface( IUnknown *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ))
    {
        *out = iface;
        IUnknown_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI path_request_AddRef( IUnknown *iface )
{
    return InterlockedIncrement( &impl_from_request( iface )->ref );
}

static ULONG WINAPI path_request_Release( IUnknown *iface )
{
    struct path_request *impl = impl_from_request( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        free( impl->path );
        free( impl );
    }
    return ref;
}

static const IUnknownVtbl path_request_vtbl =
{
    path_request_QueryInterface,
    path_request_AddRef,
    path_request_Release,
};

static HRESULT get_file_async( IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async )
{
    struct path_request *request = impl_from_request( param );
    struct storage_file *file;
    DWORD attrs;
    HRESULT hr;

    if (!called_async) return STATUS_PENDING;

    attrs = GetFileAttributesW( request->path );
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        DWORD err = GetLastError();
        WARN( "file %s not found (error %lu).\n", debugstr_w( request->path ), err );
        return HRESULT_FROM_WIN32( err ? err : ERROR_FILE_NOT_FOUND );
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
    {
        WARN( "%s is a directory.\n", debugstr_w( request->path ) );
        return HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND );
    }

    if (FAILED(hr = storage_file_create( request->path, &file ))) return hr;
    TRACE( "created StorageFile %p for %s.\n", file, debugstr_w( request->path ) );
    result->vt = VT_UNKNOWN;
    result->punkVal = (IUnknown *)&file->IStorageFile_iface;
    return S_OK;
}

struct file_factory
{
    IActivationFactory IActivationFactory_iface;
    IOrionStorageFileStatics statics_iface;
    LONG ref;
};

static inline struct file_factory *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct file_factory, IActivationFactory_iface );
}

static inline struct file_factory *impl_from_statics( IOrionStorageFileStatics *iface )
{
    return CONTAINING_RECORD( iface, struct file_factory, statics_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct file_factory *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &impl->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IOrionStorageFileStatics ))
        *out = &impl->statics_iface;

    if (*out)
    {
        IUnknown_AddRef( (IUnknown *)*out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &impl_from_IActivationFactory( iface )->ref );
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &impl_from_IActivationFactory( iface )->ref );
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    return return_class_name( L"Windows.Storage.StorageFile", class_name );
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p: StorageFile has no default constructor.\n", iface, instance );
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl factory_vtbl =
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

static HRESULT WINAPI statics_QueryInterface( IOrionStorageFileStatics *iface, REFIID iid, void **out )
{
    return factory_QueryInterface( &impl_from_statics( iface )->IActivationFactory_iface, iid, out );
}

static ULONG WINAPI statics_AddRef( IOrionStorageFileStatics *iface )
{
    return factory_AddRef( &impl_from_statics( iface )->IActivationFactory_iface );
}

static ULONG WINAPI statics_Release( IOrionStorageFileStatics *iface )
{
    return factory_Release( &impl_from_statics( iface )->IActivationFactory_iface );
}

static HRESULT WINAPI statics_GetIids( IOrionStorageFileStatics *iface, ULONG *iid_count, IID **iids )
{
    return factory_GetIids( &impl_from_statics( iface )->IActivationFactory_iface, iid_count, iids );
}

static HRESULT WINAPI statics_GetRuntimeClassName( IOrionStorageFileStatics *iface, HSTRING *class_name )
{
    return factory_GetRuntimeClassName( &impl_from_statics( iface )->IActivationFactory_iface, class_name );
}

static HRESULT WINAPI statics_GetTrustLevel( IOrionStorageFileStatics *iface, TrustLevel *trust_level )
{
    return factory_GetTrustLevel( &impl_from_statics( iface )->IActivationFactory_iface, trust_level );
}

static HRESULT WINAPI statics_GetFileFromPathAsync( IOrionStorageFileStatics *iface, HSTRING path, IAsyncOperation_StorageFile **operation )
{
    struct path_request *request;
    const WCHAR *buffer;
    HRESULT hr;

    TRACE( "iface %p, path %s, operation %p.\n", iface, debugstr_hstring( path ), operation );

    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!path) return E_INVALIDARG;

    if (!(request = calloc( 1, sizeof(*request) ))) return E_OUTOFMEMORY;
    request->IUnknown_iface.lpVtbl = &path_request_vtbl;
    request->ref = 1;
    buffer = WindowsGetStringRawBuffer( path, NULL );
    if (!(request->path = wcsdup( buffer ? buffer : L"" )))
    {
        free( request );
        return E_OUTOFMEMORY;
    }

    hr = async_operation_inspectable_create( &IID_IAsyncOperation_StorageFile, (IUnknown *)iface, &request->IUnknown_iface,
                                             get_file_async, (IAsyncOperation_IInspectable **)operation );
    IUnknown_Release( &request->IUnknown_iface );
    return hr;
}

static const IOrionStorageFileStaticsVtbl statics_vtbl =
{
    statics_QueryInterface,
    statics_AddRef,
    statics_Release,
    /* IInspectable methods */
    statics_GetIids,
    statics_GetRuntimeClassName,
    statics_GetTrustLevel,
    /* IStorageFileStatics methods */
    statics_GetFileFromPathAsync,
    (void *)stub_GetFileFromApplicationUriAsync,
    (void *)stub_CreateStreamedFileAsync,
    (void *)stub_ReplaceWithStreamedFileAsync,
    (void *)stub_CreateStreamedFileFromUriAsync,
    (void *)stub_ReplaceWithStreamedFileFromUriAsync,
};

static struct file_factory storage_file_factory_impl =
{
    {&factory_vtbl},
    {&statics_vtbl},
    1,
};

IActivationFactory *storage_file_factory = &storage_file_factory_impl.IActivationFactory_iface;
