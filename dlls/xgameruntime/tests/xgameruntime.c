/*
 * Xbox Game runtime Library Tests
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

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#define COBJMACROS
#include <initguid.h>
#include <windef.h>
#include <winbase.h>
#include <winternl.h>
#include <roapi.h>
#include <activation.h>
#include <unknwn.h>
#include <xgameerr.h>
#include <xgame.h>
#include <xlauncher.h>
#include <xnetworking.h>
#include <xasyncprovider.h>

#include "wine/test.h"

struct initialize_options
{
    UINT32 reserved;
    BOOLEAN is_inline_config;
    const char *game_config;
};

static HRESULT (WINAPI *pQueryApiImpl)( const GUID *, REFIID, void ** );
static HRESULT (WINAPI *pInitializeApiImplEx2)( ULONG, ULONG, CHAR, struct initialize_options * );

static void test_launcher(void)
{
    IXLauncherImpl *launcher = NULL;
    XDisplayTimeoutDeferralHandle handle;
    HRESULT hr;

    hr = pQueryApiImpl( &CLSID_XLauncherImpl, &IID_IXLauncherImpl, (void **)&launcher );
    ok( hr == S_OK, "QueryApiImpl(XLauncher) returned %#lx.\n", hr );
    if (FAILED(hr) || !launcher) return;

    hr = IXLauncherImpl_XLaunchUri( launcher, NULL, NULL );
    ok( hr == E_POINTER, "NULL URI returned %#lx.\n", hr );
    hr = IXLauncherImpl_XLaunchUri( launcher, NULL, "" );
    ok( hr == E_INVALIDARG, "Empty URI returned %#lx.\n", hr );
    hr = IXLauncherImpl_XLaunchUri( launcher, NULL, "https://example.invalid/\xc0\xaf" );
    ok( hr == HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION), "Invalid UTF-8 returned %#lx.\n", hr );

    handle = (XDisplayTimeoutDeferralHandle)0xdeadbeef;
    hr = IXLauncherImpl_XDisplayAcquireTimeoutDeferral( launcher, &handle );
    ok( hr == E_NOTIMPL, "Deferral returned %#lx.\n", hr );
    ok( !handle, "Failed deferral left handle %p.\n", handle );
    IXLauncherImpl_Release( launcher );
}

static void CALLBACK connectivity_changed( void *context, const XNetworkingConnectivityHint *hint )
{
    unsigned int *calls = context;
    ++*calls;
}

static void test_networking(void)
{
    static const XNetworkingStatisticsBuffer empty_statistics;
    IXNetworkingImpl2 *networking = NULL;
    XNetworkingSecurityInformation security = {0}, *result;
    XNetworkingStatisticsBuffer statistics;
    XNetworkingThumbprint thumbprint = {0};
    XTaskQueueRegistrationToken token;
    unsigned int calls = 0;
    UINT64 value = ~(UINT64)0;
    BYTE buffer[sizeof(security)];
    HRESULT hr;

    hr = pQueryApiImpl( &CLSID_XNetworkingImpl, &IID_IXNetworkingImpl2, (void **)&networking );
    ok( hr == S_OK, "QueryApiImpl(XNetworking2) returned %#lx.\n", hr );
    if (FAILED(hr) || !networking) return;

    hr = IXNetworkingImpl2_XNetworkingQueryConfigurationSetting( networking,
            XNetworkingConfigurationSetting_MaxTitleTcpQueuedReceiveBufferSize, &value );
    ok( hr == E_NOTIMPL, "QueryConfigurationSetting returned %#lx.\n", hr );
    ok( !value, "QueryConfigurationSetting did not clear output.\n" );
    hr = IXNetworkingImpl2_XNetworkingQueryConfigurationSetting( networking,
            XNetworkingConfigurationSetting_MaxTitleTcpQueuedReceiveBufferSize, NULL );
    ok( hr == E_POINTER, "NULL configuration output returned %#lx.\n", hr );
    hr = IXNetworkingImpl2_XNetworkingSetConfigurationSetting( networking,
            XNetworkingConfigurationSetting_MaxTitleTcpQueuedReceiveBufferSize, 0 );
    ok( hr == E_NOTIMPL, "SetConfigurationSetting returned %#lx.\n", hr );
    memset( &statistics, 0xcc, sizeof(statistics) );
    hr = IXNetworkingImpl2_XNetworkingQueryStatistics( networking,
            XNetworkingStatisticsType_TitleTcpQueuedReceivedBufferUsage, &statistics );
    ok( hr == E_NOTIMPL, "QueryStatistics returned %#lx.\n", hr );
    ok( !memcmp( &statistics, &empty_statistics, sizeof(statistics) ), "Statistics output was not cleared.\n" );
    hr = IXNetworkingImpl2_XNetworkingQueryStatistics( networking,
            XNetworkingStatisticsType_TitleTcpQueuedReceivedBufferUsage, NULL );
    ok( hr == E_POINTER, "NULL statistics output returned %#lx.\n", hr );

    hr = IXNetworkingImpl2_XNetworkingGetConnectivityHint( networking, NULL );
    ok( hr == E_POINTER, "NULL connectivity hint returned %#lx.\n", hr );
    hr = IXNetworkingImpl2_XNetworkingRegisterConnectivityHintChanged( networking, NULL, &calls, NULL, &token );
    ok( hr == E_POINTER, "NULL connectivity callback returned %#lx.\n", hr );
    hr = IXNetworkingImpl2_XNetworkingRegisterConnectivityHintChanged( networking, NULL, &calls, connectivity_changed, NULL );
    ok( hr == E_POINTER, "NULL registration token returned %#lx.\n", hr );
    ok( !calls, "Invalid registration invoked callback.\n" );

    result = (void *)0xdeadbeef;
    hr = IXNetworkingImpl2_XNetworkingQuerySecurityInformationForUrlAsyncResult( networking,
            NULL, sizeof(buffer), NULL, buffer, &result );
    ok( hr == E_POINTER, "NULL security async block returned %#lx.\n", hr );
    ok( !result, "Failed security result left pointer %p.\n", result );
    hr = IXNetworkingImpl2_XNetworkingVerifyServerCertificate( networking, NULL, &security );
    ok( hr == E_POINTER, "NULL request handle returned %#lx.\n", hr );
    hr = IXNetworkingImpl2_XNetworkingVerifyServerCertificate( networking, (void *)1, NULL );
    ok( hr == E_POINTER, "NULL security info returned %#lx.\n", hr );
    security.thumbprintCount = 1;
    hr = IXNetworkingImpl2_XNetworkingVerifyServerCertificate( networking, (void *)1, &security );
    ok( hr == E_INVALIDARG, "NULL pin array returned %#lx.\n", hr );
    security.thumbprints = &thumbprint;
    hr = IXNetworkingImpl2_XNetworkingVerifyServerCertificate( networking, (void *)1, &security );
    ok( hr == E_NOTIMPL, "Unimplemented pin validation returned %#lx.\n", hr );

    IXNetworkingImpl2_Release( networking );
}

static DWORD WINAPI time_sensitive_thread( void *arg )
{
    IXThreadingImpl *threading = arg;
    HRESULT hr;

    ok( !IXThreadingImpl_XThreadIsTimeSensitive( threading ), "New thread inherited time sensitivity.\n" );
    hr = IXThreadingImpl_XThreadSetTimeSensitive( threading, TRUE );
    ok( hr == S_OK, "Setting worker time sensitivity returned %#lx.\n", hr );
    ok( IXThreadingImpl_XThreadIsTimeSensitive( threading ), "Worker time sensitivity was not set.\n" );
    hr = IXThreadingImpl_XThreadSetTimeSensitive( threading, FALSE );
    ok( hr == S_OK, "Clearing worker time sensitivity returned %#lx.\n", hr );
    ok( !IXThreadingImpl_XThreadIsTimeSensitive( threading ), "Worker time sensitivity was not cleared.\n" );
    return 0;
}

static void test_thread_time_sensitivity(void)
{
    IXThreadingImpl *threading = NULL;
    HANDLE thread;
    HRESULT hr;

    hr = pQueryApiImpl( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void **)&threading );
    ok( hr == S_OK, "QueryApiImpl(XThreading) returned %#lx.\n", hr );
    if (FAILED(hr) || !threading) return;

    ok( !IXThreadingImpl_XThreadIsTimeSensitive( threading ), "Main thread initially time sensitive.\n" );
    hr = IXThreadingImpl_XThreadSetTimeSensitive( threading, TRUE );
    ok( hr == S_OK, "Setting main thread time sensitivity returned %#lx.\n", hr );
    thread = CreateThread( NULL, 0, time_sensitive_thread, threading, 0, NULL );
    ok( !!thread, "CreateThread failed, error %lu.\n", GetLastError() );
    if (thread)
    {
        WaitForSingleObject( thread, INFINITE );
        CloseHandle( thread );
        ok( IXThreadingImpl_XThreadIsTimeSensitive( threading ), "Worker changed main thread time sensitivity.\n" );
    }
    hr = IXThreadingImpl_XThreadSetTimeSensitive( threading, FALSE );
    ok( hr == S_OK, "Clearing main thread time sensitivity returned %#lx.\n", hr );
    IXThreadingImpl_XThreadAssertNotTimeSensitive( threading );
    IXThreadingImpl_Release( threading );
}

static void test_initialization(void)
{
    static const char *invalid_configs[] =
    {
        "",
        "<Game",
        "<Other/>",
        "<Game><TitleId>invalid</TitleId></Game>",
        "<Game><TitleId>100000000</TitleId></Game>",
        "<Game><TitleId>-1</TitleId></Game>",
    };
    struct initialize_options options = {0, TRUE, NULL};
    IXGameImpl *game = NULL;
    UINT32 title_id = 0;
    unsigned int i;
    HRESULT hr;

    hr = pInitializeApiImplEx2( 0, 0, 0, &options );
    ok( hr == E_INVALIDARG, "NULL config returned %#lx.\n", hr );
    for (i = 0; i < ARRAY_SIZE(invalid_configs); ++i)
    {
        options.game_config = invalid_configs[i];
        hr = pInitializeApiImplEx2( 0, 0, 0, &options );
        ok( hr == E_GAMERUNTIME_GAMECONFIG_BAD_FORMAT, "Invalid config %u returned %#lx.\n", i, hr );
    }

    options.game_config = "<Game><TitleId>FFFFFFFF</TitleId><MSAAppId>test</MSAAppId></Game>";
    hr = pInitializeApiImplEx2( 0, 0, 0, &options );
    ok( hr == S_OK, "Retry with valid inline config returned %#lx.\n", hr );

    hr = pQueryApiImpl( &CLSID_XGameImpl, &IID_IXGameImpl, (void **)&game );
    ok( hr == S_OK, "QueryApiImpl(XGame) returned %#lx.\n", hr );
    if (FAILED(hr) || !game) return;
    hr = IXGameImpl_XGameGetXboxTitleId( game, NULL );
    ok( hr == E_POINTER, "NULL title ID returned %#lx.\n", hr );
    hr = IXGameImpl_XGameGetXboxTitleId( game, &title_id );
    ok( hr == S_OK, "Getting title ID returned %#lx.\n", hr );
    ok( title_id == 0xffffffff, "Unexpected title ID %#x.\n", title_id );
    IXGameImpl_Release( game );
}

START_TEST(xgameruntime)
{
    HMODULE module;
    HRESULT hr;

    hr = RoInitialize( RO_INIT_MULTITHREADED );
    ok( SUCCEEDED(hr), "RoInitialize failed, hr %#lx.\n", hr );
    if (FAILED(hr)) return;

    module = LoadLibraryA( "xgameruntime.dll" );
    if (!module)
    {
        win_skip( "xgameruntime.dll not available, error %lu.\n", GetLastError() );
        RoUninitialize();
        return;
    }
    pQueryApiImpl = (void *)GetProcAddress( module, "QueryApiImpl" );
    pInitializeApiImplEx2 = (void *)GetProcAddress( module, "InitializeApiImplEx2" );
    if (!pQueryApiImpl || !pInitializeApiImplEx2)
        win_skip( "GDK runtime exports not available.\n" );
    else
    {
        test_launcher();
        test_networking();
        test_thread_time_sensitivity();
        test_initialization();
    }

    FreeLibrary( module );
    RoUninitialize();
}
