/* WinRT Microsoft.Windows.Storage.Pickers - FileSavePicker.FileTypeChoices
 * (IMap<HSTRING, IVector<HSTRING>>, insertion ordered)
 *
 * Copyright 2026 OrionBE contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdlib.h>

#include "private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(pickers);

struct choices_map
{
    IMap_HSTRING_IVector_HSTRING IMap_HSTRING_IVector_HSTRING_iface;
    IIterable_IKeyValuePair_HSTRING_IVector_HSTRING IIterable_IKeyValuePair_HSTRING_IVector_HSTRING_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    UINT32 size;
    UINT32 capacity;
    HSTRING *keys;
    IVector_HSTRING **values;
};

static inline struct choices_map *impl_from_IMap_HSTRING_IVector_HSTRING( IMap_HSTRING_IVector_HSTRING *iface )
{
    return CONTAINING_RECORD( iface, struct choices_map, IMap_HSTRING_IVector_HSTRING_iface );
}

static HRESULT WINAPI map_QueryInterface( IMap_HSTRING_IVector_HSTRING *iface, REFIID iid, void **out )
{
    struct choices_map *impl = impl_from_IMap_HSTRING_IVector_HSTRING( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IMap_HSTRING_IVector_HSTRING ))
    {
        *out = &impl->IMap_HSTRING_IVector_HSTRING_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_IIterable_IKeyValuePair_HSTRING_IVector_HSTRING ))
    {
        *out = &impl->IIterable_IKeyValuePair_HSTRING_IVector_HSTRING_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI map_AddRef( IMap_HSTRING_IVector_HSTRING *iface )
{
    struct choices_map *impl = impl_from_IMap_HSTRING_IVector_HSTRING( iface );
    return InterlockedIncrement( &impl->ref );
}

static void map_clear( struct choices_map *impl )
{
    UINT32 i;
    for (i = 0; i < impl->size; i++)
    {
        WindowsDeleteString( impl->keys[i] );
        if (impl->values[i]) IVector_HSTRING_Release( impl->values[i] );
    }
    impl->size = 0;
}

static ULONG WINAPI map_Release( IMap_HSTRING_IVector_HSTRING *iface )
{
    struct choices_map *impl = impl_from_IMap_HSTRING_IVector_HSTRING( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        map_clear( impl );
        free( impl->keys );
        free( impl->values );
        impl->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection( &impl->cs );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI map_GetIids( IMap_HSTRING_IVector_HSTRING *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI map_GetRuntimeClassName( IMap_HSTRING_IVector_HSTRING *iface, HSTRING *class_name )
{
    static const WCHAR name[] = L"Windows.Foundation.Collections.IMap`2<String, Windows.Foundation.Collections.IVector`1<String>>";
    return WindowsCreateString( name, wcslen( name ), class_name );
}

static HRESULT WINAPI map_GetTrustLevel( IMap_HSTRING_IVector_HSTRING *iface, TrustLevel *trust_level )
{
    *trust_level = BaseTrust;
    return S_OK;
}

static BOOL map_find( struct choices_map *impl, HSTRING key, UINT32 *index )
{
    UINT32 i;
    INT32 order;

    for (i = 0; i < impl->size; i++)
    {
        if (SUCCEEDED(WindowsCompareStringOrdinal( impl->keys[i], key, &order )) && !order)
        {
            *index = i;
            return TRUE;
        }
    }
    return FALSE;
}

static HRESULT WINAPI map_Lookup( IMap_HSTRING_IVector_HSTRING *iface, HSTRING key, IVector_HSTRING **value )
{
    struct choices_map *impl = impl_from_IMap_HSTRING_IVector_HSTRING( iface );
    HRESULT hr = E_BOUNDS;
    UINT32 index;

    TRACE( "iface %p, key %s, value %p.\n", iface, debugstr_hstring( key ), value );

    EnterCriticalSection( &impl->cs );
    if (map_find( impl, key, &index ))
    {
        if ((*value = impl->values[index])) IVector_HSTRING_AddRef( *value );
        hr = S_OK;
    }
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI map_get_Size( IMap_HSTRING_IVector_HSTRING *iface, UINT32 *size )
{
    struct choices_map *impl = impl_from_IMap_HSTRING_IVector_HSTRING( iface );
    TRACE( "iface %p, size %p.\n", iface, size );
    *size = impl->size;
    return S_OK;
}

static HRESULT WINAPI map_HasKey( IMap_HSTRING_IVector_HSTRING *iface, HSTRING key, boolean *found )
{
    struct choices_map *impl = impl_from_IMap_HSTRING_IVector_HSTRING( iface );
    UINT32 index;

    TRACE( "iface %p, key %s, found %p.\n", iface, debugstr_hstring( key ), found );

    EnterCriticalSection( &impl->cs );
    *found = map_find( impl, key, &index );
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static HRESULT WINAPI map_GetView( IMap_HSTRING_IVector_HSTRING *iface, IMapView_HSTRING_IVector_HSTRING **view )
{
    FIXME( "iface %p, view %p stub!\n", iface, view );
    return E_NOTIMPL;
}

static HRESULT WINAPI map_Insert( IMap_HSTRING_IVector_HSTRING *iface, HSTRING key, IVector_HSTRING *value, boolean *replaced )
{
    struct choices_map *impl = impl_from_IMap_HSTRING_IVector_HSTRING( iface );
    HRESULT hr = S_OK;
    UINT32 index;

    TRACE( "iface %p, key %s, value %p, replaced %p.\n", iface, debugstr_hstring( key ), value, replaced );

    EnterCriticalSection( &impl->cs );
    if (map_find( impl, key, &index ))
    {
        if (impl->values[index]) IVector_HSTRING_Release( impl->values[index] );
        if ((impl->values[index] = value)) IVector_HSTRING_AddRef( value );
        *replaced = TRUE;
    }
    else
    {
        if (impl->size == impl->capacity)
        {
            UINT32 capacity = max( 8, impl->capacity * 2 );
            HSTRING *keys = realloc( impl->keys, capacity * sizeof(*keys) );
            IVector_HSTRING **values = keys ? realloc( impl->values, capacity * sizeof(*values) ) : NULL;
            if (keys) impl->keys = keys;
            if (values) impl->values = values;
            if (!keys || !values) hr = E_OUTOFMEMORY;
            else impl->capacity = capacity;
        }
        if (SUCCEEDED(hr) && SUCCEEDED(hr = WindowsDuplicateString( key, &impl->keys[impl->size] )))
        {
            if ((impl->values[impl->size] = value)) IVector_HSTRING_AddRef( value );
            impl->size++;
        }
        *replaced = FALSE;
    }
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI map_Remove( IMap_HSTRING_IVector_HSTRING *iface, HSTRING key )
{
    struct choices_map *impl = impl_from_IMap_HSTRING_IVector_HSTRING( iface );
    HRESULT hr = E_BOUNDS;
    UINT32 index;

    TRACE( "iface %p, key %s.\n", iface, debugstr_hstring( key ) );

    EnterCriticalSection( &impl->cs );
    if (map_find( impl, key, &index ))
    {
        WindowsDeleteString( impl->keys[index] );
        if (impl->values[index]) IVector_HSTRING_Release( impl->values[index] );
        impl->size--;
        memmove( impl->keys + index, impl->keys + index + 1, (impl->size - index) * sizeof(*impl->keys) );
        memmove( impl->values + index, impl->values + index + 1, (impl->size - index) * sizeof(*impl->values) );
        hr = S_OK;
    }
    LeaveCriticalSection( &impl->cs );
    return hr;
}

static HRESULT WINAPI map_Clear( IMap_HSTRING_IVector_HSTRING *iface )
{
    struct choices_map *impl = impl_from_IMap_HSTRING_IVector_HSTRING( iface );
    TRACE( "iface %p.\n", iface );
    EnterCriticalSection( &impl->cs );
    map_clear( impl );
    LeaveCriticalSection( &impl->cs );
    return S_OK;
}

static const struct IMap_HSTRING_IVector_HSTRINGVtbl map_vtbl =
{
    map_QueryInterface,
    map_AddRef,
    map_Release,
    /* IInspectable methods */
    map_GetIids,
    map_GetRuntimeClassName,
    map_GetTrustLevel,
    /* IMap<HSTRING, IVector<HSTRING>> methods */
    map_Lookup,
    map_get_Size,
    map_HasKey,
    map_GetView,
    map_Insert,
    map_Remove,
    map_Clear,
};

DEFINE_IINSPECTABLE( map_iterable, IIterable_IKeyValuePair_HSTRING_IVector_HSTRING, struct choices_map,
                     IMap_HSTRING_IVector_HSTRING_iface )

static HRESULT WINAPI map_iterable_First( IIterable_IKeyValuePair_HSTRING_IVector_HSTRING *iface,
                                          IIterator_IKeyValuePair_HSTRING_IVector_HSTRING **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static const struct IIterable_IKeyValuePair_HSTRING_IVector_HSTRINGVtbl map_iterable_vtbl =
{
    map_iterable_QueryInterface,
    map_iterable_AddRef,
    map_iterable_Release,
    /* IInspectable methods */
    map_iterable_GetIids,
    map_iterable_GetRuntimeClassName,
    map_iterable_GetTrustLevel,
    /* IIterable<IKeyValuePair<HSTRING, IVector<HSTRING>>> methods */
    map_iterable_First,
};

HRESULT file_type_choices_create( IMap_HSTRING_IVector_HSTRING **out )
{
    struct choices_map *impl;

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IMap_HSTRING_IVector_HSTRING_iface.lpVtbl = &map_vtbl;
    impl->IIterable_IKeyValuePair_HSTRING_IVector_HSTRING_iface.lpVtbl = &map_iterable_vtbl;
    impl->ref = 1;
    InitializeCriticalSectionEx( &impl->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO );
    impl->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": choices_map.cs");
    *out = &impl->IMap_HSTRING_IVector_HSTRING_iface;
    return S_OK;
}

static BOOL append( WCHAR **buf, size_t *len, const WCHAR *str, UINT32 str_len )
{
    WCHAR *tmp;
    if (!(tmp = realloc( *buf, (*len + str_len + 1) * sizeof(WCHAR) ))) return FALSE;
    *buf = tmp;
    memcpy( *buf + *len, str, str_len * sizeof(WCHAR) );
    *len += str_len;
    (*buf)[*len] = 0;
    return TRUE;
}

/* { "Minecraft World": [".mcworld"] } -> "Minecraft World\t*.mcworld\n" */
WCHAR *file_type_choices_to_filters( IMap_HSTRING_IVector_HSTRING *iface )
{
    struct choices_map *impl = impl_from_IMap_HSTRING_IVector_HSTRING( iface );
    WCHAR *ret = NULL;
    size_t len = 0;
    UINT32 i, j;

    EnterCriticalSection( &impl->cs );
    for (i = 0; i < impl->size; i++)
    {
        const WCHAR *label;
        UINT32 label_len, count = 0;
        BOOL first = TRUE;

        label = WindowsGetStringRawBuffer( impl->keys[i], &label_len );
        if (!impl->values[i] || FAILED(IVector_HSTRING_get_Size( impl->values[i], &count )) || !count) continue;
        append( &ret, &len, label, label_len );
        append( &ret, &len, L"\t", 1 );
        for (j = 0; j < count; j++)
        {
            const WCHAR *ext;
            UINT32 ext_len;
            HSTRING str;

            if (FAILED(IVector_HSTRING_GetAt( impl->values[i], j, &str ))) continue;
            ext = WindowsGetStringRawBuffer( str, &ext_len );
            if (ext_len && *ext == '.') { ext++; ext_len--; }
            if (ext_len)
            {
                if (!first) append( &ret, &len, L";", 1 );
                append( &ret, &len, L"*.", 2 );
                append( &ret, &len, ext, ext_len );
                first = FALSE;
            }
            WindowsDeleteString( str );
        }
        append( &ret, &len, L"\n", 1 );
    }
    LeaveCriticalSection( &impl->cs );
    return ret;
}
