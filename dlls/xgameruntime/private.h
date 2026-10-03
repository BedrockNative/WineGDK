/*
 * Xbox Game runtime Library
 * 
 * Written by Weather
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

#ifndef __WINE_XGAMERUNTIME_PRIVATE_H
#define __WINE_XGAMERUNTIME_PRIVATE_H

#define COBJMACROS
#include <stdlib.h>
#include <windows.h>
#include <windef.h>
#include <winstring.h>
#include <roapi.h>
#include <activation.h>
#include <assert.h>
#include <unknwn.h>

#ifdef __cplusplus
// Bug: WinRT in C++ within Wine lacks proper C++ type handling
// Redefine boolean as bool, and DOUBLE as double to prevent compile issues.
// Casting is purely handled by libstdc++, so it's not an issue here.
#undef boolean
#define boolean bool
#undef DOUBLE
#define DOUBLE double

// Bug: __WINESRC__ is not defined in C++ contexts.
#define __WINESRC__ 1
#include <cstdint>
#endif

#define WIDL_EXPLICIT_AGGREGATE_RETURNS

#include <xgameerr.h>
#include <xsystem.h>
#include <xgame.h>
#include <xlauncher.h>
#include <xgameruntimefeature.h>
#include <xnetworking.h>
#include <xuser.h>
#include <xasync.h>
#include <xasyncprovider.h>
#include <xlauncher.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "wine/unixlib.h"
#ifdef __cplusplus
}
#endif
#include "wine/debug.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_Data_Json
#include "windows.data.json.h"
#define WIDL_using_Windows_Globalization
#include "windows.globalization.h"
#define WIDL_using_Windows_System_Profile
#include "windows.system.profile.h"
#define WIDL_using_Windows_Data_Json
#include "windows.data.json.h"
#define WIDL_using_Xodus
#include "xodusprovider.h"

#include "userprovider.h"

#define RETURN_HR(hr)                                           TRACE("Returning HR %#lx\n", hr); return(hr)
#define RETURN_LAST_ERROR()                                     return HRESULT_FROM_WIN32(GetLastError())
#define RETURN_WIN32(win32err)                                  return HRESULT_FROM_WIN32(win32err)

#define RETURN_IF_FAILED(hr)                                    do { HRESULT __hrRet = hr; if (FAILED(__hrRet)) { RETURN_HR(__hrRet); }} while (0)
#define RETURN_IF_WIN32_BOOL_FALSE(win32BOOL)                   do { BOOL __boolRet = win32BOOL; if (!__boolRet) { RETURN_LAST_ERROR(); }} while (0)
#define RETURN_IF_NULL_ALLOC(ptr)                               do { if ((ptr) == nullptr) { RETURN_HR(E_OUTOFMEMORY); }} while (0)
#define RETURN_HR_IF(hr, condition)                             do { if (condition) { RETURN_HR(hr); }} while (0)
#define RETURN_HR_IF_FALSE(hr, condition)                       do { if (!(condition)) { RETURN_HR(hr); }} while (0)
#define RETURN_LAST_ERROR_IF(condition)                         do { if (condition) { RETURN_LAST_ERROR(); }} while (0)
#define RETURN_LAST_ERROR_IF_NULL(ptr)                          do { if ((ptr) == nullptr) { RETURN_LAST_ERROR(); }} while (0)

#define LOG_IF_FAILED(hr)                                       do { HRESULT __hrRet = hr; if (FAILED(__hrRet)) { TRACE("libHttpClient error %s: 0x%#lx", #hr, __hrRet); }} while (0)

#define FAIL_FAST_MSG(fmt, ...)                        \
    TRACE(fmt, ##__VA_ARGS__);                         \
    assert(false);                                     \

#define FAIL_FAST_IF_FAILED(hr)                                 do { HRESULT __hrRet = hr; if (FAILED(__hrRet)) { FAIL_FAST_MSG("%s 0x%#lx", #hr, __hrRet); }} while (0)

#define POLL_BUFFER_SIZE 0x10008 /* UINT16 payload plus IPC header */
#define XODUS_SOCKET_SUFFIX "xodus.sock"
/* OrionBE: 5 s was shorter than a slow DNS lookup (login.live.com) and made the game crash; allow up to 60 s. */
#define IPC_REQUEST_TIMEOUT_MS 60000
#define XODUS_INTEROP 1
#endif

extern BOOLEAN initializeCalled;
extern BOOLEAN xodusAvailable;

extern char *msaAppId;
extern UINT32 titleId;
extern BOOLEAN fullTrust;

/* C linkage so main.c and C++ translation units share the same symbols. */
EXTERN_C IXThreadingImpl *x_threading_impl;
EXTERN_C IXGameRuntimeFeatureImpl *x_game_runtime_feature;
EXTERN_C IXSystemImpl *x_system;
EXTERN_C IXSystemAnalyticsImpl *x_system_analytics;
EXTERN_C IXNetworkingImpl *x_networking;
EXTERN_C IXGameImpl *x_game;
EXTERN_C IXUserImpl6 *x_user;
EXTERN_C IXUserDeviceImpl *x_user_device;
EXTERN_C IXLauncherImpl *x_launcher;

#ifdef __cplusplus
EXTERN_C ABI::Xodus::IIPCLayer *xodus_ipclayer;
EXTERN_C ABI::Xodus::IXodusService *xodus_service;
EXTERN_C ABI::Xodus::IXodusXMLBuilder *xodus_xml_builder;
#else
EXTERN_C IIPCLayer *xodus_ipclayer;
EXTERN_C IXodusService *xodus_service;
EXTERN_C IXodusXMLBuilder *xodus_xml_builder;
#endif

EXTERN_C HRESULT WINAPI QueryApiImpl( const GUID *runtimeClassId, REFIID interfaceId, void **out );

typedef struct _INITIALIZE_OPTIONS
{
    UINT32 unknown;
    BOOLEAN isInlineConfig;
    const char *gameConfig;
} INITIALIZE_OPTIONS;

typedef struct _POLL_SOCKET_ARGS
{
    BYTE curr_buffer[POLL_BUFFER_SIZE];
    SIZE_T curr_buffer_size;
} POLL_SOCKET_ARGS;

enum unix_funcs
{
    conn_socket,
    poll_socket,
    send_frame
};

EXTERN_C unixlib_handle_t unixhandle;


typedef HRESULT (WINAPI *async_operation_callback)( IUnknown *invoker, PVOID param, PROPVARIANT *result );

#define DEFINE_ASYNC_COMPLETED_HANDLER( name, iface_type, async_type )                              \
    struct name                                                                                     \
    {                                                                                               \
        iface_type iface_type##_iface;                                                              \
        LONG refcount;                                                                              \
        BOOL invoked;                                                                               \
        HANDLE event;                                                                               \
    };                                                                                              \
                                                                                                    \
    struct name *impl_from_##name( iface_type *iface )                                              \
    {                                                                                               \
        return CONTAINING_RECORD( iface, struct name, iface_type##_iface );                         \
    }                                                                                               \
                                                                                                    \
    static HRESULT WINAPI name##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    {                                                                                               \
        if (!out) return E_POINTER;                                                                             \
        *out = NULL;                                                                                \
        if (!iid) return E_INVALIDARG;                                                                          \
        if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IAgileObject ) ||           \
            IsEqualGUID( iid, &IID_##iface_type ))                                                  \
        {                                                                                           \
            IUnknown_AddRef( iface );                                                               \
            *out = iface;                                                                           \
            return S_OK;                                                                            \
        }                                                                                           \
                                                                                                    \
        *out = NULL;                                                                                \
        return E_NOINTERFACE;                                                                       \
    }                                                                                               \
                                                                                                    \
    static ULONG WINAPI name##_AddRef( iface_type *iface )                                          \
    {                                                                                               \
        struct name *impl = CONTAINING_RECORD( iface, struct name, iface_type##_iface );            \
        return InterlockedIncrement( &impl->refcount );                                             \
    }                                                                                               \
                                                                                                    \
    static ULONG WINAPI name##_Release( iface_type *iface )                                         \
    {                                                                                               \
        struct name *impl = CONTAINING_RECORD( iface, struct name, iface_type##_iface );            \
        ULONG ref = InterlockedDecrement( &impl->refcount );                                        \
        if (!ref) { CloseHandle( impl->event ); free( impl ); }                                                 \
        return ref;                                                                                 \
    }                                                                                               \
                                                                                                    \
    static HRESULT WINAPI name##_Invoke( iface_type *iface, async_type *async, AsyncStatus status ) \
    {                                                                                               \
        struct name *impl = CONTAINING_RECORD( iface, struct name, iface_type##_iface );            \
                                                                                                    \
        TRACE( "iface %p, async %p, status %u\n", iface, async, status );                           \
                                                                                                    \
        impl->invoked = TRUE;                                                                       \
        if (impl->event) SetEvent( impl->event );                                                   \
        return S_OK;                                                                                \
    }                                                                                               \
                                                                                                    \
    static iface_type##Vtbl name##_vtbl =                                                           \
    {                                                                                               \
        name##_QueryInterface,                                                                      \
        name##_AddRef,                                                                              \
        name##_Release,                                                                             \
        name##_Invoke,                                                                              \
    };                                                                                              \
                                                                                                    \
    static iface_type *name##_create( HANDLE event )                                                \
    {                                                                                               \
        struct name *impl;                                                                          \
                                                                                                    \
        if (!(impl = calloc( 1, sizeof(*impl) ))) return NULL;                                      \
        impl->iface_type##_iface.lpVtbl = &name##_vtbl;                                             \
        if (!DuplicateHandle( GetCurrentProcess(), event, GetCurrentProcess(), &impl->event, 0, FALSE, DUPLICATE_SAME_ACCESS ))\
        { free( impl ); return NULL; }                                                                          \
        impl->refcount = 1;                                                                         \
                                                                                                    \
        return &impl->iface_type##_iface;                                                           \
    }                                                                                               \
                                                                                                    \
    static DWORD await_##async_type( async_type *async, DWORD timeout )                             \
    {                                                                                               \
        iface_type *handler;                                                                        \
        HANDLE event;                                                                               \
        HRESULT hr;                                                                                 \
        DWORD ret;                                                                                  \
                                                                                                    \
        if (!async) return E_POINTER;                                                                           \
        event = CreateEventW( NULL, FALSE, FALSE, NULL );                                           \
        if (!event) return HRESULT_FROM_WIN32( GetLastError() );                                                \
        handler = name##_create( event );                                                           \
        if (!handler) { CloseHandle( event ); return E_OUTOFMEMORY; }                                           \
        hr = async_type##_put_Completed( async, handler );                                          \
        if (FAILED(hr)) { iface_type##_Release( handler ); CloseHandle( event ); return hr; }                   \
        ret = WaitForSingleObject( event, timeout );                                                \
        CloseHandle( event );                                                                       \
        iface_type##_Release( handler );                                                            \
                                                                                                    \
        return ret;                                                                                 \
    }


#endif
