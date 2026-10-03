/* Independent pointer input delivered on the creating thread.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "private.h"
#include "wine/winrt_events.h"
#include "corewindow.h"
WINE_DEFAULT_DEBUG_CHANNEL(xaml);
#define INPUT_MESSAGE (WM_APP + 0x319)
struct input;
struct bridge
{
    ITypedEventHandler_CoreWindow_PointerEventArgs iface;
    LONG ref;
    SRWLOCK lock;
    struct input *owner;
    UINT event;
    EventRegistrationToken token;
};
struct input
{
    ICoreInputSourceBase ICoreInputSourceBase_iface;
    ICorePointerInputSource ICorePointerInputSource_iface;
    ICoreDispatcher ICoreDispatcher_iface;
    ICoreDispatcherWithTaskPriority ICoreDispatcherWithTaskPriority_iface;
    LONG ref;
    DWORD thread;
    ICoreWindow *window;
    HWND target;
    UINT32 devices;
    BOOL enabled, stopped;
    struct dispatcher_queue queue;
    struct bridge *bridges[7];
    struct winrt_event events[7], enabled_event;
};
struct queued_input { struct input *owner; IPointerEventArgs *args; UINT event; };
static HRESULT WINAPI input_QueryInterface(ICoreInputSourceBase *iface, REFIID iid, void **out)
{
    struct input *impl = CONTAINING_RECORD(iface, struct input, ICoreInputSourceBase_iface);
    if (!out) return E_POINTER;
    *out = NULL;
    TRACE("input iid %s\n", debugstr_guid(iid));
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_ICoreInputSourceBase)) *out = iface;
    else if (IsEqualGUID(iid, &IID_ICorePointerInputSource)) *out = &impl->ICorePointerInputSource_iface;
    else if (IsEqualGUID(iid, &IID_ICoreDispatcher)) *out = &impl->ICoreDispatcher_iface;
    else if (IsEqualGUID(iid, &IID_ICoreDispatcherWithTaskPriority)) *out = &impl->ICoreDispatcherWithTaskPriority_iface;
    else return E_NOINTERFACE;
    ICoreInputSourceBase_AddRef(iface); return S_OK;
}
static ULONG WINAPI input_AddRef(ICoreInputSourceBase *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct input, ICoreInputSourceBase_iface)->ref); }
static void input_unsubscribe(struct input *impl);
static ULONG WINAPI input_Release(ICoreInputSourceBase *iface)
{
    struct input *impl = CONTAINING_RECORD(iface, struct input, ICoreInputSourceBase_iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    UINT i;
    if (!ref)
    {
        input_unsubscribe(impl);
        dispatcher_queue_close(&impl->queue);
        for (i = 0; i < ARRAY_SIZE(impl->events); ++i) winrt_event_clear(&impl->events[i]);
        winrt_event_clear(&impl->enabled_event);
        ICoreWindow_Release(impl->window);
        free(impl);
    }
    return ref;
}
static HRESULT WINAPI input_GetIids(ICoreInputSourceBase *iface, ULONG *count, IID **ids)
{ if (!count || !ids) return E_POINTER; *count = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI input_GetRuntimeClassName(ICoreInputSourceBase *iface, HSTRING *out)
{ const WCHAR name[] = L"Windows.UI.Core.CoreIndependentInputSource"; return WindowsCreateString(name, ARRAY_SIZE(name) - 1, out); }
static HRESULT WINAPI input_GetTrustLevel(ICoreInputSourceBase *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI input_get_Dispatcher(ICoreInputSourceBase *iface, ICoreDispatcher **out)
{ return input_QueryInterface(iface, &IID_ICoreDispatcher, (void **)out); }
static HRESULT WINAPI input_get_IsInputEnabled(ICoreInputSourceBase *iface, boolean *out)
{ if (!out) return E_POINTER; *out = CONTAINING_RECORD(iface, struct input, ICoreInputSourceBase_iface)->enabled; return S_OK; }
static HRESULT WINAPI input_put_IsInputEnabled(ICoreInputSourceBase *iface, boolean value)
{ CONTAINING_RECORD(iface, struct input, ICoreInputSourceBase_iface)->enabled = value; return S_OK; }
static HRESULT WINAPI input_add_InputEnabled(ICoreInputSourceBase *iface, IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&CONTAINING_RECORD(iface, struct input, ICoreInputSourceBase_iface)->enabled_event, handler, token); }
static HRESULT WINAPI input_remove_InputEnabled(ICoreInputSourceBase *iface, EventRegistrationToken token)
{ return winrt_event_remove(&CONTAINING_RECORD(iface, struct input, ICoreInputSourceBase_iface)->enabled_event, token); }
static const ICoreInputSourceBaseVtbl input_vtbl = {input_QueryInterface, input_AddRef, input_Release, input_GetIids,
    input_GetRuntimeClassName, input_GetTrustLevel, input_get_Dispatcher, input_get_IsInputEnabled, input_put_IsInputEnabled,
    input_add_InputEnabled, input_remove_InputEnabled};

static HRESULT WINAPI bridge_QueryInterface(ITypedEventHandler_CoreWindow_PointerEventArgs *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IAgileObject) && !IsEqualGUID(iid, &IID_ITypedEventHandler_CoreWindow_PointerEventArgs)) return E_NOINTERFACE;
    *out = iface; ITypedEventHandler_CoreWindow_PointerEventArgs_AddRef(iface); return S_OK;
}
static ULONG WINAPI bridge_AddRef(ITypedEventHandler_CoreWindow_PointerEventArgs *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct bridge, iface)->ref); }
static ULONG WINAPI bridge_Release(ITypedEventHandler_CoreWindow_PointerEventArgs *iface)
{
    struct bridge *impl = CONTAINING_RECORD(iface, struct bridge, iface);
    ULONG ref = InterlockedDecrement(&impl->ref); if (!ref) free(impl); return ref;
}
static HRESULT WINAPI bridge_Invoke(ITypedEventHandler_CoreWindow_PointerEventArgs *iface, ICoreWindow *sender, IPointerEventArgs *args)
{
    struct bridge *bridge = CONTAINING_RECORD(iface, struct bridge, iface);
    struct input *impl;
    struct queued_input *queued;
    LONG ref;
    AcquireSRWLockShared(&bridge->lock);
    impl = bridge->owner;
    if (impl)
    {
        ref = impl->ref;
        while (ref && InterlockedCompareExchange(&impl->ref, ref + 1, ref) != ref) ref = impl->ref;
        if (!ref) impl = NULL;
    }
    ReleaseSRWLockShared(&bridge->lock);
    if (!impl) return S_OK;
    TRACE("queue pointer event %u to thread %lu\n", bridge->event, impl->thread);
    if (!(queued = malloc(sizeof(*queued)))) { input_Release(&impl->ICoreInputSourceBase_iface); return E_OUTOFMEMORY; }
    queued->owner = impl; queued->event = bridge->event; queued->args = args; IPointerEventArgs_AddRef(args);
    if (!PostMessageW(impl->queue.hwnd, INPUT_MESSAGE, 0, (LPARAM)queued))
    { IPointerEventArgs_Release(args); free(queued); input_Release(&impl->ICoreInputSourceBase_iface); }
    return S_OK;
}
static const ITypedEventHandler_CoreWindow_PointerEventArgsVtbl bridge_vtbl = {bridge_QueryInterface, bridge_AddRef, bridge_Release, bridge_Invoke};
static LRESULT CALLBACK input_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    struct input *impl = (void *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW *)lparam)->lpCreateParams);
    if (msg == INPUT_MESSAGE)
    {
        struct queued_input *queued = (void *)lparam;
        impl = queued->owner;
        TRACE("dispatch pointer event %u, enabled %d stopped %d\n", queued->event, impl->enabled, impl->stopped);
        if (impl->enabled && !impl->stopped) winrt_event_notify(&impl->events[queued->event], &impl->ICoreInputSourceBase_iface, queued->args);
        IPointerEventArgs_Release(queued->args); free(queued); input_Release(&impl->ICoreInputSourceBase_iface);
        return 0;
    }
    if (msg == WINE_WM_DISPATCH && impl) { dispatcher_queue_dispatch(&impl->queue); return 0; }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}
DEFINE_IINSPECTABLE(dispatcher, ICoreDispatcher, struct input, ICoreInputSourceBase_iface)
static HRESULT WINAPI dispatcher_get_HasThreadAccess(ICoreDispatcher *iface, boolean *out)
{ if (!out) return E_POINTER; *out = impl_from_ICoreDispatcher(iface)->thread == GetCurrentThreadId(); return S_OK; }
static HRESULT WINAPI dispatcher_ProcessEvents(ICoreDispatcher *iface, CoreProcessEventsOption options)
{
    struct input *impl = impl_from_ICoreDispatcher(iface);
    MSG msg;
    BOOL wait, present, ret;
    TRACE("ProcessEvents thread %lu options %d\n", impl->thread, options);
    if (impl->thread != GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (options < 0 || options > CoreProcessEventsOption_ProcessAllIfPresent) return E_INVALIDARG;
    wait = options == CoreProcessEventsOption_ProcessOneAndAllPending || options == CoreProcessEventsOption_ProcessUntilQuit;
    while (!impl->stopped)
    {
        present = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE);
        if (!present)
        {
            if (!wait) break;
            ret = GetMessageW(&msg, NULL, 0, 0);
            if (ret == -1) return HRESULT_FROM_WIN32(GetLastError());
            if (!ret) break;
        }
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg); DispatchMessageW(&msg);
        if (options == CoreProcessEventsOption_ProcessOneIfPresent) break;
        wait = options == CoreProcessEventsOption_ProcessUntilQuit;
    }
    return S_OK;
}
static HRESULT WINAPI dispatcher_RunAsync(ICoreDispatcher *iface, CoreDispatcherPriority priority, IDispatchedHandler *handler, IAsyncAction **out)
{ return dispatcher_queue_add(&impl_from_ICoreDispatcher(iface)->queue, priority, handler, out); }
static HRESULT WINAPI dispatcher_RunIdleAsync(ICoreDispatcher *iface, IIdleDispatchedHandler *handler, IAsyncAction **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const ICoreDispatcherVtbl dispatcher_vtbl = {dispatcher_QueryInterface, dispatcher_AddRef, dispatcher_Release,
    dispatcher_GetIids, dispatcher_GetRuntimeClassName, dispatcher_GetTrustLevel, dispatcher_get_HasThreadAccess,
    dispatcher_ProcessEvents, dispatcher_RunAsync, dispatcher_RunIdleAsync};
DEFINE_IINSPECTABLE(priority, ICoreDispatcherWithTaskPriority, struct input, ICoreInputSourceBase_iface)
static HRESULT WINAPI priority_get_CurrentPriority(ICoreDispatcherWithTaskPriority *iface, CoreDispatcherPriority *out)
{ if (!out) return E_POINTER; *out = impl_from_ICoreDispatcherWithTaskPriority(iface)->queue.current_priority; return S_OK; }
static HRESULT WINAPI priority_put_CurrentPriority(ICoreDispatcherWithTaskPriority *iface, CoreDispatcherPriority value)
{ if (value < -2 || value > 1) return E_INVALIDARG; impl_from_ICoreDispatcherWithTaskPriority(iface)->queue.current_priority = value; return S_OK; }
static HRESULT WINAPI priority_ShouldYieldToPriority(ICoreDispatcherWithTaskPriority *iface, CoreDispatcherPriority value, boolean *out)
{ if (!out) return E_POINTER; *out = dispatcher_queue_should_yield(&impl_from_ICoreDispatcherWithTaskPriority(iface)->queue, value); return S_OK; }
static HRESULT WINAPI priority_ShouldYield(ICoreDispatcherWithTaskPriority *iface, boolean *out)
{ return priority_ShouldYieldToPriority(iface, impl_from_ICoreDispatcherWithTaskPriority(iface)->queue.current_priority, out); }
static HRESULT WINAPI priority_StopProcessEvents(ICoreDispatcherWithTaskPriority *iface)
{
    struct input *impl = impl_from_ICoreDispatcherWithTaskPriority(iface);
    impl->stopped = TRUE; PostMessageW(impl->queue.hwnd, WM_NULL, 0, 0); return S_OK;
}
static const ICoreDispatcherWithTaskPriorityVtbl priority_vtbl = {priority_QueryInterface, priority_AddRef, priority_Release,
    priority_GetIids, priority_GetRuntimeClassName, priority_GetTrustLevel, priority_get_CurrentPriority,
    priority_put_CurrentPriority, priority_ShouldYield, priority_ShouldYieldToPriority, priority_StopProcessEvents};

/* Cursor and capture changes must execute on the CoreWindow's UI thread. */
struct input_command { IDispatchedHandler iface; LONG ref; struct input *input; HANDLE done; UINT op; void *value; HRESULT result; };
static HRESULT WINAPI command_QueryInterface(IDispatchedHandler *iface, REFIID iid, void **out)
{ if (!out) return E_POINTER; *out = NULL; if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IDispatchedHandler) && !IsEqualGUID(iid, &IID_IAgileObject)) return E_NOINTERFACE; *out = iface; IDispatchedHandler_AddRef(iface); return S_OK; }
static ULONG WINAPI command_AddRef(IDispatchedHandler *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct input_command, iface)->ref); }
static ULONG WINAPI command_Release(IDispatchedHandler *iface)
{
    struct input_command *cmd = CONTAINING_RECORD(iface, struct input_command, iface);
    HANDLE done = cmd->done;
    ULONG ref = InterlockedDecrement(&cmd->ref);
    if (ref == 1 && done) SetEvent(done);
    return ref;
}
static HRESULT WINAPI command_Invoke(IDispatchedHandler *iface)
{
    struct input_command *cmd = CONTAINING_RECORD(iface, struct input_command, iface);
    ICoreWindow *window = cmd->input->window;
    switch (cmd->op)
    {
    case 0: cmd->result = ICoreWindow_ReleasePointerCapture(window); break;
    case 1: cmd->result = ICoreWindow_SetPointerCapture(window); break;
    case 2: *(boolean *)cmd->value = GetCapture() == cmd->input->target; cmd->result = S_OK; break;
    case 3: cmd->result = ICoreWindow_get_PointerPosition(window, cmd->value); break;
    case 4: cmd->result = ICoreWindow_get_PointerCursor(window, cmd->value); break;
    case 5: cmd->result = ICoreWindow_put_PointerCursor(window, cmd->value); break;
    }
    return S_OK;
}
static const IDispatchedHandlerVtbl command_vtbl = {command_QueryInterface, command_AddRef, command_Release, command_Invoke};
static HRESULT run_command(struct input *impl, UINT op, void *value)
{
    struct input_command cmd = {{&command_vtbl}, 1, impl, NULL, op, value, E_FAIL};
    ICoreDispatcher *dispatcher;
    IAsyncAction *action;
    boolean access;
    HRESULT hr = ICoreWindow_get_Dispatcher(impl->window, &dispatcher);
    if (FAILED(hr)) return hr;
    ICoreDispatcher_get_HasThreadAccess(dispatcher, &access);
    if (access) { command_Invoke(&cmd.iface); hr = cmd.result; }
    else
    {
        if (!(cmd.done = CreateEventW(NULL, TRUE, FALSE, NULL))) hr = HRESULT_FROM_WIN32(GetLastError());
        else
        {
            hr = ICoreDispatcher_RunAsync(dispatcher, CoreDispatcherPriority_Normal, &cmd.iface, &action);
            if (SUCCEEDED(hr))
            {
                WaitForSingleObject(cmd.done, INFINITE);
                hr = cmd.result; IAsyncAction_Release(action);
            }
            CloseHandle(cmd.done);
        }
    }
    ICoreDispatcher_Release(dispatcher); return hr;
}
DEFINE_IINSPECTABLE(pointer, ICorePointerInputSource, struct input, ICoreInputSourceBase_iface)
static HRESULT WINAPI pointer_ReleasePointerCapture(ICorePointerInputSource *iface) { return run_command(impl_from_ICorePointerInputSource(iface), 0, NULL); }
static HRESULT WINAPI pointer_SetPointerCapture(ICorePointerInputSource *iface) { return run_command(impl_from_ICorePointerInputSource(iface), 1, NULL); }
static HRESULT WINAPI pointer_get_HasCapture(ICorePointerInputSource *iface, boolean *out) { if (!out) return E_POINTER; return run_command(impl_from_ICorePointerInputSource(iface), 2, out); }
static HRESULT WINAPI pointer_get_PointerPosition(ICorePointerInputSource *iface, Point *out) { if (!out) return E_POINTER; return run_command(impl_from_ICorePointerInputSource(iface), 3, out); }
static HRESULT WINAPI pointer_get_PointerCursor(ICorePointerInputSource *iface, ICoreCursor **out) { if (!out) return E_POINTER; return run_command(impl_from_ICorePointerInputSource(iface), 4, out); }
static HRESULT WINAPI pointer_put_PointerCursor(ICorePointerInputSource *iface, ICoreCursor *value) { return run_command(impl_from_ICorePointerInputSource(iface), 5, value); }
static HRESULT WINAPI pointer_add_PointerCaptureLost(ICorePointerInputSource *iface, ITypedEventHandler_IInspectable_PointerEventArgs *handler, EventRegistrationToken *token)
{ TRACE("subscribe %s\n", __func__); return winrt_event_add(&impl_from_ICorePointerInputSource(iface)->events[0], handler, token); }
static HRESULT WINAPI pointer_remove_PointerCaptureLost(ICorePointerInputSource *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_ICorePointerInputSource(iface)->events[0], token); }
static HRESULT WINAPI pointer_add_PointerEntered(ICorePointerInputSource *iface, ITypedEventHandler_IInspectable_PointerEventArgs *handler, EventRegistrationToken *token)
{ TRACE("subscribe %s\n", __func__); return winrt_event_add(&impl_from_ICorePointerInputSource(iface)->events[1], handler, token); }
static HRESULT WINAPI pointer_remove_PointerEntered(ICorePointerInputSource *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_ICorePointerInputSource(iface)->events[1], token); }
static HRESULT WINAPI pointer_add_PointerExited(ICorePointerInputSource *iface, ITypedEventHandler_IInspectable_PointerEventArgs *handler, EventRegistrationToken *token)
{ TRACE("subscribe %s\n", __func__); return winrt_event_add(&impl_from_ICorePointerInputSource(iface)->events[2], handler, token); }
static HRESULT WINAPI pointer_remove_PointerExited(ICorePointerInputSource *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_ICorePointerInputSource(iface)->events[2], token); }
static HRESULT WINAPI pointer_add_PointerMoved(ICorePointerInputSource *iface, ITypedEventHandler_IInspectable_PointerEventArgs *handler, EventRegistrationToken *token)
{ TRACE("subscribe %s\n", __func__); return winrt_event_add(&impl_from_ICorePointerInputSource(iface)->events[3], handler, token); }
static HRESULT WINAPI pointer_remove_PointerMoved(ICorePointerInputSource *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_ICorePointerInputSource(iface)->events[3], token); }
static HRESULT WINAPI pointer_add_PointerPressed(ICorePointerInputSource *iface, ITypedEventHandler_IInspectable_PointerEventArgs *handler, EventRegistrationToken *token)
{ TRACE("subscribe %s\n", __func__); return winrt_event_add(&impl_from_ICorePointerInputSource(iface)->events[4], handler, token); }
static HRESULT WINAPI pointer_remove_PointerPressed(ICorePointerInputSource *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_ICorePointerInputSource(iface)->events[4], token); }
static HRESULT WINAPI pointer_add_PointerReleased(ICorePointerInputSource *iface, ITypedEventHandler_IInspectable_PointerEventArgs *handler, EventRegistrationToken *token)
{ TRACE("subscribe %s\n", __func__); return winrt_event_add(&impl_from_ICorePointerInputSource(iface)->events[5], handler, token); }
static HRESULT WINAPI pointer_remove_PointerReleased(ICorePointerInputSource *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_ICorePointerInputSource(iface)->events[5], token); }
static HRESULT WINAPI pointer_add_PointerWheelChanged(ICorePointerInputSource *iface, ITypedEventHandler_IInspectable_PointerEventArgs *handler, EventRegistrationToken *token)
{ TRACE("subscribe %s\n", __func__); return winrt_event_add(&impl_from_ICorePointerInputSource(iface)->events[6], handler, token); }
static HRESULT WINAPI pointer_remove_PointerWheelChanged(ICorePointerInputSource *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_ICorePointerInputSource(iface)->events[6], token); }
static const ICorePointerInputSourceVtbl pointer_vtbl = {pointer_QueryInterface, pointer_AddRef, pointer_Release,
    pointer_GetIids, pointer_GetRuntimeClassName, pointer_GetTrustLevel, pointer_ReleasePointerCapture, pointer_SetPointerCapture,
    pointer_get_HasCapture, pointer_get_PointerPosition, pointer_get_PointerCursor, pointer_put_PointerCursor,
    pointer_add_PointerCaptureLost, pointer_remove_PointerCaptureLost,
    pointer_add_PointerEntered, pointer_remove_PointerEntered,
    pointer_add_PointerExited, pointer_remove_PointerExited,
    pointer_add_PointerMoved, pointer_remove_PointerMoved,
    pointer_add_PointerPressed, pointer_remove_PointerPressed,
    pointer_add_PointerReleased, pointer_remove_PointerReleased,
    pointer_add_PointerWheelChanged, pointer_remove_PointerWheelChanged };
static void input_unsubscribe(struct input *impl)
{
    UINT i;
    for (i = 0; i < ARRAY_SIZE(impl->bridges); ++i)
        if (impl->bridges[i])
        {
            AcquireSRWLockExclusive(&impl->bridges[i]->lock);
            impl->bridges[i]->owner = NULL;
            ReleaseSRWLockExclusive(&impl->bridges[i]->lock);
        }
    if (impl->bridges[0]) ICoreWindow_remove_PointerCaptureLost(impl->window, impl->bridges[0]->token);
    if (impl->bridges[1]) ICoreWindow_remove_PointerEntered(impl->window, impl->bridges[1]->token);
    if (impl->bridges[2]) ICoreWindow_remove_PointerExited(impl->window, impl->bridges[2]->token);
    if (impl->bridges[3]) ICoreWindow_remove_PointerMoved(impl->window, impl->bridges[3]->token);
    if (impl->bridges[4]) ICoreWindow_remove_PointerPressed(impl->window, impl->bridges[4]->token);
    if (impl->bridges[5]) ICoreWindow_remove_PointerReleased(impl->window, impl->bridges[5]->token);
    if (impl->bridges[6]) ICoreWindow_remove_PointerWheelChanged(impl->window, impl->bridges[6]->token);
    for (i = 0; i < ARRAY_SIZE(impl->bridges); ++i)
        if (impl->bridges[i]) { bridge_Release(&impl->bridges[i]->iface); impl->bridges[i] = NULL; }
    if (impl->queue.hwnd)
    {
        SetWindowLongPtrW(impl->queue.hwnd, GWLP_USERDATA, 0);
        if (impl->thread == GetCurrentThreadId()) DestroyWindow(impl->queue.hwnd);
        else PostMessageW(impl->queue.hwnd, WM_CLOSE, 0, 0);
    }
}
HRESULT xaml_input_create(ICoreWindow *window, UINT32 devices, IInspectable **out)
{
    struct input *impl;
    ICoreWindowInterop *interop;
    WNDCLASSW cls = {0};
    UINT i;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!window || !devices || devices & ~7u) return E_INVALIDARG;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->ICoreInputSourceBase_iface.lpVtbl = &input_vtbl;
    impl->ICorePointerInputSource_iface.lpVtbl = &pointer_vtbl;
    impl->ICoreDispatcher_iface.lpVtbl = &dispatcher_vtbl;
    impl->ICoreDispatcherWithTaskPriority_iface.lpVtbl = &priority_vtbl;
    impl->ref = 1; impl->thread = GetCurrentThreadId(); impl->devices = devices; impl->enabled = TRUE;
    impl->window = window; ICoreWindow_AddRef(window);
    hr = ICoreWindow_QueryInterface(window, &IID_ICoreWindowInterop, (void **)&interop);
    if (FAILED(hr)) goto failed;
    hr = ICoreWindowInterop_get_WindowHandle(interop, &impl->target); ICoreWindowInterop_Release(interop);
    if (FAILED(hr)) goto failed;
    cls.lpfnWndProc = input_window_proc; cls.lpszClassName = L"Wine.Xaml.IndependentInput";
    cls.hInstance = GetModuleHandleW(L"windows.ui.xaml.dll");
    if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { hr = HRESULT_FROM_WIN32(GetLastError()); goto failed; }
    impl->queue.hwnd = CreateWindowExW(0, cls.lpszClassName, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, cls.hInstance, impl);
    if (!impl->queue.hwnd) { hr = HRESULT_FROM_WIN32(GetLastError()); goto failed; }
    for (i = 0; i < ARRAY_SIZE(impl->bridges); ++i)
    {
        if (!(impl->bridges[i] = calloc(1, sizeof(*impl->bridges[i])))) { hr = E_OUTOFMEMORY; goto failed; }
        impl->bridges[i]->iface.lpVtbl = &bridge_vtbl; impl->bridges[i]->ref = 1;
        impl->bridges[i]->owner = impl; impl->bridges[i]->event = i;
    }
    if (FAILED(hr = ICoreWindow_add_PointerCaptureLost(window, &impl->bridges[0]->iface, &impl->bridges[0]->token))) goto failed;
    if (FAILED(hr = ICoreWindow_add_PointerEntered(window, &impl->bridges[1]->iface, &impl->bridges[1]->token))) goto failed;
    if (FAILED(hr = ICoreWindow_add_PointerExited(window, &impl->bridges[2]->iface, &impl->bridges[2]->token))) goto failed;
    if (FAILED(hr = ICoreWindow_add_PointerMoved(window, &impl->bridges[3]->iface, &impl->bridges[3]->token))) goto failed;
    if (FAILED(hr = ICoreWindow_add_PointerPressed(window, &impl->bridges[4]->iface, &impl->bridges[4]->token))) goto failed;
    if (FAILED(hr = ICoreWindow_add_PointerReleased(window, &impl->bridges[5]->iface, &impl->bridges[5]->token))) goto failed;
    if (FAILED(hr = ICoreWindow_add_PointerWheelChanged(window, &impl->bridges[6]->iface, &impl->bridges[6]->token))) goto failed;
    *out = (IInspectable *)&impl->ICoreInputSourceBase_iface;
    return S_OK;
failed:
    input_Release(&impl->ICoreInputSourceBase_iface); return hr;
}
