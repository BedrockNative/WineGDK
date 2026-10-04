/*
 * GDK error callbacks and policy.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

#include "../../private.h"
#include "errhandlingapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdkc);

static SRWLOCK error_lock = SRWLOCK_INIT;
static XErrorCallback *error_callback;
static void *error_context;
static XErrorOptions debugger_options = XErrorOptions_OutputDebugStringOnError;
static XErrorOptions normal_options;
static __declspec(thread) BOOL reporting_error;

static HRESULT WINAPI error_QueryInterface( IXErrorImpl *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!iid) return E_INVALIDARG;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IXErrorImpl)) return E_NOINTERFACE;
    *out = iface;
    IXErrorImpl_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI error_AddRef( IXErrorImpl *iface ) { return 2; }
static ULONG WINAPI error_Release( IXErrorImpl *iface ) { return 1; }

static HRESULT WINAPI error_Reserved( IXErrorImpl *iface )
{
    return E_NOTIMPL;
}

static void WINAPI error_SetCallback( IXErrorImpl *iface, XErrorCallback *callback, void *context )
{
    AcquireSRWLockExclusive( &error_lock );
    error_callback = callback;
    error_context = context;
    ReleaseSRWLockExclusive( &error_lock );
}

static void WINAPI error_SetOptions( IXErrorImpl *iface, XErrorOptions debugger, XErrorOptions normal )
{
    AcquireSRWLockExclusive( &error_lock );
    debugger_options = debugger;
    normal_options = normal;
    ReleaseSRWLockExclusive( &error_lock );
}

HRESULT WINAPI x_error_report( HRESULT status, const char *message )
{
    XErrorCallback *callback;
    XErrorOptions options;
    void *context;
    BOOL proceed = TRUE;

    if (!message) return E_INVALIDARG;
    if (SUCCEEDED(status) || reporting_error) return status;
    AcquireSRWLockShared( &error_lock );
    callback = error_callback;
    context = error_context;
    options = IsDebuggerPresent() ? debugger_options : normal_options;
    ReleaseSRWLockShared( &error_lock );

    /* The callback can change policy or report another error. Never invoke it
     * while holding the registration lock, and prevent recursive reporting. */
    reporting_error = TRUE;
    if (callback) proceed = callback( status, message, context );
    reporting_error = FALSE;
    WARN( "status %#lx: %s\n", status, debugstr_a(message) );
    if (!proceed) return status;
    if (options & XErrorOptions_OutputDebugStringOnError) OutputDebugStringA( message );
    if (options & XErrorOptions_DebugBreakOnError) DebugBreak();
    if (options & XErrorOptions_FailFastOnError) RaiseFailFastException( NULL, NULL, 0 );
    return status;
}

static const IXErrorImplVtbl error_vtbl =
{
    error_QueryInterface, error_AddRef, error_Release, error_Reserved,
    error_SetCallback, error_SetOptions
};
static IXErrorImpl error_impl = { &error_vtbl };
IXErrorImpl *x_error_impl = &error_impl;
