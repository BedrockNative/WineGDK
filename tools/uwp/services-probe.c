/* Integration checks for audio device watchers and apartment shutdown. */
#define COBJMACROS
#define CONST_VTABLE
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Devices_Enumeration
#include "initguid.h"
#include "windows.devices.enumeration.h"
#include "roapi.h"
#include "winstring.h"
#include <stdio.h>

static int failures;
#define CHECK(test) do { if (!(test)) { printf("FAIL line %d: %s\n", __LINE__, #test); ++failures; } } while (0)
struct handler
{
    ITypedEventHandler_DeviceWatcher_IInspectable iface;
    LONG ref, calls;
    HANDLE event;
};
static HRESULT WINAPI handler_qi(ITypedEventHandler_DeviceWatcher_IInspectable *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IAgileObject) &&
        !IsEqualGUID(iid, &IID_ITypedEventHandler_DeviceWatcher_IInspectable)) return E_NOINTERFACE;
    *out = iface;
    ITypedEventHandler_DeviceWatcher_IInspectable_AddRef(iface);
    return S_OK;
}
static ULONG WINAPI handler_addref(ITypedEventHandler_DeviceWatcher_IInspectable *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct handler, iface)->ref); }
static ULONG WINAPI handler_release(ITypedEventHandler_DeviceWatcher_IInspectable *iface)
{ return InterlockedDecrement(&CONTAINING_RECORD(iface, struct handler, iface)->ref); }
static HRESULT WINAPI handler_invoke(ITypedEventHandler_DeviceWatcher_IInspectable *iface, IDeviceWatcher *sender, IInspectable *args)
{
    struct handler *handler = CONTAINING_RECORD(iface, struct handler, iface);
    (void)sender; (void)args;
    InterlockedIncrement(&handler->calls);
    SetEvent(handler->event);
    return S_OK;
}
static const ITypedEventHandler_DeviceWatcher_IInspectableVtbl handler_vtbl =
{ handler_qi, handler_addref, handler_release, handler_invoke };

static void test_audio(void)
{
    static const WCHAR name[] = L"Windows.Devices.Enumeration.DeviceInformation";
    IDeviceInformationStatics *statics = NULL;
    IDeviceWatcher *watcher = NULL;
    struct handler handler = {{&handler_vtbl}, 1, 0, NULL};
    EventRegistrationToken enumerated, stopped, updated, removed;
    DeviceWatcherStatus status;
    HSTRING str;
    HRESULT hr;
    unsigned int i;
    handler.event = CreateEventW(NULL, FALSE, FALSE, NULL);
    WindowsCreateString(name, (sizeof(name)/sizeof(*name))-1, &str);
    hr = RoGetActivationFactory(str, &IID_IDeviceInformationStatics, (void **)&statics);
    WindowsDeleteString(str);
    CHECK(hr == S_OK);
    if (FAILED(hr)) { CloseHandle(handler.event); return; }
    CHECK(IDeviceInformationStatics_CreateWatcherDeviceClass(statics, (DeviceClass)-1, &watcher) == E_INVALIDARG && !watcher);
    CHECK(IDeviceInformationStatics_CreateWatcherDeviceClass(statics, DeviceClass_AudioRender, NULL) == E_POINTER);
    for (i = 0; i < 2; ++i)
    {
        hr = IDeviceInformationStatics_CreateWatcherDeviceClass(statics,
                i ? DeviceClass_AudioCapture : DeviceClass_AudioRender, &watcher);
        CHECK(hr == S_OK);
        if (FAILED(hr)) continue;
        CHECK(IDeviceWatcher_get_Status(watcher, &status) == S_OK && status == DeviceWatcherStatus_Created);
        CHECK(IDeviceWatcher_add_EnumerationCompleted(watcher, &handler.iface, &enumerated) == S_OK);
        CHECK(IDeviceWatcher_add_Stopped(watcher, &handler.iface, &stopped) == S_OK);
        CHECK(IDeviceWatcher_add_Updated(watcher, (ITypedEventHandler_DeviceWatcher_DeviceInformationUpdate *)&handler.iface, &updated) == S_OK);
        CHECK(IDeviceWatcher_add_Removed(watcher, (ITypedEventHandler_DeviceWatcher_DeviceInformationUpdate *)&handler.iface, &removed) == S_OK);
        CHECK(updated.value && removed.value && updated.value != removed.value);
        CHECK(handler.ref == 5);
        CHECK(IDeviceWatcher_remove_Updated(watcher, updated) == S_OK);
        CHECK(IDeviceWatcher_remove_Removed(watcher, removed) == S_OK);
        CHECK(handler.ref == 3);
        CHECK(IDeviceWatcher_Start(watcher) == S_OK);
        CHECK(WaitForSingleObject(handler.event, 5000) == WAIT_OBJECT_0);
        CHECK(IDeviceWatcher_get_Status(watcher, &status) == S_OK && status == DeviceWatcherStatus_EnumerationCompleted);
        CHECK(IDeviceWatcher_Start(watcher) == E_ILLEGAL_METHOD_CALL);
        CHECK(IDeviceWatcher_Stop(watcher) == S_OK);
        CHECK(WaitForSingleObject(handler.event, 5000) == WAIT_OBJECT_0);
        CHECK(IDeviceWatcher_get_Status(watcher, &status) == S_OK && status == DeviceWatcherStatus_Stopped);
        CHECK(IDeviceWatcher_remove_EnumerationCompleted(watcher, enumerated) == S_OK);
        CHECK(IDeviceWatcher_remove_Stopped(watcher, stopped) == S_OK);
        IDeviceWatcher_Release(watcher);
        /* Invoke signals before the worker releases its delegate snapshot. Wait
         * for that reference before reusing or leaving the stack-based handler. */
        for (unsigned j = 0; j < 5000 && InterlockedCompareExchange(&handler.ref,0,0) != 1; ++j) Sleep(1);
        CHECK(InterlockedCompareExchange(&handler.ref,0,0) == 1);
        if (InterlockedCompareExchange(&handler.ref,0,0) != 1) ExitProcess(1);
    }
    CHECK(handler.calls == 4);
    CHECK(handler.ref == 1);
    IDeviceInformationStatics_Release(statics);
    CloseHandle(handler.event);
}

struct shutdown_handler { IApartmentShutdown iface; LONG ref, calls; UINT64 id; };
static HRESULT WINAPI shutdown_qi(IApartmentShutdown *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IApartmentShutdown)) return E_NOINTERFACE;
    *out = iface; IApartmentShutdown_AddRef(iface); return S_OK;
}
static ULONG WINAPI shutdown_addref(IApartmentShutdown *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct shutdown_handler, iface)->ref); }
static ULONG WINAPI shutdown_release(IApartmentShutdown *iface)
{ return InterlockedDecrement(&CONTAINING_RECORD(iface, struct shutdown_handler, iface)->ref); }
static void WINAPI shutdown_notify(IApartmentShutdown *iface, UINT64 id)
{
    struct shutdown_handler *handler = CONTAINING_RECORD(iface, struct shutdown_handler, iface);
    CHECK(id == handler->id);
    ++handler->calls;
}
static const IApartmentShutdownVtbl shutdown_vtbl = {shutdown_qi, shutdown_addref, shutdown_release, shutdown_notify};
int main(void)
{
    struct shutdown_handler canceled = {{&shutdown_vtbl}, 1, 0, 0}, pending = {{&shutdown_vtbl}, 1, 0, 0};
    HRESULT (WINAPI *register_shutdown)(IApartmentShutdown *, UINT64 *, APARTMENT_SHUTDOWN_REGISTRATION_COOKIE *);
    HRESULT (WINAPI *unregister_shutdown)(APARTMENT_SHUTDOWN_REGISTRATION_COOKIE);
    APARTMENT_SHUTDOWN_REGISTRATION_COOKIE a, b;
    UINT64 id;
    HMODULE module = LoadLibraryW(L"combase.dll");
    register_shutdown = (void *)GetProcAddress(module, "RoRegisterForApartmentShutdown");
    unregister_shutdown = (void *)GetProcAddress(module, "RoUnregisterForApartmentShutdown");
    CHECK(register_shutdown && unregister_shutdown);
    CHECK(RoInitialize(RO_INIT_MULTITHREADED) == S_OK);
    test_audio();
    RoUninitialize();
    CHECK(RoInitialize(RO_INIT_SINGLETHREADED) == S_OK);
    CHECK(RoGetApartmentIdentifier(&id) == S_OK);
    CHECK(register_shutdown(&canceled.iface, &canceled.id, &a) == S_OK);
    CHECK(register_shutdown(&pending.iface, &pending.id, &b) == S_OK);
    CHECK(id == canceled.id && id == pending.id && a != b);
    CHECK(canceled.ref == 2 && pending.ref == 2);
    CHECK(unregister_shutdown(a) == S_OK && canceled.ref == 1);
    CHECK(unregister_shutdown(a) == E_INVALIDARG);
    RoUninitialize();
    CHECK(canceled.calls == 0 && pending.calls == 1 && pending.ref == 1);
    CHECK(unregister_shutdown(b) == E_INVALIDARG);
    FreeLibrary(module);
    printf("services: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return !!failures;
}
