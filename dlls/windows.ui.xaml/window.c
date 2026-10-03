/* XAML Window backed by the thread's CoreWindow.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "private.h"
#include "roapi.h"
#define WIDL_using_Windows_UI_Xaml
#define WIDL_using_Windows_UI_Core
#include "windows.ui.xaml.h"
WINE_DEFAULT_DEBUG_CHANNEL(xaml);
struct window_factory
{
    IActivationFactory IActivationFactory_iface;
    IWindowStatics IWindowStatics_iface;
    IWindow IWindow_iface;
};
static IInspectable *content;
HRESULT xaml_get_core_window(IInspectable **out)
{
    ICoreWindowStatic *statics;
    HSTRING name;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = WindowsCreateString(L"Windows.UI.Core.CoreWindow", 26, &name);
    if (FAILED(hr)) return hr;
    hr = RoGetActivationFactory(name, &IID_ICoreWindowStatic, (void **)&statics);
    WindowsDeleteString(name);
    if (FAILED(hr)) return hr;
    hr = ICoreWindowStatic_GetForCurrentThread(statics, (ICoreWindow **)out);
    ICoreWindowStatic_Release(statics);
    return SUCCEEDED(hr) && !*out ? E_ILLEGAL_METHOD_CALL : hr;
}
void xaml_window_clear(void) { IInspectable *old = content; content = NULL; if (old) IInspectable_Release(old); }
static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct window_factory *impl = CONTAINING_RECORD(iface, struct window_factory, IActivationFactory_iface);
    if (!out) return E_POINTER;
    *out = NULL;
    TRACE("Window iid %s\n", debugstr_guid(iid));
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IActivationFactory) || IsEqualGUID(iid, &IID_IAgileObject)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IWindowStatics)) *out = &impl->IWindowStatics_iface;
    else if (IsEqualGUID(iid, &IID_IWindow)) *out = &impl->IWindow_iface;
    else return E_NOINTERFACE;
    return S_OK;
}
static ULONG WINAPI factory_AddRef(IActivationFactory *iface) { return 2; }
static ULONG WINAPI factory_Release(IActivationFactory *iface) { return 1; }
static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **ids)
{ if (!count || !ids) return E_POINTER; *count = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *out)
{ return WindowsCreateString(L"Windows.UI.Xaml.Window", 22, out); }
static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IActivationFactoryVtbl factory_vtbl = {factory_QueryInterface, factory_AddRef, factory_Release,
    factory_GetIids, factory_GetRuntimeClassName, factory_GetTrustLevel, factory_ActivateInstance};
DEFINE_IINSPECTABLE(window, IWindow, struct window_factory, IActivationFactory_iface)
DEFINE_IINSPECTABLE(statics, IWindowStatics, struct window_factory, IActivationFactory_iface)
static HRESULT WINAPI statics_get_Current(IWindowStatics *iface, IWindow **out)
{
    IInspectable *core;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (FAILED(hr = xaml_get_core_window(&core))) return hr;
    IInspectable_Release(core);
    return statics_QueryInterface(iface, &IID_IWindow, (void **)out);
}
static const IWindowStaticsVtbl statics_vtbl = {statics_QueryInterface, statics_AddRef, statics_Release,
    statics_GetIids, statics_GetRuntimeClassName, statics_GetTrustLevel, statics_get_Current};
#define FORWARD(method, args) \
    ICoreWindow *core; HRESULT hr = xaml_get_core_window((IInspectable **)&core); \
    if (FAILED(hr)) return hr; hr = ICoreWindow_##method args; ICoreWindow_Release(core); return hr
static HRESULT WINAPI window_get_Bounds(IWindow *iface, Rect *out) { FORWARD(get_Bounds, (core, out)); }
static HRESULT WINAPI window_get_Visible(IWindow *iface, boolean *out) { FORWARD(get_Visible, (core, out)); }
static HRESULT WINAPI window_get_CoreWindow(IWindow *iface, ICoreWindow **out) { return xaml_get_core_window((IInspectable **)out); }
static HRESULT WINAPI window_get_Dispatcher(IWindow *iface, ICoreDispatcher **out) { FORWARD(get_Dispatcher, (core, out)); }
static HRESULT WINAPI window_get_Content(IWindow *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = content; if (*out) IInspectable_AddRef(*out); return S_OK; }
static HRESULT WINAPI window_put_Content(IWindow *iface, IInspectable *value)
{
    TRACE("content %p\n", value);
    if (value) IInspectable_AddRef(value);
    xaml_window_clear(); content = value;
    return S_OK;
}
#define WINDOW_EVENT(name, type) \
static HRESULT WINAPI window_add_##name(IWindow *iface, IInspectable *handler, EventRegistrationToken *token) \
{ FORWARD(add_##name, (core, (type *)handler, token)); } \
static HRESULT WINAPI window_remove_##name(IWindow *iface, EventRegistrationToken token) \
{ FORWARD(remove_##name, (core, token)); }
WINDOW_EVENT(Activated, ITypedEventHandler_CoreWindow_WindowActivatedEventArgs)
WINDOW_EVENT(Closed, ITypedEventHandler_CoreWindow_CoreWindowEventArgs)
WINDOW_EVENT(SizeChanged, ITypedEventHandler_CoreWindow_WindowSizeChangedEventArgs)
WINDOW_EVENT(VisibilityChanged, ITypedEventHandler_CoreWindow_VisibilityChangedEventArgs)
static HRESULT WINAPI window_Activate(IWindow *iface) { FORWARD(Activate, (core)); }
static HRESULT WINAPI window_Close(IWindow *iface) { FORWARD(Close, (core)); }
static const IWindowVtbl window_vtbl = {window_QueryInterface, window_AddRef, window_Release,
    window_GetIids, window_GetRuntimeClassName, window_GetTrustLevel, window_get_Bounds, window_get_Visible,
    window_get_Content, window_put_Content, window_get_CoreWindow, window_get_Dispatcher,
    window_add_Activated, window_remove_Activated, window_add_Closed, window_remove_Closed,
    window_add_SizeChanged, window_remove_SizeChanged, window_add_VisibilityChanged, window_remove_VisibilityChanged,
    window_Activate, window_Close};
static struct window_factory factory = {{&factory_vtbl}, {&statics_vtbl}, {&window_vtbl}};
IActivationFactory *window_factory = &factory.IActivationFactory_iface;

HRESULT xaml_window_layout(void)
{
    ICoreWindow *window;
    Rect bounds;
    Size size;
    HRESULT hr;
    if (!content) return S_OK;
    if (FAILED(hr = xaml_get_core_window((IInspectable **)&window))) return hr;
    hr = ICoreWindow_get_Bounds(window, &bounds); ICoreWindow_Release(window);
    if (FAILED(hr)) return hr;
    size.Width = bounds.Width; size.Height = bounds.Height;
    return xaml_control_layout(content, size);
}
