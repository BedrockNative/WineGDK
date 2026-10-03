/* Package identity for explicitly configured loose packages.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIDL_using_Windows_System
#include "private.h"
#include "appmodel.h"
#include "winreg.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(model);

struct package_identity
{
    IPackageId iface;
    LONG ref;
    PACKAGE_ID *id;
    HSTRING full_name, family_name, publisher;
};
static HRESULT WINAPI identity_qi(IPackageId *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) &&
        !IsEqualGUID(iid, &IID_IAgileObject) && !IsEqualGUID(iid, &IID_IPackageId)) return E_NOINTERFACE;
    *out = iface; IPackageId_AddRef(iface); return S_OK;
}
static ULONG WINAPI identity_addref(IPackageId *iface)
{ return InterlockedIncrement(&((struct package_identity *)iface)->ref); }
static ULONG WINAPI identity_release(IPackageId *iface)
{
    struct package_identity *impl = (void *)iface;
    ULONG ref = InterlockedDecrement(&impl->ref);
    if (!ref)
    {
        WindowsDeleteString(impl->full_name); WindowsDeleteString(impl->family_name);
        WindowsDeleteString(impl->publisher); free(impl->id); free(impl);
    }
    return ref;
}
static HRESULT WINAPI identity_iids(IPackageId *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0; *iids = CoTaskMemAlloc(sizeof(IID));
    if (!*iids) return E_OUTOFMEMORY;
    **iids = IID_IPackageId; *count = 1; return S_OK;
}
static HRESULT WINAPI identity_class(IPackageId *iface, HSTRING *out)
{ return WindowsCreateString(L"Windows.ApplicationModel.PackageId", 34, out); }
static HRESULT WINAPI identity_trust(IPackageId *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI identity_name(IPackageId *iface, HSTRING *out)
{
    const WCHAR *name = ((struct package_identity *)iface)->id->name;
    return WindowsCreateString(name, wcslen(name), out);
}
static HRESULT WINAPI identity_version(IPackageId *iface, PackageVersion *out)
{
    PACKAGE_VERSION version = ((struct package_identity *)iface)->id->version;
    if (!out) return E_POINTER;
    out->Major = version.Major; out->Minor = version.Minor;
    out->Build = version.Build; out->Revision = version.Revision; return S_OK;
}
static HRESULT WINAPI identity_arch(IPackageId *iface, ProcessorArchitecture *out)
{ if (!out) return E_POINTER; *out = ((struct package_identity *)iface)->id->processorArchitecture; return S_OK; }
static HRESULT WINAPI identity_resource(IPackageId *iface, HSTRING *out)
{
    const WCHAR *value = ((struct package_identity *)iface)->id->resourceId;
    return WindowsCreateString(value, wcslen(value), out);
}
static HRESULT WINAPI identity_publisher(IPackageId *iface, HSTRING *out)
{ return WindowsDuplicateString(((struct package_identity *)iface)->publisher, out); }
static HRESULT WINAPI identity_publisher_id(IPackageId *iface, HSTRING *out)
{
    const WCHAR *value = ((struct package_identity *)iface)->id->publisherId;
    return WindowsCreateString(value, wcslen(value), out);
}
static HRESULT WINAPI identity_full(IPackageId *iface, HSTRING *out)
{ return WindowsDuplicateString(((struct package_identity *)iface)->full_name, out); }
static HRESULT WINAPI identity_family(IPackageId *iface, HSTRING *out)
{ return WindowsDuplicateString(((struct package_identity *)iface)->family_name, out); }
static const IPackageIdVtbl identity_vtbl = {
    identity_qi, identity_addref, identity_release, identity_iids, identity_class, identity_trust,
    identity_name, identity_version, identity_arch, identity_resource, identity_publisher,
    identity_publisher_id, identity_full, identity_family
};
static HRESULT identity_string(LONG (WINAPI *getter)(UINT32 *, WCHAR *), HSTRING *out)
{
    UINT32 size = 0;
    WCHAR *buffer;
    LONG ret = getter(&size, NULL);
    HRESULT hr;
    if (ret != ERROR_INSUFFICIENT_BUFFER) return HRESULT_FROM_WIN32(ret);
    if (!(buffer = malloc(size * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    ret = getter(&size, buffer);
    hr = ret ? HRESULT_FROM_WIN32(ret) : WindowsCreateString(buffer, size - 1, out);
    free(buffer); return hr;
}
HRESULT package_identity_create(IPackageId **out)
{
    struct package_identity *impl;
    WCHAR executable[MAX_PATH], key[MAX_PATH + 40], publisher[1024], *name;
    UINT32 size = 0;
    DWORD bytes = sizeof(publisher);
    LONG ret;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    ret = GetCurrentPackageId(&size, NULL);
    if (ret != ERROR_INSUFFICIENT_BUFFER) return HRESULT_FROM_WIN32(ret);
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl = &identity_vtbl; impl->ref = 1;
    if (!(impl->id = malloc(size))) { hr = E_OUTOFMEMORY; goto failed; }
    ret = GetCurrentPackageId(&size, (BYTE *)impl->id);
    if (ret) { hr = HRESULT_FROM_WIN32(ret); goto failed; }
    if (FAILED(hr = identity_string(GetCurrentPackageFullName, &impl->full_name))) goto failed;
    if (FAILED(hr = identity_string(GetCurrentPackageFamilyName, &impl->family_name))) goto failed;
    if (impl->id->publisher)
    {
        if (FAILED(hr = WindowsCreateString(impl->id->publisher, wcslen(impl->id->publisher), &impl->publisher))) goto failed;
    }
    else if (GetModuleFileNameW(NULL, executable, ARRAY_SIZE(executable)))
    {
        name = wcsrchr(executable, '\\'); name = name ? name + 1 : executable;
        swprintf(key, ARRAY_SIZE(key), L"Software\\Wine\\AppDefaults\\%s", name);
        if (!RegGetValueW(HKEY_CURRENT_USER, key, L"PackagePublisher", RRF_RT_REG_SZ, NULL, publisher, &bytes))
            if (FAILED(hr = WindowsCreateString(publisher, wcslen(publisher), &impl->publisher))) goto failed;
    }
    TRACE("configured package identity %s\n", debugstr_hstring(impl->full_name));
    *out = &impl->iface; return S_OK;
failed:
    IPackageId_Release(&impl->iface); return hr;
}
