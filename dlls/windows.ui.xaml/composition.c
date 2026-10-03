/* XAML composition frame notifications. SPDX-License-Identifier: LGPL-2.1-or-later */
#include "private.h"
#include "wine/winrt_events.h"
#define WIDL_using_Windows_UI_Xaml_Media
#include "windows.ui.xaml.h"
WINE_DEFAULT_DEBUG_CHANNEL(xaml);
struct composition_factory
{
    IActivationFactory IActivationFactory_iface;
    ICompositionTargetStatics ICompositionTargetStatics_iface;
};
static struct winrt_event rendering, lost;
static UINT_PTR timer;
static ULONGLONG started;
struct rendering_args { IRenderingEventArgs IRenderingEventArgs_iface; LONG ref; TimeSpan time; };
static HRESULT WINAPI args_QueryInterface(IRenderingEventArgs *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) && !IsEqualGUID(iid, &IID_IRenderingEventArgs)) return E_NOINTERFACE;
    *out = iface; IRenderingEventArgs_AddRef(iface); return S_OK;
}
static ULONG WINAPI args_AddRef(IRenderingEventArgs *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct rendering_args, IRenderingEventArgs_iface)->ref); }
static ULONG WINAPI args_Release(IRenderingEventArgs *iface)
{
    struct rendering_args *impl = CONTAINING_RECORD(iface, struct rendering_args, IRenderingEventArgs_iface);
    ULONG ref = InterlockedDecrement(&impl->ref); if (!ref) free(impl); return ref;
}
static HRESULT WINAPI args_GetIids(IRenderingEventArgs *iface, ULONG *count, IID **ids)
{ if (!count || !ids) return E_POINTER; *count = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI args_GetRuntimeClassName(IRenderingEventArgs *iface, HSTRING *out)
{ return WindowsCreateString(L"Windows.UI.Xaml.Media.RenderingEventArgs", 40, out); }
static HRESULT WINAPI args_GetTrustLevel(IRenderingEventArgs *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI args_get_RenderingTime(IRenderingEventArgs *iface, TimeSpan *out)
{ if (!out) return E_POINTER; *out = CONTAINING_RECORD(iface, struct rendering_args, IRenderingEventArgs_iface)->time; return S_OK; }
static const IRenderingEventArgsVtbl args_vtbl = {args_QueryInterface, args_AddRef, args_Release, args_GetIids,
    args_GetRuntimeClassName, args_GetTrustLevel, args_get_RenderingTime};
static void CALLBACK render_frame(HWND hwnd, UINT msg, UINT_PTR id, DWORD now)
{
    struct rendering_args *args = calloc(1, sizeof(*args));
    HRESULT hr;
    if (!args) return;
    args->IRenderingEventArgs_iface.lpVtbl = &args_vtbl; args->ref = 1;
    args->time.Duration = (GetTickCount64() - started) * 10000;
    hr = xaml_window_layout();
    if (SUCCEEDED(hr)) hr = winrt_event_notify(&rendering, NULL, &args->IRenderingEventArgs_iface);
    if (FAILED(hr)) WARN("Rendering callback failed %#lx\n", hr);
    IRenderingEventArgs_Release(&args->IRenderingEventArgs_iface);
}
void xaml_composition_clear(void)
{
    if (timer) KillTimer(NULL, timer);
    timer = 0; winrt_event_clear(&rendering); winrt_event_clear(&lost);
}
static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct composition_factory *impl = CONTAINING_RECORD(iface, struct composition_factory, IActivationFactory_iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IActivationFactory) || IsEqualGUID(iid, &IID_IAgileObject)) *out = iface;
    else if (IsEqualGUID(iid, &IID_ICompositionTargetStatics)) *out = &impl->ICompositionTargetStatics_iface;
    else return E_NOINTERFACE;
    return S_OK;
}
static ULONG WINAPI factory_AddRef(IActivationFactory *iface) { return 2; }
static ULONG WINAPI factory_Release(IActivationFactory *iface) { return 1; }
static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **ids)
{ if (!count || !ids) return E_POINTER; *count = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *out)
{ return WindowsCreateString(L"Windows.UI.Xaml.Media.CompositionTarget", 39, out); }
static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IActivationFactoryVtbl factory_vtbl = {factory_QueryInterface, factory_AddRef, factory_Release,
    factory_GetIids, factory_GetRuntimeClassName, factory_GetTrustLevel, factory_ActivateInstance};
DEFINE_IINSPECTABLE(statics, ICompositionTargetStatics, struct composition_factory, IActivationFactory_iface)
static HRESULT WINAPI statics_add_Rendering(ICompositionTargetStatics *iface, IEventHandler_IInspectable *handler, EventRegistrationToken *token)
{
    HRESULT hr = winrt_event_add(&rendering, handler, token);
    if (SUCCEEDED(hr) && !timer)
    {
        started = GetTickCount64(); timer = SetTimer(NULL, 0, 16, render_frame);
        if (!timer) { winrt_event_remove(&rendering, *token); hr = HRESULT_FROM_WIN32(GetLastError()); }
    }
    return hr;
}
static HRESULT WINAPI statics_remove_Rendering(ICompositionTargetStatics *iface, EventRegistrationToken token)
{ return winrt_event_remove(&rendering, token); }
static HRESULT WINAPI statics_add_SurfaceContentsLost(ICompositionTargetStatics *iface, IEventHandler_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&lost, handler, token); }
static HRESULT WINAPI statics_remove_SurfaceContentsLost(ICompositionTargetStatics *iface, EventRegistrationToken token)
{ return winrt_event_remove(&lost, token); }
static const ICompositionTargetStaticsVtbl statics_vtbl = {statics_QueryInterface, statics_AddRef, statics_Release,
    statics_GetIids, statics_GetRuntimeClassName, statics_GetTrustLevel, statics_add_Rendering, statics_remove_Rendering,
    statics_add_SurfaceContentsLost, statics_remove_SurfaceContentsLost};
static struct composition_factory factory = {{&factory_vtbl}, {&statics_vtbl}};
IActivationFactory *composition_factory = &factory.IActivationFactory_iface;
