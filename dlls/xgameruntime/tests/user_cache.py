#!/usr/bin/env python3
"""Test the real default-user cache gate with a mocked account-checking broker."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'GDKComponent/System/XUser.c').read_text()
start = source.index('static SRWLOCK msa_user_lock')
end = source.index('static void clear_msa_user', start)
prelude = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef int BOOL, BOOLEAN, HRESULT, SRWLOCK;
typedef uint32_t UINT32;
typedef uint64_t ULONGLONG;
typedef int64_t LONGLONG;
struct User { int IUser_iface; } object;
typedef struct User *XUserHandle;
#define SRWLOCK_INIT 0
#define S_OK 0
#define S_FALSE 1
#define SESSION_FILETIME_EPOCH 116444736000000000ULL
#define TRACE(...) ((void)0)
static char *msaAppId = "app";
static UINT32 titleId = 42;
static BOOLEAN fullTrust = 1;
static ULONGLONG ticks=100, now=SESSION_FILETIME_EPOCH+10000000000ULL;
static int broker_calls, refs, broker_status=S_OK;
static const char *broker_account="account";
static LONGLONG broker_expiry=1200;
static ULONGLONG GetTickCount64(void) { return ticks; }
static ULONGLONG session_time(void) { return now; }
static void AcquireSRWLockExclusive(SRWLOCK *p) { assert(!*p); *p=1; }
static void ReleaseSRWLockExclusive(SRWLOCK *p) { assert(*p); *p=0; }
static void IUser_AddRef(int *p) { assert(p==&object.IUser_iface); ++refs; }
static HRESULT xodus_session_cache(const char *op,const char *puid,LONGLONG expiry,
 const char *input,char **output,LONGLONG *returned)
{
 assert(!strcmp(op,"get") && !expiry && !input && !output);
 ++broker_calls; *returned=broker_expiry;
 return broker_account && !strcmp(puid,broker_account) ? broker_status : S_FALSE;
}
'''
tests = r'''
static void expect_miss(void) {
 XUserHandle user=(void*)1; int old_refs=refs;
 assert(reuse_msa_user(&user)==S_FALSE && !user && refs==old_refs && !msa_user_lock);
}
int main(void) {
 XUserHandle user;
 expect_miss(); assert(!broker_calls);
 msa_user=&object; msa_user_puid="account"; msa_user_app_id="app";
 msa_user_title=42; msa_user_trust=1; msa_user_loaded=100;
 msa_user_expires=now+2000000000ULL;
 assert(reuse_msa_user(&user)==S_OK && user==&object && refs==1 && broker_calls==1);
 broker_account="other"; expect_miss();
 broker_account=NULL; expect_miss();
 broker_account="account"; broker_status=-1; expect_miss();
 broker_status=S_FALSE; expect_miss();
 broker_status=S_OK; broker_expiry=1060; expect_miss();
 broker_expiry=1200; ticks=60100; expect_miss(); ticks=100;
 msa_user_expires=now+600000000; expect_miss(); msa_user_expires=now+2000000000;
 int calls=broker_calls;
 msaAppId="other"; expect_miss(); msaAppId="app";
 titleId=43; expect_miss(); titleId=42;
 fullTrust=0; expect_miss(); fullTrust=1;
 msa_user_app_id=NULL; expect_miss(); msa_user_app_id="app";
 assert(broker_calls==calls);
 assert(!msa_user_matches("other"));
 assert(reuse_msa_user(&user)==S_OK && refs==2);
 puts("PASS: reuse, account switch/logout, broker failure/miss, expiry, age and request identity");
}
'''
with tempfile.TemporaryDirectory(prefix='xuser-cache-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(prelude + source[start:end] + tests)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
