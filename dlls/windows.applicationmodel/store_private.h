/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef WINE_APPLICATIONMODEL_STORE_PRIVATE_H
#define WINE_APPLICATIONMODEL_STORE_PRIVATE_H
#include "private.h"
#define WIDL_using_Wine_Internal
#include "store_async_private.h"
#define DEFINE_IINSPECTABLE_OUTER(pfx,iface_type,impl_type,outer_iface) \
    DEFINE_IINSPECTABLE_(pfx,iface_type,impl_type,impl_from_##iface_type,iface_type##_iface,impl->outer_iface)
enum store_field {
    STORE_STATUS, STORE_APP_ID, STORE_STORE_ID, STORE_PFN, STORE_RECEIPT,
    STORE_ACTIVE, STORE_TRIAL, STORE_EXPIRATION, STORE_NAME, STORE_DESCRIPTION,
    STORE_PRICE, STORE_CURRENCY, STORE_AGE, STORE_MARKET, STORE_FIELD_COUNT
};
struct store_info { HSTRING fields[STORE_FIELD_COUNT]; };
HRESULT store_request(const WCHAR *operation, struct store_info *info);
void store_info_clear(struct store_info *info);
HRESULT async_operation_extension_create(const GUID *, IUnknown *, IUnknown *, async_operation_callback, IAsyncOperation_ExtendedExecutionResult **);
HRESULT async_operation_hstring_create(IUnknown *, IUnknown *, async_operation_callback, IAsyncOperation_HSTRING **);
HRESULT async_operation_inspectable_create(const GUID *, IUnknown *, IUnknown *, async_operation_callback, IAsyncOperation_IInspectable **);
#endif
