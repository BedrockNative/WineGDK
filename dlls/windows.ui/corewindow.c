/* WinRT Windows.UI.Core.CoreWindow Implementation
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
#include "winuser.h"
#include "roapi.h"
#include "wine/winrt_events.h"

#include "wine/debug.h"

#include "window_state.h"

WINE_DEFAULT_DEBUG_CHANNEL(ui);


static DWORD window_tls = TLS_OUT_OF_INDEXES;
static INIT_ONCE window_init = INIT_ONCE_STATIC_INIT;

enum window_event_type
{
    WINDOW_EVENT_Activated,
    WINDOW_EVENT_AutomationProviderRequested,
    WINDOW_EVENT_CharacterReceived,
    WINDOW_EVENT_Closed,
    WINDOW_EVENT_InputEnabled,
    WINDOW_EVENT_KeyDown,
    WINDOW_EVENT_KeyUp,
    WINDOW_EVENT_PointerCaptureLost,
    WINDOW_EVENT_PointerEntered,
    WINDOW_EVENT_PointerExited,
    WINDOW_EVENT_PointerMoved,
    WINDOW_EVENT_PointerPressed,
    WINDOW_EVENT_PointerReleased,
    WINDOW_EVENT_TouchHitTesting,
    WINDOW_EVENT_PointerWheelChanged,
    WINDOW_EVENT_SizeChanged,
    WINDOW_EVENT_VisibilityChanged,
    WINDOW_EVENT_ResizeStarted,
    WINDOW_EVENT_ResizeCompleted,
    WINDOW_EVENT_AcceleratorKeyActivated,
    WINDOW_EVENT_COUNT
};

struct core_window
{
    ICoreWindow ICoreWindow_iface;
    ICoreWindow2 ICoreWindow2_iface;
    ICoreWindow4 ICoreWindow4_iface;
    ICoreWindowInterop ICoreWindowInterop_iface;
    ICoreDispatcher ICoreDispatcher_iface;
    ICoreAcceleratorKeys ICoreAcceleratorKeys_iface;
    ICoreDispatcherWithTaskPriority ICoreDispatcherWithTaskPriority_iface;
    struct corewindow_state state;
    BOOL stop_events;
    LONG ref;
    DWORD thread;
    HWND hwnd;
    struct dispatcher_queue queue;
    CoreWindowFlowDirection flow;
    ICoreCursor *cursor;
    HCURSOR native_cursor;
    HICON small_icon, large_icon;
    BOOL tracking_mouse, clipped;
    RECT clip_rect;
    IUnknown *marshaler;
    IPointerVisualizationSettings *feedback;
    ISystemNavigationManager *navigation;
    IMouseDevice *mouse;
    IPropertySet *properties;
    boolean message_handled;
    struct winrt_event events[WINDOW_EVENT_COUNT];
};

static struct core_window *impl_from_ICoreWindow( ICoreWindow *iface )
{
    return CONTAINING_RECORD( iface, struct core_window, ICoreWindow_iface );
}

static HRESULT window_check( struct core_window *window )
{
    if (window->thread != GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    return window->hwnd ? S_OK : RO_E_CLOSED;
}

static HRESULT WINAPI window_QueryInterface( ICoreWindow *iface, REFIID iid, void **out )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &IID_ICoreWindow ))
        *out = iface;
    else if (IsEqualGUID(iid, &IID_IMarshal))
        return IUnknown_QueryInterface(window->marshaler,iid,out);
    else if (IsEqualGUID(iid, &IID_ICoreWindow2)) *out = &window->ICoreWindow2_iface;
    else if (IsEqualGUID(iid, &IID_ICoreWindow4)) *out = &window->ICoreWindow4_iface;
    else if (IsEqualGUID( iid, &IID_ICoreWindowInterop )) *out = &window->ICoreWindowInterop_iface;
    else
    {
        FIXME( "window interface %s not implemented.\n", debugstr_guid( iid ) );
        return E_NOINTERFACE;
    }
    ICoreWindow_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI window_AddRef( ICoreWindow *iface )
{
    return InterlockedIncrement( &impl_from_ICoreWindow( iface )->ref );
}

static ULONG WINAPI window_Release( ICoreWindow *iface )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    ULONG ref = InterlockedDecrement( &window->ref );
    if (!ref)
    {
        unsigned int i;
        for (i = 0; i < ARRAY_SIZE(window->events); ++i) winrt_event_clear( &window->events[i] );
        if (window->navigation) ISystemNavigationManager_Release(window->navigation);
        if (window->mouse) IMouseDevice_Release(window->mouse);
        if (window->marshaler) IUnknown_Release(window->marshaler);
        if (window->feedback) IPointerVisualizationSettings_Release(window->feedback);
        if (window->cursor) ICoreCursor_Release( window->cursor );
        if (window->properties) IPropertySet_Release( window->properties );
        if (window->small_icon) DestroyIcon( window->small_icon );
        if (window->large_icon) DestroyIcon( window->large_icon );
        free( window );
    }
    return ref;
}

static HRESULT WINAPI window_GetIids( ICoreWindow *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = IID_ICoreWindow;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI window_GetRuntimeClassName( ICoreWindow *iface, HSTRING *name )
{
    const WCHAR *str = RuntimeClass_Windows_UI_Core_CoreWindow;
    return WindowsCreateString( str, wcslen( str ), name );
}

static HRESULT WINAPI window_GetTrustLevel( ICoreWindow *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI window_get_AutomationHostProvider( ICoreWindow *iface, IInspectable **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI window_get_Bounds( ICoreWindow *iface, Rect *value )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    RECT rect;
    POINT origin = {0};
    float scale;
    HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr = window_check( window ))) return hr;
    if (!GetClientRect( window->hwnd, &rect ) || !ClientToScreen( window->hwnd, &origin ))
        return HRESULT_FROM_WIN32( GetLastError() );
    scale = 96.0f / GetDpiForWindow( window->hwnd );
    value->X = origin.x * scale;
    value->Y = origin.y * scale;
    value->Width = (rect.right - rect.left) * scale;
    value->Height = (rect.bottom - rect.top) * scale;
    return S_OK;
}

static HRESULT WINAPI window_get_CustomProperties( ICoreWindow *iface, IPropertySet **out )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    HSTRING name;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (FAILED(hr = window_check( window ))) return hr;
    if (!window->properties)
    {
        const WCHAR *str = RuntimeClass_Windows_Foundation_Collections_PropertySet;
        if (FAILED(hr = WindowsCreateString( str, wcslen( str ), &name ))) return hr;
        hr = RoActivateInstance( name, (IInspectable **)&window->properties );
        WindowsDeleteString( name );
        if (FAILED(hr)) return hr;
    }
    IPropertySet_AddRef( (*out = window->properties) );
    return S_OK;
}

static HRESULT WINAPI window_get_Dispatcher( ICoreWindow *iface, ICoreDispatcher **out )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    if (!out) return E_POINTER;
    *out = &window->ICoreDispatcher_iface;
    ICoreDispatcher_AddRef( *out );
    return S_OK;
}

static HRESULT WINAPI window_get_FlowDirection( ICoreWindow *iface, CoreWindowFlowDirection *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ICoreWindow( iface )->flow;
    return S_OK;
}

static HRESULT WINAPI window_put_FlowDirection( ICoreWindow *iface, CoreWindowFlowDirection value )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    HRESULT hr;
    if (value != CoreWindowFlowDirection_LeftToRight && value != CoreWindowFlowDirection_RightToLeft) return E_INVALIDARG;
    if (FAILED(hr = window_check( window ))) return hr;
    window->flow = value;
    return S_OK;
}

static HRESULT WINAPI window_get_IsInputEnabled( ICoreWindow *iface, boolean *value )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr = window_check( window ))) return hr;
    *value = IsWindowEnabled( window->hwnd );
    return S_OK;
}

static HRESULT WINAPI window_put_IsInputEnabled( ICoreWindow *iface, boolean value )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    HRESULT hr;
    if (FAILED(hr = window_check( window ))) return hr;
    EnableWindow( window->hwnd, value );
    return S_OK;
}

static HRESULT WINAPI window_get_PointerCursor( ICoreWindow *iface, ICoreCursor **value )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = NULL;
    if (FAILED(hr = window_check( window ))) return hr;
    if ((*value = window->cursor)) ICoreCursor_AddRef( *value );
    return S_OK;
}

static void window_release_clip(struct core_window *window)
{
    RECT current;
    if (!window->clipped) return;
    if (GetClipCursor(&current) && EqualRect(&current, &window->clip_rect)) ClipCursor(NULL);
    window->clipped = FALSE;
}

static void window_update_clip(struct core_window *window)
{
    RECT rect;
    if (window->cursor || !window->hwnd || GetForegroundWindow() != window->hwnd ||
        !IsWindowVisible(window->hwnd) || IsIconic(window->hwnd))
    {
        window_release_clip(window);
        return;
    }
    GetClientRect(window->hwnd, &rect);
    MapWindowPoints(window->hwnd, NULL, (POINT *)&rect, 2);
    if (rect.right > rect.left && rect.bottom > rect.top && ClipCursor(&rect))
    {
        window->clip_rect = rect;
        window->clipped = TRUE;
    }
}

static HRESULT WINAPI window_put_PointerCursor( ICoreWindow *iface, ICoreCursor *value )
{
    struct core_window *window = impl_from_ICoreWindow(iface);
    HCURSOR cursor;
    ICoreCursor *old;
    POINT pos;
    HRESULT hr;
    if (FAILED(hr = window_check(window))) return hr;
    if (FAILED(hr = corecursor_handle(value, &cursor))) return hr;
    if (value) ICoreCursor_AddRef(value);
    old = window->cursor;
    window->cursor = value;
    window->native_cursor = cursor;
    if (old) ICoreCursor_Release(old);
    window_update_clip(window);
    if (GetCapture() == window->hwnd ||
        (GetCursorPos(&pos) && WindowFromPoint(pos) == window->hwnd)) SetCursor(cursor);
    return S_OK;
}

static HRESULT WINAPI window_get_PointerPosition( ICoreWindow *iface, Point *value )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    POINT point;
    float scale;
    HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr = window_check( window ))) return hr;
    if (!GetCursorPos( &point ) || !ScreenToClient( window->hwnd, &point )) return HRESULT_FROM_WIN32( GetLastError() );
    scale = 96.0f / GetDpiForWindow( window->hwnd );
    value->X = point.x * scale;
    value->Y = point.y * scale;
    return S_OK;
}

static HRESULT WINAPI window_get_Visible( ICoreWindow *iface, boolean *value )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    HRESULT hr;
    if (!value) return E_POINTER;
    if (FAILED(hr = window_check( window ))) return hr;
    *value = IsWindowVisible( window->hwnd );
    return S_OK;
}

static HRESULT WINAPI window_Activate( ICoreWindow *iface )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    HRESULT hr;
    if (FAILED(hr = window_check( window ))) return hr;
    ShowWindow( window->hwnd, !window->state.activated && window->state.maximized &&
        !window->state.fullscreen ? SW_SHOWMAXIMIZED : SW_SHOW );
    window->state.activated = TRUE;
    SetForegroundWindow( window->hwnd );
    SetFocus( window->hwnd );
    return S_OK;
}

static HRESULT WINAPI window_Close( ICoreWindow *iface )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    if (window->thread != GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (window->hwnd) SendMessageW( window->hwnd, WM_CLOSE, 0, 0 );
    return S_OK;
}

static HRESULT WINAPI window_GetAsyncKeyState( ICoreWindow *iface, VirtualKey key, CoreVirtualKeyStates *state )
{
    SHORT value;
    if (!state) return E_POINTER;
    value = GetAsyncKeyState( key );
    *state = value & 0x8000 ? CoreVirtualKeyStates_Down : CoreVirtualKeyStates_None;
    return S_OK;
}

static HRESULT WINAPI window_GetKeyState( ICoreWindow *iface, VirtualKey key, CoreVirtualKeyStates *state )
{
    SHORT value;
    if (!state) return E_POINTER;
    value = GetKeyState( key );
    *state = (value & 0x8000 ? CoreVirtualKeyStates_Down : 0) | (value & 1 ? CoreVirtualKeyStates_Locked : 0);
    return S_OK;
}

static HRESULT WINAPI window_ReleasePointerCapture( ICoreWindow *iface )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    HRESULT hr;
    if (FAILED(hr = window_check( window ))) return hr;
    if (GetCapture() == window->hwnd) ReleaseCapture();
    return S_OK;
}

static HRESULT WINAPI window_SetPointerCapture( ICoreWindow *iface )
{
    struct core_window *window = impl_from_ICoreWindow( iface );
    HRESULT hr;
    if (FAILED(hr = window_check( window ))) return hr;
    SetCapture( window->hwnd );
    return GetCapture() == window->hwnd ? S_OK : E_FAIL;
}

static HRESULT WINAPI window_add_Activated( ICoreWindow *iface, ITypedEventHandler_CoreWindow_WindowActivatedEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_Activated], handler, token );
}

static HRESULT WINAPI window_remove_Activated( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_Activated], token );
}

static HRESULT WINAPI window_add_AutomationProviderRequested( ICoreWindow *iface, ITypedEventHandler_CoreWindow_AutomationProviderRequestedEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_AutomationProviderRequested], handler, token );
}

static HRESULT WINAPI window_remove_AutomationProviderRequested( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_AutomationProviderRequested], token );
}

static HRESULT WINAPI window_add_CharacterReceived( ICoreWindow *iface, ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_CharacterReceived], handler, token );
}

static HRESULT WINAPI window_remove_CharacterReceived( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_CharacterReceived], token );
}

static HRESULT WINAPI window_add_Closed( ICoreWindow *iface, ITypedEventHandler_CoreWindow_CoreWindowEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_Closed], handler, token );
}

static HRESULT WINAPI window_remove_Closed( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_Closed], token );
}

static HRESULT WINAPI window_add_InputEnabled( ICoreWindow *iface, ITypedEventHandler_CoreWindow_InputEnabledEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_InputEnabled], handler, token );
}

static HRESULT WINAPI window_remove_InputEnabled( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_InputEnabled], token );
}

static HRESULT WINAPI window_add_KeyDown( ICoreWindow *iface, ITypedEventHandler_CoreWindow_KeyEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_KeyDown], handler, token );
}

static HRESULT WINAPI window_remove_KeyDown( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_KeyDown], token );
}

static HRESULT WINAPI window_add_KeyUp( ICoreWindow *iface, ITypedEventHandler_CoreWindow_KeyEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_KeyUp], handler, token );
}

static HRESULT WINAPI window_remove_KeyUp( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_KeyUp], token );
}

static HRESULT WINAPI window_add_PointerCaptureLost( ICoreWindow *iface, ITypedEventHandler_CoreWindow_PointerEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerCaptureLost], handler, token );
}

static HRESULT WINAPI window_remove_PointerCaptureLost( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerCaptureLost], token );
}

static HRESULT WINAPI window_add_PointerEntered( ICoreWindow *iface, ITypedEventHandler_CoreWindow_PointerEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerEntered], handler, token );
}

static HRESULT WINAPI window_remove_PointerEntered( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerEntered], token );
}

static HRESULT WINAPI window_add_PointerExited( ICoreWindow *iface, ITypedEventHandler_CoreWindow_PointerEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerExited], handler, token );
}

static HRESULT WINAPI window_remove_PointerExited( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerExited], token );
}

static HRESULT WINAPI window_add_PointerMoved( ICoreWindow *iface, ITypedEventHandler_CoreWindow_PointerEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerMoved], handler, token );
}

static HRESULT WINAPI window_remove_PointerMoved( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerMoved], token );
}

static HRESULT WINAPI window_add_PointerPressed( ICoreWindow *iface, ITypedEventHandler_CoreWindow_PointerEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerPressed], handler, token );
}

static HRESULT WINAPI window_remove_PointerPressed( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerPressed], token );
}

static HRESULT WINAPI window_add_PointerReleased( ICoreWindow *iface, ITypedEventHandler_CoreWindow_PointerEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerReleased], handler, token );
}

static HRESULT WINAPI window_remove_PointerReleased( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerReleased], token );
}

static HRESULT WINAPI window_add_TouchHitTesting( ICoreWindow *iface, ITypedEventHandler_CoreWindow_TouchHitTestingEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_TouchHitTesting], handler, token );
}

static HRESULT WINAPI window_remove_TouchHitTesting( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_TouchHitTesting], token );
}

static HRESULT WINAPI window_add_PointerWheelChanged( ICoreWindow *iface, ITypedEventHandler_CoreWindow_PointerEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerWheelChanged], handler, token );
}

static HRESULT WINAPI window_remove_PointerWheelChanged( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_PointerWheelChanged], token );
}

static HRESULT WINAPI window_add_SizeChanged( ICoreWindow *iface, ITypedEventHandler_CoreWindow_WindowSizeChangedEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_SizeChanged], handler, token );
}

static HRESULT WINAPI window_remove_SizeChanged( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_SizeChanged], token );
}

static HRESULT WINAPI window_add_VisibilityChanged( ICoreWindow *iface, ITypedEventHandler_CoreWindow_VisibilityChangedEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_VisibilityChanged], handler, token );
}

static HRESULT WINAPI window_remove_VisibilityChanged( ICoreWindow *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreWindow( iface )->events[WINDOW_EVENT_VisibilityChanged], token );
}

static const ICoreWindowVtbl window_vtbl =
{
    window_QueryInterface,
    window_AddRef,
    window_Release,
    window_GetIids,
    window_GetRuntimeClassName,
    window_GetTrustLevel,
    window_get_AutomationHostProvider,
    window_get_Bounds,
    window_get_CustomProperties,
    window_get_Dispatcher,
    window_get_FlowDirection,
    window_put_FlowDirection,
    window_get_IsInputEnabled,
    window_put_IsInputEnabled,
    window_get_PointerCursor,
    window_put_PointerCursor,
    window_get_PointerPosition,
    window_get_Visible,
    window_Activate,
    window_Close,
    window_GetAsyncKeyState,
    window_GetKeyState,
    window_ReleasePointerCapture,
    window_SetPointerCapture,
    window_add_Activated,
    window_remove_Activated,
    window_add_AutomationProviderRequested,
    window_remove_AutomationProviderRequested,
    window_add_CharacterReceived,
    window_remove_CharacterReceived,
    window_add_Closed,
    window_remove_Closed,
    window_add_InputEnabled,
    window_remove_InputEnabled,
    window_add_KeyDown,
    window_remove_KeyDown,
    window_add_KeyUp,
    window_remove_KeyUp,
    window_add_PointerCaptureLost,
    window_remove_PointerCaptureLost,
    window_add_PointerEntered,
    window_remove_PointerEntered,
    window_add_PointerExited,
    window_remove_PointerExited,
    window_add_PointerMoved,
    window_remove_PointerMoved,
    window_add_PointerPressed,
    window_remove_PointerPressed,
    window_add_PointerReleased,
    window_remove_PointerReleased,
    window_add_TouchHitTesting,
    window_remove_TouchHitTesting,
    window_add_PointerWheelChanged,
    window_remove_PointerWheelChanged,
    window_add_SizeChanged,
    window_remove_SizeChanged,
    window_add_VisibilityChanged,
    window_remove_VisibilityChanged
};

DEFINE_IINSPECTABLE(window2, ICoreWindow2, struct core_window, ICoreWindow_iface)
static HRESULT WINAPI window2_put_PointerPosition(ICoreWindow2 *iface, Point value)
{
    struct core_window *window=impl_from_ICoreWindow2(iface);
    float scale;
    POINT point;
    HRESULT hr;
    if (FAILED(hr=window_check(window))) return hr;
    scale=GetDpiForWindow(window->hwnd)/96.0f;
    point.x=value.X*scale; point.y=value.Y*scale;
    if (!ClientToScreen(window->hwnd,&point) || !SetCursorPos(point.x,point.y)) return HRESULT_FROM_WIN32(GetLastError());
    return S_OK;
}
static const ICoreWindow2Vtbl window2_vtbl =
{ window2_QueryInterface,window2_AddRef,window2_Release,window2_GetIids,window2_GetRuntimeClassName,window2_GetTrustLevel,window2_put_PointerPosition };
DEFINE_IINSPECTABLE(window4, ICoreWindow4, struct core_window, ICoreWindow_iface)
static HRESULT WINAPI window4_add_ResizeStarted(ICoreWindow4 *iface, ITypedEventHandler_CoreWindow_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&impl_from_ICoreWindow4(iface)->events[WINDOW_EVENT_ResizeStarted],handler,token); }
static HRESULT WINAPI window4_remove_ResizeStarted(ICoreWindow4 *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_ICoreWindow4(iface)->events[WINDOW_EVENT_ResizeStarted],token); }
static HRESULT WINAPI window4_add_ResizeCompleted(ICoreWindow4 *iface, ITypedEventHandler_CoreWindow_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&impl_from_ICoreWindow4(iface)->events[WINDOW_EVENT_ResizeCompleted],handler,token); }
static HRESULT WINAPI window4_remove_ResizeCompleted(ICoreWindow4 *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_ICoreWindow4(iface)->events[WINDOW_EVENT_ResizeCompleted],token); }
static const ICoreWindow4Vtbl window4_vtbl =
{ window4_QueryInterface,window4_AddRef,window4_Release,window4_GetIids,window4_GetRuntimeClassName,window4_GetTrustLevel,
  window4_add_ResizeStarted,window4_remove_ResizeStarted,window4_add_ResizeCompleted,window4_remove_ResizeCompleted };

static struct core_window *impl_from_ICoreWindowInterop( ICoreWindowInterop *iface )
{
    return CONTAINING_RECORD( iface, struct core_window, ICoreWindowInterop_iface );
}
static HRESULT WINAPI interop_QueryInterface( ICoreWindowInterop *iface, REFIID iid, void **out )
{
    return window_QueryInterface( &impl_from_ICoreWindowInterop( iface )->ICoreWindow_iface, iid, out );
}
static ULONG WINAPI interop_AddRef( ICoreWindowInterop *iface )
{
    return window_AddRef( &impl_from_ICoreWindowInterop( iface )->ICoreWindow_iface );
}
static ULONG WINAPI interop_Release( ICoreWindowInterop *iface )
{
    return window_Release( &impl_from_ICoreWindowInterop( iface )->ICoreWindow_iface );
}
static HRESULT WINAPI interop_get_WindowHandle( ICoreWindowInterop *iface, HWND *hwnd )
{
    struct core_window *window = impl_from_ICoreWindowInterop( iface );
    if (!hwnd) return E_POINTER;
    *hwnd = window->hwnd;
    return *hwnd ? S_OK : RO_E_CLOSED;
}
static HRESULT WINAPI interop_put_MessageHandled( ICoreWindowInterop *iface, boolean handled )
{
    struct core_window *window = impl_from_ICoreWindowInterop( iface );
    HRESULT hr;
    if (FAILED(hr = window_check( window ))) return hr;
    window->message_handled = handled;
    return S_OK;
}
static const ICoreWindowInteropVtbl interop_vtbl =
{
    interop_QueryInterface, interop_AddRef, interop_Release, interop_get_WindowHandle, interop_put_MessageHandled
};

static struct core_window *impl_from_ICoreDispatcher( ICoreDispatcher *iface )
{
    return CONTAINING_RECORD( iface, struct core_window, ICoreDispatcher_iface );
}
static HRESULT WINAPI dispatcher_QueryInterface( ICoreDispatcher *iface, REFIID iid, void **out )
{
    struct core_window *window=impl_from_ICoreDispatcher(iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid,&IID_ICoreAcceleratorKeys)) *out=&window->ICoreAcceleratorKeys_iface;
    else if (IsEqualGUID(iid,&IID_ICoreDispatcherWithTaskPriority)) *out=&window->ICoreDispatcherWithTaskPriority_iface;
    else if (IsEqualGUID(iid,&IID_IUnknown) || IsEqualGUID(iid,&IID_IInspectable) ||
             IsEqualGUID(iid,&IID_IAgileObject) || IsEqualGUID(iid,&IID_ICoreDispatcher)) *out=iface;
    else { FIXME("dispatcher interface %s not implemented.\n",debugstr_guid(iid)); return E_NOINTERFACE; }
    ICoreDispatcher_AddRef(iface); return S_OK;
}
static ULONG WINAPI dispatcher_AddRef( ICoreDispatcher *iface )
{
    return window_AddRef( &impl_from_ICoreDispatcher( iface )->ICoreWindow_iface );
}
static ULONG WINAPI dispatcher_Release( ICoreDispatcher *iface )
{
    return window_Release( &impl_from_ICoreDispatcher( iface )->ICoreWindow_iface );
}
static HRESULT WINAPI dispatcher_GetIids( ICoreDispatcher *iface, ULONG *count, IID **iids )
{
    HRESULT hr = window_GetIids( &impl_from_ICoreDispatcher( iface )->ICoreWindow_iface, count, iids );
    if (SUCCEEDED(hr)) **iids = IID_ICoreDispatcher;
    return hr;
}
static HRESULT WINAPI dispatcher_GetRuntimeClassName( ICoreDispatcher *iface, HSTRING *name )
{
    const WCHAR *str = RuntimeClass_Windows_UI_Core_CoreDispatcher;
    return WindowsCreateString( str, wcslen( str ), name );
}
static HRESULT WINAPI dispatcher_GetTrustLevel( ICoreDispatcher *iface, TrustLevel *level )
{
    return window_GetTrustLevel( &impl_from_ICoreDispatcher( iface )->ICoreWindow_iface, level );
}
static HRESULT WINAPI dispatcher_get_HasThreadAccess( ICoreDispatcher *iface, boolean *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ICoreDispatcher( iface )->thread == GetCurrentThreadId();
    return S_OK;
}
static HRESULT WINAPI dispatcher_ProcessEvents( ICoreDispatcher *iface, CoreProcessEventsOption options )
{
    struct core_window *window = impl_from_ICoreDispatcher( iface );
    MSG msg;
    BOOL wait, pending, ret;
    if (window->thread != GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (options < CoreProcessEventsOption_ProcessOneAndAllPending || options > CoreProcessEventsOption_ProcessAllIfPresent)
        return E_INVALIDARG;
    wait = options == CoreProcessEventsOption_ProcessOneAndAllPending || options == CoreProcessEventsOption_ProcessUntilQuit;
    window->stop_events=FALSE;
    while (window->hwnd && !window->stop_events)
    {
        pending = PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE );
        if (!pending)
        {
            if (!wait) break;
            ret = GetMessageW( &msg, NULL, 0, 0 );
            if (ret == -1) return HRESULT_FROM_WIN32( GetLastError() );
            if (!ret) break;
        }
        if (msg.message == WM_QUIT) break;
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
        if (options == CoreProcessEventsOption_ProcessOneIfPresent) break;
        wait = options == CoreProcessEventsOption_ProcessUntilQuit;
    }
    return S_OK;
}
static HRESULT WINAPI dispatcher_RunAsync( ICoreDispatcher *iface, CoreDispatcherPriority priority,
                                           IDispatchedHandler *callback, IAsyncAction **action )
{
    return dispatcher_queue_add(&impl_from_ICoreDispatcher(iface)->queue,priority,callback,action);
}
static HRESULT WINAPI dispatcher_RunIdleAsync( ICoreDispatcher *iface, IIdleDispatchedHandler *callback, IAsyncAction **action )
{
    FIXME( "callback %p stub!\n", callback );
    if (!action) return E_POINTER;
    *action = NULL;
    return E_NOTIMPL;
}
static const ICoreDispatcherVtbl dispatcher_vtbl =
{
    dispatcher_QueryInterface, dispatcher_AddRef, dispatcher_Release, dispatcher_GetIids,
    dispatcher_GetRuntimeClassName, dispatcher_GetTrustLevel, dispatcher_get_HasThreadAccess,
    dispatcher_ProcessEvents, dispatcher_RunAsync, dispatcher_RunIdleAsync
};

DEFINE_IINSPECTABLE(priority,ICoreDispatcherWithTaskPriority,struct core_window,ICoreDispatcher_iface)
static HRESULT WINAPI priority_get_CurrentPriority(ICoreDispatcherWithTaskPriority *iface, CoreDispatcherPriority *value)
{
    struct core_window *window=impl_from_ICoreDispatcherWithTaskPriority(iface);
    if (!value) return E_POINTER;
    if (window->thread!=GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    *value=window->queue.current_priority; return S_OK;
}
static HRESULT WINAPI priority_put_CurrentPriority(ICoreDispatcherWithTaskPriority *iface, CoreDispatcherPriority value)
{
    struct core_window *window=impl_from_ICoreDispatcherWithTaskPriority(iface);
    if (window->thread!=GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (value<CoreDispatcherPriority_Idle || value>CoreDispatcherPriority_High) return E_INVALIDARG;
    window->queue.current_priority=value; return S_OK;
}
static HRESULT WINAPI priority_ShouldYieldToPriority(ICoreDispatcherWithTaskPriority *iface, CoreDispatcherPriority priority, boolean *value)
{
    struct core_window *window=impl_from_ICoreDispatcherWithTaskPriority(iface);
    if (!value) return E_POINTER;
    if (window->thread!=GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (priority<CoreDispatcherPriority_Idle || priority>CoreDispatcherPriority_High) return E_INVALIDARG;
    *value=dispatcher_queue_should_yield(&window->queue,priority); return S_OK;
}
static HRESULT WINAPI priority_ShouldYield(ICoreDispatcherWithTaskPriority *iface, boolean *value)
{ return priority_ShouldYieldToPriority(iface,impl_from_ICoreDispatcherWithTaskPriority(iface)->queue.current_priority,value); }
static HRESULT WINAPI priority_StopProcessEvents(ICoreDispatcherWithTaskPriority *iface)
{
    struct core_window *window=impl_from_ICoreDispatcherWithTaskPriority(iface);
    if (window->thread!=GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    window->stop_events=TRUE; return S_OK;
}
static const ICoreDispatcherWithTaskPriorityVtbl priority_vtbl={priority_QueryInterface,priority_AddRef,priority_Release,
    priority_GetIids,priority_GetRuntimeClassName,priority_GetTrustLevel,priority_get_CurrentPriority,priority_put_CurrentPriority,
    priority_ShouldYield,priority_ShouldYieldToPriority,priority_StopProcessEvents};

DEFINE_IINSPECTABLE(accelerator, ICoreAcceleratorKeys, struct core_window, ICoreDispatcher_iface)
static HRESULT WINAPI accelerator_add_AcceleratorKeyActivated(ICoreAcceleratorKeys *iface,
    ITypedEventHandler_CoreDispatcher_AcceleratorKeyEventArgs *handler, EventRegistrationToken *token)
{
    struct core_window *window=impl_from_ICoreAcceleratorKeys(iface);
    HRESULT hr=window_check(window);
    return FAILED(hr) ? hr : winrt_event_add(&window->events[WINDOW_EVENT_AcceleratorKeyActivated],handler,token);
}
static HRESULT WINAPI accelerator_remove_AcceleratorKeyActivated(ICoreAcceleratorKeys *iface, EventRegistrationToken token)
{ return winrt_event_remove(&impl_from_ICoreAcceleratorKeys(iface)->events[WINDOW_EVENT_AcceleratorKeyActivated],token); }
static const ICoreAcceleratorKeysVtbl accelerator_vtbl={accelerator_QueryInterface,accelerator_AddRef,accelerator_Release,
    accelerator_GetIids,accelerator_GetRuntimeClassName,accelerator_GetTrustLevel,
    accelerator_add_AcceleratorKeyActivated,accelerator_remove_AcceleratorKeyActivated};

struct window_event_args
{
    ICoreWindowEventArgs ICoreWindowEventArgs_iface;
    IWindowSizeChangedEventArgs IWindowSizeChangedEventArgs_iface;
    IVisibilityChangedEventArgs IVisibilityChangedEventArgs_iface;
    IWindowActivatedEventArgs IWindowActivatedEventArgs_iface;
    IInputEnabledEventArgs IInputEnabledEventArgs_iface;
    IAcceleratorKeyEventArgs IAcceleratorKeyEventArgs_iface;
    IKeyEventArgs IKeyEventArgs_iface;
    ICharacterReceivedEventArgs ICharacterReceivedEventArgs_iface;
    VirtualKey key;
    CorePhysicalKeyStatus key_status;
    CoreAcceleratorKeyEventType key_type;
    LONG ref;
    unsigned int kind;
    boolean handled;
    Size size;
    boolean visible, input;
    CoreWindowActivationState activated;
};
static struct window_event_args *impl_from_ICoreWindowEventArgs( ICoreWindowEventArgs *iface )
{
    return CONTAINING_RECORD( iface, struct window_event_args, ICoreWindowEventArgs_iface );
}
static HRESULT WINAPI args_QueryInterface( ICoreWindowEventArgs *iface, REFIID iid, void **out )
{
    struct window_event_args *args = impl_from_ICoreWindowEventArgs( iface );
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_ICoreWindowEventArgs )) *out = iface;
    else if (args->kind == 1 && IsEqualGUID( iid, &IID_IWindowSizeChangedEventArgs )) *out = &args->IWindowSizeChangedEventArgs_iface;
    else if (args->kind == 2 && IsEqualGUID( iid, &IID_IVisibilityChangedEventArgs )) *out = &args->IVisibilityChangedEventArgs_iface;
    else if (args->kind == 3 && IsEqualGUID( iid, &IID_IWindowActivatedEventArgs )) *out = &args->IWindowActivatedEventArgs_iface;
    else if (args->kind == 4 && IsEqualGUID( iid, &IID_IInputEnabledEventArgs )) *out = &args->IInputEnabledEventArgs_iface;
    else if (args->kind == 5 && IsEqualGUID(iid,&IID_IAcceleratorKeyEventArgs)) *out=&args->IAcceleratorKeyEventArgs_iface;
    else if (args->kind == 6 && IsEqualGUID(iid,&IID_IKeyEventArgs)) *out=&args->IKeyEventArgs_iface;
    else if (args->kind == 7 && IsEqualGUID(iid,&IID_ICharacterReceivedEventArgs)) *out=&args->ICharacterReceivedEventArgs_iface;
    else return E_NOINTERFACE;
    ICoreWindowEventArgs_AddRef( iface );
    return S_OK;
}
static ULONG WINAPI args_AddRef( ICoreWindowEventArgs *iface )
{
    return InterlockedIncrement( &impl_from_ICoreWindowEventArgs( iface )->ref );
}
static ULONG WINAPI args_Release( ICoreWindowEventArgs *iface )
{
    struct window_event_args *args = impl_from_ICoreWindowEventArgs( iface );
    ULONG ref = InterlockedDecrement( &args->ref );
    if (!ref) free( args );
    return ref;
}
static HRESULT WINAPI args_GetIids( ICoreWindowEventArgs *iface, ULONG *count, IID **iids )
{
    static const IID *const specialized[] =
    {
        NULL, &IID_IWindowSizeChangedEventArgs, &IID_IVisibilityChangedEventArgs,
        &IID_IWindowActivatedEventArgs, &IID_IInputEnabledEventArgs, &IID_IAcceleratorKeyEventArgs, &IID_IKeyEventArgs, &IID_ICharacterReceivedEventArgs
    };
    struct window_event_args *args = impl_from_ICoreWindowEventArgs( iface );
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( 2 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_ICoreWindowEventArgs;
    *count = 1;
    if (args->kind) (*iids)[(*count)++] = *specialized[args->kind];
    return S_OK;
}
static HRESULT WINAPI args_GetRuntimeClassName( ICoreWindowEventArgs *iface, HSTRING *name )
{
    static const WCHAR *const names[] =
    {
        RuntimeClass_Windows_UI_Core_CoreWindowEventArgs,
        RuntimeClass_Windows_UI_Core_WindowSizeChangedEventArgs,
        RuntimeClass_Windows_UI_Core_VisibilityChangedEventArgs,
        RuntimeClass_Windows_UI_Core_WindowActivatedEventArgs,
        RuntimeClass_Windows_UI_Core_InputEnabledEventArgs,
        RuntimeClass_Windows_UI_Core_AcceleratorKeyEventArgs,
        RuntimeClass_Windows_UI_Core_KeyEventArgs,
        RuntimeClass_Windows_UI_Core_CharacterReceivedEventArgs
    };
    const WCHAR *str = names[impl_from_ICoreWindowEventArgs( iface )->kind];
    return WindowsCreateString( str, wcslen( str ), name );
}
static HRESULT WINAPI args_GetTrustLevel( ICoreWindowEventArgs *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}
static HRESULT WINAPI args_get_Handled( ICoreWindowEventArgs *iface, boolean *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ICoreWindowEventArgs( iface )->handled;
    return S_OK;
}
static HRESULT WINAPI args_put_Handled( ICoreWindowEventArgs *iface, boolean value )
{
    impl_from_ICoreWindowEventArgs( iface )->handled = value;
    return S_OK;
}
static const ICoreWindowEventArgsVtbl args_vtbl =
{
    args_QueryInterface, args_AddRef, args_Release, args_GetIids,
    args_GetRuntimeClassName, args_GetTrustLevel, args_get_Handled, args_put_Handled
};

DEFINE_IINSPECTABLE( args_size, IWindowSizeChangedEventArgs, struct window_event_args, ICoreWindowEventArgs_iface )
static HRESULT WINAPI args_size_get_Size( IWindowSizeChangedEventArgs *iface, Size *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_IWindowSizeChangedEventArgs( iface )->size;
    return S_OK;
}
static const IWindowSizeChangedEventArgsVtbl args_size_vtbl =
{
    args_size_QueryInterface, args_size_AddRef, args_size_Release, args_size_GetIids,
    args_size_GetRuntimeClassName, args_size_GetTrustLevel, args_size_get_Size
};

DEFINE_IINSPECTABLE( args_visible, IVisibilityChangedEventArgs, struct window_event_args, ICoreWindowEventArgs_iface )
static HRESULT WINAPI args_visible_get_Visible( IVisibilityChangedEventArgs *iface, boolean *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_IVisibilityChangedEventArgs( iface )->visible;
    return S_OK;
}
static const IVisibilityChangedEventArgsVtbl args_visible_vtbl =
{
    args_visible_QueryInterface, args_visible_AddRef, args_visible_Release, args_visible_GetIids,
    args_visible_GetRuntimeClassName, args_visible_GetTrustLevel, args_visible_get_Visible
};

DEFINE_IINSPECTABLE( args_activated, IWindowActivatedEventArgs, struct window_event_args, ICoreWindowEventArgs_iface )
static HRESULT WINAPI args_activated_get_WindowActivationState( IWindowActivatedEventArgs *iface, CoreWindowActivationState *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_IWindowActivatedEventArgs( iface )->activated;
    return S_OK;
}
static const IWindowActivatedEventArgsVtbl args_activated_vtbl =
{
    args_activated_QueryInterface, args_activated_AddRef, args_activated_Release, args_activated_GetIids,
    args_activated_GetRuntimeClassName, args_activated_GetTrustLevel, args_activated_get_WindowActivationState
};

DEFINE_IINSPECTABLE( args_input, IInputEnabledEventArgs, struct window_event_args, ICoreWindowEventArgs_iface )
static HRESULT WINAPI args_input_get_InputEnabled( IInputEnabledEventArgs *iface, boolean *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_IInputEnabledEventArgs( iface )->input;
    return S_OK;
}
static const IInputEnabledEventArgsVtbl args_input_vtbl =
{
    args_input_QueryInterface, args_input_AddRef, args_input_Release, args_input_GetIids,
    args_input_GetRuntimeClassName, args_input_GetTrustLevel, args_input_get_InputEnabled
};


DEFINE_IINSPECTABLE(args_accel, IAcceleratorKeyEventArgs, struct window_event_args, ICoreWindowEventArgs_iface)
static HRESULT WINAPI args_accel_get_EventType(IAcceleratorKeyEventArgs *iface, CoreAcceleratorKeyEventType *value)
{ if (!value) return E_POINTER; *value=impl_from_IAcceleratorKeyEventArgs(iface)->key_type; return S_OK; }
static HRESULT WINAPI args_accel_get_VirtualKey(IAcceleratorKeyEventArgs *iface, VirtualKey *value)
{ if (!value) return E_POINTER; *value=impl_from_IAcceleratorKeyEventArgs(iface)->key; return S_OK; }
static HRESULT WINAPI args_accel_get_KeyStatus(IAcceleratorKeyEventArgs *iface, CorePhysicalKeyStatus *value)
{ if (!value) return E_POINTER; *value=impl_from_IAcceleratorKeyEventArgs(iface)->key_status; return S_OK; }
static const IAcceleratorKeyEventArgsVtbl args_accel_vtbl={args_accel_QueryInterface,args_accel_AddRef,args_accel_Release,args_accel_GetIids,args_accel_GetRuntimeClassName,args_accel_GetTrustLevel,args_accel_get_EventType,args_accel_get_VirtualKey,args_accel_get_KeyStatus};

DEFINE_IINSPECTABLE(args_key, IKeyEventArgs, struct window_event_args, ICoreWindowEventArgs_iface)
static HRESULT WINAPI args_key_get_VirtualKey(IKeyEventArgs *iface, VirtualKey *value)
{ if (!value) return E_POINTER; *value=impl_from_IKeyEventArgs(iface)->key; return S_OK; }
static HRESULT WINAPI args_key_get_KeyStatus(IKeyEventArgs *iface, CorePhysicalKeyStatus *value)
{ if (!value) return E_POINTER; *value=impl_from_IKeyEventArgs(iface)->key_status; return S_OK; }
static const IKeyEventArgsVtbl args_key_vtbl={args_key_QueryInterface,args_key_AddRef,args_key_Release,args_key_GetIids,args_key_GetRuntimeClassName,args_key_GetTrustLevel,args_key_get_VirtualKey,args_key_get_KeyStatus};

DEFINE_IINSPECTABLE(args_character, ICharacterReceivedEventArgs, struct window_event_args, ICoreWindowEventArgs_iface)
static HRESULT WINAPI args_character_get_KeyCode(ICharacterReceivedEventArgs *iface, UINT32 *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_ICharacterReceivedEventArgs(iface)->key;
    return S_OK;
}
static HRESULT WINAPI args_character_get_KeyStatus(ICharacterReceivedEventArgs *iface, CorePhysicalKeyStatus *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_ICharacterReceivedEventArgs(iface)->key_status;
    return S_OK;
}
static const ICharacterReceivedEventArgsVtbl args_character_vtbl =
{
    args_character_QueryInterface, args_character_AddRef, args_character_Release,
    args_character_GetIids, args_character_GetRuntimeClassName, args_character_GetTrustLevel,
    args_character_get_KeyCode, args_character_get_KeyStatus
};

static struct window_event_args *create_window_args( unsigned int kind )
{
    struct window_event_args *args = calloc( 1, sizeof(*args) );
    if (!args) return NULL;
    args->ICoreWindowEventArgs_iface.lpVtbl = &args_vtbl;
    args->IWindowSizeChangedEventArgs_iface.lpVtbl = &args_size_vtbl;
    args->IVisibilityChangedEventArgs_iface.lpVtbl = &args_visible_vtbl;
    args->IWindowActivatedEventArgs_iface.lpVtbl = &args_activated_vtbl;
    args->IInputEnabledEventArgs_iface.lpVtbl = &args_input_vtbl;
    args->IAcceleratorKeyEventArgs_iface.lpVtbl=&args_accel_vtbl;
    args->IKeyEventArgs_iface.lpVtbl=&args_key_vtbl;
    args->ICharacterReceivedEventArgs_iface.lpVtbl=&args_character_vtbl;
    args->ref = 1;
    args->kind = kind;
    return args;
}

static BOOL window_pointer_event(struct core_window *window, unsigned int event, UINT msg, WPARAM wparam, LPARAM lparam)
{
    IPointerEventArgs *args;
    ICoreWindowEventArgs *base;
    boolean handled = FALSE;
    if (FAILED(pointer_args_create(window->hwnd, msg, wparam, lparam, &args))) return FALSE;
    winrt_event_notify(&window->events[event], &window->ICoreWindow_iface, args);
    if (SUCCEEDED(IPointerEventArgs_QueryInterface(args, &IID_ICoreWindowEventArgs, (void **)&base)))
    {
        ICoreWindowEventArgs_get_Handled(base, &handled);
        ICoreWindowEventArgs_Release(base);
    }
    IPointerEventArgs_Release(args);
    return handled;
}

static LRESULT CALLBACK core_window_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    struct core_window *window = (void *)GetWindowLongPtrW( hwnd, GWLP_USERDATA );
    struct window_event_args *args;
    LRESULT ret = 0;
    BOOL handled = FALSE;
    unsigned int i;

    if (msg == WM_NCCREATE)
    {
        window = ((CREATESTRUCTW *)lparam)->lpCreateParams;
        SetWindowLongPtrW( hwnd, GWLP_USERDATA, (LONG_PTR)window );
        window->hwnd = hwnd;
        window->queue.hwnd=hwnd;
        ICoreWindow_AddRef( &window->ICoreWindow_iface ); /* HWND ownership. */
    }
    if (!window) return DefWindowProcW( hwnd, msg, wparam, lparam );
    ICoreWindow_AddRef( &window->ICoreWindow_iface ); /* A callback may close the window. */
    window->message_handled = FALSE;
    switch (msg)
    {
    case WINE_WM_DISPATCH:
        dispatcher_queue_dispatch(&window->queue);
        handled=TRUE;
        break;
    case WM_TIMER:
        if (wparam == COREWINDOW_SAVE_TIMER) { corewindow_state_save(&window->state, hwnd); handled = TRUE; }
        break;
    case WM_WINDOWPOSCHANGED:
        corewindow_state_update(&window->state, hwnd);
        break;
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        if (wparam == VK_F11 || (wparam == VK_RETURN && (lparam & (1u << 29))))
        {
            if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !(lparam & (1u << 30)))
                corewindow_state_fullscreen(&window->state, hwnd, !window->state.fullscreen);
            handled = TRUE;
            break;
        }
        /* fall through */
    case WM_CHAR: case WM_SYSCHAR: case WM_DEADCHAR: case WM_SYSDEADCHAR:
        if (msg == WM_SYSCHAR && wparam == VK_RETURN && (lparam & (1u << 29)))
        { handled = TRUE; break; }
        if ((args=create_window_args(5)))
        {
            args->key=wparam;
            args->key_type=msg-WM_KEYDOWN;
            args->key_status.RepeatCount=LOWORD(lparam);
            args->key_status.ScanCode=(lparam>>16)&0xff;
            args->key_status.IsExtendedKey=!!(lparam & (1u<<24));
            args->key_status.IsMenuKeyDown=!!(lparam & (1u<<29));
            args->key_status.WasKeyDown=!!(lparam & (1u<<30));
            args->key_status.IsKeyReleased=!!(lparam & (1u<<31));
            winrt_event_notify(&window->events[WINDOW_EVENT_AcceleratorKeyActivated],&window->ICoreDispatcher_iface,&args->IAcceleratorKeyEventArgs_iface);
            if (!args->handled && (msg==WM_KEYDOWN || msg==WM_KEYUP || msg==WM_SYSKEYDOWN || msg==WM_SYSKEYUP))
            {
                struct window_event_args *key_args=create_window_args(6);
                if (key_args)
                {
                    key_args->key=args->key; key_args->key_status=args->key_status;
                    winrt_event_notify(&window->events[(msg==WM_KEYDOWN || msg==WM_SYSKEYDOWN) ? WINDOW_EVENT_KeyDown : WINDOW_EVENT_KeyUp],
                        &window->ICoreWindow_iface,&key_args->IKeyEventArgs_iface);
                    args->handled=key_args->handled;
                    ICoreWindowEventArgs_Release(&key_args->ICoreWindowEventArgs_iface);
                }
            }
            /* WM_CHAR already contains the layout/composition result. Preserve UTF-16
             * units (including surrogate pairs), and do not emit unfinished dead keys. */
            if (!args->handled && (msg == WM_CHAR || msg == WM_SYSCHAR))
            {
                struct window_event_args *character = create_window_args(7);
                if (character)
                {
                    character->key = args->key;
                    character->key_status = args->key_status;
                    winrt_event_notify(&window->events[WINDOW_EVENT_CharacterReceived],
                        &window->ICoreWindow_iface, &character->ICharacterReceivedEventArgs_iface);
                    args->handled = character->handled;
                    ICoreWindowEventArgs_Release(&character->ICoreWindowEventArgs_iface);
                }
            }
            handled=args->handled;
            ICoreWindowEventArgs_Release(&args->ICoreWindowEventArgs_iface);
        }
        break;
    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT)
        {
            SetCursor(window->native_cursor);
            handled = TRUE;
            ret = TRUE;
        }
        break;
    case WM_MOUSEMOVE:
        if (!window->tracking_mouse)
        {
            TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd, 0};
            window->tracking_mouse = TrackMouseEvent(&track);
            window_pointer_event(window, WINDOW_EVENT_PointerEntered, msg, wparam, lparam);
        }
        if (window->hwnd) handled = window_pointer_event(window, WINDOW_EVENT_PointerMoved, msg, wparam, lparam);
        break;
    case WM_MOUSELEAVE:
        window->tracking_mouse = FALSE;
        handled = window_pointer_event(window, WINDOW_EVENT_PointerExited, msg, wparam, lparam);
        break;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN:
    case WM_LBUTTONDBLCLK: case WM_RBUTTONDBLCLK: case WM_MBUTTONDBLCLK: case WM_XBUTTONDBLCLK:
        handled = window_pointer_event(window, WINDOW_EVENT_PointerPressed, msg, wparam, lparam);
        if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) { handled = TRUE; ret = TRUE; }
        break;
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP:
        handled = window_pointer_event(window, WINDOW_EVENT_PointerReleased, msg, wparam, lparam);
        if (msg == WM_XBUTTONUP) { handled = TRUE; ret = TRUE; }
        break;
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        handled = window_pointer_event(window, WINDOW_EVENT_PointerWheelChanged, msg, wparam, lparam);
        break;
    case WM_CAPTURECHANGED:
        window_pointer_event(window, WINDOW_EVENT_PointerCaptureLost, msg, wparam, lparam);
        break;
    case WM_KILLFOCUS:
        window_release_clip(window);
        break;
    case WM_SETFOCUS: case WM_MOVE:
        window_update_clip(window);
        break;
    case WM_INPUT:
        if (window->mouse) mouse_input(window->mouse,(HRAWINPUT)lparam);
        break;
    case WM_ENTERSIZEMOVE:
        winrt_event_notify(&window->events[WINDOW_EVENT_ResizeStarted],&window->ICoreWindow_iface,NULL);
        break;
    case WM_EXITSIZEMOVE:
        winrt_event_notify(&window->events[WINDOW_EVENT_ResizeCompleted],&window->ICoreWindow_iface,NULL);
        break;
    case WM_APPCOMMAND:
        if (GET_APPCOMMAND_LPARAM(lparam) == APPCOMMAND_BROWSER_BACKWARD && window->navigation)
            handled = navigation_back(window->navigation);
        break;
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == WINE_SC_BACK && window->navigation)
            handled = navigation_back(window->navigation);
        break;
    case WM_CLOSE:
        if ((args = create_window_args( 0 )))
        {
            winrt_event_notify( &window->events[WINDOW_EVENT_Closed], &window->ICoreWindow_iface, &args->ICoreWindowEventArgs_iface );
            handled = args->handled;
            ICoreWindowEventArgs_Release( &args->ICoreWindowEventArgs_iface );
        }
        if (!handled && window->hwnd) DestroyWindow( hwnd );
        handled = TRUE;
        break;
    case WM_SIZE:
        window_update_clip(window);
        if ((args = create_window_args( 1 )))
        {
            float scale = 96.0f / GetDpiForWindow( hwnd );
            args->size.Width = LOWORD(lparam) * scale;
            args->size.Height = HIWORD(lparam) * scale;
            winrt_event_notify( &window->events[WINDOW_EVENT_SizeChanged], &window->ICoreWindow_iface, &args->IWindowSizeChangedEventArgs_iface );
            ICoreWindowEventArgs_Release( &args->ICoreWindowEventArgs_iface );
        }
        break;
    case WM_SHOWWINDOW:
        if ((args = create_window_args( 2 )))
        {
            args->visible = !!wparam;
            winrt_event_notify( &window->events[WINDOW_EVENT_VisibilityChanged], &window->ICoreWindow_iface, &args->IVisibilityChangedEventArgs_iface );
            ICoreWindowEventArgs_Release( &args->ICoreWindowEventArgs_iface );
        }
        break;
    case WM_ACTIVATE:
        if ((args = create_window_args( 3 )))
        {
            args->activated = LOWORD(wparam) == WA_INACTIVE ? CoreWindowActivationState_Deactivated :
                              LOWORD(wparam) == WA_CLICKACTIVE ? CoreWindowActivationState_PointerActivated :
                              CoreWindowActivationState_CodeActivated;
            winrt_event_notify( &window->events[WINDOW_EVENT_Activated], &window->ICoreWindow_iface, &args->IWindowActivatedEventArgs_iface );
            ICoreWindowEventArgs_Release( &args->ICoreWindowEventArgs_iface );
        }
        break;
    case WM_ENABLE:
        if ((args = create_window_args( 4 )))
        {
            args->input = !!wparam;
            winrt_event_notify( &window->events[WINDOW_EVENT_InputEnabled], &window->ICoreWindow_iface, &args->IInputEnabledEventArgs_iface );
            ICoreWindowEventArgs_Release( &args->ICoreWindowEventArgs_iface );
        }
        break;
    case WM_NCDESTROY:
        corewindow_state_save(&window->state, hwnd);
        window_release_clip(window);
        SetWindowLongPtrW( hwnd, GWLP_USERDATA, 0 );
        window->hwnd = NULL;
        dispatcher_queue_close(&window->queue);
        if (window->navigation) navigation_close(window->navigation);
        if (window->mouse) mouse_close(window->mouse);
        if (TlsGetValue( window_tls ) == window) TlsSetValue( window_tls, NULL );
        for (i = 0; i < ARRAY_SIZE(window->events); ++i) winrt_event_clear( &window->events[i] );
        ICoreWindow_Release( &window->ICoreWindow_iface ); /* HWND ownership. */
        break;
    }
    if (!handled && !window->message_handled) ret = DefWindowProcW( hwnd, msg, wparam, lparam );
    ICoreWindow_Release( &window->ICoreWindow_iface );
    return ret;
}

static BOOL CALLBACK init_core_window( INIT_ONCE *once, void *param, void **context )
{
    WNDCLASSEXW cls = {sizeof(cls)};
    DWORD tls = TlsAlloc();
    if (tls == TLS_OUT_OF_INDEXES) return FALSE;
    cls.lpfnWndProc = core_window_proc;
    cls.hInstance = GetModuleHandleW( L"windows.ui.dll" );
    cls.hCursor = LoadCursorW( NULL, (const WCHAR *)IDC_ARROW );
    cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    cls.lpszClassName = L"Windows.UI.Core.CoreWindow";
    if (!RegisterClassExW( &cls ) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        TlsFree( tls );
        return FALSE;
    }
    window_tls = tls;
    return TRUE;
}

HRESULT WINAPI __wine_create_core_window( ICoreWindow **out )
{
    struct core_window *window;
    WCHAR title[1024];
    RECT rect;
    DWORD error;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!InitOnceExecuteOnce( &window_init, init_core_window, NULL, NULL )) return HRESULT_FROM_WIN32( GetLastError() );
    if (TlsGetValue( window_tls )) return E_ILLEGAL_METHOD_CALL;
    if (!(window = calloc( 1, sizeof(*window) ))) return E_OUTOFMEMORY;
    window->ICoreWindow_iface.lpVtbl = &window_vtbl;
    window->ICoreWindow2_iface.lpVtbl = &window2_vtbl;
    window->ICoreWindow4_iface.lpVtbl = &window4_vtbl;
    window->ICoreWindowInterop_iface.lpVtbl = &interop_vtbl;
    window->ICoreDispatcher_iface.lpVtbl = &dispatcher_vtbl;
    window->ICoreAcceleratorKeys_iface.lpVtbl = &accelerator_vtbl;
    window->ICoreDispatcherWithTaskPriority_iface.lpVtbl=&priority_vtbl;
    window->ref = 1;
    window->thread = GetCurrentThreadId();
    if (FAILED(hr = corecursor_create(CoreCursorType_Arrow, 0, &window->cursor)))
    { ICoreWindow_Release(&window->ICoreWindow_iface); return hr; }
    corecursor_handle(window->cursor, &window->native_cursor);
    /* Transfer references between apartments while window operations enforce the owner thread. */
    if (FAILED(hr=CoCreateFreeThreadedMarshaler((IUnknown *)&window->ICoreWindow_iface,&window->marshaler)))
    { ICoreWindow_Release(&window->ICoreWindow_iface); return hr; }
    corewindow_load_metadata( title, ARRAY_SIZE(title), &window->small_icon, &window->large_icon );
    corewindow_state_init(&window->state, &rect);
    if (!CreateWindowExW( 0, L"Windows.UI.Core.CoreWindow", title, WS_OVERLAPPEDWINDOW,
                         rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                         NULL, NULL, GetModuleHandleW( L"windows.ui.dll" ), window ))
    {
        error = GetLastError();
        ICoreWindow_Release( &window->ICoreWindow_iface );
        return HRESULT_FROM_WIN32( error );
    }
    if (window->small_icon) SendMessageW( window->hwnd, WM_SETICON, ICON_SMALL, (LPARAM)window->small_icon );
    if (window->large_icon) SendMessageW( window->hwnd, WM_SETICON, ICON_BIG, (LPARAM)window->large_icon );
    TlsSetValue( window_tls, window ); /* Borrowed while HWND owns a reference. */
    window->state.ready = TRUE;
    if (window->state.restore_fullscreen) corewindow_state_fullscreen(&window->state, window->hwnd, TRUE);
    *out = &window->ICoreWindow_iface;
    TRACE( "created window %p, hwnd %p, thread %lu\n", *out, window->hwnd, window->thread );
    return S_OK;
}

HRESULT WINAPI __wine_destroy_core_window( ICoreWindow *iface )
{
    struct core_window *window;
    if (!iface || iface->lpVtbl != &window_vtbl) return E_INVALIDARG;
    window = impl_from_ICoreWindow( iface );
    if (window->thread != GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (window->hwnd) DestroyWindow( window->hwnd );
    return S_OK;
}

HRESULT WINAPI __wine_core_window_set_fullscreen(ICoreWindow *iface, BOOL fullscreen)
{
    struct core_window *window;
    HRESULT hr;
    if (!iface || iface->lpVtbl != &window_vtbl) return E_INVALIDARG;
    window = impl_from_ICoreWindow(iface);
    if (FAILED(hr = window_check(window))) return hr;
    return corewindow_state_fullscreen(&window->state, window->hwnd, fullscreen);
}
HRESULT WINAPI __wine_core_window_get_fullscreen(ICoreWindow *iface, BOOL *fullscreen)
{
    struct core_window *window;
    HRESULT hr;
    if (!fullscreen) return E_POINTER;
    *fullscreen = FALSE;
    if (!iface || iface->lpVtbl != &window_vtbl) return E_INVALIDARG;
    window = impl_from_ICoreWindow(iface);
    if (FAILED(hr = window_check(window))) return hr;
    *fullscreen = window->state.fullscreen;
    return S_OK;
}

struct corewindow_statics
{
    IActivationFactory IActivationFactory_iface;
    ICoreWindowStatic ICoreWindowStatic_iface;
    LONG ref;
};

static inline struct corewindow_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct corewindow_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct corewindow_statics *impl = impl_from_IActivationFactory( iface );

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
    else if (IsEqualGUID( iid, &IID_ICoreWindowStatic ))
    {
        *out = &impl->ICoreWindowStatic_iface;
        ICoreWindowStatic_AddRef( &impl->ICoreWindowStatic_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct corewindow_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct corewindow_statics *impl = impl_from_IActivationFactory( iface );
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

DEFINE_IINSPECTABLE( corewindow_static, ICoreWindowStatic, struct corewindow_statics, IActivationFactory_iface )

static HRESULT STDMETHODCALLTYPE corewindow_static_GetForCurrentThread( ICoreWindowStatic *iface, ICoreWindow **windows )
{
    struct core_window *window;
    if (!windows) return E_POINTER;
    *windows = NULL;
    window = window_tls == TLS_OUT_OF_INDEXES ? NULL : TlsGetValue( window_tls );
    if (window) ICoreWindow_AddRef( (*windows = &window->ICoreWindow_iface) );
    return S_OK;
}

static const struct ICoreWindowStaticVtbl corewindow_static_vtbl =
{
    corewindow_static_QueryInterface,
    corewindow_static_AddRef,
    corewindow_static_Release,
    /* IInspectable methods */
    corewindow_static_GetIids,
    corewindow_static_GetRuntimeClassName,
    corewindow_static_GetTrustLevel,
    /* ICoreWindowStatic methods */
    corewindow_static_GetForCurrentThread
};

static struct corewindow_statics corewindow_statics =
{
    {&factory_vtbl},
    {&corewindow_static_vtbl},
    1,
};

IActivationFactory *corewindow_factory = &corewindow_statics.IActivationFactory_iface;

HRESULT corewindow_get_navigation(ISystemNavigationManager **out)
{
    struct core_window *window;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    window = window_tls == TLS_OUT_OF_INDEXES ? NULL : TlsGetValue(window_tls);
    if (!window) return E_ILLEGAL_METHOD_CALL;
    if (!window->navigation && FAILED(hr = navigation_create(window->hwnd, &window->navigation))) return hr;
    ISystemNavigationManager_AddRef((*out = window->navigation));
    return S_OK;
}

HRESULT corewindow_get_pointervisualization(IPointerVisualizationSettings **out)
{
    struct core_window *window;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out=NULL;
    window=window_tls==TLS_OUT_OF_INDEXES ? NULL : TlsGetValue(window_tls);
    if (!window) return E_ILLEGAL_METHOD_CALL;
    if (!window->feedback && FAILED(hr=pointervisualization_create(&window->feedback))) return hr;
    IPointerVisualizationSettings_AddRef((*out=window->feedback));
    return S_OK;
}

HRESULT corewindow_get_mouse(IMouseDevice **out)
{
    struct core_window *window;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out=NULL;
    window=window_tls==TLS_OUT_OF_INDEXES ? NULL : TlsGetValue(window_tls);
    if (!window) return E_ILLEGAL_METHOD_CALL;
    if (!window->mouse && FAILED(hr=mouse_create(window->hwnd,&window->mouse))) return hr;
    IMouseDevice_AddRef((*out=window->mouse)); return S_OK;
}
