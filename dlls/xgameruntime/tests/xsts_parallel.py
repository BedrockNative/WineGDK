#!/usr/bin/env python3
"""Concurrency tests for the production XSTS request coalescing code."""
from pathlib import Path
import subprocess
import tempfile
source = (Path(__file__).resolve().parents[1] / 'GDKComponent/System/XUser.c').read_text()
entry = source[source.index('struct xsts_entry\n'):source.index('#define XSTS_CACHE_SIZE')]
start = source.index('static void user_release_xsts_request(')
end = source.index('static HRESULT WINAPI user_RequestXstsToken', start)
prelude = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
typedef int BOOL, HRESULT;
typedef uint32_t UINT32;
typedef uint64_t ULONGLONG;
typedef char *HSTRING;
typedef pthread_mutex_t SRWLOCK;
typedef pthread_cond_t CONDITION_VARIABLE;
typedef struct { BOOL title; } URL_COMPONENTSA;
#define TRUE 1
#define FALSE 0
#define S_OK 0
#define E_OUTOFMEMORY -12
#define FAILED(h) ((h)<0)
#define SUCCEEDED(h) ((h)>=0)
#define INFINITE 0
#define HRESULT_FROM_WIN32(h) (-(h))
static void WindowsDeleteString(HSTRING h) { free(h); }
static HRESULT WindowsDuplicateString(HSTRING h,HSTRING *out) {
 *out=strdup(h); return *out?S_OK:E_OUTOFMEMORY;
}
static int GetLastError(void) { return errno; }
'''
mock = r'''
struct XUser {
 ULONGLONG xstsGeneration,xstsExpiry;
 char *xstsRelyingParty;
 HSTRING xstsToken,userHash;
 BOOL xstsWithTitle;
 SRWLOCK xstsLock;
 CONDITION_VARIABLE xstsChanged;
 struct xsts_request *xstsRequests;
};
static pthread_mutex_t transport=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static int calls,blocked,peak,waiters,release_transport,fail_transport;
static BOOL SleepConditionVariableSRW(CONDITION_VARIABLE *cv,SRWLOCK *lock,int t,int f) {
 (void)t;(void)f;
 pthread_mutex_lock(&transport);++waiters;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&transport);
 return !pthread_cond_wait(cv,lock);
}
static void WakeAllConditionVariable(CONDITION_VARIABLE *cv) { pthread_cond_broadcast(cv); }
static ULONGLONG session_time(void) { return 1; }
static BOOL url_wants_title_claims(const URL_COMPONENTSA *url,const char *rp) { (void)rp;return url->title; }
static BOOL user_xsts_cache_activate(struct XUser *u,const char *rp,BOOL title) {
 (void)u;(void)rp;(void)title;return FALSE; /* Force retained-result path after another RP publishes. */
}
static HRESULT user_activate_xsts(struct XUser *u,const struct xsts_entry *e) {
 free(u->xstsToken);free(u->userHash);free(u->xstsRelyingParty);
 u->xstsToken=strdup(e->token);u->userHash=strdup(e->userHash);u->xstsRelyingParty=strdup(e->relyingParty);
 u->xstsExpiry=e->expiry;u->xstsWithTitle=e->withTitle;return S_OK;
}
static HRESULT user_request_xsts_token(struct XUser *u,const char *rp,BOOL title,BOOL concurrent) {
 ULONGLONG generation=u->xstsGeneration;
 assert(concurrent);
 pthread_mutex_unlock(&u->xstsLock);
 pthread_mutex_lock(&transport);++calls;++blocked;if(blocked>peak)peak=blocked;
 pthread_cond_broadcast(&changed);
 while(!release_transport)pthread_cond_wait(&changed,&transport);
 --blocked;pthread_mutex_unlock(&transport);
 pthread_mutex_lock(&u->xstsLock);
 if(generation!=u->xstsGeneration)return -1237;
 if(fail_transport)return -5;
 struct xsts_entry result={1000000000,(char*)rp,title,(char*)rp,title?"title-hash":"user-hash"};
 return user_activate_xsts(u,&result);
}
'''
test = r'''
struct job { struct XUser *user; const char *rp; BOOL title; HRESULT result; };
static void *run(void *ptr) {
 struct job *j=ptr;URL_COMPONENTSA url={j->title};
 pthread_mutex_lock(&j->user->xstsLock);
 j->result=user_ensure_xsts_for_url(j->user,&url,j->rp);
 if(SUCCEEDED(j->result)) {
  assert(!strcmp(j->user->xstsToken,j->rp));
  assert(!strcmp(j->user->userHash,j->title?"title-hash":"user-hash"));
 }
 pthread_mutex_unlock(&j->user->xstsLock);return NULL;
}
static void wait_for(int n,int w) {
 pthread_mutex_lock(&transport);
 while(calls<n||waiters<w)pthread_cond_wait(&changed,&transport);
 pthread_mutex_unlock(&transport);
}
static void scenario(int fail,int refresh,int different_title) {
 struct XUser u={.xstsLock=PTHREAD_MUTEX_INITIALIZER,.xstsChanged=PTHREAD_COND_INITIALIZER};
 calls=blocked=peak=waiters=release_transport=0;fail_transport=fail;
 struct job a={&u,"playfab",TRUE,0},dup=a,b={&u,different_title?"playfab":"realms",!different_title,0};
 pthread_t ta,td,tb;
 assert(!pthread_create(&ta,NULL,run,&a));wait_for(1,0);
 assert(!pthread_create(&td,NULL,run,&dup));wait_for(1,1);
 assert(!pthread_create(&tb,NULL,run,&b));wait_for(2,1);
 assert(peak==2); /* Different RP or title context reached transport concurrently. */
 if(refresh) { pthread_mutex_lock(&u.xstsLock);++u.xstsGeneration;pthread_mutex_unlock(&u.xstsLock); }
 pthread_mutex_lock(&transport);release_transport=1;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&transport);
 pthread_join(ta,NULL);pthread_join(td,NULL);pthread_join(tb,NULL);
 assert(a.result==(fail?-5:0)&&dup.result==a.result&&b.result==a.result);
 assert(!u.xstsRequests);
 if(!refresh)assert(calls==2);else assert(calls>=4);
 free(u.xstsToken);free(u.userHash);free(u.xstsRelyingParty);
 pthread_cond_destroy(&u.xstsChanged);pthread_mutex_destroy(&u.xstsLock);
}
int main(void) {
 for(int i=0;i<40;++i) { scenario(0,0,0);scenario(1,0,0);scenario(0,1,0);scenario(0,0,1); }
 puts("PASS: overlapping RPs, duplicate coalescing, shared failures, title separation and refresh races");
}
'''
with tempfile.TemporaryDirectory(prefix='xsts-parallel-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(prelude + entry + mock + source[start:end] + test)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pthread', '-fsanitize=address,undefined', '-g', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True, timeout=30)
