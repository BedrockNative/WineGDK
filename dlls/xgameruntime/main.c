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

#include "initguid.h"
#include "private.h"
#include "GDKComponent/InitInternalGDKC.h"

#include "ntstatus.h"

WINE_DEFAULT_DEBUG_CHANNEL(xgameruntime);

static HMODULE xgameruntime;
static HMODULE xgameruntime_threading;
static BOOL builtin_threading;

unixlib_handle_t unixhandle;

static VOID LoadOtherRuntime( DWORD *asked )
{
    HKEY hKey;
    LPCSTR subKey = "Software\\Wine\\WineGDK";
    LPCSTR valueName = "LoadOtherRuntimeAsked";
    DWORD value;
    DWORD dataSize = sizeof(DWORD);
    LONG result;

    *asked = 0;

    result = RegCreateKeyExA(
        HKEY_LOCAL_MACHINE,
        subKey,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_READ | KEY_WRITE,
        NULL,
        &hKey,
        NULL
    );

    if (result != ERROR_SUCCESS) {
        return;
    }

    // Try to read the value
    result = RegQueryValueExA(
        hKey,
        valueName,
        NULL,
        NULL,
        (LPBYTE)&value,
        &dataSize
    );

    if ( result == ERROR_FILE_NOT_FOUND ) 
    {
        value = 1;

        result = RegSetValueExA(
            hKey,
            valueName,
            0,
            REG_DWORD,
            (const BYTE*)&value,
            sizeof(DWORD)
        );
    } else if ( result == ERROR_SUCCESS ) 
    {
        *asked = value;

        value = 1;

        result = RegSetValueExA(
            hKey,
            valueName,
            0,
            REG_DWORD,
            (const BYTE*)&value,
            sizeof(DWORD)
        );
    }

    RegCloseKey( hKey );
    return;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    /* Runtime singletons and background work can outlive the caller. */
    return S_FALSE;
}

BOOL WINAPI DllMain( HINSTANCE hinst, DWORD reason, void *reserved )
{
    TRACE("inst %p, reason %lu, reserved %p.\n", hinst, reason, reserved);

    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            char option[2];

            DisableThreadLibraryCalls(hinst);
            builtin_threading = !(GetEnvironmentVariableA("WINEGDK_BUILTIN_XTHREADING", option,
                                                         sizeof(option)) == 1 && option[0] == '0');
            if (!builtin_threading) xgameruntime_threading = LoadLibraryA("xgameruntime.dll.threading");
            break;
        }
        case DLL_PROCESS_DETACH:
            if (reserved) break;
            if (msaAppId) free( msaAppId );
            if (xgameruntime) FreeLibrary(xgameruntime);
            if (xgameruntime_threading) FreeLibrary(xgameruntime_threading);
        break;
    }
    return TRUE;
}

typedef HRESULT (WINAPI *InitializeApiImplEx2_ext)( ULONG gdkVer, ULONG gsVer, CHAR mode, INITIALIZE_OPTIONS *options );

HRESULT WINAPI InitializeApiImplEx2( ULONG gdkVer, ULONG gsVer, CHAR mode, INITIALIZE_OPTIONS *options )
{
    //  Initialization can be done however we want on our side.
    // You can choose to return `S_OK` once the full SDK is implemented.
    //
    //   Documentation for INITIALIZE_OPTIONS is at 
    //  https://learn.microsoft.com/en-us/xbox/gdk/docs/reference/system/xgameruntimeinit/functions/xgameruntimeinitializewithoptions
    // 
    // NOTE: Never rely on INITIALIZE_OPTIONS to provide anything, as it can be nullptr.
    //

    TRACE("gdkVer %ld, gsVer %ld, mode %d, options %p stub!\n", gdkVer, gsVer, mode, options);    
    return InitializeGDKComponent( options );
}

HRESULT WINAPI InitializeApiImplEx( ULONG gdkVer, ULONG gsVer, CHAR mode )
{
    TRACE("gdkVer %ld, gsVer %ld, mode %d\n", gdkVer, gsVer, mode);
    return InitializeApiImplEx2( gdkVer, gsVer, mode, NULL );
}

HRESULT WINAPI InitializeApiImpl( ULONG gdkVer, ULONG gsVer )
{
    TRACE("gdkVer %ld, gsVer %ld\n", gdkVer, gsVer);
    return InitializeApiImplEx2( gdkVer, gsVer, 0, NULL );
}

typedef HRESULT (WINAPI *QueryApiImpl_ext)( const GUID *runtimeClassId, REFIID interfaceId, void **out );

HRESULT WINAPI QueryApiImpl( const GUID *runtimeClassId, REFIID interfaceId, void **out )
{
    QueryApiImpl_ext func;
    DWORD asked;

    TRACE( "runtimeClassId %s, interfaceId %s, out %p\n",
           debugstr_guid( runtimeClassId ), debugstr_guid( interfaceId ), out );

    if (!out) return E_POINTER;
    *out = NULL;
    if (!runtimeClassId || !interfaceId) return E_INVALIDARG;

    if (IsEqualGUID( runtimeClassId, &CLSID_XErrorImpl ))
        return IXErrorImpl_QueryInterface( x_error_impl, interfaceId, out );
    if (IsEqualGUID( runtimeClassId, &CLSID_XLauncherImpl ))
        return IXLauncherImpl_QueryInterface( x_launcher, interfaceId, out );
    if (IsEqualGUID( runtimeClassId, &CLSID_XSystemImpl ))
        return IXSystemImpl_QueryInterface( x_system, interfaceId, out );
    if (IsEqualGUID( runtimeClassId, &CLSID_XGameRuntimeFeatureImpl ))
        return IXGameRuntimeFeatureImpl_QueryInterface( x_game_runtime_feature, interfaceId, out );
    if (IsEqualGUID( runtimeClassId, &CLSID_XSystemAnalyticsImpl ))
        return IXSystemAnalyticsImpl_QueryInterface( x_system_analytics, interfaceId, out );
    if (IsEqualGUID( runtimeClassId, &CLSID_XNetworkingImpl ))
        return IXNetworkingImpl_QueryInterface( x_networking, interfaceId, out );
    if (IsEqualGUID( runtimeClassId, &CLSID_XThreadingImpl ))
    {
        if (builtin_threading) return IXThreadingImpl_QueryInterface( x_threading_impl, interfaceId, out );

        func = xgameruntime_threading ?
            (QueryApiImpl_ext)GetProcAddress( xgameruntime_threading, "QueryApiImpl" ) : NULL;
        if (func) return func( runtimeClassId, interfaceId, out );

        LoadOtherRuntime( &asked );
        if (!asked) WARN("Using WineGDK's built-in XThreading implementation.\n");
        return IXThreadingImpl_QueryInterface( x_threading_impl, interfaceId, out );
    }
    if (IsEqualGUID( runtimeClassId, &CLSID_XGameImpl ))
        return IXGameImpl_QueryInterface( x_game, interfaceId, out );
    if (IsEqualGUID( runtimeClassId, &CLSID_XUserImpl ))
        return IXUserImpl6_QueryInterface( x_user, interfaceId, out );
    if (IsEqualGUID( runtimeClassId, &CLSID_XUserDeviceImpl ))
        return IXUserDeviceImpl_QueryInterface( x_user_device, interfaceId, out );
    if (IsEqualGUID( runtimeClassId, &CLSID_XLauncherImpl ))
        return IXLauncherImpl_QueryInterface( x_launcher, interfaceId, out );

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( runtimeClassId ) );
    return E_NOTIMPL;
}

HRESULT WINAPI UninitializeApiImpl( void )
{
    TRACE("stub!\n");
    return E_NOTIMPL;
}

HRESULT WINAPI XErrorReport( HRESULT status, LPCSTR message )
{
    return x_error_report( status, message );
}
