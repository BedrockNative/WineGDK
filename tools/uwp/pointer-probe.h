/* Checks native mouse messages are delivered as WinRT pointer snapshots. */
struct pointer_probe
{
    ITypedEventHandler_CoreWindow_PointerEventArgs iface;
    LONG ref;
    unsigned int calls;
    PointerUpdateKind kind;
    boolean pressed, horizontal;
    INT32 wheel;
    Point position;
    IPointerPoint *saved;
};
static HRESULT WINAPI pointer_probe_qi(ITypedEventHandler_CoreWindow_PointerEventArgs *iface, REFIID iid, void **out)
{ (void)iid; *out = iface; ITypedEventHandler_CoreWindow_PointerEventArgs_AddRef(iface); return S_OK; }
static ULONG WINAPI pointer_probe_addref(ITypedEventHandler_CoreWindow_PointerEventArgs *iface)
{ return InterlockedIncrement(&((struct pointer_probe *)iface)->ref); }
static ULONG WINAPI pointer_probe_release(ITypedEventHandler_CoreWindow_PointerEventArgs *iface)
{ return InterlockedDecrement(&((struct pointer_probe *)iface)->ref); }
static HRESULT WINAPI pointer_probe_invoke(ITypedEventHandler_CoreWindow_PointerEventArgs *iface, ICoreWindow *window, IPointerEventArgs *args)
{
    struct pointer_probe *probe = (void *)iface;
    IPointerPoint *point;
    IPointerPointProperties *props;
    IPointerDevice *device;
    ICoreWindowEventArgs *base;
    PointerDeviceType type;
    PointerUpdateKind kind;
    boolean flag;
    INT32 wheel;
    Point position;
    (void)window;
    ++probe->calls;
    CHECK(IPointerEventArgs_get_CurrentPoint(args, &point) == S_OK);
    CHECK(IPointerPoint_get_Position(point, &position) == S_OK);
    CHECK(position.X == probe->position.X && position.Y == probe->position.Y);
    CHECK(IPointerPoint_get_PointerDevice(point, &device) == S_OK);
    CHECK(IPointerDevice_get_PointerDeviceType(device, &type) == S_OK && type == PointerDeviceType_Mouse);
    IPointerDevice_Release(device);
    CHECK(IPointerPoint_get_Properties(point, &props) == S_OK);
    CHECK(IPointerPointProperties_get_PointerUpdateKind(props, &kind) == S_OK && kind == probe->kind);
    CHECK(IPointerPointProperties_get_IsLeftButtonPressed(props, &flag) == S_OK && flag == probe->pressed);
    CHECK(IPointerPointProperties_get_MouseWheelDelta(props, &wheel) == S_OK && wheel == probe->wheel);
    CHECK(IPointerPointProperties_get_IsHorizontalMouseWheel(props, &flag) == S_OK && flag == probe->horizontal);
    IPointerPointProperties_Release(props);
    if (!probe->saved) { probe->saved = point; IPointerPoint_AddRef(point); }
    IPointerPoint_Release(point);
    CHECK(IPointerEventArgs_QueryInterface(args, &IID_ICoreWindowEventArgs, (void **)&base) == S_OK);
    CHECK(ICoreWindowEventArgs_put_Handled(base, TRUE) == S_OK);
    ICoreWindowEventArgs_Release(base);
    return S_OK;
}
static const ITypedEventHandler_CoreWindow_PointerEventArgsVtbl pointer_probe_vtbl =
{pointer_probe_qi, pointer_probe_addref, pointer_probe_release, pointer_probe_invoke};
static void test_pointer(ICoreWindow *window, HWND hwnd)
{
    struct pointer_probe probe = {{&pointer_probe_vtbl}, 1, 0, PointerUpdateKind_LeftButtonPressed, TRUE, FALSE, 0, {0,0}, NULL};
    EventRegistrationToken down, up, moved, wheel;
    ICoreCursorFactory *factory;
    ICoreCursor *cursor, *actual;
    IPointerPointProperties *props;
    boolean flag;
    CoreCursorType type;
    POINT pos = {42, 31};
    float scale = 96.0f / GetDpiForWindow(hwnd);
    CHECK(get_factory(L"Windows.UI.Core.CoreCursor", &IID_ICoreCursorFactory, (void **)&factory) == S_OK);
    CHECK(ICoreCursorFactory_CreateCursor(factory, CoreCursorType_Hand, 0, &cursor) == S_OK);
    CHECK(ICoreCursor_get_Type(cursor, &type) == S_OK && type == CoreCursorType_Hand);
    CHECK(ICoreWindow_put_PointerCursor(window, cursor) == S_OK);
    CHECK(ICoreWindow_get_PointerCursor(window, &actual) == S_OK && actual == cursor);
    ICoreCursor_Release(actual);
    SendMessageW(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    CHECK(GetCursor() == LoadCursorW(NULL, (const WCHAR *)IDC_HAND));
    CHECK(ICoreWindow_put_PointerCursor(window, NULL) == S_OK);
    CHECK(ICoreWindow_get_PointerCursor(window, &actual) == S_OK && !actual);
    SendMessageW(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    CHECK(GetCursor() == NULL);
    CHECK(ICoreWindow_put_PointerCursor(window, cursor) == S_OK);
    SendMessageW(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    CHECK(GetCursor() == LoadCursorW(NULL, (const WCHAR *)IDC_HAND));
    ICoreCursor_Release(cursor);
    CHECK(ICoreCursorFactory_CreateCursor(factory, CoreCursorType_Arrow, 0, &cursor) == S_OK);
    CHECK(ICoreWindow_put_PointerCursor(window, cursor) == S_OK);
    ICoreCursor_Release(cursor);
    ICoreCursorFactory_Release(factory);

    CHECK(ICoreWindow_add_PointerPressed(window, &probe.iface, &down) == S_OK);
    CHECK(ICoreWindow_add_PointerReleased(window, &probe.iface, &up) == S_OK);
    CHECK(ICoreWindow_add_PointerMoved(window, &probe.iface, &moved) == S_OK);
    CHECK(ICoreWindow_add_PointerWheelChanged(window, &probe.iface, &wheel) == S_OK);
    CHECK(probe.ref == 5);
    probe.position = (Point){42*scale, 31*scale};
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(42, 31));
    CHECK(probe.calls == 1);
    probe.kind = PointerUpdateKind_LeftButtonReleased; probe.pressed = FALSE;
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(42, 31));
    CHECK(probe.calls == 2);
    probe.kind = PointerUpdateKind_Other;
    probe.position.X = -12*scale;
    SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(-12, 31));
    CHECK(probe.calls == 3);
    probe.position.X = 42*scale; probe.wheel = -WHEEL_DELTA;
    ClientToScreen(hwnd, &pos);
    SendMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), MAKELPARAM(pos.x, pos.y));
    CHECK(probe.calls == 4);
    probe.horizontal = TRUE;
    SendMessageW(hwnd, WM_MOUSEHWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), MAKELPARAM(pos.x, pos.y));
    CHECK(probe.calls == 5);
    /* Retained snapshots must not change when subsequent events arrive. */
    CHECK(IPointerPoint_get_Properties(probe.saved, &props) == S_OK);
    CHECK(IPointerPointProperties_get_IsLeftButtonPressed(props, &flag) == S_OK && flag);
    IPointerPointProperties_Release(props);
    IPointerPoint_Release(probe.saved);
    CHECK(ICoreWindow_remove_PointerPressed(window, down) == S_OK);
    CHECK(ICoreWindow_remove_PointerReleased(window, up) == S_OK);
    CHECK(ICoreWindow_remove_PointerMoved(window, moved) == S_OK);
    CHECK(ICoreWindow_remove_PointerWheelChanged(window, wheel) == S_OK);
    CHECK(probe.ref == 1);
}
