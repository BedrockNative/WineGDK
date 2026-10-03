#!/usr/bin/env python3
"""Run allocation/signature regressions against the implementation with native ASan.

Only Windows/network/threading dependencies are mocked. The tested functions and
structures are extracted verbatim from XUser.c and util.c, so no Xbox account or
working wineserver is required. Wide strings use the host wchar_t ABI; the Wine
build separately checks the Windows ABI. Run:
    python3 dlls/xgameruntime/tests/memory.py
Under ptrace-based sandboxes, prepend ASAN_OPTIONS=detect_leaks=0; HTTP allocation
ownership is also checked explicitly by the mocks.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
USER = (ROOT / "GDKComponent/System/XUser.c").read_text()
UTIL = (ROOT / "util.c").read_text()


def block(source, prefix):
    start = source.index(prefix)
    brace = source.index("{", start)
    depth = 0
    # Skip braces inside strings and comments without changing source offsets.
    for token in re.finditer(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',
                             source[brace:], re.S):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if not depth:
                end = brace + token.end()
                if source[end:end + 1] == ";":
                    end += 1
                return source[start:end] + "\n"
    raise ValueError(prefix)


PRELUDE = r'''
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
typedef int32_t HRESULT;
typedef int BOOL;
typedef unsigned char BOOLEAN, BYTE, UCHAR;
typedef uint32_t UINT32, DWORD, ULONG;
typedef uint64_t UINT64;
typedef size_t SIZE_T;
typedef wchar_t WCHAR;
typedef WCHAR *HSTRING;
typedef void *HINTERNET;
#define WINAPI
#define S_OK 0
#define E_POINTER ((HRESULT)0x80004003)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define E_OUTOFMEMORY ((HRESULT)0x8007000e)
#define E_FAIL ((HRESULT)0x80004005)
#define E_ABORT ((HRESULT)0x80004004)
#define E_NOTIMPL ((HRESULT)0x80004001)
#define FAILED(x) ((HRESULT)(x) < 0)
#define SUCCEEDED(x) (!FAILED(x))
#define TRUE 1
#define FALSE 0
#define TRACE(...) ((void)0)
#define FIXME(...) ((void)0)
#define MAXDWORD UINT32_MAX
#define ERROR_INSUFFICIENT_BUFFER 122
#define HRESULT_FROM_WIN32(x) ((HRESULT)(0x80070000 | (x)))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define CP_UTF8 65001
#define WC_ERR_INVALID_CHARS 128
#define INTERNET_SCHEME_HTTP 1
#define INTERNET_SCHEME_HTTPS 2
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 0
#define WINHTTP_NO_PROXY_NAME NULL
#define WINHTTP_NO_PROXY_BYPASS NULL
#define INTERNET_DEFAULT_HTTPS_PORT 443
#define WINHTTP_NO_REFERER NULL
#define WINHTTP_FLAG_SECURE 1
#define WINHTTP_QUERY_STATUS_CODE 1
#define WINHTTP_QUERY_FLAG_NUMBER 2
#define WINHTTP_HEADER_NAME_BY_INDEX NULL
#define WINHTTP_NO_HEADER_INDEX NULL
#define USER_AGENT L"test"
static DWORD GetLastError(void) { return 5; }
static unsigned int http_step, http_fail_step, read_count, realloc_count, realloc_fail, http_allocations;
static BOOL http_ok(void) { return ++http_step != http_fail_step; }
static HINTERNET WinHttpOpen(const WCHAR*a,DWORD b,const WCHAR*c,const WCHAR*d,DWORD e)
{ return http_ok() ? (void *)1 : NULL; }
static HINTERNET WinHttpConnect(HINTERNET a,const WCHAR*b,DWORD c,DWORD d)
{ return http_ok() ? (void *)2 : NULL; }
static HINTERNET WinHttpOpenRequest(HINTERNET a,const WCHAR*b,const WCHAR*c,const WCHAR*d,const WCHAR*e,const WCHAR**f,DWORD g)
{ return http_ok() ? (void *)3 : NULL; }
static BOOL WinHttpSendRequest(HINTERNET a,const WCHAR*b,DWORD c,void*d,DWORD e,DWORD f,SIZE_T g)
{ return http_ok(); }
static BOOL WinHttpReceiveResponse(HINTERNET a,void*b) { return http_ok(); }
static BOOL WinHttpQueryHeaders(HINTERNET a,DWORD b,const WCHAR*c,DWORD*d,DWORD*e,void*f)
{ *d = 200; return http_ok(); }
static BOOL WinHttpQueryDataAvailable(HINTERNET a,DWORD *size)
{ *size = read_count < 2 ? 4 : 0; return http_ok(); }
static BOOL WinHttpReadData(HINTERNET a,void*b,DWORD size,DWORD*read)
{ memset(b,'x',size); *read = size; ++read_count; return http_ok(); }
static BOOL WinHttpCloseHandle(HINTERNET a) { return TRUE; }
static void *mock_realloc(void *p, size_t size)
{
    void *result;
    if (++realloc_count == realloc_fail) return NULL;
    result = realloc(p,size);
    if (result && !p) ++http_allocations;
    return result;
}
static void http_free(void *p)
{ if (p) { assert(http_allocations); --http_allocations; } free(p); }
struct endpoint { char *host, *path, *relyingParty; };
typedef struct IUser { unsigned int ref; } IUser;
struct XUser {
    IUser IUser_iface;
    HSTRING userHash, xstsToken, refreshToken;
    char *xstsRelyingParty;
    int xstsLock;
    time_t oauth_expiry;
    struct endpoint *endpoints;
    UINT32 endpointsLen;
};
typedef struct XUser *XUserHandle;
typedef int IXUserImpl6, IXThreadingImpl;
typedef unsigned int XUserGetTokenAndSignatureOptions;
#define XUserGetTokenAndSignatureOptions_None 0
#define XUserGetTokenAndSignatureOptions_ForceRefresh 1
#define XUserGetTokenAndSignatureOptions_AllUsers 2
typedef struct { int unused; } XAsyncBlock;
typedef enum { XAsyncOp_Begin, XAsyncOp_DoWork, XAsyncOp_GetResult, XAsyncOp_Cancel, XAsyncOp_Cleanup } XAsyncOp;
typedef struct { XAsyncBlock *async; SIZE_T bufferSize; void *buffer, *context; } XAsyncProviderData;
typedef HRESULT XAsyncProvider(XAsyncOp,const XAsyncProviderData *);
typedef struct { SIZE_T tokenSize, signatureSize; const char *token,*signature; } XUserGetTokenAndSignatureData;
typedef struct { SIZE_T tokenCount, signatureCount; const WCHAR *token,*signature; } XUserGetTokenAndSignatureUtf16Data;
typedef struct { const char *name,*value; } XUserGetTokenAndSignatureHttpHeader;
typedef struct { const WCHAR *name,*value; } XUserGetTokenAndSignatureUtf16HttpHeader;
typedef struct {
    DWORD dwStructSize;
    char *lpszHostName; DWORD dwHostNameLength;
    char *lpszUrlPath; DWORD dwUrlPathLength;
    char *lpszExtraInfo; DWORD dwExtraInfoLength;
    int nScheme;
} URL_COMPONENTSA;
typedef struct { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;
static unsigned int oauth_refreshes, user_refreshes, xsts_refreshes, lock_depth, apartment_depth;
#define RO_INIT_MULTITHREADED 1
#define RPC_E_CHANGED_MODE ((HRESULT)0x80010106)
static HRESULT RoInitialize(int mode) { ++apartment_depth; return S_OK; }
static void RoUninitialize(void) { assert(apartment_depth-- == 1); }
static unsigned int begin_fail;
static HRESULT completed_hr;
static SIZE_T completed_size;
static XAsyncProviderData pending;
static XAsyncProvider *pending_provider;
static const int CLSID_XThreadingImpl = 0, IID_IXThreadingImpl = 0;
static ULONG IUser_AddRef(IUser *u) { return ++u->ref; }
static ULONG IUser_Release(IUser *u) { assert(u->ref); return --u->ref; }
static HRESULT IUser_RefreshOAuthToken(IUser *u) { ++oauth_refreshes; return S_OK; }
static HRESULT IUser_RequestUserToken(IUser *u) { ++user_refreshes; return S_OK; }
static HRESULT user_request_xsts_token(struct XUser *u,const char *party)
{ ++xsts_refreshes; return E_ABORT; }
static void AcquireSRWLockExclusive(int *lock) { assert(!lock_depth++); }
static void ReleaseSRWLockExclusive(int *lock) { assert(lock_depth-- == 1); }
static const WCHAR *WindowsGetStringRawBuffer(HSTRING s,UINT32 *len)
{ if (len) *len = s ? wcslen(s) : 0; return s ? s : L""; }
static int WideCharToMultiByte(UINT32 cp,DWORD flags,const WCHAR *in,int len,char *out,int cap,void*a,void*b)
{
    int size = len < 0 ? (int)wcslen(in) + 1 : len;
    if (!size) return 0;
    if (!out) return size;
    assert(cap >= size);
    for (int i = 0; i < size; ++i) { assert(in[i] < 128); out[i] = in[i]; }
    return size;
}
static BOOL InternetCrackUrlA(const char *url,DWORD a,DWORD b,URL_COMPONENTSA *u)
{
    const char *host, *path, *query;
    if (!url || strncmp(url,"https://",8)) return FALSE;
    host = url + 8;
    path = strchr(host,'/');
    if (!path) path = url + strlen(url);
    query = strchr(path,'?');
    u->lpszHostName = (char *)host; u->dwHostNameLength = path - host;
    u->lpszUrlPath = (char *)path; u->dwUrlPathLength = (query ? query : url + strlen(url)) - path;
    u->dwExtraInfoLength = query ? strlen(query) : 0;
    u->nScheme = INTERNET_SCHEME_HTTPS;
    return TRUE;
}
static void GetSystemTimeAsFileTime(FILETIME *t)
{ t->dwHighDateTime = 0x11223344; t->dwLowDateTime = 0x55667788; }
static char signed_data[20000];
static SIZE_T signed_size;
static HRESULT IUser_SignData(IUser *u,ULONG size,UCHAR *data,ULONG sig_size,UCHAR *sig)
{
    assert(size < sizeof(signed_data)); memcpy(signed_data,data,size); signed_size = size;
    assert(sig_size == 64); memset(sig,0xab,sig_size); return S_OK;
}
static HRESULT QueryApiImpl(const int *a,const int *b,void **out)
{ static IXThreadingImpl threading; *out = &threading; return S_OK; }
static ULONG IXThreadingImpl_Release(IXThreadingImpl *t) { return 1; }
static HRESULT IXThreadingImpl_XAsyncBegin(IXThreadingImpl *t,XAsyncBlock *a,void *ctx,const void *id,const char *name,XAsyncProvider *p)
{ if (begin_fail) return E_ABORT; pending = (XAsyncProviderData){a,0,NULL,ctx}; pending_provider = p; return S_OK; }
static HRESULT IXThreadingImpl_XAsyncSchedule(IXThreadingImpl *t,XAsyncBlock *a,UINT32 delay) { return S_OK; }
static void IXThreadingImpl_XAsyncComplete(IXThreadingImpl *t,XAsyncBlock *a,HRESULT hr,SIZE_T size)
{ completed_hr = hr; completed_size = size; }
'''

TESTS = r'''
static void test_http(void)
{
    UCHAR *buffer; SIZE_T size; HRESULT hr;
    for (unsigned int failure = 1; failure <= 10; ++failure)
    {
        http_step = read_count = realloc_count = 0; http_fail_step = failure; realloc_fail = 0;
        buffer = (void *)0xdeadbeef; size = 123;
        hr = http_request(L"GET",L"example.org",L"/",NULL,NULL,NULL,&buffer,&size);
        assert(FAILED(hr)); assert(!buffer); assert(!size); assert(!http_allocations);
    }
    http_step = read_count = realloc_count = 0; http_fail_step = 0; realloc_fail = 2;
    hr = http_request(L"GET",L"example.org",L"/",NULL,NULL,NULL,&buffer,&size);
    assert(hr == E_OUTOFMEMORY); assert(!buffer); assert(!size); assert(!http_allocations);
    http_step = read_count = realloc_count = realloc_fail = 0;
    hr = http_request(L"GET",L"example.org",L"/",NULL,NULL,NULL,&buffer,&size);
    assert(hr == S_OK); assert(size == 8); assert(!memcmp(buffer,"xxxxxxxx",8)); http_free(buffer);
    assert(http_request(NULL,L"host",L"/",NULL,NULL,NULL,&buffer,&size) == E_INVALIDARG);
    assert(!buffer && !size);
}
static void test_base64(void)
{
    const char *inputs[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
    const char *expected[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
    for (unsigned int i = 0; i < 7; ++i)
    {
        char out[20] = {0}; WCHAR wide[20] = {0};
        size_t len = strlen(expected[i]);
        assert(encode_base64(strlen(inputs[i]),(BYTE *)inputs[i],len,out,TRUE) == S_OK);
        assert(!strcmp(out,expected[i]));
        assert(encode_base64_utf16(strlen(inputs[i]),(BYTE *)inputs[i],len,wide,TRUE) == S_OK);
        for (size_t j = 0; j <= len; ++j) assert(wide[j] == expected[i][j]);
    }
    char out[4]; const BYTE input[] = {0xfb,0xff};
    assert(encode_base64_url(2,input,3,out,FALSE) == S_OK); assert(!memcmp(out,"-_8",3));
    assert(encode_base64(UINT32_MAX,input,sizeof(out),out,TRUE) == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
}
static void test_endpoints(void)
{
    free_endpoints(NULL,3); /* Cache transfer and failed allocation both retain the count. */
    struct endpoint *e = calloc(2,sizeof(*e));
    e[0].host = strdup("example.org"); e[0].path = strdup("/");
    free_endpoints(e,2);
    assert(endpoint_host_matches("a.playfabapi.com",16,"playfabapi.com"));
    assert(!endpoint_host_matches("evilplayfabapi.com",18,"playfabapi.com"));
    assert(!endpoint_host_matches(NULL,4,"test"));
}
static void test_signing(BOOL wide)
{
    struct XUser user = {.IUser_iface = {1}, .userHash = L"123", .xstsToken = L"token",
                        .xstsRelyingParty = "http://xboxlive.com"};
    XAsyncBlock async = {0};
    XUserGetTokenAndSignatureHttpHeader header = {"header", "value"};
    XUserGetTokenAndSignatureUtf16HttpHeader wide_header = {L"header", L"value"};
    const char body[] = "payload";
    HRESULT hr;
    if (wide) hr = x_user_XUserGetTokenAndSignatureUtf16Async(NULL,&user,0,L"pOsT",L"https://example.org/path?q=1",1,&wide_header,sizeof(body)-1,body,&async);
    else hr = x_user_XUserGetTokenAndSignatureAsync(NULL,&user,0,"pOsT","https://example.org/path?q=1",1,&header,sizeof(body)-1,body,&async);
    assert(hr == S_OK && user.IUser_iface.ref == 2);
    assert(pending_provider(XAsyncOp_DoWork,&pending) == S_OK);
    assert(completed_hr == S_OK && completed_size > 104 && !lock_depth && !apartment_depth);
    assert(!memcmp(signed_data+14,"POST\0/path?q=1\0XBL3.0 x=123;token\0value\0payload\0",sizeof("POST\0/path?q=1\0XBL3.0 x=123;token\0value\0payload") ));
    assert(signed_size == 14+sizeof("POST\0/path?q=1\0XBL3.0 x=123;token\0value\0payload"));
    pending.buffer = malloc(completed_size); pending.bufferSize = completed_size-1;
    assert(pending_provider(XAsyncOp_GetResult,&pending) == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
    pending.bufferSize = completed_size;
    assert(pending_provider(XAsyncOp_GetResult,&pending) == S_OK);
    assert(pending_provider(XAsyncOp_Cleanup,&pending) == S_OK);
    assert(user.IUser_iface.ref == 1);
    if (wide)
    {
        XUserGetTokenAndSignatureUtf16Data *r = pending.buffer;
        assert(r->token == (WCHAR *)(r+1)); assert(!wcscmp(r->token,L"XBL3.0 x=123;token"));
        assert(r->signature == r->token+r->tokenCount+1 && r->signatureCount == 104);
        assert(!r->signature[104]);
        assert((char *)(r->signature+105) == (char *)pending.buffer+completed_size);
    }
    else
    {
        XUserGetTokenAndSignatureData *r = pending.buffer;
        assert(r->token == (char *)(r+1)); assert(!strcmp(r->token,"XBL3.0 x=123;token"));
        assert(r->signature == r->token+r->tokenSize+1 && r->signatureSize == 104);
        assert(!r->signature[104]);
        assert(r->signature+105 == (char *)pending.buffer+completed_size);
    }
    free(pending.buffer);
    begin_fail = 1;
    hr = x_user_XUserGetTokenAndSignatureUtf16Async(NULL,&user,0,L"GET",L"https://example.org/",0,NULL,0,NULL,&async);
    assert(hr == E_ABORT && user.IUser_iface.ref == 1); begin_fail = 0;
    user.refreshToken = L"refresh";
    hr = x_user_XUserGetTokenAndSignatureAsync(NULL,&user,XUserGetTokenAndSignatureOptions_ForceRefresh,"GET","https://example.org/",0,NULL,0,NULL,&async);
    assert(hr == S_OK);
    assert(pending_provider(XAsyncOp_DoWork,&pending) == S_OK);
    assert(completed_hr == E_ABORT && !completed_size && !lock_depth && !apartment_depth);
    assert(oauth_refreshes && user_refreshes && xsts_refreshes);
    assert(pending_provider(XAsyncOp_Cleanup,&pending) == S_OK && user.IUser_iface.ref == 1);
}
static void test_large_body_and_expiry(void)
{
    struct XUser user = {.IUser_iface = {1}, .userHash = L"123", .xstsToken = L"token",
                        .xstsRelyingParty = "http://xboxlive.com"};
    XAsyncBlock async = {0};
    char body[9000], method[513];
    unsigned int refreshes = xsts_refreshes;
    memset(body,'b',sizeof(body)); memset(method,'p',sizeof(method)-1); method[512] = 0;
    assert(x_user_XUserGetTokenAndSignatureAsync(NULL,&user,0,method,"https://example.org/",0,NULL,sizeof(body),body,&async) == S_OK);
    assert(pending_provider(XAsyncOp_DoWork,&pending) == S_OK && completed_hr == S_OK);
    assert(signed_size == 512+1+strlen("XBL3.0 x=123;token")+8192+18);
    assert(signed_data[signed_size-2] == 'b' && signed_data[signed_size-1] == 0);
    assert(pending_provider(XAsyncOp_Cleanup,&pending) == S_OK && user.IUser_iface.ref == 1);
    user.oauth_expiry = time(NULL)-1; user.refreshToken = L"refresh";
    assert(x_user_XUserGetTokenAndSignatureAsync(NULL,&user,0,"GET","https://example.org/",0,NULL,0,NULL,&async) == S_OK);
    assert(pending_provider(XAsyncOp_DoWork,&pending) == S_OK && completed_hr == E_ABORT);
    assert(xsts_refreshes == refreshes+1 && !lock_depth && !apartment_depth);
    assert(pending_provider(XAsyncOp_Cleanup,&pending) == S_OK && user.IUser_iface.ref == 1);
}
int main(void)
{
    test_http(); test_base64(); test_endpoints(); test_signing(FALSE); test_signing(TRUE); test_large_body_and_expiry();
    puts("PASS: HTTP failure/allocation cleanup, base64, endpoint ownership, UTF8/UTF16 signing/result lifetime, ForceRefresh");
    return 0;
}
'''

parts = [PRELUDE, "#define realloc mock_realloc\n#define free http_free\n", block(UTIL, "HRESULT http_request("), "#undef realloc\n#undef free\n"]
parts.append(UTIL[UTIL.index("#define encode_base64_("):UTIL.index("#define get_json_(")])
for prefix in ("static void free_endpoints(", "static BOOL ascii_equal_i(",
               "static BOOL endpoint_host_matches(", "static const char *user_GetRelyingParty(",
               "struct XUserGetTokenAndSignatureContext\n", "static HRESULT WINAPI XUserGetTokenAndSignatureProvider(",
               "static HRESULT WINAPI x_user_XUserGetTokenAndSignatureAsync(",
               "static HRESULT WINAPI x_user_XUserGetTokenAndSignatureUtf16Async("):
    parts.append(block(USER, prefix))
parts.append(TESTS)
with tempfile.TemporaryDirectory(prefix="winegdk-memory-") as tmp:
    src, exe = Path(tmp) / "memory.c", Path(tmp) / "memory"
    src.write_text("\n".join(parts))
    command = shlex.split(os.environ.get("CC", "cc"))
    subprocess.run(command + ["-std=gnu11", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                              "-Wall", "-Wextra", "-Wno-unused-parameter", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
