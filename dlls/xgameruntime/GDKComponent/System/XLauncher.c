/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XLauncher
 *
 * Copyright 2026 Olivia Ryan
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

#include "private.h"
#include "shellapi.h"
#include "xlauncher.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdkc);

struct x_launcher
{
    IXLauncherImpl IXLauncherImpl_iface;
    LONG ref;
};

static inline struct x_launcher *impl_from_IXLauncherImpl( IXLauncherImpl *iface )
{
    return CONTAINING_RECORD( iface, struct x_launcher, IXLauncherImpl_iface );
}

static HRESULT WINAPI x_launcher_QueryInterface( IXLauncherImpl *iface, REFIID iid, void **out )
{
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (!out) return E_POINTER;
    *out = NULL;
    if (!iid) return E_INVALIDARG;

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IXLauncherImpl ))
    {
        IXLauncherImpl_AddRef( iface );
        *out = iface;
        return S_OK;
    }

    return E_NOINTERFACE;
}

static ULONG WINAPI x_launcher_AddRef( IXLauncherImpl *iface )
{
    struct x_launcher *impl = impl_from_IXLauncherImpl( iface );
    return InterlockedIncrement( &impl->ref );
}

static ULONG WINAPI x_launcher_Release( IXLauncherImpl *iface )
{
    struct x_launcher *impl = impl_from_IXLauncherImpl( iface );
    return InterlockedDecrement( &impl->ref );
}

static HRESULT WINAPI x_launcher_XLaunchUri( IXLauncherImpl *iface, XUserHandle user, const char *uri )
{
    SHELLEXECUTEINFOW info = { sizeof(info) };
    WCHAR *uri_w;
    DWORD error;
    HRESULT hr;
    int length;

    TRACE( "iface %p, user %p, uri %s.\n", iface, user, debugstr_a( uri ) );

    if (!uri) return E_POINTER;
    if (!*uri) return E_INVALIDARG;
    if (!(length = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, uri, -1, NULL, 0 )))
        return HRESULT_FROM_WIN32( GetLastError() );
    if (!(uri_w = malloc( (SIZE_T)length * sizeof(*uri_w) ))) return E_OUTOFMEMORY;
    if (!MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, uri, -1, uri_w, length ))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        free( uri_w );
        return hr;
    }

    info.fMask = SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open";
    info.lpFile = uri_w;
    info.nShow = SW_SHOW;
    if (ShellExecuteExW( &info )) hr = S_OK;
    else
    {
        error = GetLastError();
        hr = HRESULT_FROM_WIN32( error ? error : ERROR_GEN_FAILURE );
    }

    free( uri_w );
    return hr;
}

static HRESULT WINAPI x_launcher_XDisplayAcquireTimeoutDeferral( IXLauncherImpl *iface,
        XDisplayTimeoutDeferralHandle *handle )
{
    FIXME( "iface %p, handle %p stub!\n", iface, handle );
    if (!handle) return E_POINTER;
    *handle = NULL;
    return E_NOTIMPL;
}

static void WINAPI x_launcher_XDisplayCloseTimeoutDeferralHandle( IXLauncherImpl *iface,
        XDisplayTimeoutDeferralHandle handle )
{
    FIXME( "iface %p, handle %p stub!\n", iface, handle );
}

static const struct IXLauncherImplVtbl x_launcher_vtbl =
{
    x_launcher_QueryInterface,
    x_launcher_AddRef,
    x_launcher_Release,
    x_launcher_XLaunchUri,
    x_launcher_XDisplayAcquireTimeoutDeferral,
    x_launcher_XDisplayCloseTimeoutDeferralHandle,
};

static struct x_launcher x_launcher_impl = {{&x_launcher_vtbl}, 1};

IXLauncherImpl *x_launcher = &x_launcher_impl.IXLauncherImpl_iface;
