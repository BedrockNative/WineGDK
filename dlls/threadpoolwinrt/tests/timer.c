/* ThreadPoolTimer tests. SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "winstring.h"
#include "initguid.h"
#include "roapi.h"
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_System_Threading
#include "windows.system.threading.h"
#include "wine/test.h"

struct handlers
{
    ITimerElapsedHandler elapsed;
    ITimerDestroyedHandler destroyed;
    LONG ref, count, completions;
    HANDLE done;
    BOOL cancel;
};
static HRESULT WINAPI elapsed_QueryInterface(ITimerElapsedHandler *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IAgileObject) &&
        !IsEqualGUID(iid, &IID_ITimerElapsedHandler)) return E_NOINTERFACE;
    *out = iface; ITimerElapsedHandler_AddRef(iface); return S_OK;
}
static ULONG WINAPI elapsed_AddRef(ITimerElapsedHandler *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct handlers, elapsed)->ref); }
static ULONG WINAPI elapsed_Release(ITimerElapsedHandler *iface)
{ return InterlockedDecrement(&CONTAINING_RECORD(iface, struct handlers, elapsed)->ref); }
static HRESULT WINAPI elapsed_Invoke(ITimerElapsedHandler *iface, IThreadPoolTimer *timer)
{
    struct handlers *impl = CONTAINING_RECORD(iface, struct handlers, elapsed);
    InterlockedIncrement(&impl->count);
    if (impl->cancel) IThreadPoolTimer_Cancel(timer);
    return S_OK;
}
static const ITimerElapsedHandlerVtbl elapsed_vtbl = {elapsed_QueryInterface, elapsed_AddRef, elapsed_Release, elapsed_Invoke};
static HRESULT WINAPI destroyed_QueryInterface(ITimerDestroyedHandler *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IAgileObject) &&
        !IsEqualGUID(iid, &IID_ITimerDestroyedHandler)) return E_NOINTERFACE;
    *out = iface; ITimerDestroyedHandler_AddRef(iface); return S_OK;
}
static ULONG WINAPI destroyed_AddRef(ITimerDestroyedHandler *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct handlers, destroyed)->ref); }
static ULONG WINAPI destroyed_Release(ITimerDestroyedHandler *iface)
{ return InterlockedDecrement(&CONTAINING_RECORD(iface, struct handlers, destroyed)->ref); }
static HRESULT WINAPI destroyed_Invoke(ITimerDestroyedHandler *iface, IThreadPoolTimer *timer)
{
    struct handlers *impl = CONTAINING_RECORD(iface, struct handlers, destroyed);
    InterlockedIncrement(&impl->completions);
    SetEvent(impl->done);
    return S_OK;
}
static const ITimerDestroyedHandlerVtbl destroyed_vtbl = {destroyed_QueryInterface, destroyed_AddRef, destroyed_Release, destroyed_Invoke};
static void test_timer(void)
{
    const WCHAR *name = L"Windows.System.Threading.ThreadPoolTimer";
    IThreadPoolTimerStatics *statics;
    IThreadPoolTimer *timer;
    struct handlers handlers = {{&elapsed_vtbl}, {&destroyed_vtbl}, 1};
    HSTRING classname;
    TimeSpan duration = {200000}, value;
    HRESULT hr;
    DWORD ret;
    unsigned int i;

    RoInitialize(RO_INIT_MULTITHREADED);
    WindowsCreateString(name, wcslen(name), &classname);
    hr = RoGetActivationFactory(classname, &IID_IThreadPoolTimerStatics, (void **)&statics);
    WindowsDeleteString(classname);
    if (FAILED(hr)) { win_skip("ThreadPoolTimer unavailable: %#lx.\n", hr); RoUninitialize(); return; }
    handlers.done = CreateEventW(NULL, TRUE, FALSE, NULL);
    hr = IThreadPoolTimerStatics_CreateTimerWithCompletion(statics, &handlers.elapsed, duration, &handlers.destroyed, &timer);
    ok(hr == S_OK, "CreateTimer returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        IThreadPoolTimer_get_Delay(timer, &value);
        ok(value.Duration == duration.Duration, "Unexpected delay %I64d.\n", value.Duration);
        IThreadPoolTimer_get_Period(timer, &value);
        ok(!value.Duration, "Unexpected period.\n");
        /* Releasing the returned reference must not cancel the callback. */
        IThreadPoolTimer_Release(timer);
        ret = WaitForSingleObject(handlers.done, 5000);
        ok(ret == WAIT_OBJECT_0, "Completion wait %lu.\n", ret);
        ok(handlers.count == 1 && handlers.completions == 1, "Counts %ld/%ld.\n", handlers.count, handlers.completions);
    }
    for (i = 0; i < 2; ++i)
    {
        ResetEvent(handlers.done); handlers.count = handlers.completions = 0;
        handlers.cancel = i;
        duration.Duration = i ? 200000 : 600000000;
        hr = IThreadPoolTimerStatics_CreatePeriodicTimerWithCompletion(statics, &handlers.elapsed, duration, &handlers.destroyed, &timer);
        ok(hr == S_OK, "CreatePeriodicTimer returned %#lx.\n", hr);
        if (FAILED(hr)) continue;
        IThreadPoolTimer_get_Period(timer, &value);
        ok(value.Duration == duration.Duration, "Unexpected period.\n");
        IThreadPoolTimer_get_Delay(timer, &value);
        ok(!value.Duration, "Unexpected delay.\n");
        if (!i) { IThreadPoolTimer_Cancel(timer); IThreadPoolTimer_Cancel(timer); }
        ret = WaitForSingleObject(handlers.done, 5000);
        ok(ret == WAIT_OBJECT_0, "Completion wait %lu.\n", ret);
        Sleep(50);
        ok(handlers.count == i && handlers.completions == 1, "Counts %ld/%ld, expected %u/1.\n", handlers.count, handlers.completions, i);
        IThreadPoolTimer_Release(timer);
    }
    /* Completion can precede the final delegate Release. */
    for (i = 0; i < 100 && handlers.ref != 1; ++i) Sleep(10);
    ok(handlers.ref == 1, "Leaked delegate references: %ld.\n", handlers.ref);
    CloseHandle(handlers.done);
    IThreadPoolTimerStatics_Release(statics);
    RoUninitialize();
}
START_TEST(timer) { test_timer(); }
