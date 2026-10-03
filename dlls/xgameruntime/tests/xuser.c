/*
 * XUser argument and result validation tests
 *
 * Copyright 2026 WineGDK contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "xuser.h"
#include "wine/test.h"

static void test_arguments( IXUserImpl6 *user )
{
    XUserGetTokenAndSignatureUtf16Data *wide_result = (void *)0xdeadbeef;
    XUserGetTokenAndSignatureData *result = (void *)0xdeadbeef;
    XUserHandle duplicate = (void *)0xdeadbeef;
    XAsyncBlock async = {0};
    SIZE_T size = 123;
    HRESULT hr;

    hr = IXUserImpl6_XUserDuplicateHandle( user, NULL, &duplicate );
    ok( hr == E_POINTER, "Unexpected duplicate result %#lx.\n", hr );
    ok( !duplicate, "Failure left output handle %p.\n", duplicate );
    hr = IXUserImpl6_XUserDuplicateHandle( user, NULL, NULL );
    ok( hr == E_POINTER, "Unexpected NULL output result %#lx.\n", hr );
    IXUserImpl6_XUserCloseHandle( user, NULL );

    hr = IXUserImpl6_XUserGetMaxUsers( user, NULL );
    ok( hr == E_POINTER, "Unexpected max users result %#lx.\n", hr );
    hr = IXUserImpl6_XUserGetId( user, NULL, NULL );
    ok( hr == E_POINTER, "Unexpected user id result %#lx.\n", hr );
    hr = IXUserImpl6_XUserGetIsGuest( user, NULL, NULL );
    ok( hr == E_POINTER, "Unexpected guest result %#lx.\n", hr );
    hr = IXUserImpl6_XUserGetState( user, NULL, NULL );
    ok( hr == E_POINTER, "Unexpected state result %#lx.\n", hr );
    hr = IXUserImpl6_XUserGetAgeGroup( user, NULL, NULL );
    ok( hr == E_POINTER, "Unexpected age group result %#lx.\n", hr );

    hr = IXUserImpl6_XUserGetTokenAndSignatureAsync( user, NULL,
            XUserGetTokenAndSignatureOptions_None, "GET", "https://example.org/", 0, NULL, 0, NULL, &async );
    ok( hr == E_POINTER, "Unexpected token request result %#lx.\n", hr );
    hr = IXUserImpl6_XUserGetTokenAndSignatureUtf16Async( user, NULL,
            XUserGetTokenAndSignatureOptions_None, L"GET", L"https://example.org/", 0, NULL, 0, NULL, &async );
    ok( hr == E_POINTER, "Unexpected UTF16 token request result %#lx.\n", hr );
    hr = IXUserImpl6_XUserGetTokenAndSignatureResult( user, NULL, 0, NULL, &result, NULL );
    ok( hr == E_POINTER, "Unexpected token result %#lx.\n", hr );
    ok( !result, "Failure left output result %p.\n", result );
    hr = IXUserImpl6_XUserGetTokenAndSignatureUtf16Result( user, NULL, 0, NULL, &wide_result, NULL );
    ok( hr == E_POINTER, "Unexpected UTF16 token result %#lx.\n", hr );
    ok( !wide_result, "Failure left UTF16 output result %p.\n", wide_result );
    hr = IXUserImpl6_XUserGetTokenAndSignatureResultSize( user, &async, NULL );
    ok( hr == E_POINTER, "Unexpected token result size %#lx.\n", hr );
    hr = IXUserImpl6_XUserGetTokenAndSignatureUtf16ResultSize( user, NULL, &size );
    ok( hr == E_POINTER, "Unexpected UTF16 token result size %#lx.\n", hr );
    hr = IXUserImpl6_XUserGetGamerPictureResultSize( user, NULL, &size );
    ok( hr == E_POINTER, "Unexpected picture result size %#lx.\n", hr );
}

START_TEST(xuser)
{
    HRESULT (WINAPI *query)( const GUID *, REFIID, void ** );
    IXUserImpl6 *user;
    HMODULE module;
    HRESULT hr;

    if (!(module = LoadLibraryA( "xgameruntime.dll" )))
    {
        win_skip( "xgameruntime.dll is unavailable.\n" );
        return;
    }
    query = (void *)GetProcAddress( module, "QueryApiImpl" );
    if (!query)
    {
        win_skip( "QueryApiImpl is unavailable.\n" );
        FreeLibrary( module );
        return;
    }
    hr = query( &CLSID_XUserImpl, &IID_IXUserImpl6, (void **)&user );
    ok( hr == S_OK, "QueryApiImpl failed, hr %#lx.\n", hr );
    if (SUCCEEDED(hr))
    {
        test_arguments( user );
        IXUserImpl6_Release( user );
    }
    FreeLibrary( module );
}
