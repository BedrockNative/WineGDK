/* Desktop-host power snapshots and subscriptions. LGPL-2.1-or-later. */
#include "private.h"

WINE_DEFAULT_DEBUG_CHANNEL(connectivity);
struct push_statics
{
    IActivationFactory IActivationFactory_iface;
    IPushNotificationChannelManagerStatics IPushNotificationChannelManagerStatics_iface;
    LONG ref;
};

static inline struct push_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct push_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct push_statics *impl = impl_from_IActivationFactory( iface );

    if (!out) return E_POINTER;
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
    else if (IsEqualGUID( iid, &IID_IPushNotificationChannelManagerStatics ))
    {
        *out = &impl->IPushNotificationChannelManagerStatics_iface;
        IPushNotificationChannelManagerStatics_AddRef( &impl->IPushNotificationChannelManagerStatics_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct push_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct push_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids = IID_IPushNotificationChannelManagerStatics;
    *count = 1;
    return S_OK;
}
static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    const WCHAR *runtime = RuntimeClass_Windows_Networking_PushNotifications_PushNotificationChannelManager;
    return WindowsCreateString(runtime, wcslen(runtime), name);
}
static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust )
{
    if (!trust) return E_POINTER;
    *trust = BaseTrust;
    return S_OK;
}
static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    if (!instance) return E_POINTER;
    *instance = NULL;
    return E_ILLEGAL_METHOD_CALL;
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

DEFINE_IINSPECTABLE( push_static, IPushNotificationChannelManagerStatics, struct push_statics, IActivationFactory_iface )


static HRESULT channel_callback(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    /* WNS requires a platform service that is not available on this host. */
    return called_async ? WPN_E_PLATFORM_UNAVAILABLE : 0x103;
}
static HRESULT WINAPI create_channel(IPushNotificationChannelManagerStatics *iface,
                                    IAsyncOperation_PushNotificationChannel **out)
{
    if (!out) return E_POINTER;
    return async_operation_inspectable_create(&IID_IAsyncOperation_PushNotificationChannel,
        (IUnknown *)iface, NULL, channel_callback, (IAsyncOperation_IInspectable **)out);
}
static HRESULT WINAPI create_channel_id(IPushNotificationChannelManagerStatics *iface, HSTRING id,
                                       IAsyncOperation_PushNotificationChannel **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!WindowsGetStringLen(id)) return E_INVALIDARG;
    return create_channel(iface, out);
}
static const IPushNotificationChannelManagerStaticsVtbl statics_vtbl = {
    push_static_QueryInterface, push_static_AddRef, push_static_Release,
    push_static_GetIids, push_static_GetRuntimeClassName, push_static_GetTrustLevel,
    create_channel, create_channel_id, create_channel_id
};
static struct push_statics manager = {{&factory_vtbl}, {&statics_vtbl}, 1};
IActivationFactory *push_manager_factory = &manager.IActivationFactory_iface;
