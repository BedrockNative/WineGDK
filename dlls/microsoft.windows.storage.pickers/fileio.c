/* Local-file Windows.Storage.FileIO operations.
 * Copyright 2026 WineGDK contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "private.h"
#define WIDL_using_Windows_Storage
#include "windows.storage.h"
#include "robuffer.h"
#include "roapi.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(pickers);

struct file_io_factory
{
    IActivationFactory IActivationFactory_iface;
    IFileIOStatics IFileIOStatics_iface;
    LONG ref;
};
static struct file_io_factory *impl_from_IActivationFactory(IActivationFactory *iface)
{ return CONTAINING_RECORD(iface, struct file_io_factory, IActivationFactory_iface); }
static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct file_io_factory *impl = impl_from_IActivationFactory(iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid,&IID_IUnknown) || IsEqualGUID(iid,&IID_IInspectable) ||
        IsEqualGUID(iid,&IID_IAgileObject) || IsEqualGUID(iid,&IID_IActivationFactory)) *out = iface;
    else if (IsEqualGUID(iid,&IID_IFileIOStatics)) *out = &impl->IFileIOStatics_iface;
    else return E_NOINTERFACE;
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}
static ULONG WINAPI factory_AddRef(IActivationFactory *iface)
{ return InterlockedIncrement(&impl_from_IActivationFactory(iface)->ref); }
static ULONG WINAPI factory_Release(IActivationFactory *iface)
{ return InterlockedDecrement(&impl_from_IActivationFactory(iface)->ref); }
static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **out)
{
    if (!count || !out) return E_POINTER;
    *count = 0;
    if (!(*out = CoTaskMemAlloc(sizeof(**out)))) return E_OUTOFMEMORY;
    **out = IID_IFileIOStatics; *count = 1; return S_OK;
}
static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *out)
{ return WindowsCreateString(L"Windows.Storage.FileIO",22,out); }
static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IActivationFactoryVtbl factory_vtbl = {factory_QueryInterface,factory_AddRef,factory_Release,
    factory_GetIids,factory_GetRuntimeClassName,factory_GetTrustLevel,factory_ActivateInstance};

struct io_request
{
    IUnknown IUnknown_iface;
    LONG ref;
    HSTRING path;
    BYTE *bytes;
    UINT32 length;
};
static struct io_request *request_impl(IUnknown *iface)
{ return CONTAINING_RECORD(iface,struct io_request,IUnknown_iface); }
static HRESULT WINAPI request_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown)) return E_NOINTERFACE;
    *out = iface; IUnknown_AddRef(iface); return S_OK;
}
static ULONG WINAPI request_AddRef(IUnknown *iface)
{ return InterlockedIncrement(&request_impl(iface)->ref); }
static ULONG WINAPI request_Release(IUnknown *iface)
{
    struct io_request *impl = request_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { WindowsDeleteString(impl->path); free(impl->bytes); free(impl); }
    return ref;
}
static const IUnknownVtbl request_vtbl = {request_QueryInterface,request_AddRef,request_Release};
static HRESULT request_create(IStorageFile *file, struct io_request **out)
{
    struct io_request *request;
    IStorageItem *item;
    HRESULT hr;
    *out = NULL;
    if (!file) return E_INVALIDARG;
    if (!(request = calloc(1,sizeof(*request)))) return E_OUTOFMEMORY;
    request->IUnknown_iface.lpVtbl = &request_vtbl;
    request->ref = 1;
    hr = IStorageFile_QueryInterface(file,&IID_IStorageItem,(void **)&item);
    if (SUCCEEDED(hr))
    {
        hr = IStorageItem_get_Path(item,&request->path);
        IStorageItem_Release(item);
        if (SUCCEEDED(hr) && !WindowsGetStringLen(request->path)) hr = E_NOTIMPL;
    }
    if (FAILED(hr)) IUnknown_Release(&request->IUnknown_iface);
    else *out = request;
    return hr;
}
static HRESULT write_bytes(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    struct io_request *request = request_impl(param);
    HANDLE file;
    DWORD written;
    HRESULT hr = S_OK;
    if (!called_async) return STATUS_PENDING;
    file = CreateFileW(WindowsGetStringRawBuffer(request->path,NULL),GENERIC_WRITE,
        FILE_SHARE_READ,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
    if (!WriteFile(file,request->bytes,request->length,&written,NULL)) hr = HRESULT_FROM_WIN32(GetLastError());
    else if (written != request->length) hr = HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
    CloseHandle(file);
    TRACE("FileIO wrote %u bytes to %s, hr %#lx.\n",request->length,debugstr_hstring(request->path),hr);
    return hr;
}
static HRESULT read_buffer(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    struct io_request *request = request_impl(param);
    IBufferFactory *factory = NULL;
    IBuffer *buffer = NULL;
    IBufferByteAccess *access = NULL;
    HANDLE file;
    LARGE_INTEGER size;
    HSTRING name;
    BYTE *bytes;
    DWORD count;
    HRESULT hr;
    if (!called_async) return STATUS_PENDING;
    file = CreateFileW(WindowsGetStringRawBuffer(request->path,NULL),GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
    if (!GetFileSizeEx(file,&size)) { hr = HRESULT_FROM_WIN32(GetLastError()); goto done; }
    if (size.QuadPart > UINT_MAX) { hr = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE); goto done; }
    if (FAILED(hr = WindowsCreateString(L"Windows.Storage.Streams.Buffer",30,&name))) goto done;
    hr = RoGetActivationFactory(name,&IID_IBufferFactory,(void **)&factory);
    WindowsDeleteString(name);
    if (FAILED(hr)) goto done;
    if (FAILED(hr = IBufferFactory_Create(factory,size.QuadPart,&buffer))) goto done;
    if (FAILED(hr = IBuffer_QueryInterface(buffer,&IID_IBufferByteAccess,(void **)&access))) goto done;
    if (FAILED(hr = IBufferByteAccess_Buffer(access,&bytes))) goto done;
    if (!ReadFile(file,bytes,size.QuadPart,&count,NULL)) { hr = HRESULT_FROM_WIN32(GetLastError()); goto done; }
    if (SUCCEEDED(hr = IBuffer_put_Length(buffer,count)))
    { result->vt = VT_UNKNOWN; result->punkVal = (IUnknown *)buffer; IBuffer_AddRef(buffer); }
    TRACE("FileIO read %lu bytes from %s, hr %#lx.\n",count,debugstr_hstring(request->path),hr);
done:
    if (access) IBufferByteAccess_Release(access);
    if (buffer) IBuffer_Release(buffer);
    if (factory) IBufferFactory_Release(factory);
    CloseHandle(file);
    return hr;
}
DEFINE_IINSPECTABLE(statics,IFileIOStatics,struct file_io_factory,IActivationFactory_iface)
static HRESULT WINAPI statics_WriteBytesAsync(IFileIOStatics *iface, IStorageFile *file, UINT32 length,
        BYTE *bytes, IAsyncAction **out)
{
    struct io_request *request;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (length && !bytes) return E_INVALIDARG;
    if (FAILED(hr = request_create(file,&request))) return hr;
    if (length && !(request->bytes = malloc(length))) hr = E_OUTOFMEMORY;
    else
    {
        if (length) memcpy(request->bytes,bytes,length);
        request->length = length;
        hr = async_action_create((IUnknown *)file,&request->IUnknown_iface,write_bytes,out);
    }
    IUnknown_Release(&request->IUnknown_iface);
    return hr;
}
static HRESULT WINAPI statics_WriteBufferAsync(IFileIOStatics *iface, IStorageFile *file, IBuffer *buffer, IAsyncAction **out)
{
    IBufferByteAccess *access;
    BYTE *bytes;
    UINT32 length;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!buffer) return E_INVALIDARG;
    if (FAILED(hr = IBuffer_get_Length(buffer,&length))) return hr;
    if (FAILED(hr = IBuffer_QueryInterface(buffer,&IID_IBufferByteAccess,(void **)&access))) return hr;
    if (SUCCEEDED(hr = IBufferByteAccess_Buffer(access,&bytes))) hr = statics_WriteBytesAsync(iface,file,length,bytes,out);
    IBufferByteAccess_Release(access);
    return hr;
}
static HRESULT WINAPI statics_ReadBufferAsync(IFileIOStatics *iface, IStorageFile *file, IAsyncOperation_IBuffer **out)
{
    struct io_request *request;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (FAILED(hr = request_create(file,&request))) return hr;
    hr = async_operation_inspectable_create(&IID_IAsyncOperation_IBuffer,(IUnknown *)file,&request->IUnknown_iface,
        read_buffer,(IAsyncOperation_IInspectable **)out);
    IUnknown_Release(&request->IUnknown_iface);
    return hr;
}
/* Text and line conversions are separate APIs from the binary import/export path. */
#define STUB(name, ...) static HRESULT WINAPI statics_##name(IFileIOStatics *iface, __VA_ARGS__) \
    { if (!out) return E_POINTER; *out = NULL; FIXME(#name " not implemented.\n"); return E_NOTIMPL; }
STUB(ReadTextAsync,IStorageFile *file,IAsyncOperation_HSTRING **out)
STUB(ReadTextWithEncodingAsync,IStorageFile *file,UnicodeEncoding encoding,IAsyncOperation_HSTRING **out)
STUB(WriteTextAsync,IStorageFile *file,HSTRING contents,IAsyncAction **out)
STUB(WriteTextWithEncodingAsync,IStorageFile *file,HSTRING contents,UnicodeEncoding encoding,IAsyncAction **out)
STUB(AppendTextAsync,IStorageFile *file,HSTRING contents,IAsyncAction **out)
STUB(AppendTextWithEncodingAsync,IStorageFile *file,HSTRING contents,UnicodeEncoding encoding,IAsyncAction **out)
STUB(ReadLinesAsync,IStorageFile *file,IAsyncOperation_IVector_HSTRING **out)
STUB(ReadLinesWithEncodingAsync,IStorageFile *file,UnicodeEncoding encoding,IAsyncOperation_IVector_HSTRING **out)
STUB(WriteLinesAsync,IStorageFile *file,IIterable_HSTRING *lines,IAsyncAction **out)
STUB(WriteLinesWithEncodingAsync,IStorageFile *file,IIterable_HSTRING *lines,UnicodeEncoding encoding,IAsyncAction **out)
STUB(AppendLinesAsync,IStorageFile *file,IIterable_HSTRING *lines,IAsyncAction **out)
STUB(AppendLinesWithEncodingAsync,IStorageFile *file,IIterable_HSTRING *lines,UnicodeEncoding encoding,IAsyncAction **out)
#undef STUB
static const IFileIOStaticsVtbl statics_vtbl =
{
    statics_QueryInterface,statics_AddRef,statics_Release,statics_GetIids,statics_GetRuntimeClassName,statics_GetTrustLevel,
    statics_ReadTextAsync,statics_ReadTextWithEncodingAsync,statics_WriteTextAsync,statics_WriteTextWithEncodingAsync,
    statics_AppendTextAsync,statics_AppendTextWithEncodingAsync,statics_ReadLinesAsync,statics_ReadLinesWithEncodingAsync,
    statics_WriteLinesAsync,statics_WriteLinesWithEncodingAsync,statics_AppendLinesAsync,statics_AppendLinesWithEncodingAsync,
    statics_ReadBufferAsync,statics_WriteBufferAsync,statics_WriteBytesAsync
};
static struct file_io_factory factory = {{&factory_vtbl},{&statics_vtbl},1};
IActivationFactory *file_io_factory = &factory.IActivationFactory_iface;
