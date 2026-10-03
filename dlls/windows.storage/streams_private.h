#ifndef __WINE_STORAGE_STREAMS_PRIVATE_H
#define __WINE_STORAGE_STREAMS_PRIVATE_H
#include "private.h"
#define WIDL_using_Wine_Internal
#include "streams_async_private.h"
extern HRESULT async_operation_uint32_create(IUnknown *, IUnknown *, streams_operation_callback, IAsyncOperation_UINT32 **);
#endif
