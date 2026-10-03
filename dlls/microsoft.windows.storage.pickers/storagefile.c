/* Windows.Storage.StorageFile (minimal, backed by Win32 files)
 *
 * Minecraft's file import flow gets a path from Microsoft.Windows.Storage.Pickers and then calls
 * Windows.Storage.StorageFile.GetFileFromPathAsync() to wrap it. Wine's windows.storage has no StorageFile class
 * (activation returned CLASS_E_CLASSNOTAVAILABLE, which the game's C++/WinRT code turns into a fail-fast), so this
 * provides the statics and a read-only file object. CopyAsync is backed by Win32; stream/move/delete operations are not implemented yet.
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
#include "appmodel.h"
#include "roapi.h"
#define WIDL_using_Windows_Storage
#define WIDL_using_Windows_Storage_Provider
#define WIDL_using_Windows_Storage_Streams
#include "windows.storage.h"
#include "windows.storage.streams.h"

HRESULT async_operation_file_update_create(IUnknown *invoker, async_operation_callback callback,
        IAsyncOperation_FileUpdateStatus **out);

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
    HRESULT (WINAPI *GetFileFromApplicationUriAsync)( IOrionStorageFileStatics *iface, IUriRuntimeClass *uri, IAsyncOperation_StorageFile **operation );
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

NOTIMPL_STUB( OpenTransactedWriteAsync )
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

static HRESULT open_file(IUnknown *invoker, PROPVARIANT *result, BOOL writable)
{
    IStorageFile *file = (IStorageFile *)invoker;
    IRandomAccessStream *stream;
    HSTRING type;
    HRESULT hr;
    if (FAILED(hr = file_get_ContentType(file, &type))) return hr;
    hr = file_stream_create(impl_from_IStorageFile(file)->path, type, writable, &stream);
    WindowsDeleteString(type);
    if (SUCCEEDED(hr))
    {
        result->vt = VT_UNKNOWN;
        result->punkVal = (IUnknown *)stream;
    }
    return hr;
}
static HRESULT open_file_read_async(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{ return called_async ? open_file(invoker, result, FALSE) : STATUS_PENDING; }
static HRESULT open_file_write_async(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{ return called_async ? open_file(invoker, result, TRUE) : STATUS_PENDING; }
static HRESULT WINAPI file_OpenAsync(IStorageFile *iface, FileAccessMode mode, IAsyncOperation_IRandomAccessStream **out)
{
    TRACE("iface %p, mode %u, out %p.\n", iface, mode, out);
    if (!out) return E_POINTER;
    *out = NULL;
    if (mode != FileAccessMode_Read && mode != FileAccessMode_ReadWrite) return E_INVALIDARG;
    return async_operation_inspectable_create(&IID_IAsyncOperation_IRandomAccessStream, (IUnknown *)iface, NULL,
            mode == FileAccessMode_ReadWrite ? open_file_write_async : open_file_read_async,
            (IAsyncOperation_IInspectable **)out);
}

static HRESULT WINAPI file_CopyOverloadDefaultNameAndOptions( IStorageFile *, IStorageFolder *, IAsyncOperation_StorageFile ** );
static HRESULT WINAPI file_CopyOverloadDefaultOptions( IStorageFile *, IStorageFolder *, HSTRING, IAsyncOperation_StorageFile ** );
static HRESULT WINAPI file_CopyOverload( IStorageFile *, IStorageFolder *, HSTRING, NameCollisionOption, IAsyncOperation_StorageFile ** );

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
    file_OpenAsync,
    (void *)stub_OpenTransactedWriteAsync,
    file_CopyOverloadDefaultNameAndOptions,
    file_CopyOverloadDefaultOptions,
    file_CopyOverload,
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

static HRESULT open_read_async(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    PROPVARIANT stream = {0};
    HRESULT hr = open_file_read_async(invoker, param, &stream, called_async);
    if (SUCCEEDED(hr) && hr != STATUS_PENDING)
    {
        result->vt = VT_UNKNOWN;
        hr = IUnknown_QueryInterface(stream.punkVal, &IID_IRandomAccessStreamWithContentType, (void **)&result->punkVal);
        IUnknown_Release(stream.punkVal);
    }
    return hr;
}
static HRESULT WINAPI rasref_OpenReadAsync(IRandomAccessStreamReference *iface, IAsyncOperation_IRandomAccessStreamWithContentType **out)
{
    struct storage_file *impl = impl_from_IRandomAccessStreamReference(iface);
    if (!out) return E_POINTER;
    return async_operation_inspectable_create(&IID_IAsyncOperation_IRandomAccessStreamWithContentType,
            (IUnknown *)&impl->IStorageFile_iface, NULL, open_read_async, (IAsyncOperation_IInspectable **)out);
}

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
    rasref_OpenReadAsync,
};

DEFINE_IINSPECTABLE( isref, IInputStreamReference, struct storage_file, IStorageFile_iface )

static HRESULT open_sequential_async(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    PROPVARIANT stream = {0};
    HRESULT hr = open_file_read_async(invoker, param, &stream, called_async);
    if (SUCCEEDED(hr) && hr != STATUS_PENDING)
    {
        result->vt = VT_UNKNOWN;
        hr = IUnknown_QueryInterface(stream.punkVal, &IID_IInputStream, (void **)&result->punkVal);
        IUnknown_Release(stream.punkVal);
    }
    return hr;
}
static HRESULT WINAPI isref_OpenSequentialReadAsync(IInputStreamReference *iface, IAsyncOperation_IInputStream **out)
{
    struct storage_file *impl = impl_from_IInputStreamReference(iface);
    if (!out) return E_POINTER;
    return async_operation_inspectable_create(&IID_IAsyncOperation_IInputStream,
            (IUnknown *)&impl->IStorageFile_iface, NULL, open_sequential_async, (IAsyncOperation_IInspectable **)out);
}

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
    isref_OpenSequentialReadAsync,
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

HRESULT storage_file_create_object( const WCHAR *path, IUnknown **out )
{
    struct storage_file *file;
    HRESULT hr;

    if (FAILED(hr = storage_file_create( path, &file ))) return hr;
    *out = (IUnknown *)&file->IStorageFile_iface;
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
    NameCollisionOption collision;
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

/* Snapshot the destination before dispatch, without retaining an apartment-bound folder. */
static HRESULT copy_file_async( IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async )
{
    struct storage_file *source = impl_from_IStorageFile( (IStorageFile *)invoker );
    struct path_request *request = impl_from_request( param );
    const WCHAR *extension = path_extension( request->path );
    WCHAR *destination;
    UINT suffix = 2, capacity = wcslen(request->path) + 32;
    DWORD error;
    HRESULT hr;
    IUnknown *file;

    if (!called_async) return STATUS_PENDING;
    if (!(destination = malloc(capacity * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    wcscpy(destination, request->path);
    for (;;)
    {
        if (CopyFileW(source->path, destination, request->collision != NameCollisionOption_ReplaceExisting))
        {
            hr = storage_file_create_object(destination, &file);
            if (SUCCEEDED(hr)) { result->vt = VT_UNKNOWN; result->punkVal = file; }
            break;
        }
        error = GetLastError();
        if (request->collision != NameCollisionOption_GenerateUniqueName ||
            (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) || suffix == ~0u)
        { hr = HRESULT_FROM_WIN32(error); break; }
        swprintf(destination, capacity, L"%.*s (%u)%s", (int)(extension - request->path), request->path,
                 suffix++, extension);
    }
    TRACE("CopyAsync %s -> %s, result %#lx.\n", debugstr_w(source->path), debugstr_w(destination), hr);
    free(destination);
    return hr;
}

static HRESULT WINAPI file_CopyOverload( IStorageFile *iface, IStorageFolder *folder, HSTRING name,
                                         NameCollisionOption option, IAsyncOperation_StorageFile **operation )
{
    struct path_request *request;
    IStorageItem *item;
    HSTRING path;
    const WCHAR *filename, *directory;
    UINT32 length, i;
    SIZE_T capacity;
    HRESULT hr;

    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!folder || (unsigned int)option > NameCollisionOption_FailIfExists) return E_INVALIDARG;
    filename = WindowsGetStringRawBuffer(name, &length);
    if (!length || filename[length - 1] == '.' || filename[length - 1] == ' ') return E_INVALIDARG;
    for (i = 0; i < length; ++i)
        if (filename[i] < 32 || wcschr(L"\\/:*?\"<>|", filename[i])) return E_INVALIDARG;
    if (FAILED(hr = IStorageFolder_QueryInterface(folder, &IID_IStorageItem, (void **)&item))) return hr;
    hr = IStorageItem_get_Path(item, &path);
    IStorageItem_Release(item);
    if (FAILED(hr)) return hr;
    directory = WindowsGetStringRawBuffer(path, NULL);
    capacity = wcslen(directory) + length + 2;
    if (!*directory) hr = E_INVALIDARG;
    else if (capacity > 32768) hr = HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
    else if (!(request = calloc(1, sizeof(*request)))) hr = E_OUTOFMEMORY;
    else
    {
        request->IUnknown_iface.lpVtbl = &path_request_vtbl;
        request->ref = 1;
        request->collision = option;
        if (!(request->path = malloc(capacity * sizeof(WCHAR)))) hr = E_OUTOFMEMORY;
        else
        {
            swprintf(request->path, capacity, L"%s\\%s", directory, filename);
            hr = async_operation_inspectable_create(&IID_IAsyncOperation_StorageFile, (IUnknown *)iface,
                                                    &request->IUnknown_iface, copy_file_async,
                                                    (IAsyncOperation_IInspectable **)operation);
        }
        IUnknown_Release(&request->IUnknown_iface);
    }
    WindowsDeleteString(path);
    return hr;
}

static HRESULT WINAPI file_CopyOverloadDefaultOptions( IStorageFile *iface, IStorageFolder *folder,
                                                       HSTRING name, IAsyncOperation_StorageFile **operation )
{
    return file_CopyOverload(iface, folder, name, NameCollisionOption_FailIfExists, operation);
}

static HRESULT WINAPI file_CopyOverloadDefaultNameAndOptions( IStorageFile *iface, IStorageFolder *folder,
                                                              IAsyncOperation_StorageFile **operation )
{
    const WCHAR *path = impl_from_IStorageFile(iface)->path, *name = wcsrchr(path, '\\');
    const WCHAR *slash = wcsrchr(path, '/');
    HSTRING filename;
    HRESULT hr;
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (slash && (!name || slash > name)) name = slash;
    name = name ? name + 1 : path;
    if (FAILED(hr = WindowsCreateString(name, wcslen(name), &filename))) return hr;
    hr = file_CopyOverloadDefaultOptions(iface, folder, filename, operation);
    WindowsDeleteString(filename);
    return hr;
}

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

/* Decode the URI path exactly once, including UTF-8 escaped characters. */
static int uri_hex( char c )
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static HRESULT decode_uri_path( HSTRING value, WCHAR **out )
{
    const WCHAR *str;
    char *bytes;
    UINT32 length;
    int count, i, j, high, low;
    WCHAR *path;

    *out = NULL;
    str = WindowsGetStringRawBuffer(value, &length);
    if (!length || wcslen(str) != length) return E_INVALIDARG;
    count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str, length, NULL, 0, NULL, NULL);
    if (!count) return E_INVALIDARG;
    if (!(bytes = malloc(count))) return E_OUTOFMEMORY;
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str, length, bytes, count, NULL, NULL);
    for (i = j = 0; i < count; ++i)
    {
        if (bytes[i] == '%')
        {
            if (i + 2 >= count || (high = uri_hex(bytes[i + 1])) < 0 || (low = uri_hex(bytes[i + 2])) < 0)
            { free(bytes); return E_INVALIDARG; }
            bytes[j] = (high << 4) | low;
            i += 2;
        }
        else bytes[j] = bytes[i];
        if (!bytes[j++]) { free(bytes); return E_INVALIDARG; }
    }
    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, j, NULL, 0);
    if (!count) { free(bytes); return E_INVALIDARG; }
    if (!(path = malloc((count + 1) * sizeof(*path)))) { free(bytes); return E_OUTOFMEMORY; }
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, j, path, count);
    path[count] = 0;
    free(bytes);
    *out = path;
    return S_OK;
}

static HRESULT application_data_path( const WCHAR *name, HSTRING *path )
{
    IApplicationDataStatics *statics;
    IApplicationData *data;
    IStorageFolder *folder;
    IStorageItem *item;
    HSTRING cls;
    HRESULT hr;

    if (FAILED(hr = WindowsCreateString(L"Windows.Storage.ApplicationData", 31, &cls))) return hr;
    hr = RoGetActivationFactory(cls, &IID_IApplicationDataStatics, (void **)&statics);
    WindowsDeleteString(cls);
    if (FAILED(hr)) return hr;
    hr = IApplicationDataStatics_get_Current(statics, &data);
    IApplicationDataStatics_Release(statics);
    if (FAILED(hr)) return hr;
    if (!wcscmp(name, L"local")) hr = IApplicationData_get_LocalFolder(data, &folder);
    else if (!wcscmp(name, L"roaming")) hr = IApplicationData_get_RoamingFolder(data, &folder);
    else if (!wcscmp(name, L"temp")) hr = IApplicationData_get_TemporaryFolder(data, &folder);
    else hr = E_INVALIDARG;
    IApplicationData_Release(data);
    if (FAILED(hr)) return hr;
    hr = IStorageFolder_QueryInterface(folder, &IID_IStorageItem, (void **)&item);
    IStorageFolder_Release(folder);
    if (FAILED(hr)) return hr;
    hr = IStorageItem_get_Path(item, path);
    IStorageItem_Release(item);
    return hr;
}

static HRESULT WINAPI statics_GetFileFromApplicationUriAsync( IOrionStorageFileStatics *iface,
        IUriRuntimeClass *uri, IAsyncOperation_StorageFile **operation )
{
    HSTRING scheme = NULL, host = NULL, path = NULL, root_string = NULL, full_path = NULL;
    WCHAR *decoded = NULL, *root = NULL, *full = NULL, *relative, *component, *end;
    PACKAGE_ID *id = NULL;
    UINT32 length = 0;
    LONG ret;
    HRESULT hr;

    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!uri) return E_INVALIDARG;
    if (FAILED(hr = IUriRuntimeClass_get_SchemeName(uri, &scheme)) ||
        FAILED(hr = IUriRuntimeClass_get_Host(uri, &host)) ||
        FAILED(hr = IUriRuntimeClass_get_Path(uri, &path))) goto done;
    TRACE("scheme %s, host %s, path %s.\n", debugstr_hstring(scheme), debugstr_hstring(host), debugstr_hstring(path));
    if (FAILED(hr = decode_uri_path(path, &decoded))) goto done;
    hr = E_INVALIDARG;
    if (decoded[0] != '/') goto done;
    relative = decoded + 1;
    if (!wcsicmp(WindowsGetStringRawBuffer(scheme, NULL), L"ms-appx"))
    {
        if (WindowsGetStringLen(host))
        {
            ret = GetCurrentPackageId(&length, NULL);
            if (ret != ERROR_INSUFFICIENT_BUFFER) { hr = HRESULT_FROM_WIN32(ret); goto done; }
            if (!(id = malloc(length))) { hr = E_OUTOFMEMORY; goto done; }
            if ((ret = GetCurrentPackageId(&length, (BYTE *)id))) { hr = HRESULT_FROM_WIN32(ret); goto done; }
            if (wcsicmp(id->name, WindowsGetStringRawBuffer(host, NULL))) { hr = E_INVALIDARG; goto done; }
        }
        length = 0;
        ret = GetCurrentPackagePath(&length, NULL);
        if (ret != ERROR_INSUFFICIENT_BUFFER) { hr = HRESULT_FROM_WIN32(ret); goto done; }
        if (!(root = malloc(length * sizeof(*root)))) { hr = E_OUTOFMEMORY; goto done; }
        if ((ret = GetCurrentPackagePath(&length, root))) { hr = HRESULT_FROM_WIN32(ret); goto done; }
    }
    else if (!wcsicmp(WindowsGetStringRawBuffer(scheme, NULL), L"ms-appdata"))
    {
        if (WindowsGetStringLen(host) || !(end = wcschr(relative, '/'))) goto done;
        *end = 0;
        if (FAILED(hr = application_data_path(relative, &root_string))) goto done;
        relative = end + 1;
        if (!(root = wcsdup(WindowsGetStringRawBuffer(root_string, NULL)))) { hr = E_OUTOFMEMORY; goto done; }
    }
    else goto done;

    /* Do not let escaped separators, dot components, or DOS device/stream syntax escape the root. */
    hr = E_INVALIDARG;
    for (component = relative; *component; component = end + 1)
    {
        end = wcschr(component, '/');
        if (!end) end = component + wcslen(component);
        if (component == end || end[-1] == '.' || end[-1] == ' ') goto done;
        for (WCHAR *p = component; p < end; ++p)
            if (*p < 32 || wcschr(L"\\:*?\"<>|", *p)) goto done;
        if (!*end) break;
    }
    if (!*relative) goto done;
    length = wcslen(root) + wcslen(relative) + 2;
    if (!(full = malloc(length * sizeof(*full)))) { hr = E_OUTOFMEMORY; goto done; }
    swprintf(full, length, L"%s\\%s", root, relative);
    for (end = full; *end; ++end) if (*end == '/') *end = '\\';
    if (SUCCEEDED(hr = WindowsCreateString(full, wcslen(full), &full_path)))
        hr = statics_GetFileFromPathAsync(iface, full_path, operation);
done:
    free(id);
    free(root);
    free(full);
    free(decoded);
    WindowsDeleteString(scheme);
    WindowsDeleteString(host);
    WindowsDeleteString(path);
    WindowsDeleteString(root_string);
    WindowsDeleteString(full_path);
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
    statics_GetFileFromApplicationUriAsync,
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

/* Files selected through the desktop portal are local files, with no Windows
 * cached-file provider. Updates therefore complete when local I/O completes. */
struct cached_factory
{
    IActivationFactory IActivationFactory_iface;
    ICachedFileManagerStatics ICachedFileManagerStatics_iface;
    LONG ref;
};
static HRESULT WINAPI cached_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct cached_factory *impl = CONTAINING_RECORD(iface, struct cached_factory, IActivationFactory_iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IActivationFactory)) *out = iface;
    else if (IsEqualGUID(iid, &IID_ICachedFileManagerStatics)) *out = &impl->ICachedFileManagerStatics_iface;
    else return E_NOINTERFACE;
    IActivationFactory_AddRef(iface);
    return S_OK;
}
static ULONG WINAPI cached_AddRef(IActivationFactory *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct cached_factory, IActivationFactory_iface)->ref); }
static ULONG WINAPI cached_Release(IActivationFactory *iface)
{ return InterlockedDecrement(&CONTAINING_RECORD(iface, struct cached_factory, IActivationFactory_iface)->ref); }
static HRESULT WINAPI cached_GetIids(IActivationFactory *iface, ULONG *count, IID **ids)
{
    if (!count || !ids) return E_POINTER;
    *count = 0;
    if (!(*ids = CoTaskMemAlloc(sizeof(**ids)))) return E_OUTOFMEMORY;
    **ids = IID_ICachedFileManagerStatics;
    *count = 1;
    return S_OK;
}
static HRESULT WINAPI cached_GetRuntimeClassName(IActivationFactory *iface, HSTRING *out)
{ return return_class_name(L"Windows.Storage.CachedFileManager", out); }
static HRESULT WINAPI cached_GetTrustLevel(IActivationFactory *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI cached_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IActivationFactoryVtbl cached_vtbl =
{
    cached_QueryInterface, cached_AddRef, cached_Release, cached_GetIids,
    cached_GetRuntimeClassName, cached_GetTrustLevel, cached_ActivateInstance
};
DEFINE_IINSPECTABLE(cached_statics, ICachedFileManagerStatics, struct cached_factory, IActivationFactory_iface)
static HRESULT validate_local_file(IStorageFile *file)
{
    DWORD attributes;
    if (!file) return E_INVALIDARG;
    /* Do not claim to synchronize a file supplied by an unsupported provider. */
    if (file->lpVtbl != &storage_file_vtbl) return E_NOTIMPL;
    attributes = GetFileAttributesW(impl_from_IStorageFile(file)->path);
    if (attributes == INVALID_FILE_ATTRIBUTES) return HRESULT_FROM_WIN32(GetLastError());
    return attributes & FILE_ATTRIBUTE_DIRECTORY ? E_INVALIDARG : S_OK;
}
static HRESULT WINAPI cached_statics_DeferUpdates(ICachedFileManagerStatics *iface, IStorageFile *file)
{
    TRACE("file %p\n", file);
    return validate_local_file(file);
}
static HRESULT complete_updates_async(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    HRESULT hr;
    if (!called_async) return STATUS_PENDING;
    if (FAILED(hr = validate_local_file((IStorageFile *)invoker))) return hr;
    result->vt = VT_I4;
    result->lVal = FileUpdateStatus_Complete;
    return S_OK;
}
static HRESULT WINAPI cached_statics_CompleteUpdatesAsync(ICachedFileManagerStatics *iface, IStorageFile *file,
        IAsyncOperation_FileUpdateStatus **out)
{
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (FAILED(hr = validate_local_file(file))) return hr;
    return async_operation_file_update_create((IUnknown *)file, complete_updates_async, out);
}
static const ICachedFileManagerStaticsVtbl cached_statics_vtbl =
{
    cached_statics_QueryInterface, cached_statics_AddRef, cached_statics_Release, cached_statics_GetIids,
    cached_statics_GetRuntimeClassName, cached_statics_GetTrustLevel,
    cached_statics_DeferUpdates, cached_statics_CompleteUpdatesAsync
};
static struct cached_factory cached_factory = {{&cached_vtbl}, {&cached_statics_vtbl}, 1};
IActivationFactory *cached_file_manager_factory = &cached_factory.IActivationFactory_iface;
