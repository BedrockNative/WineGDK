/* Input capability queries backed by Wine's native device metrics. LGPL-2.1-or-later. */
#include "private.h"
#include "winuser.h"
struct mousecapabilities { IMouseCapabilities iface; LONG ref; };
static HRESULT WINAPI mousecapabilities_qi(IMouseCapabilities *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) && !IsEqualGUID(iid,&IID_IAgileObject) && !IsEqualGUID(iid,&IID_IMouseCapabilities)) return E_NOINTERFACE;
    *out=iface; IMouseCapabilities_AddRef(iface); return S_OK;
}
static ULONG WINAPI mousecapabilities_addref(IMouseCapabilities *iface) { return InterlockedIncrement(&((struct mousecapabilities *)iface)->ref); }
static ULONG WINAPI mousecapabilities_release(IMouseCapabilities *iface) { ULONG ref=InterlockedDecrement(&((struct mousecapabilities *)iface)->ref); if (!ref) free(iface); return ref; }
static HRESULT WINAPI mousecapabilities_iids(IMouseCapabilities *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count=0; if (!(*iids=CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids=IID_IMouseCapabilities; *count=1; return S_OK;
}
static HRESULT WINAPI mousecapabilities_name(IMouseCapabilities *iface, HSTRING *name)
{ const WCHAR *str=RuntimeClass_Windows_Devices_Input_MouseCapabilities; return WindowsCreateString(str,wcslen(str),name); }
static HRESULT WINAPI mousecapabilities_trust(IMouseCapabilities *iface, TrustLevel *level) { if (!level) return E_POINTER; *level=BaseTrust; return S_OK; }
static HRESULT WINAPI mousecapabilities_MousePresent(IMouseCapabilities *iface, INT32 *value) { if (!value) return E_POINTER; *value=GetSystemMetrics(SM_MOUSEPRESENT); return S_OK; }
static HRESULT WINAPI mousecapabilities_VerticalWheelPresent(IMouseCapabilities *iface, INT32 *value) { if (!value) return E_POINTER; *value=GetSystemMetrics(SM_MOUSEWHEELPRESENT); return S_OK; }
static HRESULT WINAPI mousecapabilities_HorizontalWheelPresent(IMouseCapabilities *iface, INT32 *value) { if (!value) return E_POINTER; *value=GetSystemMetrics(SM_MOUSEHORIZONTALWHEELPRESENT); return S_OK; }
static HRESULT WINAPI mousecapabilities_SwapButtons(IMouseCapabilities *iface, INT32 *value) { if (!value) return E_POINTER; *value=GetSystemMetrics(SM_SWAPBUTTON); return S_OK; }
static HRESULT WINAPI mousecapabilities_NumberOfButtons(IMouseCapabilities *iface, UINT32 *value) { if (!value) return E_POINTER; *value=GetSystemMetrics(SM_CMOUSEBUTTONS); return S_OK; }
static const IMouseCapabilitiesVtbl mousecapabilities_vtbl={mousecapabilities_qi,mousecapabilities_addref,mousecapabilities_release,mousecapabilities_iids,mousecapabilities_name,mousecapabilities_trust,mousecapabilities_MousePresent,mousecapabilities_VerticalWheelPresent,mousecapabilities_HorizontalWheelPresent,mousecapabilities_SwapButtons,mousecapabilities_NumberOfButtons};
struct keyboardcapabilities { IKeyboardCapabilities iface; LONG ref; };
static HRESULT WINAPI keyboardcapabilities_qi(IKeyboardCapabilities *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) && !IsEqualGUID(iid,&IID_IAgileObject) && !IsEqualGUID(iid,&IID_IKeyboardCapabilities)) return E_NOINTERFACE;
    *out=iface; IKeyboardCapabilities_AddRef(iface); return S_OK;
}
static ULONG WINAPI keyboardcapabilities_addref(IKeyboardCapabilities *iface) { return InterlockedIncrement(&((struct keyboardcapabilities *)iface)->ref); }
static ULONG WINAPI keyboardcapabilities_release(IKeyboardCapabilities *iface) { ULONG ref=InterlockedDecrement(&((struct keyboardcapabilities *)iface)->ref); if (!ref) free(iface); return ref; }
static HRESULT WINAPI keyboardcapabilities_iids(IKeyboardCapabilities *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count=0; if (!(*iids=CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids=IID_IKeyboardCapabilities; *count=1; return S_OK;
}
static HRESULT WINAPI keyboardcapabilities_name(IKeyboardCapabilities *iface, HSTRING *name)
{ const WCHAR *str=RuntimeClass_Windows_Devices_Input_KeyboardCapabilities; return WindowsCreateString(str,wcslen(str),name); }
static HRESULT WINAPI keyboardcapabilities_trust(IKeyboardCapabilities *iface, TrustLevel *level) { if (!level) return E_POINTER; *level=BaseTrust; return S_OK; }
static HRESULT WINAPI keyboardcapabilities_KeyboardPresent(IKeyboardCapabilities *iface, INT32 *value) { if (!value) return E_POINTER; *value=!!GetKeyboardType(0); return S_OK; }
static const IKeyboardCapabilitiesVtbl keyboardcapabilities_vtbl={keyboardcapabilities_qi,keyboardcapabilities_addref,keyboardcapabilities_release,keyboardcapabilities_iids,keyboardcapabilities_name,keyboardcapabilities_trust,keyboardcapabilities_KeyboardPresent};
struct touchcapabilities { ITouchCapabilities iface; LONG ref; };
static HRESULT WINAPI touchcapabilities_qi(ITouchCapabilities *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) && !IsEqualGUID(iid,&IID_IAgileObject) && !IsEqualGUID(iid,&IID_ITouchCapabilities)) return E_NOINTERFACE;
    *out=iface; ITouchCapabilities_AddRef(iface); return S_OK;
}
static ULONG WINAPI touchcapabilities_addref(ITouchCapabilities *iface) { return InterlockedIncrement(&((struct touchcapabilities *)iface)->ref); }
static ULONG WINAPI touchcapabilities_release(ITouchCapabilities *iface) { ULONG ref=InterlockedDecrement(&((struct touchcapabilities *)iface)->ref); if (!ref) free(iface); return ref; }
static HRESULT WINAPI touchcapabilities_iids(ITouchCapabilities *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count=0; if (!(*iids=CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids=IID_ITouchCapabilities; *count=1; return S_OK;
}
static HRESULT WINAPI touchcapabilities_name(ITouchCapabilities *iface, HSTRING *name)
{ const WCHAR *str=RuntimeClass_Windows_Devices_Input_TouchCapabilities; return WindowsCreateString(str,wcslen(str),name); }
static HRESULT WINAPI touchcapabilities_trust(ITouchCapabilities *iface, TrustLevel *level) { if (!level) return E_POINTER; *level=BaseTrust; return S_OK; }
static HRESULT WINAPI touchcapabilities_TouchPresent(ITouchCapabilities *iface, INT32 *value) { if (!value) return E_POINTER; *value=!!(GetSystemMetrics(SM_DIGITIZER) & (NID_INTEGRATED_TOUCH | NID_EXTERNAL_TOUCH)); return S_OK; }
static HRESULT WINAPI touchcapabilities_Contacts(ITouchCapabilities *iface, UINT32 *value) { if (!value) return E_POINTER; *value=GetSystemMetrics(SM_MAXIMUMTOUCHES); return S_OK; }
static const ITouchCapabilitiesVtbl touchcapabilities_vtbl={touchcapabilities_qi,touchcapabilities_addref,touchcapabilities_release,touchcapabilities_iids,touchcapabilities_name,touchcapabilities_trust,touchcapabilities_TouchPresent,touchcapabilities_Contacts};
struct capability_factory { IActivationFactory iface; LONG ref; unsigned int kind; };
static HRESULT WINAPI factory_qi(IActivationFactory *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) && !IsEqualGUID(iid,&IID_IAgileObject) && !IsEqualGUID(iid,&IID_IActivationFactory)) return E_NOINTERFACE;
    *out=iface; IActivationFactory_AddRef(iface); return S_OK;
}
static ULONG WINAPI factory_addref(IActivationFactory *iface) { return InterlockedIncrement(&((struct capability_factory *)iface)->ref); }
static ULONG WINAPI factory_release(IActivationFactory *iface) { return InterlockedDecrement(&((struct capability_factory *)iface)->ref); }
static HRESULT WINAPI factory_iids(IActivationFactory *iface, ULONG *count, IID **iids)
{ if (!count || !iids) return E_POINTER; *count=0; *iids=NULL; return S_OK; }
static HRESULT WINAPI factory_name(IActivationFactory *iface, HSTRING *name)
{
    static const WCHAR *const names[]={L"Windows.Devices.Input.MouseCapabilities",L"Windows.Devices.Input.KeyboardCapabilities",L"Windows.Devices.Input.TouchCapabilities"};
    const WCHAR *str=names[((struct capability_factory *)iface)->kind];
    return WindowsCreateString(str,wcslen(str),name);
}
static HRESULT WINAPI factory_trust(IActivationFactory *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level=BaseTrust; return S_OK; }
static HRESULT WINAPI factory_activate(IActivationFactory *iface, IInspectable **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    switch (((struct capability_factory *)iface)->kind) {
    case 0: {
        struct mousecapabilities *impl=calloc(1,sizeof(*impl));
        if (!impl) return E_OUTOFMEMORY;
        impl->iface.lpVtbl=&mousecapabilities_vtbl; impl->ref=1; *out=(IInspectable *)&impl->iface; return S_OK;
    }
    case 1: {
        struct keyboardcapabilities *impl=calloc(1,sizeof(*impl));
        if (!impl) return E_OUTOFMEMORY;
        impl->iface.lpVtbl=&keyboardcapabilities_vtbl; impl->ref=1; *out=(IInspectable *)&impl->iface; return S_OK;
    }
    case 2: {
        struct touchcapabilities *impl=calloc(1,sizeof(*impl));
        if (!impl) return E_OUTOFMEMORY;
        impl->iface.lpVtbl=&touchcapabilities_vtbl; impl->ref=1; *out=(IInspectable *)&impl->iface; return S_OK;
    }
    }
    return E_UNEXPECTED;
}
static const IActivationFactoryVtbl factory_vtbl={factory_qi,factory_addref,factory_release,factory_iids,factory_name,factory_trust,factory_activate};
static struct capability_factory factories[]={{{&factory_vtbl},1,0},{{&factory_vtbl},1,1},{{&factory_vtbl},1,2}};
IActivationFactory *mousecapabilities_factory=&factories[0].iface;
IActivationFactory *keyboardcapabilities_factory=&factories[1].iface;
IActivationFactory *touchcapabilities_factory=&factories[2].iface;
