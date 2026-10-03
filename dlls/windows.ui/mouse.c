/* Per-view raw mouse movement. LGPL-2.1-or-later. */
#include "private.h"
#include "winuser.h"
#include "wine/debug.h"
#include "wine/winrt_events.h"
WINE_DEFAULT_DEBUG_CHANNEL(ui);
struct mouse { IMouseDevice iface; LONG ref; HWND hwnd; struct winrt_event moved; };
struct mouse_args { IMouseEventArgs iface; LONG ref; MouseDelta delta; };

static HRESULT WINAPI mouse_QueryInterface(IMouseDevice *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) && !IsEqualGUID(iid,&IID_IMouseDevice)) return E_NOINTERFACE;
    *out=iface; IMouseDevice_AddRef(iface); return S_OK;
}
static ULONG WINAPI mouse_AddRef(IMouseDevice *iface)
{ return InterlockedIncrement(&((struct mouse *)iface)->ref); }
static ULONG WINAPI mouse_Release(IMouseDevice *iface)
{
    struct mouse *impl=(void *)iface;
    ULONG ref=InterlockedDecrement(&impl->ref);
    if (!ref) { winrt_event_clear(&impl->moved); free(impl); }
    return ref;
}
static HRESULT WINAPI mouse_GetIids(IMouseDevice *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count=0; if (!(*iids=CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids=IID_IMouseDevice; *count=1; return S_OK;
}
static HRESULT WINAPI mouse_GetRuntimeClassName(IMouseDevice *iface, HSTRING *name)
{ const WCHAR *str=RuntimeClass_Windows_Devices_Input_MouseDevice; return WindowsCreateString(str,wcslen(str),name); }
static HRESULT WINAPI mouse_GetTrustLevel(IMouseDevice *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level=BaseTrust; return S_OK; }

static HRESULT WINAPI mouse_args_QueryInterface(IMouseEventArgs *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) && !IsEqualGUID(iid,&IID_IMouseEventArgs)) return E_NOINTERFACE;
    *out=iface; IMouseEventArgs_AddRef(iface); return S_OK;
}
static ULONG WINAPI mouse_args_AddRef(IMouseEventArgs *iface)
{ return InterlockedIncrement(&((struct mouse_args *)iface)->ref); }
static ULONG WINAPI mouse_args_Release(IMouseEventArgs *iface)
{
    struct mouse_args *impl=(void *)iface;
    ULONG ref=InterlockedDecrement(&impl->ref);
    if (!ref) {  free(impl); }
    return ref;
}
static HRESULT WINAPI mouse_args_GetIids(IMouseEventArgs *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count=0; if (!(*iids=CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids=IID_IMouseEventArgs; *count=1; return S_OK;
}
static HRESULT WINAPI mouse_args_GetRuntimeClassName(IMouseEventArgs *iface, HSTRING *name)
{ const WCHAR *str=RuntimeClass_Windows_Devices_Input_MouseEventArgs; return WindowsCreateString(str,wcslen(str),name); }
static HRESULT WINAPI mouse_args_GetTrustLevel(IMouseEventArgs *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level=BaseTrust; return S_OK; }

static HRESULT WINAPI mouse_add_MouseMoved(IMouseDevice *iface, ITypedEventHandler_MouseDevice_MouseEventArgs *handler, EventRegistrationToken *token)
{
    struct mouse *impl=(void *)iface;
    RAWINPUTDEVICE device={1,2,0,impl->hwnd};
    if (!impl->hwnd) return RO_E_CLOSED;
    if (!handler || !token) return E_POINTER;
    if (!RegisterRawInputDevices(&device,1,sizeof(device))) return HRESULT_FROM_WIN32(GetLastError());
    return winrt_event_add(&impl->moved,handler,token);
}
static HRESULT WINAPI mouse_remove_MouseMoved(IMouseDevice *iface, EventRegistrationToken token)
{ return winrt_event_remove(&((struct mouse *)iface)->moved,token); }
static HRESULT WINAPI mouse_args_get_MouseDelta(IMouseEventArgs *iface, MouseDelta *delta)
{ if (!delta) return E_POINTER; *delta=((struct mouse_args *)iface)->delta; return S_OK; }
static const IMouseDeviceVtbl mouse_vtbl={mouse_QueryInterface,mouse_AddRef,mouse_Release,mouse_GetIids,mouse_GetRuntimeClassName,mouse_GetTrustLevel,mouse_add_MouseMoved,mouse_remove_MouseMoved};
static const IMouseEventArgsVtbl mouse_args_vtbl={mouse_args_QueryInterface,mouse_args_AddRef,mouse_args_Release,mouse_args_GetIids,mouse_args_GetRuntimeClassName,mouse_args_GetTrustLevel,mouse_args_get_MouseDelta};
HRESULT mouse_create(HWND hwnd, IMouseDevice **out)
{
    struct mouse *impl=calloc(1,sizeof(*impl));
    if (!impl) return E_OUTOFMEMORY;
    impl->iface.lpVtbl=&mouse_vtbl; impl->ref=1; impl->hwnd=hwnd; *out=&impl->iface; return S_OK;
}
void mouse_close(IMouseDevice *iface)
{
    struct mouse *impl=(void *)iface;
    RAWINPUTDEVICE *devices, remove={1,2,RIDEV_REMOVE,NULL};
    UINT count=0,i;
    /* Do not remove a registration another input consumer has replaced. */
    if (!GetRegisteredRawInputDevices(NULL,&count,sizeof(*devices)) && count && (devices=malloc(count*sizeof(*devices))))
    {
        if (GetRegisteredRawInputDevices(devices,&count,sizeof(*devices)) != (UINT)-1)
            for (i=0;i<count;i++)
                if (devices[i].usUsagePage==1 && devices[i].usUsage==2 && devices[i].hwndTarget==impl->hwnd)
                    RegisterRawInputDevices(&remove,1,sizeof(remove));
        free(devices);
    }
    impl->hwnd=NULL; winrt_event_clear(&impl->moved);
}
void mouse_input(IMouseDevice *iface, HRAWINPUT input)
{
    struct mouse *impl=(void *)iface;
    struct mouse_args *args;
    RAWINPUT raw;
    UINT size=sizeof(raw);
    if (GetRawInputData(input,RID_INPUT,&raw,&size,sizeof(raw.header))==(UINT)-1 || raw.header.dwType!=RIM_TYPEMOUSE) return;
    /* Absolute tablet devices require separate coordinate tracking. */
    if (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) return;
    if (!(args=calloc(1,sizeof(*args)))) return;
    args->iface.lpVtbl=&mouse_args_vtbl; args->ref=1;
    args->delta.X=raw.data.mouse.lLastX; args->delta.Y=raw.data.mouse.lLastY;
    winrt_event_notify(&impl->moved,iface,&args->iface);
    IMouseEventArgs_Release(&args->iface);
}
struct mouse_statics
{
    IActivationFactory IActivationFactory_iface;
    IMouseDeviceStatics IMouseDeviceStatics_iface;
    LONG ref;
};

static inline struct mouse_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct mouse_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct mouse_statics *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IActivationFactory_AddRef( &impl->IActivationFactory_iface );
        return S_OK;
    }
    else if (IsEqualGUID( iid, &IID_IMouseDeviceStatics ))
    {
        *out = &impl->IMouseDeviceStatics_iface;
        IMouseDeviceStatics_AddRef( &impl->IMouseDeviceStatics_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct mouse_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct mouse_statics *impl = impl_from_IActivationFactory( iface );
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
    FIXME( "iface %p, instance %p.\n", iface, instance );
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

DEFINE_IINSPECTABLE( mouse_static, IMouseDeviceStatics, struct mouse_statics, IActivationFactory_iface )

static HRESULT STDMETHODCALLTYPE mouse_static_GetForCurrentView(IMouseDeviceStatics *iface, IMouseDevice **out)
{ return corewindow_get_mouse(out); }

static const struct IMouseDeviceStaticsVtbl mouse_static_vtbl =
{
    mouse_static_QueryInterface,
    mouse_static_AddRef,
    mouse_static_Release,
    /* IInspectable methods */
    mouse_static_GetIids,
    mouse_static_GetRuntimeClassName,
    mouse_static_GetTrustLevel,
    /* IMouseDeviceStatics methods */
    mouse_static_GetForCurrentView
};

static struct mouse_statics mouse_statics =
{
    {&factory_vtbl},
    {&mouse_static_vtbl},
    1,
};

IActivationFactory *mouse_factory = &mouse_statics.IActivationFactory_iface;
