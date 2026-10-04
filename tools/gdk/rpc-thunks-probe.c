/* Exercise exported COM delegation thunks across register/stack arguments.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef HRESULT (WINAPI *method_t)(void *,ULONG,double,void *,ULONGLONG,ULONG);
static void *expected_this;
static unsigned int checks, failures;
#define check(c) do { ++checks; if (!(c)) { ++failures; printf("FAIL %u: %s\n",__LINE__,#c); } } while (0)

static HRESULT WINAPI method(void *self,ULONG a,double b,void *c,ULONGLONG d,ULONG e)
{
    check(self == expected_this);
    check(a == 0x12345678 && b == 1.25 && c == &expected_this);
    check(d == 0x123456789abcdef0ULL && e == 0x87654321);
    return S_FALSE;
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll"), rpc = LoadLibraryA("rpcrt4.dll");
    void *vtable[33] = {0}, *object = vtable, *proxy[5] = {0};
    char name[80];
    unsigned int i;
    method_t thunk;
    FARPROC stubless;
    if (!combase || !rpc) return 2;
    expected_this = &object;
    proxy[4] = expected_this;
    for (i = 3; i <= 32; ++i)
    {
        snprintf(name,sizeof(name),"ObjectStublessClient%u",i);
        stubless = GetProcAddress(combase,name);
        check(stubless && stubless == GetProcAddress(rpc,name));
        snprintf(name,sizeof(name),"NdrProxyForwardingFunction%u",i);
        thunk = (void *)GetProcAddress(combase,name);
        check(thunk && (void *)thunk == (void *)GetProcAddress(rpc,name));
        if (!thunk) continue;
        vtable[i] = method;
        check(thunk(proxy,0x12345678,1.25,&expected_this,0x123456789abcdef0ULL,0x87654321) == S_FALSE);
        vtable[i] = NULL;
    }
    printf("%u checks, %u failures\n",checks,failures);
    return !!failures;
}
