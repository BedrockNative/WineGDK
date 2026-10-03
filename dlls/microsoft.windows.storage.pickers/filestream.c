/* File-backed WinRT streams.
 * Copyright 2026 WineGDK contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"
#include "initguid.h"
#include "robuffer.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(pickers);

struct file_stream
{
    IRandomAccessStream IRandomAccessStream_iface;
    IInputStream IInputStream_iface;
    IOutputStream IOutputStream_iface;
    IClosable IClosable_iface;
    IRandomAccessStreamWithContentType IRandomAccessStreamWithContentType_iface;
    IContentTypeProvider IContentTypeProvider_iface;
    LONG ref;
    HANDLE file;
    BOOL writable;
    HSTRING content_type;
    CRITICAL_SECTION cs;
};

static struct file_stream *stream_impl(IRandomAccessStream *iface)
{ return CONTAINING_RECORD(iface, struct file_stream, IRandomAccessStream_iface); }

static HRESULT WINAPI stream_QueryInterface(IRandomAccessStream *iface, REFIID iid, void **out)
{
    struct file_stream *impl = stream_impl(iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IRandomAccessStream)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IInputStream)) *out = &impl->IInputStream_iface;
    else if (IsEqualGUID(iid, &IID_IOutputStream)) *out = &impl->IOutputStream_iface;
    else if (IsEqualGUID(iid, &IID_IClosable)) *out = &impl->IClosable_iface;
    else if (IsEqualGUID(iid, &IID_IRandomAccessStreamWithContentType)) *out = &impl->IRandomAccessStreamWithContentType_iface;
    else if (IsEqualGUID(iid, &IID_IContentTypeProvider)) *out = &impl->IContentTypeProvider_iface;
    else { WARN("unsupported stream interface %s\n", debugstr_guid(iid)); return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}
static ULONG WINAPI stream_AddRef(IRandomAccessStream *iface)
{ return InterlockedIncrement(&stream_impl(iface)->ref); }
static ULONG WINAPI stream_Release(IRandomAccessStream *iface)
{
    struct file_stream *impl = stream_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref)
    {
        if (impl->file != INVALID_HANDLE_VALUE) CloseHandle(impl->file);
        WindowsDeleteString(impl->content_type);
        DeleteCriticalSection(&impl->cs);
        free(impl);
    }
    return ref;
}
static HRESULT WINAPI stream_GetIids(IRandomAccessStream *iface, ULONG *count, IID **ids)
{
    static const IID * const interfaces[] = {&IID_IRandomAccessStream, &IID_IInputStream, &IID_IOutputStream,
        &IID_IClosable, &IID_IRandomAccessStreamWithContentType, &IID_IContentTypeProvider};
    unsigned i;
    if (!count || !ids) return E_POINTER;
    *count = 0;
    if (!(*ids = CoTaskMemAlloc(sizeof(IID) * ARRAY_SIZE(interfaces)))) return E_OUTOFMEMORY;
    for (i = 0; i < ARRAY_SIZE(interfaces); ++i) (*ids)[i] = *interfaces[i];
    *count = ARRAY_SIZE(interfaces);
    return S_OK;
}
static HRESULT WINAPI stream_GetRuntimeClassName(IRandomAccessStream *iface, HSTRING *out)
{
    static const WCHAR name[] = L"Windows.Storage.Streams.FileRandomAccessStream";
    return WindowsCreateString(name, ARRAY_SIZE(name) - 1, out);
}
static HRESULT WINAPI stream_GetTrustLevel(IRandomAccessStream *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }

static HRESULT WINAPI stream_get_Size(IRandomAccessStream *iface, UINT64 *out)
{
    struct file_stream *impl = stream_impl(iface);
    LARGE_INTEGER size;
    HRESULT hr = S_OK;
    if (!out) return E_POINTER;
    *out = 0;
    EnterCriticalSection(&impl->cs);
    if (impl->file == INVALID_HANDLE_VALUE) hr = RO_E_CLOSED;
    else if (!GetFileSizeEx(impl->file, &size)) hr = HRESULT_FROM_WIN32(GetLastError());
    else *out = size.QuadPart;
    LeaveCriticalSection(&impl->cs);
    return hr;
}
static HRESULT WINAPI stream_put_Size(IRandomAccessStream *iface, UINT64 value)
{
    struct file_stream *impl = stream_impl(iface);
    FILE_END_OF_FILE_INFO info;
    HRESULT hr = S_OK;
    if (value > MAXLONGLONG) return E_INVALIDARG;
    info.EndOfFile.QuadPart = value;
    EnterCriticalSection(&impl->cs);
    if (impl->file == INVALID_HANDLE_VALUE) hr = RO_E_CLOSED;
    else if (!impl->writable) hr = E_ACCESSDENIED;
    else if (!SetFileInformationByHandle(impl->file, FileEndOfFileInfo, &info, sizeof(info)))
        hr = HRESULT_FROM_WIN32(GetLastError());
    LeaveCriticalSection(&impl->cs);
    return hr;
}
static HRESULT stream_from_handle(HANDLE file, HSTRING type, BOOL writable, IRandomAccessStream **out);
static HRESULT WINAPI stream_CloneStream(IRandomAccessStream *iface, IRandomAccessStream **out)
{
    struct file_stream *impl = stream_impl(iface);
    HANDLE file = INVALID_HANDLE_VALUE;
    HRESULT hr = S_OK;
    if (!out) return E_POINTER;
    *out = NULL;
    EnterCriticalSection(&impl->cs);
    if (impl->file == INVALID_HANDLE_VALUE) hr = RO_E_CLOSED;
    else if ((file = ReOpenFile(impl->file, GENERIC_READ | (impl->writable ? GENERIC_WRITE : 0),
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0)) == INVALID_HANDLE_VALUE)
        hr = HRESULT_FROM_WIN32(GetLastError());
    LeaveCriticalSection(&impl->cs);
    if (SUCCEEDED(hr)) hr = stream_from_handle(file, impl->content_type, impl->writable, out);
    return hr;
}
static HRESULT WINAPI stream_Seek(IRandomAccessStream *iface, UINT64 position)
{
    struct file_stream *impl = stream_impl(iface);
    LARGE_INTEGER offset;
    HRESULT hr = S_OK;
    if (position > MAXLONGLONG) return E_INVALIDARG;
    offset.QuadPart = position;
    EnterCriticalSection(&impl->cs);
    if (impl->file == INVALID_HANDLE_VALUE) hr = RO_E_CLOSED;
    else if (!SetFilePointerEx(impl->file, offset, NULL, FILE_BEGIN)) hr = HRESULT_FROM_WIN32(GetLastError());
    LeaveCriticalSection(&impl->cs);
    return hr;
}
static HRESULT WINAPI stream_get_Position(IRandomAccessStream *iface, UINT64 *out)
{
    struct file_stream *impl = stream_impl(iface);
    LARGE_INTEGER offset = {{0}}, pos;
    HRESULT hr = S_OK;
    if (!out) return E_POINTER;
    *out = 0;
    EnterCriticalSection(&impl->cs);
    if (impl->file == INVALID_HANDLE_VALUE) hr = RO_E_CLOSED;
    else if (!SetFilePointerEx(impl->file, offset, &pos, FILE_CURRENT)) hr = HRESULT_FROM_WIN32(GetLastError());
    else *out = pos.QuadPart;
    LeaveCriticalSection(&impl->cs);
    return hr;
}
static HRESULT WINAPI stream_GetInputStreamAt(IRandomAccessStream *iface, UINT64 position, IInputStream **out)
{
    IRandomAccessStream *clone;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (FAILED(hr = stream_CloneStream(iface, &clone))) return hr;
    if (SUCCEEDED(hr = IRandomAccessStream_Seek(clone, position)))
        hr = IRandomAccessStream_QueryInterface(clone, &IID_IInputStream, (void **)out);
    IRandomAccessStream_Release(clone);
    return hr;
}
static HRESULT WINAPI stream_GetOutputStreamAt(IRandomAccessStream *iface, UINT64 position, IOutputStream **out)
{
    struct file_stream *impl = stream_impl(iface);
    IRandomAccessStream *clone;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!impl->writable) return E_ACCESSDENIED;
    if (FAILED(hr = stream_CloneStream(iface, &clone))) return hr;
    if (SUCCEEDED(hr = IRandomAccessStream_Seek(clone, position)))
        hr = IRandomAccessStream_QueryInterface(clone, &IID_IOutputStream, (void **)out);
    IRandomAccessStream_Release(clone);
    return hr;
}
static HRESULT WINAPI stream_get_CanRead(IRandomAccessStream *iface, boolean *out)
{
    struct file_stream *impl = stream_impl(iface);
    if (!out) return E_POINTER;
    EnterCriticalSection(&impl->cs);
    *out = impl->file != INVALID_HANDLE_VALUE;
    LeaveCriticalSection(&impl->cs);
    return S_OK;
}
static HRESULT WINAPI stream_get_CanWrite(IRandomAccessStream *iface, boolean *out)
{
    struct file_stream *impl = stream_impl(iface);
    if (!out) return E_POINTER;
    EnterCriticalSection(&impl->cs);
    *out = impl->writable && impl->file != INVALID_HANDLE_VALUE;
    LeaveCriticalSection(&impl->cs);
    return S_OK;
}

static const IRandomAccessStreamVtbl stream_vtbl =
{
    stream_QueryInterface, stream_AddRef, stream_Release, stream_GetIids, stream_GetRuntimeClassName, stream_GetTrustLevel,
    stream_get_Size, stream_put_Size, stream_GetInputStreamAt, stream_GetOutputStreamAt, stream_get_Position,
    stream_Seek, stream_CloneStream, stream_get_CanRead, stream_get_CanWrite,
};

DEFINE_IINSPECTABLE(closable, IClosable, struct file_stream, IRandomAccessStream_iface)
static HRESULT WINAPI closable_Close(IClosable *iface)
{
    struct file_stream *impl = impl_from_IClosable(iface);
    EnterCriticalSection(&impl->cs);
    if (impl->file != INVALID_HANDLE_VALUE) CloseHandle(impl->file);
    impl->file = INVALID_HANDLE_VALUE;
    LeaveCriticalSection(&impl->cs);
    return S_OK;
}
static const IClosableVtbl closable_vtbl =
{
    closable_QueryInterface, closable_AddRef, closable_Release, closable_GetIids,
    closable_GetRuntimeClassName, closable_GetTrustLevel, closable_Close,
};

/* The request retains the caller's buffer until the worker and results have released it. */
struct read_request
{
    IUnknown IUnknown_iface;
    LONG ref;
    IBuffer *buffer;
    UINT32 count;
};
static HRESULT WINAPI request_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    *out = iface;
    IUnknown_AddRef(iface);
    return S_OK;
}
static ULONG WINAPI request_AddRef(IUnknown *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct read_request, IUnknown_iface)->ref); }
static ULONG WINAPI request_Release(IUnknown *iface)
{
    struct read_request *impl = CONTAINING_RECORD(iface, struct read_request, IUnknown_iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { IBuffer_Release(impl->buffer); free(impl); }
    return ref;
}
static const IUnknownVtbl request_vtbl = {request_QueryInterface, request_AddRef, request_Release};
static HRESULT read_async(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    struct file_stream *impl = stream_impl((IRandomAccessStream *)invoker);
    struct read_request *request = CONTAINING_RECORD(param, struct read_request, IUnknown_iface);
    IBufferByteAccess *access;
    BYTE *bytes;
    DWORD read = 0;
    HRESULT hr;
    if (!called_async) return STATUS_PENDING;
    if (FAILED(hr = IBuffer_QueryInterface(request->buffer, &IID_IBufferByteAccess, (void **)&access))) return hr;
    hr = IBufferByteAccess_Buffer(access, &bytes);
    if (SUCCEEDED(hr))
    {
        EnterCriticalSection(&impl->cs);
        if (impl->file == INVALID_HANDLE_VALUE) hr = RO_E_CLOSED;
        else if (!ReadFile(impl->file, bytes, request->count, &read, NULL)) hr = HRESULT_FROM_WIN32(GetLastError());
        LeaveCriticalSection(&impl->cs);
    }
    IBufferByteAccess_Release(access);
    if (SUCCEEDED(hr)) hr = IBuffer_put_Length(request->buffer, read);
    if (SUCCEEDED(hr))
    {
        result->vt = VT_UNKNOWN;
        result->punkVal = (IUnknown *)request->buffer;
        IBuffer_AddRef(request->buffer);
    }
    TRACE("read %lu/%u bytes, hr %#lx\n", read, request->count, hr);
    return hr;
}
DEFINE_IINSPECTABLE(input, IInputStream, struct file_stream, IRandomAccessStream_iface)
static HRESULT WINAPI input_ReadAsync(IInputStream *iface, IBuffer *buffer, UINT32 count, InputStreamOptions options,
        IAsyncOperationWithProgress_IBuffer_UINT32 **out)
{
    struct file_stream *impl = impl_from_IInputStream(iface);
    struct read_request *request;
    UINT32 capacity;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!buffer || options & ~(InputStreamOptions_Partial | InputStreamOptions_ReadAhead)) return E_INVALIDARG;
    if (FAILED(hr = IBuffer_get_Capacity(buffer, &capacity))) return hr;
    if (count > capacity) return E_INVALIDARG;
    if (!(request = calloc(1, sizeof(*request)))) return E_OUTOFMEMORY;
    request->IUnknown_iface.lpVtbl = &request_vtbl;
    request->ref = 1;
    request->count = count;
    request->buffer = buffer;
    IBuffer_AddRef(buffer);
    hr = async_operation_buffer_read_create((IUnknown *)&impl->IRandomAccessStream_iface,
            &request->IUnknown_iface, read_async, out);
    IUnknown_Release(&request->IUnknown_iface);
    return hr;
}
static const IInputStreamVtbl input_vtbl =
{
    input_QueryInterface, input_AddRef, input_Release, input_GetIids, input_GetRuntimeClassName, input_GetTrustLevel,
    input_ReadAsync,
};
DEFINE_IINSPECTABLE(output, IOutputStream, struct file_stream, IRandomAccessStream_iface)
static HRESULT write_async(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    struct file_stream *impl = stream_impl((IRandomAccessStream *)invoker);
    struct read_request *request = CONTAINING_RECORD(param, struct read_request, IUnknown_iface);
    IBufferByteAccess *access;
    BYTE *bytes;
    DWORD written = 0;
    HRESULT hr;
    if (!called_async) return STATUS_PENDING;
    if (FAILED(hr = IBuffer_QueryInterface(request->buffer, &IID_IBufferByteAccess, (void **)&access))) return hr;
    hr = IBufferByteAccess_Buffer(access, &bytes);
    if (SUCCEEDED(hr))
    {
        EnterCriticalSection(&impl->cs);
        if (impl->file == INVALID_HANDLE_VALUE) hr = RO_E_CLOSED;
        else if (!WriteFile(impl->file, bytes, request->count, &written, NULL)) hr = HRESULT_FROM_WIN32(GetLastError());
        LeaveCriticalSection(&impl->cs);
    }
    IBufferByteAccess_Release(access);
    if (SUCCEEDED(hr)) { result->vt = VT_UI4; result->ulVal = written; }
    TRACE("wrote %lu/%u bytes, hr %#lx\n", written, request->count, hr);
    return hr;
}
static HRESULT WINAPI output_WriteAsync(IOutputStream *iface, IBuffer *buffer, IAsyncOperationWithProgress_UINT32_UINT32 **out)
{
    struct file_stream *impl = impl_from_IOutputStream(iface);
    struct read_request *request;
    UINT32 length;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!impl->writable) return E_ACCESSDENIED;
    if (!buffer) return E_INVALIDARG;
    if (FAILED(hr = IBuffer_get_Length(buffer, &length))) return hr;
    if (!(request = calloc(1, sizeof(*request)))) return E_OUTOFMEMORY;
    request->IUnknown_iface.lpVtbl = &request_vtbl;
    request->ref = 1;
    request->count = length;
    request->buffer = buffer;
    IBuffer_AddRef(buffer);
    hr = async_operation_buffer_write_create((IUnknown *)&impl->IRandomAccessStream_iface,
            &request->IUnknown_iface, write_async, out);
    IUnknown_Release(&request->IUnknown_iface);
    return hr;
}
static HRESULT flush_async(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    struct file_stream *impl = stream_impl((IRandomAccessStream *)invoker);
    HRESULT hr = S_OK;
    if (!called_async) return STATUS_PENDING;
    EnterCriticalSection(&impl->cs);
    if (impl->file == INVALID_HANDLE_VALUE) hr = RO_E_CLOSED;
    else if (!FlushFileBuffers(impl->file)) hr = HRESULT_FROM_WIN32(GetLastError());
    LeaveCriticalSection(&impl->cs);
    if (SUCCEEDED(hr)) { result->vt = VT_BOOL; result->boolVal = VARIANT_TRUE; }
    return hr;
}
static HRESULT WINAPI output_FlushAsync(IOutputStream *iface, IAsyncOperation_boolean **out)
{
    struct file_stream *impl = impl_from_IOutputStream(iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (!impl->writable) return E_ACCESSDENIED;
    return async_operation_boolean_create((IUnknown *)&impl->IRandomAccessStream_iface, NULL, flush_async, out);
}
static const IOutputStreamVtbl output_vtbl =
{
    output_QueryInterface, output_AddRef, output_Release, output_GetIids, output_GetRuntimeClassName, output_GetTrustLevel,
    output_WriteAsync, output_FlushAsync,
};
DEFINE_IINSPECTABLE(typed, IRandomAccessStreamWithContentType, struct file_stream, IRandomAccessStream_iface)
static const IRandomAccessStreamWithContentTypeVtbl typed_vtbl =
{
    typed_QueryInterface, typed_AddRef, typed_Release, typed_GetIids, typed_GetRuntimeClassName, typed_GetTrustLevel,
};
DEFINE_IINSPECTABLE(content, IContentTypeProvider, struct file_stream, IRandomAccessStream_iface)
static HRESULT WINAPI content_get_ContentType(IContentTypeProvider *iface, HSTRING *out)
{ return WindowsDuplicateString(impl_from_IContentTypeProvider(iface)->content_type, out); }
static const IContentTypeProviderVtbl content_vtbl =
{
    content_QueryInterface, content_AddRef, content_Release, content_GetIids, content_GetRuntimeClassName, content_GetTrustLevel,
    content_get_ContentType,
};
static HRESULT stream_from_handle(HANDLE file, HSTRING type, BOOL writable, IRandomAccessStream **out)
{
    struct file_stream *impl;
    HRESULT hr;
    if (!(impl = calloc(1, sizeof(*impl)))) { CloseHandle(file); return E_OUTOFMEMORY; }
    if (FAILED(hr = WindowsDuplicateString(type, &impl->content_type))) { free(impl); CloseHandle(file); return hr; }
    impl->IRandomAccessStream_iface.lpVtbl = &stream_vtbl;
    impl->IInputStream_iface.lpVtbl = &input_vtbl;
    impl->IOutputStream_iface.lpVtbl = &output_vtbl;
    impl->IClosable_iface.lpVtbl = &closable_vtbl;
    impl->IRandomAccessStreamWithContentType_iface.lpVtbl = &typed_vtbl;
    impl->IContentTypeProvider_iface.lpVtbl = &content_vtbl;
    impl->ref = 1;
    impl->file = file;
    impl->writable = writable;
    InitializeCriticalSection(&impl->cs);
    *out = &impl->IRandomAccessStream_iface;
    return S_OK;
}
HRESULT file_stream_create(const WCHAR *path, HSTRING content_type, BOOL writable, IRandomAccessStream **out)
{
    HANDLE file;
    *out = NULL;
    file = CreateFileW(path, GENERIC_READ | (writable ? GENERIC_WRITE : 0),
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
    return stream_from_handle(file, content_type, writable, out);
}
