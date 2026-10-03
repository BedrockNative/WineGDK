/* Basic XAML control state and composition. SPDX-License-Identifier: LGPL-2.1-or-later */
#include "private.h"
#include "roapi.h"
#include "dxgi.h"
#include "corewindow.h"
#include "math.h"
#include "wine/winrt_events.h"
#define WIDL_using_Windows_UI_Core
#include "windows.ui.core.h"
#define WIDL_using_Windows_UI_Xaml
#include "windows.ui.xaml.h"
WINE_DEFAULT_DEBUG_CHANNEL(xaml);
struct control;
struct node_iface { const void *lpVtbl; struct control *owner; };
struct node_base_vtbl
{
    HRESULT (WINAPI *QueryInterface)(struct node_iface *, REFIID, void **);
    ULONG (WINAPI *AddRef)(struct node_iface *);
    ULONG (WINAPI *Release)(struct node_iface *);
    HRESULT (WINAPI *GetIids)(struct node_iface *, ULONG *, IID **);
    HRESULT (WINAPI *GetRuntimeClassName)(struct node_iface *, HSTRING *);
    HRESULT (WINAPI *GetTrustLevel)(struct node_iface *, TrustLevel *);
};
struct node_thickness { DOUBLE left, top, right, bottom; };
struct node_weight { UINT16 weight; };
#include "controls_abi.h"
enum node_value_kind { NODE_DATA, NODE_OBJECT, NODE_STRING };
struct node_property
{
    struct node_property *next;
    WCHAR *name;
    enum node_value_kind kind;
    SIZE_T size;
    BYTE data[sizeof(struct node_thickness)];
};
struct node_event { struct node_event *next; WCHAR *name; struct winrt_event value; };
struct control
{
    struct node_iface inner, interfaces[NODE_IFACE_COUNT], native_panel;
    IInspectable *outer;
    LONG ref;
    UINT mask, primary;
    const WCHAR *class_name;
    struct node_property *properties;
    struct node_event *events;
    ICoreWindow *window;
    IVector_IInspectable *children;
    Size actual_size;
    BOOL laid_out;
};
static const GUID children_vector_iid = {0xb4c1e3ac,0x8768,0x5b9d,{0xa6,0x61,0xf6,0x33,0x30,0xb8,0x50,0x7b}};
static const GUID children_view_iid = {0xf3864c10,0x14a4,0x5516,{0xb1,0xd9,0x63,0xb6,0x57,0x94,0x29,0xb1}};
static const GUID children_iterable_iid = {0x42e26ae1,0xd357,0x57e8,{0xbb,0x48,0xf7,0x5c,0x9f,0xf6,0x9d,0x91}};
static const GUID children_iterator_iid = {0x1d1f9d60,0xd53b,0x57f7,{0xb1,0x44,0x8f,0x7c,0x48,0x78,0x46,0xe8}};
static const struct vector_iids children_iids = {&children_vector_iid, &children_view_iid, &children_iterable_iid, &children_iterator_iid};
static const GUID native_panel_iid = {0xf92f19d2,0x3ade,0x45a6,{0xa2,0x0c,0xf6,0xf1,0xea,0x90,0x55,0x4b}};
static struct control *focused;
static HRESULT node_layout(struct node_iface *iface);
static HRESULT node_text_changed(struct control *impl);
static HRESULT WINAPI node_inner_QueryInterface(struct node_iface *iface, REFIID iid, void **out);
static ULONG WINAPI node_inner_AddRef(struct node_iface *iface) { return InterlockedIncrement(&iface->owner->ref); }
static void property_clear(struct node_property *p)
{
    if (p->kind == NODE_OBJECT && *(IInspectable **)p->data) IInspectable_Release(*(IInspectable **)p->data);
    if (p->kind == NODE_STRING) WindowsDeleteString(*(HSTRING *)p->data);
}
static ULONG WINAPI node_inner_Release(struct node_iface *iface)
{
    struct control *impl = iface->owner;
    ULONG ref = InterlockedDecrement(&impl->ref);
    struct node_property *p;
    struct node_event *event;
    if (!ref)
    {
        if (focused == impl) focused = NULL;
        if (impl->window) ICoreWindow_Release(impl->window);
        if (impl->children) IVector_IInspectable_Release(impl->children);
        while ((p = impl->properties)) { impl->properties = p->next; property_clear(p); free(p->name); free(p); }
        while ((event = impl->events)) { impl->events = event->next; winrt_event_clear(&event->value); free(event->name); free(event); }
        free(impl);
    }
    return ref;
}
static HRESULT WINAPI node_QueryInterface(struct node_iface *iface, REFIID iid, void **out)
{ return IInspectable_QueryInterface(iface->owner->outer, iid, out); }
static ULONG WINAPI node_AddRef(struct node_iface *iface) { return IInspectable_AddRef(iface->owner->outer); }
static ULONG WINAPI node_Release(struct node_iface *iface) { return IInspectable_Release(iface->owner->outer); }
static HRESULT WINAPI node_GetIids(struct node_iface *iface, ULONG *count, IID **ids)
{ if (!count || !ids) return E_POINTER; *count = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI node_GetRuntimeClassName(struct node_iface *iface, HSTRING *out)
{ return WindowsCreateString(iface->owner->class_name, wcslen(iface->owner->class_name), out); }
static HRESULT WINAPI node_GetTrustLevel(struct node_iface *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI node_inner_QueryInterface(struct node_iface *iface, REFIID iid, void **out)
{
    struct control *impl = iface->owner;
    unsigned int i;
    if (!out) return E_POINTER;
    *out = NULL;
    TRACE("%s iid %s\n", debugstr_w(impl->class_name), debugstr_guid(iid));
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable))
    { *out = &impl->inner; node_inner_AddRef(&impl->inner); return S_OK; }
    if ((impl->mask & (1u << NODE_ISwapChainPanel)) && IsEqualGUID(iid, &native_panel_iid))
    { *out = &impl->native_panel; node_AddRef(*out); return S_OK; }
    for (i = 0; i < NODE_IFACE_COUNT; ++i)
        if ((impl->mask & (1u << i)) && IsEqualGUID(iid, node_iids[i]))
        { *out = &impl->interfaces[i]; node_AddRef(*out); return S_OK; }
    return E_NOINTERFACE;
}
#define NODE_BASE {node_QueryInterface, node_AddRef, node_Release, node_GetIids, node_GetRuntimeClassName, node_GetTrustLevel}
static const struct node_base_vtbl node_inner_vtbl = {node_inner_QueryInterface, node_inner_AddRef, node_inner_Release,
    node_GetIids, node_GetRuntimeClassName, node_GetTrustLevel};
static struct node_property *find_property(struct control *impl, const WCHAR *name)
{
    struct node_property *p;
    for (p = impl->properties; p; p = p->next) if (!wcscmp(p->name, name)) return p;
    return NULL;
}
static struct node_event *find_event(struct control *impl, const WCHAR *name, BOOL create)
{
    struct node_event *p;
    for (p = impl->events; p; p = p->next) if (!wcscmp(p->name, name)) return p;
    if (!create || !(p = calloc(1, sizeof(*p)))) return NULL;
    if (!(p->name = wcsdup(name))) { free(p); return NULL; }
    p->next = impl->events; impl->events = p;
    return p;
}
static HRESULT node_notify(struct control *impl, const WCHAR *name, void *args)
{
    struct node_event *p = find_event(impl, name, FALSE);
    return p ? winrt_event_notify(&p->value, impl->outer, args) : S_OK;
}
static HRESULT node_set(struct node_iface *iface, const WCHAR *name, enum node_value_kind kind, const void *value, SIZE_T size)
{
    struct control *impl = iface->owner;
    struct node_property *p = find_property(impl, name);
    HSTRING string = NULL;
    HRESULT hr;
    TRACE("%s put %s\n", debugstr_w(impl->class_name), debugstr_w(name));
    if (size > sizeof(p->data)) return E_INVALIDARG;
    if (kind == NODE_STRING && FAILED(hr = WindowsDuplicateString(*(HSTRING *)value, &string))) return hr;
    if (!p)
    {
        if (!(p = calloc(1, sizeof(*p))) || !(p->name = wcsdup(name)))
        { free(p); WindowsDeleteString(string); return E_OUTOFMEMORY; }
        p->next = impl->properties; impl->properties = p;
    }
    if (kind == NODE_OBJECT && *(IInspectable **)value) IInspectable_AddRef(*(IInspectable **)value);
    property_clear(p); p->kind = kind; p->size = size;
    if (kind == NODE_STRING) memcpy(p->data, &string, sizeof(string));
    else memcpy(p->data, value, size);
    if (!wcscmp(name, L"Text")) return node_text_changed(impl);
    return S_OK;
}
static HRESULT node_get(struct node_iface *iface, const WCHAR *name, enum node_value_kind kind, void *out, SIZE_T size)
{
    struct control *impl = iface->owner;
    struct node_property *p = find_property(impl, name);
    TRACE("%s get %s\n", debugstr_w(impl->class_name), debugstr_w(name));
    if (!out) return E_POINTER;
    memset(out, 0, size);
    if (p)
    {
        if (p->kind != kind || p->size != size) return E_UNEXPECTED;
        if (kind == NODE_STRING) return WindowsDuplicateString(*(HSTRING *)p->data, out);
        memcpy(out, p->data, size);
        if (kind == NODE_OBJECT && *(IInspectable **)out) IInspectable_AddRef(*(IInspectable **)out);
        return S_OK;
    }
    if (kind == NODE_OBJECT && !wcscmp(name, L"Dispatcher"))
    {
        ICoreWindow *window;
        HRESULT hr = xaml_get_core_window((IInspectable **)&window);
        if (FAILED(hr)) return hr;
        hr = ICoreWindow_get_Dispatcher(window, out);
        ICoreWindow_Release(window);
        return hr;
    }
    if (kind == NODE_DATA && size == sizeof(Size) && !wcscmp(name, L"RenderSize"))
    { *(Size *)out = impl->actual_size; return S_OK; }
    if (kind == NODE_DATA && size == sizeof(DOUBLE) &&
        (!wcscmp(name, L"ActualWidth") || !wcscmp(name, L"ActualHeight")))
    { *(DOUBLE *)out = !wcscmp(name, L"ActualWidth") ? impl->actual_size.Width : impl->actual_size.Height; return S_OK; }
    if (kind == NODE_OBJECT && !wcscmp(name, L"Children"))
    {
        HRESULT hr;
        if (!impl->children && FAILED(hr = vector_create(&children_iids, (void **)&impl->children))) return hr;
        *(IInspectable **)out = (IInspectable *)impl->children;
        IVector_IInspectable_AddRef(impl->children);
    }
    if (kind == NODE_DATA)
    {
        if (size == sizeof(DOUBLE))
        {
            if (!wcscmp(name, L"Width") || !wcscmp(name, L"Height")) *(DOUBLE *)out = NAN;
            if (!wcscmp(name, L"MaxWidth") || !wcscmp(name, L"MaxHeight")) *(DOUBLE *)out = INFINITY;
            if (!wcscmp(name, L"FontSize")) *(DOUBLE *)out = 14;
            if (!wcscmp(name, L"Opacity")) *(DOUBLE *)out = 1;
        }
        if (size == sizeof(FLOAT) && (!wcscmp(name, L"CompositionScaleX") || !wcscmp(name, L"CompositionScaleY"))) *(FLOAT *)out = 1;
        if (size == sizeof(boolean) && (!wcscmp(name, L"IsEnabled") || !wcscmp(name, L"IsHitTestVisible"))) *(boolean *)out = TRUE;
        if (size == sizeof(struct node_weight) && !wcscmp(name, L"FontWeight")) ((struct node_weight *)out)->weight = 400;
    }
    return S_OK;
}
static HRESULT node_event_add(struct node_iface *iface, const WCHAR *name, IInspectable *handler, EventRegistrationToken *token)
{
    struct node_event *event = find_event(iface->owner, name, TRUE);
    TRACE("%s add %s\n", debugstr_w(iface->owner->class_name), debugstr_w(name));
    return event ? winrt_event_add(&event->value, handler, token) : E_OUTOFMEMORY;
}
static HRESULT node_event_remove(struct node_iface *iface, const WCHAR *name, EventRegistrationToken token)
{
    struct node_event *event = find_event(iface->owner, name, FALSE);
    return event ? winrt_event_remove(&event->value, token) : S_OK;
}
static HRESULT node_focus(struct node_iface *iface, INT32 state, boolean *out)
{
    if (!out) return E_POINTER;
    *out = FALSE;
    if (state < 0 || state > 3) return E_INVALIDARG;
    focused = state ? iface->owner : NULL;
    node_set(iface, L"FocusState", NODE_DATA, &state, sizeof(state));
    *out = TRUE;
    return node_notify(iface->owner, state ? L"GotFocus" : L"LostFocus", NULL);
}
static HRESULT node_find_name(struct node_iface *iface, HSTRING name, IInspectable **out)
{
    struct node_property *p = find_property(iface->owner, L"Name");
    if (!out) return E_POINTER;
    *out = NULL;
    if (p && !wcscmp(WindowsGetStringRawBuffer(*(HSTRING *)p->data, NULL), WindowsGetStringRawBuffer(name, NULL)))
    { *out = iface->owner->outer; IInspectable_AddRef(*out); }
    return S_OK;
}
static HRESULT node_layout(struct node_iface *iface) { return node_notify(iface->owner, L"LayoutUpdated", NULL); }
static HRESULT node_select(struct node_iface *iface, INT32 start, INT32 length)
{
    if (start < 0 || length < 0) return E_INVALIDARG;
    node_set(iface, L"SelectionStart", NODE_DATA, &start, sizeof(start));
    node_set(iface, L"SelectionLength", NODE_DATA, &length, sizeof(length));
    return node_notify(iface->owner, L"SelectionChanged", NULL);
}
static HRESULT node_create_input(struct node_iface *iface, INT32 devices, IInspectable **out)
{ return xaml_input_create(iface->owner->window, devices, out); }
#include "controls_methods.h"

static HRESULT get_control_iface(IInspectable *object, struct node_iface **out);
static const GUID canvas_statics_iid = {0x40ce5c46,0x2962,0x446f,{0xaa,0xfb,0x4c,0xdc,0x48,0x69,0x39,0xc9}};
static HRESULT WINAPI panel_SetSwapChain(struct node_iface *iface, IUnknown *value)
{
    IDXGISwapChain *swapchain;
    DXGI_SWAP_CHAIN_DESC desc;
    ICoreWindowInterop *interop;
    IInspectable *window;
    HWND hwnd;
    HRESULT hr;
    TRACE("panel %p swapchain %p\n", iface, value);
    if (value)
    {
        if (FAILED(hr = IUnknown_QueryInterface(value, &IID_IDXGISwapChain, (void **)&swapchain))) return hr;
        hr = IDXGISwapChain_GetDesc(swapchain, &desc); IDXGISwapChain_Release(swapchain);
        if (FAILED(hr)) return hr;
        if (FAILED(hr = xaml_get_core_window(&window))) return hr;
        hr = IInspectable_QueryInterface(window, &IID_ICoreWindowInterop, (void **)&interop);
        IInspectable_Release(window);
        if (FAILED(hr)) return hr;
        hr = ICoreWindowInterop_get_WindowHandle(interop, &hwnd); ICoreWindowInterop_Release(interop);
        if (FAILED(hr)) return hr;
        if (desc.OutputWindow != hwnd) return E_NOTIMPL;
    }
    return node_set(iface, L"SwapChain", NODE_OBJECT, &value, sizeof(value));
}
static const struct
{
    HRESULT (WINAPI *QueryInterface)(struct node_iface *, REFIID, void **);
    ULONG (WINAPI *AddRef)(struct node_iface *);
    ULONG (WINAPI *Release)(struct node_iface *);
    HRESULT (WINAPI *SetSwapChain)(struct node_iface *, IUnknown *);
} panel_native_vtbl = {node_QueryInterface, node_AddRef, node_Release, panel_SetSwapChain};
struct control_factory
{
    IActivationFactory iface;
    struct { const void *lpVtbl; struct control_factory *owner; } create;
    const WCHAR *name;
    GUID create_iid;
    UINT primary, mask;
    struct { const void *lpVtbl; struct control_factory *owner; } statics;
};
static HRESULT control_create(struct control_factory *factory, IInspectable *outer, IInspectable **inner, IInspectable **out)
{
    struct control *impl;
    unsigned int i;
    if (!out) return E_POINTER;
    *out = NULL;
    if (inner) *inner = NULL;
    if (outer && !inner) return E_INVALIDARG;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    if (factory->primary == NODE_ISwapChainPanel) xaml_get_core_window((IInspectable **)&impl->window);
    impl->ref = 1; impl->class_name = factory->name;
    impl->primary = factory->primary; impl->mask = factory->mask;
    impl->native_panel.lpVtbl = &panel_native_vtbl; impl->native_panel.owner = impl;
    impl->inner.lpVtbl = &node_inner_vtbl; impl->inner.owner = impl;
    impl->outer = outer ? outer : (IInspectable *)&impl->inner;
    for (i = 0; i < NODE_IFACE_COUNT; ++i) { impl->interfaces[i].lpVtbl = node_vtables[i]; impl->interfaces[i].owner = impl; }
    *out = (IInspectable *)&impl->interfaces[impl->primary];
    if (inner) { *inner = (IInspectable *)&impl->inner; IInspectable_AddRef(*out); }
    return S_OK;
}
static HRESULT WINAPI control_factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct control_factory *impl = CONTAINING_RECORD(iface, struct control_factory, iface);
    if (!out) return E_POINTER;
    *out = NULL;
    TRACE("%s factory iid %s\n", debugstr_w(impl->name), debugstr_guid(iid));
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IActivationFactory) || IsEqualGUID(iid, &IID_IAgileObject)) *out = iface;
    else if (impl->primary == NODE_ICanvas && IsEqualGUID(iid, &canvas_statics_iid)) *out = &impl->statics;
    else if (IsEqualGUID(iid, &impl->create_iid)) *out = &impl->create;
    else return E_NOINTERFACE;
    return S_OK;
}
static ULONG WINAPI control_factory_AddRef(IActivationFactory *iface) { return 2; }
static ULONG WINAPI control_factory_Release(IActivationFactory *iface) { return 1; }
static HRESULT WINAPI control_factory_GetIids(IActivationFactory *iface, ULONG *count, IID **ids)
{ if (!count || !ids) return E_POINTER; *count = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI control_factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *name)
{
    const WCHAR *value = CONTAINING_RECORD(iface, struct control_factory, iface)->name;
    return WindowsCreateString(value, wcslen(value), name);
}
static HRESULT WINAPI control_factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }
static HRESULT WINAPI control_factory_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{ return control_create(CONTAINING_RECORD(iface, struct control_factory, iface), NULL, NULL, out); }
static const IActivationFactoryVtbl control_factory_vtbl = {control_factory_QueryInterface, control_factory_AddRef,
    control_factory_Release, control_factory_GetIids, control_factory_GetRuntimeClassName,
    control_factory_GetTrustLevel, control_factory_ActivateInstance};
struct create_iface { const void *lpVtbl; struct control_factory *owner; };
static HRESULT WINAPI create_QueryInterface(struct create_iface *iface, REFIID iid, void **out)
{ return control_factory_QueryInterface(&iface->owner->iface, iid, out); }
static ULONG WINAPI create_AddRef(struct create_iface *iface) { return 2; }
static ULONG WINAPI create_Release(struct create_iface *iface) { return 1; }
static HRESULT WINAPI create_GetIids(struct create_iface *iface, ULONG *count, IID **ids)
{ return control_factory_GetIids(&iface->owner->iface, count, ids); }
static HRESULT WINAPI create_GetRuntimeClassName(struct create_iface *iface, HSTRING *name)
{ return control_factory_GetRuntimeClassName(&iface->owner->iface, name); }
static HRESULT WINAPI create_GetTrustLevel(struct create_iface *iface, TrustLevel *value)
{ return control_factory_GetTrustLevel(&iface->owner->iface, value); }
static HRESULT WINAPI create_Instance(struct create_iface *iface, IInspectable *outer, IInspectable **inner, IInspectable **out)
{ return control_create(iface->owner, outer, inner, out); }
static const struct
{
    HRESULT (WINAPI *QueryInterface)(struct create_iface *, REFIID, void **);
    ULONG (WINAPI *AddRef)(struct create_iface *);
    ULONG (WINAPI *Release)(struct create_iface *);
    HRESULT (WINAPI *GetIids)(struct create_iface *, ULONG *, IID **);
    HRESULT (WINAPI *GetRuntimeClassName)(struct create_iface *, HSTRING *);
    HRESULT (WINAPI *GetTrustLevel)(struct create_iface *, TrustLevel *);
    HRESULT (WINAPI *CreateInstance)(struct create_iface *, IInspectable *, IInspectable **, IInspectable **);
} control_create_vtbl = {create_QueryInterface, create_AddRef, create_Release, create_GetIids,
    create_GetRuntimeClassName, create_GetTrustLevel, create_Instance};
static HRESULT WINAPI canvas_Property(struct create_iface *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
#define WIDEN_(s) L##s
#define WIDEN(s) WIDEN_(s)
#define CANVAS_POSITION(name, type) \
static HRESULT WINAPI canvas_Get##name(struct create_iface *iface, IInspectable *object, type *out) \
{ \
    struct node_iface *node; HRESULT hr; \
    if (!object || !out) return E_POINTER; \
    if (FAILED(hr = get_control_iface(object, &node))) return hr; \
    hr = node_get(node, L"Canvas." WIDEN(#name), NODE_DATA, out, sizeof(*out)); node_Release(node); return hr; \
} \
static HRESULT WINAPI canvas_Set##name(struct create_iface *iface, IInspectable *object, type value) \
{ \
    struct node_iface *node; HRESULT hr; \
    if (!object) return E_POINTER; \
    if (FAILED(hr = get_control_iface(object, &node))) return hr; \
    hr = node_set(node, L"Canvas." WIDEN(#name), NODE_DATA, &value, sizeof(value)); node_Release(node); return hr; \
}
CANVAS_POSITION(Left, DOUBLE)
CANVAS_POSITION(Top, DOUBLE)
CANVAS_POSITION(ZIndex, INT32)
static const struct
{
    HRESULT (WINAPI *QueryInterface)(struct create_iface *, REFIID, void **);
    ULONG (WINAPI *AddRef)(struct create_iface *);
    ULONG (WINAPI *Release)(struct create_iface *);
    HRESULT (WINAPI *GetIids)(struct create_iface *, ULONG *, IID **);
    HRESULT (WINAPI *GetRuntimeClassName)(struct create_iface *, HSTRING *);
    HRESULT (WINAPI *GetTrustLevel)(struct create_iface *, TrustLevel *);
    HRESULT (WINAPI *LeftProperty)(struct create_iface *, IInspectable **);
    HRESULT (WINAPI *GetLeft)(struct create_iface *, IInspectable *, DOUBLE *);
    HRESULT (WINAPI *SetLeft)(struct create_iface *, IInspectable *, DOUBLE);
    HRESULT (WINAPI *TopProperty)(struct create_iface *, IInspectable **);
    HRESULT (WINAPI *GetTop)(struct create_iface *, IInspectable *, DOUBLE *);
    HRESULT (WINAPI *SetTop)(struct create_iface *, IInspectable *, DOUBLE);
    HRESULT (WINAPI *ZIndexProperty)(struct create_iface *, IInspectable **);
    HRESULT (WINAPI *GetZIndex)(struct create_iface *, IInspectable *, INT32 *);
    HRESULT (WINAPI *SetZIndex)(struct create_iface *, IInspectable *, INT32);
} canvas_statics_vtbl = {create_QueryInterface, create_AddRef, create_Release, create_GetIids,
    create_GetRuntimeClassName, create_GetTrustLevel, canvas_Property, canvas_GetLeft, canvas_SetLeft,
    canvas_Property, canvas_GetTop, canvas_SetTop, canvas_Property, canvas_GetZIndex, canvas_SetZIndex};
static HRESULT WINAPI font_Create(struct create_iface *iface, HSTRING name, IInspectable *outer, IInspectable **inner, IInspectable **out)
{
    HRESULT hr = control_create(iface->owner, outer, inner, out);
    if (SUCCEEDED(hr)) hr = node_set((struct node_iface *)*out, L"Source", NODE_STRING, &name, sizeof(name));
    return hr;
}
static HRESULT WINAPI brush_Create(struct create_iface *iface, Color color, IInspectable **out)
{
    HRESULT hr = control_create(iface->owner, NULL, NULL, out);
    if (SUCCEEDED(hr)) hr = node_set((struct node_iface *)*out, L"Color", NODE_DATA, &color, sizeof(color));
    return hr;
}
#define CREATE_BASE create_QueryInterface, create_AddRef, create_Release, create_GetIids, create_GetRuntimeClassName, create_GetTrustLevel
struct font_create_vtbl
{
    struct node_base_vtbl base;
    HRESULT (WINAPI *Create)(struct create_iface *, HSTRING, IInspectable *, IInspectable **, IInspectable **);
};
/* Factory interfaces share IInspectable's six ABI slots. */
static const struct { void *base[6]; HRESULT (WINAPI *Create)(struct create_iface *, HSTRING, IInspectable *, IInspectable **, IInspectable **); }
font_create_vtbl = {{CREATE_BASE}, font_Create};
static const struct { void *base[6]; HRESULT (WINAPI *Create)(struct create_iface *, Color, IInspectable **); }
brush_create_vtbl = {{CREATE_BASE}, brush_Create};
static struct control_factory control_factories[] = {
    {{&control_factory_vtbl}, {&control_create_vtbl, &control_factories[0]}, L"Windows.UI.Xaml.Controls.Page", {0xdfa149ac,0x1849,0x445e,{0x93,0x7c,0x40,0xa9,0x59,0x0c,0xc0,0x76}}, NODE_IPage, (1u << NODE_IDependencyObject) | (1u << NODE_IUIElement) | (1u << NODE_IFrameworkElement) | (1u << NODE_IControl) | (1u << NODE_IUserControl) | (1u << NODE_IPage)},
    {{&control_factory_vtbl}, {&control_create_vtbl, &control_factories[1]}, L"Windows.UI.Xaml.Controls.UserControl", {0x38b1ed92,0xa28a,0x4972,{0x93,0xdf,0xf4,0xf7,0x59,0xb8,0xaf,0xd2}}, NODE_IUserControl, (1u << NODE_IDependencyObject) | (1u << NODE_IUIElement) | (1u << NODE_IFrameworkElement) | (1u << NODE_IControl) | (1u << NODE_IUserControl)},
    {{&control_factory_vtbl}, {&control_create_vtbl, &control_factories[2]}, L"Windows.UI.Xaml.Controls.Canvas", {0x1b328bd1,0xb400,0x4a8e,{0x94,0x3b,0x5a,0xd2,0xc4,0x5b,0xe0,0xdf}}, NODE_ICanvas, (1u << NODE_IDependencyObject) | (1u << NODE_IUIElement) | (1u << NODE_IFrameworkElement) | (1u << NODE_IPanel) | (1u << NODE_ICanvas), {&canvas_statics_vtbl, &control_factories[2]}},
    {{&control_factory_vtbl}, {&control_create_vtbl, &control_factories[3]}, L"Windows.UI.Xaml.Controls.SwapChainPanel", {0xf38f8d7f,0x1a48,0x49cb,{0x86,0xd2,0x10,0xea,0xaa,0xf6,0xfd,0x70}}, NODE_ISwapChainPanel, (1u << NODE_IDependencyObject) | (1u << NODE_IUIElement) | (1u << NODE_IFrameworkElement) | (1u << NODE_IPanel) | (1u << NODE_ISwapChainPanel)},
    {{&control_factory_vtbl}, {&control_create_vtbl, &control_factories[4]}, L"Windows.UI.Xaml.Controls.TextBox", {0x710e4278,0x8529,0x47d3,{0x8d,0x8e,0x30,0x7e,0x34,0xcf,0xf0,0x81}}, NODE_ITextBox, (1u << NODE_IDependencyObject) | (1u << NODE_IUIElement) | (1u << NODE_IFrameworkElement) | (1u << NODE_IControl) | (1u << NODE_ITextBox2) | (1u << NODE_ITextBox)},
    {{&control_factory_vtbl}, {&control_create_vtbl, &control_factories[5]}, L"Windows.UI.Xaml.Controls.Button", {0x80a13c19,0x843a,0x451c,{0x8c,0xf5,0x44,0xc7,0x01,0xb0,0xe2,0x16}}, NODE_IButton, (1u << NODE_IDependencyObject) | (1u << NODE_IUIElement) | (1u << NODE_IFrameworkElement) | (1u << NODE_IControl) | (1u << NODE_IContentControl) | (1u << NODE_IButton)},
    {{&control_factory_vtbl}, {&font_create_vtbl, &control_factories[6]}, L"Windows.UI.Xaml.Media.FontFamily", {0xd5603377,0x3dae,0x4dcd,{0xaf,0x09,0xf9,0x49,0x8e,0x9e,0xc6,0x59}}, NODE_IFontFamily, (1u << NODE_IDependencyObject) | (1u << NODE_IFontFamily)},
    {{&control_factory_vtbl}, {&brush_create_vtbl, &control_factories[7]}, L"Windows.UI.Xaml.Media.SolidColorBrush", {0xd935ce0c,0x86f5,0x4da6,{0x8a,0x27,0xb1,0x61,0x9e,0xf7,0xf9,0x2b}}, NODE_ISolidColorBrush, (1u << NODE_IDependencyObject) | (1u << NODE_IBrush) | (1u << NODE_ISolidColorBrush)},
};
HRESULT xaml_control_factory(const WCHAR *name, IActivationFactory **out)
{
    unsigned int i;
    *out = NULL;
    for (i = 0; i < ARRAY_SIZE(control_factories); ++i)
        if (!wcscmp(name, control_factories[i].name)) { *out = &control_factories[i].iface; return S_OK; }
    return CLASS_E_CLASSNOTAVAILABLE;
}

static HRESULT get_control_iface(IInspectable *object, struct node_iface **out)
{
    HRESULT hr = IInspectable_QueryInterface(object, &node_iid_IDependencyObject, (void **)out);
    if (SUCCEEDED(hr) && (*out)->lpVtbl != node_vtables[NODE_IDependencyObject])
    { IInspectable_Release((IInspectable *)*out); *out = NULL; return E_NOINTERFACE; }
    return hr;
}
static HRESULT control_set(IInspectable *object, const WCHAR *name, enum node_value_kind kind,
        const void *value, SIZE_T size)
{
    struct node_iface *iface;
    HRESULT hr = get_control_iface(object, &iface);
    if (FAILED(hr)) return hr;
    hr = node_set(iface, name, kind, value, size);
    node_Release(iface);
    return hr;
}
HRESULT xaml_control_set_string(IInspectable *object, const WCHAR *name, HSTRING value)
{ return control_set(object, name, NODE_STRING, &value, sizeof(value)); }
HRESULT xaml_control_set_double(IInspectable *object, const WCHAR *name, DOUBLE value)
{ return control_set(object, name, NODE_DATA, &value, sizeof(value)); }
HRESULT xaml_control_set_int(IInspectable *object, const WCHAR *name, INT32 value)
{ return control_set(object, name, NODE_DATA, &value, sizeof(value)); }
HRESULT xaml_control_set_content(IInspectable *object, IInspectable *value)
{ return control_set(object, L"Content", NODE_OBJECT, &value, sizeof(value)); }
HRESULT xaml_control_add_child(IInspectable *object, IInspectable *value)
{
    struct node_iface *iface;
    IVector_IInspectable *children;
    HRESULT hr = get_control_iface(object, &iface);
    if (FAILED(hr)) return hr;
    if (!(iface->owner->mask & (1u << NODE_IPanel))) hr = E_NOINTERFACE;
    else if (SUCCEEDED(hr = node_get(iface, L"Children", NODE_OBJECT, &children, sizeof(children))))
    { hr = IVector_IInspectable_Append(children, value); IVector_IInspectable_Release(children); }
    node_Release(iface);
    return hr;
}

struct size_args
{
    ISizeChangedEventArgs ISizeChangedEventArgs_iface;
    IRoutedEventArgs IRoutedEventArgs_iface;
    LONG ref;
    IInspectable *source;
    Size old_size, new_size;
    BOOL text_changed;
};
static const GUID text_args_iid = {0x4dd04f7d,0x7a11,0x4b2e,{0x99,0x33,0x57,0x7d,0xf3,0x92,0x52,0xb6}};
static HRESULT WINAPI size_QueryInterface(ISizeChangedEventArgs *iface, REFIID iid, void **out)
{
    struct size_args *impl = CONTAINING_RECORD(iface, struct size_args, ISizeChangedEventArgs_iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, impl->text_changed ? &text_args_iid : &IID_ISizeChangedEventArgs)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IRoutedEventArgs)) *out = &impl->IRoutedEventArgs_iface;
    else return E_NOINTERFACE;
    ISizeChangedEventArgs_AddRef(iface); return S_OK;
}
static ULONG WINAPI size_AddRef(ISizeChangedEventArgs *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct size_args, ISizeChangedEventArgs_iface)->ref); }
static ULONG WINAPI size_Release(ISizeChangedEventArgs *iface)
{
    struct size_args *impl = CONTAINING_RECORD(iface, struct size_args, ISizeChangedEventArgs_iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { IInspectable_Release(impl->source); free(impl); }
    return ref;
}
static HRESULT WINAPI size_GetIids(ISizeChangedEventArgs *iface, ULONG *count, IID **ids)
{ if (!count || !ids) return E_POINTER; *count = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI size_GetRuntimeClassName(ISizeChangedEventArgs *iface, HSTRING *out)
{
    const WCHAR *name = CONTAINING_RECORD(iface, struct size_args, ISizeChangedEventArgs_iface)->text_changed ?
        L"Windows.UI.Xaml.Controls.TextChangedEventArgs" : L"Windows.UI.Xaml.SizeChangedEventArgs";
    return WindowsCreateString(name, wcslen(name), out);
}
static HRESULT WINAPI size_GetTrustLevel(ISizeChangedEventArgs *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI size_get_PreviousSize(ISizeChangedEventArgs *iface, Size *out)
{ if (!out) return E_POINTER; *out = CONTAINING_RECORD(iface, struct size_args, ISizeChangedEventArgs_iface)->old_size; return S_OK; }
static HRESULT WINAPI size_get_NewSize(ISizeChangedEventArgs *iface, Size *out)
{ if (!out) return E_POINTER; *out = CONTAINING_RECORD(iface, struct size_args, ISizeChangedEventArgs_iface)->new_size; return S_OK; }
static const ISizeChangedEventArgsVtbl size_vtbl = {size_QueryInterface, size_AddRef, size_Release, size_GetIids,
    size_GetRuntimeClassName, size_GetTrustLevel, size_get_PreviousSize, size_get_NewSize};
DEFINE_IINSPECTABLE(routed, IRoutedEventArgs, struct size_args, ISizeChangedEventArgs_iface)
static HRESULT WINAPI routed_get_OriginalSource(IRoutedEventArgs *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = impl_from_IRoutedEventArgs(iface)->source; IInspectable_AddRef(*out); return S_OK; }
static const IRoutedEventArgsVtbl routed_vtbl = {routed_QueryInterface, routed_AddRef, routed_Release, routed_GetIids,
    routed_GetRuntimeClassName, routed_GetTrustLevel, routed_get_OriginalSource};
HRESULT xaml_control_layout(IInspectable *object, Size available)
{
    struct node_iface *iface;
    struct control *impl;
    struct node_property *p;
    struct size_args *args;
    IInspectable *child;
    Size size = available;
    UINT32 count, i;
    BOOL loaded;
    HRESULT hr = get_control_iface(object, &iface);
    if (FAILED(hr)) return hr;
    impl = iface->owner;
    if ((p = find_property(impl, L"Width")) && p->size == sizeof(DOUBLE) && isfinite(*(DOUBLE *)p->data)) size.Width = *(DOUBLE *)p->data;
    if ((p = find_property(impl, L"Height")) && p->size == sizeof(DOUBLE) && isfinite(*(DOUBLE *)p->data)) size.Height = *(DOUBLE *)p->data;
    loaded = impl->laid_out;
    if (!loaded || memcmp(&size, &impl->actual_size, sizeof(size)))
    {
        if (!(args = calloc(1, sizeof(*args)))) { node_Release(iface); return E_OUTOFMEMORY; }
        args->ISizeChangedEventArgs_iface.lpVtbl = &size_vtbl;
        args->IRoutedEventArgs_iface.lpVtbl = &routed_vtbl;
        args->ref = 1; args->source = impl->outer; IInspectable_AddRef(args->source);
        args->old_size = impl->actual_size; args->new_size = size;
        impl->actual_size = size; impl->laid_out = TRUE;
        hr = node_notify(impl, L"SizeChanged", &args->ISizeChangedEventArgs_iface);
        if (SUCCEEDED(hr) && !loaded) hr = node_notify(impl, L"Loaded", &args->IRoutedEventArgs_iface);
        ISizeChangedEventArgs_Release(&args->ISizeChangedEventArgs_iface);
    }
    if (SUCCEEDED(hr) && (p = find_property(impl, L"Content")) && p->kind == NODE_OBJECT && (child = *(IInspectable **)p->data))
        hr = xaml_control_layout(child, size);
    if (SUCCEEDED(hr) && impl->children && SUCCEEDED(hr = IVector_IInspectable_get_Size(impl->children, &count)))
        for (i = 0; i < count && SUCCEEDED(hr); ++i)
        {
            if (SUCCEEDED(hr = IVector_IInspectable_GetAt(impl->children, i, &child)))
            { hr = xaml_control_layout(child, size); IInspectable_Release(child); }
        }
    node_Release(iface);
    return hr;
}

static HRESULT node_text_changed(struct control *impl)
{
    struct size_args *args = calloc(1, sizeof(*args));
    HRESULT hr;
    if (!args) return E_OUTOFMEMORY;
    args->ISizeChangedEventArgs_iface.lpVtbl = &size_vtbl;
    args->IRoutedEventArgs_iface.lpVtbl = &routed_vtbl;
    args->ref = 1; args->text_changed = TRUE;
    args->source = impl->outer; IInspectable_AddRef(args->source);
    hr = node_notify(impl, L"TextChanged", &args->ISizeChangedEventArgs_iface);
    ISizeChangedEventArgs_Release(&args->ISizeChangedEventArgs_iface);
    return hr;
}
boolean xaml_control_character(UINT32 character)
{
    struct control *impl = focused;
    struct node_iface *iface;
    HSTRING old = NULL, text = NULL;
    const WCHAR *value;
    WCHAR units[2], *buffer;
    UINT32 length, count = 0;
    INT32 start = 0, selected = 0, maximum = 0;
    boolean readonly = FALSE, multiline = FALSE;
    HRESULT hr;
    if (!impl || !(impl->mask & (1u << NODE_ITextBox))) return FALSE;
    iface = &impl->interfaces[NODE_ITextBox]; node_AddRef(iface);
    node_get(iface, L"IsReadOnly", NODE_DATA, &readonly, sizeof(readonly));
    if (readonly) { node_Release(iface); return FALSE; }
    if (FAILED(node_get(iface, L"Text", NODE_STRING, &old, sizeof(old)))) { node_Release(iface); return FALSE; }
    value = WindowsGetStringRawBuffer(old, &length);
    node_get(iface, L"SelectionStart", NODE_DATA, &start, sizeof(start));
    node_get(iface, L"SelectionLength", NODE_DATA, &selected, sizeof(selected));
    node_get(iface, L"MaxLength", NODE_DATA, &maximum, sizeof(maximum));
    start = min(max(start, 0), length); selected = min(max(selected, 0), length - start);
    if (character == 1) { node_select(iface, 0, length); goto done; }
    if (character == 8)
    {
        if (!selected && start) { --start; selected = 1; if (start && value[start] >= 0xdc00 && value[start] <= 0xdfff && value[start - 1] >= 0xd800 && value[start - 1] <= 0xdbff) { --start; ++selected; } }
    }
    else
    {
        if (character == 13)
        {
            node_get(iface, L"AcceptsReturn", NODE_DATA, &multiline, sizeof(multiline));
            if (!multiline) { WindowsDeleteString(old); node_Release(iface); return FALSE; }
        }
        else if (character < 32 || character > 0x10ffff) { WindowsDeleteString(old); node_Release(iface); return FALSE; }
        if (character > 0xffff) { character -= 0x10000; units[count++] = 0xd800 + (character >> 10); units[count++] = 0xdc00 + (character & 0x3ff); }
        else units[count++] = character;
    }
    if (maximum > 0 && length - selected + count > maximum) goto done;
    if (!(buffer = malloc((length - selected + count + 1) * sizeof(*buffer)))) goto done;
    memcpy(buffer, value, start * sizeof(*buffer));
    memcpy(buffer + start, units, count * sizeof(*buffer));
    memcpy(buffer + start + count, value + start + selected, (length - start - selected) * sizeof(*buffer));
    hr = WindowsCreateString(buffer, length - selected + count, &text); free(buffer);
    if (SUCCEEDED(hr))
    {
        node_select(iface, start + count, 0);
        node_set(iface, L"Text", NODE_STRING, &text, sizeof(text));
        WindowsDeleteString(text);
    }
done:
    WindowsDeleteString(old); node_Release(iface); return TRUE;
}
