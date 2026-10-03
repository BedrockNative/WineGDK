/* Resource language contexts. LGPL-2.1-or-later. */
#include "private.h"
#include "roapi.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(model);
static HRESULT resource_context_create(IResourceContext **out);
struct resource_statics
{
    IActivationFactory IActivationFactory_iface;
    IResourceContextStatics2 IResourceContextStatics2_iface;
    LONG ref;
};

static inline struct resource_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct resource_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct resource_statics *impl = impl_from_IActivationFactory( iface );

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
    else if (IsEqualGUID( iid, &IID_IResourceContextStatics2 ))
    {
        *out = &impl->IResourceContextStatics2_iface;
        IResourceContextStatics2_AddRef( &impl->IResourceContextStatics2_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct resource_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct resource_statics *impl = impl_from_IActivationFactory( iface );
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
    return resource_context_create((IResourceContext **)instance);
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

DEFINE_IINSPECTABLE( resource_static, IResourceContextStatics2, struct resource_statics, IActivationFactory_iface )

struct resource_context { IResourceContext iface; LONG ref; SRWLOCK lock; IVectorView_HSTRING *languages; };
static HRESULT WINAPI context_qi(IResourceContext *iface, REFIID iid, void **out)
{
 if (!out) return E_POINTER; *out=NULL;
 if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) && !IsEqualGUID(iid,&IID_IAgileObject) && !IsEqualGUID(iid,&IID_IResourceContext)) return E_NOINTERFACE;
 *out=iface; IResourceContext_AddRef(iface); return S_OK;
}
static ULONG WINAPI context_addref(IResourceContext *iface) { return InterlockedIncrement(&((struct resource_context *)iface)->ref); }
static ULONG WINAPI context_release(IResourceContext *iface)
{
 struct resource_context *impl=(void *)iface; ULONG ref=InterlockedDecrement(&impl->ref);
 if (!ref) { if (impl->languages) IVectorView_HSTRING_Release(impl->languages); free(impl); } return ref;
}
static HRESULT WINAPI context_iids(IResourceContext *iface, ULONG *count, IID **iids)
{
 if (!count || !iids) return E_POINTER;
 *count=0; if (!(*iids=CoTaskMemAlloc(sizeof(IID)))) return E_OUTOFMEMORY;
 **iids=IID_IResourceContext; *count=1; return S_OK;
}
static HRESULT WINAPI context_class(IResourceContext *iface, HSTRING *out)
{ const WCHAR *name=L"Windows.ApplicationModel.Resources.Core.ResourceContext"; return WindowsCreateString(name,wcslen(name),out); }
static HRESULT WINAPI context_trust(IResourceContext *iface, TrustLevel *out) { if (!out) return E_POINTER; *out=BaseTrust; return S_OK; }
static HRESULT WINAPI context_qualifiers(IResourceContext *iface, IObservableMap_HSTRING_HSTRING **out)
{ if (!out) return E_POINTER; *out=NULL; FIXME("qualifier map not implemented\n"); return E_NOTIMPL; }
static HRESULT default_languages(IVectorView_HSTRING **out)
{
 IApplicationLanguagesStatics *statics; HSTRING name; HRESULT hr;
 const WCHAR *type=L"Windows.Globalization.ApplicationLanguages";
 if (FAILED(hr=WindowsCreateString(type,wcslen(type),&name))) return hr;
 hr=RoGetActivationFactory(name,&IID_IApplicationLanguagesStatics,(void **)&statics);
 WindowsDeleteString(name);
 if (FAILED(hr)) return hr;
 hr=IApplicationLanguagesStatics_get_Languages(statics,out);
 IApplicationLanguagesStatics_Release(statics); return hr;
}
static HRESULT WINAPI context_languages(IResourceContext *iface, IVectorView_HSTRING **out)
{
 struct resource_context *impl=(void *)iface;
 if (!out) return E_POINTER;
 AcquireSRWLockShared(&impl->lock); *out=impl->languages; IVectorView_HSTRING_AddRef(*out); ReleaseSRWLockShared(&impl->lock); return S_OK;
}
static HRESULT WINAPI context_set_languages(IResourceContext *iface, IVectorView_HSTRING *value)
{
 struct resource_context *impl=(void *)iface; IVectorView_HSTRING *old;
 if (!value) return E_INVALIDARG;
 IVectorView_HSTRING_AddRef(value); AcquireSRWLockExclusive(&impl->lock); old=impl->languages; impl->languages=value; ReleaseSRWLockExclusive(&impl->lock); IVectorView_HSTRING_Release(old); return S_OK;
}
static HRESULT WINAPI context_reset(IResourceContext *iface)
{
 IVectorView_HSTRING *languages; HRESULT hr=default_languages(&languages);
 if (SUCCEEDED(hr)) { hr=context_set_languages(iface,languages); IVectorView_HSTRING_Release(languages); } return hr;
}
static HRESULT WINAPI context_reset_keys(IResourceContext *iface, IIterable_HSTRING *names) { return E_NOTIMPL; }
static HRESULT WINAPI context_override(IResourceContext *iface, IInspectable *qualifiers) { return E_NOTIMPL; }
static HRESULT WINAPI context_clone(IResourceContext *iface, IResourceContext **out)
{
 IVectorView_HSTRING *languages; HRESULT hr=resource_context_create(out);
 if (SUCCEEDED(hr)) { context_languages(iface,&languages); context_set_languages(*out,languages); IVectorView_HSTRING_Release(languages); } return hr;
}
static const IResourceContextVtbl context_vtbl={context_qi,context_addref,context_release,context_iids,context_class,context_trust,context_qualifiers,context_reset,context_reset_keys,context_override,context_clone,context_languages,context_set_languages};
static HRESULT resource_context_create(IResourceContext **out)
{
 struct resource_context *impl; HRESULT hr;
 if (!out) return E_POINTER; *out=NULL;
 if (!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
 impl->iface.lpVtbl=&context_vtbl; impl->ref=1;
 if (FAILED(hr=default_languages(&impl->languages))) { free(impl); return hr; }
 *out=&impl->iface; return S_OK;
}
static HRESULT WINAPI resource_static_view(IResourceContextStatics2 *iface,IResourceContext **out) { return resource_context_create(out); }
static HRESULT WINAPI resource_static_set(IResourceContextStatics2 *iface,HSTRING key,HSTRING value) { FIXME("global qualifier %s not implemented\n",debugstr_hstring(key)); return E_NOTIMPL; }
static HRESULT WINAPI resource_static_reset(IResourceContextStatics2 *iface) { return S_OK; }
static HRESULT WINAPI resource_static_reset_keys(IResourceContextStatics2 *iface,IIterable_HSTRING *names) { return E_NOTIMPL; }
static const IResourceContextStatics2Vtbl resource_vtbl={resource_static_QueryInterface,resource_static_AddRef,resource_static_Release,resource_static_GetIids,resource_static_GetRuntimeClassName,resource_static_GetTrustLevel,resource_static_view,resource_static_set,resource_static_reset,resource_static_reset_keys,resource_static_view};
static struct resource_statics resource_statics={{&factory_vtbl},{&resource_vtbl},1};
IActivationFactory *resource_context_factory=&resource_statics.IActivationFactory_iface;
