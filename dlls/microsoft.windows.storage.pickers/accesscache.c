/* Persistent local-file WinRT future-access lists.
 * Copyright 2026 WineGDK contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "private.h"
#define WIDL_using_Windows_Storage
#include "windows.storage.h"
#include "initguid.h"
#define WIDL_using_Windows_Storage_AccessCache
#include "windows.storage.accesscache.h"
#include "appmodel.h"
#include "winreg.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(pickers);

struct access_list { IStorageItemAccessList IStorageItemAccessList_iface; LONG ref; HKEY paths, metadata; };
static SRWLOCK cache_lock = SRWLOCK_INIT;
static struct access_list *list_impl(IStorageItemAccessList *iface)
{ return CONTAINING_RECORD(iface,struct access_list,IStorageItemAccessList_iface); }
static HRESULT WINAPI list_QueryInterface(IStorageItemAccessList *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown) && !IsEqualGUID(iid,&IID_IInspectable) &&
        !IsEqualGUID(iid,&IID_IAgileObject) && !IsEqualGUID(iid,&IID_IStorageItemAccessList)) return E_NOINTERFACE;
    *out = iface; IStorageItemAccessList_AddRef(iface); return S_OK;
}
static ULONG WINAPI list_AddRef(IStorageItemAccessList *iface)
{ return InterlockedIncrement(&list_impl(iface)->ref); }
static ULONG WINAPI list_Release(IStorageItemAccessList *iface)
{
    struct access_list *impl = list_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { RegCloseKey(impl->paths); RegCloseKey(impl->metadata); free(impl); }
    return ref;
}
static HRESULT WINAPI list_GetIids(IStorageItemAccessList *iface, ULONG *count, IID **out)
{
    if (!count || !out) return E_POINTER;
    *count = 0;
    if (!(*out = CoTaskMemAlloc(sizeof(**out)))) return E_OUTOFMEMORY;
    **out = IID_IStorageItemAccessList; *count = 1; return S_OK;
}
static HRESULT WINAPI list_GetRuntimeClassName(IStorageItemAccessList *iface, HSTRING *out)
{
    static const WCHAR name[] = L"Windows.Storage.AccessCache.StorageItemAccessList";
    return WindowsCreateString(name,ARRAY_SIZE(name)-1,out);
}
static HRESULT WINAPI list_GetTrustLevel(IStorageItemAccessList *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static BOOL valid_token(HSTRING token)
{
    UINT32 length;
    const WCHAR *str = WindowsGetStringRawBuffer(token,&length);
    return length && length <= 384 && wcslen(str) == length;
}
static HRESULT WINAPI list_AddOrReplace(IStorageItemAccessList *iface, HSTRING token, IStorageItem *item, HSTRING metadata)
{
    struct access_list *impl = list_impl(iface);
    HSTRING path;
    DWORD count, type;
    LONG ret;
    HRESULT hr;
    if (!item || !valid_token(token) || WindowsGetStringLen(metadata) > 32768) return E_INVALIDARG;
    if (FAILED(hr = IStorageItem_get_Path(item,&path))) return hr;
    if (!WindowsGetStringLen(path)) { WindowsDeleteString(path); return E_NOTIMPL; }
    AcquireSRWLockExclusive(&cache_lock);
    ret = RegQueryValueExW(impl->paths,WindowsGetStringRawBuffer(token,NULL),NULL,&type,NULL,NULL);
    if (ret == ERROR_FILE_NOT_FOUND)
    {
        ret = RegQueryInfoKeyW(impl->paths,NULL,NULL,NULL,NULL,NULL,NULL,&count,NULL,NULL,NULL,NULL);
        if (!ret && count >= 1000) ret = ERROR_TOO_MANY_NAMES;
    }
    if (!ret) ret = RegSetValueExW(impl->metadata,WindowsGetStringRawBuffer(token,NULL),0,REG_SZ,
        (const BYTE *)WindowsGetStringRawBuffer(metadata,NULL),(WindowsGetStringLen(metadata)+1)*sizeof(WCHAR));
    if (!ret) ret = RegSetValueExW(impl->paths,WindowsGetStringRawBuffer(token,NULL),0,REG_SZ,
        (const BYTE *)WindowsGetStringRawBuffer(path,NULL),(WindowsGetStringLen(path)+1)*sizeof(WCHAR));
    ReleaseSRWLockExclusive(&cache_lock);
    TRACE("cached token %s -> %s, status %ld.\n",debugstr_hstring(token),debugstr_hstring(path),ret);
    WindowsDeleteString(path);
    return HRESULT_FROM_WIN32(ret);
}
static HRESULT WINAPI list_AddOrReplaceOverloadDefaultMetadata(IStorageItemAccessList *iface, HSTRING token, IStorageItem *item)
{ return list_AddOrReplace(iface,token,item,NULL); }
static HRESULT WINAPI list_Add(IStorageItemAccessList *iface, IStorageItem *item, HSTRING metadata, HSTRING *token)
{
    GUID id;
    WCHAR text[40];
    HRESULT hr;
    if (!token) return E_POINTER;
    *token = NULL;
    if (!item) return E_INVALIDARG;
    if (FAILED(hr = CoCreateGuid(&id))) return hr;
    StringFromGUID2(&id,text,ARRAY_SIZE(text));
    if (FAILED(hr = WindowsCreateString(text,wcslen(text),token))) return hr;
    if (FAILED(hr = list_AddOrReplace(iface,*token,item,metadata))) { WindowsDeleteString(*token); *token = NULL; }
    return hr;
}
static HRESULT WINAPI list_AddOverloadDefaultMetadata(IStorageItemAccessList *iface, IStorageItem *item, HSTRING *token)
{ return list_Add(iface,item,NULL,token); }
static HRESULT WINAPI list_Remove(IStorageItemAccessList *iface, HSTRING token)
{
    struct access_list *impl = list_impl(iface);
    LONG ret;
    if (!valid_token(token)) return E_INVALIDARG;
    AcquireSRWLockExclusive(&cache_lock);
    ret = RegDeleteValueW(impl->paths,WindowsGetStringRawBuffer(token,NULL));
    if (!ret) RegDeleteValueW(impl->metadata,WindowsGetStringRawBuffer(token,NULL));
    ReleaseSRWLockExclusive(&cache_lock);
    return HRESULT_FROM_WIN32(ret);
}
static HRESULT WINAPI list_ContainsItem(IStorageItemAccessList *iface, HSTRING token, boolean *out)
{
    LONG ret;
    if (!out) return E_POINTER;
    *out = FALSE;
    if (!valid_token(token)) return E_INVALIDARG;
    ret = RegQueryValueExW(list_impl(iface)->paths,WindowsGetStringRawBuffer(token,NULL),NULL,NULL,NULL,NULL);
    if (!ret) *out = TRUE;
    return ret == ERROR_FILE_NOT_FOUND ? S_OK : HRESULT_FROM_WIN32(ret);
}
static HRESULT WINAPI list_Clear(IStorageItemAccessList *iface)
{
    struct access_list *impl = list_impl(iface);
    LONG ret;
    AcquireSRWLockExclusive(&cache_lock);
    ret = RegDeleteTreeW(impl->paths,NULL);
    if (!ret) ret = RegDeleteTreeW(impl->metadata,NULL);
    ReleaseSRWLockExclusive(&cache_lock);
    return HRESULT_FROM_WIN32(ret);
}
static HRESULT WINAPI list_CheckAccess(IStorageItemAccessList *iface, IStorageItem *item, boolean *out)
{
    HSTRING path;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = FALSE;
    if (!item) return E_INVALIDARG;
    if (FAILED(hr = IStorageItem_get_Path(item,&path))) return hr;
    *out = WindowsGetStringLen(path) && GetFileAttributesW(WindowsGetStringRawBuffer(path,NULL)) != INVALID_FILE_ATTRIBUTES;
    WindowsDeleteString(path);
    return S_OK;
}
static HRESULT WINAPI list_get_MaximumItemsAllowed(IStorageItemAccessList *iface, UINT32 *out)
{ if (!out) return E_POINTER; *out = 1000; return S_OK; }
static HRESULT WINAPI list_get_Entries(IStorageItemAccessList *iface, IVectorView_AccessListEntry **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }

struct lookup { IUnknown IUnknown_iface; LONG ref; WCHAR *path; unsigned kind; HRESULT hr; };
static struct lookup *lookup_impl(IUnknown *iface) { return CONTAINING_RECORD(iface,struct lookup,IUnknown_iface); }
static HRESULT WINAPI lookup_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER; *out = NULL;
    if (!IsEqualGUID(iid,&IID_IUnknown)) return E_NOINTERFACE;
    *out = iface; IUnknown_AddRef(iface); return S_OK;
}
static ULONG WINAPI lookup_AddRef(IUnknown *iface) { return InterlockedIncrement(&lookup_impl(iface)->ref); }
static ULONG WINAPI lookup_Release(IUnknown *iface)
{
    struct lookup *impl = lookup_impl(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref) { free(impl->path); free(impl); } return ref;
}
static const IUnknownVtbl lookup_vtbl = {lookup_QueryInterface,lookup_AddRef,lookup_Release};
typedef HRESULT (WINAPI *create_folder_fn)(const WCHAR *, IUnknown **);
static create_folder_fn create_folder;
static INIT_ONCE folder_once = INIT_ONCE_STATIC_INIT;
static BOOL WINAPI load_folder(INIT_ONCE *once, void *param, void **context)
{
    HMODULE module = LoadLibraryW(L"windows.storage.dll");
    if (module) create_folder = (void *)GetProcAddress(module,"__wine_create_storage_folder");
    return TRUE; /* Retain the module for the returned COM objects. */
}
static HRESULT lookup_async(IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async)
{
    struct lookup *request = lookup_impl(param);
    IUnknown *object;
    DWORD attrs;
    HRESULT hr;
    if (!called_async) return STATUS_PENDING;
    if (FAILED(request->hr)) return request->hr;
    attrs = GetFileAttributesW(request->path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return HRESULT_FROM_WIN32(GetLastError());
    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
    {
        if (request->kind == 1) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        InitOnceExecuteOnce(&folder_once,load_folder,NULL,NULL);
        hr = create_folder ? create_folder(request->path,&object) : E_NOTIMPL;
    }
    else if (request->kind == 2) return HRESULT_FROM_WIN32(ERROR_DIRECTORY);
    else hr = storage_file_create_object(request->path,&object);
    if (FAILED(hr)) return hr;
    if (!request->kind)
    {
        hr = IUnknown_QueryInterface(object,&IID_IStorageItem,(void **)&result->punkVal);
        IUnknown_Release(object);
    }
    else result->punkVal = object;
    if (SUCCEEDED(hr)) result->vt = VT_UNKNOWN;
    return hr;
}
static HRESULT lookup_create(IStorageItemAccessList *iface, HSTRING token, AccessCacheOptions options,
        unsigned kind, const GUID *iid, IAsyncOperation_IInspectable **out)
{
    struct lookup *request;
    DWORD size = 0;
    LONG ret;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!valid_token(token) || ((unsigned)options & ~15u)) return E_INVALIDARG;
    if (!(request = calloc(1,sizeof(*request)))) return E_OUTOFMEMORY;
    request->IUnknown_iface.lpVtbl = &lookup_vtbl; request->ref = 1; request->kind = kind;
    AcquireSRWLockShared(&cache_lock);
    ret = RegGetValueW(list_impl(iface)->paths,NULL,WindowsGetStringRawBuffer(token,NULL),RRF_RT_REG_SZ,NULL,NULL,&size);
    if (!ret && !(request->path = malloc(size))) ret = ERROR_NOT_ENOUGH_MEMORY;
    if (!ret) ret = RegGetValueW(list_impl(iface)->paths,NULL,WindowsGetStringRawBuffer(token,NULL),RRF_RT_REG_SZ,NULL,request->path,&size);
    ReleaseSRWLockShared(&cache_lock);
    request->hr = HRESULT_FROM_WIN32(ret);
    hr = async_operation_inspectable_create(iid,(IUnknown *)iface,&request->IUnknown_iface,lookup_async,out);
    IUnknown_Release(&request->IUnknown_iface);
    return hr;
}
#define GET_METHOD(name,type,kind) \
static HRESULT WINAPI list_Get##name##WithOptionsAsync(IStorageItemAccessList *iface,HSTRING token,AccessCacheOptions options,type **out) \
{ return lookup_create(iface,token,options,kind,&IID_##type,(IAsyncOperation_IInspectable **)out); } \
static HRESULT WINAPI list_Get##name##Async(IStorageItemAccessList *iface,HSTRING token,type **out) \
{ return list_Get##name##WithOptionsAsync(iface,token,AccessCacheOptions_None,out); }
GET_METHOD(Item,IAsyncOperation_IStorageItem,0)
GET_METHOD(File,IAsyncOperation_StorageFile,1)
GET_METHOD(Folder,IAsyncOperation_StorageFolder,2)
#undef GET_METHOD
static const IStorageItemAccessListVtbl list_vtbl = {
    list_QueryInterface,list_AddRef,list_Release,list_GetIids,list_GetRuntimeClassName,list_GetTrustLevel,
    list_AddOverloadDefaultMetadata,list_Add,list_AddOrReplaceOverloadDefaultMetadata,list_AddOrReplace,
    list_GetItemAsync,list_GetFileAsync,list_GetFolderAsync,list_GetItemWithOptionsAsync,list_GetFileWithOptionsAsync,
    list_GetFolderWithOptionsAsync,list_Remove,list_ContainsItem,list_Clear,list_CheckAccess,list_get_Entries,list_get_MaximumItemsAllowed
};
static HRESULT create_list(IStorageItemAccessList **out)
{
    struct access_list *impl;
    WCHAR family[256], executable[32768], key_name[384];
    UINT32 length = ARRAY_SIZE(family);
    UINT64 hash = 14695981039346656037ULL;
    HKEY root;
    LONG ret;
    unsigned i;
    if (!out) return E_POINTER;
    *out = NULL;
    ret = GetCurrentPackageFamilyName(&length,family);
    if (ret == APPMODEL_ERROR_NO_PACKAGE)
    {
        if (!GetModuleFileNameW(NULL,executable,ARRAY_SIZE(executable))) return HRESULT_FROM_WIN32(GetLastError());
        for (i = 0; executable[i]; ++i) { hash ^= towlower(executable[i]); hash *= 1099511628211ULL; }
        swprintf(family,ARRAY_SIZE(family),L"Unpackaged-%016I64x",hash);
    }
    else if (ret) return HRESULT_FROM_WIN32(ret);
    swprintf(key_name,ARRAY_SIZE(key_name),L"Software\\Wine\\WinRT\\StorageAccessCache\\%s\\FutureAccessList",family);
    if (!(impl = calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IStorageItemAccessList_iface.lpVtbl = &list_vtbl; impl->ref = 1;
    ret = RegCreateKeyExW(HKEY_CURRENT_USER,key_name,0,NULL,0,KEY_READ|KEY_WRITE|KEY_WOW64_64KEY,NULL,&root,NULL);
    if (!ret)
    {
        ret = RegCreateKeyExW(root,L"Paths",0,NULL,0,KEY_READ|KEY_WRITE,NULL,&impl->paths,NULL);
        if (!ret) ret = RegCreateKeyExW(root,L"Metadata",0,NULL,0,KEY_READ|KEY_WRITE,NULL,&impl->metadata,NULL);
        RegCloseKey(root);
    }
    if (ret) { IStorageItemAccessList_Release(&impl->IStorageItemAccessList_iface); return HRESULT_FROM_WIN32(ret); }
    *out = &impl->IStorageItemAccessList_iface;
    return S_OK;
}
struct permissions_factory { IActivationFactory IActivationFactory_iface; IStorageApplicationPermissionsStatics statics; LONG ref; };
static struct permissions_factory *factory_impl(IActivationFactory *iface)
{ return CONTAINING_RECORD(iface,struct permissions_factory,IActivationFactory_iface); }
static HRESULT WINAPI factory_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER; *out = NULL;
    if (IsEqualGUID(iid,&IID_IUnknown) || IsEqualGUID(iid,&IID_IInspectable) || IsEqualGUID(iid,&IID_IAgileObject) ||
        IsEqualGUID(iid,&IID_IActivationFactory)) *out = iface;
    else if (IsEqualGUID(iid,&IID_IStorageApplicationPermissionsStatics)) *out = &factory_impl(iface)->statics;
    else return E_NOINTERFACE;
    IUnknown_AddRef((IUnknown *)*out); return S_OK;
}
static ULONG WINAPI factory_AddRef(IActivationFactory *iface) { return InterlockedIncrement(&factory_impl(iface)->ref); }
static ULONG WINAPI factory_Release(IActivationFactory *iface) { return InterlockedDecrement(&factory_impl(iface)->ref); }
static HRESULT WINAPI factory_GetIids(IActivationFactory *iface, ULONG *count, IID **out)
{
    if (!count || !out) return E_POINTER; *count = 0;
    if (!(*out = CoTaskMemAlloc(sizeof(**out)))) return E_OUTOFMEMORY;
    **out = IID_IStorageApplicationPermissionsStatics; *count = 1; return S_OK;
}
static HRESULT WINAPI factory_GetRuntimeClassName(IActivationFactory *iface, HSTRING *out)
{
    static const WCHAR name[] = L"Windows.Storage.AccessCache.StorageApplicationPermissions";
    return WindowsCreateString(name,ARRAY_SIZE(name)-1,out);
}
static HRESULT WINAPI factory_GetTrustLevel(IActivationFactory *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI factory_ActivateInstance(IActivationFactory *iface, IInspectable **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IActivationFactoryVtbl factory_vtbl = {factory_QueryInterface,factory_AddRef,factory_Release,
    factory_GetIids,factory_GetRuntimeClassName,factory_GetTrustLevel,factory_ActivateInstance};
DEFINE_IINSPECTABLE_(statics,IStorageApplicationPermissionsStatics,struct permissions_factory,statics_impl,statics,&impl->IActivationFactory_iface)
static HRESULT WINAPI statics_get_FutureAccessList(IStorageApplicationPermissionsStatics *iface, IStorageItemAccessList **out)
{ return create_list(out); }
static HRESULT WINAPI statics_get_MostRecentlyUsedList(IStorageApplicationPermissionsStatics *iface, __x_ABI_CWindows_CStorage_CAccessCache_CIStorageItemMostRecentlyUsedList **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static const IStorageApplicationPermissionsStaticsVtbl statics_vtbl = {statics_QueryInterface,statics_AddRef,statics_Release,
    statics_GetIids,statics_GetRuntimeClassName,statics_GetTrustLevel,statics_get_FutureAccessList,statics_get_MostRecentlyUsedList};
static struct permissions_factory factory = {{&factory_vtbl},{&statics_vtbl},1};
IActivationFactory *storage_permissions_factory = &factory.IActivationFactory_iface;
