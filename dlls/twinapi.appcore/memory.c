/* Desktop-host memory accounting. LGPL-2.1-or-later. */
#include "private.h"
#include "psapi.h"
WINE_DEFAULT_DEBUG_CHANNEL(twinapi);
struct memory_statics
{
    IActivationFactory IActivationFactory_iface;
    IMemoryManagerStatics IMemoryManagerStatics_iface;
    LONG ref;
};

static inline struct memory_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct memory_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct memory_statics *impl = impl_from_IActivationFactory( iface );

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
    else if (IsEqualGUID( iid, &IID_IMemoryManagerStatics ))
    {
        *out = &impl->IMemoryManagerStatics_iface;
        IMemoryManagerStatics_AddRef( &impl->IMemoryManagerStatics_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct memory_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct memory_statics *impl = impl_from_IActivationFactory( iface );
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

DEFINE_IINSPECTABLE( memory_static, IMemoryManagerStatics, struct memory_statics, IActivationFactory_iface )

static HRESULT WINAPI memory_static_get_AppMemoryUsage(IMemoryManagerStatics *iface, UINT64 *value)
{
    PROCESS_MEMORY_COUNTERS counters;
    if (!value) return E_POINTER;
    *value=0;
    if (!GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&counters,sizeof(counters))) return HRESULT_FROM_WIN32(GetLastError());
    *value=counters.PagefileUsage; return S_OK;
}
static HRESULT WINAPI memory_static_get_AppMemoryUsageLimit(IMemoryManagerStatics *iface, UINT64 *value)
{
    MEMORYSTATUSEX status={sizeof(status)};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job;
    if (!value) return E_POINTER;
    *value=0;
    if (!GlobalMemoryStatusEx(&status)) return HRESULT_FROM_WIN32(GetLastError());
    /* Loose desktop app: no UWP resource-manager budget. Respect available
     * system commit capacity, address space and any explicit process job cap. */
    *value=min(status.ullTotalPageFile,status.ullTotalVirtual);
    if (QueryInformationJobObject(NULL,JobObjectExtendedLimitInformation,&job,sizeof(job),NULL) &&
        (job.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_PROCESS_MEMORY)) *value=min(*value,job.ProcessMemoryLimit);
    return S_OK;
}
static HRESULT WINAPI memory_static_get_AppMemoryUsageLevel(IMemoryManagerStatics *iface, AppMemoryUsageLevel *value)
{
    if (!value) return E_POINTER;
    /* The desktop host does not expose UWP pressure-level thresholds. */
    return E_NOTIMPL;
}
static HRESULT WINAPI memory_static_add_AppMemoryUsageIncreased(IMemoryManagerStatics *iface, IEventHandler_IInspectable *handler, EventRegistrationToken *token)
{ if (!handler || !token) return E_POINTER; token->value=0; return E_NOTIMPL; }
static HRESULT WINAPI memory_static_remove_AppMemoryUsageIncreased(IMemoryManagerStatics *iface, EventRegistrationToken token)
{ return E_NOTIMPL; }
static HRESULT WINAPI memory_static_add_AppMemoryUsageDecreased(IMemoryManagerStatics *iface, IEventHandler_IInspectable *handler, EventRegistrationToken *token)
{ if (!handler || !token) return E_POINTER; token->value=0; return E_NOTIMPL; }
static HRESULT WINAPI memory_static_remove_AppMemoryUsageDecreased(IMemoryManagerStatics *iface, EventRegistrationToken token)
{ return E_NOTIMPL; }
static HRESULT WINAPI memory_static_add_AppMemoryUsageLimitChanging(IMemoryManagerStatics *iface, IEventHandler_AppMemoryUsageLimitChangingEventArgs *handler, EventRegistrationToken *token)
{ if (!handler || !token) return E_POINTER; token->value=0; return E_NOTIMPL; }
static HRESULT WINAPI memory_static_remove_AppMemoryUsageLimitChanging(IMemoryManagerStatics *iface, EventRegistrationToken token)
{ return E_NOTIMPL; }

static const struct IMemoryManagerStaticsVtbl memory_static_vtbl =
{
    memory_static_QueryInterface,
    memory_static_AddRef,
    memory_static_Release,
    /* IInspectable methods */
    memory_static_GetIids,
    memory_static_GetRuntimeClassName,
    memory_static_GetTrustLevel,
    /* IMemoryManagerStatics methods */
    memory_static_get_AppMemoryUsage,
    memory_static_get_AppMemoryUsageLimit,
    memory_static_get_AppMemoryUsageLevel,
    memory_static_add_AppMemoryUsageIncreased,
    memory_static_remove_AppMemoryUsageIncreased,
    memory_static_add_AppMemoryUsageDecreased,
    memory_static_remove_AppMemoryUsageDecreased,
    memory_static_add_AppMemoryUsageLimitChanging,
    memory_static_remove_AppMemoryUsageLimitChanging
};

static struct memory_statics memory_statics =
{
    {&factory_vtbl},
    {&memory_static_vtbl},
    1,
};

IActivationFactory *memory_factory = &memory_statics.IActivationFactory_iface;
