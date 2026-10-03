/* String properties for the web account broker. SPDX-License-Identifier: LGPL-2.1-or-later */
#include "webcore_private.h"
WINE_DEFAULT_DEBUG_CHANNEL(onlineid);
struct string_map {
    IMap_HSTRING_HSTRING iface;
    IMapView_HSTRING_HSTRING IMapView_HSTRING_HSTRING_iface;
    LONG ref;
    SRWLOCK lock;
    UINT32 count;
    struct entry { HSTRING key,value; } *entries;
};
static void map_destroy(struct string_map *impl)
{
    UINT32 i;
    for (i=0;i<impl->count;i++) { WindowsDeleteString(impl->entries[i].key); WindowsDeleteString(impl->entries[i].value); }
    free(impl->entries);
}
OBJECT_METHODS(map,IMap_HSTRING_HSTRING,struct string_map,map_destroy(impl),L"Windows.Foundation.Collections.StringMap")
static HRESULT WINAPI map_qi(IMap_HSTRING_HSTRING *iface,REFIID iid,void **out)
{
    struct string_map *impl=(void *)iface;
    if (!out) return E_POINTER; *out=NULL;
    if (BASE_QI(IMap_HSTRING_HSTRING)) *out=iface;
    else if (IsEqualGUID(iid,&IID_IMapView_HSTRING_HSTRING)) *out=&impl->IMapView_HSTRING_HSTRING_iface;
    else return E_NOINTERFACE;
    map_addref(iface); return S_OK;
}
static UINT32 find_entry(struct string_map *impl,HSTRING key)
{
    UINT32 i; INT32 order;
    for(i=0;i<impl->count;i++) if (SUCCEEDED(WindowsCompareStringOrdinal(key,impl->entries[i].key,&order)) && !order) break;
    return i;
}
static HRESULT WINAPI map_lookup(IMap_HSTRING_HSTRING *iface,HSTRING key,HSTRING *out)
{
    struct string_map *impl=(void *)iface; UINT32 i; HRESULT hr=E_BOUNDS;
    if (!out) return E_POINTER; *out=NULL;
    AcquireSRWLockShared(&impl->lock); i=find_entry(impl,key);
    if (i<impl->count) hr=WindowsDuplicateString(impl->entries[i].value,out);
    ReleaseSRWLockShared(&impl->lock);
    TRACE("property lookup %s: %#lx\n",debugstr_hstring(key),hr); return hr;
}
static HRESULT WINAPI map_size(IMap_HSTRING_HSTRING *iface,UINT32 *out)
{
    struct string_map *impl=(void *)iface; if (!out) return E_POINTER;
    AcquireSRWLockShared(&impl->lock); *out=impl->count; ReleaseSRWLockShared(&impl->lock); return S_OK;
}
static HRESULT WINAPI map_has(IMap_HSTRING_HSTRING *iface,HSTRING key,boolean *out)
{
    struct string_map *impl=(void *)iface; if (!out) return E_POINTER;
    AcquireSRWLockShared(&impl->lock); *out=find_entry(impl,key)<impl->count; ReleaseSRWLockShared(&impl->lock); TRACE("property has %s: %u\n",debugstr_hstring(key),*out); return S_OK;
}
static HRESULT WINAPI map_insert(IMap_HSTRING_HSTRING *iface,HSTRING key,HSTRING value,boolean *replaced)
{
    struct string_map *impl=(void *)iface; struct entry *entries; HSTRING k=NULL,v=NULL; HRESULT hr; UINT32 i;
    if (!replaced) return E_POINTER; *replaced=FALSE;
    if (FAILED(hr=WindowsDuplicateString(key,&k))) return hr;
    if (FAILED(hr=WindowsDuplicateString(value,&v))) { WindowsDeleteString(k); return hr; }
    AcquireSRWLockExclusive(&impl->lock); i=find_entry(impl,key);
    if (i<impl->count) { WindowsDeleteString(impl->entries[i].value); WindowsDeleteString(k); impl->entries[i].value=v; *replaced=TRUE; }
    else if (!(entries=realloc(impl->entries,(impl->count+1)*sizeof(*entries)))) { WindowsDeleteString(k); WindowsDeleteString(v); hr=E_OUTOFMEMORY; }
    else { impl->entries=entries; entries[i].key=k; entries[i].value=v; impl->count++; }
    ReleaseSRWLockExclusive(&impl->lock);
    TRACE("property insert %s: %#lx\n",debugstr_hstring(key),hr); return hr;
}
static HRESULT WINAPI map_remove(IMap_HSTRING_HSTRING *iface,HSTRING key)
{
    struct string_map *impl=(void *)iface; UINT32 i; HRESULT hr=E_BOUNDS;
    AcquireSRWLockExclusive(&impl->lock); i=find_entry(impl,key);
    if(i<impl->count) { WindowsDeleteString(impl->entries[i].key); WindowsDeleteString(impl->entries[i].value); impl->count--; memmove(impl->entries+i,impl->entries+i+1,(impl->count-i)*sizeof(*impl->entries)); hr=S_OK; }
    ReleaseSRWLockExclusive(&impl->lock); return hr;
}
static HRESULT WINAPI map_clear(IMap_HSTRING_HSTRING *iface)
{
    struct string_map *impl=(void *)iface;
    AcquireSRWLockExclusive(&impl->lock); map_destroy(impl); impl->entries=NULL; impl->count=0; ReleaseSRWLockExclusive(&impl->lock); return S_OK;
}
static HRESULT WINAPI map_view(IMap_HSTRING_HSTRING *iface,IMapView_HSTRING_HSTRING **out)
{
    struct string_map *impl=(void *)iface; IMap_HSTRING_HSTRING *copy; HRESULT hr; UINT32 i; boolean replaced;
    if (!out) return E_POINTER; *out=NULL;
    if (FAILED(hr=string_map_create(&copy))) return hr;
    AcquireSRWLockShared(&impl->lock);
    for(i=0;i<impl->count && SUCCEEDED(hr);i++) hr=map_insert(copy,impl->entries[i].key,impl->entries[i].value,&replaced);
    ReleaseSRWLockShared(&impl->lock);
    if (SUCCEEDED(hr)) hr=map_qi(copy,&IID_IMapView_HSTRING_HSTRING,(void **)out);
    map_release(copy); return hr;
}
static const IMap_HSTRING_HSTRINGVtbl map_vtbl={OBJECT_VTBL(map),map_lookup,map_size,map_has,map_view,map_insert,map_remove,map_clear};
DEFINE_IINSPECTABLE(view,IMapView_HSTRING_HSTRING,struct string_map,iface)
static HRESULT WINAPI view_lookup(IMapView_HSTRING_HSTRING *iface,HSTRING key,HSTRING *out) { return map_lookup(&impl_from_IMapView_HSTRING_HSTRING(iface)->iface,key,out); }
static HRESULT WINAPI view_size(IMapView_HSTRING_HSTRING *iface,UINT32 *out) { return map_size(&impl_from_IMapView_HSTRING_HSTRING(iface)->iface,out); }
static HRESULT WINAPI view_has(IMapView_HSTRING_HSTRING *iface,HSTRING key,boolean *out) { return map_has(&impl_from_IMapView_HSTRING_HSTRING(iface)->iface,key,out); }
static HRESULT WINAPI view_split(IMapView_HSTRING_HSTRING *iface,IMapView_HSTRING_HSTRING **first,IMapView_HSTRING_HSTRING **second)
{ if (!first || !second) return E_POINTER; *first=*second=NULL; return E_NOTIMPL; }
static const IMapView_HSTRING_HSTRINGVtbl view_vtbl={view_QueryInterface,view_AddRef,view_Release,view_GetIids,view_GetRuntimeClassName,view_GetTrustLevel,view_lookup,view_size,view_has,view_split};
HRESULT string_map_create(IMap_HSTRING_HSTRING **out)
{
    struct string_map *impl; if (!out) return E_POINTER; *out=NULL;
    if (!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl=&map_vtbl; impl->IMapView_HSTRING_HSTRING_iface.lpVtbl=&view_vtbl; impl->ref=1; *out=&impl->iface; return S_OK;
}
