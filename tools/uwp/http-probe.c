/* Real loopback requests through the registered FreeThreadedXMLHTTP60 class. */
#define COBJMACROS
#define CONST_VTABLE
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "initguid.h"
#include "msxml6.h"

static LONG failures;
#define CHECK(test) do { if (!(test)) { printf("FAIL line %d: %s\n", __LINE__, #test); InterlockedIncrement(&failures); } } while (0)
struct callback
{
    IXMLHTTPRequest2Callback iface;
    LONG ref;
    HANDLE done;
    HRESULT error;
    DWORD status;
    unsigned int redirects, headers, responses, errors, bytes, notifications;
    DWORD hash;
    char data[256];
    BOOL custom;
    ISequentialStream *retained;
};
static HRESULT WINAPI cb_qi(IXMLHTTPRequest2Callback *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IXMLHTTPRequest2Callback)) return E_NOINTERFACE;
    *out = iface; IXMLHTTPRequest2Callback_AddRef(iface); return S_OK;
}
static ULONG WINAPI cb_addref(IXMLHTTPRequest2Callback *iface)
{ return InterlockedIncrement(&((struct callback *)iface)->ref); }
static ULONG WINAPI cb_release(IXMLHTTPRequest2Callback *iface)
{ return InterlockedDecrement(&((struct callback *)iface)->ref); }
static HRESULT WINAPI cb_redirect(IXMLHTTPRequest2Callback *iface, IXMLHTTPRequest2 *request, const WCHAR *url)
{ (void)request; CHECK(url && *url); ++((struct callback *)iface)->redirects; return S_OK; }
static HRESULT WINAPI cb_headers(IXMLHTTPRequest2Callback *iface, IXMLHTTPRequest2 *request, DWORD status, const WCHAR *text)
{
    struct callback *callback = (void *)iface;
    WCHAR *value = NULL;
    CHECK(text != NULL);
    callback->status = status; ++callback->headers;
    CHECK(IXMLHTTPRequest2_GetResponseHeader(request, L"X-Probe", &value) == S_OK);
    if (value) { CHECK(!wcscmp(value, L"wine-http")); CoTaskMemFree(value); }
    value = NULL;
    CHECK(IXMLHTTPRequest2_GetAllResponseHeaders(request, &value) == S_OK && value);
    if (value) CHECK(wcsncmp(value, L"HTTP/", 5) && wcsstr(value, L"X-Probe: wine-http\r\n"));
    CoTaskMemFree(value);
    return S_OK;
}
static void read_response(struct callback *callback, ISequentialStream *stream)
{
    char buffer[4096];
    ULONG read;
    unsigned int i;
    HRESULT hr;
    if (callback->custom) return;
    do
    {
        hr = ISequentialStream_Read(stream, buffer, sizeof(buffer), &read);
        CHECK(SUCCEEDED(hr));
        for (i = 0; i < read; ++i) callback->hash = (callback->hash ^ (BYTE)buffer[i]) * 16777619u;
        if (callback->bytes < sizeof(callback->data) - 1)
            memcpy(callback->data + callback->bytes, buffer, min(read, sizeof(callback->data) - callback->bytes - 1));
        callback->bytes += read;
    } while (hr == S_OK && read);
}
static HRESULT WINAPI cb_data(IXMLHTTPRequest2Callback *iface, IXMLHTTPRequest2 *request, ISequentialStream *stream)
{
    struct callback *callback = (void *)iface;
    (void)request; ++callback->notifications; read_response(callback, stream); return S_OK;
}
static HRESULT WINAPI cb_response(IXMLHTTPRequest2Callback *iface, IXMLHTTPRequest2 *request, ISequentialStream *stream)
{
    struct callback *callback = (void *)iface;
    (void)request;
    read_response(callback, stream);
    callback->retained = stream; ISequentialStream_AddRef(stream);
    ++callback->responses; SetEvent(callback->done); return S_OK;
}
static HRESULT WINAPI cb_error(IXMLHTTPRequest2Callback *iface, IXMLHTTPRequest2 *request, HRESULT error)
{
    struct callback *callback = (void *)iface;
    (void)request;
    callback->error = error; ++callback->errors; SetEvent(callback->done); return S_OK;
}
static const IXMLHTTPRequest2CallbackVtbl callback_vtbl =
{cb_qi, cb_addref, cb_release, cb_redirect, cb_headers, cb_data, cb_response, cb_error};
static void run_request(unsigned int port, const WCHAR *path, BOOL post, BOOL custom, BOOL abort, BOOL timeout)
{
    struct callback callback = { .iface = {&callback_vtbl}, .ref = 1, .hash = 2166136261u };
    IXMLHTTPRequest2 *request = NULL;
    IStream *body = NULL, *output = NULL;
    WCHAR url[128];
    const char payload[] = "post-body-123";
    char actual[256] = {0};
    HRESULT hr;
    LARGE_INTEGER zero = {{0}};
    ULONG written;
    unsigned int i;
    callback.done = CreateEventW(NULL, FALSE, FALSE, NULL); callback.custom = custom;
    swprintf(url, 128, L"http://127.0.0.1:%u%ls", port, path);
    hr = CoCreateInstance(&CLSID_FreeThreadedXMLHTTP60, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLHTTPRequest2, (void **)&request);
    CHECK(hr == S_OK);
    if (FAILED(hr)) { CloseHandle(callback.done); return; }
    CHECK(IXMLHTTPRequest2_Send(request, NULL, 0) == E_UNEXPECTED);
    CHECK(IXMLHTTPRequest2_Open(request, post ? L"POST" : L"GET", url, &callback.iface, NULL, NULL, NULL, NULL) == S_OK);
    CHECK(IXMLHTTPRequest2_SetProperty(request, XHR_PROP_TIMEOUT, timeout ? 100 : 5000) == S_OK);
    CHECK(IXMLHTTPRequest2_SetProperty(request, XHR_PROP_NO_AUTH, XHR_AUTH_NONE) == S_OK);
    if (!wcsncmp(path, L"/large", 6))
        CHECK(IXMLHTTPRequest2_SetProperty(request, XHR_PROP_ONDATA_THRESHOLD, ~(ULONGLONG)0) == S_OK);
    CHECK(IXMLHTTPRequest2_SetRequestHeader(request, L"X-Request", L"wine-http") == S_OK);
    CHECK(IXMLHTTPRequest2_SetRequestHeader(request, L"X-Bad\r\nInjected", L"x") == E_INVALIDARG);
    if (post)
    {
        CHECK(CreateStreamOnHGlobal(NULL, TRUE, &body) == S_OK);
        CHECK(IStream_Write(body, payload, sizeof(payload)-1, &written) == S_OK && written == sizeof(payload)-1);
        CHECK(IStream_Seek(body, zero, STREAM_SEEK_SET, NULL) == S_OK);
    }
    if (custom)
    {
        CHECK(CreateStreamOnHGlobal(NULL, TRUE, &output) == S_OK);
        CHECK(IXMLHTTPRequest2_SetCustomResponseStream(request, (ISequentialStream *)output) == S_OK);
    }
    CHECK(IXMLHTTPRequest2_Send(request, (ISequentialStream *)body, post ? sizeof(payload)-1 : 0) == S_OK);
    CHECK(IXMLHTTPRequest2_Send(request, NULL, 0) == E_UNEXPECTED);
    if (abort) { Sleep(40); CHECK(IXMLHTTPRequest2_Abort(request) == S_OK); }
    CHECK(WaitForSingleObject(callback.done, 10000) == WAIT_OBJECT_0);
    for (i = 0; i < 1000 && callback.ref != 1; ++i) Sleep(1);
    CHECK(callback.ref == 1);
    if (abort || timeout)
    {
        CHECK(callback.errors == 1 && callback.responses == 0 && FAILED(callback.error));
        if (abort) CHECK(callback.error == E_ABORT);
    }
    else
    {
        CHECK(callback.errors == 0 && callback.responses == 1 && callback.headers == 1);
        CHECK(callback.status == (post ? 201 : !wcscmp(path, L"/missing") ? 404 : 200));
        CHECK(callback.redirects == (!wcscmp(path, L"/redirect") ? 1 : 0));
        if (custom)
        {
            CHECK(IStream_Seek(output, zero, STREAM_SEEK_SET, NULL) == S_OK);
            CHECK(SUCCEEDED(IStream_Read(output, actual, sizeof(actual)-1, &written)));
        }
        else strcpy(actual, callback.data);
        if (!wcsncmp(path, L"/large", 6))
        {
            DWORD expected_hash = 2166136261u;
            CHECK(callback.bytes == 16 * 8193 && callback.notifications == 0);
            for (i = 0; i < 16 * 8193; ++i) expected_hash = (expected_hash ^ "0123456789abcdef"[i % 16]) * 16777619u;
            CHECK(callback.hash == expected_hash);
        }
        else CHECK(!strcmp(actual, post ? payload : !wcscmp(path, L"/missing") ? "not-found" : "transport-ok"));
    }
    IXMLHTTPRequest2_Release(request);
    if (body) IStream_Release(body);
    if (output) IStream_Release(output);
    if (callback.retained) ISequentialStream_Release(callback.retained);
    CloseHandle(callback.done);
}
int main(int argc, char **argv)
{
    unsigned int port = argc > 1 ? atoi(argv[1]) : 18931;
    IXMLHTTPRequest *legacy = NULL;
    CHECK(CoInitializeEx(NULL, COINIT_MULTITHREADED) == S_OK);
    CHECK(CoCreateInstance(&CLSID_XMLHTTP60, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLHTTPRequest, (void **)&legacy) == S_OK);
    if (legacy) IXMLHTTPRequest_Release(legacy);
    run_request(port, L"/get", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/continue", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/continue-headers", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/redirect", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/gzip", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/chunked-gzip", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/chunked-gzip-delayed", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/get", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/deflate", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/raw-deflate", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/chunked-raw-deflate", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/large", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/large-gzip", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/large-deflate", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/large-raw-deflate", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/missing", FALSE, FALSE, FALSE, FALSE);
    run_request(port, L"/post", TRUE, FALSE, FALSE, FALSE);
    run_request(port, L"/get", FALSE, TRUE, FALSE, FALSE);
    run_request(port, L"/slow", FALSE, FALSE, TRUE, FALSE);
    run_request(port, L"/slow", FALSE, FALSE, FALSE, TRUE);
    CoUninitialize();
    printf("http: %s (%ld failures)\n", failures ? "FAIL" : "PASS", failures);
    return !!failures;
}
