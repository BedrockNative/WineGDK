/* WinRT PnpObject support.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"
#include "devquery.h"
#include "aqs.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(enumeration);

static const DEV_OBJECT_TYPE object_types[] =
{
    DevObjectTypeUnknown, DevObjectTypeDeviceInterface, DevObjectTypeDeviceContainer,
    DevObjectTypeDevice, DevObjectTypeDeviceInterfaceClass, DevObjectTypeAEP,
    DevObjectTypeAEPContainer, DevObjectTypeAEPService, DevObjectTypeDevicePanel, DevObjectTypeAEPProtocol
};

struct pnp_object
{
    IPnpObject IPnpObject_iface;
    LONG ref;
    PnpObjectType type;
    IDeviceInformation *information;
};

static struct pnp_object *impl_from_IPnpObject( IPnpObject *iface )
{
    return CONTAINING_RECORD( iface, struct pnp_object, IPnpObject_iface );
}

static HRESULT get_iids( REFIID iid, ULONG *count, IID **iids )
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc( sizeof(**iids) ))) return E_OUTOFMEMORY;
    **iids = *iid;
    *count = 1;
    return S_OK;
}

static HRESULT get_class_name( HSTRING *name )
{
    const WCHAR *classname = RuntimeClass_Windows_Devices_Enumeration_Pnp_PnpObject;
    return WindowsCreateString( classname, wcslen( classname ), name );
}

static HRESULT WINAPI pnp_QueryInterface( IPnpObject *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_IPnpObject )) return E_NOINTERFACE;
    IPnpObject_AddRef( iface );
    *out = iface;
    return S_OK;
}

static ULONG WINAPI pnp_AddRef( IPnpObject *iface )
{
    return InterlockedIncrement( &impl_from_IPnpObject( iface )->ref );
}

static ULONG WINAPI pnp_Release( IPnpObject *iface )
{
    struct pnp_object *impl = impl_from_IPnpObject( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        IDeviceInformation_Release( impl->information );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI pnp_GetIids( IPnpObject *iface, ULONG *count, IID **iids )
{
    return get_iids( &IID_IPnpObject, count, iids );
}

static HRESULT WINAPI pnp_GetRuntimeClassName( IPnpObject *iface, HSTRING *name )
{
    return get_class_name( name );
}

static HRESULT WINAPI pnp_GetTrustLevel( IPnpObject *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI pnp_get_Type( IPnpObject *iface, PnpObjectType *type )
{
    if (!type) return E_POINTER;
    *type = impl_from_IPnpObject( iface )->type;
    return S_OK;
}

static HRESULT WINAPI pnp_get_Id( IPnpObject *iface, HSTRING *id )
{
    if (!id) return E_POINTER;
    return IDeviceInformation_get_Id( impl_from_IPnpObject( iface )->information, id );
}

static HRESULT WINAPI pnp_get_Properties( IPnpObject *iface, IMapView_HSTRING_IInspectable **properties )
{
    if (!properties) return E_POINTER;
    return IDeviceInformation_get_Properties( impl_from_IPnpObject( iface )->information, properties );
}

static HRESULT WINAPI pnp_Update( IPnpObject *iface, IPnpObjectUpdate *update )
{
    FIXME( "iface %p, update %p stub!\n", iface, update );
    return E_NOTIMPL;
}

static const IPnpObjectVtbl pnp_vtbl =
{
    pnp_QueryInterface, pnp_AddRef, pnp_Release, pnp_GetIids, pnp_GetRuntimeClassName,
    pnp_GetTrustLevel, pnp_get_Type, pnp_get_Id, pnp_get_Properties, pnp_Update
};

static HRESULT pnp_create( PnpObjectType type, const DEV_OBJECT *object, IPnpObject **out )
{
    struct pnp_object *impl;
    HRESULT hr;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IPnpObject_iface.lpVtbl = &pnp_vtbl;
    impl->ref = 1;
    impl->type = type;
    if (FAILED(hr = device_information_create( object, &impl->information )))
    {
        free( impl );
        return hr;
    }
    *out = &impl->IPnpObject_iface;
    return S_OK;
}

struct query_params
{
    IUnknown IUnknown_iface;
    LONG ref;
    PnpObjectType type;
    HSTRING id;
    DEVPROPCOMPKEY *keys;
    ULONG count;
    struct aqs_expr *filter;
};

static struct query_params *impl_from_IUnknown( IUnknown *iface )
{
    return CONTAINING_RECORD( iface, struct query_params, IUnknown_iface );
}

static HRESULT WINAPI params_QueryInterface( IUnknown *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown )) return E_NOINTERFACE;
    IUnknown_AddRef( iface );
    *out = iface;
    return S_OK;
}

static ULONG WINAPI params_AddRef( IUnknown *iface )
{
    return InterlockedIncrement( &impl_from_IUnknown( iface )->ref );
}

static ULONG WINAPI params_Release( IUnknown *iface )
{
    struct query_params *params = impl_from_IUnknown( iface );
    ULONG ref = InterlockedDecrement( &params->ref );
    if (!ref)
    {
        WindowsDeleteString( params->id );
        free( params->keys );
        if (params->filter) free_aqs_expr( params->filter );
        free( params );
    }
    return ref;
}

static const IUnknownVtbl params_vtbl = {params_QueryInterface, params_AddRef, params_Release};

static HRESULT params_create( PnpObjectType type, HSTRING id, IIterable_HSTRING *properties,
                              HSTRING filter, struct query_params **out )
{
    struct query_params *params;
    IIterator_HSTRING *iterator;
    boolean valid;
    HRESULT hr;

    if (type <= PnpObjectType_Unknown || type >= ARRAY_SIZE(object_types) || !properties) return E_INVALIDARG;
    if (!(params = calloc( 1, sizeof(*params) ))) return E_OUTOFMEMORY;
    params->IUnknown_iface.lpVtbl = &params_vtbl;
    params->ref = 1;
    params->type = type;
    if (FAILED(hr = WindowsDuplicateString( id, &params->id ))) goto done;
    if (filter && FAILED(hr = aqs_parse_query( WindowsGetStringRawBuffer( filter, NULL ), &params->filter, NULL ))) goto done;
    if (FAILED(hr = IIterable_HSTRING_First( properties, &iterator ))) goto done;
    for (hr = IIterator_HSTRING_get_HasCurrent( iterator, &valid ); SUCCEEDED(hr) && valid;
         hr = IIterator_HSTRING_MoveNext( iterator, &valid ))
    {
        DEVPROPCOMPKEY key = {0}, *keys;
        const WCHAR *text;
        HSTRING name;
        ULONG i;

        if (FAILED(hr = IIterator_HSTRING_get_Current( iterator, &name ))) break;
        TRACE( "requested property %s\n", debugstr_hstring( name ) );
        text = WindowsGetStringRawBuffer( name, NULL );
        hr = text[0] == '{' ? PSPropertyKeyFromString( text, (PROPERTYKEY *)&key.Key ) :
                             PSGetPropertyKeyFromName( text, (PROPERTYKEY *)&key.Key );
        WindowsDeleteString( name );
        if (FAILED(hr)) break;
        for (i = 0; i < params->count; ++i)
            if (IsEqualPropertyKey( params->keys[i].Key, key.Key )) break;
        if (i < params->count) continue;
        if (!(keys = realloc( params->keys, (params->count + 1) * sizeof(*keys) )))
        {
            hr = E_OUTOFMEMORY;
            break;
        }
        params->keys = keys;
        params->keys[params->count++] = key;
    }
    IIterator_HSTRING_Release( iterator );
done:
    if (FAILED(hr)) IUnknown_Release( &params->IUnknown_iface );
    else *out = params;
    return hr;
}

static HRESULT query_one( IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async )
{
    struct query_params *params = impl_from_IUnknown( param );
    DEV_OBJECT object = {object_types[params->type], WindowsGetStringRawBuffer( params->id, NULL )};
    IPnpObject *pnp;
    HRESULT hr;

    if (!called_async) return STATUS_PENDING;
    hr = DevGetObjectProperties( object.ObjectType, object.pszObjectId, DevQueryFlagNone,
                                params->count, params->keys, &object.cPropertyCount, &object.pProperties );
    if (FAILED(hr)) return hr;
    hr = pnp_create( params->type, &object, &pnp );
    DevFreeObjectProperties( object.cPropertyCount, object.pProperties );
    if (SUCCEEDED(hr))
    {
        result->vt = VT_UNKNOWN;
        result->punkVal = (IUnknown *)pnp;
    }
    return hr;
}

static HRESULT query_all( IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async )
{
    static const struct vector_iids iids =
    {
        &IID_IVector_IInspectable, &IID_IVectorView_PnpObject, &IID_IIterable_PnpObject, &IID_IIterator_PnpObject
    };
    struct query_params *params = impl_from_IUnknown( param );
    IVector_IInspectable *vector;
    IVectorView_IInspectable *view;
    const DEV_OBJECT *objects;
    ULONG count, i;
    HRESULT hr;

    if (!called_async) return STATUS_PENDING;
    if (FAILED(hr = DevGetObjects( object_types[params->type], DevQueryFlagNone, params->count, params->keys,
                                  params->filter ? params->filter->len : 0,
                                  params->filter ? params->filter->filters : NULL, &count, &objects ))) return hr;
    if (SUCCEEDED(hr = vector_create( &iids, (void **)&vector )))
    {
        for (i = 0; SUCCEEDED(hr) && i < count; ++i)
        {
            IPnpObject *pnp;
            if (SUCCEEDED(hr = pnp_create( params->type, objects + i, &pnp )))
            {
                hr = IVector_IInspectable_Append( vector, (IInspectable *)pnp );
                IPnpObject_Release( pnp );
            }
        }
        if (SUCCEEDED(hr)) hr = IVector_IInspectable_GetView( vector, &view );
        IVector_IInspectable_Release( vector );
    }
    DevFreeObjects( count, objects );
    if (SUCCEEDED(hr))
    {
        result->vt = VT_UNKNOWN;
        result->punkVal = (IUnknown *)view;
    }
    return hr;
}

struct pnp_statics
{
    IActivationFactory IActivationFactory_iface;
    IPnpObjectStatics IPnpObjectStatics_iface;
    LONG ref;
};

static struct pnp_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct pnp_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct pnp_statics *impl = impl_from_IActivationFactory( iface );
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IPnpObjectStatics )) *out = &impl->IPnpObjectStatics_iface;
    else if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
             IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory )) *out = iface;
    else return E_NOINTERFACE;
    IActivationFactory_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &impl_from_IActivationFactory( iface )->ref );
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &impl_from_IActivationFactory( iface )->ref );
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    return get_iids( &IID_IPnpObjectStatics, count, iids );
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    return get_class_name( name );
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    if (!level) return E_POINTER;
    *level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface, factory_AddRef, factory_Release, factory_GetIids,
    factory_GetRuntimeClassName, factory_GetTrustLevel, factory_ActivateInstance
};

DEFINE_IINSPECTABLE( statics, IPnpObjectStatics, struct pnp_statics, IActivationFactory_iface )

static HRESULT WINAPI statics_CreateFromIdAsync( IPnpObjectStatics *iface, PnpObjectType type, HSTRING id,
                                               IIterable_HSTRING *properties, IAsyncOperation_PnpObject **operation )
{
    struct query_params *params;
    HRESULT hr;

    TRACE( "type %d, id %s, properties %p\n", type, debugstr_hstring( id ), properties );
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!WindowsGetStringLen( id )) return E_INVALIDARG;
    if (FAILED(hr = params_create( type, id, properties, NULL, &params ))) return hr;
    hr = async_operation_inspectable_create( &IID_IAsyncOperation_PnpObject, (IUnknown *)iface,
                                            &params->IUnknown_iface, query_one, (IAsyncOperation_IInspectable **)operation );
    IUnknown_Release( &params->IUnknown_iface );
    return hr;
}

static HRESULT WINAPI statics_FindAllAsyncAqsFilter( IPnpObjectStatics *iface, PnpObjectType type,
                                                   IIterable_HSTRING *properties, HSTRING filter,
                                                   IAsyncOperation_PnpObjectCollection **operation )
{
    struct query_params *params;
    HRESULT hr;

    TRACE( "type %d, properties %p, filter %s\n", type, properties, debugstr_hstring( filter ) );
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (FAILED(hr = params_create( type, NULL, properties, filter, &params ))) return hr;
    hr = async_operation_inspectable_create( &IID_IAsyncOperation_PnpObjectCollection, (IUnknown *)iface,
                                            &params->IUnknown_iface, query_all, (IAsyncOperation_IInspectable **)operation );
    IUnknown_Release( &params->IUnknown_iface );
    return hr;
}

static HRESULT WINAPI statics_FindAllAsync( IPnpObjectStatics *iface, PnpObjectType type,
                                          IIterable_HSTRING *properties, IAsyncOperation_PnpObjectCollection **operation )
{
    return statics_FindAllAsyncAqsFilter( iface, type, properties, NULL, operation );
}

static HRESULT WINAPI statics_CreateWatcherAqsFilter( IPnpObjectStatics *iface, PnpObjectType type,
                                                    IIterable_HSTRING *properties, HSTRING filter, IPnpObjectWatcher **watcher )
{
    FIXME( "type %d, properties %p, filter %s stub!\n", type, properties, debugstr_hstring( filter ) );
    if (!watcher) return E_POINTER;
    *watcher = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI statics_CreateWatcher( IPnpObjectStatics *iface, PnpObjectType type,
                                           IIterable_HSTRING *properties, IPnpObjectWatcher **watcher )
{
    return statics_CreateWatcherAqsFilter( iface, type, properties, NULL, watcher );
}

static const IPnpObjectStaticsVtbl statics_vtbl =
{
    statics_QueryInterface, statics_AddRef, statics_Release, statics_GetIids,
    statics_GetRuntimeClassName, statics_GetTrustLevel, statics_CreateFromIdAsync,
    statics_FindAllAsync, statics_FindAllAsyncAqsFilter, statics_CreateWatcher, statics_CreateWatcherAqsFilter
};

static struct pnp_statics statics = {{&factory_vtbl}, {&statics_vtbl}, 1};
IActivationFactory *pnp_factory = &statics.IActivationFactory_iface;
