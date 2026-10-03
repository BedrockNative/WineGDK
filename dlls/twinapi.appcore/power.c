/* Desktop-host power snapshots and subscriptions. LGPL-2.1-or-later. */
#include "private.h"
#include "wine/winrt_events.h"
WINE_DEFAULT_DEBUG_CHANNEL(twinapi);
struct power_statics
{
    IActivationFactory IActivationFactory_iface;
    IPowerManagerStatics IPowerManagerStatics_iface;
    LONG ref;
};

static inline struct power_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct power_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct power_statics *impl = impl_from_IActivationFactory( iface );

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
    else if (IsEqualGUID( iid, &IID_IPowerManagerStatics ))
    {
        *out = &impl->IPowerManagerStatics_iface;
        IPowerManagerStatics_AddRef( &impl->IPowerManagerStatics_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct power_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct power_statics *impl = impl_from_IActivationFactory( iface );
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

DEFINE_IINSPECTABLE( power_static, IPowerManagerStatics, struct power_statics, IActivationFactory_iface )

static struct winrt_event power_events[5];
static HRESULT power_status(SYSTEM_POWER_STATUS *status)
{ return GetSystemPowerStatus(status) ? S_OK : HRESULT_FROM_WIN32(GetLastError()); }
static HRESULT WINAPI power_static_get_EnergySaverStatus(IPowerManagerStatics *iface, EnergySaverStatus *out)
{
 SYSTEM_POWER_STATUS status; HRESULT hr;
 if (!out) return E_POINTER;
 if (FAILED(hr=power_status(&status))) return hr;
 *out=status.SystemStatusFlag ? EnergySaverStatus_On : EnergySaverStatus_Off; return S_OK;
}
static HRESULT WINAPI power_static_get_BatteryStatus(IPowerManagerStatics *iface, BatteryStatus *out)
{
 SYSTEM_POWER_STATUS status; HRESULT hr;
 if (!out) return E_POINTER;
 if (FAILED(hr=power_status(&status))) return hr;
 *out=(status.BatteryFlag & 128) ? BatteryStatus_NotPresent :
       (status.BatteryFlag & 8) ? BatteryStatus_Charging :
       status.ACLineStatus==0 ? BatteryStatus_Discharging : BatteryStatus_Idle; return S_OK;
}
static HRESULT WINAPI power_static_get_PowerSupplyStatus(IPowerManagerStatics *iface, PowerSupplyStatus *out)
{
 SYSTEM_POWER_STATUS status; HRESULT hr;
 if (!out) return E_POINTER;
 if (FAILED(hr=power_status(&status))) return hr;
 *out=status.ACLineStatus==1 ? PowerSupplyStatus_Adequate : PowerSupplyStatus_NotPresent; return S_OK;
}
static HRESULT WINAPI power_static_get_RemainingChargePercent(IPowerManagerStatics *iface, INT32 *out)
{
 SYSTEM_POWER_STATUS status; HRESULT hr;
 if (!out) return E_POINTER;
 if (FAILED(hr=power_status(&status))) return hr;
 *out=status.BatteryLifePercent==255 ? -1 : status.BatteryLifePercent; return S_OK;
}
static HRESULT WINAPI power_static_get_RemainingDischargeTime(IPowerManagerStatics *iface, TimeSpan *out)
{
 SYSTEM_POWER_STATUS status; HRESULT hr;
 if (!out) return E_POINTER;
 if (FAILED(hr=power_status(&status))) return hr;
 out->Duration=status.BatteryLifeTime==MAXDWORD ? MAXLONGLONG : (INT64)status.BatteryLifeTime*10000000; return S_OK;
}
static HRESULT WINAPI power_static_add_EnergySaverStatusChanged(IPowerManagerStatics *iface, IEventHandler_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&power_events[0],handler,token); }
static HRESULT WINAPI power_static_remove_EnergySaverStatusChanged(IPowerManagerStatics *iface, EventRegistrationToken token)
{ return winrt_event_remove(&power_events[0],token); }
static HRESULT WINAPI power_static_add_BatteryStatusChanged(IPowerManagerStatics *iface, IEventHandler_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&power_events[1],handler,token); }
static HRESULT WINAPI power_static_remove_BatteryStatusChanged(IPowerManagerStatics *iface, EventRegistrationToken token)
{ return winrt_event_remove(&power_events[1],token); }
static HRESULT WINAPI power_static_add_PowerSupplyStatusChanged(IPowerManagerStatics *iface, IEventHandler_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&power_events[2],handler,token); }
static HRESULT WINAPI power_static_remove_PowerSupplyStatusChanged(IPowerManagerStatics *iface, EventRegistrationToken token)
{ return winrt_event_remove(&power_events[2],token); }
static HRESULT WINAPI power_static_add_RemainingChargePercentChanged(IPowerManagerStatics *iface, IEventHandler_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&power_events[3],handler,token); }
static HRESULT WINAPI power_static_remove_RemainingChargePercentChanged(IPowerManagerStatics *iface, EventRegistrationToken token)
{ return winrt_event_remove(&power_events[3],token); }
static HRESULT WINAPI power_static_add_RemainingDischargeTimeChanged(IPowerManagerStatics *iface, IEventHandler_IInspectable *handler, EventRegistrationToken *token)
{ return winrt_event_add(&power_events[4],handler,token); }
static HRESULT WINAPI power_static_remove_RemainingDischargeTimeChanged(IPowerManagerStatics *iface, EventRegistrationToken token)
{ return winrt_event_remove(&power_events[4],token); }
static const IPowerManagerStaticsVtbl power_vtbl = { power_static_QueryInterface, power_static_AddRef, power_static_Release, power_static_GetIids, power_static_GetRuntimeClassName, power_static_GetTrustLevel,
power_static_get_EnergySaverStatus, power_static_add_EnergySaverStatusChanged, power_static_remove_EnergySaverStatusChanged,
power_static_get_BatteryStatus, power_static_add_BatteryStatusChanged, power_static_remove_BatteryStatusChanged,
power_static_get_PowerSupplyStatus, power_static_add_PowerSupplyStatusChanged, power_static_remove_PowerSupplyStatusChanged,
power_static_get_RemainingChargePercent, power_static_add_RemainingChargePercentChanged, power_static_remove_RemainingChargePercentChanged,
power_static_get_RemainingDischargeTime, power_static_add_RemainingDischargeTimeChanged, power_static_remove_RemainingDischargeTimeChanged,
};
static struct power_statics power_statics = { {&factory_vtbl}, {&power_vtbl}, 1 };
IActivationFactory *power_factory = &power_statics.IActivationFactory_iface;
