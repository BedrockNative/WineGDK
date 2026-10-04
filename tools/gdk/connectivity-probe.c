/* GDK connectivity registration regression tests. SPDX-License-Identifier: LGPL-2.1-or-later */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <xnetworking.h>
#include <xasyncprovider.h>

static IXNetworkingImpl2 *network;
static LONG checks, failures;
#define check(c) do { InterlockedIncrement(&checks); if (!(c)) { InterlockedIncrement(&failures); printf("FAIL %d: %s\n",__LINE__,#c); } } while(0)
struct observer { LONG calls; HANDLE entered, release; XTaskQueueRegistrationToken token; BOOL self_remove; };
static void WINAPI changed(void *context, const XNetworkingConnectivityHint *hint)
{
    struct observer *o = context;
    XNetworkingConnectivityHint current;
    check(hint && hint->networkInitialized && hint->connectivityLevel == XNetworkingConnectivityLevelHint_InternetAccess);
    check(SUCCEEDED(IXNetworkingImpl2_XNetworkingGetConnectivityHint(network, &current)) && current.networkInitialized);
    InterlockedIncrement(&o->calls);
    if (o->self_remove) check(IXNetworkingImpl2_XNetworkingUnregisterConnectivityHintChanged(network, o->token, TRUE));
    if (o->entered) SetEvent(o->entered);
    if (o->release) WaitForSingleObject(o->release, 10000);
}
static DWORD WINAPI unregister_thread(void *context)
{
    struct observer *o = context;
    return IXNetworkingImpl2_XNetworkingUnregisterConnectivityHintChanged(network, o->token, TRUE) ? 0 : 1;
}
int main(void)
{
    HMODULE mod = LoadLibraryA("xgameruntime.dll");
    HRESULT (WINAPI *query)(const GUID *, REFIID, void **);
    IXThreadingImpl *threading = NULL;
    XTaskQueueHandle queue = NULL;
    XNetworkingConnectivityHint hint;
    struct observer o[7] = {{0}};
    HANDLE thread;
    DWORD result;
    unsigned i;
    if (!mod || !(query = (void *)GetProcAddress(mod, "QueryApiImpl"))) return 2;
    check(SUCCEEDED(query(&CLSID_XNetworkingImpl, &IID_IXNetworkingImpl2, (void **)&network)));
    check(SUCCEEDED(query(&CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void **)&threading)));
    if (!network || !threading) return 2;
    check(SUCCEEDED(IXThreadingImpl_XTaskQueueCreate(threading, XTaskQueueDispatchMode_Manual, XTaskQueueDispatchMode_Manual, &queue)));
    if (!queue) return 2;
    check(SUCCEEDED(IXNetworkingImpl2_XNetworkingGetConnectivityHint(network, &hint)) && hint.networkInitialized);
    /* Fast startup must see an initialized host stack, even before Xbox login.
     * Register several observers without dispatching any notifications yet. */
    for (i = 0; i < 4; ++i)
        check(SUCCEEDED(IXNetworkingImpl2_XNetworkingRegisterConnectivityHintChanged(network, queue, o+i, changed, &o[i].token)));
    check(o[0].token.token != o[1].token.token && o[1].token.token != o[2].token.token);
    check(IXNetworkingImpl2_XNetworkingUnregisterConnectivityHintChanged(network, o[3].token, TRUE));
    o[2].self_remove = TRUE;
    /* A manual completion queue must not call observers before dispatch. */
    check(!o[0].calls && !o[1].calls && !o[2].calls && !o[3].calls);
    for (i = 0; i < 4; ++i) check(IXThreadingImpl_XTaskQueueDispatch(threading, queue, XTaskQueuePort_Completion, 1000));
    check(o[0].calls == 1 && o[1].calls == 1 && o[2].calls == 1 && !o[3].calls);
    check(IXNetworkingImpl2_XNetworkingUnregisterConnectivityHintChanged(network, o[0].token, TRUE));
    check(IXNetworkingImpl2_XNetworkingUnregisterConnectivityHintChanged(network, o[1].token, TRUE));
    /* Late registration is queued, and unregister cancels a pending callback. */
    check(SUCCEEDED(IXNetworkingImpl2_XNetworkingRegisterConnectivityHintChanged(network, queue, o+4, changed, &o[4].token)));
    check(!o[4].calls);
    check(IXThreadingImpl_XTaskQueueDispatch(threading, queue, XTaskQueuePort_Completion, 1000));
    check(o[4].calls == 1);
    check(IXNetworkingImpl2_XNetworkingUnregisterConnectivityHintChanged(network, o[4].token, TRUE));
    check(SUCCEEDED(IXNetworkingImpl2_XNetworkingRegisterConnectivityHintChanged(network, queue, o+5, changed, &o[5].token)));
    check(IXNetworkingImpl2_XNetworkingUnregisterConnectivityHintChanged(network, o[5].token, TRUE));
    check(IXThreadingImpl_XTaskQueueDispatch(threading, queue, XTaskQueuePort_Completion, 1000));
    check(!o[5].calls);
    /* wait=TRUE cannot return while another thread still uses the context. */
    o[6].entered = CreateEventW(NULL, TRUE, FALSE, NULL);
    o[6].release = CreateEventW(NULL, TRUE, FALSE, NULL);
    check(SUCCEEDED(IXNetworkingImpl2_XNetworkingRegisterConnectivityHintChanged(network, NULL, o+6, changed, &o[6].token)));
    check(WaitForSingleObject(o[6].entered, 3000) == WAIT_OBJECT_0);
    thread = CreateThread(NULL, 0, unregister_thread, o+6, 0, NULL);
    check(WaitForSingleObject(thread, 50) == WAIT_TIMEOUT);
    SetEvent(o[6].release);
    check(WaitForSingleObject(thread, 3000) == WAIT_OBJECT_0);
    GetExitCodeThread(thread, &result); check(!result);
    CloseHandle(thread); CloseHandle(o[6].entered); CloseHandle(o[6].release);
    IXThreadingImpl_XTaskQueueCloseHandle(threading, queue);
    IXThreadingImpl_Release(threading); IXNetworkingImpl2_Release(network);
    printf("%ld checks, %ld failures\n", checks, failures);
    return !!failures;
}
