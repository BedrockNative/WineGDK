/* WinRT Windows.UI.ViewManagement.ApplicationView implementation
 *
 * Copyright 2025 Zhiyi Zhang for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "private.h"

WINE_DEFAULT_DEBUG_CHANNEL(twinapi);

/* Native single-view ApplicationView backing. */
#include "winuser.h"
#include "commctrl.h"
#include "corewindow.h"
#include "roapi.h"
#include "wine/winrt_events.h"

extern HRESULT WINAPI __wine_core_window_set_fullscreen(ICoreWindow *, BOOL);
extern HRESULT WINAPI __wine_core_window_get_fullscreen(ICoreWindow *, BOOL *);

struct application_view
{
    IApplicationView IApplicationView_iface;
    IApplicationView2 IApplicationView2_iface;
    IApplicationView3 IApplicationView3_iface;
    LONG ref;
    HWND hwnd;
    DWORD thread;
    ICoreWindow *core;
    Size minimum;
    ApplicationViewBoundsMode bounds_mode;
    FullScreenSystemOverlayMode overlay;
    struct winrt_event consolidated, bounds_changed;
};
static struct application_view *impl_from_IApplicationView(IApplicationView *iface)
{ return CONTAINING_RECORD(iface,struct application_view,IApplicationView_iface); }
static HRESULT view_check(struct application_view *impl)
{ return impl->thread!=GetCurrentThreadId() ? RPC_E_WRONG_THREAD : !impl->hwnd ? RO_E_CLOSED : S_OK; }
static HRESULT WINAPI view_QueryInterface(IApplicationView *iface, REFIID iid, void **out)
{
    struct application_view *impl=impl_from_IApplicationView(iface);
    if (!out) return E_POINTER;
    *out=NULL;
    if (IsEqualGUID(iid,&IID_IApplicationView2)) *out=&impl->IApplicationView2_iface;
    else if (IsEqualGUID(iid,&IID_IApplicationView3)) *out=&impl->IApplicationView3_iface;
    else if (IsEqualGUID(iid,&IID_IUnknown) || IsEqualGUID(iid,&IID_IInspectable) || IsEqualGUID(iid,&IID_IApplicationView)) *out=iface;
    else { FIXME("ApplicationView interface %s not implemented.\n",debugstr_guid(iid)); return E_NOINTERFACE; }
    IApplicationView_AddRef(iface); return S_OK;
}
static ULONG WINAPI view_AddRef(IApplicationView *iface)
{ return InterlockedIncrement(&impl_from_IApplicationView(iface)->ref); }
static ULONG WINAPI view_Release(IApplicationView *iface)
{
    struct application_view *impl=impl_from_IApplicationView(iface);
    ULONG ref=InterlockedDecrement(&impl->ref);
    if (!ref)
    {
        winrt_event_clear(&impl->consolidated); winrt_event_clear(&impl->bounds_changed);
        ICoreWindow_Release(impl->core); free(impl);
    }
    return ref;
}
static HRESULT WINAPI view_GetIids(IApplicationView *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count=0; if (!(*iids=CoTaskMemAlloc(3*sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0]=IID_IApplicationView; (*iids)[1]=IID_IApplicationView2; (*iids)[2]=IID_IApplicationView3; *count=3; return S_OK;
}
static HRESULT WINAPI view_GetRuntimeClassName(IApplicationView *iface, HSTRING *name)
{ const WCHAR *str=RuntimeClass_Windows_UI_ViewManagement_ApplicationView; return WindowsCreateString(str,wcslen(str),name); }
static HRESULT WINAPI view_GetTrustLevel(IApplicationView *iface, TrustLevel *trust)
{ if (!trust) return E_POINTER; *trust=BaseTrust; return S_OK; }
static HRESULT WINAPI view_get_Orientation(IApplicationView *iface, ApplicationViewOrientation *value)
{
    struct application_view *impl=impl_from_IApplicationView(iface);
    RECT rect; HRESULT hr=view_check(impl);
    if (!value) return E_POINTER;
    if (FAILED(hr)) return hr;
    GetClientRect(impl->hwnd,&rect);
    *value=rect.right>=rect.bottom ? ApplicationViewOrientation_Landscape : ApplicationViewOrientation_Portrait; return S_OK;
}
static HRESULT view_edge(IApplicationView *iface, boolean *value, BOOL right)
{
    struct application_view *impl=impl_from_IApplicationView(iface);
    MONITORINFO monitor={sizeof(monitor)};
    RECT rect; HRESULT hr=view_check(impl);
    if (!value) return E_POINTER;
    if (FAILED(hr)) return hr;
    GetWindowRect(impl->hwnd,&rect); GetMonitorInfoW(MonitorFromWindow(impl->hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
    *value=right ? rect.right>=monitor.rcMonitor.right : rect.left<=monitor.rcMonitor.left; return S_OK;
}
static HRESULT WINAPI view_get_AdjacentToLeftDisplayEdge(IApplicationView *iface, boolean *value) { return view_edge(iface,value,FALSE); }
static HRESULT WINAPI view_get_AdjacentToRightDisplayEdge(IApplicationView *iface, boolean *value) { return view_edge(iface,value,TRUE); }
static HRESULT WINAPI view_get_IsFullScreen(IApplicationView *iface, boolean *value)
{
    BOOL fullscreen;
    HRESULT hr;
    if (!value) return E_POINTER;
    hr = __wine_core_window_get_fullscreen(impl_from_IApplicationView(iface)->core, &fullscreen);
    if (SUCCEEDED(hr)) *value = fullscreen;
    return hr;
}
static HRESULT WINAPI view_get_IsOnLockScreen(IApplicationView *iface, boolean *value)
{ if (!value) return E_POINTER; *value=FALSE; return S_OK; }
static HRESULT WINAPI view_get_IsScreenCaptureEnabled(IApplicationView *iface, boolean *value)
{
    struct application_view *impl=impl_from_IApplicationView(iface);
    DWORD affinity; HRESULT hr=view_check(impl);
    if (!value) return E_POINTER;
    if (FAILED(hr)) return hr;
    if (!GetWindowDisplayAffinity(impl->hwnd,&affinity)) return HRESULT_FROM_WIN32(GetLastError());
    *value=affinity==WDA_NONE; return S_OK;
}
static HRESULT WINAPI view_put_IsScreenCaptureEnabled(IApplicationView *iface, boolean value)
{
    struct application_view *impl=impl_from_IApplicationView(iface);
    HRESULT hr=view_check(impl); if (FAILED(hr)) return hr;
    return SetWindowDisplayAffinity(impl->hwnd,value ? WDA_NONE : WDA_MONITOR) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}
static HRESULT WINAPI view_put_Title(IApplicationView *iface, HSTRING value)
{
    struct application_view *impl=impl_from_IApplicationView(iface);
    HRESULT hr=view_check(impl); if (FAILED(hr)) return hr;
    return SetWindowTextW(impl->hwnd,WindowsGetStringRawBuffer(value,NULL)) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}
static HRESULT WINAPI view_get_Title(IApplicationView *iface, HSTRING *value)
{
    struct application_view *impl=impl_from_IApplicationView(iface);
    WCHAR *str; UINT len; HRESULT hr=view_check(impl);
    if (!value) return E_POINTER;
    *value=NULL; if (FAILED(hr)) return hr;
    len=GetWindowTextLengthW(impl->hwnd);
    if (!(str=malloc((len+1)*sizeof(*str)))) return E_OUTOFMEMORY;
    len=GetWindowTextW(impl->hwnd,str,len+1); hr=WindowsCreateString(str,len,value); free(str); return hr;
}
static HRESULT WINAPI view_get_Id(IApplicationView *iface, INT32 *value)
{ if (!value) return E_POINTER; *value=HandleToLong(impl_from_IApplicationView(iface)->hwnd); return S_OK; }
static HRESULT WINAPI view_add_Consolidated(IApplicationView *iface, ITypedEventHandler_ApplicationView_ApplicationViewConsolidatedEventArgs *handler, EventRegistrationToken *token)
{ return winrt_event_add(&impl_from_IApplicationView(iface)->consolidated,handler,token); }
static HRESULT WINAPI view_remove_Consolidated(IApplicationView *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_IApplicationView(iface)->consolidated,token); }
static const IApplicationViewVtbl view_vtbl={view_QueryInterface,view_AddRef,view_Release,view_GetIids,view_GetRuntimeClassName,view_GetTrustLevel,
    view_get_Orientation,view_get_AdjacentToLeftDisplayEdge,view_get_AdjacentToRightDisplayEdge,view_get_IsFullScreen,view_get_IsOnLockScreen,
    view_get_IsScreenCaptureEnabled,view_put_IsScreenCaptureEnabled,view_put_Title,view_get_Title,view_get_Id,view_add_Consolidated,view_remove_Consolidated};
DEFINE_IINSPECTABLE(view2,IApplicationView2,struct application_view,IApplicationView_iface)
static HRESULT WINAPI view2_get_SuppressSystemOverlays(IApplicationView2 *iface, boolean *value)
{ if (!value) return E_POINTER; *value=FALSE; return S_OK; }
static HRESULT WINAPI view2_put_SuppressSystemOverlays(IApplicationView2 *iface, boolean value)
{ return value ? E_NOTIMPL : S_OK; }
static HRESULT WINAPI view2_get_VisibleBounds(IApplicationView2 *iface, Rect *value)
{ return ICoreWindow_get_Bounds(impl_from_IApplicationView2(iface)->core,value); }
static HRESULT WINAPI view2_add_VisibleBoundsChanged(IApplicationView2 *iface, ITypedEventHandler_ApplicationView_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&impl_from_IApplicationView2(iface)->bounds_changed,handler,token); }
static HRESULT WINAPI view2_remove_VisibleBoundsChanged(IApplicationView2 *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_IApplicationView2(iface)->bounds_changed,token); }
static HRESULT WINAPI view2_SetDesiredBoundsMode(IApplicationView2 *iface, ApplicationViewBoundsMode mode, boolean *success)
{
    if (!success) return E_POINTER;
    *success=FALSE;
    if (mode!=ApplicationViewBoundsMode_UseVisible && mode!=ApplicationViewBoundsMode_UseCoreWindow) return E_INVALIDARG;
    impl_from_IApplicationView2(iface)->bounds_mode=mode; *success=TRUE; return S_OK;
}
static HRESULT WINAPI view2_get_DesiredBoundsMode(IApplicationView2 *iface, ApplicationViewBoundsMode *value)
{ if (!value) return E_POINTER; *value=impl_from_IApplicationView2(iface)->bounds_mode; return S_OK; }
static const IApplicationView2Vtbl view2_vtbl={view2_QueryInterface,view2_AddRef,view2_Release,view2_GetIids,view2_GetRuntimeClassName,view2_GetTrustLevel,
    view2_get_SuppressSystemOverlays,view2_put_SuppressSystemOverlays,view2_get_VisibleBounds,view2_add_VisibleBoundsChanged,
    view2_remove_VisibleBoundsChanged,view2_SetDesiredBoundsMode,view2_get_DesiredBoundsMode};
DEFINE_IINSPECTABLE(view3,IApplicationView3,struct application_view,IApplicationView_iface)
static HRESULT WINAPI view3_get_TitleBar(IApplicationView3 *iface, IApplicationViewTitleBar **value)
{ if (!value) return E_POINTER; *value=NULL; FIXME("TitleBar not implemented.\n"); return E_NOTIMPL; }
static HRESULT WINAPI view3_get_FullScreenSystemOverlayMode(IApplicationView3 *iface, FullScreenSystemOverlayMode *value)
{ if (!value) return E_POINTER; *value=impl_from_IApplicationView3(iface)->overlay; return S_OK; }
static HRESULT WINAPI view3_put_FullScreenSystemOverlayMode(IApplicationView3 *iface, FullScreenSystemOverlayMode value)
{
    if (value<FullScreenSystemOverlayMode_Standard || value>FullScreenSystemOverlayMode_Minimal) return E_INVALIDARG;
    /* Desktop decorations are already absent while fullscreen. */
    impl_from_IApplicationView3(iface)->overlay=value; return S_OK;
}
static HRESULT WINAPI view3_get_IsFullScreenMode(IApplicationView3 *iface, boolean *value)
{ return view_get_IsFullScreen(&impl_from_IApplicationView3(iface)->IApplicationView_iface,value); }
static HRESULT WINAPI view3_TryEnterFullScreenMode(IApplicationView3 *iface, boolean *success)
{
    struct application_view *impl=impl_from_IApplicationView3(iface);
    HRESULT hr;
    if (!success) return E_POINTER;
    *success = FALSE;
    if (FAILED(hr = view_check(impl))) return hr;
    hr = __wine_core_window_set_fullscreen(impl->core, TRUE);
    *success = SUCCEEDED(hr);
    return hr;
}
static HRESULT WINAPI view3_ExitFullScreenMode(IApplicationView3 *iface)
{
    return __wine_core_window_set_fullscreen(impl_from_IApplicationView3(iface)->core, FALSE);
}
static HRESULT WINAPI view3_ShowStandardSystemOverlays(IApplicationView3 *iface)
{ return E_NOTIMPL; }
static HRESULT WINAPI view3_TryResizeView(IApplicationView3 *iface, Size value, boolean *success)
{
    struct application_view *impl=impl_from_IApplicationView3(iface);
    float scale; RECT rect; HRESULT hr=view_check(impl);
    if (!success) return E_POINTER;
    *success=FALSE; if (FAILED(hr)) return hr;
    if (!(value.Width>0) || !(value.Height>0) || value.Width>32767 || value.Height>32767) return E_INVALIDARG;
    { BOOL fullscreen;
      if (SUCCEEDED(__wine_core_window_get_fullscreen(impl->core, &fullscreen)) && fullscreen) return S_OK; }
    scale=GetDpiForWindow(impl->hwnd)/96.0f;
    rect.left=rect.top=0; rect.right=value.Width*scale; rect.bottom=value.Height*scale;
    AdjustWindowRectExForDpi(&rect,GetWindowLongW(impl->hwnd,GWL_STYLE),FALSE,GetWindowLongW(impl->hwnd,GWL_EXSTYLE),GetDpiForWindow(impl->hwnd));
    *success=SetWindowPos(impl->hwnd,NULL,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE); return S_OK;
}
static HRESULT WINAPI view3_SetPreferredMinSize(IApplicationView3 *iface, Size size)
{
    if (!(size.Width>=0) || !(size.Height>=0) || size.Width>32767 || size.Height>32767) return E_INVALIDARG;
    impl_from_IApplicationView3(iface)->minimum=size; return S_OK;
}
static const IApplicationView3Vtbl view3_vtbl={view3_QueryInterface,view3_AddRef,view3_Release,view3_GetIids,view3_GetRuntimeClassName,view3_GetTrustLevel,
    view3_get_TitleBar,view3_get_FullScreenSystemOverlayMode,view3_put_FullScreenSystemOverlayMode,view3_get_IsFullScreenMode,
    view3_TryEnterFullScreenMode,view3_ExitFullScreenMode,view3_ShowStandardSystemOverlays,view3_TryResizeView,view3_SetPreferredMinSize};
static const WCHAR view_property[]=L"Wine.ApplicationView";
static LRESULT CALLBACK view_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
{
    struct application_view *impl=(void *)data;
    LRESULT result;
    IApplicationView_AddRef(&impl->IApplicationView_iface);
    result=DefSubclassProc(hwnd,msg,wp,lp);
    if (msg==WM_SIZE || msg==WM_DPICHANGED) winrt_event_notify(&impl->bounds_changed,&impl->IApplicationView_iface,NULL);
    if (msg==WM_GETMINMAXINFO && (GetWindowLongW(hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW))
    {
        MINMAXINFO *info=(void *)lp;
        RECT rect={0,0,impl->minimum.Width*GetDpiForWindow(hwnd)/96.0f,impl->minimum.Height*GetDpiForWindow(hwnd)/96.0f};
        AdjustWindowRectExForDpi(&rect,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE),GetDpiForWindow(hwnd));
        info->ptMinTrackSize.x=max(info->ptMinTrackSize.x,rect.right-rect.left);
        info->ptMinTrackSize.y=max(info->ptMinTrackSize.y,rect.bottom-rect.top);
    }
    if (msg==WM_NCDESTROY)
    {
        RemoveWindowSubclass(hwnd,view_proc,id); RemovePropW(hwnd,view_property); impl->hwnd=NULL;
        winrt_event_clear(&impl->consolidated); winrt_event_clear(&impl->bounds_changed);
        IApplicationView_Release(&impl->IApplicationView_iface);
    }
    IApplicationView_Release(&impl->IApplicationView_iface); return result;
}
static HRESULT application_view_current(IApplicationView **out)
{
    ICoreWindowStatic *statics;
    ICoreWindow *core=NULL;
    ICoreWindowInterop *interop;
    HSTRING name;
    HWND hwnd;
    struct application_view *impl;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out=NULL;
    WindowsCreateString(RuntimeClass_Windows_UI_Core_CoreWindow,wcslen(RuntimeClass_Windows_UI_Core_CoreWindow),&name);
    hr=RoGetActivationFactory(name,&IID_ICoreWindowStatic,(void **)&statics); WindowsDeleteString(name);
    if (FAILED(hr)) return hr;
    hr=ICoreWindowStatic_GetForCurrentThread(statics,&core); ICoreWindowStatic_Release(statics);
    if (FAILED(hr)) return hr;
    if (!core) return E_ILLEGAL_METHOD_CALL;
    hr=ICoreWindow_QueryInterface(core,&IID_ICoreWindowInterop,(void **)&interop);
    if (SUCCEEDED(hr)) { hr=ICoreWindowInterop_get_WindowHandle(interop,&hwnd); ICoreWindowInterop_Release(interop); }
    if (FAILED(hr)) { ICoreWindow_Release(core); return hr; }
    if ((impl=(void *)GetPropW(hwnd,view_property))) ICoreWindow_Release(core);
    else
    {
        if (!(impl=calloc(1,sizeof(*impl)))) { ICoreWindow_Release(core); return E_OUTOFMEMORY; }
        impl->IApplicationView_iface.lpVtbl=&view_vtbl; impl->IApplicationView2_iface.lpVtbl=&view2_vtbl;
        impl->IApplicationView3_iface.lpVtbl=&view3_vtbl; impl->ref=1;
        impl->hwnd=hwnd; impl->thread=GetCurrentThreadId(); impl->core=core;
        if (!SetPropW(hwnd,view_property,impl) || !SetWindowSubclass(hwnd,view_proc,1,(DWORD_PTR)impl))
        { RemovePropW(hwnd,view_property); IApplicationView_Release(&impl->IApplicationView_iface); return E_FAIL; }
    }
    *out=&impl->IApplicationView_iface; IApplicationView_AddRef(*out); return S_OK;
}

struct factory
{
    IActivationFactory IActivationFactory_iface;
    IApplicationViewStatics IApplicationViewStatics_iface;
    IApplicationViewStatics2 IApplicationViewStatics2_iface;
    LONG ref;
};

static inline struct factory *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct factory, IActivationFactory_iface);
}

static HRESULT WINAPI activation_factory_QueryInterface(IActivationFactory *iface, REFIID iid,
                                                        void **out)
{
    struct factory *impl = impl_from_IActivationFactory(iface);

    TRACE("iface %p, iid %s, out %p stub!\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown)
        || IsEqualGUID(iid, &IID_IInspectable)
        || IsEqualGUID(iid, &IID_IAgileObject)
        || IsEqualGUID(iid, &IID_IActivationFactory))
    {
        IActivationFactory_AddRef(&impl->IActivationFactory_iface);
        *out = &impl->IActivationFactory_iface;
        return S_OK;
    }
    else if (IsEqualGUID(iid, &IID_IApplicationViewStatics))
    {
        IApplicationViewStatics_AddRef(&impl->IApplicationViewStatics_iface);
        *out = &impl->IApplicationViewStatics_iface;
        return S_OK;
    }
    else if (IsEqualGUID(iid, &IID_IApplicationViewStatics2))
    {
        IApplicationViewStatics2_AddRef(&impl->IApplicationViewStatics2_iface);
        *out = &impl->IApplicationViewStatics2_iface;
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI activation_factory_AddRef(IActivationFactory *iface)
{
    struct factory *impl = impl_from_IActivationFactory(iface);
    ULONG ref = InterlockedIncrement(&impl->ref);
    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static ULONG WINAPI activation_factory_Release(IActivationFactory *iface)
{
    struct factory *impl = impl_from_IActivationFactory(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static HRESULT WINAPI activation_factory_GetIids(IActivationFactory *iface, ULONG *iid_count,
                                                 IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI activation_factory_GetRuntimeClassName(IActivationFactory *iface,
                                                             HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT WINAPI activation_factory_GetTrustLevel(IActivationFactory *iface,
                                                       TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT WINAPI activation_factory_ActivateInstance(IActivationFactory *iface,
                                                          IInspectable **instance)
{
    FIXME("iface %p, instance %p stub!\n", iface, instance);
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl activation_factory_vtbl =
{
    activation_factory_QueryInterface,
    activation_factory_AddRef,
    activation_factory_Release,
    /* IInspectable methods */
    activation_factory_GetIids,
    activation_factory_GetRuntimeClassName,
    activation_factory_GetTrustLevel,
    /* IActivationFactory methods */
    activation_factory_ActivateInstance,
};

DEFINE_IINSPECTABLE(statics, IApplicationViewStatics, struct factory, IActivationFactory_iface)

static HRESULT WINAPI statics_Value(IApplicationViewStatics *iface, ApplicationViewState *value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI statics_TryUnsnap(IApplicationViewStatics *iface, boolean *success)
{
    FIXME("iface %p, success %p stub!\n", iface, success);
    return E_NOTIMPL;
}

static const struct IApplicationViewStaticsVtbl statics_vtbl =
{
    statics_QueryInterface,
    statics_AddRef,
    statics_Release,
    /* IInspectable methods */
    statics_GetIids,
    statics_GetRuntimeClassName,
    statics_GetTrustLevel,
    /* IApplicationViewStatics methods */
    statics_Value,
    statics_TryUnsnap,
};

DEFINE_IINSPECTABLE(statics2, IApplicationViewStatics2, struct factory, IActivationFactory_iface)

static HRESULT WINAPI statics2_GetForCurrentView(IApplicationViewStatics2 *iface, IApplicationView **current)
{
    return application_view_current(current);
}

static HRESULT WINAPI statics2_get_TerminateAppOnFinalViewClose(IApplicationViewStatics2 *iface, boolean *value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI statics2_put_TerminateAppOnFinalViewClose(IApplicationViewStatics2 *iface, boolean value)
{
    FIXME("iface %p, value %d stub!\n", iface, value);
    return E_NOTIMPL;
}

static const struct IApplicationViewStatics2Vtbl statics2_vtbl =
{
    statics2_QueryInterface,
    statics2_AddRef,
    statics2_Release,
    /* IInspectable methods */
    statics2_GetIids,
    statics2_GetRuntimeClassName,
    statics2_GetTrustLevel,
    /* IApplicationViewStatics2 methods */
    statics2_GetForCurrentView,
    statics2_get_TerminateAppOnFinalViewClose,
    statics2_put_TerminateAppOnFinalViewClose
};

static struct factory factory =
{
    {&activation_factory_vtbl},
    {&statics_vtbl},
    {&statics2_vtbl},
    1,
};

IActivationFactory *application_view_factory = &factory.IActivationFactory_iface;
