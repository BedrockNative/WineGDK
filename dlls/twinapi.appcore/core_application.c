/* WinRT Windows.ApplicationModel.Core.CoreApplication implementation
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
#include "roapi.h"
#include "appmodel.h"
#include "winreg.h"
#include "wine/winrt_events.h"

WINE_DEFAULT_DEBUG_CHANNEL(twinapi);

HRESULT WINAPI __wine_create_core_window( ICoreWindow **out );
HRESULT WINAPI __wine_destroy_core_window( ICoreWindow *window );

struct factory
{
    IActivationFactory IActivationFactory_iface;
    ICoreApplication ICoreApplication_iface;
    ICoreImmersiveApplication ICoreImmersiveApplication_iface;
    LONG ref;
};

static inline struct factory *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct factory, IActivationFactory_iface );
}

static HRESULT WINAPI activation_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct factory *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (!out) return E_POINTER;
    *out = NULL;

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        IActivationFactory_AddRef( &impl->IActivationFactory_iface );
        *out = &impl->IActivationFactory_iface;
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_ICoreApplication ))
    {
        ICoreApplication_AddRef( &impl->ICoreApplication_iface );
        *out = &impl->ICoreApplication_iface;
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_ICoreImmersiveApplication ))
    {
        *out = &impl->ICoreImmersiveApplication_iface;
        IActivationFactory_AddRef( iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI activation_factory_AddRef( IActivationFactory *iface )
{
    struct factory *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI activation_factory_Release( IActivationFactory *iface )
{
    struct factory *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI activation_factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    if (!iid_count || !iids) return E_POINTER;
    *iid_count = 0;
    if (!(*iids = CoTaskMemAlloc( 2 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_ICoreApplication;
    (*iids)[1] = IID_ICoreImmersiveApplication;
    *iid_count = 2;
    return S_OK;
}

static HRESULT WINAPI activation_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    const WCHAR *name = RuntimeClass_Windows_ApplicationModel_Core_CoreApplication;
    return WindowsCreateString( name, wcslen( name ), class_name );
}

static HRESULT WINAPI activation_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    if (!trust_level) return E_POINTER;
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI activation_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
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


struct application_view
{
    ICoreApplicationView ICoreApplicationView_iface;
    ICoreApplicationView2 ICoreApplicationView2_iface;
    LONG ref;
    DWORD thread;
    ICoreWindow *window;
    struct winrt_event activated;
};

static struct application_view *current_view;
static DWORD application_thread;
static SRWLOCK application_lock = SRWLOCK_INIT;
static LONG application_running;
static IPropertySet *application_properties;
static struct winrt_event suspending, resuming;

static struct application_view *impl_from_ICoreApplicationView( ICoreApplicationView *iface )
{
    return CONTAINING_RECORD( iface, struct application_view, ICoreApplicationView_iface );
}

static HRESULT WINAPI view_QueryInterface( ICoreApplicationView *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_ICoreApplicationView2 ))
    {
        *out = &impl_from_ICoreApplicationView( iface )->ICoreApplicationView2_iface;
        ICoreApplicationView_AddRef( iface );
        return S_OK;
    }
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_ICoreApplicationView ))
    {
        FIXME( "view interface %s not implemented.\n", debugstr_guid( iid ) );
        return E_NOINTERFACE;
    }
    *out = iface;
    ICoreApplicationView_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI view_AddRef( ICoreApplicationView *iface )
{
    return InterlockedIncrement( &impl_from_ICoreApplicationView( iface )->ref );
}

static ULONG WINAPI view_Release( ICoreApplicationView *iface )
{
    struct application_view *view = impl_from_ICoreApplicationView( iface );
    ULONG ref = InterlockedDecrement( &view->ref );
    if (!ref)
    {
        winrt_event_clear( &view->activated );
        if (view->window) ICoreWindow_Release( view->window );
        free( view );
    }
    return ref;
}

static HRESULT WINAPI view_GetIids( ICoreApplicationView *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( 2 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_ICoreApplicationView;
    (*iids)[1] = IID_ICoreApplicationView2;
    *count = 2;
    return S_OK;
}

static HRESULT WINAPI view_GetRuntimeClassName( ICoreApplicationView *iface, HSTRING *name )
{
    const WCHAR *str = RuntimeClass_Windows_ApplicationModel_Core_CoreApplicationView;
    return WindowsCreateString( str, wcslen( str ), name );
}

static HRESULT WINAPI view_GetTrustLevel( ICoreApplicationView *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI view_get_CoreWindow( ICoreApplicationView *iface, ICoreWindow **out )
{
    struct application_view *view = impl_from_ICoreApplicationView( iface );
    if (!out) return E_POINTER;
    *out = NULL;
    AcquireSRWLockShared( &application_lock );
    if ((*out = view->window)) ICoreWindow_AddRef( *out );
    ReleaseSRWLockShared( &application_lock );
    return S_OK;
}

static HRESULT WINAPI view_add_Activated( ICoreApplicationView *iface,
    ITypedEventHandler_CoreApplicationView_IActivatedEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &impl_from_ICoreApplicationView( iface )->activated, handler, token );
}

static HRESULT WINAPI view_remove_Activated( ICoreApplicationView *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &impl_from_ICoreApplicationView( iface )->activated, token );
}

static HRESULT WINAPI view_get_IsMain( ICoreApplicationView *iface, boolean *value )
{
    if (!value) return E_POINTER;
    *value = TRUE;
    return S_OK;
}

static HRESULT WINAPI view_get_IsHosted( ICoreApplicationView *iface, boolean *value )
{
    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

static const ICoreApplicationViewVtbl view_vtbl =
{
    view_QueryInterface, view_AddRef, view_Release, view_GetIids, view_GetRuntimeClassName,
    view_GetTrustLevel, view_get_CoreWindow, view_add_Activated, view_remove_Activated,
    view_get_IsMain, view_get_IsHosted
};

DEFINE_IINSPECTABLE( view2, ICoreApplicationView2, struct application_view, ICoreApplicationView_iface )

static HRESULT WINAPI view2_get_Dispatcher( ICoreApplicationView2 *iface, ICoreDispatcher **out )
{
    struct application_view *view = impl_from_ICoreApplicationView2( iface );
    ICoreWindow *window;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (FAILED(hr = view_get_CoreWindow( &view->ICoreApplicationView_iface, &window ))) return hr;
    if (!window) return E_ILLEGAL_METHOD_CALL;
    hr = ICoreWindow_get_Dispatcher( window, out );
    ICoreWindow_Release( window );
    return hr;
}

static const ICoreApplicationView2Vtbl view2_vtbl =
{
    view2_QueryInterface, view2_AddRef, view2_Release, view2_GetIids,
    view2_GetRuntimeClassName, view2_GetTrustLevel, view2_get_Dispatcher
};

DEFINE_IINSPECTABLE( application, ICoreApplication, struct factory, IActivationFactory_iface )

static HRESULT WINAPI application_get_Id( ICoreApplication *iface, HSTRING *value )
{
    WCHAR executable[MAX_PATH], key[MAX_PATH+48], id[128], *name;
    WCHAR aumid[320];
    UINT32 length = ARRAY_SIZE(aumid);
    DWORD size=sizeof(id);
    LONG ret;
    if (!value) return E_POINTER;
    *value=NULL;
    if (!GetCurrentApplicationUserModelId(&length, aumid) && (name = wcschr(aumid, '!')))
        return WindowsCreateString(name + 1, wcslen(name + 1), value);
    if (!GetModuleFileNameW(NULL,executable,ARRAY_SIZE(executable))) return HRESULT_FROM_WIN32(GetLastError());
    name=wcsrchr(executable,'\\'); name=name ? name+1 : executable;
    swprintf(key,ARRAY_SIZE(key),L"Software\\Wine\\AppDefaults\\%s\\Package",name);
    ret=RegGetValueW(HKEY_CURRENT_USER,key,L"ApplicationId",RRF_RT_REG_SZ,NULL,id,&size);
    if (ret==ERROR_FILE_NOT_FOUND || ret==ERROR_PATH_NOT_FOUND) ret=APPMODEL_ERROR_NO_PACKAGE;
    if (ret) return HRESULT_FROM_WIN32(ret);
    return WindowsCreateString(id,wcslen(id),value);
}

static HRESULT WINAPI application_add_Suspending( ICoreApplication *iface,
    IEventHandler_SuspendingEventArgs *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &suspending, handler, token );
}

static HRESULT WINAPI application_remove_Suspending( ICoreApplication *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &suspending, token );
}

static HRESULT WINAPI application_add_Resuming( ICoreApplication *iface,
    IEventHandler_IInspectable *handler, EventRegistrationToken *token )
{
    return winrt_event_add( &resuming, handler, token );
}

static HRESULT WINAPI application_remove_Resuming( ICoreApplication *iface, EventRegistrationToken token )
{
    return winrt_event_remove( &resuming, token );
}

static HRESULT WINAPI application_get_Properties( ICoreApplication *iface, IPropertySet **out )
{
    IPropertySet *properties = NULL;
    HSTRING name;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    AcquireSRWLockShared( &application_lock );
    if (application_properties) IPropertySet_AddRef( (*out = application_properties) );
    ReleaseSRWLockShared( &application_lock );
    if (*out) return S_OK;
    if (FAILED(hr = WindowsCreateString( RuntimeClass_Windows_Foundation_Collections_PropertySet,
        wcslen( RuntimeClass_Windows_Foundation_Collections_PropertySet ), &name ))) return hr;
    hr = RoActivateInstance( name, (IInspectable **)&properties );
    WindowsDeleteString( name );
    if (FAILED(hr)) return hr;
    AcquireSRWLockExclusive( &application_lock );
    if (!application_properties)
    {
        application_properties = properties;
        IPropertySet_AddRef( properties );
    }
    IPropertySet_AddRef( (*out = application_properties) );
    ReleaseSRWLockExclusive( &application_lock );
    IPropertySet_Release( properties );
    return S_OK;
}

static HRESULT WINAPI application_GetCurrentView( ICoreApplication *iface, ICoreApplicationView **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    AcquireSRWLockShared( &application_lock );
    if (current_view && GetCurrentThreadId() == application_thread)
        ICoreApplicationView_AddRef( (*out = &current_view->ICoreApplicationView_iface) );
    ReleaseSRWLockShared( &application_lock );
    return *out ? S_OK : E_ILLEGAL_METHOD_CALL;
}

struct launch_args
{
    IActivatedEventArgs IActivatedEventArgs_iface;
    ILaunchActivatedEventArgs ILaunchActivatedEventArgs_iface;
    IPrelaunchActivatedEventArgs IPrelaunchActivatedEventArgs_iface;
    LONG ref;
    HSTRING arguments;
};

static struct launch_args *impl_from_IActivatedEventArgs( IActivatedEventArgs *iface )
{
    return CONTAINING_RECORD( iface, struct launch_args, IActivatedEventArgs_iface );
}
static HRESULT WINAPI launch_QueryInterface( IActivatedEventArgs *iface, REFIID iid, void **out )
{
    struct launch_args *args = impl_from_IActivatedEventArgs( iface );
    TRACE("launch args iid %s\n", debugstr_guid(iid));
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivatedEventArgs )) *out = iface;
    else if (IsEqualGUID( iid, &IID_ILaunchActivatedEventArgs )) *out = &args->ILaunchActivatedEventArgs_iface;
    else if (IsEqualGUID( iid, &IID_IPrelaunchActivatedEventArgs )) *out = &args->IPrelaunchActivatedEventArgs_iface;
    else return E_NOINTERFACE;
    IActivatedEventArgs_AddRef( iface );
    return S_OK;
}
static ULONG WINAPI launch_AddRef( IActivatedEventArgs *iface )
{
    return InterlockedIncrement( &impl_from_IActivatedEventArgs( iface )->ref );
}
static ULONG WINAPI launch_Release( IActivatedEventArgs *iface )
{
    struct launch_args *args = impl_from_IActivatedEventArgs( iface );
    ULONG ref = InterlockedDecrement( &args->ref );
    if (!ref)
    {
        WindowsDeleteString( args->arguments );
        free( args );
    }
    return ref;
}
static HRESULT WINAPI launch_GetIids( IActivatedEventArgs *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( 3 * sizeof(**iids) ))) return E_OUTOFMEMORY;
    (*iids)[0] = IID_IActivatedEventArgs;
    (*iids)[1] = IID_ILaunchActivatedEventArgs;
    (*iids)[2] = IID_IPrelaunchActivatedEventArgs;
    *count = 3;
    return S_OK;
}
static HRESULT WINAPI launch_GetRuntimeClassName( IActivatedEventArgs *iface, HSTRING *name )
{
    const WCHAR str[] = L"Windows.ApplicationModel.Activation.LaunchActivatedEventArgs";
    return WindowsCreateString( str, ARRAY_SIZE(str) - 1, name );
}
static HRESULT WINAPI launch_GetTrustLevel( IActivatedEventArgs *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}
static HRESULT WINAPI launch_get_Kind( IActivatedEventArgs *iface, ActivationKind *kind )
{
    if (!kind) return E_POINTER;
    *kind = ActivationKind_Launch;
    return S_OK;
}
static HRESULT WINAPI launch_get_PreviousExecutionState( IActivatedEventArgs *iface, ApplicationExecutionState *state )
{
    if (!state) return E_POINTER;
    *state = ApplicationExecutionState_NotRunning;
    return S_OK;
}
static HRESULT WINAPI launch_get_SplashScreen( IActivatedEventArgs *iface, ISplashScreen **splash )
{
    if (!splash) return E_POINTER;
    *splash = NULL;
    return S_OK; /* Loose applications have no shell-provided splash screen. */
}
static const IActivatedEventArgsVtbl launch_vtbl =
{
    launch_QueryInterface, launch_AddRef, launch_Release, launch_GetIids, launch_GetRuntimeClassName,
    launch_GetTrustLevel, launch_get_Kind, launch_get_PreviousExecutionState, launch_get_SplashScreen
};
DEFINE_IINSPECTABLE( launch_detail, ILaunchActivatedEventArgs, struct launch_args, IActivatedEventArgs_iface )
static HRESULT WINAPI launch_detail_get_Arguments( ILaunchActivatedEventArgs *iface, HSTRING *value )
{
    return WindowsDuplicateString( impl_from_ILaunchActivatedEventArgs( iface )->arguments, value );
}
static HRESULT WINAPI launch_detail_get_TileId( ILaunchActivatedEventArgs *iface, HSTRING *value )
{
    return WindowsCreateString( NULL, 0, value ); /* No tile for a loose executable. */
}
static const ILaunchActivatedEventArgsVtbl launch_detail_vtbl =
{
    launch_detail_QueryInterface, launch_detail_AddRef, launch_detail_Release, launch_detail_GetIids,
    launch_detail_GetRuntimeClassName, launch_detail_GetTrustLevel, launch_detail_get_Arguments, launch_detail_get_TileId
};
DEFINE_IINSPECTABLE(prelaunch, IPrelaunchActivatedEventArgs, struct launch_args, IActivatedEventArgs_iface)
static HRESULT WINAPI prelaunch_get_PrelaunchActivated(IPrelaunchActivatedEventArgs *iface, boolean *value)
{
    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}
static const IPrelaunchActivatedEventArgsVtbl prelaunch_vtbl = {prelaunch_QueryInterface, prelaunch_AddRef,
    prelaunch_Release, prelaunch_GetIids, prelaunch_GetRuntimeClassName, prelaunch_GetTrustLevel,
    prelaunch_get_PrelaunchActivated};

static HRESULT launch_args_create( IActivatedEventArgs **out )
{
    const WCHAR *command = GetCommandLineW();
    struct launch_args *args;
    BOOL quoted = FALSE;
    HRESULT hr;
    if (!(args = calloc( 1, sizeof(*args) ))) return E_OUTOFMEMORY;
    args->IActivatedEventArgs_iface.lpVtbl = &launch_vtbl;
    args->ILaunchActivatedEventArgs_iface.lpVtbl = &launch_detail_vtbl;
    args->IPrelaunchActivatedEventArgs_iface.lpVtbl = &prelaunch_vtbl;
    args->ref = 1;
    /* Keep the raw launch arguments, preserving their quotes. */
    while (*command && (quoted || (*command != ' ' && *command != '\t')))
        if (*command++ == '"') quoted = !quoted;
    while (*command == ' ' || *command == '\t') ++command;
    if (FAILED(hr = WindowsCreateString( command, wcslen( command ), &args->arguments )))
    {
        free( args );
        return hr;
    }
    *out = &args->IActivatedEventArgs_iface;
    return S_OK;
}

static HRESULT WINAPI application_Run( ICoreApplication *iface, IFrameworkViewSource *source )
{
    struct application_view *view;
    ICoreWindow *window = NULL;
    IFrameworkView *framework = NULL;
    HRESULT hr, cleanup_hr;
    BOOL ran = FALSE;

    WCHAR *package_path;
    UINT32 path_length = 0;

    TRACE( "source %p\n", source );
    if (!source) return E_INVALIDARG;
    if (InterlockedCompareExchange( &application_running, 1, 0 )) return E_ILLEGAL_METHOD_CALL;
    if (!(view = calloc( 1, sizeof(*view) )))
    {
        InterlockedExchange( &application_running, 0 );
        return E_OUTOFMEMORY;
    }
    view->ICoreApplicationView_iface.lpVtbl = &view_vtbl;
    view->ICoreApplicationView2_iface.lpVtbl = &view2_vtbl;
    view->ref = 1;
    view->thread = GetCurrentThreadId();
    AcquireSRWLockExclusive( &application_lock );
    current_view = view;
    application_thread = view->thread;
    ReleaseSRWLockExclusive( &application_lock );
    /* UWP applications start in their package's installed location. */
    if (GetCurrentPackagePath(&path_length, NULL) == ERROR_INSUFFICIENT_BUFFER &&
        (package_path = malloc(path_length * sizeof(WCHAR))))
    {
        if (!GetCurrentPackagePath(&path_length, package_path)) SetCurrentDirectoryW(package_path);
        free(package_path);
    }
    IFrameworkViewSource_AddRef( source );
    hr = IFrameworkViewSource_CreateView( source, &framework );
    if (SUCCEEDED(hr) && !framework) hr = E_UNEXPECTED;
    if (SUCCEEDED(hr))
    {
        TRACE( "calling Initialize\n" );
        hr = IFrameworkView_Initialize( framework, &view->ICoreApplicationView_iface );
    }
    if (SUCCEEDED(hr))
    {
        hr = __wine_create_core_window( &window );
        AcquireSRWLockExclusive( &application_lock );
        view->window = window;
        ReleaseSRWLockExclusive( &application_lock );
    }
    if (SUCCEEDED(hr))
    {
        TRACE( "calling SetWindow\n" );
        hr = IFrameworkView_SetWindow( framework, view->window );
    }
    if (SUCCEEDED(hr))
    {
        TRACE( "calling Load\n" );
        hr = IFrameworkView_Load( framework, NULL );
    }
    if (SUCCEEDED(hr))
    {
        IActivatedEventArgs *args;
        if (SUCCEEDED(hr = launch_args_create( &args )))
        {
            TRACE( "notifying Activated\n" );
            hr = winrt_event_notify( &view->activated, &view->ICoreApplicationView_iface, args );
            IActivatedEventArgs_Release( args );
        }
    }
    if (SUCCEEDED(hr))
    {
        TRACE( "calling Run\n" );
        ran = TRUE;
        hr = IFrameworkView_Run( framework );
    }
    /* A rejected startup may not have created the resources Uninitialize uses. */
    if (ran)
    {
        TRACE( "calling Uninitialize, previous result %#lx\n", hr );
        cleanup_hr = IFrameworkView_Uninitialize( framework );
        if (SUCCEEDED(hr)) hr = cleanup_hr;
    }
    AcquireSRWLockExclusive( &application_lock );
    current_view = NULL;
    application_thread = 0;
    ReleaseSRWLockExclusive( &application_lock );
    /* Break view/delegate/application reference cycles before releasing the view. */
    winrt_event_clear( &view->activated );
    if (view->window) __wine_destroy_core_window( view->window );
    if (framework) IFrameworkView_Release( framework );
    IFrameworkViewSource_Release( source );
    ICoreApplicationView_Release( &view->ICoreApplicationView_iface );
    InterlockedExchange( &application_running, 0 );
    TRACE( "Run finished, hr %#lx\n", hr );
    return hr;
}

static HRESULT WINAPI application_RunWithActivationFactories( ICoreApplication *iface, IGetActivationFactory *factory )
{
    FIXME( "factory %p stub!\n", factory );
    return E_NOTIMPL;
}

static const ICoreApplicationVtbl application_vtbl =
{
    application_QueryInterface, application_AddRef, application_Release, application_GetIids,
    application_GetRuntimeClassName, application_GetTrustLevel, application_get_Id,
    application_add_Suspending, application_remove_Suspending, application_add_Resuming,
    application_remove_Resuming, application_get_Properties, application_GetCurrentView,
    application_Run, application_RunWithActivationFactories
};

DEFINE_IINSPECTABLE( immersive, ICoreImmersiveApplication, struct factory, IActivationFactory_iface )

static HRESULT WINAPI immersive_get_Views( ICoreImmersiveApplication *iface, IVectorView_CoreApplicationView **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    FIXME( "iface %p, out %p stub!\n", iface, out );
    return E_NOTIMPL;
}

static HRESULT WINAPI immersive_CreateNewView( ICoreImmersiveApplication *iface, HSTRING type,
                                              HSTRING entry, ICoreApplicationView **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    FIXME( "type %s, entry %s stub!\n", debugstr_hstring(type), debugstr_hstring(entry) );
    return E_NOTIMPL;
}

static HRESULT WINAPI immersive_get_MainView( ICoreImmersiveApplication *iface, ICoreApplicationView **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    AcquireSRWLockShared( &application_lock );
    if (current_view) ICoreApplicationView_AddRef( (*out = &current_view->ICoreApplicationView_iface) );
    ReleaseSRWLockShared( &application_lock );
    return *out ? S_OK : E_ILLEGAL_METHOD_CALL;
}

static const ICoreImmersiveApplicationVtbl immersive_vtbl =
{
    immersive_QueryInterface, immersive_AddRef, immersive_Release, immersive_GetIids,
    immersive_GetRuntimeClassName, immersive_GetTrustLevel, immersive_get_Views,
    immersive_CreateNewView, immersive_get_MainView
};

static struct factory factory =
{
    {&activation_factory_vtbl},
    {&application_vtbl},
    {&immersive_vtbl},
    1,
};

IActivationFactory *core_application_factory = &factory.IActivationFactory_iface;
