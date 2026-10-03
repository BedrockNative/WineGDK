/* WinRT Windows.Graphics Implementation
 *
 * Copyright 2026 Zhiyi Zhang for CodeWeavers
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

#include "initguid.h"
#include "private.h"
#include "winuser.h"
#include "commctrl.h"
#include "roapi.h"
#include "corewindow.h"
#include "wine/winrt_events.h"

WINE_DEFAULT_DEBUG_CHANNEL(display);

static struct winrt_event contents_invalidated;
struct display_info
{
    IDisplayInformation IDisplayInformation_iface;
    IDisplayInformation2 IDisplayInformation2_iface;
    IDisplayInformation3 IDisplayInformation3_iface;
    IDisplayInformation4 IDisplayInformation4_iface;
    LONG ref;
    HWND hwnd;
    DWORD thread;
    struct winrt_event orientation, dpi, stereo, color;
};
static struct display_info *impl_from_IDisplayInformation(IDisplayInformation *iface)
{ return CONTAINING_RECORD(iface,struct display_info,IDisplayInformation_iface); }
static HRESULT WINAPI info_QueryInterface(IDisplayInformation *iface, REFIID iid, void **out)
{
    struct display_info *impl=impl_from_IDisplayInformation(iface);
    if (!out) return E_POINTER;
    *out=NULL;
    if (IsEqualGUID(iid,&IID_IUnknown) || IsEqualGUID(iid,&IID_IInspectable) || IsEqualGUID(iid,&IID_IDisplayInformation)) *out=iface;
    else if (IsEqualGUID(iid,&IID_IDisplayInformation2)) *out=&impl->IDisplayInformation2_iface;
    else if (IsEqualGUID(iid,&IID_IDisplayInformation3)) *out=&impl->IDisplayInformation3_iface;
    else if (IsEqualGUID(iid,&IID_IDisplayInformation4)) *out=&impl->IDisplayInformation4_iface;
    else { FIXME("display interface %s unsupported\n",debugstr_guid(iid)); return E_NOINTERFACE; }
    IDisplayInformation_AddRef(iface); return S_OK;
}
static ULONG WINAPI info_AddRef(IDisplayInformation *iface)
{ return InterlockedIncrement(&impl_from_IDisplayInformation(iface)->ref); }
static ULONG WINAPI info_Release(IDisplayInformation *iface)
{
    struct display_info *impl=impl_from_IDisplayInformation(iface);
    ULONG ref=InterlockedDecrement(&impl->ref);
    if (!ref)
    {
        winrt_event_clear(&impl->orientation); winrt_event_clear(&impl->dpi);
        winrt_event_clear(&impl->stereo); winrt_event_clear(&impl->color);
        free(impl);
    }
    return ref;
}
static HRESULT WINAPI info_GetIids(IDisplayInformation *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count=0;
    if (!(*iids=CoTaskMemAlloc(4*sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0]=IID_IDisplayInformation; (*iids)[1]=IID_IDisplayInformation2;
    (*iids)[2]=IID_IDisplayInformation3; (*iids)[3]=IID_IDisplayInformation4; *count=4; return S_OK;
}
static HRESULT WINAPI info_GetRuntimeClassName(IDisplayInformation *iface, HSTRING *name)
{ const WCHAR *str=RuntimeClass_Windows_Graphics_Display_DisplayInformation; return WindowsCreateString(str,wcslen(str),name); }
static HRESULT WINAPI info_GetTrustLevel(IDisplayInformation *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level=BaseTrust; return S_OK; }
static HRESULT info_monitor(struct display_info *impl, MONITORINFOEXW *monitor, DEVMODEW *mode)
{
    if (impl->thread!=GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (!impl->hwnd) return RO_E_CLOSED;
    memset(monitor,0,sizeof(*monitor)); monitor->cbSize=sizeof(*monitor);
    if (!GetMonitorInfoW(MonitorFromWindow(impl->hwnd,MONITOR_DEFAULTTONEAREST),(MONITORINFO *)monitor)) return HRESULT_FROM_WIN32(GetLastError());
    memset(mode,0,sizeof(*mode)); mode->dmSize=sizeof(*mode);
    if (!EnumDisplaySettingsW(monitor->szDevice,ENUM_CURRENT_SETTINGS,mode)) return HRESULT_FROM_WIN32(GetLastError());
    return S_OK;
}
static HRESULT WINAPI info_get_NativeOrientation(IDisplayInformation *iface, DisplayOrientations *value)
{
    MONITORINFOEXW monitor; DEVMODEW mode; HRESULT hr; BOOL landscape;
    if (!value) return E_POINTER;
    if (FAILED(hr=info_monitor(impl_from_IDisplayInformation(iface),&monitor,&mode))) return hr;
    landscape=mode.dmPelsWidth>=mode.dmPelsHeight;
    if (mode.dmDisplayOrientation==DMDO_90 || mode.dmDisplayOrientation==DMDO_270) landscape=!landscape;
    *value=landscape ? DisplayOrientations_Landscape : DisplayOrientations_Portrait;
    return S_OK;
}
static HRESULT WINAPI info_get_CurrentOrientation(IDisplayInformation *iface, DisplayOrientations *value)
{
    MONITORINFOEXW monitor; DEVMODEW mode; HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr=info_monitor(impl_from_IDisplayInformation(iface),&monitor,&mode))) return hr;
    *value=mode.dmPelsWidth>=mode.dmPelsHeight ? DisplayOrientations_Landscape : DisplayOrientations_Portrait;
    if (mode.dmDisplayOrientation==DMDO_180 || mode.dmDisplayOrientation==DMDO_270) *value<<=2;
    return S_OK;
}
static HRESULT WINAPI info_get_LogicalDpi(IDisplayInformation *iface, FLOAT *value)
{
    struct display_info *impl=impl_from_IDisplayInformation(iface);
    if (!value) return E_POINTER;
    if (impl->thread!=GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (!impl->hwnd) return RO_E_CLOSED;
    *value=GetDpiForWindow(impl->hwnd);
    return *value ? S_OK : E_FAIL;
}
static HRESULT WINAPI info_get_ResolutionScale(IDisplayInformation *iface, ResolutionScale *value)
{
    FLOAT dpi; HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr=info_get_LogicalDpi(iface,&dpi))) return hr;
    *value=(ResolutionScale)(dpi*100/96+0.5f); return S_OK;
}
static HRESULT info_raw_dpi(IDisplayInformation *iface, FLOAT *value, BOOL horizontal)
{
    MONITORINFOEXW monitor; DEVMODEW mode; HDC dc; int mm, pixels; HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr=info_monitor(impl_from_IDisplayInformation(iface),&monitor,&mode))) return hr;
    if (!(dc=CreateDCW(L"DISPLAY",monitor.szDevice,NULL,NULL))) return HRESULT_FROM_WIN32(GetLastError());
    mm=GetDeviceCaps(dc,horizontal ? HORZSIZE : VERTSIZE);
    pixels=horizontal ? mode.dmPelsWidth : mode.dmPelsHeight;
    DeleteDC(dc);
    if (mm<=0) return info_get_LogicalDpi(iface,value);
    *value=pixels*25.4f/mm; return S_OK;
}
static HRESULT WINAPI info_get_RawDpiX(IDisplayInformation *iface, FLOAT *value) { return info_raw_dpi(iface,value,TRUE); }
static HRESULT WINAPI info_get_RawDpiY(IDisplayInformation *iface, FLOAT *value) { return info_raw_dpi(iface,value,FALSE); }
static HRESULT WINAPI info_get_StereoEnabled(IDisplayInformation *iface, boolean *value)
{ if (!value) return E_POINTER; *value=FALSE; return S_OK; }
static HRESULT WINAPI info_GetColorProfileAsync(IDisplayInformation *iface, IAsyncOperation_IRandomAccessStream **out)
{ if (!out) return E_POINTER; *out=NULL; return E_NOTIMPL; }
static HRESULT WINAPI info_add_OrientationChanged(IDisplayInformation *iface, ITypedEventHandler_DisplayInformation_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&impl_from_IDisplayInformation(iface)->orientation,handler,token); }
static HRESULT WINAPI info_remove_OrientationChanged(IDisplayInformation *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_IDisplayInformation(iface)->orientation,token); }
static HRESULT WINAPI info_add_DpiChanged(IDisplayInformation *iface, ITypedEventHandler_DisplayInformation_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&impl_from_IDisplayInformation(iface)->dpi,handler,token); }
static HRESULT WINAPI info_remove_DpiChanged(IDisplayInformation *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_IDisplayInformation(iface)->dpi,token); }
static HRESULT WINAPI info_add_StereoEnabledChanged(IDisplayInformation *iface, ITypedEventHandler_DisplayInformation_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&impl_from_IDisplayInformation(iface)->stereo,handler,token); }
static HRESULT WINAPI info_remove_StereoEnabledChanged(IDisplayInformation *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_IDisplayInformation(iface)->stereo,token); }
static HRESULT WINAPI info_add_ColorProfileChanged(IDisplayInformation *iface, ITypedEventHandler_DisplayInformation_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&impl_from_IDisplayInformation(iface)->color,handler,token); }
static HRESULT WINAPI info_remove_ColorProfileChanged(IDisplayInformation *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_IDisplayInformation(iface)->color,token); }
static const IDisplayInformationVtbl info_vtbl =
{
    info_QueryInterface,info_AddRef,info_Release,info_GetIids,info_GetRuntimeClassName,info_GetTrustLevel,
    info_get_CurrentOrientation,info_get_NativeOrientation,info_add_OrientationChanged,info_remove_OrientationChanged,
    info_get_ResolutionScale,info_get_LogicalDpi,info_get_RawDpiX,info_get_RawDpiY,info_add_DpiChanged,info_remove_DpiChanged,
    info_get_StereoEnabled,info_add_StereoEnabledChanged,info_remove_StereoEnabledChanged,info_GetColorProfileAsync,
    info_add_ColorProfileChanged,info_remove_ColorProfileChanged
};
DEFINE_IINSPECTABLE(info2, IDisplayInformation2, struct display_info, IDisplayInformation_iface)
static HRESULT WINAPI info2_get_RawPixelsPerViewPixel(IDisplayInformation2 *iface, DOUBLE *value)
{
    FLOAT dpi; HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr=info_get_LogicalDpi(&impl_from_IDisplayInformation2(iface)->IDisplayInformation_iface,&dpi))) return hr;
    *value=dpi/96.0; return S_OK;
}
static const IDisplayInformation2Vtbl info2_vtbl =
{ info2_QueryInterface,info2_AddRef,info2_Release,info2_GetIids,info2_GetRuntimeClassName,info2_GetTrustLevel,info2_get_RawPixelsPerViewPixel };
DEFINE_IINSPECTABLE(info3, IDisplayInformation3, struct display_info, IDisplayInformation_iface)
static HRESULT WINAPI info3_get_DiagonalSizeInInches(IDisplayInformation3 *iface, IReference_DOUBLE **value)
{ if (!value) return E_POINTER; *value=NULL; return S_OK; /* No verified panel measurement. */ }
static const IDisplayInformation3Vtbl info3_vtbl =
{ info3_QueryInterface,info3_AddRef,info3_Release,info3_GetIids,info3_GetRuntimeClassName,info3_GetTrustLevel,info3_get_DiagonalSizeInInches };
DEFINE_IINSPECTABLE(info4, IDisplayInformation4, struct display_info, IDisplayInformation_iface)
static HRESULT WINAPI info4_get_ScreenWidthInRawPixels(IDisplayInformation4 *iface, UINT32 *value)
{
    MONITORINFOEXW monitor; DEVMODEW mode; HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr=info_monitor(impl_from_IDisplayInformation4(iface),&monitor,&mode))) return hr;
    *value=mode.dmPelsWidth; return S_OK;
}
static HRESULT WINAPI info4_get_ScreenHeightInRawPixels(IDisplayInformation4 *iface, UINT32 *value)
{
    MONITORINFOEXW monitor; DEVMODEW mode; HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr=info_monitor(impl_from_IDisplayInformation4(iface),&monitor,&mode))) return hr;
    *value=mode.dmPelsHeight; return S_OK;
}
static const IDisplayInformation4Vtbl info4_vtbl =
{ info4_QueryInterface,info4_AddRef,info4_Release,info4_GetIids,info4_GetRuntimeClassName,info4_GetTrustLevel,
  info4_get_ScreenWidthInRawPixels,info4_get_ScreenHeightInRawPixels };
static const WCHAR display_property[]=L"Wine.DisplayInformation";
static LRESULT CALLBACK display_subclass(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR data)
{
    struct display_info *impl=(void *)data;
    LRESULT ret;
    IDisplayInformation_AddRef(&impl->IDisplayInformation_iface);
    if (msg==WM_DPICHANGED) winrt_event_notify(&impl->dpi,&impl->IDisplayInformation_iface,NULL);
    if (msg==WM_DISPLAYCHANGE)
    {
        winrt_event_notify(&impl->orientation,&impl->IDisplayInformation_iface,NULL);
        winrt_event_notify(&contents_invalidated,&impl->IDisplayInformation_iface,NULL);
    }
    if (msg==WM_NCDESTROY)
    {
        RemoveWindowSubclass(hwnd,display_subclass,0);
        RemovePropW(hwnd,display_property);
        impl->hwnd=NULL;
        winrt_event_clear(&impl->orientation); winrt_event_clear(&impl->dpi);
        winrt_event_clear(&impl->stereo); winrt_event_clear(&impl->color);
        IDisplayInformation_Release(&impl->IDisplayInformation_iface); /* Window ownership. */
    }
    ret=DefSubclassProc(hwnd,msg,wparam,lparam);
    IDisplayInformation_Release(&impl->IDisplayInformation_iface);
    return ret;
}
static HRESULT display_for_current_view(IDisplayInformation **out)
{
    ICoreWindowStatic *statics;
    ICoreWindow *window;
    ICoreWindowInterop *interop;
    HSTRING name;
    HWND hwnd;
    struct display_info *impl;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out=NULL;
    WindowsCreateString(RuntimeClass_Windows_UI_Core_CoreWindow,wcslen(RuntimeClass_Windows_UI_Core_CoreWindow),&name);
    hr=RoGetActivationFactory(name,&IID_ICoreWindowStatic,(void **)&statics);
    WindowsDeleteString(name);
    if (FAILED(hr)) return hr;
    hr=ICoreWindowStatic_GetForCurrentThread(statics,&window);
    ICoreWindowStatic_Release(statics);
    if (FAILED(hr)) return hr;
    if (!window) return E_ILLEGAL_METHOD_CALL;
    hr=ICoreWindow_QueryInterface(window,&IID_ICoreWindowInterop,(void **)&interop);
    ICoreWindow_Release(window);
    if (FAILED(hr)) return hr;
    hr=ICoreWindowInterop_get_WindowHandle(interop,&hwnd);
    ICoreWindowInterop_Release(interop);
    if (FAILED(hr)) return hr;
    if (!(impl=(void *)GetPropW(hwnd,display_property)))
    {
        if (!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
        impl->IDisplayInformation_iface.lpVtbl=&info_vtbl;
        impl->IDisplayInformation2_iface.lpVtbl=&info2_vtbl;
        impl->IDisplayInformation3_iface.lpVtbl=&info3_vtbl;
        impl->IDisplayInformation4_iface.lpVtbl=&info4_vtbl;
        impl->ref=1; impl->hwnd=hwnd; impl->thread=GetCurrentThreadId();
        if (!SetPropW(hwnd,display_property,impl)) { free(impl); return HRESULT_FROM_WIN32(GetLastError()); }
        if (!SetWindowSubclass(hwnd,display_subclass,0,(DWORD_PTR)impl))
        { RemovePropW(hwnd,display_property); free(impl); return E_FAIL; }
    }
    IDisplayInformation_AddRef((*out=&impl->IDisplayInformation_iface));
    return S_OK;
}

struct display_info_statics
{
    IActivationFactory IActivationFactory_iface;
    IDisplayInformationStatics IDisplayInformationStatics_iface;
    LONG ref;
};

static inline struct display_info_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct display_info_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct display_info_statics *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown )
        || IsEqualGUID( iid, &IID_IAgileObject )
        || IsEqualGUID( iid, &IID_IInspectable )
        || IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IActivationFactory_AddRef( *out );
        return S_OK;
    }
    else if (IsEqualGUID( iid, &IID_IDisplayInformationStatics ))
    {
        *out = &impl->IDisplayInformationStatics_iface;
        IDisplayInformationStatics_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct display_info_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct display_info_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    /* IInspectable methods */
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    /* IActivationFactory methods */
    factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( display_info_statics, IDisplayInformationStatics, struct display_info_statics,
                     IActivationFactory_iface )

static HRESULT WINAPI display_info_statics_GetForCurrentView( IDisplayInformationStatics *iface,
        IDisplayInformation **current )
{
    return display_for_current_view(current);
}

static HRESULT WINAPI display_info_statics_get_AutoRotationPreferences( IDisplayInformationStatics *iface,
        DisplayOrientations *value )
{
    if (!value) return E_POINTER;
    return GetDisplayAutoRotationPreferences((ORIENTATION_PREFERENCE *)value) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

static HRESULT WINAPI display_info_statics_put_AutoRotationPreferences( IDisplayInformationStatics *iface,
        DisplayOrientations value )
{
    return SetDisplayAutoRotationPreferences((ORIENTATION_PREFERENCE)value) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

static HRESULT WINAPI display_info_statics_add_DisplayContentsInvalidated( IDisplayInformationStatics *iface,
        ITypedEventHandler_DisplayInformation_IInspectable *handler, EventRegistrationToken *token )
{
    return winrt_event_add(&contents_invalidated,handler,token);
}

static HRESULT WINAPI display_info_statics_remove_DisplayContentsInvalidated( IDisplayInformationStatics *iface,
        EventRegistrationToken token )
{
    return winrt_event_remove(&contents_invalidated,token);
}

static const struct IDisplayInformationStaticsVtbl display_info_statics_vtbl =
{
    display_info_statics_QueryInterface,
    display_info_statics_AddRef,
    display_info_statics_Release,
    /* IInspectable methods */
    display_info_statics_GetIids,
    display_info_statics_GetRuntimeClassName,
    display_info_statics_GetTrustLevel,
    /* IDisplayInformationStatics methods */
    display_info_statics_GetForCurrentView,
    display_info_statics_get_AutoRotationPreferences,
    display_info_statics_put_AutoRotationPreferences,
    display_info_statics_add_DisplayContentsInvalidated,
    display_info_statics_remove_DisplayContentsInvalidated
};

static struct display_info_statics display_info_statics =
{
    {&factory_vtbl},
    {&display_info_statics_vtbl},
    1,
};

static IActivationFactory *display_info_factory = &display_info_statics.IActivationFactory_iface;

HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID riid, void **out )
{
    FIXME( "clsid %s, riid %s, out %p stub!\n", debugstr_guid( clsid ), debugstr_guid( riid ), out );
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllGetActivationFactory( HSTRING classid, IActivationFactory **factory )
{
    const WCHAR *name = WindowsGetStringRawBuffer( classid, NULL );

    TRACE( "classid %s, factory %p.\n", debugstr_hstring( classid ), factory );

    *factory = NULL;

    if (!wcscmp( name, RuntimeClass_Windows_Graphics_Display_DisplayInformation ))
        IActivationFactory_QueryInterface( display_info_factory, &IID_IActivationFactory, (void **)factory );

    return *factory ? S_OK : CLASS_E_CLASSNOTAVAILABLE;
}
