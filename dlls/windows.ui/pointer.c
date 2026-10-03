/* Mouse-backed CoreWindow pointer event snapshots. */
#include "private.h"
#include "winuser.h"

struct pointer_point
{
    IPointerPoint point;
    IPointerPointProperties properties;
    IPointerDevice device;
    LONG ref;
    Point position;
    UINT32 frame;
    UINT64 timestamp;
    UINT buttons;
    INT32 wheel;
    boolean horizontal, canceled;
    PointerUpdateKind update;
};

static ULONG snapshot_addref(struct pointer_point *point) { return InterlockedIncrement(&point->ref); }
static ULONG snapshot_release(struct pointer_point *point)
{ ULONG ref = InterlockedDecrement(&point->ref); if (!ref) free(point); return ref; }

/* Point, properties, and device have separate COM identities, but share the immutable snapshot lifetime. */
#define POINT_BASE(prefix, type, member, classname) \
static struct pointer_point *prefix##_impl(type *iface) { return CONTAINING_RECORD(iface, struct pointer_point, member); } \
static HRESULT WINAPI prefix##_qi(type *iface, REFIID iid, void **out) \
{ \
    if (!out) return E_POINTER; \
    *out = NULL; \
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) && \
        !IsEqualGUID(iid, &IID_IAgileObject) && !IsEqualGUID(iid, &IID_##type)) return E_NOINTERFACE; \
    *out = iface; snapshot_addref(prefix##_impl(iface)); return S_OK; \
} \
static ULONG WINAPI prefix##_addref(type *iface) { return snapshot_addref(prefix##_impl(iface)); } \
static ULONG WINAPI prefix##_release(type *iface) { return snapshot_release(prefix##_impl(iface)); } \
static HRESULT WINAPI prefix##_iids(type *iface, ULONG *count, IID **iids) \
{ \
    if (!count || !iids) return E_POINTER; \
    *count = 0; if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY; \
    **iids = IID_##type; *count = 1; return S_OK; \
} \
static HRESULT WINAPI prefix##_name(type *iface, HSTRING *name) \
{ const WCHAR *str = classname; return WindowsCreateString(str, wcslen(str), name); } \
static HRESULT WINAPI prefix##_trust(type *iface, TrustLevel *level) \
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }
POINT_BASE(point, IPointerPoint, point, L"Windows.UI.Input.PointerPoint")
POINT_BASE(props, IPointerPointProperties, properties, L"Windows.UI.Input.PointerPointProperties")
POINT_BASE(device, IPointerDevice, device, L"Windows.Devices.Input.PointerDevice")
#define GETTER(prefix, iface_type, name, value_type, expression) \
static HRESULT WINAPI prefix##_##name(iface_type *iface, value_type *value) \
{ if (!value) return E_POINTER; *value = expression; return S_OK; }

static HRESULT WINAPI point_device(IPointerPoint *iface, IPointerDevice **out)
{ if (!out) return E_POINTER; *out = &point_impl(iface)->device; snapshot_addref(point_impl(iface)); return S_OK; }
GETTER(point, IPointerPoint, position, Point, point_impl(iface)->position)
GETTER(point, IPointerPoint, id, UINT32, 1)
GETTER(point, IPointerPoint, frame, UINT32, point_impl(iface)->frame)
GETTER(point, IPointerPoint, timestamp, UINT64, point_impl(iface)->timestamp)
GETTER(point, IPointerPoint, contact, boolean, !!point_impl(iface)->buttons)
static HRESULT WINAPI point_properties(IPointerPoint *iface, IPointerPointProperties **out)
{ if (!out) return E_POINTER; *out = &point_impl(iface)->properties; snapshot_addref(point_impl(iface)); return S_OK; }
static const IPointerPointVtbl point_vtbl = {point_qi, point_addref, point_release, point_iids, point_name, point_trust,
    point_device, point_position, point_position, point_id, point_frame, point_timestamp, point_contact, point_properties};
GETTER(props, IPointerPointProperties, Pressure, FLOAT, 0.5f)
GETTER(props, IPointerPointProperties, IsInverted, boolean, FALSE)
GETTER(props, IPointerPointProperties, IsEraser, boolean, FALSE)
GETTER(props, IPointerPointProperties, Orientation, FLOAT, 0.0f)
GETTER(props, IPointerPointProperties, XTilt, FLOAT, 0.0f)
GETTER(props, IPointerPointProperties, YTilt, FLOAT, 0.0f)
GETTER(props, IPointerPointProperties, Twist, FLOAT, 0.0f)
GETTER(props, IPointerPointProperties, ContactRect, Rect, ((Rect){props_impl(iface)->position.X, props_impl(iface)->position.Y, 0, 0}))
GETTER(props, IPointerPointProperties, ContactRectRaw, Rect, ((Rect){props_impl(iface)->position.X, props_impl(iface)->position.Y, 0, 0}))
GETTER(props, IPointerPointProperties, TouchConfidence, boolean, TRUE)
GETTER(props, IPointerPointProperties, IsLeftButtonPressed, boolean, !!(props_impl(iface)->buttons & MK_LBUTTON))
GETTER(props, IPointerPointProperties, IsRightButtonPressed, boolean, !!(props_impl(iface)->buttons & MK_RBUTTON))
GETTER(props, IPointerPointProperties, IsMiddleButtonPressed, boolean, !!(props_impl(iface)->buttons & MK_MBUTTON))
GETTER(props, IPointerPointProperties, MouseWheelDelta, INT32, props_impl(iface)->wheel)
GETTER(props, IPointerPointProperties, IsHorizontalMouseWheel, boolean, props_impl(iface)->horizontal)
GETTER(props, IPointerPointProperties, IsPrimary, boolean, TRUE)
GETTER(props, IPointerPointProperties, IsInRange, boolean, TRUE)
GETTER(props, IPointerPointProperties, IsCanceled, boolean, props_impl(iface)->canceled)
GETTER(props, IPointerPointProperties, IsBarrelButtonPressed, boolean, FALSE)
GETTER(props, IPointerPointProperties, IsXButton1Pressed, boolean, !!(props_impl(iface)->buttons & MK_XBUTTON1))
GETTER(props, IPointerPointProperties, IsXButton2Pressed, boolean, !!(props_impl(iface)->buttons & MK_XBUTTON2))
GETTER(props, IPointerPointProperties, PointerUpdateKind, PointerUpdateKind, props_impl(iface)->update)
static HRESULT WINAPI props_has_usage(IPointerPointProperties *iface, UINT32 page, UINT32 id, boolean *out)
{ if (!out) return E_POINTER; *out = FALSE; return S_OK; }
static HRESULT WINAPI props_usage(IPointerPointProperties *iface, UINT32 page, UINT32 id, INT32 *out)
{ if (!out) return E_POINTER; *out = 0; return E_NOTIMPL; }
static const IPointerPointPropertiesVtbl props_vtbl = {props_qi, props_addref, props_release, props_iids, props_name, props_trust,
    props_Pressure,
    props_IsInverted,
    props_IsEraser,
    props_Orientation,
    props_XTilt,
    props_YTilt,
    props_Twist,
    props_ContactRect,
    props_ContactRectRaw,
    props_TouchConfidence,
    props_IsLeftButtonPressed,
    props_IsRightButtonPressed,
    props_IsMiddleButtonPressed,
    props_MouseWheelDelta,
    props_IsHorizontalMouseWheel,
    props_IsPrimary,
    props_IsInRange,
    props_IsCanceled,
    props_IsBarrelButtonPressed,
    props_IsXButton1Pressed,
    props_IsXButton2Pressed,
    props_PointerUpdateKind,
    props_has_usage, props_usage};
GETTER(device, IPointerDevice, type, PointerDeviceType, PointerDeviceType_Mouse)
GETTER(device, IPointerDevice, integrated, boolean, FALSE)
GETTER(device, IPointerDevice, contacts, UINT32, 1)
static HRESULT WINAPI device_rect(IPointerDevice *iface, Rect *out)
{
    if (!out) return E_POINTER;
    *out = (Rect){GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                  GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)};
    return S_OK;
}
static HRESULT WINAPI device_usages(IPointerDevice *iface, IVectorView_PointerDeviceUsage **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IPointerDeviceVtbl device_vtbl = {device_qi, device_addref, device_release, device_iids, device_name, device_trust,
    device_type, device_integrated, device_contacts, device_rect, device_rect, device_usages};

struct pointer_args
{
    IPointerEventArgs IPointerEventArgs_iface;
    ICoreWindowEventArgs ICoreWindowEventArgs_iface;
    LONG ref;
    struct pointer_point *point;
    VirtualKeyModifiers modifiers;
    boolean handled;
};
static HRESULT WINAPI args_QueryInterface(IPointerEventArgs *iface, REFIID iid, void **out)
{
    struct pointer_args *args = CONTAINING_RECORD(iface, struct pointer_args, IPointerEventArgs_iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IPointerEventArgs)) *out = iface;
    else if (IsEqualGUID(iid, &IID_ICoreWindowEventArgs)) *out = &args->ICoreWindowEventArgs_iface;
    else return E_NOINTERFACE;
    IPointerEventArgs_AddRef(iface); return S_OK;
}
static ULONG WINAPI args_AddRef(IPointerEventArgs *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct pointer_args, IPointerEventArgs_iface)->ref); }
static ULONG WINAPI args_Release(IPointerEventArgs *iface)
{
    struct pointer_args *args = CONTAINING_RECORD(iface, struct pointer_args, IPointerEventArgs_iface);
    ULONG ref = InterlockedDecrement(&args->ref);
    if (!ref) { snapshot_release(args->point); free(args); }
    return ref;
}
static HRESULT WINAPI args_GetIids(IPointerEventArgs *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0; if (!(*iids = CoTaskMemAlloc(2*sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IPointerEventArgs; (*iids)[1] = IID_ICoreWindowEventArgs; *count = 2; return S_OK;
}
static HRESULT WINAPI args_GetRuntimeClassName(IPointerEventArgs *iface, HSTRING *name)
{ const WCHAR *str = L"Windows.UI.Core.PointerEventArgs"; return WindowsCreateString(str, wcslen(str), name); }
static HRESULT WINAPI args_GetTrustLevel(IPointerEventArgs *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }
static HRESULT WINAPI args_point(IPointerEventArgs *iface, IPointerPoint **out)
{
    struct pointer_args *args = CONTAINING_RECORD(iface, struct pointer_args, IPointerEventArgs_iface);
    if (!out) return E_POINTER;
    *out = &args->point->point; snapshot_addref(args->point); return S_OK;
}
static HRESULT WINAPI args_modifiers(IPointerEventArgs *iface, VirtualKeyModifiers *out)
{
    if (!out) return E_POINTER;
    *out = CONTAINING_RECORD(iface, struct pointer_args, IPointerEventArgs_iface)->modifiers; return S_OK;
}
static HRESULT WINAPI args_intermediate(IPointerEventArgs *iface, IVector_PointerPoint **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IPointerEventArgsVtbl args_vtbl = {args_QueryInterface, args_AddRef, args_Release, args_GetIids,
    args_GetRuntimeClassName, args_GetTrustLevel, args_point, args_modifiers, args_intermediate};
DEFINE_IINSPECTABLE(event_args, ICoreWindowEventArgs, struct pointer_args, IPointerEventArgs_iface)
static HRESULT WINAPI args_get_handled(ICoreWindowEventArgs *iface, boolean *out)
{ if (!out) return E_POINTER; *out = impl_from_ICoreWindowEventArgs(iface)->handled; return S_OK; }
static HRESULT WINAPI args_put_handled(ICoreWindowEventArgs *iface, boolean value)
{ impl_from_ICoreWindowEventArgs(iface)->handled = value; return S_OK; }
static const ICoreWindowEventArgsVtbl event_vtbl = {event_args_QueryInterface, event_args_AddRef, event_args_Release,
    event_args_GetIids, event_args_GetRuntimeClassName, event_args_GetTrustLevel, args_get_handled, args_put_handled};

HRESULT pointer_args_create(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, IPointerEventArgs **out)
{
    static LONG frame;
    struct pointer_args *args;
    struct pointer_point *point;
    LARGE_INTEGER counter, frequency;
    POINT pos = {(short)LOWORD(lparam), (short)HIWORD(lparam)};
    float scale = 96.0f / GetDpiForWindow(hwnd);
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(args = calloc(1, sizeof(*args)))) return E_OUTOFMEMORY;
    if (!(point = calloc(1, sizeof(*point)))) { free(args); return E_OUTOFMEMORY; }
    point->point.lpVtbl = &point_vtbl; point->properties.lpVtbl = &props_vtbl;
    point->device.lpVtbl = &device_vtbl; point->ref = 1;
    args->IPointerEventArgs_iface.lpVtbl = &args_vtbl; args->ICoreWindowEventArgs_iface.lpVtbl = &event_vtbl;
    args->point = point; args->ref = 1;
    if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) ScreenToClient(hwnd, &pos);
    if (msg == WM_MOUSELEAVE || msg == WM_CAPTURECHANGED)
    {
        GetCursorPos(&pos); ScreenToClient(hwnd, &pos);
        wparam = 0;
        if (GetKeyState(VK_LBUTTON) & 0x8000) wparam |= MK_LBUTTON;
        if (GetKeyState(VK_RBUTTON) & 0x8000) wparam |= MK_RBUTTON;
        if (GetKeyState(VK_MBUTTON) & 0x8000) wparam |= MK_MBUTTON;
    }
    point->position = (Point){pos.x * scale, pos.y * scale};
    point->buttons = LOWORD(wparam) & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2);
    point->frame = InterlockedIncrement(&frame);
    QueryPerformanceCounter(&counter); QueryPerformanceFrequency(&frequency);
    point->timestamp = counter.QuadPart / frequency.QuadPart * 1000000 +
                       counter.QuadPart % frequency.QuadPart * 1000000 / frequency.QuadPart;
    point->canceled = msg == WM_CAPTURECHANGED;
    if (wparam & MK_CONTROL) args->modifiers |= VirtualKeyModifiers_Control;
    if (wparam & MK_SHIFT) args->modifiers |= VirtualKeyModifiers_Shift;
    if (GetKeyState(VK_MENU) & 0x8000) args->modifiers |= VirtualKeyModifiers_Menu;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) args->modifiers |= VirtualKeyModifiers_Windows;
    switch (msg)
    {
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: point->update = PointerUpdateKind_LeftButtonPressed; break;
    case WM_LBUTTONUP: point->update = PointerUpdateKind_LeftButtonReleased; break;
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: point->update = PointerUpdateKind_RightButtonPressed; break;
    case WM_RBUTTONUP: point->update = PointerUpdateKind_RightButtonReleased; break;
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: point->update = PointerUpdateKind_MiddleButtonPressed; break;
    case WM_MBUTTONUP: point->update = PointerUpdateKind_MiddleButtonReleased; break;
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        point->update = HIWORD(wparam) == XBUTTON1 ? PointerUpdateKind_XButton1Pressed : PointerUpdateKind_XButton2Pressed; break;
    case WM_XBUTTONUP:
        point->update = HIWORD(wparam) == XBUTTON1 ? PointerUpdateKind_XButton1Released : PointerUpdateKind_XButton2Released; break;
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        point->wheel = (short)HIWORD(wparam); point->horizontal = msg == WM_MOUSEHWHEEL; break;
    }
    *out = &args->IPointerEventArgs_iface;
    return S_OK;
}
