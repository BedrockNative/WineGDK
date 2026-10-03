/* Desktop holographic capability detection. LGPL-2.1-or-later. */
#include "private.h"
WINE_DEFAULT_DEBUG_CHANNEL(twinapi);
struct holographic_statics
{
    IActivationFactory IActivationFactory_iface;
    IHolographicApplicationPreviewStatics IHolographicApplicationPreviewStatics_iface;
    LONG ref;
};

static inline struct holographic_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct holographic_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct holographic_statics *impl = impl_from_IActivationFactory( iface );

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
    else if (IsEqualGUID( iid, &IID_IHolographicApplicationPreviewStatics ))
    {
        *out = &impl->IHolographicApplicationPreviewStatics_iface;
        IHolographicApplicationPreviewStatics_AddRef( &impl->IHolographicApplicationPreviewStatics_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct holographic_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct holographic_statics *impl = impl_from_IActivationFactory( iface );
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

DEFINE_IINSPECTABLE( holographic_static, IHolographicApplicationPreviewStatics, struct holographic_statics, IActivationFactory_iface )

static HRESULT STDMETHODCALLTYPE holographic_static_IsCurrentViewPresentedOnHolographicDisplay(IHolographicApplicationPreviewStatics *iface, boolean *out)
{ if (!out) return E_POINTER; *out=FALSE; return S_OK; }
static HRESULT STDMETHODCALLTYPE holographic_static_IsHolographicActivation(IHolographicApplicationPreviewStatics *iface, IActivatedEventArgs *args, boolean *out)
{ if (!args) return E_INVALIDARG; if (!out) return E_POINTER; *out=FALSE; return S_OK; }

static const struct IHolographicApplicationPreviewStaticsVtbl holographic_static_vtbl =
{
    holographic_static_QueryInterface,
    holographic_static_AddRef,
    holographic_static_Release,
    /* IInspectable methods */
    holographic_static_GetIids,
    holographic_static_GetRuntimeClassName,
    holographic_static_GetTrustLevel,
    /* IHolographicApplicationPreviewStatics methods */
    holographic_static_IsCurrentViewPresentedOnHolographicDisplay,
    holographic_static_IsHolographicActivation
};

static struct holographic_statics holographic_statics =
{
    {&factory_vtbl},
    {&holographic_static_vtbl},
    1,
};

IActivationFactory *holographic_factory = &holographic_statics.IActivationFactory_iface;
