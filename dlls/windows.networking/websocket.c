/*
 * Copyright 2022 Zhiyi Zhang for CodeWeavers
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
#include "private.h"
#include "roapi.h"
#include "robuffer.h"
#include "winhttp.h"
#include "wine/winrt_events.h"

WINE_DEFAULT_DEBUG_CHANNEL(winsock);

struct socket_control
{
    IMessageWebSocketControl IMessageWebSocketControl_iface;
    IWebSocketControl IWebSocketControl_iface;
    LONG ref;
    SRWLOCK lock;
    UINT32 max_message, outbound_buffer;
    SocketMessageType message_type;
    IVector_HSTRING *protocols;
    BOOL connected;
};
struct message_args
{
    IMessageWebSocketMessageReceivedEventArgs IMessageWebSocketMessageReceivedEventArgs_iface;
    LONG ref;
    SocketMessageType type;
    IBuffer *buffer;
};
struct close_args
{
    IWebSocketClosedEventArgs IWebSocketClosedEventArgs_iface;
    LONG ref;
    UINT16 code;
    HSTRING reason;
};
struct message_socket
{
    IMessageWebSocket IMessageWebSocket_iface;
    IWebSocket IWebSocket_iface;
    IClosable IClosable_iface;
    IOutputStream IOutputStream_iface;
    IWebSocketInformation IWebSocketInformation_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    struct socket_control *control;
    struct winrt_event messages, closed_events;
    HINTERNET session, connection, request, socket;
    HSTRING uri, protocol;
    WCHAR *headers;
    BOOL started, closed;
    LONG sending, close_notified;
};
static HRESULT socket_close(struct message_socket *, UINT16, HSTRING);

static HRESULT metadata_iids(REFIID iid, ULONG *count, IID **ids)
{
    if (!count || !ids) return E_POINTER;
    *count = 0;
    if (!(*ids = CoTaskMemAlloc(sizeof(**ids)))) return E_OUTOFMEMORY;
    **ids = *iid;
    *count = 1;
    return S_OK;
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

static struct socket_control *control_impl(IMessageWebSocketControl *iface)
{ return CONTAINING_RECORD(iface, struct socket_control, IMessageWebSocketControl_iface); }
static HRESULT WINAPI control_QueryInterface(IMessageWebSocketControl *iface, REFIID iid, void **out)
{
    struct socket_control *impl = control_impl(iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IMessageWebSocketControl)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IWebSocketControl)) *out = &impl->IWebSocketControl_iface;
    else { WARN("unsupported interface %s\n", debugstr_guid(iid)); return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}
static ULONG WINAPI control_AddRef(IMessageWebSocketControl *iface)
{ return InterlockedIncrement(&control_impl(iface)->ref); }
static ULONG WINAPI control_Release(IMessageWebSocketControl *iface)
{
    struct socket_control *impl = control_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { IVector_HSTRING_Release(impl->protocols); free(impl); }
    return ref;
}
static HRESULT WINAPI control_GetIids(IMessageWebSocketControl *iface, ULONG *count, IID **ids)
{ return metadata_iids(&IID_IMessageWebSocketControl, count, ids); }
static HRESULT WINAPI control_GetRuntimeClassName(IMessageWebSocketControl *iface, HSTRING *value)
{
    static const WCHAR name[] = L"Windows.Networking.Sockets.MessageWebSocketControl";
    return WindowsCreateString(name, ARRAY_SIZE(name)-1, value);
}
static HRESULT WINAPI control_GetTrustLevel(IMessageWebSocketControl *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }
static HRESULT WINAPI control_get_MaxMessageSize(IMessageWebSocketControl *iface, UINT32 *value)
{ if (!value) return E_POINTER; *value = InterlockedCompareExchange((LONG *)&control_impl(iface)->max_message, 0, 0); return S_OK; }
static HRESULT WINAPI control_put_MaxMessageSize(IMessageWebSocketControl *iface, UINT32 value)
{
    InterlockedExchange((LONG *)&control_impl(iface)->max_message, value);
    return S_OK;
}
static HRESULT WINAPI control_get_MessageType(IMessageWebSocketControl *iface, SocketMessageType *value)
{ if (!value) return E_POINTER; *value = InterlockedCompareExchange((LONG *)&control_impl(iface)->message_type, 0, 0); return S_OK; }
static HRESULT WINAPI control_put_MessageType(IMessageWebSocketControl *iface, SocketMessageType value)
{
    if (value != SocketMessageType_Binary && value != SocketMessageType_Utf8) return E_INVALIDARG;
    InterlockedExchange((LONG *)&control_impl(iface)->message_type, value);
    return S_OK;
}

static const IMessageWebSocketControlVtbl control_vtbl =
{
    control_QueryInterface,
    control_AddRef,
    control_Release,
    control_GetIids,
    control_GetRuntimeClassName,
    control_GetTrustLevel,
    control_get_MaxMessageSize,
    control_put_MaxMessageSize,
    control_get_MessageType,
    control_put_MessageType,
};

DEFINE_IINSPECTABLE(web_control, IWebSocketControl, struct socket_control, IMessageWebSocketControl_iface)
static HRESULT WINAPI web_control_get_OutboundBufferSizeInBytes(IWebSocketControl *iface, UINT32 *value)
{ if (!value) return E_POINTER; *value = impl_from_IWebSocketControl(iface)->outbound_buffer; return S_OK; }
static HRESULT WINAPI web_control_put_OutboundBufferSizeInBytes(IWebSocketControl *iface, UINT32 value)
{
    struct socket_control *impl = impl_from_IWebSocketControl(iface);
    HRESULT hr = S_OK;
    AcquireSRWLockExclusive(&impl->lock);
    if (impl->connected) hr = E_ILLEGAL_METHOD_CALL;
    else impl->outbound_buffer = value;
    ReleaseSRWLockExclusive(&impl->lock);
    return hr;
}
static HRESULT WINAPI web_control_get_ServerCredential(IWebSocketControl *iface, IPasswordCredential **value)
{ if (!value) return E_POINTER; *value = NULL; return S_OK; }
static HRESULT WINAPI web_control_put_ServerCredential(IWebSocketControl *iface, IPasswordCredential *value)
{ return value ? E_NOTIMPL : S_OK; }
static HRESULT WINAPI web_control_get_ProxyCredential(IWebSocketControl *iface, IPasswordCredential **value)
{ if (!value) return E_POINTER; *value = NULL; return S_OK; }
static HRESULT WINAPI web_control_put_ProxyCredential(IWebSocketControl *iface, IPasswordCredential *value)
{ return value ? E_NOTIMPL : S_OK; }
static HRESULT WINAPI web_control_get_SupportedProtocols(IWebSocketControl *iface, IVector_HSTRING **value)
{
    if (!value) return E_POINTER;
    *value = impl_from_IWebSocketControl(iface)->protocols;
    IVector_HSTRING_AddRef(*value);
    return S_OK;
}

static const IWebSocketControlVtbl web_control_vtbl =
{
    web_control_QueryInterface,
    web_control_AddRef,
    web_control_Release,
    web_control_GetIids,
    web_control_GetRuntimeClassName,
    web_control_GetTrustLevel,
    web_control_get_OutboundBufferSizeInBytes,
    web_control_put_OutboundBufferSizeInBytes,
    web_control_get_ServerCredential,
    web_control_put_ServerCredential,
    web_control_get_ProxyCredential,
    web_control_put_ProxyCredential,
    web_control_get_SupportedProtocols,
};

static struct message_args *message_impl(IMessageWebSocketMessageReceivedEventArgs *iface)
{ return CONTAINING_RECORD(iface, struct message_args, IMessageWebSocketMessageReceivedEventArgs_iface); }
static HRESULT WINAPI message_QueryInterface(IMessageWebSocketMessageReceivedEventArgs *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IMessageWebSocketMessageReceivedEventArgs)) *out = iface;
    else { WARN("unsupported interface %s\n", debugstr_guid(iid)); return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}
static ULONG WINAPI message_AddRef(IMessageWebSocketMessageReceivedEventArgs *iface)
{ return InterlockedIncrement(&message_impl(iface)->ref); }
static ULONG WINAPI message_Release(IMessageWebSocketMessageReceivedEventArgs *iface)
{
    struct message_args *impl = message_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { IBuffer_Release(impl->buffer); free(impl); }
    return ref;
}
static HRESULT WINAPI message_GetIids(IMessageWebSocketMessageReceivedEventArgs *iface, ULONG *count, IID **ids)
{ return metadata_iids(&IID_IMessageWebSocketMessageReceivedEventArgs, count, ids); }
static HRESULT WINAPI message_GetRuntimeClassName(IMessageWebSocketMessageReceivedEventArgs *iface, HSTRING *value)
{
    static const WCHAR name[] = L"Windows.Networking.Sockets.MessageWebSocketMessageReceivedEventArgs";
    return WindowsCreateString(name, ARRAY_SIZE(name)-1, value);
}
static HRESULT WINAPI message_GetTrustLevel(IMessageWebSocketMessageReceivedEventArgs *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }

static HRESULT WINAPI message_get_MessageType(IMessageWebSocketMessageReceivedEventArgs *iface, SocketMessageType *value)
{ if (!value) return E_POINTER; *value = message_impl(iface)->type; return S_OK; }
static HRESULT WINAPI message_GetDataReader(IMessageWebSocketMessageReceivedEventArgs *iface, IDataReader **value)
{
    static const WCHAR name[] = L"Windows.Storage.Streams.DataReader";
    HSTRING_HEADER header;
    HSTRING str;
    IDataReaderStatics *factory;
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    WindowsCreateStringReference(name, ARRAY_SIZE(name)-1, &header, &str);
    if (FAILED(hr = RoGetActivationFactory(str, &IID_IDataReaderStatics, (void **)&factory))) return hr;
    hr = IDataReaderStatics_FromBuffer(factory, message_impl(iface)->buffer, value);
    IDataReaderStatics_Release(factory);
    return hr;
}
static HRESULT WINAPI message_GetDataStream(IMessageWebSocketMessageReceivedEventArgs *iface, IInputStream **value)
{ if (!value) return E_POINTER; *value = NULL; return E_NOTIMPL; }

static const IMessageWebSocketMessageReceivedEventArgsVtbl message_vtbl =
{
    message_QueryInterface,
    message_AddRef,
    message_Release,
    message_GetIids,
    message_GetRuntimeClassName,
    message_GetTrustLevel,
    message_get_MessageType,
    message_GetDataReader,
    message_GetDataStream,
};

static struct close_args *closed_impl(IWebSocketClosedEventArgs *iface)
{ return CONTAINING_RECORD(iface, struct close_args, IWebSocketClosedEventArgs_iface); }
static HRESULT WINAPI closed_QueryInterface(IWebSocketClosedEventArgs *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IWebSocketClosedEventArgs)) *out = iface;
    else { WARN("unsupported interface %s\n", debugstr_guid(iid)); return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}
static ULONG WINAPI closed_AddRef(IWebSocketClosedEventArgs *iface)
{ return InterlockedIncrement(&closed_impl(iface)->ref); }
static ULONG WINAPI closed_Release(IWebSocketClosedEventArgs *iface)
{
    struct close_args *impl = closed_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { WindowsDeleteString(impl->reason); free(impl); }
    return ref;
}
static HRESULT WINAPI closed_GetIids(IWebSocketClosedEventArgs *iface, ULONG *count, IID **ids)
{ return metadata_iids(&IID_IWebSocketClosedEventArgs, count, ids); }
static HRESULT WINAPI closed_GetRuntimeClassName(IWebSocketClosedEventArgs *iface, HSTRING *value)
{
    static const WCHAR name[] = L"Windows.Networking.Sockets.WebSocketClosedEventArgs";
    return WindowsCreateString(name, ARRAY_SIZE(name)-1, value);
}
static HRESULT WINAPI closed_GetTrustLevel(IWebSocketClosedEventArgs *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }

static HRESULT WINAPI closed_get_Code(IWebSocketClosedEventArgs *iface, UINT16 *value)
{ if (!value) return E_POINTER; *value = closed_impl(iface)->code; return S_OK; }
static HRESULT WINAPI closed_get_Reason(IWebSocketClosedEventArgs *iface, HSTRING *value)
{ return WindowsDuplicateString(closed_impl(iface)->reason, value); }

static const IWebSocketClosedEventArgsVtbl closed_vtbl =
{
    closed_QueryInterface,
    closed_AddRef,
    closed_Release,
    closed_GetIids,
    closed_GetRuntimeClassName,
    closed_GetTrustLevel,
    closed_get_Code,
    closed_get_Reason,
};

static struct message_socket *socket_impl(IMessageWebSocket *iface)
{ return CONTAINING_RECORD(iface, struct message_socket, IMessageWebSocket_iface); }
static HRESULT WINAPI socket_QueryInterface(IMessageWebSocket *iface, REFIID iid, void **out)
{
    struct message_socket *impl = socket_impl(iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IMessageWebSocket)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IWebSocket)) *out = &impl->IWebSocket_iface;
    else if (IsEqualGUID(iid, &IID_IClosable)) *out = &impl->IClosable_iface;
    else if (IsEqualGUID(iid, &IID_IOutputStream)) *out = &impl->IOutputStream_iface;
    else if (IsEqualGUID(iid, &IID_IWebSocketInformation)) *out = &impl->IWebSocketInformation_iface;
    else { WARN("unsupported interface %s\n", debugstr_guid(iid)); return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}
static ULONG WINAPI socket_AddRef(IMessageWebSocket *iface)
{ return InterlockedIncrement(&socket_impl(iface)->ref); }
static ULONG WINAPI socket_Release(IMessageWebSocket *iface)
{
    struct message_socket *impl = socket_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { socket_close(impl, 1000, NULL);
        winrt_event_clear(&impl->messages); winrt_event_clear(&impl->closed_events);
        IMessageWebSocketControl_Release(&impl->control->IMessageWebSocketControl_iface);
        WindowsDeleteString(impl->uri); WindowsDeleteString(impl->protocol);
        if (impl->headers) { SecureZeroMemory(impl->headers, wcslen(impl->headers)*sizeof(WCHAR)); free(impl->headers); }
        DeleteCriticalSection(&impl->cs); free(impl); }
    return ref;
}
static HRESULT WINAPI socket_GetIids(IMessageWebSocket *iface, ULONG *count, IID **ids)
{ return metadata_iids(&IID_IMessageWebSocket, count, ids); }
static HRESULT WINAPI socket_GetRuntimeClassName(IMessageWebSocket *iface, HSTRING *value)
{
    static const WCHAR name[] = L"Windows.Networking.Sockets.MessageWebSocket";
    return WindowsCreateString(name, ARRAY_SIZE(name)-1, value);
}
static HRESULT WINAPI socket_GetTrustLevel(IMessageWebSocket *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }

static HRESULT WINAPI socket_get_Control(IMessageWebSocket *iface, IMessageWebSocketControl **value)
{
    if (!value) return E_POINTER;
    *value = &socket_impl(iface)->control->IMessageWebSocketControl_iface;
    IMessageWebSocketControl_AddRef(*value);
    return S_OK;
}
static HRESULT WINAPI socket_get_Information(IMessageWebSocket *iface, IWebSocketInformation **value)
{ return socket_QueryInterface(iface, &IID_IWebSocketInformation, (void **)value); }
static HRESULT WINAPI socket_add_MessageReceived(IMessageWebSocket *iface,
    ITypedEventHandler_MessageWebSocket_MessageWebSocketMessageReceivedEventArgs *handler, EventRegistrationToken *token)
{ return winrt_event_add(&socket_impl(iface)->messages, handler, token); }
static HRESULT WINAPI socket_remove_MessageReceived(IMessageWebSocket *iface, EventRegistrationToken token)
{ return winrt_event_remove(&socket_impl(iface)->messages, token); }

static const IMessageWebSocketVtbl socket_vtbl =
{
    socket_QueryInterface,
    socket_AddRef,
    socket_Release,
    socket_GetIids,
    socket_GetRuntimeClassName,
    socket_GetTrustLevel,
    socket_get_Control,
    socket_get_Information,
    socket_add_MessageReceived,
    socket_remove_MessageReceived,
};

static void notify_closed(struct message_socket *impl, UINT16 code, const char *reason, UINT32 size)
{
    struct close_args *args;
    WCHAR text[123];
    int length;
    if (InterlockedExchange(&impl->close_notified, 1)) return;
    if (!(args = calloc(1, sizeof(*args)))) return;
    args->IWebSocketClosedEventArgs_iface.lpVtbl = &closed_vtbl;
    args->ref = 1;
    args->code = code;
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reason, size, text, ARRAY_SIZE(text));
    if (length) WindowsCreateString(text, length, &args->reason);
    TRACE("WebSocket closed, code %u.\n", code);
    winrt_event_notify(&impl->closed_events, &impl->IWebSocket_iface, &args->IWebSocketClosedEventArgs_iface);
    IWebSocketClosedEventArgs_Release(&args->IWebSocketClosedEventArgs_iface);
}
static HRESULT socket_close(struct message_socket *impl, UINT16 code, HSTRING reason)
{
    HINTERNET socket, request, connection, session;
    char text[123];
    UINT32 length;
    const WCHAR *str = WindowsGetStringRawBuffer(reason, &length);
    int bytes = length ? WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str, length, text, sizeof(text), NULL, NULL) : 0;
    if (length && !bytes) return E_INVALIDARG;
    if (code != 1000 && (code < 1001 || code == 1004 || code == 1005 || code == 1006 || code == 1015 || code >= 5000)) return E_INVALIDARG;
    EnterCriticalSection(&impl->cs);
    impl->closed = TRUE;
    socket = impl->socket; impl->socket = NULL;
    request = impl->request; impl->request = NULL;
    connection = impl->connection; impl->connection = NULL;
    session = impl->session; impl->session = NULL;
    LeaveCriticalSection(&impl->cs);
    if (socket)
    {
        WinHttpWebSocketShutdown(socket, code, text, bytes);
        WinHttpCloseHandle(socket);
    }
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    return S_OK;
}

static DWORD WINAPI receive_thread(void *param)
{
    struct message_socket *impl = param;
    BYTE chunk[16384], *message = NULL, *tmp;
    UINT32 size = 0, capacity = 0, max_size;
    DWORD count, error, reason_size = 0;
    UINT16 close_code = 1006;
    char reason[123];
    WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
    HINTERNET handle;
    struct message_args *args;
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    for (;;)
    {
        EnterCriticalSection(&impl->cs);
        handle = impl->socket;
        LeaveCriticalSection(&impl->cs);
        if (!handle) break;
        error = WinHttpWebSocketReceive(handle, chunk, sizeof(chunk), &count, &type);
        if (error) { WARN("WebSocket receive failed, error %lu.\n", error); break; }
        if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
        {
            if (WinHttpWebSocketQueryCloseStatus(handle, &close_code, reason, sizeof(reason), &reason_size))
            { close_code = 1006; reason_size = 0; }
            break;
        }
        max_size = InterlockedCompareExchange((LONG *)&impl->control->max_message, 0, 0);
        if (count > UINT32_MAX - size || (max_size && size + count > max_size))
        { close_code = 1009; break; }
        if (size + count > capacity)
        {
            capacity = size + count;
            if (!(tmp = realloc(message, capacity))) { close_code = 1011; break; }
            message = tmp;
        }
        if (count) memcpy(message + size, chunk, count);
        size += count;
        if (type != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE && type != WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) continue;
        if (!(args = calloc(1, sizeof(*args)))) { close_code = 1011; break; }
        args->IMessageWebSocketMessageReceivedEventArgs_iface.lpVtbl = &message_vtbl;
        args->ref = 1;
        args->type = type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ? SocketMessageType_Utf8 : SocketMessageType_Binary;
        if (FAILED(make_buffer(message, size, &args->buffer))) { free(args); close_code = 1011; break; }
        TRACE("WebSocket received message, bytes %u, type %u.\n", size, args->type);
        winrt_event_notify(&impl->messages, &impl->IMessageWebSocket_iface, &args->IMessageWebSocketMessageReceivedEventArgs_iface);
        IMessageWebSocketMessageReceivedEventArgs_Release(&args->IMessageWebSocketMessageReceivedEventArgs_iface);
        size = 0;
    }
    free(message);
    socket_close(impl, close_code == 1006 ? 1000 : close_code, NULL);
    notify_closed(impl, close_code, reason, reason_size);
    if (SUCCEEDED(hr)) RoUninitialize();
    IMessageWebSocket_Release(&impl->IMessageWebSocket_iface);
    return 0;
}

/* Header values can contain account credentials. Never log them. */
static BOOL valid_header(const WCHAR *str, UINT32 size, BOOL name)
{
    UINT32 i;
    if (name && !size) return FALSE;
    for (i = 0; i < size; ++i)
        if (!str[i] || str[i] == '\r' || str[i] == '\n' ||
            (name && (str[i] <= 32 || str[i] >= 127 || wcschr(L"()<>@,;:\"/[]?={}\\", str[i])))) return FALSE;
    return TRUE;
}
static HRESULT add_protocols(struct message_socket *impl, HINTERNET request)
{
    UINT32 count, i, length;
    HSTRING value;
    WCHAR *header = NULL, *tmp;
    size_t size = wcslen(L"Sec-WebSocket-Protocol: ");
    HRESULT hr;
    if (FAILED(hr = IVector_HSTRING_get_Size(impl->control->protocols, &count)) || !count) return hr;
    if (!(header = malloc((size+1)*sizeof(WCHAR)))) return E_OUTOFMEMORY;
    wcscpy(header, L"Sec-WebSocket-Protocol: ");
    for (i = 0; i < count; ++i)
    {
        const WCHAR *str;
        if (FAILED(hr = IVector_HSTRING_GetAt(impl->control->protocols, i, &value))) break;
        str = WindowsGetStringRawBuffer(value, &length);
        if (!valid_header(str, length, TRUE)) hr = E_INVALIDARG;
        else if (!(tmp = realloc(header, (size+length+3)*sizeof(WCHAR)))) hr = E_OUTOFMEMORY;
        else
        {
            header = tmp;
            if (i) { header[size++] = ','; header[size++] = ' '; }
            memcpy(header+size, str, length*sizeof(WCHAR)); size += length; header[size] = 0;
        }
        WindowsDeleteString(value);
        if (FAILED(hr)) break;
    }
    if (SUCCEEDED(hr) && !WinHttpAddRequestHeaders(request, header, size, WINHTTP_ADDREQ_FLAG_ADD)) hr = HRESULT_FROM_WIN32(GetLastError());
    free(header);
    return hr;
}
static HRESULT connect_callback(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    struct message_socket *impl = socket_impl((IMessageWebSocket *)invoker);
    URL_COMPONENTS parts = { sizeof(parts) };
    const WCHAR *uri;
    WCHAR *url = NULL, *host = NULL, *path = NULL, protocol[256];
    HINTERNET session = NULL, connection = NULL, request = NULL, socket = NULL;
    DWORD secure, status, size, disabled = WINHTTP_DISABLE_REDIRECTS;
    HANDLE thread;
    HRESULT hr = S_OK;
    if (!called_async) return STATUS_PENDING;
    uri = WindowsGetStringRawBuffer(impl->uri, NULL);
    secure = !wcsncmp(uri, L"wss://", 6) ? WINHTTP_FLAG_SECURE : 0;
    if (!(url = malloc((wcslen(uri)+3)*sizeof(WCHAR)))) return E_OUTOFMEMORY;
    wcscpy(url, secure ? L"https" : L"http");
    wcscat(url, uri + (secure ? 3 : 2));
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = parts.dwUserNameLength = parts.dwPasswordLength = -1;
    if (!WinHttpCrackUrl(url, 0, 0, &parts)) { hr = HRESULT_FROM_WIN32(GetLastError()); goto done; }
    if (parts.dwUserNameLength || parts.dwPasswordLength) { hr = E_INVALIDARG; goto done; }
    if (!(host = malloc((parts.dwHostNameLength+1)*sizeof(WCHAR))) ||
        !(path = malloc((parts.dwUrlPathLength+parts.dwExtraInfoLength+2)*sizeof(WCHAR)))) { hr = E_OUTOFMEMORY; goto done; }
    memcpy(host, parts.lpszHostName, parts.dwHostNameLength*sizeof(WCHAR)); host[parts.dwHostNameLength] = 0;
    size = parts.dwUrlPathLength;
    if (size) memcpy(path, parts.lpszUrlPath, size*sizeof(WCHAR)); else path[size++] = '/';
    if (parts.dwExtraInfoLength) memcpy(path+size, parts.lpszExtraInfo, parts.dwExtraInfoLength*sizeof(WCHAR));
    path[size+parts.dwExtraInfoLength] = 0;
    EnterCriticalSection(&impl->cs);
    if (impl->closed) hr = RO_E_CLOSED;
    else if (!(session = impl->session = WinHttpOpen(NULL, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0))) hr = HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr)) WinHttpSetTimeouts(session, 30000, 30000, 30000, 30000);
    if (SUCCEEDED(hr) && !(connection = impl->connection = WinHttpConnect(session, host, parts.nPort, 0))) hr = HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr) && !(request = impl->request = WinHttpOpenRequest(connection, L"GET", path, NULL, NULL, NULL, secure))) hr = HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr) && (!WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) ||
        !WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)))) hr = HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr) && impl->headers && !WinHttpAddRequestHeaders(request, impl->headers, -1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) hr = HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr)) hr = add_protocols(impl, request);
    LeaveCriticalSection(&impl->cs);
    if (FAILED(hr)) goto done;
    if (!WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0) || !WinHttpReceiveResponse(request, NULL))
    { hr = HRESULT_FROM_WIN32(GetLastError()); goto done; }
    size = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL))
    { hr = HRESULT_FROM_WIN32(GetLastError()); goto done; }
    if (status != 101) { WARN("WebSocket handshake HTTP %lu.\n", status); hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_HTTP, status); goto done; }
    EnterCriticalSection(&impl->cs);
    if (impl->closed) hr = RO_E_CLOSED;
    else if (!(socket = impl->socket = WinHttpWebSocketCompleteUpgrade(request, 0))) hr = HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr))
    {
        size = sizeof(protocol);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"Sec-WebSocket-Protocol", protocol, &size, NULL))
            hr = WindowsCreateString(protocol, wcslen(protocol), &impl->protocol);
        WinHttpCloseHandle(impl->request); impl->request = NULL;
        IMessageWebSocket_AddRef(&impl->IMessageWebSocket_iface);
        if (!(thread = CreateThread(NULL, 0, receive_thread, impl, 0, NULL)))
        { hr = HRESULT_FROM_WIN32(GetLastError()); IMessageWebSocket_Release(&impl->IMessageWebSocket_iface); }
        else CloseHandle(thread);
    }
    LeaveCriticalSection(&impl->cs);
    if (SUCCEEDED(hr)) TRACE("WebSocket connected to %s, HTTP 101.\n", debugstr_w(host));
done:
    free(url); free(host); free(path);
    if (FAILED(hr)) { WARN("WebSocket connection failed, hr %#lx.\n", hr); socket_close(impl, 1000, NULL); }
    return hr;
}

DEFINE_IINSPECTABLE(web_socket, IWebSocket, struct message_socket, IMessageWebSocket_iface)
static HRESULT WINAPI web_socket_get_OutputStream(IWebSocket *iface, IOutputStream **value)
{ return web_socket_QueryInterface(iface, &IID_IOutputStream, (void **)value); }
static HRESULT WINAPI web_socket_ConnectAsync(IWebSocket *iface, IUriRuntimeClass *uri, IAsyncAction **value)
{
    struct message_socket *impl = impl_from_IWebSocket(iface);
    HSTRING text;
    const WCHAR *str;
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!uri) return E_INVALIDARG;
    if (FAILED(hr = IUriRuntimeClass_get_AbsoluteUri(uri, &text))) return hr;
    str = WindowsGetStringRawBuffer(text, NULL);
    if ((wcsncmp(str, L"ws://", 5) && wcsncmp(str, L"wss://", 6)) || wcschr(str, '#'))
    { WindowsDeleteString(text); return E_INVALIDARG; }
    EnterCriticalSection(&impl->cs);
    if (impl->closed) hr = RO_E_CLOSED;
    else if (impl->started) hr = E_ILLEGAL_METHOD_CALL;
    else
    {
        impl->started = TRUE;
        impl->uri = text; text = NULL;
        AcquireSRWLockExclusive(&impl->control->lock);
        impl->control->connected = TRUE;
        ReleaseSRWLockExclusive(&impl->control->lock);
        hr = async_action_create((IUnknown *)&impl->IMessageWebSocket_iface, connect_callback, value);
        if (FAILED(hr)) { impl->started = FALSE; WindowsDeleteString(impl->uri); impl->uri = NULL; }
    }
    LeaveCriticalSection(&impl->cs);
    WindowsDeleteString(text);
    return hr;
}
static HRESULT WINAPI web_socket_SetRequestHeader(IWebSocket *iface, HSTRING name, HSTRING value)
{
    struct message_socket *impl = impl_from_IWebSocket(iface);
    UINT32 namesize, valuesize;
    const WCHAR *n = WindowsGetStringRawBuffer(name, &namesize), *v = WindowsGetStringRawBuffer(value, &valuesize);
    size_t size;
    WCHAR *headers;
    HRESULT hr = S_OK;
    if (!valid_header(n, namesize, TRUE) || !valid_header(v, valuesize, FALSE)) return E_INVALIDARG;
    if (!_wcsicmp(n, L"Host") || !_wcsicmp(n, L"Connection") || !_wcsicmp(n, L"Upgrade") ||
        !_wcsicmp(n, L"Sec-WebSocket-Key") || !_wcsicmp(n, L"Sec-WebSocket-Version")) return E_INVALIDARG;
    EnterCriticalSection(&impl->cs);
    size = impl->headers ? wcslen(impl->headers) : 0;
    if (impl->closed) hr = RO_E_CLOSED;
    else if (impl->started) hr = E_ILLEGAL_METHOD_CALL;
    else if (size > 65536 || namesize > 65536 || valuesize > 65536) hr = E_INVALIDARG;
    else if (!(headers = malloc((size+namesize+valuesize+5)*sizeof(WCHAR)))) hr = E_OUTOFMEMORY;
    else
    {
        if (size) memcpy(headers, impl->headers, size*sizeof(WCHAR));
        memcpy(headers+size, n, namesize*sizeof(WCHAR)); size += namesize;
        headers[size++] = ':'; headers[size++] = ' ';
        memcpy(headers+size, v, valuesize*sizeof(WCHAR)); size += valuesize;
        headers[size++] = '\r'; headers[size++] = '\n'; headers[size] = 0;
        if (impl->headers) { SecureZeroMemory(impl->headers, wcslen(impl->headers)*sizeof(WCHAR)); free(impl->headers); }
        impl->headers = headers;
    }
    LeaveCriticalSection(&impl->cs);
    return hr;
}
static HRESULT WINAPI web_socket_add_Closed(IWebSocket *iface, ITypedEventHandler_IWebSocket_WebSocketClosedEventArgs *handler, EventRegistrationToken *token)
{ return winrt_event_add(&impl_from_IWebSocket(iface)->closed_events, handler, token); }
static HRESULT WINAPI web_socket_remove_Closed(IWebSocket *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_IWebSocket(iface)->closed_events, token); }
static HRESULT WINAPI web_socket_Close(IWebSocket *iface, UINT16 code, HSTRING reason)
{ return socket_close(impl_from_IWebSocket(iface), code, reason); }

static const IWebSocketVtbl web_socket_vtbl =
{
    web_socket_QueryInterface,
    web_socket_AddRef,
    web_socket_Release,
    web_socket_GetIids,
    web_socket_GetRuntimeClassName,
    web_socket_GetTrustLevel,
    web_socket_get_OutputStream,
    web_socket_ConnectAsync,
    web_socket_SetRequestHeader,
    web_socket_add_Closed,
    web_socket_remove_Closed,
    web_socket_Close,
};

DEFINE_IINSPECTABLE(closable, IClosable, struct message_socket, IMessageWebSocket_iface)
static HRESULT WINAPI closable_Close(IClosable *iface)
{ return socket_close(impl_from_IClosable(iface), 1000, NULL); }

static const IClosableVtbl closable_vtbl =
{
    closable_QueryInterface,
    closable_AddRef,
    closable_Release,
    closable_GetIids,
    closable_GetRuntimeClassName,
    closable_GetTrustLevel,
    closable_Close,
};

DEFINE_IINSPECTABLE(output, IOutputStream, struct message_socket, IMessageWebSocket_iface)
static HRESULT send_callback(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    struct message_socket *impl = socket_impl((IMessageWebSocket *)invoker);
    IBuffer *buffer = (IBuffer *)param;
    IBufferByteAccess *access;
    UINT32 length;
    BYTE *bytes;
    HINTERNET handle;
    SocketMessageType type;
    HRESULT hr;
    DWORD error;
    if (!called_async) return STATUS_PENDING;
    hr = IBuffer_get_Length(buffer, &length);
    if (SUCCEEDED(hr) && SUCCEEDED(hr = IBuffer_QueryInterface(buffer, &IID_IBufferByteAccess, (void **)&access)))
    {
        if (SUCCEEDED(hr = IBufferByteAccess_Buffer(access, &bytes)))
        {
            EnterCriticalSection(&impl->cs);
            handle = impl->socket;
            LeaveCriticalSection(&impl->cs);
            type = InterlockedCompareExchange((LONG *)&impl->control->message_type, 0, 0);
            if (!handle) hr = RO_E_CLOSED;
            else if ((error = WinHttpWebSocketSend(handle, type == SocketMessageType_Utf8 ? WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE : WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, bytes, length))) hr = HRESULT_FROM_WIN32(error);
        }
        IBufferByteAccess_Release(access);
    }
    InterlockedExchange(&impl->sending, 0);
    result->vt = VT_UI4;
    result->ulVal = SUCCEEDED(hr) ? length : 0;
    if (SUCCEEDED(hr)) TRACE("WebSocket sent message, bytes %u.\n", length);
    return hr;
}
static HRESULT WINAPI output_WriteAsync(IOutputStream *iface, IBuffer *buffer, IAsyncOperationWithProgress_UINT32_UINT32 **value)
{
    struct message_socket *impl = impl_from_IOutputStream(iface);
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!buffer) return E_INVALIDARG;
    EnterCriticalSection(&impl->cs);
    if (impl->closed) hr = RO_E_CLOSED;
    else if (!impl->socket) hr = E_ILLEGAL_METHOD_CALL;
    else if (InterlockedCompareExchange(&impl->sending, 1, 0)) hr = E_ILLEGAL_METHOD_CALL;
    else
    {
        hr = async_operation_uint32_progress_create((IUnknown *)&impl->IMessageWebSocket_iface, (IUnknown *)buffer, send_callback, value);
        if (FAILED(hr)) InterlockedExchange(&impl->sending, 0);
    }
    LeaveCriticalSection(&impl->cs);
    return hr;
}
static HRESULT flush_callback(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{ result->vt = VT_BOOL; result->boolVal = VARIANT_TRUE; return S_OK; }
static HRESULT WINAPI output_FlushAsync(IOutputStream *iface, IAsyncOperation_boolean **value)
{
    struct message_socket *impl = impl_from_IOutputStream(iface);
    if (!value) return E_POINTER;
    *value = NULL;
    if (impl->closed) return RO_E_CLOSED;
    return async_operation_boolean_create((IUnknown *)&impl->IMessageWebSocket_iface, NULL, flush_callback, value);
}

static const IOutputStreamVtbl output_vtbl =
{
    output_QueryInterface,
    output_AddRef,
    output_Release,
    output_GetIids,
    output_GetRuntimeClassName,
    output_GetTrustLevel,
    output_WriteAsync,
    output_FlushAsync,
};

DEFINE_IINSPECTABLE(information, IWebSocketInformation, struct message_socket, IMessageWebSocket_iface)
static HRESULT WINAPI information_get_LocalAddress(IWebSocketInformation *iface, IHostName **value)
{ if (!value) return E_POINTER; *value = NULL; return E_NOTIMPL; }
static HRESULT WINAPI information_get_BandwidthStatistics(IWebSocketInformation *iface, BandwidthStatistics *value)
{ if (!value) return E_POINTER; memset(value, 0, sizeof(*value)); return E_NOTIMPL; }
static HRESULT WINAPI information_get_Protocol(IWebSocketInformation *iface, HSTRING *value)
{
    struct message_socket *impl = impl_from_IWebSocketInformation(iface);
    HRESULT hr;
    EnterCriticalSection(&impl->cs);
    hr = WindowsDuplicateString(impl->protocol, value);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static const IWebSocketInformationVtbl information_vtbl =
{
    information_QueryInterface,
    information_AddRef,
    information_Release,
    information_GetIids,
    information_GetRuntimeClassName,
    information_GetTrustLevel,
    information_get_LocalAddress,
    information_get_BandwidthStatistics,
    information_get_Protocol,
};

static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) &&
        !IsEqualGUID(iid, &IID_IActivationFactory) && !IsEqualGUID(iid, &IID_IAgileObject)) return E_NOINTERFACE;
    *out = iface; IActivationFactory_AddRef(iface); return S_OK;
}
static ULONG WINAPI factory_AddRef(IActivationFactory *iface) { return 2; }
static ULONG WINAPI factory_Release(IActivationFactory *iface) { return 1; }
static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **ids)
{ return metadata_iids(&IID_IActivationFactory, count, ids); }
static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *value)
{ return socket_GetRuntimeClassName(NULL, value); }
static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }
static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **value)
{
    struct message_socket *impl;
    struct socket_control *control;
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    if (!(control = calloc(1, sizeof(*control)))) { free(impl); return E_OUTOFMEMORY; }
    control->IMessageWebSocketControl_iface.lpVtbl = &control_vtbl;
    control->IWebSocketControl_iface.lpVtbl = &web_control_vtbl;
    control->ref = 1;
    control->max_message = 65536;
    control->outbound_buffer = 65536;
    if (FAILED(hr = vector_hstring_create(&control->protocols))) { free(control); free(impl); return hr; }
    impl->IMessageWebSocket_iface.lpVtbl = &socket_vtbl;
    impl->IWebSocket_iface.lpVtbl = &web_socket_vtbl;
    impl->IClosable_iface.lpVtbl = &closable_vtbl;
    impl->IOutputStream_iface.lpVtbl = &output_vtbl;
    impl->IWebSocketInformation_iface.lpVtbl = &information_vtbl;
    impl->ref = 1;
    impl->control = control;
    InitializeCriticalSection(&impl->cs);
    *value = (IInspectable *)&impl->IMessageWebSocket_iface;
    return S_OK;
}

static const IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    factory_ActivateInstance,
};
static IActivationFactory factory = { &factory_vtbl };
IActivationFactory *message_websocket_factory = &factory;
