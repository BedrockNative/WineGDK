/* Legacy Store API backed by Xodus. LGPL-2.1-or-later. */
#include <errno.h>
#include "store_private.h"
#include "roapi.h"
#include "wine/debug.h"
#include "wine/winrt_events.h"
WINE_DEFAULT_DEBUG_CHANNEL(model);
struct license
{
    ILicenseInformation iface;
    LONG ref;
    struct winrt_event changed;
    INIT_ONCE once;
    HRESULT status;
    boolean active, trial;
    DateTime expiration;
};

static BOOL CALLBACK license_load(INIT_ONCE *once, void *param, void **context)
{
    struct license *impl = param;
    struct store_info info;
    const WCHAR *active, *trial, *expiration;
    WCHAR *end;

    impl->status = store_request(L"License", &info);
    if (SUCCEEDED(impl->status))
    {
        active = WindowsGetStringRawBuffer(info.fields[STORE_ACTIVE], NULL);
        trial = WindowsGetStringRawBuffer(info.fields[STORE_TRIAL], NULL);
        expiration = WindowsGetStringRawBuffer(info.fields[STORE_EXPIRATION], NULL);
        if ((wcscmp(active, L"true") && wcscmp(active, L"false")) ||
            (wcscmp(trial, L"true") && wcscmp(trial, L"false"))) impl->status = E_INVALIDARG;
        else
        {
            errno = 0;
            impl->expiration.UniversalTime = wcstoll(expiration, &end, 10);
            if (end == expiration || *end || errno == ERANGE || impl->expiration.UniversalTime < 0)
                impl->status = E_INVALIDARG;
            else
            {
                impl->active = !wcscmp(active, L"true");
                impl->trial = !wcscmp(trial, L"true");
            }
        }
    }
    store_info_clear(&info);
    TRACE("License query completed: %#lx, active %u, trial %u.\n", impl->status, impl->active, impl->trial);
    return TRUE;
}

static HRESULT license_update(struct license *impl)
{
    InitOnceExecuteOnce(&impl->once, license_load, impl, NULL);
    return impl->status;
}
static HRESULT WINAPI license_QueryInterface(ILicenseInformation *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) && !IsEqualGUID(iid,&IID_IAgileObject) && !IsEqualGUID(iid,&IID_ILicenseInformation)) return E_NOINTERFACE;
    *out=iface; ILicenseInformation_AddRef(iface); return S_OK;
}
static ULONG WINAPI license_AddRef(ILicenseInformation *iface) { return InterlockedIncrement(&((struct license *)iface)->ref); }
static ULONG WINAPI license_Release(ILicenseInformation *iface)
{
    struct license *impl=(void *)iface;
    ULONG ref=InterlockedDecrement(&impl->ref);
    if (!ref) { winrt_event_clear(&impl->changed); free(impl); } return ref;
}
static HRESULT WINAPI license_GetIids(ILicenseInformation *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count=0; if (!(*iids=CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids=IID_ILicenseInformation; *count=1; return S_OK;
}
static HRESULT WINAPI license_GetRuntimeClassName(ILicenseInformation *iface, HSTRING *name)
{ const WCHAR *str=RuntimeClass_Windows_ApplicationModel_Store_LicenseInformation; return WindowsCreateString(str,wcslen(str),name); }
static HRESULT WINAPI license_GetTrustLevel(ILicenseInformation *iface, TrustLevel *trust)
{ if (!trust) return E_POINTER; *trust=BaseTrust; return S_OK; }
static HRESULT WINAPI license_get_ProductLicenses(ILicenseInformation *iface, IMapView_HSTRING_ProductLicense **out)
{ if (!out) return E_POINTER; *out=NULL; FIXME("ProductLicenses not implemented.\n"); return E_NOTIMPL; }
static HRESULT WINAPI license_get_IsActive(ILicenseInformation *iface, boolean *value)
{
    struct license *impl = (void *)iface;
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = FALSE;
    if (SUCCEEDED(hr = license_update(impl))) *value = impl->active;
    return hr;
}
static HRESULT WINAPI license_get_IsTrial(ILicenseInformation *iface, boolean *value)
{
    struct license *impl = (void *)iface;
    HRESULT hr;
    if (!value) return E_POINTER;
    *value = FALSE;
    if (SUCCEEDED(hr = license_update(impl))) *value = impl->trial;
    return hr;
}
static HRESULT WINAPI license_get_ExpirationDate(ILicenseInformation *iface, DateTime *value)
{
    struct license *impl = (void *)iface;
    HRESULT hr;
    if (!value) return E_POINTER;
    value->UniversalTime = 0;
    if (SUCCEEDED(hr = license_update(impl))) *value = impl->expiration;
    return hr;
}
static HRESULT WINAPI license_add_LicenseChanged(ILicenseInformation *iface, ILicenseChangedEventHandler *handler, EventRegistrationToken *token)
{ return winrt_event_add(&((struct license *)iface)->changed,handler,token); }
static HRESULT WINAPI license_remove_LicenseChanged(ILicenseInformation *iface, EventRegistrationToken token)
{ return winrt_event_remove(&((struct license *)iface)->changed,token); }
static const ILicenseInformationVtbl license_vtbl={license_QueryInterface,license_AddRef,license_Release,license_GetIids,
    license_GetRuntimeClassName,license_GetTrustLevel,license_get_ProductLicenses,license_get_IsActive,license_get_IsTrial,
    license_get_ExpirationDate,license_add_LicenseChanged,license_remove_LicenseChanged};
struct currentapp_statics
{
    IActivationFactory IActivationFactory_iface;
    ICurrentApp ICurrentApp_iface;
    LONG ref;
};

static inline struct currentapp_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct currentapp_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct currentapp_statics *impl = impl_from_IActivationFactory( iface );

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
    else if (IsEqualGUID( iid, &IID_ICurrentApp ))
    {
        *out = &impl->ICurrentApp_iface;
        ICurrentApp_AddRef( &impl->ICurrentApp_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct currentapp_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct currentapp_statics *impl = impl_from_IActivationFactory( iface );
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

DEFINE_IINSPECTABLE( currentapp_static, ICurrentApp, struct currentapp_statics, IActivationFactory_iface )

static HRESULT WINAPI currentapp_static_get_LicenseInformation(ICurrentApp *iface, ILicenseInformation **out)
{
    struct license *impl;
    if (!out) return E_POINTER;
    *out=NULL;
    if (!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl=&license_vtbl; impl->ref=1; *out=&impl->iface; return S_OK;
}
static HRESULT WINAPI currentapp_static_get_LinkUri(ICurrentApp *iface, IUriRuntimeClass **out)
{ if (!out) return E_POINTER; *out=NULL; FIXME("get_LinkUri: Store service unavailable.\n"); return E_NOTIMPL; }
static HRESULT WINAPI currentapp_static_get_AppId(ICurrentApp *iface, GUID *out)
{ if (!out) return E_POINTER; memset(out,0,sizeof(*out)); FIXME("get_AppId: Store service unavailable.\n"); return E_NOTIMPL; }
static HRESULT WINAPI currentapp_static_RequestAppPurchaseAsync(ICurrentApp *iface, boolean include_receipt, IAsyncOperation_HSTRING **out)
{ if (!out) return E_POINTER; *out=NULL; FIXME("RequestAppPurchaseAsync: Store service unavailable.\n"); return E_NOTIMPL; }
static HRESULT WINAPI currentapp_static_RequestProductPurchaseAsync(ICurrentApp *iface, HSTRING product_id, boolean include_receipt, IAsyncOperation_HSTRING **out)
{ if (!out) return E_POINTER; *out=NULL; FIXME("RequestProductPurchaseAsync: Store service unavailable.\n"); return E_NOTIMPL; }
static HRESULT WINAPI currentapp_static_LoadListingInformationAsync(ICurrentApp *iface, IAsyncOperation_ListingInformation **out)
{ if (!out) return E_POINTER; *out=NULL; FIXME("LoadListingInformationAsync: Store service unavailable.\n"); return E_NOTIMPL; }
static HRESULT receipt_callback(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    struct store_info info;
    const WCHAR *receipt;
    UINT32 length;
    HRESULT hr;
    if (!called_async) return STATUS_PENDING;
    if (FAILED(hr = store_request(L"AppReceipt", &info))) return hr;
    receipt = WindowsGetStringRawBuffer(info.fields[STORE_RECEIPT], &length);
    if (!(result->pwszVal = CoTaskMemAlloc((length + 1) * sizeof(WCHAR)))) hr = E_OUTOFMEMORY;
    else { result->vt = VT_LPWSTR; memcpy(result->pwszVal, receipt, (length + 1) * sizeof(WCHAR)); }
    store_info_clear(&info);
    TRACE("App receipt request completed: %#lx, characters %u.\n", hr, length);
    return hr;
}
static HRESULT WINAPI currentapp_static_GetAppReceiptAsync(ICurrentApp *iface, IAsyncOperation_HSTRING **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    return async_operation_hstring_create((IUnknown *)iface, NULL, receipt_callback, out);
}
static HRESULT WINAPI currentapp_static_GetProductReceiptAsync(ICurrentApp *iface, HSTRING product_id, IAsyncOperation_HSTRING **out)
{ if (!out) return E_POINTER; *out=NULL; FIXME("GetProductReceiptAsync: Store service unavailable.\n"); return E_NOTIMPL; }

static const struct ICurrentAppVtbl currentapp_static_vtbl =
{
    currentapp_static_QueryInterface,
    currentapp_static_AddRef,
    currentapp_static_Release,
    /* IInspectable methods */
    currentapp_static_GetIids,
    currentapp_static_GetRuntimeClassName,
    currentapp_static_GetTrustLevel,
    /* ICurrentApp methods */
currentapp_static_get_LicenseInformation,
    currentapp_static_get_LinkUri,
    currentapp_static_get_AppId,
    currentapp_static_RequestAppPurchaseAsync,
    currentapp_static_RequestProductPurchaseAsync,
    currentapp_static_LoadListingInformationAsync,
    currentapp_static_GetAppReceiptAsync,
    currentapp_static_GetProductReceiptAsync
};

static struct currentapp_statics currentapp_statics =
{
    {&factory_vtbl},
    {&currentapp_static_vtbl},
    1,
};

IActivationFactory *currentapp_factory = &currentapp_statics.IActivationFactory_iface;
