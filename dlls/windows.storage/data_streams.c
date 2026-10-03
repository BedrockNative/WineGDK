/* WinRT Windows.Storage Implementation
 *
 * Copyright (C) 2025 Mohamad Al-Jaf
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


#include <stdint.h>
#include "streams_private.h"
#include "roapi.h"
#include "initguid.h"
#include "robuffer.h"

WINE_DEFAULT_DEBUG_CHANNEL(storage);

struct data_reader
{
    IDataReader IDataReader_iface;
    IClosable IClosable_iface;
    LONG ref;
    BYTE *bytes;
    UINT32 size, position;
    UnicodeEncoding encoding;
    ByteOrder order;
    InputStreamOptions options;
    BOOL closed;
};
struct data_writer
{
    IDataWriter IDataWriter_iface;
    IClosable IClosable_iface;
    LONG ref;
    BYTE *bytes;
    UINT32 size, capacity;
    UnicodeEncoding encoding;
    ByteOrder order;
    IOutputStream *stream;
    BOOL closed;
};
static HRESULT metadata_iids(REFIID iid, ULONG *count, IID **ids)
{
    if (!count || !ids) return E_POINTER;
    *count = 0;
    if (!(*ids = CoTaskMemAlloc(sizeof(**ids)))) return E_OUTOFMEMORY;
    **ids = *iid; *count = 1; return S_OK;
}
static HRESULT make_buffer(const BYTE *bytes, UINT32 size, IBuffer **out)
{
    static const WCHAR name[] = L"Windows.Storage.Streams.Buffer";
    HSTRING_HEADER header;
    HSTRING str;
    IBufferFactory *factory;
    IBufferByteAccess *access;
    BYTE *dst;
    HRESULT hr;
    *out = NULL;
    WindowsCreateStringReference(name, ARRAY_SIZE(name)-1, &header, &str);
    if (FAILED(hr = RoGetActivationFactory(str, &IID_IBufferFactory, (void **)&factory))) return hr;
    hr = IBufferFactory_Create(factory, size, out);
    IBufferFactory_Release(factory);
    if (FAILED(hr)) return hr;
    if (SUCCEEDED(hr = IBuffer_QueryInterface(*out, &IID_IBufferByteAccess, (void **)&access)))
    {
        if (SUCCEEDED(hr = IBufferByteAccess_Buffer(access, &dst)) && size) memcpy(dst, bytes, size);
        IBufferByteAccess_Release(access);
    }
    if (SUCCEEDED(hr)) hr = IBuffer_put_Length(*out, size);
    if (FAILED(hr)) { IBuffer_Release(*out); *out = NULL; }
    return hr;
}

static struct data_reader *reader_impl(IDataReader *iface)
{ return CONTAINING_RECORD(iface, struct data_reader, IDataReader_iface); }
static HRESULT WINAPI reader_QueryInterface(IDataReader *iface, REFIID iid, void **out)
{
    struct data_reader *impl = reader_impl(iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IDataReader)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IClosable)) *out = &impl->IClosable_iface;
    else { WARN("unsupported interface %s\n", debugstr_guid(iid)); return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}
static ULONG WINAPI reader_AddRef(IDataReader *iface)
{ return InterlockedIncrement(&reader_impl(iface)->ref); }
static ULONG WINAPI reader_Release(IDataReader *iface)
{
    struct data_reader *impl = reader_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { free(impl->bytes); free(impl); }
    return ref;
}
static HRESULT WINAPI reader_GetIids(IDataReader *iface, ULONG *count, IID **ids)
{ return metadata_iids(&IID_IDataReader, count, ids); }
static HRESULT WINAPI reader_GetRuntimeClassName(IDataReader *iface, HSTRING *value)
{
    static const WCHAR name[] = L"Windows.Storage.Streams.DataReader";
    return WindowsCreateString(name, ARRAY_SIZE(name)-1, value);
}
static HRESULT WINAPI reader_GetTrustLevel(IDataReader *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }

static HRESULT WINAPI reader_get_UnconsumedBufferLength(IDataReader *iface, UINT32 *value)
{
    struct data_reader *impl = reader_impl(iface);
    if (!value) return E_POINTER;
    if (impl->closed) return RO_E_CLOSED;
    *value = impl->size - impl->position; return S_OK;
}
static HRESULT WINAPI reader_get_UnicodeEncoding(IDataReader *iface, UnicodeEncoding *value)
{
    if (!value) return E_POINTER;
    if (reader_impl(iface)->closed) return RO_E_CLOSED;
    *value = reader_impl(iface)->encoding; return S_OK;
}
static HRESULT WINAPI reader_put_UnicodeEncoding(IDataReader *iface, UnicodeEncoding value)
{
    if (reader_impl(iface)->closed) return RO_E_CLOSED;
    if (value < UnicodeEncoding_Utf8 || value > UnicodeEncoding_Utf16BE) return E_INVALIDARG;
    reader_impl(iface)->encoding = value; return S_OK;
}
static HRESULT WINAPI reader_get_ByteOrder(IDataReader *iface, ByteOrder *value)
{
    if (!value) return E_POINTER;
    if (reader_impl(iface)->closed) return RO_E_CLOSED;
    *value = reader_impl(iface)->order; return S_OK;
}
static HRESULT WINAPI reader_put_ByteOrder(IDataReader *iface, ByteOrder value)
{
    if (reader_impl(iface)->closed) return RO_E_CLOSED;
    if (value != ByteOrder_LittleEndian && value != ByteOrder_BigEndian) return E_INVALIDARG;
    reader_impl(iface)->order = value; return S_OK;
}
static HRESULT WINAPI reader_get_InputStreamOptions(IDataReader *iface, InputStreamOptions *value)
{
    if (!value) return E_POINTER;
    if (reader_impl(iface)->closed) return RO_E_CLOSED;
    *value = reader_impl(iface)->options; return S_OK;
}
static HRESULT WINAPI reader_put_InputStreamOptions(IDataReader *iface, InputStreamOptions value)
{
    if (reader_impl(iface)->closed) return RO_E_CLOSED;
    if (value & ~(InputStreamOptions_Partial | InputStreamOptions_ReadAhead)) return E_INVALIDARG;
    reader_impl(iface)->options = value; return S_OK;
}

static HRESULT WINAPI reader_ReadBytes(IDataReader *iface, UINT32 count, BYTE *value)
{
    struct data_reader *impl = reader_impl(iface);
    if (count && !value) return E_POINTER;
    if (impl->closed) return RO_E_CLOSED;
    if (count > impl->size - impl->position) return E_BOUNDS;
    if (count) memcpy(value, impl->bytes + impl->position, count);
    impl->position += count; return S_OK;
}
static HRESULT reader_number(IDataReader *iface, void *value, UINT32 size)
{
    struct data_reader *impl = reader_impl(iface);
    BYTE bytes[8];
    UINT32 i;
    HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr = reader_ReadBytes(iface, size, bytes))) return hr;
    for (i = 0; i < size; ++i) ((BYTE *)value)[i] = bytes[impl->order == ByteOrder_BigEndian ? size - i - 1 : i];
    return S_OK;
}
static HRESULT WINAPI reader_ReadByte(IDataReader *iface, BYTE *value)
{ return reader_ReadBytes(iface, 1, value); }
static HRESULT WINAPI reader_ReadBuffer(IDataReader *iface, UINT32 count, IBuffer **value)
{
    struct data_reader *impl = reader_impl(iface);
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    if (impl->closed) return RO_E_CLOSED;
    if (count > impl->size - impl->position) return E_BOUNDS;
    hr = make_buffer(count ? impl->bytes + impl->position : NULL, count, value);
    if (SUCCEEDED(hr)) impl->position += count;
    return hr;
}
static HRESULT WINAPI reader_ReadBoolean(IDataReader *iface, boolean *value)
{
    BYTE byte;
    HRESULT hr;
    if (!value) return E_POINTER;
    hr = reader_ReadByte(iface, &byte);
    if (SUCCEEDED(hr)) *value = !!byte;
    return hr;
}
static HRESULT WINAPI reader_ReadGuid(IDataReader *iface, GUID *value)
{
    struct data_reader *impl = reader_impl(iface);
    if (!value) return E_POINTER;
    if (impl->closed) return RO_E_CLOSED;
    if (sizeof(*value) > impl->size - impl->position) return E_BOUNDS;
    reader_number(iface, &value->Data1, 4);
    reader_number(iface, &value->Data2, 2);
    reader_number(iface, &value->Data3, 2);
    return reader_ReadBytes(iface, sizeof(value->Data4), value->Data4);
}
static HRESULT WINAPI reader_ReadInt16(IDataReader *iface, INT16 *value)
{ return reader_number(iface, value, sizeof(*value)); }
static HRESULT WINAPI reader_ReadInt32(IDataReader *iface, INT32 *value)
{ return reader_number(iface, value, sizeof(*value)); }
static HRESULT WINAPI reader_ReadInt64(IDataReader *iface, INT64 *value)
{ return reader_number(iface, value, sizeof(*value)); }
static HRESULT WINAPI reader_ReadUInt16(IDataReader *iface, UINT16 *value)
{ return reader_number(iface, value, sizeof(*value)); }
static HRESULT WINAPI reader_ReadUInt32(IDataReader *iface, UINT32 *value)
{ return reader_number(iface, value, sizeof(*value)); }
static HRESULT WINAPI reader_ReadUInt64(IDataReader *iface, UINT64 *value)
{ return reader_number(iface, value, sizeof(*value)); }
static HRESULT WINAPI reader_ReadSingle(IDataReader *iface, FLOAT *value)
{ return reader_number(iface, value, sizeof(*value)); }
static HRESULT WINAPI reader_ReadDouble(IDataReader *iface, DOUBLE *value)
{ return reader_number(iface, value, sizeof(*value)); }
static HRESULT WINAPI reader_ReadDateTime(IDataReader *iface, DateTime *value)
{ return reader_number(iface, value, sizeof(*value)); }
static HRESULT WINAPI reader_ReadTimeSpan(IDataReader *iface, TimeSpan *value)
{ return reader_number(iface, value, sizeof(*value)); }

static HRESULT WINAPI reader_ReadString(IDataReader *iface, UINT32 count, HSTRING *value)
{
    struct data_reader *impl = reader_impl(iface);
    UINT32 bytes, i;
    int length;
    WCHAR *text;
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    if (impl->closed) return RO_E_CLOSED;
    if (count > INT_MAX / 2) return E_BOUNDS;
    bytes = impl->encoding == UnicodeEncoding_Utf8 ? count : count * 2;
    if (bytes > impl->size - impl->position) return E_BOUNDS;
    if (!count) return S_OK;
    if (impl->encoding == UnicodeEncoding_Utf8)
    {
        length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (char *)impl->bytes + impl->position, bytes, NULL, 0);
        if (!length) return HRESULT_FROM_WIN32(GetLastError());
    }
    else length = count;
    if (!(text = malloc(length * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    if (impl->encoding == UnicodeEncoding_Utf8)
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (char *)impl->bytes + impl->position, bytes, text, length);
    else for (i = 0; i < count; ++i)
    {
        BYTE *p = impl->bytes + impl->position + i * 2;
        text[i] = impl->encoding == UnicodeEncoding_Utf16BE ? (p[0] << 8) | p[1] : p[0] | (p[1] << 8);
    }
    hr = WindowsCreateString(text, length, value);
    free(text);
    if (SUCCEEDED(hr)) impl->position += bytes;
    return hr;
}
static HRESULT WINAPI reader_LoadAsync(IDataReader *iface, UINT32 count, IAsyncOperation_UINT32 **value)
{ if (!value) return E_POINTER; *value = NULL; return reader_impl(iface)->closed ? RO_E_CLOSED : E_ILLEGAL_METHOD_CALL; }
static HRESULT WINAPI reader_DetachBuffer(IDataReader *iface, IBuffer **value)
{ return reader_ReadBuffer(iface, reader_impl(iface)->size - reader_impl(iface)->position, value); }
static HRESULT WINAPI reader_DetachStream(IDataReader *iface, IInputStream **value)
{ if (!value) return E_POINTER; *value = NULL; return reader_impl(iface)->closed ? RO_E_CLOSED : S_OK; }

static const IDataReaderVtbl reader_vtbl =
{
    reader_QueryInterface,
    reader_AddRef,
    reader_Release,
    reader_GetIids,
    reader_GetRuntimeClassName,
    reader_GetTrustLevel,
    reader_get_UnconsumedBufferLength,
    reader_get_UnicodeEncoding,
    reader_put_UnicodeEncoding,
    reader_get_ByteOrder,
    reader_put_ByteOrder,
    reader_get_InputStreamOptions,
    reader_put_InputStreamOptions,
    reader_ReadByte,
    reader_ReadBytes,
    reader_ReadBuffer,
    reader_ReadBoolean,
    reader_ReadGuid,
    reader_ReadInt16,
    reader_ReadInt32,
    reader_ReadInt64,
    reader_ReadUInt16,
    reader_ReadUInt32,
    reader_ReadUInt64,
    reader_ReadSingle,
    reader_ReadDouble,
    reader_ReadString,
    reader_ReadDateTime,
    reader_ReadTimeSpan,
    reader_LoadAsync,
    reader_DetachBuffer,
    reader_DetachStream,
};
DEFINE_IINSPECTABLE(reader_close, IClosable, struct data_reader, IDataReader_iface)
static HRESULT WINAPI reader_close_Close(IClosable *iface)
{ impl_from_IClosable(iface)->closed = TRUE; return S_OK; }

static const IClosableVtbl reader_close_vtbl =
{
    reader_close_QueryInterface,
    reader_close_AddRef,
    reader_close_Release,
    reader_close_GetIids,
    reader_close_GetRuntimeClassName,
    reader_close_GetTrustLevel,
    reader_close_Close,
};

static struct data_writer *writer_impl(IDataWriter *iface)
{ return CONTAINING_RECORD(iface, struct data_writer, IDataWriter_iface); }
static HRESULT WINAPI writer_QueryInterface(IDataWriter *iface, REFIID iid, void **out)
{
    struct data_writer *impl = writer_impl(iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IDataWriter)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IClosable)) *out = &impl->IClosable_iface;
    else { WARN("unsupported interface %s\n", debugstr_guid(iid)); return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}
static ULONG WINAPI writer_AddRef(IDataWriter *iface)
{ return InterlockedIncrement(&writer_impl(iface)->ref); }
static ULONG WINAPI writer_Release(IDataWriter *iface)
{
    struct data_writer *impl = writer_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { if (impl->stream) IOutputStream_Release(impl->stream); free(impl->bytes); free(impl); }
    return ref;
}
static HRESULT WINAPI writer_GetIids(IDataWriter *iface, ULONG *count, IID **ids)
{ return metadata_iids(&IID_IDataWriter, count, ids); }
static HRESULT WINAPI writer_GetRuntimeClassName(IDataWriter *iface, HSTRING *value)
{
    static const WCHAR name[] = L"Windows.Storage.Streams.DataWriter";
    return WindowsCreateString(name, ARRAY_SIZE(name)-1, value);
}
static HRESULT WINAPI writer_GetTrustLevel(IDataWriter *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }

static HRESULT WINAPI writer_get_UnstoredBufferLength(IDataWriter *iface, UINT32 *value)
{ if (!value) return E_POINTER; if (writer_impl(iface)->closed) return RO_E_CLOSED; *value = writer_impl(iface)->size; return S_OK; }
static HRESULT WINAPI writer_get_UnicodeEncoding(IDataWriter *iface, UnicodeEncoding *value)
{ if (!value) return E_POINTER; if (writer_impl(iface)->closed) return RO_E_CLOSED; *value = writer_impl(iface)->encoding; return S_OK; }
static HRESULT WINAPI writer_put_UnicodeEncoding(IDataWriter *iface, UnicodeEncoding value)
{ if (writer_impl(iface)->closed) return RO_E_CLOSED; if (value < UnicodeEncoding_Utf8 || value > UnicodeEncoding_Utf16BE) return E_INVALIDARG; writer_impl(iface)->encoding = value; return S_OK; }
static HRESULT WINAPI writer_get_ByteOrder(IDataWriter *iface, ByteOrder *value)
{ if (!value) return E_POINTER; if (writer_impl(iface)->closed) return RO_E_CLOSED; *value = writer_impl(iface)->order; return S_OK; }
static HRESULT WINAPI writer_put_ByteOrder(IDataWriter *iface, ByteOrder value)
{ if (writer_impl(iface)->closed) return RO_E_CLOSED; if (value != ByteOrder_LittleEndian && value != ByteOrder_BigEndian) return E_INVALIDARG; writer_impl(iface)->order = value; return S_OK; }

static HRESULT WINAPI writer_WriteBytes(IDataWriter *iface, UINT32 count, BYTE *value)
{
    struct data_writer *impl = writer_impl(iface);
    BYTE *bytes;
    UINT32 size;
    if (count && !value) return E_POINTER;
    if (impl->closed) return RO_E_CLOSED;
    if (count > UINT32_MAX - impl->size) return E_OUTOFMEMORY;
    size = impl->size + count;
    if (size > impl->capacity)
    {
        if (!(bytes = realloc(impl->bytes, size))) return E_OUTOFMEMORY;
        impl->bytes = bytes; impl->capacity = size;
    }
    if (count) memcpy(impl->bytes + impl->size, value, count);
    impl->size = size;
    return S_OK;
}
static HRESULT writer_number(IDataWriter *iface, const void *value, UINT32 size)
{
    struct data_writer *impl = writer_impl(iface);
    BYTE bytes[8];
    UINT32 i;
    for (i = 0; i < size; ++i) bytes[i] = ((const BYTE *)value)[impl->order == ByteOrder_BigEndian ? size - i - 1 : i];
    return writer_WriteBytes(iface, size, bytes);
}
static HRESULT WINAPI writer_WriteByte(IDataWriter *iface, BYTE value)
{ return writer_WriteBytes(iface, 1, &value); }
static HRESULT WINAPI writer_WriteBufferRange(IDataWriter *iface, IBuffer *buffer, UINT32 start, UINT32 count)
{
    IBufferByteAccess *access;
    BYTE *bytes;
    UINT32 length;
    HRESULT hr;
    if (!buffer) return E_INVALIDARG;
    if (FAILED(hr = IBuffer_get_Length(buffer, &length))) return hr;
    if (start > length || count > length - start) return E_BOUNDS;
    if (FAILED(hr = IBuffer_QueryInterface(buffer, &IID_IBufferByteAccess, (void **)&access))) return hr;
    if (SUCCEEDED(hr = IBufferByteAccess_Buffer(access, &bytes))) hr = writer_WriteBytes(iface, count, count ? bytes + start : NULL);
    IBufferByteAccess_Release(access);
    return hr;
}
static HRESULT WINAPI writer_WriteBuffer(IDataWriter *iface, IBuffer *buffer)
{
    UINT32 length;
    HRESULT hr;
    if (!buffer) return E_INVALIDARG;
    if (FAILED(hr = IBuffer_get_Length(buffer, &length))) return hr;
    return writer_WriteBufferRange(iface, buffer, 0, length);
}
static HRESULT WINAPI writer_WriteBoolean(IDataWriter *iface, boolean value)
{ return writer_WriteByte(iface, !!value); }
static HRESULT WINAPI writer_WriteGuid(IDataWriter *iface, GUID value)
{
    HRESULT hr;
    if (FAILED(hr = writer_number(iface, &value.Data1, 4)) ||
        FAILED(hr = writer_number(iface, &value.Data2, 2)) ||
        FAILED(hr = writer_number(iface, &value.Data3, 2))) return hr;
    return writer_WriteBytes(iface, sizeof(value.Data4), value.Data4);
}
static HRESULT WINAPI writer_WriteInt16(IDataWriter *iface, INT16 value)
{ return writer_number(iface, &value, sizeof(value)); }
static HRESULT WINAPI writer_WriteInt32(IDataWriter *iface, INT32 value)
{ return writer_number(iface, &value, sizeof(value)); }
static HRESULT WINAPI writer_WriteInt64(IDataWriter *iface, INT64 value)
{ return writer_number(iface, &value, sizeof(value)); }
static HRESULT WINAPI writer_WriteUInt16(IDataWriter *iface, UINT16 value)
{ return writer_number(iface, &value, sizeof(value)); }
static HRESULT WINAPI writer_WriteUInt32(IDataWriter *iface, UINT32 value)
{ return writer_number(iface, &value, sizeof(value)); }
static HRESULT WINAPI writer_WriteUInt64(IDataWriter *iface, UINT64 value)
{ return writer_number(iface, &value, sizeof(value)); }
static HRESULT WINAPI writer_WriteSingle(IDataWriter *iface, FLOAT value)
{ return writer_number(iface, &value, sizeof(value)); }
static HRESULT WINAPI writer_WriteDouble(IDataWriter *iface, DOUBLE value)
{ return writer_number(iface, &value, sizeof(value)); }
static HRESULT WINAPI writer_WriteDateTime(IDataWriter *iface, DateTime value)
{ return writer_number(iface, &value, sizeof(value)); }
static HRESULT WINAPI writer_WriteTimeSpan(IDataWriter *iface, TimeSpan value)
{ return writer_number(iface, &value, sizeof(value)); }

static HRESULT WINAPI writer_MeasureString(IDataWriter *iface, HSTRING value, UINT32 *count)
{
    struct data_writer *impl = writer_impl(iface);
    UINT32 length;
    const WCHAR *str = WindowsGetStringRawBuffer(value, &length);
    if (!count) return E_POINTER;
    *count = 0;
    if (impl->closed) return RO_E_CLOSED;
    if (length > INT_MAX / 2) return E_INVALIDARG;
    if (!length) return S_OK;
    if (impl->encoding != UnicodeEncoding_Utf8) { *count = length * 2; return S_OK; }
    *count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str, length, NULL, 0, NULL, NULL);
    return *count ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}
static HRESULT WINAPI writer_WriteString(IDataWriter *iface, HSTRING value, UINT32 *count)
{
    struct data_writer *impl = writer_impl(iface);
    UINT32 length, i;
    const WCHAR *str = WindowsGetStringRawBuffer(value, &length);
    BYTE *bytes;
    HRESULT hr = writer_MeasureString(iface, value, count);
    if (FAILED(hr) || !*count) return hr;
    if (!(bytes = malloc(*count))) return E_OUTOFMEMORY;
    if (impl->encoding == UnicodeEncoding_Utf8)
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str, length, (char *)bytes, *count, NULL, NULL);
    else for (i = 0; i < length; ++i)
    {
        bytes[2*i] = impl->encoding == UnicodeEncoding_Utf16BE ? str[i] >> 8 : str[i];
        bytes[2*i+1] = impl->encoding == UnicodeEncoding_Utf16BE ? str[i] : str[i] >> 8;
    }
    hr = writer_WriteBytes(iface, *count, bytes);
    free(bytes);
    return hr;
}

struct write_waiter
{
    IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32 IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32_iface;
    LONG ref;
    HANDLE event;
};
static struct write_waiter *waiter_impl(IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32 *iface)
{ return CONTAINING_RECORD(iface, struct write_waiter, IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32_iface); }
static HRESULT WINAPI waiter_QueryInterface(IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32 *iface, REFIID iid, void **value)
{
    if (!value) return E_POINTER;
    *value = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IAgileObject) &&
        !IsEqualGUID(iid, &IID_IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32)) return E_NOINTERFACE;
    *value = iface; IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32_AddRef(iface); return S_OK;
}
static ULONG WINAPI waiter_AddRef(IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32 *iface)
{ return InterlockedIncrement(&waiter_impl(iface)->ref); }
static ULONG WINAPI waiter_Release(IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32 *iface)
{
    struct write_waiter *impl = waiter_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { CloseHandle(impl->event); free(impl); }
    return ref;
}
static HRESULT WINAPI waiter_Invoke(IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32 *iface,
    IAsyncOperationWithProgress_UINT32_UINT32 *operation, AsyncStatus status)
{ SetEvent(waiter_impl(iface)->event); return S_OK; }
static const IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32Vtbl waiter_vtbl =
{ waiter_QueryInterface, waiter_AddRef, waiter_Release, waiter_Invoke };

static HRESULT store_callback(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    IOutputStream *stream = (IOutputStream *)invoker;
    IAsyncOperationWithProgress_UINT32_UINT32 *operation;
    struct write_waiter *waiter;
    UINT32 count;
    HRESULT hr;
    if (!called_async) return STATUS_PENDING;
    if (!(waiter = calloc(1, sizeof(*waiter)))) return E_OUTOFMEMORY;
    waiter->IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32_iface.lpVtbl = &waiter_vtbl;
    waiter->ref = 1;
    if (!(waiter->event = CreateEventW(NULL, TRUE, FALSE, NULL))) { hr = HRESULT_FROM_WIN32(GetLastError()); free(waiter); return hr; }
    hr = IOutputStream_WriteAsync(stream, (IBuffer *)param, &operation);
    if (SUCCEEDED(hr))
    {
        hr = IAsyncOperationWithProgress_UINT32_UINT32_put_Completed(operation, &waiter->IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32_iface);
        if (SUCCEEDED(hr))
        {
            WaitForSingleObject(waiter->event, INFINITE);
            result->vt = VT_UI4;
            hr = IAsyncOperationWithProgress_UINT32_UINT32_GetResults(operation, &count);
            if (SUCCEEDED(hr)) result->ulVal = count;
        }
        IAsyncOperationWithProgress_UINT32_UINT32_Release(operation);
    }
    waiter_Release(&waiter->IAsyncOperationWithProgressCompletedHandler_UINT32_UINT32_iface);
    return hr;
}
static HRESULT WINAPI writer_StoreAsync(IDataWriter *iface, IAsyncOperation_UINT32 **value)
{
    struct data_writer *impl = writer_impl(iface);
    IBuffer *buffer;
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    if (impl->closed) return RO_E_CLOSED;
    if (!impl->stream) return E_ILLEGAL_METHOD_CALL;
    if (FAILED(hr = make_buffer(impl->bytes, impl->size, &buffer))) return hr;
    hr = async_operation_uint32_create((IUnknown *)impl->stream, (IUnknown *)buffer, store_callback, value);
    IBuffer_Release(buffer);
    if (SUCCEEDED(hr)) impl->size = 0;
    return hr;
}
static HRESULT WINAPI writer_FlushAsync(IDataWriter *iface, IAsyncOperation_boolean **value)
{
    struct data_writer *impl = writer_impl(iface);
    if (!value) return E_POINTER;
    *value = NULL;
    if (impl->closed) return RO_E_CLOSED;
    if (!impl->stream) return E_ILLEGAL_METHOD_CALL;
    return IOutputStream_FlushAsync(impl->stream, value);
}
static HRESULT WINAPI writer_DetachBuffer(IDataWriter *iface, IBuffer **value)
{
    struct data_writer *impl = writer_impl(iface);
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    if (impl->closed) return RO_E_CLOSED;
    hr = make_buffer(impl->bytes, impl->size, value);
    if (SUCCEEDED(hr)) impl->size = 0;
    return hr;
}
static HRESULT WINAPI writer_DetachStream(IDataWriter *iface, IOutputStream **value)
{
    struct data_writer *impl = writer_impl(iface);
    if (!value) return E_POINTER;
    *value = NULL;
    if (impl->closed) return RO_E_CLOSED;
    *value = impl->stream; impl->stream = NULL; return S_OK;
}

static const IDataWriterVtbl writer_vtbl =
{
    writer_QueryInterface,
    writer_AddRef,
    writer_Release,
    writer_GetIids,
    writer_GetRuntimeClassName,
    writer_GetTrustLevel,
    writer_get_UnstoredBufferLength,
    writer_get_UnicodeEncoding,
    writer_put_UnicodeEncoding,
    writer_get_ByteOrder,
    writer_put_ByteOrder,
    writer_WriteByte,
    writer_WriteBytes,
    writer_WriteBuffer,
    writer_WriteBufferRange,
    writer_WriteBoolean,
    writer_WriteGuid,
    writer_WriteInt16,
    writer_WriteInt32,
    writer_WriteInt64,
    writer_WriteUInt16,
    writer_WriteUInt32,
    writer_WriteUInt64,
    writer_WriteSingle,
    writer_WriteDouble,
    writer_WriteDateTime,
    writer_WriteTimeSpan,
    writer_WriteString,
    writer_MeasureString,
    writer_StoreAsync,
    writer_FlushAsync,
    writer_DetachBuffer,
    writer_DetachStream,
};
DEFINE_IINSPECTABLE_(writer_close, IClosable, struct data_writer, writer_closable_impl, IClosable_iface, &impl->IDataWriter_iface)
static HRESULT WINAPI writer_close_Close(IClosable *iface)
{
    struct data_writer *impl = writer_closable_impl(iface);
    IClosable *stream;
    if (impl->closed) return S_OK;
    impl->closed = TRUE;
    if (impl->stream && SUCCEEDED(IOutputStream_QueryInterface(impl->stream, &IID_IClosable, (void **)&stream)))
    { IClosable_Close(stream); IClosable_Release(stream); }
    return S_OK;
}

static const IClosableVtbl writer_close_vtbl =
{
    writer_close_QueryInterface,
    writer_close_AddRef,
    writer_close_Release,
    writer_close_GetIids,
    writer_close_GetRuntimeClassName,
    writer_close_GetTrustLevel,
    writer_close_Close,
};

static HRESULT reader_create(IBuffer *buffer, IDataReader **value)
{
    struct data_reader *impl;
    IBufferByteAccess *access;
    BYTE *bytes;
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!buffer) return E_INVALIDARG;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IDataReader_iface.lpVtbl = &reader_vtbl;
    impl->IClosable_iface.lpVtbl = &reader_close_vtbl;
    impl->ref = 1;
    impl->order = ByteOrder_BigEndian;
    if (SUCCEEDED(hr = IBuffer_get_Length(buffer, &impl->size)) &&
        SUCCEEDED(hr = IBuffer_QueryInterface(buffer, &IID_IBufferByteAccess, (void **)&access)))
    {
        if (SUCCEEDED(hr = IBufferByteAccess_Buffer(access, &bytes)) && impl->size)
        {
            if (!(impl->bytes = malloc(impl->size))) hr = E_OUTOFMEMORY;
            else memcpy(impl->bytes, bytes, impl->size);
        }
        IBufferByteAccess_Release(access);
    }
    if (FAILED(hr)) { IDataReader_Release(&impl->IDataReader_iface); return hr; }
    *value = &impl->IDataReader_iface; return S_OK;
}
static HRESULT writer_create(IOutputStream *stream, IDataWriter **value)
{
    struct data_writer *impl;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IDataWriter_iface.lpVtbl = &writer_vtbl;
    impl->IClosable_iface.lpVtbl = &writer_close_vtbl;
    impl->ref = 1;
    impl->order = ByteOrder_BigEndian;
    if ((impl->stream = stream)) IOutputStream_AddRef(stream);
    *value = &impl->IDataWriter_iface; return S_OK;
}
struct reader_factory
{
    IActivationFactory IActivationFactory_iface;
    IDataReaderStatics IDataReaderStatics_iface;
    IDataReaderFactory IDataReaderFactory_iface;
};
struct writer_factory
{
    IActivationFactory IActivationFactory_iface;
    IDataWriterFactory IDataWriterFactory_iface;
};
static HRESULT WINAPI reader_factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **value)
{
    struct reader_factory *impl = CONTAINING_RECORD(iface, struct reader_factory, IActivationFactory_iface);
    if (!value) return E_POINTER;
    *value = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IActivationFactory)) *value = iface;
    else if (IsEqualGUID(iid, &IID_IDataReaderStatics)) *value = &impl->IDataReaderStatics_iface;
    else if (IsEqualGUID(iid, &IID_IDataReaderFactory)) *value = &impl->IDataReaderFactory_iface;
    else return E_NOINTERFACE;
    IUnknown_AddRef((IUnknown *)*value); return S_OK;
}
static ULONG WINAPI reader_factory_AddRef(IActivationFactory *iface) { return 2; }
static ULONG WINAPI reader_factory_Release(IActivationFactory *iface) { return 1; }
static HRESULT WINAPI reader_factory_GetIids(IActivationFactory *iface, ULONG *count, IID **ids)
{ return metadata_iids(&IID_IActivationFactory, count, ids); }
static HRESULT WINAPI reader_factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *value)
{ return reader_GetRuntimeClassName(NULL, value); }
static HRESULT WINAPI reader_factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }
static HRESULT WINAPI reader_factory_ActivateInstance(IActivationFactory *iface, IInspectable **value)
{ if (!value) return E_POINTER; *value = NULL; return E_NOTIMPL; }

static const IActivationFactoryVtbl reader_factory_vtbl =
{
    reader_factory_QueryInterface,
    reader_factory_AddRef,
    reader_factory_Release,
    reader_factory_GetIids,
    reader_factory_GetRuntimeClassName,
    reader_factory_GetTrustLevel,
    reader_factory_ActivateInstance,
};
static HRESULT WINAPI writer_factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **value)
{
    struct writer_factory *impl = CONTAINING_RECORD(iface, struct writer_factory, IActivationFactory_iface);
    if (!value) return E_POINTER;
    *value = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IActivationFactory)) *value = iface;
    else if (IsEqualGUID(iid, &IID_IDataWriterFactory)) *value = &impl->IDataWriterFactory_iface;
    else return E_NOINTERFACE;
    IUnknown_AddRef((IUnknown *)*value); return S_OK;
}
static ULONG WINAPI writer_factory_AddRef(IActivationFactory *iface) { return 2; }
static ULONG WINAPI writer_factory_Release(IActivationFactory *iface) { return 1; }
static HRESULT WINAPI writer_factory_GetIids(IActivationFactory *iface, ULONG *count, IID **ids)
{ return metadata_iids(&IID_IActivationFactory, count, ids); }
static HRESULT WINAPI writer_factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *value)
{ return writer_GetRuntimeClassName(NULL, value); }
static HRESULT WINAPI writer_factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }
static HRESULT WINAPI writer_factory_ActivateInstance(IActivationFactory *iface, IInspectable **value)
{ return writer_create(NULL, (IDataWriter **)value); }

static const IActivationFactoryVtbl writer_factory_vtbl =
{
    writer_factory_QueryInterface,
    writer_factory_AddRef,
    writer_factory_Release,
    writer_factory_GetIids,
    writer_factory_GetRuntimeClassName,
    writer_factory_GetTrustLevel,
    writer_factory_ActivateInstance,
};
DEFINE_IINSPECTABLE(reader_statics, IDataReaderStatics, struct reader_factory, IActivationFactory_iface)
static HRESULT WINAPI reader_statics_FromBuffer(IDataReaderStatics *iface, IBuffer *buffer, IDataReader **value)
{ return reader_create(buffer, value); }

static const IDataReaderStaticsVtbl reader_statics_vtbl =
{
    reader_statics_QueryInterface,
    reader_statics_AddRef,
    reader_statics_Release,
    reader_statics_GetIids,
    reader_statics_GetRuntimeClassName,
    reader_statics_GetTrustLevel,
    reader_statics_FromBuffer,
};
DEFINE_IINSPECTABLE(reader_factory2, IDataReaderFactory, struct reader_factory, IActivationFactory_iface)
static HRESULT WINAPI reader_factory2_CreateDataReader(IDataReaderFactory *iface, IInputStream *stream, IDataReader **value)
{ if (!value) return E_POINTER; *value = NULL; return E_NOTIMPL; }

static const IDataReaderFactoryVtbl reader_factory2_vtbl =
{
    reader_factory2_QueryInterface,
    reader_factory2_AddRef,
    reader_factory2_Release,
    reader_factory2_GetIids,
    reader_factory2_GetRuntimeClassName,
    reader_factory2_GetTrustLevel,
    reader_factory2_CreateDataReader,
};
DEFINE_IINSPECTABLE(writer_factory2, IDataWriterFactory, struct writer_factory, IActivationFactory_iface)
static HRESULT WINAPI writer_factory2_CreateDataWriter(IDataWriterFactory *iface, IOutputStream *stream, IDataWriter **value)
{ return writer_create(stream, value); }

static const IDataWriterFactoryVtbl writer_factory2_vtbl =
{
    writer_factory2_QueryInterface,
    writer_factory2_AddRef,
    writer_factory2_Release,
    writer_factory2_GetIids,
    writer_factory2_GetRuntimeClassName,
    writer_factory2_GetTrustLevel,
    writer_factory2_CreateDataWriter,
};
static struct reader_factory reader_factory =
{ { &reader_factory_vtbl }, { &reader_statics_vtbl }, { &reader_factory2_vtbl } };
static struct writer_factory writer_factory =
{ { &writer_factory_vtbl }, { &writer_factory2_vtbl } };
IActivationFactory *data_reader_factory = &reader_factory.IActivationFactory_iface;
IActivationFactory *data_writer_factory = &writer_factory.IActivationFactory_iface;
