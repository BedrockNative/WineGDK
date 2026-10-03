/* Integration probe for the single-view CoreApplication/CoreWindow runtime. */
#define COBJMACROS
#define CONST_VTABLE
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_ApplicationModel
#define WIDL_using_Windows_ApplicationModel_Activation
#define WIDL_using_Windows_ApplicationModel_Core
#define WIDL_using_Windows_UI_Core
#define WIDL_using_Windows_Devices_Input
#define WIDL_using_Windows_UI_Input
#define WIDL_using_Windows_Graphics_Display
#define WIDL_using_Windows_Foundation_Metadata
#define WIDL_using_Windows_ApplicationModel_Preview_Holographic
#include "initguid.h"
#include "windows.applicationmodel.core.h"
#include "corewindow.h"
#include "windows.graphics.display.h"
#include "windows.foundation.metadata.h"
#include "windows.applicationmodel.preview.holographic.h"
#define WIDL_using_Windows_UI_Text_Core
#include "windows.ui.text.core.h"
#include "roapi.h"
#include "winstring.h"
#include "winuser.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(test) do { if (!(test)) { printf("FAIL line %d: %s\n", __LINE__, #test); ++failures; } } while (0)

#include "view-services-probe.h"
#include "pointer-probe.h"
#include "text-probe.h"
#include "editcontext-probe.h"

struct app
{
    IFrameworkViewSource source;
    IFrameworkView framework;
    ITypedEventHandler_CoreApplicationView_IActivatedEventArgs activated;
    ITypedEventHandler_IInspectable_IInspectable window_event;
    LONG ref;
    ICoreApplication *application;
    ICoreApplicationView *view;
    ICoreWindow *window;
    ICoreDispatcher *dispatcher;
    ICoreWindowStatic *window_statics;
    HWND hwnd;
    EventRegistrationToken activation_token, size_token, visibility_token, close_token;
    unsigned int fail_stage, step, resized, visible, closed;
    char order[16];
};
static struct app *from_source(IFrameworkViewSource *iface) { return CONTAINING_RECORD(iface, struct app, source); }
static struct app *from_framework(IFrameworkView *iface) { return CONTAINING_RECORD(iface, struct app, framework); }
static struct app *from_activated(ITypedEventHandler_CoreApplicationView_IActivatedEventArgs *iface)
{ return CONTAINING_RECORD(iface, struct app, activated); }
static struct app *from_event(ITypedEventHandler_IInspectable_IInspectable *iface)
{ return CONTAINING_RECORD(iface, struct app, window_event); }
static HRESULT step(struct app *app, unsigned int stage, char name)
{
    app->order[app->step++] = name;
    app->order[app->step] = 0;
    return app->fail_stage == stage ? E_ACCESSDENIED : S_OK;
}
static HRESULT app_qi(struct app *app, REFIID iid, void **out)
{
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IFrameworkViewSource))
        *out = &app->source;
    else if (IsEqualGUID(iid, &IID_IFrameworkView)) *out = &app->framework;
    else if (IsEqualGUID(iid, &IID_ITypedEventHandler_CoreApplicationView_IActivatedEventArgs)) *out = &app->activated;
    else return E_NOINTERFACE;
    InterlockedIncrement(&app->ref);
    return S_OK;
}
#define BASE(prefix, type, from) \
static HRESULT WINAPI prefix##_qi(type *iface, REFIID iid, void **out) { return app_qi(from(iface), iid, out); } \
static ULONG WINAPI prefix##_addref(type *iface) { return InterlockedIncrement(&from(iface)->ref); } \
static ULONG WINAPI prefix##_release(type *iface) { return InterlockedDecrement(&from(iface)->ref); }
#define INSPECTABLE(prefix, type) \
static HRESULT WINAPI prefix##_iids(type *iface, ULONG *count, IID **iids) { (void)iface; (void)count; (void)iids; return E_NOTIMPL; } \
static HRESULT WINAPI prefix##_name(type *iface, HSTRING *name) { (void)iface; (void)name; return E_NOTIMPL; } \
static HRESULT WINAPI prefix##_trust(type *iface, TrustLevel *trust) { (void)iface; (void)trust; return E_NOTIMPL; }
BASE(source, IFrameworkViewSource, from_source)
BASE(framework, IFrameworkView, from_framework)
BASE(activated, ITypedEventHandler_CoreApplicationView_IActivatedEventArgs, from_activated)
BASE(event, ITypedEventHandler_IInspectable_IInspectable, from_event)
INSPECTABLE(source, IFrameworkViewSource)
INSPECTABLE(framework, IFrameworkView)

static HRESULT WINAPI source_create(IFrameworkViewSource *iface, IFrameworkView **out)
{
    struct app *app = from_source(iface);
    HRESULT hr = step(app, 1, 'C');
    *out = NULL;
    if (FAILED(hr)) return hr;
    *out = &app->framework;
    IFrameworkView_AddRef(*out);
    return S_OK;
}
static HRESULT WINAPI framework_initialize(IFrameworkView *iface, ICoreApplicationView *view)
{
    struct app *app = from_framework(iface);
    ICoreApplicationView *current;
    ICoreWindow *window = (void *)1;
    ICoreImmersiveApplication *immersive = NULL;
    boolean flag;
    CHECK(ICoreApplication_GetCurrentView(app->application, &current) == S_OK && current == view);
    ICoreApplicationView_Release(current);
    CHECK(ICoreApplication_QueryInterface(app->application, &IID_ICoreImmersiveApplication, (void **)&immersive) == S_OK);
    if (immersive)
    {
        CHECK(ICoreImmersiveApplication_get_MainView(immersive, NULL) == E_POINTER);
        CHECK(ICoreImmersiveApplication_get_MainView(immersive, &current) == S_OK && current == view);
        if (current) ICoreApplicationView_Release(current);
        ICoreImmersiveApplication_Release(immersive);
    }
    CHECK(ICoreApplicationView_get_IsMain(view, &flag) == S_OK && flag);
    CHECK(ICoreApplicationView_get_IsHosted(view, &flag) == S_OK && !flag);
    CHECK(ICoreApplicationView_get_CoreWindow(view, &window) == S_OK && !window);
    CHECK(ICoreApplication_Run(app->application, &app->source) == E_ILLEGAL_METHOD_CALL);
    app->view = view;
    ICoreApplicationView_AddRef(view);
    CHECK(ICoreApplicationView_add_Activated(view, &app->activated, &app->activation_token) == S_OK);
    return step(app, 2, 'I');
}
static DWORD WINAPI other_thread(void *param)
{
    struct app *app = param;
    ICoreApplicationView *view = (void *)1;
    ICoreWindow *window = (void *)1;
    ICoreImmersiveApplication *immersive = NULL;
    ICoreDispatcher *dispatcher = NULL;
    boolean flag;
    Rect bounds;
    CHECK(ICoreDispatcher_get_HasThreadAccess(app->dispatcher, &flag) == S_OK && !flag);
    CHECK(ICoreDispatcher_ProcessEvents(app->dispatcher, CoreProcessEventsOption_ProcessAllIfPresent) == RPC_E_WRONG_THREAD);
    CHECK(ICoreWindow_get_Bounds(app->window, &bounds) == RPC_E_WRONG_THREAD);
    CHECK(ICoreWindowStatic_GetForCurrentThread(app->window_statics, &window) == S_OK && !window);
    CHECK(ICoreApplication_GetCurrentView(app->application, &view) == E_ILLEGAL_METHOD_CALL && !view);
    CHECK(ICoreApplication_QueryInterface(app->application, &IID_ICoreImmersiveApplication, (void **)&immersive) == S_OK);
    if (immersive)
    {
        CHECK(ICoreImmersiveApplication_get_MainView(immersive, &view) == S_OK && view == app->view);
        if (view)
        {
            CHECK(ICoreApplicationView_get_CoreWindow(view, &window) == S_OK && window == app->window);
            if (window)
            {
                CHECK(ICoreWindow_get_Dispatcher(window, &dispatcher) == S_OK && dispatcher == app->dispatcher);
                if (dispatcher) ICoreDispatcher_Release(dispatcher);
                ICoreWindow_Release(window);
            }
            ICoreApplicationView_Release(view);
        }
        ICoreImmersiveApplication_Release(immersive);
    }
    return 0;
}
static HRESULT WINAPI framework_set_window(IFrameworkView *iface, ICoreWindow *window)
{
    struct app *app = from_framework(iface);
    ICoreWindow *current;
    ICoreWindowInterop *interop;
    ICoreApplicationView2 *view2;
    ICoreDispatcher *dispatcher;
    HANDLE thread;
    Rect bounds;
    boolean flag;
    app->window = window;
    ICoreWindow_AddRef(window);
    CHECK(ICoreWindowStatic_GetForCurrentThread(app->window_statics, &current) == S_OK && current == window);
    ICoreWindow_Release(current);
    CHECK(ICoreApplicationView_get_CoreWindow(app->view, &current) == S_OK && current == window);
    ICoreWindow_Release(current);
    CHECK(ICoreWindow_get_Visible(window, &flag) == S_OK && !flag);
    CHECK(ICoreWindow_get_Bounds(window, &bounds) == S_OK && bounds.Width > 0 && bounds.Height > 0);
    CHECK(ICoreWindow_get_Dispatcher(window, &app->dispatcher) == S_OK);
    CHECK(ICoreApplicationView_QueryInterface(app->view, &IID_ICoreApplicationView2, (void **)&view2) == S_OK);
    CHECK(ICoreApplicationView2_get_Dispatcher(view2, &dispatcher) == S_OK && dispatcher == app->dispatcher);
    ICoreDispatcher_Release(dispatcher);
    ICoreApplicationView2_Release(view2);
    CHECK(ICoreDispatcher_get_HasThreadAccess(app->dispatcher, &flag) == S_OK && flag);
    CHECK(ICoreDispatcher_ProcessEvents(app->dispatcher, (CoreProcessEventsOption)99) == E_INVALIDARG);
    CHECK(ICoreWindow_QueryInterface(window, &IID_ICoreWindowInterop, (void **)&interop) == S_OK);
    CHECK(ICoreWindowInterop_get_WindowHandle(interop, &app->hwnd) == S_OK && IsWindow(app->hwnd));
    ICoreWindowInterop_Release(interop);
    if (!app->fail_stage) { test_view_services(window,app->dispatcher,app->hwnd); test_pointer(window,app->hwnd); test_text(window,app->dispatcher,app->hwnd); test_editcontext(app->hwnd); }
    thread = CreateThread(NULL, 0, other_thread, app, 0, NULL);
    CHECK(thread && WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    CloseHandle(thread);
    CHECK(ICoreWindow_add_SizeChanged(window, (ITypedEventHandler_CoreWindow_WindowSizeChangedEventArgs *)&app->window_event, &app->size_token) == S_OK);
    CHECK(ICoreWindow_add_VisibilityChanged(window, (ITypedEventHandler_CoreWindow_VisibilityChangedEventArgs *)&app->window_event, &app->visibility_token) == S_OK);
    CHECK(ICoreWindow_add_Closed(window, (ITypedEventHandler_CoreWindow_CoreWindowEventArgs *)&app->window_event, &app->close_token) == S_OK);
    return step(app, 3, 'W');
}
static HRESULT WINAPI framework_load(IFrameworkView *iface, HSTRING entry)
{
    CHECK(!WindowsGetStringLen(entry)); /* Loose executable, no registered entry point. */
    return step(from_framework(iface), 4, 'L');
}
static HRESULT WINAPI activated_invoke(ITypedEventHandler_CoreApplicationView_IActivatedEventArgs *iface,
                                      ICoreApplicationView *view, IActivatedEventArgs *args)
{
    struct app *app = from_activated(iface);
    ILaunchActivatedEventArgs *launch;
    ActivationKind kind;
    ApplicationExecutionState state;
    HSTRING value;
    HRESULT hr;
    CHECK(view == app->view);
    CHECK(IActivatedEventArgs_get_Kind(args, &kind) == S_OK && kind == ActivationKind_Launch);
    CHECK(IActivatedEventArgs_get_PreviousExecutionState(args, &state) == S_OK && state == ApplicationExecutionState_NotRunning);
    CHECK(IActivatedEventArgs_QueryInterface(args, &IID_ILaunchActivatedEventArgs, (void **)&launch) == S_OK);
    CHECK(ILaunchActivatedEventArgs_get_Arguments(launch, &value) == S_OK);
    WindowsDeleteString(value);
    ILaunchActivatedEventArgs_Release(launch);
    /* Self-unsubscription must not destroy the delegate while it is being called. */
    CHECK(ICoreApplicationView_remove_Activated(view, app->activation_token) == S_OK);
    hr = step(app, 5, 'A');
    if (FAILED(hr)) return hr;
    CHECK(ICoreWindow_Activate(app->window) == S_OK);
    return S_OK;
}
static HRESULT WINAPI event_invoke(ITypedEventHandler_IInspectable_IInspectable *iface, IInspectable *sender, IInspectable *args)
{
    struct app *app = from_event(iface);
    IWindowSizeChangedEventArgs *sized;
    IVisibilityChangedEventArgs *visible;
    ICoreWindowEventArgs *closed;
    CHECK(sender == (IInspectable *)app->window);
    if (SUCCEEDED(IInspectable_QueryInterface(args, &IID_IWindowSizeChangedEventArgs, (void **)&sized)))
    {
        Size size;
        CHECK(IWindowSizeChangedEventArgs_get_Size(sized, &size) == S_OK && size.Width >= 0 && size.Height >= 0);
        ++app->resized;
        IWindowSizeChangedEventArgs_Release(sized);
    }
    else if (SUCCEEDED(IInspectable_QueryInterface(args, &IID_IVisibilityChangedEventArgs, (void **)&visible)))
    {
        boolean flag;
        CHECK(IVisibilityChangedEventArgs_get_Visible(visible, &flag) == S_OK);
        if (flag) ++app->visible;
        IVisibilityChangedEventArgs_Release(visible);
    }
    else
    {
        CHECK(IInspectable_QueryInterface(args, &IID_ICoreWindowEventArgs, (void **)&closed) == S_OK);
        ++app->closed;
        /* Removing this handler during dispatch exercises delegate ownership. */
        CHECK(ICoreWindow_remove_Closed(app->window, app->close_token) == S_OK);
        ICoreWindowEventArgs_Release(closed);
    }
    return S_OK;
}
static HRESULT WINAPI framework_run(IFrameworkView *iface)
{
    struct app *app = from_framework(iface);
    boolean visible;
    HRESULT hr = step(app, 6, 'R');
    CHECK(ICoreWindow_get_Visible(app->window, &visible) == S_OK && visible);
    CHECK(app->visible >= 1);
    CHECK(SetWindowPos(app->hwnd, NULL, 0, 0, 640, 480, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    CHECK(ICoreDispatcher_ProcessEvents(app->dispatcher, CoreProcessEventsOption_ProcessAllIfPresent) == S_OK);
    CHECK(app->resized >= 1);
    /* A posted close must wake and terminate ProcessUntilQuit without a busy loop. */
    CHECK(PostMessageW(app->hwnd, WM_CLOSE, 0, 0));
    CHECK(ICoreDispatcher_ProcessEvents(app->dispatcher, CoreProcessEventsOption_ProcessUntilQuit) == S_OK);
    CHECK(app->closed == 1 && !IsWindow(app->hwnd));
    return hr;
}
static HRESULT WINAPI framework_uninitialize(IFrameworkView *iface)
{
    return step(from_framework(iface), 7, 'U');
}
static const IFrameworkViewSourceVtbl source_vtbl =
{ source_qi, source_addref, source_release, source_iids, source_name, source_trust, source_create };
static const IFrameworkViewVtbl framework_vtbl =
{ framework_qi, framework_addref, framework_release, framework_iids, framework_name, framework_trust,
  framework_initialize, framework_set_window, framework_load, framework_run, framework_uninitialize };
static const ITypedEventHandler_CoreApplicationView_IActivatedEventArgsVtbl activated_vtbl =
{ activated_qi, activated_addref, activated_release, activated_invoke };
static const ITypedEventHandler_IInspectable_IInspectableVtbl event_vtbl =
{ event_qi, event_addref, event_release, event_invoke };

int main(void)
{
    const char *expected[] = {"CIWLARU", "C", "CI", "CIW", "CIWL", "CIWLA", "CIWLARU", "CIWLARU"};
    struct app app = {0};
    ICoreApplication *application;
    ICoreWindowStatic *statics;
    IPropertySet *properties, *again;
    HSTRING name;
    HRESULT hr;
    CHECK(RoInitialize(RO_INIT_SINGLETHREADED) == S_OK);
    WindowsCreateString(L"Windows.ApplicationModel.Core.CoreApplication", 45, &name);
    hr = RoGetActivationFactory(name, &IID_ICoreApplication, (void **)&application);
    WindowsDeleteString(name);
    CHECK(hr == S_OK);
    if (FAILED(hr)) { printf("CoreApplication activation %#lx\n", hr); return 1; }
    WindowsCreateString(L"Windows.UI.Core.CoreWindow", 26, &name);
    hr = RoGetActivationFactory(name, &IID_ICoreWindowStatic, (void **)&statics);
    WindowsDeleteString(name);
    CHECK(hr == S_OK);
    if (FAILED(hr)) return 1;
    CHECK(ICoreApplication_Run(application, NULL) == E_INVALIDARG);
    CHECK(ICoreApplication_get_Properties(application, &properties) == S_OK);
    CHECK(ICoreApplication_get_Properties(application, &again) == S_OK && properties == again);
    IPropertySet_Release(properties);
    IPropertySet_Release(again);
    for (unsigned int stage = 0; stage < sizeof(expected) / sizeof(*expected); ++stage)
    {
        ICoreWindow *window = (void *)1;
        ICoreApplicationView *view = (void *)1;
        memset(&app, 0, sizeof(app));
        app.source.lpVtbl = &source_vtbl;
        app.framework.lpVtbl = &framework_vtbl;
        app.activated.lpVtbl = &activated_vtbl;
        app.window_event.lpVtbl = &event_vtbl;
        app.ref = 1;
        app.application = application;
        app.window_statics = statics;
        app.fail_stage = stage;
        CHECK(ICoreWindowStatic_GetForCurrentThread(statics, &window) == S_OK && !window);
        hr = ICoreApplication_Run(application, &app.source);
        CHECK(hr == (stage ? E_ACCESSDENIED : S_OK));
        CHECK(!strcmp(app.order, expected[stage]));
        CHECK(ICoreApplication_GetCurrentView(application, &view) == E_ILLEGAL_METHOD_CALL && !view);
        {
            ICoreImmersiveApplication *immersive = NULL;
            CHECK(ICoreApplication_QueryInterface(application, &IID_ICoreImmersiveApplication, (void **)&immersive) == S_OK);
            if (immersive)
            {
                view = (void *)1;
                CHECK(ICoreImmersiveApplication_get_MainView(immersive, &view) == E_ILLEGAL_METHOD_CALL && !view);
                ICoreImmersiveApplication_Release(immersive);
            }
        }
        CHECK(ICoreWindowStatic_GetForCurrentThread(statics, &window) == S_OK && !window);
        CHECK(!app.hwnd || !IsWindow(app.hwnd));
        if (app.dispatcher) ICoreDispatcher_Release(app.dispatcher);
        if (app.window) ICoreWindow_Release(app.window);
        if (app.view) ICoreApplicationView_Release(app.view);
        CHECK(app.ref == 1);
        printf("stage %u: %s, result %#lx\n", stage, app.order, hr);
    }
    ICoreWindowStatic_Release(statics);
    ICoreApplication_Release(application);
    RoUninitialize();
    printf("core: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures != 0;
}
