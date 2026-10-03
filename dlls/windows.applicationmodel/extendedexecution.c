/* Extended execution sessions. Wine processes are not suspended by the UWP broker.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "store_private.h"
#include "wine/debug.h"
#include "wine/winrt_events.h"
WINE_DEFAULT_DEBUG_CHANNEL(model);
struct session
{
    IExtendedExecutionSession IExtendedExecutionSession_iface;
    IClosable IClosable_iface;
    LONG ref;
    BOOL closed;
    ExtendedExecutionReason reason;
    UINT32 progress;
    HSTRING description;
    struct winrt_event revoked;
};
static HRESULT WINAPI session_QueryInterface(IExtendedExecutionSession *iface, REFIID iid, void **out)
{
    struct session *impl = CONTAINING_RECORD(iface, struct session, IExtendedExecutionSession_iface);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_IExtendedExecutionSession)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IClosable)) *out = &impl->IClosable_iface;
    else return E_NOINTERFACE;
    IExtendedExecutionSession_AddRef(iface); return S_OK;
}
static ULONG WINAPI session_AddRef(IExtendedExecutionSession *iface)
{ return InterlockedIncrement(&CONTAINING_RECORD(iface, struct session, IExtendedExecutionSession_iface)->ref); }
static ULONG WINAPI session_Release(IExtendedExecutionSession *iface)
{
    struct session *impl = CONTAINING_RECORD(iface, struct session, IExtendedExecutionSession_iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { WindowsDeleteString(impl->description); winrt_event_clear(&impl->revoked); free(impl); }
    return ref;
}
static HRESULT WINAPI session_GetIids(IExtendedExecutionSession *iface, ULONG *count, IID **ids)
{ if (!count || !ids) return E_POINTER; *count = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI session_GetRuntimeClassName(IExtendedExecutionSession *iface, HSTRING *out)
{ const WCHAR *name = RuntimeClass_Windows_ApplicationModel_ExtendedExecution_ExtendedExecutionSession; return WindowsCreateString(name, wcslen(name), out); }
static HRESULT WINAPI session_GetTrustLevel(IExtendedExecutionSession *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
#define SESSION_IMPL struct session *impl = CONTAINING_RECORD(iface, struct session, IExtendedExecutionSession_iface); if (impl->closed) return RO_E_CLOSED
static HRESULT WINAPI session_get_Reason(IExtendedExecutionSession *iface, ExtendedExecutionReason *out)
{ SESSION_IMPL; if (!out) return E_POINTER; *out = impl->reason; return S_OK; }
static HRESULT WINAPI session_put_Reason(IExtendedExecutionSession *iface, ExtendedExecutionReason value)
{ SESSION_IMPL; if (value < 0 || value > 2) return E_INVALIDARG; impl->reason = value; return S_OK; }
static HRESULT WINAPI session_get_Description(IExtendedExecutionSession *iface, HSTRING *out)
{ SESSION_IMPL; return WindowsDuplicateString(impl->description, out); }
static HRESULT WINAPI session_put_Description(IExtendedExecutionSession *iface, HSTRING value)
{
    HSTRING copy; HRESULT hr; SESSION_IMPL;
    if (FAILED(hr = WindowsDuplicateString(value, &copy))) return hr;
    WindowsDeleteString(impl->description); impl->description = copy; return S_OK;
}
static HRESULT WINAPI session_get_PercentProgress(IExtendedExecutionSession *iface, UINT32 *out)
{ SESSION_IMPL; if (!out) return E_POINTER; *out = impl->progress; return S_OK; }
static HRESULT WINAPI session_put_PercentProgress(IExtendedExecutionSession *iface, UINT32 value)
{ SESSION_IMPL; if (value > 100) return E_INVALIDARG; impl->progress = value; return S_OK; }
static HRESULT WINAPI session_add_Revoked(IExtendedExecutionSession *iface, IInspectable *handler, EventRegistrationToken *token)
{ SESSION_IMPL; return winrt_event_add(&impl->revoked, handler, token); }
static HRESULT WINAPI session_remove_Revoked(IExtendedExecutionSession *iface, EventRegistrationToken token)
{ SESSION_IMPL; return winrt_event_remove(&impl->revoked, token); }
static HRESULT extension_callback(IUnknown *invoker, IUnknown *param, PROPVARIANT *out, BOOL async)
{ out->vt = VT_I4; out->lVal = ExtendedExecutionResult_Allowed; return S_OK; }
static HRESULT WINAPI session_RequestExtensionAsync(IExtendedExecutionSession *iface, IAsyncOperation_ExtendedExecutionResult **out)
{
    SESSION_IMPL;
    return async_operation_extension_create(&IID_IAsyncOperation_ExtendedExecutionResult, (IUnknown *)iface, NULL, extension_callback, out);
}
static const IExtendedExecutionSessionVtbl session_vtbl = {session_QueryInterface, session_AddRef, session_Release,
    session_GetIids, session_GetRuntimeClassName, session_GetTrustLevel, session_get_Reason, session_put_Reason,
    session_get_Description, session_put_Description, session_get_PercentProgress, session_put_PercentProgress,
    session_add_Revoked, session_remove_Revoked, session_RequestExtensionAsync};
DEFINE_IINSPECTABLE(closable, IClosable, struct session, IExtendedExecutionSession_iface)
static HRESULT WINAPI closable_Close(IClosable *iface)
{ struct session *impl = impl_from_IClosable(iface); impl->closed = TRUE; winrt_event_clear(&impl->revoked); return S_OK; }
static const IClosableVtbl closable_vtbl = {closable_QueryInterface, closable_AddRef, closable_Release,
    closable_GetIids, closable_GetRuntimeClassName, closable_GetTrustLevel, closable_Close};
static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) && !IsEqualGUID(iid, &IID_IActivationFactory) && !IsEqualGUID(iid, &IID_IAgileObject)) return E_NOINTERFACE;
    *out = iface; return S_OK;
}
static ULONG WINAPI factory_AddRef(IActivationFactory *iface) { return 2; }
static ULONG WINAPI factory_Release(IActivationFactory *iface) { return 1; }
static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **ids) { return session_GetIids(NULL, count, ids); }
static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *out) { return session_GetRuntimeClassName(NULL, out); }
static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *out) { return session_GetTrustLevel(NULL, out); }
static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{
    struct session *impl;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IExtendedExecutionSession_iface.lpVtbl = &session_vtbl;
    impl->IClosable_iface.lpVtbl = &closable_vtbl; impl->ref = 1;
    *out = (IInspectable *)&impl->IExtendedExecutionSession_iface; return S_OK;
}
static const IActivationFactoryVtbl factory_vtbl = {factory_QueryInterface, factory_AddRef, factory_Release,
    factory_GetIids, factory_GetRuntimeClassName, factory_GetTrustLevel, factory_ActivateInstance};
static IActivationFactory factory = {&factory_vtbl};
IActivationFactory *extendedexecution_factory = &factory;
