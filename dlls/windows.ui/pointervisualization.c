/* Per-view pointer feedback controls. LGPL-2.1-or-later. */
#include "private.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(ui);
struct feedback { IPointerVisualizationSettings iface; LONG ref; };
static HRESULT WINAPI feedback_QueryInterface(IPointerVisualizationSettings *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) && !IsEqualGUID(iid,&IID_IPointerVisualizationSettings)) return E_NOINTERFACE;
    *out = iface; IPointerVisualizationSettings_AddRef(iface); return S_OK;
}
static ULONG WINAPI feedback_AddRef(IPointerVisualizationSettings *iface)
{ return InterlockedIncrement(&((struct feedback *)iface)->ref); }
static ULONG WINAPI feedback_Release(IPointerVisualizationSettings *iface)
{ ULONG ref = InterlockedDecrement(&((struct feedback *)iface)->ref); if (!ref) free(iface); return ref; }
static HRESULT WINAPI feedback_GetIids(IPointerVisualizationSettings *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count=0; if (!(*iids=CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids=IID_IPointerVisualizationSettings; *count=1; return S_OK;
}
static HRESULT WINAPI feedback_GetRuntimeClassName(IPointerVisualizationSettings *iface, HSTRING *name)
{ const WCHAR *str=RuntimeClass_Windows_UI_Input_PointerVisualizationSettings; return WindowsCreateString(str,wcslen(str),name); }
static HRESULT WINAPI feedback_GetTrustLevel(IPointerVisualizationSettings *iface, TrustLevel *level)
{ if (!level) return E_POINTER; *level=BaseTrust; return S_OK; }
static HRESULT WINAPI feedback_put(IPointerVisualizationSettings *iface, boolean value)
{
    TRACE("feedback enabled %d\n",value);
    /* Wine does not draw touch/stylus contact feedback yet. Disabling it is supported. */
    return value ? E_NOTIMPL : S_OK;
}
static HRESULT WINAPI feedback_get(IPointerVisualizationSettings *iface, boolean *value)
{ if (!value) return E_POINTER; *value=FALSE; return S_OK; }
static const IPointerVisualizationSettingsVtbl feedback_vtbl =
{ feedback_QueryInterface,feedback_AddRef,feedback_Release,feedback_GetIids,feedback_GetRuntimeClassName,feedback_GetTrustLevel,
  feedback_put,feedback_get,feedback_put,feedback_get };
HRESULT pointervisualization_create(IPointerVisualizationSettings **out)
{
    struct feedback *impl=calloc(1,sizeof(*impl));
    if (!impl) return E_OUTOFMEMORY;
    impl->iface.lpVtbl=&feedback_vtbl; impl->ref=1; *out=&impl->iface; return S_OK;
}
struct pointervisualization_statics
{
    IActivationFactory IActivationFactory_iface;
    IPointerVisualizationSettingsStatics IPointerVisualizationSettingsStatics_iface;
    LONG ref;
};

static inline struct pointervisualization_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct pointervisualization_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct pointervisualization_statics *impl = impl_from_IActivationFactory( iface );

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
    else if (IsEqualGUID( iid, &IID_IPointerVisualizationSettingsStatics ))
    {
        *out = &impl->IPointerVisualizationSettingsStatics_iface;
        IPointerVisualizationSettingsStatics_AddRef( &impl->IPointerVisualizationSettingsStatics_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct pointervisualization_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct pointervisualization_statics *impl = impl_from_IActivationFactory( iface );
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

DEFINE_IINSPECTABLE( pointervisualization_static, IPointerVisualizationSettingsStatics, struct pointervisualization_statics, IActivationFactory_iface )

static HRESULT STDMETHODCALLTYPE pointervisualization_static_GetForCurrentView(IPointerVisualizationSettingsStatics *iface, IPointerVisualizationSettings **out)
{ return corewindow_get_pointervisualization(out); }

static const struct IPointerVisualizationSettingsStaticsVtbl pointervisualization_static_vtbl =
{
    pointervisualization_static_QueryInterface,
    pointervisualization_static_AddRef,
    pointervisualization_static_Release,
    /* IInspectable methods */
    pointervisualization_static_GetIids,
    pointervisualization_static_GetRuntimeClassName,
    pointervisualization_static_GetTrustLevel,
    /* IPointerVisualizationSettingsStatics methods */
    pointervisualization_static_GetForCurrentView
};

static struct pointervisualization_statics pointervisualization_statics =
{
    {&factory_vtbl},
    {&pointervisualization_static_vtbl},
    1,
};

IActivationFactory *pointervisualization_factory = &pointervisualization_statics.IActivationFactory_iface;
