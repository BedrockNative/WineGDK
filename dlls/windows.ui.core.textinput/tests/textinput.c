/*
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

#include <stdarg.h>
#define COBJMACROS
#define CONST_VTABLE
#include "initguid.h"
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winstring.h"
#include "roapi.h"
#include "weakreference.h"
#include "wine/test.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_UI_ViewManagement_Core
#define WIDL_using_Windows_Foundation_Collections
#include "windows.ui.viewmanagement.core.h"

#define WIDL_using_Windows_UI_Text_Core
#include "windows.ui.text.core.h"

#define check_interface(obj, iid, supported) _check_interface(__LINE__, obj, iid, supported)
static void _check_interface(unsigned int line, void *obj, const IID *iid, BOOL supported)
{
    IUnknown *iface = obj, *unknown;
    HRESULT hr;

    hr = IUnknown_QueryInterface(iface, iid, (void **)&unknown);
    ok_(__FILE__, line)(hr == S_OK || (!supported && hr == E_NOINTERFACE), "Got unexpected hr %#lx.\n", hr);
    if (SUCCEEDED(hr))
        IUnknown_Release(unknown);
}

static void test_CoreInputViewStatics(void)
{
    IActivationFactory *factory;
    HSTRING str = NULL;
    HRESULT hr;
    LONG ref;

    hr = WindowsCreateString(RuntimeClass_Windows_UI_ViewManagement_Core_CoreInputView,
                             wcslen(RuntimeClass_Windows_UI_ViewManagement_Core_CoreInputView), &str);
    ok(hr == S_OK, "Got unexpected hr %#lx.\n", hr);
    hr = RoGetActivationFactory(str, &IID_IActivationFactory, (void **)&factory);
    WindowsDeleteString(str);
    ok(hr == S_OK || broken(hr == REGDB_E_CLASSNOTREG), "got hr %#lx.\n", hr);
    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("%s runtimeclass not registered, skipping tests.\n",
                 wine_dbgstr_w(RuntimeClass_Windows_UI_ViewManagement_Core_CoreInputView));
        return;
    }

    check_interface(factory, &IID_IUnknown, TRUE);
    check_interface(factory, &IID_IInspectable, TRUE);
    check_interface(factory, &IID_IActivationFactory, TRUE);
    check_interface(factory, &IID_ICoreInputViewStatics, TRUE);
    check_interface(factory, &IID_IAgileObject, FALSE);

    ref = IActivationFactory_Release(factory);
    ok(ref == 1, "Got unexpected refcount %ld.\n", ref);
}

static void test_CoreInputView(void)
{
    ICoreInputViewStatics *core_input_view_statics;
    IVectorView_CoreInputViewOcclusion *occlusions;
    ICoreInputView *core_input_view;
    IActivationFactory *factory;
    HSTRING str = NULL;
    HRESULT hr;
    LONG ref;

    hr = WindowsCreateString(RuntimeClass_Windows_UI_ViewManagement_Core_CoreInputView,
                             wcslen(RuntimeClass_Windows_UI_ViewManagement_Core_CoreInputView), &str);
    ok(hr == S_OK, "Got unexpected hr %#lx.\n", hr);
    hr = RoGetActivationFactory(str, &IID_IActivationFactory, (void **)&factory);
    WindowsDeleteString(str);
    ok(hr == S_OK || broken(hr == REGDB_E_CLASSNOTREG), "got hr %#lx.\n", hr);
    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("%s runtimeclass not registered, skipping tests.\n",
                 wine_dbgstr_w(RuntimeClass_Windows_UI_ViewManagement_Core_CoreInputView));
        return;
    }

    hr = IActivationFactory_QueryInterface(factory, &IID_ICoreInputViewStatics,
                                           (void **)&core_input_view_statics);
    ok(hr == S_OK, "Got unexpected hr %#lx.\n", hr);

    hr = ICoreInputViewStatics_GetForCurrentView(core_input_view_statics, &core_input_view);
    ok(hr == S_OK, "Got unexpected hr %#lx.\n", hr);

    check_interface(core_input_view, &IID_IUnknown, TRUE);
    check_interface(core_input_view, &IID_IInspectable, TRUE);
    check_interface(core_input_view, &IID_IAgileObject, TRUE);
    check_interface(core_input_view, &IID_IWeakReferenceSource, TRUE);
    check_interface(core_input_view, &IID_ICoreInputView, TRUE);
    check_interface(core_input_view, &IID_ICoreInputView2, TRUE);
    check_interface(core_input_view, &IID_ICoreInputView3, TRUE);
    check_interface(core_input_view, &IID_ICoreInputView4, TRUE);

    hr = ICoreInputView_GetCoreInputViewOcclusions(core_input_view, &occlusions);
    ok(hr == S_OK, "Got unexpected hr %#lx.\n", hr);
    IVectorView_CoreInputViewOcclusion_Release(occlusions);

    ICoreInputView_Release(core_input_view);

    ref = ICoreInputViewStatics_Release(core_input_view_statics);
    ok(ref == 2, "Got unexpected refcount %ld.\n", ref);
    ref = IActivationFactory_Release(factory);
    ok(ref == 1, "Got unexpected refcount %ld.\n", ref);
}

static void test_CoreTextServicesManager(void)
{
    ICoreTextServicesManager *core_text_manager;
    ICoreTextServicesManagerStatics *core_text_manager_stat;
    IActivationFactory *factory;
    HSTRING str = NULL;
    HRESULT hr;
    LONG ref;

    hr = WindowsCreateString(RuntimeClass_Windows_UI_Text_Core_CoreTextServicesManager,
                             wcslen(RuntimeClass_Windows_UI_Text_Core_CoreTextServicesManager), &str);
    ok(hr == S_OK, "Got unexpected hr %#lx.\n", hr);
    hr = RoGetActivationFactory(str, &IID_IActivationFactory, (void **)&factory);
    WindowsDeleteString(str);
    ok(hr == S_OK || broken(hr == REGDB_E_CLASSNOTREG), "got hr %#lx.\n", hr);
    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("%s runtimeclass not registered, skipping tests.\n",
                 wine_dbgstr_w(RuntimeClass_Windows_UI_Text_Core_CoreTextServicesManager));
        return;
    }

    hr = IActivationFactory_QueryInterface(factory, &IID_ICoreTextServicesManagerStatics,
                                           (void **)&core_text_manager_stat);
    ok(hr == S_OK, "Got unexpected hr %#lx.\n", hr);

    hr = ICoreTextServicesManagerStatics_GetForCurrentView(core_text_manager_stat, &core_text_manager);
    ok(hr == S_OK || broken(hr == RPC_E_WRONG_THREAD) /* <= Win10 2009 */, "Got unexpected hr %#lx.\n", hr);
    if (hr == S_OK)
    {
        check_interface(core_text_manager, &IID_IUnknown, TRUE);
        check_interface(core_text_manager, &IID_IInspectable, TRUE);
        check_interface(core_text_manager, &IID_IAgileObject, TRUE);
        check_interface(core_text_manager, &IID_ICoreTextServicesManagerStatics, FALSE);
        check_interface(core_text_manager, &IID_ICoreTextServicesManager, TRUE);

        ICoreTextServicesManager_Release(core_text_manager);
    }

    ref = ICoreTextServicesManagerStatics_Release(core_text_manager_stat);
    ok(ref == 2, "Got unexpected refcount %ld.\n", ref);
    ref = IActivationFactory_Release(factory);
    ok(ref == 1, "Got unexpected refcount %ld.\n", ref);
}

typedef ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgs text_handler;
static unsigned int text_update_count;

static HRESULT WINAPI text_handler_QueryInterface(text_handler *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) &&
        !IsEqualGUID(iid, &IID_ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgs))
        return E_NOINTERFACE;
    *out = iface;
    return S_OK;
}

static ULONG WINAPI text_handler_AddRef(text_handler *iface) { return 2; }
static ULONG WINAPI text_handler_Release(text_handler *iface) { return 1; }

static HRESULT WINAPI text_handler_Invoke(text_handler *iface, ICoreTextEditContext *sender,
                                          ICoreTextTextUpdatingEventArgs *args)
{
    HSTRING text;
    HRESULT hr;
    hr = ICoreTextTextUpdatingEventArgs_get_Text(args, &text);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!wcscmp(WindowsGetStringRawBuffer(text, NULL), L"a"), "Unexpected text.\n");
    WindowsDeleteString(text);
    ++text_update_count;
    return ICoreTextTextUpdatingEventArgs_put_Result(args, CoreTextTextUpdatingResult_Succeeded);
}

static const ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgsVtbl text_handler_vtbl =
{
    text_handler_QueryInterface, text_handler_AddRef, text_handler_Release, text_handler_Invoke
};

static void test_desktop_focus(void)
{
    ICoreTextServicesManagerStatics *statics;
    ICoreTextServicesManager *manager;
    ICoreTextEditContext *context;
    text_handler handler = {&text_handler_vtbl};
    EventRegistrationToken token;
    HSTRING name;
    HWND hwnd;
    HRESULT hr;

    WindowsCreateString(RuntimeClass_Windows_UI_Text_Core_CoreTextServicesManager,
                        wcslen(RuntimeClass_Windows_UI_Text_Core_CoreTextServicesManager), &name);
    hr = RoGetActivationFactory(name, &IID_ICoreTextServicesManagerStatics, (void **)&statics);
    WindowsDeleteString(name);
    if (FAILED(hr)) { win_skip("CoreText unavailable, hr %#lx.\n", hr); return; }
    hr = ICoreTextServicesManagerStatics_GetForCurrentView(statics, &manager);
    ICoreTextServicesManagerStatics_Release(statics);
    if (FAILED(hr)) { win_skip("CoreText unavailable on this thread, hr %#lx.\n", hr); return; }
    hr = ICoreTextServicesManager_CreateEditContext(manager, &context);
    ICoreTextServicesManager_Release(manager);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    if (FAILED(hr)) return;

    hwnd = CreateWindowExW(0, L"static", L"CoreText desktop test", WS_OVERLAPPEDWINDOW,
                           0, 0, 200, 100, NULL, NULL, NULL, NULL);
    ok(!!hwnd, "Failed to create window.\n");
    SetFocus(hwnd);
    ok(GetFocus() == hwnd, "Window did not receive focus.\n");
    hr = ICoreTextEditContext_add_TextUpdating(context, &handler, &token);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = ICoreTextEditContext_NotifyFocusEnter(context);
    ok(hr == S_OK, "Desktop focus failed, hr %#lx.\n", hr);
    hr = ICoreTextEditContext_NotifyFocusEnter(context);
    ok(hr == S_OK, "Repeated focus failed, hr %#lx.\n", hr);
    /* Wine's character bridge is synchronous; native TSF need not handle injected WM_CHAR. */
    if (!strcmp(winetest_platform, "wine"))
    {
        text_update_count = 0;
        SendMessageW(hwnd, WM_CHAR, 'a', 1);
        ok(text_update_count == 1, "Got %u text updates.\n", text_update_count);
    }
    hr = ICoreTextEditContext_NotifyFocusLeave(context);
    ok(hr == S_OK, "Focus leave failed, hr %#lx.\n", hr);
    text_update_count = 0;
    SendMessageW(hwnd, WM_CHAR, 'a', 1);
    ok(!text_update_count, "Received text after leaving focus.\n");
    hr = ICoreTextEditContext_NotifyFocusEnter(context);
    ok(hr == S_OK, "Refocus failed, hr %#lx.\n", hr);
    DestroyWindow(hwnd);
    hr = ICoreTextEditContext_NotifyFocusLeave(context);
    ok(hr == S_OK, "Focus leave after destruction failed, hr %#lx.\n", hr);
    ICoreTextEditContext_remove_TextUpdating(context, token);
    ICoreTextEditContext_Release(context);
}

START_TEST(textinput)
{
    HRESULT hr;

    hr = RoInitialize(RO_INIT_MULTITHREADED);
    ok(hr == S_OK, "RoInitialize failed, hr %#lx\n", hr);

    test_CoreInputViewStatics();
    test_CoreInputView();
    test_desktop_focus();

    RoUninitialize();
}
