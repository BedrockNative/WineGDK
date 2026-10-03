/* Windows.System.Threading.ThreadPoolTimer
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <stdarg.h>
#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "roapi.h"
#include "winstring.h"
#include "activation.h"
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_System_Threading
#include "windows.system.threading.h"

struct timer
{
    IThreadPoolTimer IThreadPoolTimer_iface;
    LONG ref;
    TimeSpan period, delay;
    HANDLE stop, timer;
    ITimerElapsedHandler *elapsed;
    ITimerDestroyedHandler *destroyed;
};
static HRESULT WINAPI timer_QueryInterface(IThreadPoolTimer *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) &&
        !IsEqualGUID(iid, &IID_IAgileObject) && !IsEqualGUID(iid, &IID_IThreadPoolTimer)) return E_NOINTERFACE;
    *out = iface; IThreadPoolTimer_AddRef(iface); return S_OK;
}
static ULONG WINAPI timer_AddRef(IThreadPoolTimer *iface)
{ return InterlockedIncrement(&((struct timer *)iface)->ref); }
static ULONG WINAPI timer_Release(IThreadPoolTimer *iface)
{
    struct timer *impl = (struct timer *)iface;
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref)
    {
        if (impl->elapsed) ITimerElapsedHandler_Release(impl->elapsed);
        if (impl->destroyed) ITimerDestroyedHandler_Release(impl->destroyed);
        if (impl->stop) CloseHandle(impl->stop);
        if (impl->timer) CloseHandle(impl->timer);
        free(impl);
    }
    return ref;
}
static HRESULT WINAPI timer_GetIids(IThreadPoolTimer *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0; *iids = CoTaskMemAlloc(sizeof(**iids));
    if (!*iids) return E_OUTOFMEMORY;
    **iids = IID_IThreadPoolTimer; *count = 1; return S_OK;
}
static HRESULT WINAPI timer_GetRuntimeClassName(IThreadPoolTimer *iface, HSTRING *out)
{
    const WCHAR *name = RuntimeClass_Windows_System_Threading_ThreadPoolTimer;
    return WindowsCreateString(name, wcslen(name), out);
}
static HRESULT WINAPI timer_GetTrustLevel(IThreadPoolTimer *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI timer_get_Period(IThreadPoolTimer *iface, TimeSpan *out)
{ if (!out) return E_POINTER; *out = ((struct timer *)iface)->period; return S_OK; }
static HRESULT WINAPI timer_get_Delay(IThreadPoolTimer *iface, TimeSpan *out)
{ if (!out) return E_POINTER; *out = ((struct timer *)iface)->delay; return S_OK; }
static HRESULT WINAPI timer_Cancel(IThreadPoolTimer *iface)
{ SetEvent(((struct timer *)iface)->stop); return S_OK; }
static const IThreadPoolTimerVtbl timer_vtbl = {timer_QueryInterface, timer_AddRef, timer_Release,
    timer_GetIids, timer_GetRuntimeClassName, timer_GetTrustLevel, timer_get_Period, timer_get_Delay, timer_Cancel};

static DWORD WINAPI timer_thread(void *arg)
{
    struct timer *impl = arg;
    HANDLE handles[2] = {impl->stop, impl->timer};
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    while (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0 + 1)
    {
        ITimerElapsedHandler_Invoke(impl->elapsed, &impl->IThreadPoolTimer_iface);
        if (!impl->period.Duration) break;
    }
    CancelWaitableTimer(impl->timer);
    if (impl->destroyed) ITimerDestroyedHandler_Invoke(impl->destroyed, &impl->IThreadPoolTimer_iface);
    /* Release delegates when work finishes even if the caller keeps the timer. */
    ITimerElapsedHandler_Release(impl->elapsed); impl->elapsed = NULL;
    if (impl->destroyed) { ITimerDestroyedHandler_Release(impl->destroyed); impl->destroyed = NULL; }
    IThreadPoolTimer_Release(&impl->IThreadPoolTimer_iface);
    if (SUCCEEDED(hr)) RoUninitialize();
    return 0;
}
static HRESULT create_timer(ITimerElapsedHandler *handler, TimeSpan duration, BOOL periodic,
        ITimerDestroyedHandler *destroyed, IThreadPoolTimer **out)
{
    struct timer *impl;
    HANDLE thread;
    LARGE_INTEGER due;
    HRESULT hr;
    LONGLONG period;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!handler || duration.Duration < 0 || (periodic && !duration.Duration)) return E_INVALIDARG;
    period = periodic ? duration.Duration / 10000 + !!(duration.Duration % 10000) : 0;
    if (period > MAXLONG) return E_INVALIDARG;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IThreadPoolTimer_iface.lpVtbl = &timer_vtbl; impl->ref = 1;
    if (periodic) impl->period = duration; else impl->delay = duration;
    impl->elapsed = handler; ITimerElapsedHandler_AddRef(handler);
    if (destroyed) { impl->destroyed = destroyed; ITimerDestroyedHandler_AddRef(destroyed); }
    impl->stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    impl->timer = CreateWaitableTimerW(NULL, FALSE, NULL);
    due.QuadPart = duration.Duration ? -duration.Duration : -1;
    if (!impl->stop || !impl->timer || !SetWaitableTimer(impl->timer, &due, period, NULL, NULL, FALSE))
    { hr = HRESULT_FROM_WIN32(GetLastError()); timer_Release(&impl->IThreadPoolTimer_iface); return hr; }
    timer_AddRef(&impl->IThreadPoolTimer_iface);
    if (!(thread = CreateThread(NULL, 0, timer_thread, impl, 0, NULL)))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        timer_Release(&impl->IThreadPoolTimer_iface); timer_Release(&impl->IThreadPoolTimer_iface); return hr;
    }
    CloseHandle(thread); *out = &impl->IThreadPoolTimer_iface; return S_OK;
}
struct timer_factory
{
    IActivationFactory IActivationFactory_iface;
    IThreadPoolTimerStatics IThreadPoolTimerStatics_iface;
};
static struct timer_factory factory;
static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IActivationFactory)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IThreadPoolTimerStatics)) *out = &factory.IThreadPoolTimerStatics_iface;
    else return E_NOINTERFACE;
    IUnknown_AddRef((IUnknown *)*out); return S_OK;
}
static ULONG WINAPI factory_AddRef(IActivationFactory *iface) { return 2; }
static ULONG WINAPI factory_Release(IActivationFactory *iface) { return 1; }
static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0; *iids = CoTaskMemAlloc(sizeof(**iids));
    if (!*iids) return E_OUTOFMEMORY;
    **iids = IID_IThreadPoolTimerStatics; *count = 1; return S_OK;
}
static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *out)
{ return timer_GetRuntimeClassName(NULL, out); }
static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *out)
{ return timer_GetTrustLevel(NULL, out); }
static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IActivationFactoryVtbl factory_vtbl = {factory_QueryInterface, factory_AddRef, factory_Release,
    factory_GetIids, factory_GetRuntimeClassName, factory_GetTrustLevel, factory_ActivateInstance};
static HRESULT WINAPI statics_QueryInterface(IThreadPoolTimerStatics *iface, REFIID iid, void **out)
{ return factory_QueryInterface(&factory.IActivationFactory_iface, iid, out); }
static ULONG WINAPI statics_AddRef(IThreadPoolTimerStatics *iface) { return 2; }
static ULONG WINAPI statics_Release(IThreadPoolTimerStatics *iface) { return 1; }
static HRESULT WINAPI statics_GetIids(IThreadPoolTimerStatics *iface, ULONG *count, IID **iids)
{ return factory_GetIids(&factory.IActivationFactory_iface, count, iids); }
static HRESULT WINAPI statics_GetRuntimeClassName(IThreadPoolTimerStatics *iface, HSTRING *out)
{ return timer_GetRuntimeClassName(NULL, out); }
static HRESULT WINAPI statics_GetTrustLevel(IThreadPoolTimerStatics *iface, TrustLevel *out)
{ return timer_GetTrustLevel(NULL, out); }
static HRESULT WINAPI statics_CreatePeriodicTimer(IThreadPoolTimerStatics *iface, ITimerElapsedHandler *handler,
        TimeSpan period, IThreadPoolTimer **out)
{ return create_timer(handler, period, TRUE, NULL, out); }
static HRESULT WINAPI statics_CreateTimer(IThreadPoolTimerStatics *iface, ITimerElapsedHandler *handler,
        TimeSpan delay, IThreadPoolTimer **out)
{ return create_timer(handler, delay, FALSE, NULL, out); }
static HRESULT WINAPI statics_CreatePeriodicTimerWithCompletion(IThreadPoolTimerStatics *iface, ITimerElapsedHandler *handler,
        TimeSpan period, ITimerDestroyedHandler *destroyed, IThreadPoolTimer **out)
{ return create_timer(handler, period, TRUE, destroyed, out); }
static HRESULT WINAPI statics_CreateTimerWithCompletion(IThreadPoolTimerStatics *iface, ITimerElapsedHandler *handler,
        TimeSpan delay, ITimerDestroyedHandler *destroyed, IThreadPoolTimer **out)
{ return create_timer(handler, delay, FALSE, destroyed, out); }
static const IThreadPoolTimerStaticsVtbl statics_vtbl = {statics_QueryInterface, statics_AddRef, statics_Release,
    statics_GetIids, statics_GetRuntimeClassName, statics_GetTrustLevel, statics_CreatePeriodicTimer, statics_CreateTimer,
    statics_CreatePeriodicTimerWithCompletion, statics_CreateTimerWithCompletion};
static struct timer_factory factory = {{&factory_vtbl}, {&statics_vtbl}};
IActivationFactory *timer_factory = &factory.IActivationFactory_iface;
