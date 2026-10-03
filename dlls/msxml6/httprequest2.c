/* Asynchronous IXMLHTTPRequest2 transport using WinHTTP.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include "windef.h"
#include "winbase.h"
#include "initguid.h"
#include "objbase.h"
#include "msxml6.h"
#include "winhttp.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msxml);

struct http_request
{
    IXMLHTTPRequest3 iface;
    LONG ref;
    CRITICAL_SECTION cs;
    IUnknown *marshaler;
    IXMLHTTPRequest2Callback *callback;
    ISequentialStream *body, *custom;
    ULONGLONG length;
    HINTERNET session, connection, request;
    BOOL running, sent, aborted, headers_available;
    DWORD timeout, no_auth;
    ULONGLONG threshold;
};
static HRESULT WINAPI request_qi(IXMLHTTPRequest3 *iface, REFIID iid, void **out)
{
    struct http_request *request = (void *)iface;
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IMarshal)) return IUnknown_QueryInterface(request->marshaler, iid, out);
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IXMLHTTPRequest2) &&
        !IsEqualGUID(iid, &IID_IXMLHTTPRequest3)) return E_NOINTERFACE;
    *out = iface; IXMLHTTPRequest3_AddRef(iface); return S_OK;
}
static ULONG WINAPI request_addref(IXMLHTTPRequest3 *iface)
{ return InterlockedIncrement(&((struct http_request *)iface)->ref); }
static void close_handles(struct http_request *request)
{
    if (request->request) WinHttpCloseHandle(request->request);
    if (request->connection) WinHttpCloseHandle(request->connection);
    if (request->session) WinHttpCloseHandle(request->session);
    request->request = request->connection = request->session = NULL;
}
static ULONG WINAPI request_release(IXMLHTTPRequest3 *iface)
{
    struct http_request *request = (void *)iface;
    ULONG ref = InterlockedDecrement(&request->ref);
    if (!ref)
    {
        close_handles(request);
        if (request->callback) IXMLHTTPRequest2Callback_Release(request->callback);
        if (request->body) ISequentialStream_Release(request->body);
        if (request->custom) ISequentialStream_Release(request->custom);
        if (request->marshaler) IUnknown_Release(request->marshaler);
        DeleteCriticalSection(&request->cs);
        free(request);
    }
    return ref;
}
static HRESULT last_error(void)
{ DWORD error = GetLastError(); return HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE); }
static void CALLBACK request_status(HINTERNET handle, DWORD_PTR context, DWORD status, void *info, DWORD size)
{
    struct http_request *request = (void *)context;
    HRESULT hr;
    if (!request || status != WINHTTP_CALLBACK_STATUS_REDIRECT) return;
    hr = IXMLHTTPRequest2Callback_OnRedirect(request->callback, (IXMLHTTPRequest2 *)&request->iface, info);
    if (FAILED(hr)) IXMLHTTPRequest3_Abort(&request->iface);
}
static HRESULT WINAPI request_open(IXMLHTTPRequest3 *iface, const WCHAR *method, const WCHAR *url,
    IXMLHTTPRequest2Callback *callback, const WCHAR *user, const WCHAR *password,
    const WCHAR *proxy_user, const WCHAR *proxy_password)
{
    struct http_request *request = (void *)iface;
    URL_COMPONENTS parts = {sizeof(parts)};
    WCHAR *host = NULL, *path = NULL;
    DWORD disabled;
    HRESULT hr = S_OK;
    if (!method || !*method || !url || !callback) return E_INVALIDARG;
    if (user || password || proxy_user || proxy_password) return E_NOTIMPL;
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = parts.dwUserNameLength = parts.dwPasswordLength = -1;
    if (!WinHttpCrackUrl(url, 0, 0, &parts)) return last_error();
    if ((parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) ||
        parts.dwUserNameLength || parts.dwPasswordLength) return E_INVALIDARG;
    if (!(host = malloc((parts.dwHostNameLength + 1) * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    if (!(path = malloc((parts.dwUrlPathLength + parts.dwExtraInfoLength + 2) * sizeof(WCHAR))))
    { free(host); return E_OUTOFMEMORY; }
    memcpy(host, parts.lpszHostName, parts.dwHostNameLength * sizeof(WCHAR)); host[parts.dwHostNameLength] = 0;
    if (parts.dwUrlPathLength) memcpy(path, parts.lpszUrlPath, parts.dwUrlPathLength * sizeof(WCHAR));
    else path[parts.dwUrlPathLength++] = '/';
    if (parts.dwExtraInfoLength) memcpy(path + parts.dwUrlPathLength, parts.lpszExtraInfo, parts.dwExtraInfoLength * sizeof(WCHAR));
    path[parts.dwUrlPathLength + parts.dwExtraInfoLength] = 0;
    /* Deliberately do not log paths, query strings, headers, bodies or credentials. */
    TRACE("request %p, method %s, host %s\n", request, debugstr_w(method), debugstr_w(host));
    EnterCriticalSection(&request->cs);
    if (request->running) { hr = E_PENDING; goto done; }
    close_handles(request);
    if (request->callback) IXMLHTTPRequest2Callback_Release(request->callback);
    request->callback = NULL;
    if (request->body) ISequentialStream_Release(request->body);
    request->body = NULL;
    if (request->custom) ISequentialStream_Release(request->custom);
    request->custom = NULL;
    request->sent = request->aborted = request->headers_available = FALSE;
    if (!(request->session = WinHttpOpen(NULL, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0))) { hr = last_error(); goto done; }
    if (!WinHttpSetTimeouts(request->session, request->timeout, request->timeout, request->timeout, request->timeout))
    { hr = last_error(); goto done; }
    if (!(request->connection = WinHttpConnect(request->session, host, parts.nPort, 0))) { hr = last_error(); goto done; }
    if (!(request->request = WinHttpOpenRequest(request->connection, method, path, NULL, NULL, NULL,
        parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0))) { hr = last_error(); goto done; }
    if (!WinHttpSetOption(request->request, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT,
        &request->timeout, sizeof(request->timeout))) { hr = last_error(); goto done; }
    disabled = WINHTTP_DECOMPRESSION_FLAG_ALL;
    if (!WinHttpSetOption(request->request, WINHTTP_OPTION_DECOMPRESSION, &disabled, sizeof(disabled)))
    { hr = last_error(); goto done; }
    WinHttpSetStatusCallback(request->request, request_status, WINHTTP_CALLBACK_FLAG_REDIRECT, 0);
    request->callback = callback; IXMLHTTPRequest2Callback_AddRef(callback);
done:
    if (FAILED(hr) && hr != E_PENDING) close_handles(request);
    LeaveCriticalSection(&request->cs);
    free(host); free(path); return hr;
}
static HRESULT query_header(struct http_request *request, DWORD query, const WCHAR *name, WCHAR **out)
{
    DWORD size = 0;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!request->request || !request->headers_available) return E_PENDING;
    WinHttpQueryHeaders(request->request, query, name, NULL, &size, NULL);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return last_error();
    if (!(*out = CoTaskMemAlloc(size + sizeof(WCHAR)))) return E_OUTOFMEMORY;
    if (!WinHttpQueryHeaders(request->request, query, name, *out, &size, NULL))
    { hr = last_error(); CoTaskMemFree(*out); *out = NULL; return hr; }
    (*out)[size / sizeof(WCHAR)] = 0;
    return S_OK;
}
static BOOL is_aborted(struct http_request *request)
{
    BOOL aborted;
    EnterCriticalSection(&request->cs); aborted = request->aborted; LeaveCriticalSection(&request->cs);
    return aborted;
}
static DWORD WINAPI request_worker(void *arg)
{
    struct http_request *request = arg;
    IXMLHTTPRequest2 *iface = (IXMLHTTPRequest2 *)&request->iface;
    IXMLHTTPRequest2Callback *callback = request->callback;
    IStream *writer = NULL, *reader = NULL;
    ISequentialStream *response = request->custom;
    HINTERNET handle;
    BYTE buffer[32768];
    DWORD received, written, status, size = sizeof(status);
    ULONGLONG available = 0;
    const char *stage = "send";
    ULONGLONG remaining = request->length;
    WCHAR *status_text = NULL;
    HRESULT hr, init;
    BOOL ready;

    init = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(init)) { hr = init; goto finished; }
    EnterCriticalSection(&request->cs);
    handle = request->request;
    LeaveCriticalSection(&request->cs);
    if (is_aborted(request)) { hr = E_ABORT; goto finished; }
    if (!WinHttpSendRequest(handle, NULL, 0, NULL, 0, request->length, (DWORD_PTR)request))
    { hr = last_error(); goto finished; }
    stage = "upload";
    while (remaining)
    {
        hr = ISequentialStream_Read(request->body, buffer, min(sizeof(buffer), remaining), &received);
        if (FAILED(hr)) goto finished;
        if (!received || received > remaining) { hr = STG_E_READFAULT; goto finished; }
        if (!WinHttpWriteData(handle, buffer, received, &written)) { hr = last_error(); goto finished; }
        if (written != received) { hr = STG_E_WRITEFAULT; goto finished; }
        remaining -= received;
    }
    stage = "receive headers";
    if (!WinHttpReceiveResponse(handle, NULL)) { hr = last_error(); goto finished; }
    EnterCriticalSection(&request->cs);
    request->headers_available = TRUE;
    ready = WinHttpQueryHeaders(handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
    hr = ready ? query_header(request, WINHTTP_QUERY_STATUS_TEXT, NULL, &status_text) : last_error();
    LeaveCriticalSection(&request->cs);
    if (FAILED(hr)) goto finished;
    TRACE("request %p, HTTP status %lu\n", request, status);
    if (TRACE_ON(msxml))
    {
        WCHAR encoding[64];
        DWORD encoding_size = sizeof(encoding);
        if (WinHttpQueryHeaders(handle, WINHTTP_QUERY_CONTENT_ENCODING, NULL, encoding, &encoding_size, NULL))
            TRACE("request %p, response encoding %s\n", request, debugstr_w(encoding));
    }
    stage = "headers callback";
    if (FAILED(hr = IXMLHTTPRequest2Callback_OnHeadersAvailable(callback, iface, status, status_text))) goto finished;
    if (!response)
    {
        if (FAILED(hr = CreateStreamOnHGlobal(NULL, TRUE, &writer))) goto finished;
        if (FAILED(hr = IStream_Clone(writer, &reader))) goto finished;
        response = (ISequentialStream *)reader;
    }
    for (;;)
    {
        stage = "receive body";
        if (is_aborted(request)) { hr = E_ABORT; goto finished; }
        if (!WinHttpReadData(handle, buffer, sizeof(buffer), &received)) { hr = last_error(); goto finished; }
        if (!received) break;
        stage = "response stream write";
        hr = ISequentialStream_Write(writer ? (ISequentialStream *)writer : response, buffer, received, &written);
        if (FAILED(hr)) goto finished;
        if (written != received) { hr = STG_E_WRITEFAULT; goto finished; }
        available += received;
        if (available >= request->threshold)
        {
            stage = "data callback";
            if (FAILED(hr = IXMLHTTPRequest2Callback_OnDataAvailable(callback, iface, response))) goto finished;
            available = 0;
        }
    }
    hr = is_aborted(request) ? E_ABORT : S_OK;
finished:
    if (is_aborted(request)) hr = E_ABORT;
    if (FAILED(hr))
    {
        WARN("request %p failed during %s: %#lx\n", request, stage, hr);
        IXMLHTTPRequest2Callback_OnError(callback, iface, hr);
    }
    else
    {
        TRACE("request %p completed\n", request);
        IXMLHTTPRequest2Callback_OnResponseReceived(callback, iface, response);
    }
    CoTaskMemFree(status_text);
    if (writer) IStream_Release(writer);
    if (reader) IStream_Release(reader);
    EnterCriticalSection(&request->cs);
    request->callback = NULL;
    if (request->body) ISequentialStream_Release(request->body);
    request->body = NULL;
    if (request->custom) ISequentialStream_Release(request->custom);
    request->custom = NULL;
    request->running = FALSE;
    LeaveCriticalSection(&request->cs);
    IXMLHTTPRequest2Callback_Release(callback);
    if (SUCCEEDED(init)) CoUninitialize();
    IXMLHTTPRequest3_Release(&request->iface);
    return 0;
}
static HRESULT WINAPI request_send(IXMLHTTPRequest3 *iface, ISequentialStream *body, ULONGLONG length)
{
    struct http_request *request = (void *)iface;
    HANDLE thread;
    DWORD disabled;
    HRESULT hr = S_OK;
    if ((length && !body) || length > MAXDWORD) return E_INVALIDARG;
    EnterCriticalSection(&request->cs);
    if (!request->request || !request->callback || request->sent || request->running) { hr = E_UNEXPECTED; goto done; }
    /* Apply the final authentication policy once, so changing it before Send
     * does not leave WinHTTP's cumulative disable flags set. */
    disabled = request->no_auth == XHR_AUTH_NONE ? WINHTTP_DISABLE_AUTHENTICATION : 0;
    if (disabled && !WinHttpSetOption(request->request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)))
    { hr = last_error(); goto done; }
    request->length = length; request->body = body;
    if (body) ISequentialStream_AddRef(body);
    request->running = request->sent = TRUE;
    IXMLHTTPRequest3_AddRef(iface);
    if (!(thread = CreateThread(NULL, 0, request_worker, request, 0, NULL)))
    {
        hr = last_error(); request->running = request->sent = FALSE;
        if (body) ISequentialStream_Release(body);
        request->body = NULL; IXMLHTTPRequest3_Release(iface);
    }
    else CloseHandle(thread);
done:
    LeaveCriticalSection(&request->cs); return hr;
}
static HRESULT WINAPI request_abort(IXMLHTTPRequest3 *iface)
{
    struct http_request *request = (void *)iface;
    EnterCriticalSection(&request->cs);
    request->aborted = TRUE;
    if (request->request) { WinHttpCloseHandle(request->request); request->request = NULL; }
    LeaveCriticalSection(&request->cs);
    return S_OK;
}
static HRESULT WINAPI request_set_cookie(IXMLHTTPRequest3 *iface, const XHR_COOKIE *cookie, DWORD *state)
{ if (state) *state = 0; return E_NOTIMPL; }
static HRESULT WINAPI request_custom(IXMLHTTPRequest3 *iface, ISequentialStream *stream)
{
    struct http_request *request = (void *)iface;
    HRESULT hr = S_OK;
    EnterCriticalSection(&request->cs);
    if (request->sent) hr = E_UNEXPECTED;
    else
    {
        if (stream) ISequentialStream_AddRef(stream);
        if (request->custom) ISequentialStream_Release(request->custom);
        request->custom = stream;
    }
    LeaveCriticalSection(&request->cs); return hr;
}
static HRESULT WINAPI request_property(IXMLHTTPRequest3 *iface, XHR_PROPERTY property, ULONGLONG value)
{
    struct http_request *request = (void *)iface;
    HRESULT hr = S_OK;
    EnterCriticalSection(&request->cs);
    if (request->sent) { hr = E_UNEXPECTED; goto done; }
    switch (property)
    {
    case XHR_PROP_TIMEOUT:
        if (value > INT_MAX) { hr = E_INVALIDARG; break; }
        request->timeout = value;
        if (request->request && !WinHttpSetTimeouts(request->request, value, value, value, value)) hr = last_error();
        if (SUCCEEDED(hr) && request->request && !WinHttpSetOption(request->request,
            WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &request->timeout, sizeof(request->timeout))) hr = last_error();
        break;
    case XHR_PROP_ONDATA_THRESHOLD:
        request->threshold = max(1, value); break;
    case XHR_PROP_NO_AUTH:
        if (value > XHR_AUTH_PROXY) { hr = E_INVALIDARG; break; }
        if (value == XHR_AUTH_PROXY) { hr = E_NOTIMPL; break; }
        request->no_auth = value; break;
    case XHR_PROP_NO_CRED_PROMPT:
        if (value > XHR_CRED_PROMPT_PROXY) hr = E_INVALIDARG;
        break; /* WinHTTP does not display credential UI. */
    case XHR_PROP_NO_DEFAULT_HEADERS: case XHR_PROP_NO_CACHE: case XHR_PROP_EXTENDED_ERROR:
        if (value > 1) hr = E_INVALIDARG;
        break; /* No application defaults or cache; errors retain Win32 status. */
    case XHR_PROP_IGNORE_CERT_ERRORS: case XHR_PROP_REPORT_REDIRECT_STATUS:
        if (value) hr = E_NOTIMPL;
        break;
    default: hr = E_NOTIMPL; break;
    }
    if (FAILED(hr)) FIXME("property %u value %I64u: %#lx\n", property, value, hr);
done:
    LeaveCriticalSection(&request->cs); return hr;
}
static HRESULT WINAPI request_set_header(IXMLHTTPRequest3 *iface, const WCHAR *name, const WCHAR *value)
{
    struct http_request *request = (void *)iface;
    WCHAR *header;
    SIZE_T name_length, value_length;
    HRESULT hr = S_OK;
    if (!name || !*name || wcspbrk(name, L"\r\n:")) return E_INVALIDARG;
    if (value && wcspbrk(value, L"\r\n")) return E_INVALIDARG;
    name_length = wcslen(name); value_length = value ? wcslen(value) : 0;
    if (!(header = malloc((name_length + value_length + 4) * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    memcpy(header, name, name_length * sizeof(WCHAR)); header[name_length] = ':'; header[name_length + 1] = ' ';
    if (value_length) memcpy(header + name_length + 2, value, value_length * sizeof(WCHAR));
    header[name_length + value_length + 2] = 0;
    EnterCriticalSection(&request->cs);
    if (!request->request || request->sent) hr = E_UNEXPECTED;
    else if (!WinHttpAddRequestHeaders(request->request, header, -1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) hr = last_error();
    LeaveCriticalSection(&request->cs); free(header); return hr;
}
static HRESULT WINAPI request_all_headers(IXMLHTTPRequest3 *iface, WCHAR **out)
{
    struct http_request *request = (void *)iface;
    HRESULT hr;
    WCHAR *headers;
    EnterCriticalSection(&request->cs); hr = query_header(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, NULL, out);
    if (SUCCEEDED(hr) && (headers = wcsstr(*out, L"\r\n")))
    {
        /* WinHTTP includes the status line; XMLHTTP exposes header pairs only. */
        headers += 2;
        memmove(*out, headers, (wcslen(headers) + 1) * sizeof(WCHAR));
    }
    LeaveCriticalSection(&request->cs); return hr;
}
static HRESULT WINAPI request_get_cookie(IXMLHTTPRequest3 *iface, const WCHAR *url, const WCHAR *name,
                                        DWORD flags, ULONG *count, XHR_COOKIE **cookies)
{ if (count) *count = 0; if (cookies) *cookies = NULL; return E_NOTIMPL; }
static HRESULT WINAPI request_get_header(IXMLHTTPRequest3 *iface, const WCHAR *name, WCHAR **out)
{
    struct http_request *request = (void *)iface;
    HRESULT hr;
    if (!name) return E_INVALIDARG;
    EnterCriticalSection(&request->cs); hr = query_header(request, WINHTTP_QUERY_CUSTOM, name, out);
    LeaveCriticalSection(&request->cs); return hr;
}
static HRESULT WINAPI request_certificate(IXMLHTTPRequest3 *iface, DWORD count, const BYTE *hashes, const WCHAR *pin)
{ return E_NOTIMPL; }
static const IXMLHTTPRequest3Vtbl request_vtbl =
{
    request_qi, request_addref, request_release, request_open, request_send, request_abort,
    request_set_cookie, request_custom, request_property, request_set_header, request_all_headers,
    request_get_cookie, request_get_header, request_certificate
};
static HRESULT WINAPI factory_qi(IClassFactory *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IClassFactory)) return E_NOINTERFACE;
    *out = iface; return S_OK;
}
static ULONG WINAPI factory_addref(IClassFactory *iface) { return 2; }
static ULONG WINAPI factory_release(IClassFactory *iface) { return 1; }
static HRESULT WINAPI factory_create(IClassFactory *iface, IUnknown *outer, REFIID iid, void **out)
{
    struct http_request *request;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (!(request = calloc(1, sizeof(*request)))) return E_OUTOFMEMORY;
    request->iface.lpVtbl = &request_vtbl; request->ref = 1;
    request->timeout = 30000; request->threshold = 1;
    InitializeCriticalSection(&request->cs);
    hr = CoCreateFreeThreadedMarshaler((IUnknown *)&request->iface, &request->marshaler);
    if (SUCCEEDED(hr)) hr = IXMLHTTPRequest3_QueryInterface(&request->iface, iid, out);
    IXMLHTTPRequest3_Release(&request->iface); return hr;
}
static HRESULT WINAPI factory_lock(IClassFactory *iface, BOOL lock) { return S_OK; }
static IClassFactory factory = {& (const IClassFactoryVtbl){factory_qi, factory_addref, factory_release, factory_create, factory_lock}};
HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **out)
{
    static HMODULE msxml3;
    HMODULE module;
    HRESULT (WINAPI *get_class)(REFCLSID, REFIID, void **);
    if (IsEqualGUID(clsid, &CLSID_FreeThreadedXMLHTTP60)) return IClassFactory_QueryInterface(&factory, iid, out);
    if (!(module = msxml3))
    {
        if (!(module = LoadLibraryW(L"msxml3.dll"))) return last_error();
        if (InterlockedCompareExchangePointer((void **)&msxml3, module, NULL)) FreeLibrary(module);
        module = msxml3;
    }
    if (!(get_class = (void *)GetProcAddress(module, "DllGetClassObject"))) return CLASS_E_CLASSNOTAVAILABLE;
    return get_class(clsid, iid, out);
}
