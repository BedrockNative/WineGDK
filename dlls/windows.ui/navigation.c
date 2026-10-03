/* WinRT system navigation. LGPL-2.1-or-later. */
#include "private.h"
#include "winuser.h"
#include "wine/debug.h"
#include "wine/winrt_events.h"
WINE_DEFAULT_DEBUG_CHANNEL(ui);

struct navigation
{
    ISystemNavigationManager ISystemNavigationManager_iface;
    ISystemNavigationManager2 ISystemNavigationManager2_iface;
    LONG ref;
    HWND hwnd;
    DWORD thread;
    AppViewBackButtonVisibility visibility;
    struct winrt_event back;
};
static struct navigation *impl_from_ISystemNavigationManager( ISystemNavigationManager *iface )
{ return CONTAINING_RECORD( iface, struct navigation, ISystemNavigationManager_iface ); }
static HRESULT WINAPI navigation_QueryInterface( ISystemNavigationManager *iface, REFIID iid, void **out )
{
    struct navigation *impl = impl_from_ISystemNavigationManager( iface );
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_ISystemNavigationManager)) *out = iface;
    else if (IsEqualGUID(iid, &IID_ISystemNavigationManager2)) *out = &impl->ISystemNavigationManager2_iface;
    else return E_NOINTERFACE;
    ISystemNavigationManager_AddRef(iface);
    return S_OK;
}
static ULONG WINAPI navigation_AddRef( ISystemNavigationManager *iface )
{ return InterlockedIncrement(&impl_from_ISystemNavigationManager(iface)->ref); }
static ULONG WINAPI navigation_Release( ISystemNavigationManager *iface )
{
    struct navigation *impl = impl_from_ISystemNavigationManager(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { winrt_event_clear(&impl->back); free(impl); }
    return ref;
}
static HRESULT WINAPI navigation_GetIids( ISystemNavigationManager *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc(2 * sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_ISystemNavigationManager;
    (*iids)[1] = IID_ISystemNavigationManager2;
    *count = 2;
    return S_OK;
}
static HRESULT WINAPI navigation_GetRuntimeClassName( ISystemNavigationManager *iface, HSTRING *name )
{
    const WCHAR *str = RuntimeClass_Windows_UI_Core_SystemNavigationManager;
    return WindowsCreateString(str, wcslen(str), name);
}
static HRESULT WINAPI navigation_GetTrustLevel( ISystemNavigationManager *iface, TrustLevel *level )
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }
static HRESULT WINAPI navigation_add_BackRequested( ISystemNavigationManager *iface, IEventHandler_BackRequestedEventArgs *handler, EventRegistrationToken *token )
{
    struct navigation *impl = impl_from_ISystemNavigationManager(iface);
    if (impl->thread != GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (!impl->hwnd) return RO_E_CLOSED;
    return winrt_event_add(&impl->back, handler, token);
}
static HRESULT WINAPI navigation_remove_BackRequested( ISystemNavigationManager *iface, EventRegistrationToken token )
{ return winrt_event_remove(&impl_from_ISystemNavigationManager(iface)->back, token); }
static const ISystemNavigationManagerVtbl navigation_vtbl =
{
    navigation_QueryInterface, navigation_AddRef, navigation_Release, navigation_GetIids,
    navigation_GetRuntimeClassName, navigation_GetTrustLevel, navigation_add_BackRequested, navigation_remove_BackRequested
};
DEFINE_IINSPECTABLE(nav2, ISystemNavigationManager2, struct navigation, ISystemNavigationManager_iface)
static HRESULT WINAPI nav2_get_AppViewBackButtonVisibility( ISystemNavigationManager2 *iface, AppViewBackButtonVisibility *value )
{
    if (!value) return E_POINTER;
    *value = impl_from_ISystemNavigationManager2(iface)->visibility;
    return S_OK;
}
static HRESULT WINAPI nav2_put_AppViewBackButtonVisibility( ISystemNavigationManager2 *iface, AppViewBackButtonVisibility value )
{
    struct navigation *impl = impl_from_ISystemNavigationManager2(iface);
    HMENU menu;
    if (impl->thread != GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (!impl->hwnd) return RO_E_CLOSED;
    if (value < AppViewBackButtonVisibility_Visible || value > AppViewBackButtonVisibility_Disabled) return E_INVALIDARG;
    menu = GetSystemMenu(impl->hwnd, FALSE);
    DeleteMenu(menu, WINE_SC_BACK, MF_BYCOMMAND);
    if (value != AppViewBackButtonVisibility_Collapsed)
        AppendMenuW(menu, MF_STRING | (value == AppViewBackButtonVisibility_Disabled ? MF_GRAYED : 0), WINE_SC_BACK, L"Back");
    impl->visibility = value;
    DrawMenuBar(impl->hwnd);
    return S_OK;
}
static const ISystemNavigationManager2Vtbl nav2_vtbl =
{ nav2_QueryInterface, nav2_AddRef, nav2_Release, nav2_GetIids, nav2_GetRuntimeClassName, nav2_GetTrustLevel,
  nav2_get_AppViewBackButtonVisibility, nav2_put_AppViewBackButtonVisibility };
HRESULT navigation_create(HWND hwnd, ISystemNavigationManager **out)
{
    struct navigation *impl = calloc(1, sizeof(*impl));
    if (!impl) return E_OUTOFMEMORY;
    impl->ISystemNavigationManager_iface.lpVtbl = &navigation_vtbl;
    impl->ISystemNavigationManager2_iface.lpVtbl = &nav2_vtbl;
    impl->ref = 1;
    impl->hwnd = hwnd;
    impl->thread = GetCurrentThreadId();
    impl->visibility = AppViewBackButtonVisibility_Collapsed;
    *out = &impl->ISystemNavigationManager_iface;
    return S_OK;
}
void navigation_close(ISystemNavigationManager *iface)
{
    struct navigation *impl = impl_from_ISystemNavigationManager(iface);
    impl->hwnd = NULL;
    winrt_event_clear(&impl->back);
}

struct back_args { IBackRequestedEventArgs iface; LONG ref; boolean handled; };
static HRESULT WINAPI back_QueryInterface(IBackRequestedEventArgs *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) && !IsEqualGUID(iid,&IID_IBackRequestedEventArgs)) return E_NOINTERFACE;
    *out = iface;
    IBackRequestedEventArgs_AddRef(iface);
    return S_OK;
}
static ULONG WINAPI back_AddRef(IBackRequestedEventArgs *iface) { return InterlockedIncrement(&((struct back_args *)iface)->ref); }
static ULONG WINAPI back_Release(IBackRequestedEventArgs *iface)
{
    ULONG ref = InterlockedDecrement(&((struct back_args *)iface)->ref);
    if (!ref) free(iface);
    return ref;
}
static HRESULT WINAPI back_GetIids(IBackRequestedEventArgs *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids = IID_IBackRequestedEventArgs; *count = 1; return S_OK;
}
static HRESULT WINAPI back_GetRuntimeClassName(IBackRequestedEventArgs *iface, HSTRING *name)
{
    const WCHAR *str = RuntimeClass_Windows_UI_Core_BackRequestedEventArgs;
    return WindowsCreateString(str,wcslen(str),name);
}
static HRESULT WINAPI back_GetTrustLevel(IBackRequestedEventArgs *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }
static HRESULT WINAPI back_get_Handled(IBackRequestedEventArgs *iface, boolean *value)
{ if (!value) return E_POINTER; *value = ((struct back_args *)iface)->handled; return S_OK; }
static HRESULT WINAPI back_put_Handled(IBackRequestedEventArgs *iface, boolean value)
{ ((struct back_args *)iface)->handled = value; return S_OK; }
static const IBackRequestedEventArgsVtbl back_vtbl =
{ back_QueryInterface,back_AddRef,back_Release,back_GetIids,back_GetRuntimeClassName,back_GetTrustLevel,back_get_Handled,back_put_Handled };
BOOL navigation_back(ISystemNavigationManager *iface)
{
    struct navigation *impl = impl_from_ISystemNavigationManager(iface);
    struct back_args *args = calloc(1,sizeof(*args));
    BOOL handled;
    if (!args) return FALSE;
    args->iface.lpVtbl = &back_vtbl; args->ref = 1;
    ISystemNavigationManager_AddRef(iface);
    winrt_event_notify(&impl->back,iface,&args->iface);
    handled = args->handled;
    IBackRequestedEventArgs_Release(&args->iface);
    ISystemNavigationManager_Release(iface);
    return handled;
}
struct navigation_statics
{
    IActivationFactory IActivationFactory_iface;
    ISystemNavigationManagerStatics ISystemNavigationManagerStatics_iface;
    LONG ref;
};

static inline struct navigation_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct navigation_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct navigation_statics *impl = impl_from_IActivationFactory( iface );

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
    else if (IsEqualGUID( iid, &IID_ISystemNavigationManagerStatics ))
    {
        *out = &impl->ISystemNavigationManagerStatics_iface;
        ISystemNavigationManagerStatics_AddRef( &impl->ISystemNavigationManagerStatics_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct navigation_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct navigation_statics *impl = impl_from_IActivationFactory( iface );
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

DEFINE_IINSPECTABLE( navigation_static, ISystemNavigationManagerStatics, struct navigation_statics, IActivationFactory_iface )

static HRESULT STDMETHODCALLTYPE navigation_static_GetForCurrentView(ISystemNavigationManagerStatics *iface, ISystemNavigationManager **out)
{ return corewindow_get_navigation(out); }

static const struct ISystemNavigationManagerStaticsVtbl navigation_static_vtbl =
{
    navigation_static_QueryInterface,
    navigation_static_AddRef,
    navigation_static_Release,
    /* IInspectable methods */
    navigation_static_GetIids,
    navigation_static_GetRuntimeClassName,
    navigation_static_GetTrustLevel,
    /* ISystemNavigationManagerStatics methods */
    navigation_static_GetForCurrentView
};

static struct navigation_statics navigation_statics =
{
    {&factory_vtbl},
    {&navigation_static_vtbl},
    1,
};

IActivationFactory *navigation_factory = &navigation_statics.IActivationFactory_iface;
