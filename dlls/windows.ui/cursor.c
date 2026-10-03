/* CoreCursor objects and native cursor mapping. */
#include "private.h"
#include "winuser.h"

struct cursor { ICoreCursor iface; LONG ref; CoreCursorType type; UINT32 id; };

HRESULT corecursor_handle(ICoreCursor *cursor, HCURSOR *out)
{
    static const WORD ids[] = {32512, 32515, 0, 32649, 32651, 32513, 32646, 32643,
                              32645, 32642, 32644, 32648, 32516, 32514};
    CoreCursorType type;
    UINT32 id;
    HRESULT hr;
    *out = NULL;
    if (!cursor) return S_OK;
    if (FAILED(hr = ICoreCursor_get_Type(cursor, &type))) return hr;
    if (type == CoreCursorType_Custom)
    {
        if (FAILED(hr = ICoreCursor_get_Id(cursor, &id))) return hr;
        if (id > 0xffff) return E_INVALIDARG;
        *out = LoadCursorW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(id));
    }
    else
    {
        if ((unsigned int)type >= ARRAY_SIZE(ids)) return E_NOTIMPL;
        *out = LoadCursorW(NULL, MAKEINTRESOURCEW(ids[type]));
    }
    return *out ? S_OK : HRESULT_FROM_WIN32(ERROR_RESOURCE_NAME_NOT_FOUND);
}
static HRESULT WINAPI cursor_qi(ICoreCursor *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) &&
        !IsEqualGUID(iid, &IID_IAgileObject) && !IsEqualGUID(iid, &IID_ICoreCursor)) return E_NOINTERFACE;
    *out = iface; ICoreCursor_AddRef(iface); return S_OK;
}
static ULONG WINAPI cursor_addref(ICoreCursor *iface)
{ return InterlockedIncrement(&((struct cursor *)iface)->ref); }
static ULONG WINAPI cursor_release(ICoreCursor *iface)
{ ULONG ref = InterlockedDecrement(&((struct cursor *)iface)->ref); if (!ref) free(iface); return ref; }
static HRESULT WINAPI cursor_iids(ICoreCursor *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids = IID_ICoreCursor; *count = 1; return S_OK;
}
static HRESULT WINAPI cursor_name(ICoreCursor *iface, HSTRING *name)
{ const WCHAR *str = RuntimeClass_Windows_UI_Core_CoreCursor; return WindowsCreateString(str, wcslen(str), name); }
static HRESULT WINAPI cursor_trust(ICoreCursor *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level = BaseTrust; return S_OK; }
static HRESULT WINAPI cursor_id(ICoreCursor *iface, UINT32 *value)
{ if (!value) return E_POINTER; *value = ((struct cursor *)iface)->id; return S_OK; }
static HRESULT WINAPI cursor_type(ICoreCursor *iface, CoreCursorType *value)
{ if (!value) return E_POINTER; *value = ((struct cursor *)iface)->type; return S_OK; }
static const ICoreCursorVtbl cursor_vtbl =
{cursor_qi, cursor_addref, cursor_release, cursor_iids, cursor_name, cursor_trust, cursor_id, cursor_type};
HRESULT corecursor_create(CoreCursorType type, UINT32 id, ICoreCursor **out)
{
    struct cursor *cursor;
    if (!out) return E_POINTER;
    *out = NULL;
    if ((unsigned int)type > CoreCursorType_Person) return E_INVALIDARG;
    if (!(cursor = calloc(1, sizeof(*cursor)))) return E_OUTOFMEMORY;
    cursor->iface.lpVtbl = &cursor_vtbl; cursor->ref = 1;
    cursor->type = type; cursor->id = id;
    *out = &cursor->iface; return S_OK;
}
struct cursor_factory
{
    IActivationFactory IActivationFactory_iface;
    ICoreCursorFactory ICoreCursorFactory_iface;
    LONG ref;
};
static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    struct cursor_factory *factory = CONTAINING_RECORD(iface, struct cursor_factory, IActivationFactory_iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IActivationFactory)) *out = iface;
    else if (IsEqualGUID(iid, &IID_ICoreCursorFactory)) *out = &factory->ICoreCursorFactory_iface;
    else return E_NOINTERFACE;
    IActivationFactory_AddRef(iface); return S_OK;
}
static ULONG WINAPI factory_AddRef(IActivationFactory *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct cursor_factory, IActivationFactory_iface)->ref); }
static ULONG WINAPI factory_Release(IActivationFactory *iface)
{ return InterlockedDecrement(&CONTAINING_RECORD(iface, struct cursor_factory, IActivationFactory_iface)->ref); }
static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **iids)
{ if (!count || !iids) return E_POINTER; *count = 0; *iids = NULL; return S_OK; }
static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *name)
{ return cursor_name(NULL, name); }
static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *level)
{ return cursor_trust(NULL, level); }
static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IActivationFactoryVtbl factory_vtbl =
{factory_QueryInterface, factory_AddRef, factory_Release, factory_GetIids, factory_GetRuntimeClassName, factory_GetTrustLevel, factory_ActivateInstance};
DEFINE_IINSPECTABLE(cursor_factory, ICoreCursorFactory, struct cursor_factory, IActivationFactory_iface)
static HRESULT WINAPI factory_CreateCursor(ICoreCursorFactory *iface, CoreCursorType type, UINT32 id, ICoreCursor **out)
{ return corecursor_create(type, id, out); }
static const ICoreCursorFactoryVtbl cursor_factory_vtbl =
{cursor_factory_QueryInterface, cursor_factory_AddRef, cursor_factory_Release, cursor_factory_GetIids,
 cursor_factory_GetRuntimeClassName, cursor_factory_GetTrustLevel, factory_CreateCursor};
static struct cursor_factory factory = {{&factory_vtbl}, {&cursor_factory_vtbl}, 1};
IActivationFactory *corecursor_factory = &factory.IActivationFactory_iface;
