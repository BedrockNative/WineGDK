/* Diagnostics object helpers. SPDX-License-Identifier: LGPL-2.1-or-later */
#include "private.h"
#define OBJECT_METHODS(prefix,type,structure,destroy,class_name) \
static ULONG WINAPI prefix##_addref(type *iface) { return InterlockedIncrement(&((structure *)iface)->ref); } \
static ULONG WINAPI prefix##_release(type *iface) { structure *impl=(structure *)iface; ULONG ref=InterlockedDecrement(&impl->ref); if (!ref) { destroy; free(impl); } return ref; } \
static HRESULT WINAPI prefix##_iids(type *iface,ULONG *count,IID **iids) { if (!count || !iids) return E_POINTER; *count=0; *iids=CoTaskMemAlloc(sizeof(IID)); if (!*iids) return E_OUTOFMEMORY; **iids=IID_##type; *count=1; return S_OK; } \
static HRESULT WINAPI prefix##_class(type *iface,HSTRING *out) { return WindowsCreateString(class_name,wcslen(class_name),out); } \
static HRESULT WINAPI prefix##_trust(type *iface,TrustLevel *out) { if (!out) return E_POINTER; *out=BaseTrust; return S_OK; }
#define OBJECT_VTBL(p) p##_qi,p##_addref,p##_release,p##_iids,p##_class,p##_trust
#define BASE_QI(type) (IsEqualGUID(iid,&IID_##type) || IsEqualGUID(iid,&IID_IUnknown) || IsEqualGUID(iid,&IID_IInspectable) || IsEqualGUID(iid,&IID_IAgileObject))
static inline HRESULT string_from_wide(const WCHAR *str,HSTRING *out) { return WindowsCreateString(str,wcslen(str),out); }
