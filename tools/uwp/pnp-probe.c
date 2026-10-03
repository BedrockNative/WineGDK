/* Integration tests for PnpObject activation, async lifetime and system properties.
 * Build against this tree's generated WinRT headers (see run-probes.sh). */
#define COBJMACROS
#define CONST_VTABLE
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Devices_Enumeration_Pnp
#include "initguid.h"
#include "windows.devices.enumeration.pnp.h"
#include "roapi.h"
#include "winstring.h"
#include "winreg.h"
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(test) do { if (!(test)) { printf("FAIL line %d: %s\n", __LINE__, #test); ++failures; } } while (0)

static const WCHAR *property_names[] =
{
    L"System.Devices.ModelName", L"System.Devices.Manufacturer", L"System.Devices.ModelName"
};
struct iterator { IIterator_HSTRING iface; LONG ref; UINT32 index, count; };
static HRESULT WINAPI iter_qi(IIterator_HSTRING *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) &&
        !IsEqualGUID(iid, &IID_IIterator_HSTRING)) return E_NOINTERFACE;
    *out = iface;
    IIterator_HSTRING_AddRef(iface);
    return S_OK;
}
static ULONG WINAPI iter_addref(IIterator_HSTRING *iface) { return InterlockedIncrement(&((struct iterator *)iface)->ref); }
static ULONG WINAPI iter_release(IIterator_HSTRING *iface)
{
    ULONG ref = InterlockedDecrement(&((struct iterator *)iface)->ref);
    if (!ref) free(iface);
    return ref;
}
static HRESULT WINAPI iter_iids(IIterator_HSTRING *iface, ULONG *count, IID **iids)
{ (void)iface; (void)count; (void)iids; return E_NOTIMPL; }
static HRESULT WINAPI iter_name(IIterator_HSTRING *iface, HSTRING *name)
{ (void)iface; (void)name; return E_NOTIMPL; }
static HRESULT WINAPI iter_trust(IIterator_HSTRING *iface, TrustLevel *trust)
{ (void)iface; (void)trust; return E_NOTIMPL; }
static HRESULT WINAPI iter_current(IIterator_HSTRING *iface, HSTRING *value)
{
    struct iterator *iter = (struct iterator *)iface;
    if (iter->index >= iter->count) return E_BOUNDS;
    return WindowsCreateString(property_names[iter->index], wcslen(property_names[iter->index]), value);
}
static HRESULT WINAPI iter_valid(IIterator_HSTRING *iface, boolean *valid)
{
    struct iterator *iter = (struct iterator *)iface;
    *valid = iter->index < iter->count;
    return S_OK;
}
static HRESULT WINAPI iter_next(IIterator_HSTRING *iface, boolean *valid)
{
    ++((struct iterator *)iface)->index;
    return iter_valid(iface, valid);
}
static HRESULT WINAPI iter_many(IIterator_HSTRING *iface, UINT32 capacity, HSTRING *values, UINT32 *count)
{ (void)iface; (void)capacity; (void)values; (void)count; return E_NOTIMPL; }
static const IIterator_HSTRINGVtbl iter_vtbl =
{ iter_qi, iter_addref, iter_release, iter_iids, iter_name, iter_trust, iter_current, iter_valid, iter_next, iter_many };

struct iterable { IIterable_HSTRING iface; LONG ref; UINT32 count; };
static HRESULT WINAPI strings_qi(IIterable_HSTRING *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) &&
        !IsEqualGUID(iid, &IID_IIterable_HSTRING)) return E_NOINTERFACE;
    *out = iface;
    IIterable_HSTRING_AddRef(iface);
    return S_OK;
}
static ULONG WINAPI strings_addref(IIterable_HSTRING *iface) { return InterlockedIncrement(&((struct iterable *)iface)->ref); }
static ULONG WINAPI strings_release(IIterable_HSTRING *iface) { return InterlockedDecrement(&((struct iterable *)iface)->ref); }
static HRESULT WINAPI strings_iids(IIterable_HSTRING *iface, ULONG *count, IID **iids)
{ (void)iface; (void)count; (void)iids; return E_NOTIMPL; }
static HRESULT WINAPI strings_name(IIterable_HSTRING *iface, HSTRING *name)
{ (void)iface; (void)name; return E_NOTIMPL; }
static HRESULT WINAPI strings_trust(IIterable_HSTRING *iface, TrustLevel *trust)
{ (void)iface; (void)trust; return E_NOTIMPL; }
static HRESULT WINAPI strings_first(IIterable_HSTRING *iface, IIterator_HSTRING **out)
{
    struct iterator *iter = calloc(1, sizeof(*iter));
    if (!iter) return E_OUTOFMEMORY;
    iter->iface.lpVtbl = &iter_vtbl;
    iter->ref = 1;
    iter->count = ((struct iterable *)iface)->count;
    *out = &iter->iface;
    return S_OK;
}
static const IIterable_HSTRINGVtbl strings_vtbl =
{ strings_qi, strings_addref, strings_release, strings_iids, strings_name, strings_trust, strings_first };

struct completed { IAsyncOperationCompletedHandler_PnpObject iface; LONG ref, calls; HANDLE event; AsyncStatus status; };
static HRESULT WINAPI completed_qi(IAsyncOperationCompletedHandler_PnpObject *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IAgileObject) &&
        !IsEqualGUID(iid, &IID_IAsyncOperationCompletedHandler_PnpObject)) return E_NOINTERFACE;
    *out = iface;
    IAsyncOperationCompletedHandler_PnpObject_AddRef(iface);
    return S_OK;
}
static ULONG WINAPI completed_addref(IAsyncOperationCompletedHandler_PnpObject *iface)
{ return InterlockedIncrement(&((struct completed *)iface)->ref); }
static ULONG WINAPI completed_release(IAsyncOperationCompletedHandler_PnpObject *iface)
{ return InterlockedDecrement(&((struct completed *)iface)->ref); }
static HRESULT WINAPI completed_invoke(IAsyncOperationCompletedHandler_PnpObject *iface, IAsyncOperation_PnpObject *op, AsyncStatus status)
{
    struct completed *handler = (struct completed *)iface;
    (void)op;
    handler->status = status;
    InterlockedIncrement(&handler->calls);
    SetEvent(handler->event);
    return S_OK;
}
static const IAsyncOperationCompletedHandler_PnpObjectVtbl completed_vtbl =
{ completed_qi, completed_addref, completed_release, completed_invoke };

static HRESULT await_result(IAsyncOperation_PnpObject *op, IPnpObject **object, AsyncStatus expected)
{
    struct completed handler = {{&completed_vtbl}, 1, 0, NULL, Started};
    HRESULT hr;
    handler.event = CreateEventW(NULL, FALSE, FALSE, NULL);
    CHECK(handler.event != NULL);
    hr = IAsyncOperation_PnpObject_put_Completed(op, &handler.iface);
    CHECK(hr == S_OK);
    /* A timeout exits the probe: a live operation must never retain a stack handler. */
    if (FAILED(hr) || WaitForSingleObject(handler.event, 5000) != WAIT_OBJECT_0) ExitProcess(2);
    CHECK(handler.calls == 1 && handler.status == expected);
    hr = IAsyncOperation_PnpObject_GetResults(op, object);
    IAsyncOperation_PnpObject_Release(op);
    /* The callback signals before the async runtime drops its handler reference. */
    for (unsigned i = 0; InterlockedCompareExchange(&handler.ref, 0, 0) != 1 && i < 5000; ++i) Sleep(1);
    if (handler.ref != 1) ExitProcess(2);
    CloseHandle(handler.event);
    return hr;
}

int main(void)
{
    struct iterable properties = {{&strings_vtbl}, 1, 3};
    IPnpObjectStatics *factory;
    IAsyncOperation_PnpObject *op;
    IPnpObject *object;
    HSTRING classname, id, value;
    HRESULT hr;

    CHECK(RoInitialize(RO_INIT_MULTITHREADED) == S_OK);
    WindowsCreateString(L"Windows.Devices.Enumeration.Pnp.PnpObject", 41, &classname);
    hr = RoGetActivationFactory(classname, &IID_IPnpObjectStatics, (void **)&factory);
    WindowsDeleteString(classname);
    CHECK(hr == S_OK);
    if (FAILED(hr)) { printf("activation: %#lx\n", hr); return 1; }
    WindowsCreateString(L"{00000000-0000-0000-FFFF-FFFFFFFFFFFF}", 38, &id);
    CHECK(IPnpObjectStatics_CreateFromIdAsync(factory, PnpObjectType_DeviceContainer, id, &properties.iface, NULL) == E_POINTER);
    op = (void *)1;
    CHECK(IPnpObjectStatics_CreateFromIdAsync(factory, PnpObjectType_Unknown, id, &properties.iface, &op) == E_INVALIDARG && !op);
    CHECK(IPnpObjectStatics_CreateFromIdAsync(factory, PnpObjectType_DeviceContainer, NULL, &properties.iface, &op) == E_INVALIDARG);
    CHECK(IPnpObjectStatics_CreateFromIdAsync(factory, PnpObjectType_DeviceContainer, id, NULL, &op) == E_INVALIDARG);
    for (unsigned repeat = 0; repeat < 3; ++repeat)
    {
        IMapView_HSTRING_IInspectable *map;
        PnpObjectType type;
        UINT32 size;
        hr = IPnpObjectStatics_CreateFromIdAsync(factory, PnpObjectType_DeviceContainer, id, &properties.iface, &op);
        CHECK(hr == S_OK && op);
        if (FAILED(hr)) continue;
        object = NULL;
        hr = await_result(op, &object, Completed);
        CHECK(hr == S_OK && object);
        if (FAILED(hr) || !object) continue;
        CHECK(IPnpObject_get_Type(object, &type) == S_OK && type == PnpObjectType_DeviceContainer);
        CHECK(IPnpObject_get_Id(object, &value) == S_OK);
        CHECK(!wcscmp(WindowsGetStringRawBuffer(value, NULL), WindowsGetStringRawBuffer(id, NULL)));
        WindowsDeleteString(value);
        CHECK(IPnpObject_get_Properties(object, NULL) == E_POINTER);
        CHECK(IPnpObject_get_Properties(object, &map) == S_OK);
        IPnpObject_Release(object); /* Map must keep its data alive. */
        CHECK(IMapView_HSTRING_IInspectable_get_Size(map, &size) == S_OK && size <= 2);
        for (unsigned i = 0; i < 2; ++i)
        {
            WCHAR expected[1024];
            DWORD bytes = sizeof(expected);
            IInspectable *boxed;
            IPropertyValue *property;
            HSTRING key;
            LSTATUS status;
            status = RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS",
                                  i ? L"SystemManufacturer" : L"SystemProductName", RRF_RT_REG_SZ, NULL, expected, &bytes);
            WindowsCreateString(property_names[i], wcslen(property_names[i]), &key);
            hr = IMapView_HSTRING_IInspectable_Lookup(map, key, &boxed);
            WindowsDeleteString(key);
            if (status == ERROR_FILE_NOT_FOUND) { CHECK(hr == E_BOUNDS); continue; }
            CHECK(status == ERROR_SUCCESS && hr == S_OK);
            if (FAILED(hr)) continue;
            hr = IInspectable_QueryInterface(boxed, &IID_IPropertyValue, (void **)&property);
            IInspectable_Release(boxed);
            CHECK(hr == S_OK);
            if (FAILED(hr)) continue;
            CHECK(IPropertyValue_GetString(property, &value) == S_OK);
            CHECK(!wcscmp(expected, WindowsGetStringRawBuffer(value, NULL)));
            WindowsDeleteString(value);
            IPropertyValue_Release(property);
        }
        IMapView_HSTRING_IInspectable_Release(map);
    }
    /* Empty property iterables still identify the object and return an empty map. */
    properties.count = 0;
    hr = IPnpObjectStatics_CreateFromIdAsync(factory, PnpObjectType_DeviceContainer, id, &properties.iface, &op);
    CHECK(hr == S_OK);
    if (SUCCEEDED(hr))
    {
        object = NULL;
        hr = await_result(op, &object, Completed);
        CHECK(hr == S_OK && object);
        if (object)
        {
            IMapView_HSTRING_IInspectable *map;
            UINT32 size;
            CHECK(IPnpObject_get_Properties(object, &map) == S_OK);
            CHECK(IMapView_HSTRING_IInspectable_get_Size(map, &size) == S_OK && !size);
            IMapView_HSTRING_IInspectable_Release(map);
            IPnpObject_Release(object);
        }
    }
    WindowsDeleteString(id);
    WindowsCreateString(L"{11111111-1111-1111-1111-111111111111}", 38, &id);
    hr = IPnpObjectStatics_CreateFromIdAsync(factory, PnpObjectType_DeviceContainer, id, &properties.iface, &op);
    CHECK(hr == S_OK);
    if (SUCCEEDED(hr))
    {
        object = NULL;
        hr = await_result(op, &object, Error);
        CHECK(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && !object);
    }
    WindowsDeleteString(id);
    /* Exercise the typed collection over the existing device-interface backend. */
    {
        IAsyncOperation_PnpObjectCollection *collection_op;
        IAsyncInfo *info;
        IVectorView_PnpObject *view;
        IIterable_PnpObject *iterable;
        AsyncStatus status = Started;
        UINT32 size;
        hr = IPnpObjectStatics_FindAllAsync(factory, PnpObjectType_DeviceInterface, &properties.iface, &collection_op);
        CHECK(hr == S_OK);
        if (SUCCEEDED(hr))
        {
            CHECK(IAsyncOperation_PnpObjectCollection_QueryInterface(collection_op, &IID_IAsyncInfo, (void **)&info) == S_OK);
            for (unsigned i = 0; i < 5000 && status == Started; ++i)
            {
                CHECK(IAsyncInfo_get_Status(info, &status) == S_OK);
                if (status == Started) Sleep(1);
            }
            CHECK(status == Completed);
            IAsyncInfo_Release(info);
            hr = IAsyncOperation_PnpObjectCollection_GetResults(collection_op, &view);
            IAsyncOperation_PnpObjectCollection_Release(collection_op);
            CHECK(hr == S_OK && view);
            if (SUCCEEDED(hr) && view)
            {
                CHECK(IVectorView_PnpObject_get_Size(view, &size) == S_OK);
                CHECK(IVectorView_PnpObject_QueryInterface(view, &IID_IIterable_PnpObject, (void **)&iterable) == S_OK);
                IIterable_PnpObject_Release(iterable);
                object = NULL;
                CHECK(IVectorView_PnpObject_GetAt(view, size, &object) == E_BOUNDS && !object);
                for (UINT32 i = 0; i < size; ++i)
                {
                    PnpObjectType type;
                    CHECK(IVectorView_PnpObject_GetAt(view, i, &object) == S_OK);
                    CHECK(IPnpObject_get_Type(object, &type) == S_OK && type == PnpObjectType_DeviceInterface);
                    CHECK(IPnpObject_get_Id(object, &value) == S_OK && WindowsGetStringLen(value));
                    WindowsDeleteString(value);
                    IPnpObject_Release(object);
                }
                printf("Enumerated %u device interfaces\n", size);
                IVectorView_PnpObject_Release(view);
            }
        }
    }
    CHECK(properties.ref == 1);
    IPnpObjectStatics_Release(factory);
    RoUninitialize();
    printf("pnp: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures != 0;
}
