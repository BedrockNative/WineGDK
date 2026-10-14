#!/usr/bin/env python3
"""Exercise the real SISU cache admission code with mocked WinRT JSON objects."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'GDKComponent/System/XUser.c').read_text()
start = source.index('static void user_cache_sisu_authorization(')
end = source.index('static HRESULT user_request_title_token_sisu(', start)
prelude = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include <stdio.h>
typedef int IJsonObject, IJsonArray;
typedef const wchar_t *HSTRING;
typedef wchar_t WCHAR;
typedef uint64_t UINT64, ULONGLONG;
struct XUser { UINT64 xuid; };
#define TRUE 1
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
#define TRACE(...) ((void)0)
static IJsonObject object;
static const wchar_t *xid=L"42", *token=L"token", *hash=L"new-hash";
static int step, fail_at, stored, objects, strings;
static ULONGLONG expiry=1000000000;
static int fails(void) { return ++step==fail_at; }
static int get_json_object(IJsonObject *o,const WCHAR *key,IJsonObject **out) {
 (void)o; (void)key; if(fails())return -1; *out=&object; ++objects; return 0;
}
static int get_json_array(IJsonObject *o,const WCHAR *key,IJsonArray **out) {
 return get_json_object(o,key,out);
}
static int IJsonArray_GetObjectAt(IJsonArray *o,int index,IJsonObject **out) {
 (void)index; return get_json_object(o,L"item",out);
}
static int get_json_string(IJsonObject *o,const WCHAR *key,HSTRING *out) {
 (void)o; if(fails())return -1;
 *out=!wcscmp(key,L"Token")?token:!wcscmp(key,L"uhs")?hash:xid; ++strings; return 0;
}
static ULONGLONG xsts_expiry(IJsonObject *o) { (void)o; return expiry; }
static ULONGLONG session_time(void) { return 1; }
static size_t WindowsGetStringLen(HSTRING s) { return wcslen(s); }
static const WCHAR *WindowsGetStringRawBuffer(HSTRING s,void *n) { (void)n; return s; }
static void WindowsDeleteString(HSTRING s) { if(s)--strings; }
static void IJsonObject_Release(IJsonObject *o) { (void)o; --objects; }
static void IJsonArray_Release(IJsonArray *o) { (void)o; --objects; }
static int user_xsts_cache_store(struct XUser *u,const char *rp,int title,
 HSTRING t,HSTRING h,ULONGLONG e) {
 assert(u->xuid==42 && !strcmp(rp,"http://xboxlive.com") && title);
 assert(t==token && h==hash && e==expiry); ++stored; return 0;
}
'''
tests = r'''
static void check(int expected) {
 struct XUser user={42}; step=stored=0;
 user_cache_sisu_authorization(&user,&object);
 assert(stored==expected && !objects && !strings);
}
int main(void) {
 check(1);
 for(fail_at=1;fail_at<=7;++fail_at) check(0);
 fail_at=0;
 xid=L"43";check(0); xid=L"0";check(0);
 xid=L"42junk";check(0); xid=L"-42";check(0);
 xid=L"18446744073709551616";check(0); xid=L"";check(0); xid=L"42";
 token=L"";check(0);token=L"token";
 hash=L"";check(0);hash=L"new-hash";
 expiry=600000001;check(0);expiry=1000000000;check(1);
 puts("PASS: SISU identity, token/hash pairing, expiry, missing fields and cleanup");
}
'''
with tempfile.TemporaryDirectory(prefix='sisu-cache-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(prelude + source[start:end] + tests)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
