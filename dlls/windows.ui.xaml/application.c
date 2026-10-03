/* XAML application composition and CoreApplication hosting.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "private.h"
#include "roapi.h"
#define WIDL_using_Windows_UI_Xaml
#define WIDL_using_Windows_ApplicationModel_Core
#define WIDL_using_Windows_ApplicationModel_Activation
#define WIDL_using_Windows_UI_Core
#include "windows.ui.xaml.h"
#include "wine/winrt_events.h"
WINE_DEFAULT_DEBUG_CHANNEL(xaml);

struct application
{
    IInspectable inner;
    IApplication IApplication_iface;
    IApplicationOverrides IApplicationOverrides_iface;
    IInspectable *outer, *resources;
    LONG ref;
    INT32 theme;
    struct winrt_event unhandled, suspending, resuming;
};
static struct application *current;
static LONG running;
static HRESULT run_application(void);

static HRESULT WINAPI inner_QueryInterface(IInspectable *iface, REFIID iid, void **out)
{
    struct application *impl = CONTAINING_RECORD(iface, struct application, inner);
    if (!out) return E_POINTER;
    *out = NULL;
    TRACE("application iid %s\n", debugstr_guid(iid));
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable))
    { *out = iface; IInspectable_AddRef(iface); return S_OK; }
    if (IsEqualGUID(iid, &IID_IApplication)) *out = &impl->IApplication_iface;
    else if (IsEqualGUID(iid, &IID_IApplicationOverrides)) *out = &impl->IApplicationOverrides_iface;
    else return E_NOINTERFACE;
    IInspectable_AddRef((IInspectable *)*out);
    return S_OK;
}
static ULONG WINAPI inner_AddRef(IInspectable *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct application, inner)->ref); }
static ULONG WINAPI inner_Release(IInspectable *iface)
{
    struct application *impl = CONTAINING_RECORD(iface, struct application, inner);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref)
    {
        winrt_event_clear(&impl->unhandled); winrt_event_clear(&impl->suspending); winrt_event_clear(&impl->resuming);
        if (impl->resources) IInspectable_Release(impl->resources);
        free(impl);
    }
    return ref;
}
static HRESULT WINAPI inner_GetIids(IInspectable *iface, ULONG *count, IID **ids)
{ if (!count || !ids) return E_POINTER; *count = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI inner_GetRuntimeClassName(IInspectable *iface, HSTRING *name)
{ return WindowsCreateString(L"Windows.UI.Xaml.Application", 27, name); }
static HRESULT WINAPI inner_GetTrustLevel(IInspectable *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }
static const IInspectableVtbl inner_vtbl = {inner_QueryInterface, inner_AddRef, inner_Release,
    inner_GetIids, inner_GetRuntimeClassName, inner_GetTrustLevel};

DEFINE_IINSPECTABLE_(app, IApplication, struct application, impl_from_IApplication, IApplication_iface, impl->outer)
DEFINE_IINSPECTABLE_(overrides, IApplicationOverrides, struct application, impl_from_IApplicationOverrides, IApplicationOverrides_iface, impl->outer)
static HRESULT WINAPI app_get_Resources(IApplication *iface, IInspectable **value)
{
    struct application *impl = impl_from_IApplication(iface);
    if (!value) return E_POINTER;
    *value = impl->resources;
    if (*value) IInspectable_AddRef(*value);
    else { FIXME("Default resource dictionary not implemented.\n"); return E_NOTIMPL; }
    return S_OK;
}
static HRESULT WINAPI app_put_Resources(IApplication *iface, IInspectable *value)
{
    struct application *impl = impl_from_IApplication(iface);
    if (value) IInspectable_AddRef(value);
    if (impl->resources) IInspectable_Release(impl->resources);
    impl->resources = value;
    return S_OK;
}
static HRESULT WINAPI app_get_DebugSettings(IApplication *iface, IInspectable **value)
{ if (!value) return E_POINTER; *value = NULL; FIXME("DebugSettings not implemented.\n"); return E_NOTIMPL; }
static HRESULT WINAPI app_get_RequestedTheme(IApplication *iface, INT32 *value)
{ if (!value) return E_POINTER; *value = impl_from_IApplication(iface)->theme; return S_OK; }
static HRESULT WINAPI app_put_RequestedTheme(IApplication *iface, INT32 value)
{ if (value < 0 || value > 1) return E_INVALIDARG; impl_from_IApplication(iface)->theme = value; return S_OK; }
#define APP_EVENT(name, member, type) \
static HRESULT WINAPI app_add_##name(IApplication *iface, type *handler, EventRegistrationToken *token) \
{ return winrt_event_add(&impl_from_IApplication(iface)->member, handler, token); } \
static HRESULT WINAPI app_remove_##name(IApplication *iface, EventRegistrationToken token) \
{ return winrt_event_remove(&impl_from_IApplication(iface)->member, token); }
APP_EVENT(UnhandledException, unhandled, IUnhandledExceptionEventHandler)
APP_EVENT(Suspending, suspending, ISuspendingEventHandler)
APP_EVENT(Resuming, resuming, IEventHandler_IInspectable)
static HRESULT WINAPI app_Exit(IApplication *iface) { PostQuitMessage(0); return S_OK; }
static const IApplicationVtbl app_vtbl = {app_QueryInterface, app_AddRef, app_Release, app_GetIids,
    app_GetRuntimeClassName, app_GetTrustLevel, app_get_Resources, app_put_Resources, app_get_DebugSettings,
    app_get_RequestedTheme, app_put_RequestedTheme, app_add_UnhandledException, app_remove_UnhandledException,
    app_add_Suspending, app_remove_Suspending, app_add_Resuming, app_remove_Resuming, app_Exit};
#define OVERRIDE(name, type) \
static HRESULT WINAPI overrides_##name(IApplicationOverrides *iface, type *args) { return S_OK; }
OVERRIDE(OnActivated, IActivatedEventArgs)
OVERRIDE(OnLaunched, ILaunchActivatedEventArgs)
OVERRIDE(OnFileActivated, IInspectable)
OVERRIDE(OnSearchActivated, IInspectable)
OVERRIDE(OnShareTargetActivated, IInspectable)
OVERRIDE(OnFileOpenPickerActivated, IInspectable)
OVERRIDE(OnFileSavePickerActivated, IInspectable)
OVERRIDE(OnCachedFileUpdaterActivated, IInspectable)
OVERRIDE(OnWindowCreated, IInspectable)
static const IApplicationOverridesVtbl overrides_vtbl = {overrides_QueryInterface, overrides_AddRef,
    overrides_Release, overrides_GetIids, overrides_GetRuntimeClassName, overrides_GetTrustLevel,
    overrides_OnActivated, overrides_OnLaunched, overrides_OnFileActivated, overrides_OnSearchActivated,
    overrides_OnShareTargetActivated, overrides_OnFileOpenPickerActivated, overrides_OnFileSavePickerActivated,
    overrides_OnCachedFileUpdaterActivated, overrides_OnWindowCreated};

struct factory
{
    IActivationFactory IActivationFactory_iface;
    IApplicationFactory IApplicationFactory_iface;
    IApplicationStatics IApplicationStatics_iface;
    LONG ref;
};
static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct factory *impl = CONTAINING_RECORD(iface, struct factory, IActivationFactory_iface);
    if (!out) return E_POINTER;
    *out = NULL;
    TRACE("factory iid %s\n", debugstr_guid(iid));
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IActivationFactory) || IsEqualGUID(iid, &IID_IAgileObject)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IApplicationFactory)) *out = &impl->IApplicationFactory_iface;
    else if (IsEqualGUID(iid, &IID_IApplicationStatics)) *out = &impl->IApplicationStatics_iface;
    else return E_NOINTERFACE;
    IActivationFactory_AddRef(iface); return S_OK;
}
static ULONG WINAPI factory_AddRef(IActivationFactory *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct factory, IActivationFactory_iface)->ref); }
static ULONG WINAPI factory_Release(IActivationFactory *iface)
{ return InterlockedDecrement(&CONTAINING_RECORD(iface, struct factory, IActivationFactory_iface)->ref); }
static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **ids)
{ if (!count || !ids) return E_POINTER; *count = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *name)
{ return WindowsCreateString(L"Windows.UI.Xaml.Application", 27, name); }
static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *value)
{ if (!value) return E_POINTER; *value = BaseTrust; return S_OK; }
static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IActivationFactoryVtbl factory_vtbl = {factory_QueryInterface, factory_AddRef, factory_Release,
    factory_GetIids, factory_GetRuntimeClassName, factory_GetTrustLevel, factory_ActivateInstance};
DEFINE_IINSPECTABLE(create, IApplicationFactory, struct factory, IActivationFactory_iface)
DEFINE_IINSPECTABLE(statics, IApplicationStatics, struct factory, IActivationFactory_iface)
static HRESULT WINAPI create_CreateInstance(IApplicationFactory *iface, IInspectable *outer, IInspectable **inner, IApplication **out)
{
    struct application *impl;
    TRACE("outer %p\n", outer);
    if (!inner || !out) return E_POINTER;
    *inner = NULL; *out = NULL;
    if (current) return E_ILLEGAL_METHOD_CALL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->inner.lpVtbl = &inner_vtbl;
    impl->IApplication_iface.lpVtbl = &app_vtbl;
    impl->IApplicationOverrides_iface.lpVtbl = &overrides_vtbl;
    impl->outer = outer ? outer : &impl->inner;
    impl->ref = 1;
    *inner = &impl->inner;
    *out = &impl->IApplication_iface;
    IApplication_AddRef(*out);
    current = impl;
    IInspectable_AddRef(impl->outer);
    return S_OK;
}
static HRESULT WINAPI statics_get_Current(IApplicationStatics *iface, IApplication **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    return current ? IInspectable_QueryInterface(current->outer, &IID_IApplication, (void **)out) : E_ILLEGAL_METHOD_CALL;
}
static HRESULT WINAPI statics_Start(IApplicationStatics *iface, IApplicationInitializationCallback *callback)
{
    HRESULT hr;
    IInspectable *outer;
    if (!callback) return E_INVALIDARG;
    if (InterlockedCompareExchange(&running, 1, 0)) return E_ILLEGAL_METHOD_CALL;
    TRACE("initialization callback %p\n", callback);
    hr = IApplicationInitializationCallback_Invoke(callback, NULL);
    TRACE("initialization completed %#lx\n", hr);
    if (SUCCEEDED(hr)) hr = current ? run_application() : E_UNEXPECTED;
    xaml_composition_clear(); xaml_window_clear();
    if (current) { outer = current->outer; current = NULL; IInspectable_Release(outer); }
    InterlockedExchange(&running, 0);
    return hr;
}
static HRESULT WINAPI statics_LoadComponent(IApplicationStatics *iface, IInspectable *component, IUriRuntimeClass *uri)
{ return xaml_load_component(component, uri); }
static HRESULT WINAPI statics_LoadComponentWithResourceLocation(IApplicationStatics *iface, IInspectable *component, IUriRuntimeClass *uri, INT32 location)
{ return statics_LoadComponent(iface, component, uri); }
static const IApplicationFactoryVtbl create_vtbl = {create_QueryInterface, create_AddRef, create_Release,
    create_GetIids, create_GetRuntimeClassName, create_GetTrustLevel, create_CreateInstance};
static const IApplicationStaticsVtbl statics_vtbl = {statics_QueryInterface, statics_AddRef, statics_Release,
    statics_GetIids, statics_GetRuntimeClassName, statics_GetTrustLevel, statics_get_Current, statics_Start,
    statics_LoadComponent, statics_LoadComponentWithResourceLocation};
static struct factory factory = {{&factory_vtbl}, {&create_vtbl}, {&statics_vtbl}, 1};
IActivationFactory *application_factory = &factory.IActivationFactory_iface;

struct xaml_host
{
    IFrameworkViewSource IFrameworkViewSource_iface;
    IFrameworkView IFrameworkView_iface;
    ITypedEventHandler_CoreApplicationView_IActivatedEventArgs activated;
    LONG ref;
    ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs character;
    EventRegistrationToken character_token;
    ICoreWindow *window;
    ICoreApplicationView *view;
    EventRegistrationToken activation_token;
};
static HRESULT WINAPI source_QueryInterface(IFrameworkViewSource *iface, REFIID iid, void **out)
{
    struct xaml_host *impl = CONTAINING_RECORD(iface, struct xaml_host, IFrameworkViewSource_iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IFrameworkViewSource)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IFrameworkView)) *out = &impl->IFrameworkView_iface;
    else if (IsEqualGUID(iid, &IID_ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs)) *out = &impl->character;
    else if (IsEqualGUID(iid, &IID_ITypedEventHandler_CoreApplicationView_IActivatedEventArgs)) *out = &impl->activated;
    else return E_NOINTERFACE;
    IFrameworkViewSource_AddRef(iface);
    return S_OK;
}
static ULONG WINAPI source_AddRef(IFrameworkViewSource *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct xaml_host, IFrameworkViewSource_iface)->ref); }
static ULONG WINAPI source_Release(IFrameworkViewSource *iface)
{
    struct xaml_host *impl = CONTAINING_RECORD(iface, struct xaml_host, IFrameworkViewSource_iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref)
    {
        if (impl->view) ICoreApplicationView_Release(impl->view);
        if (impl->window) ICoreWindow_Release(impl->window);
        free(impl);
    }
    return ref;
}
static HRESULT WINAPI source_GetIids(IFrameworkViewSource *iface, ULONG *count, IID **ids)
{ return factory_GetIids(NULL, count, ids); }
static HRESULT WINAPI source_GetRuntimeClassName(IFrameworkViewSource *iface, HSTRING *name)
{ return factory_GetRuntimeClassName(NULL, name); }
static HRESULT WINAPI source_GetTrustLevel(IFrameworkViewSource *iface, TrustLevel *value)
{ return factory_GetTrustLevel(NULL, value); }
static HRESULT WINAPI source_CreateView(IFrameworkViewSource *iface, IFrameworkView **out)
{ return source_QueryInterface(iface, &IID_IFrameworkView, (void **)out); }
static const IFrameworkViewSourceVtbl source_vtbl = {source_QueryInterface, source_AddRef, source_Release,
    source_GetIids, source_GetRuntimeClassName, source_GetTrustLevel, source_CreateView};
DEFINE_IINSPECTABLE(host, IFrameworkView, struct xaml_host, IFrameworkViewSource_iface)
static HRESULT WINAPI activated_QueryInterface(ITypedEventHandler_CoreApplicationView_IActivatedEventArgs *iface, REFIID iid, void **out)
{ return source_QueryInterface(&CONTAINING_RECORD(iface, struct xaml_host, activated)->IFrameworkViewSource_iface, iid, out); }
static ULONG WINAPI activated_AddRef(ITypedEventHandler_CoreApplicationView_IActivatedEventArgs *iface)
{ return source_AddRef(&CONTAINING_RECORD(iface, struct xaml_host, activated)->IFrameworkViewSource_iface); }
static ULONG WINAPI activated_Release(ITypedEventHandler_CoreApplicationView_IActivatedEventArgs *iface)
{ return source_Release(&CONTAINING_RECORD(iface, struct xaml_host, activated)->IFrameworkViewSource_iface); }
static HRESULT WINAPI activated_Invoke(ITypedEventHandler_CoreApplicationView_IActivatedEventArgs *iface, ICoreApplicationView *sender, IActivatedEventArgs *args)
{
    IApplicationOverrides *overrides;
    ILaunchActivatedEventArgs *launch;
    HRESULT hr;
    TRACE("dispatching XAML activation\n");
    if (FAILED(hr = IInspectable_QueryInterface(current->outer, &IID_IApplicationOverrides, (void **)&overrides))) return hr;
    if (SUCCEEDED(hr = IActivatedEventArgs_QueryInterface(args, &IID_ILaunchActivatedEventArgs, (void **)&launch)))
    {
        hr = IApplicationOverrides_OnLaunched(overrides, launch);
        ILaunchActivatedEventArgs_Release(launch);
    }
    else hr = IApplicationOverrides_OnActivated(overrides, args);
    IApplicationOverrides_Release(overrides);
    return hr;
}
static const ITypedEventHandler_CoreApplicationView_IActivatedEventArgsVtbl activated_vtbl = {
    activated_QueryInterface, activated_AddRef, activated_Release, activated_Invoke};
static HRESULT WINAPI host_Initialize(IFrameworkView *iface, ICoreApplicationView *view)
{
    struct xaml_host *impl = impl_from_IFrameworkView(iface);
    impl->view = view; ICoreApplicationView_AddRef(view);
    return ICoreApplicationView_add_Activated(view, &impl->activated, &impl->activation_token);
}
static HRESULT WINAPI character_QueryInterface(ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs *iface, REFIID iid, void **out)
{ return source_QueryInterface(&CONTAINING_RECORD(iface, struct xaml_host, character)->IFrameworkViewSource_iface, iid, out); }
static ULONG WINAPI character_AddRef(ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs *iface)
{ return source_AddRef(&CONTAINING_RECORD(iface, struct xaml_host, character)->IFrameworkViewSource_iface); }
static ULONG WINAPI character_Release(ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs *iface)
{ return source_Release(&CONTAINING_RECORD(iface, struct xaml_host, character)->IFrameworkViewSource_iface); }
static HRESULT WINAPI character_Invoke(ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs *iface, ICoreWindow *sender, ICharacterReceivedEventArgs *args)
{
    UINT32 character;
    ICoreWindowEventArgs *window_args;
    HRESULT hr = ICharacterReceivedEventArgs_get_KeyCode(args, &character);
    if (SUCCEEDED(hr) && xaml_control_character(character))
    {
        hr = ICharacterReceivedEventArgs_QueryInterface(args, &IID_ICoreWindowEventArgs, (void **)&window_args);
        if (SUCCEEDED(hr))
        {
            hr = ICoreWindowEventArgs_put_Handled(window_args, TRUE);
            ICoreWindowEventArgs_Release(window_args);
        }
    }
    return hr;
}
static const ITypedEventHandler_CoreWindow_CharacterReceivedEventArgsVtbl character_vtbl = {
    character_QueryInterface, character_AddRef, character_Release, character_Invoke};
static HRESULT WINAPI host_SetWindow(IFrameworkView *iface, ICoreWindow *window)
{
    struct xaml_host *impl = impl_from_IFrameworkView(iface);
    impl->window = window; ICoreWindow_AddRef(window);
    return ICoreWindow_add_CharacterReceived(window, &impl->character, &impl->character_token);
}
static HRESULT WINAPI host_Load(IFrameworkView *iface, HSTRING entry) { return S_OK; }
static HRESULT WINAPI host_Run(IFrameworkView *iface)
{
    struct xaml_host *impl = impl_from_IFrameworkView(iface);
    ICoreDispatcher *dispatcher;
    HRESULT hr;
    if (FAILED(hr = xaml_window_layout())) return hr;
    if (FAILED(hr = ICoreWindow_Activate(impl->window))) return hr;
    if (FAILED(hr = ICoreWindow_get_Dispatcher(impl->window, &dispatcher))) return hr;
    hr = ICoreDispatcher_ProcessEvents(dispatcher, CoreProcessEventsOption_ProcessUntilQuit);
    ICoreDispatcher_Release(dispatcher);
    return hr;
}
static HRESULT WINAPI host_Uninitialize(IFrameworkView *iface)
{
    struct xaml_host *impl = impl_from_IFrameworkView(iface);
    ICoreWindow_remove_CharacterReceived(impl->window, impl->character_token);
    return ICoreApplicationView_remove_Activated(impl->view, impl->activation_token);
}
static const IFrameworkViewVtbl host_vtbl = {host_QueryInterface, host_AddRef, host_Release, host_GetIids,
    host_GetRuntimeClassName, host_GetTrustLevel, host_Initialize, host_SetWindow, host_Load, host_Run, host_Uninitialize};
static HRESULT run_application(void)
{
    struct xaml_host *impl;
    ICoreApplication *app;
    HSTRING name;
    HRESULT hr;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IFrameworkViewSource_iface.lpVtbl = &source_vtbl;
    impl->IFrameworkView_iface.lpVtbl = &host_vtbl;
    impl->character.lpVtbl = &character_vtbl;
    impl->activated.lpVtbl = &activated_vtbl;
    impl->ref = 1;
    hr = WindowsCreateString(L"Windows.ApplicationModel.Core.CoreApplication", 45, &name);
    if (SUCCEEDED(hr))
    {
        hr = RoGetActivationFactory(name, &IID_ICoreApplication, (void **)&app);
        WindowsDeleteString(name);
        if (SUCCEEDED(hr))
        {
            hr = ICoreApplication_Run(app, &impl->IFrameworkViewSource_iface);
            ICoreApplication_Release(app);
        }
    }
    IFrameworkViewSource_Release(&impl->IFrameworkViewSource_iface);
    return hr;
}
