#define COBJMACROS
#define CONST_VTABLE
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Networking_Sockets
#define WIDL_using_Windows_Storage_Streams
#include <stdio.h>
#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "initguid.h"
#include "roapi.h"
#include "winstring.h"
#include "windows.networking.sockets.h"
#include "windows.storage.streams.h"

static LONG failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); InterlockedIncrement(&failures); } } while (0)
static HSTRING string(const WCHAR *text)
{ HSTRING value = NULL; CHECK(WindowsCreateString(text, wcslen(text), &value) == S_OK); return value; }
static HRESULT factory(const WCHAR *name, REFIID iid, void **value)
{ HSTRING text = string(name); HRESULT hr = RoGetActivationFactory(text, iid, value); WindowsDeleteString(text); return hr; }
static HRESULT wait_async(IUnknown *operation)
{
    IAsyncInfo *info = NULL;
    AsyncStatus status = Started;
    DWORD start = GetTickCount();
    HRESULT hr;
    if (!operation) return E_POINTER;
    if (FAILED(hr = IUnknown_QueryInterface(operation, &IID_IAsyncInfo, (void **)&info))) return hr;
    do {
        hr = IAsyncInfo_get_Status(info, &status);
        if (FAILED(hr) || status != Started) break;
        Sleep(5);
    } while (GetTickCount() - start < 10000);
    if (SUCCEEDED(hr))
    {
        if (status == Started) hr = HRESULT_FROM_WIN32(WAIT_TIMEOUT);
        else if (status == Canceled) hr = E_ABORT;
        else IAsyncInfo_get_ErrorCode(info, &hr);
    }
    IAsyncInfo_Release(info);
    return hr;
}
struct receiver
{
    ITypedEventHandler_MessageWebSocket_MessageWebSocketMessageReceivedEventArgs iface;
    LONG ref;
    HANDLE event;
    BYTE bytes[131072];
    UINT32 length;
    SocketMessageType type;
};
static struct receiver *receiver_impl(ITypedEventHandler_MessageWebSocket_MessageWebSocketMessageReceivedEventArgs *iface)
{ return CONTAINING_RECORD(iface, struct receiver, iface); }
static HRESULT WINAPI receiver_QueryInterface(ITypedEventHandler_MessageWebSocket_MessageWebSocketMessageReceivedEventArgs *iface, REFIID iid, void **value)
{
    *value = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IAgileObject) &&
        !IsEqualGUID(iid, &IID_ITypedEventHandler_MessageWebSocket_MessageWebSocketMessageReceivedEventArgs)) return E_NOINTERFACE;
    *value = iface; ITypedEventHandler_MessageWebSocket_MessageWebSocketMessageReceivedEventArgs_AddRef(iface); return S_OK;
}
static ULONG WINAPI receiver_AddRef(ITypedEventHandler_MessageWebSocket_MessageWebSocketMessageReceivedEventArgs *iface)
{ return InterlockedIncrement(&receiver_impl(iface)->ref); }
static ULONG WINAPI receiver_Release(ITypedEventHandler_MessageWebSocket_MessageWebSocketMessageReceivedEventArgs *iface)
{ return InterlockedDecrement(&receiver_impl(iface)->ref); }
static HRESULT WINAPI receiver_Invoke(ITypedEventHandler_MessageWebSocket_MessageWebSocketMessageReceivedEventArgs *iface,
    IMessageWebSocket *sender, IMessageWebSocketMessageReceivedEventArgs *args)
{
    struct receiver *impl = receiver_impl(iface);
    IDataReader *reader = NULL;
    UINT32 remaining;
    (void)sender;
    CHECK(IMessageWebSocketMessageReceivedEventArgs_get_MessageType(args, &impl->type) == S_OK);
    CHECK(IMessageWebSocketMessageReceivedEventArgs_GetDataReader(args, &reader) == S_OK);
    if (reader)
    {
        CHECK(IDataReader_get_UnconsumedBufferLength(reader, &impl->length) == S_OK);
        CHECK(impl->length <= sizeof(impl->bytes));
        if (impl->length <= sizeof(impl->bytes)) CHECK(IDataReader_ReadBytes(reader, impl->length, impl->bytes) == S_OK);
        CHECK(IDataReader_get_UnconsumedBufferLength(reader, &remaining) == S_OK && remaining == 0);
        CHECK(IDataReader_ReadByte(reader, impl->bytes) == E_BOUNDS);
        IDataReader_Release(reader);
    }
    SetEvent(impl->event);
    return S_OK;
}
static const ITypedEventHandler_MessageWebSocket_MessageWebSocketMessageReceivedEventArgsVtbl receiver_vtbl =
{ receiver_QueryInterface, receiver_AddRef, receiver_Release, receiver_Invoke };
struct closed_receiver
{
    ITypedEventHandler_IWebSocket_WebSocketClosedEventArgs iface;
    LONG ref;
    HANDLE event;
    LONG calls;
};
static struct closed_receiver *closed_impl(ITypedEventHandler_IWebSocket_WebSocketClosedEventArgs *iface)
{ return CONTAINING_RECORD(iface, struct closed_receiver, iface); }
static HRESULT WINAPI closed_QueryInterface(ITypedEventHandler_IWebSocket_WebSocketClosedEventArgs *iface, REFIID iid, void **value)
{
    *value = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IAgileObject) && !IsEqualGUID(iid, &IID_ITypedEventHandler_IWebSocket_WebSocketClosedEventArgs)) return E_NOINTERFACE;
    *value = iface; ITypedEventHandler_IWebSocket_WebSocketClosedEventArgs_AddRef(iface); return S_OK;
}
static ULONG WINAPI closed_AddRef(ITypedEventHandler_IWebSocket_WebSocketClosedEventArgs *iface)
{ return InterlockedIncrement(&closed_impl(iface)->ref); }
static ULONG WINAPI closed_Release(ITypedEventHandler_IWebSocket_WebSocketClosedEventArgs *iface)
{ return InterlockedDecrement(&closed_impl(iface)->ref); }
static HRESULT WINAPI closed_Invoke(ITypedEventHandler_IWebSocket_WebSocketClosedEventArgs *iface, IWebSocket *sender, IWebSocketClosedEventArgs *args)
{
    struct closed_receiver *impl = closed_impl(iface);
    UINT16 code;
    (void)sender;
    CHECK(IWebSocketClosedEventArgs_get_Code(args, &code) == S_OK);
    CHECK(code >= 1000);
    InterlockedIncrement(&impl->calls);
    SetEvent(impl->event);
    return S_OK;
}
static const ITypedEventHandler_IWebSocket_WebSocketClosedEventArgsVtbl closed_vtbl =
{ closed_QueryInterface, closed_AddRef, closed_Release, closed_Invoke };

static void test_socket(const WCHAR *path, BOOL reject, BOOL cancel)
{
    IActivationFactory *activation = NULL;
    IMessageWebSocket *message = NULL;
    IWebSocket *socket = NULL;
    IMessageWebSocketControl *control = NULL;
    IWebSocketControl *webcontrol = NULL;
    IWebSocketInformation *information = NULL;
    IVector_HSTRING *protocols = NULL;
    IUriRuntimeClassFactory *uri_factory = NULL;
    IUriRuntimeClass *uri = NULL;
    IAsyncAction *connect = NULL;
    IOutputStream *output = NULL;
    IDataWriterFactory *writer_factory = NULL;
    IDataWriter *writer = NULL;
    IAsyncOperation_UINT32 *store = NULL;
    IAsyncInfo *info = NULL;
    IClosable *closable = NULL;
    HSTRING str, name, value;
    EventRegistrationToken message_token, close_token;
    struct receiver receiver = { { &receiver_vtbl }, 1 };
    struct closed_receiver closed = { { &closed_vtbl }, 1 };
    WCHAR url[256];
    BYTE payload[70000];
    UINT32 written, i, j;
    HRESULT hr;
    receiver.event = CreateEventW(NULL, TRUE, FALSE, NULL);
    closed.event = CreateEventW(NULL, TRUE, FALSE, NULL);
    CHECK(factory(L"Windows.Networking.Sockets.MessageWebSocket", &IID_IActivationFactory, (void **)&activation) == S_OK);
    if (!activation) goto done;
    CHECK(IActivationFactory_ActivateInstance(activation, (IInspectable **)&message) == S_OK);
    IActivationFactory_Release(activation);
    if (!message) goto done;
    CHECK(IMessageWebSocket_QueryInterface(message, &IID_IWebSocket, (void **)&socket) == S_OK);
    CHECK(IMessageWebSocket_get_Control(message, &control) == S_OK);
    CHECK(IMessageWebSocketControl_put_MaxMessageSize(control, sizeof(payload) + 1) == S_OK);
    CHECK(IMessageWebSocketControl_put_MessageType(control, (SocketMessageType)99) == E_INVALIDARG);
    CHECK(IMessageWebSocketControl_QueryInterface(control, &IID_IWebSocketControl, (void **)&webcontrol) == S_OK);
    CHECK(IWebSocketControl_get_SupportedProtocols(webcontrol, &protocols) == S_OK);
    str = string(L"wine-probe"); CHECK(IVector_HSTRING_Append(protocols, str) == S_OK); WindowsDeleteString(str);
    IVector_HSTRING_Release(protocols);
    name = string(L"X-Probe"); value = string(L"test\r\nInjected: yes");
    CHECK(IWebSocket_SetRequestHeader(socket, name, value) == E_INVALIDARG);
    WindowsDeleteString(value); value = string(L"test");
    CHECK(IWebSocket_SetRequestHeader(socket, name, value) == S_OK);
    WindowsDeleteString(name); WindowsDeleteString(value);
    CHECK(IMessageWebSocket_add_MessageReceived(message, &receiver.iface, &message_token) == S_OK);
    CHECK(IWebSocket_add_Closed(socket, &closed.iface, &close_token) == S_OK);
    CHECK(factory(L"Windows.Foundation.Uri", &IID_IUriRuntimeClassFactory, (void **)&uri_factory) == S_OK);
    swprintf(url, ARRAYSIZE(url), L"ws://127.0.0.1:18766/%ls", path);
    str = string(url); CHECK(IUriRuntimeClassFactory_CreateUri(uri_factory, str, &uri) == S_OK); WindowsDeleteString(str);
    IUriRuntimeClassFactory_Release(uri_factory);
    CHECK(IWebSocket_ConnectAsync(socket, uri, &connect) == S_OK);
    IUriRuntimeClass_Release(uri);
    if (!connect) goto cleanup;
    if (cancel)
    {
        Sleep(100);
        CHECK(IAsyncAction_QueryInterface(connect, &IID_IAsyncInfo, (void **)&info) == S_OK);
        CHECK(IAsyncInfo_Cancel(info) == S_OK);
        IAsyncInfo_Release(info);
        CHECK(wait_async((IUnknown *)connect) == E_ABORT);
        CHECK(IAsyncAction_GetResults(connect) == E_ABORT);
        IAsyncAction_Release(connect);
        goto cleanup;
    }
    hr = wait_async((IUnknown *)connect);
    CHECK(hr == (reject ? (HRESULT)0x80190193 : S_OK));
    CHECK(IAsyncAction_GetResults(connect) == hr);
    IAsyncAction_Release(connect);
    if (reject || FAILED(hr)) goto cleanup;
    CHECK(IMessageWebSocket_get_Information(message, &information) == S_OK);
    CHECK(IWebSocketInformation_get_Protocol(information, &str) == S_OK);
    CHECK(!wcscmp(WindowsGetStringRawBuffer(str, NULL), L"wine-probe"));
    WindowsDeleteString(str); IWebSocketInformation_Release(information);
    CHECK(IWebSocket_get_OutputStream(socket, &output) == S_OK);
    CHECK(factory(L"Windows.Storage.Streams.DataWriter", &IID_IDataWriterFactory, (void **)&writer_factory) == S_OK);
    if (writer_factory)
    {
        CHECK(IDataWriterFactory_CreateDataWriter(writer_factory, output, &writer) == S_OK);
        IDataWriterFactory_Release(writer_factory);
    }
    IOutputStream_Release(output);
    if (writer) for (j = 0; j < 3; ++j)
    {
        UINT32 length = j == 0 ? 19 : j == 1 ? sizeof(payload) : 0;
        for (i = 0; i < length; ++i) payload[i] = j ? (BYTE)i : (BYTE)('a' + i % 26);
        ResetEvent(receiver.event);
        CHECK(IMessageWebSocketControl_put_MessageType(control, j ? SocketMessageType_Binary : SocketMessageType_Utf8) == S_OK);
        CHECK(IDataWriter_WriteBytes(writer, length, payload) == S_OK);
        CHECK(IDataWriter_StoreAsync(writer, &store) == S_OK);
        if (store)
        {
            CHECK(wait_async((IUnknown *)store) == S_OK);
            CHECK(IAsyncOperation_UINT32_GetResults(store, &written) == S_OK && written == length);
            IAsyncOperation_UINT32_Release(store); store = NULL;
        }
        CHECK(WaitForSingleObject(receiver.event, 5000) == WAIT_OBJECT_0);
        CHECK(receiver.length == length && !memcmp(receiver.bytes, payload, length));
        CHECK(receiver.type == (j ? SocketMessageType_Binary : SocketMessageType_Utf8));
    }
    if (writer) IDataWriter_Release(writer);
    CHECK(IWebSocket_Close(socket, 1000, NULL) == S_OK);
    CHECK(WaitForSingleObject(closed.event, 2000) == WAIT_OBJECT_0);
    CHECK(closed.calls == 1);
cleanup:
    CHECK(IMessageWebSocket_QueryInterface(message, &IID_IClosable, (void **)&closable) == S_OK);
    IClosable_Close(closable); IClosable_Release(closable);
    IMessageWebSocket_remove_MessageReceived(message, message_token);
    IWebSocket_remove_Closed(socket, close_token);
    IWebSocketControl_Release(webcontrol);
    IMessageWebSocketControl_Release(control);
    IWebSocket_Release(socket);
    IMessageWebSocket_Release(message);
done:
    CloseHandle(receiver.event); CloseHandle(closed.event);
}
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
    test_socket(L"echo", FALSE, FALSE);
    test_socket(L"ignore-close", FALSE, FALSE);
    test_socket(L"reject", TRUE, FALSE);
    test_socket(L"stall", FALSE, TRUE);
    RoUninitialize();
    printf("WebSocket probe: %ld failures\n", failures);
    return !!failures;
}
