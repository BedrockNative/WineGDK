/* WinRT Microsoft.Windows.Storage.Pickers - dialog request (PE side)
 *
 * Copyright 2026 OrionBE contributors
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "private.h"
#include "winuser.h"
#include "winnls.h"

#include "wine/debug.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(pickers);

#define RESULT_BUFFER_SIZE (512 * 1024)

static inline struct picker_request *impl_from_IUnknown( IUnknown *iface )
{
    return CONTAINING_RECORD( iface, struct picker_request, IUnknown_iface );
}

static HRESULT WINAPI request_QueryInterface( IUnknown *iface, REFIID iid, void **out )
{
    struct picker_request *impl = impl_from_IUnknown( iface );

    if (IsEqualGUID( iid, &IID_IUnknown ))
    {
        *out = &impl->IUnknown_iface;
        IUnknown_AddRef( &impl->IUnknown_iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI request_AddRef( IUnknown *iface )
{
    struct picker_request *impl = impl_from_IUnknown( iface );
    return InterlockedIncrement( &impl->ref );
}

static ULONG WINAPI request_Release( IUnknown *iface )
{
    struct picker_request *impl = impl_from_IUnknown( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    if (!ref)
    {
        free( impl->title );
        free( impl->accept_label );
        free( impl->filters );
        free( impl->current_name );
        free( impl->current_folder );
        free( impl->default_ext );
        free( impl );
    }
    return ref;
}

static const IUnknownVtbl request_vtbl =
{
    request_QueryInterface,
    request_AddRef,
    request_Release,
};

static HWND get_picker_owner( UINT64 window_id )
{
    HWND hwnd = (HWND)(ULONG_PTR)window_id;
    DWORD process;

    if (!hwnd || !IsWindow( hwnd )) hwnd = GetForegroundWindow();
    if (!hwnd) return NULL;
    hwnd = GetAncestor( hwnd, GA_ROOT );
    GetWindowThreadProcessId( hwnd, &process );
    return process == GetCurrentProcessId() ? hwnd : NULL;
}

HRESULT picker_request_create( enum picker_kind kind, UINT64 window_id, struct picker_request **out )
{
    struct picker_request *impl;

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IUnknown_iface.lpVtbl = &request_vtbl;
    impl->ref = 1;
    impl->kind = kind;
    /* Resolve the owner on the caller's thread, before the async worker starts. */
    impl->window_id = (UINT64)(ULONG_PTR)get_picker_owner( window_id );
    impl->start_location = PickerLocationId_Unspecified;
    *out = impl;
    return S_OK;
}

WCHAR *hstring_dup( HSTRING str )
{
    UINT32 len;
    const WCHAR *buf = WindowsGetStringRawBuffer( str, &len );
    WCHAR *ret;

    if (!len) return NULL;
    if (!(ret = malloc( (len + 1) * sizeof(WCHAR) ))) return NULL;
    memcpy( ret, buf, len * sizeof(WCHAR) );
    ret[len] = 0;
    return ret;
}

static char *to_utf8( const WCHAR *str )
{
    int len;
    char *ret;

    if (!str) return NULL;
    len = WideCharToMultiByte( CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL );
    if (!(ret = malloc( len ))) return NULL;
    WideCharToMultiByte( CP_UTF8, 0, str, -1, ret, len, NULL, NULL );
    return ret;
}

static UINT64 get_x11_window( UINT64 window_id )
{
    HWND hwnd = (HWND)(ULONG_PTR)window_id;
    HANDLE xwin;

    if (!hwnd || !IsWindow( hwnd )) return 0;
    xwin = GetPropW( hwnd, L"__wine_x11_whole_window" );
    TRACE( "hwnd %p -> X11 window %p\n", hwnd, xwin );
    return (UINT64)(ULONG_PTR)xwin;
}

static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
static BOOL unix_ready;

static BOOL WINAPI init_unixlib( INIT_ONCE *once, void *param, void **context )
{
    NTSTATUS status = __wine_init_unix_call();
    if (status) ERR( "failed to load the Unix library, status %#lx\n", status );
    else unix_ready = TRUE;
    return TRUE;
}

typedef WCHAR * (CDECL *wine_get_dos_file_name_t)( const char * );
typedef char * (CDECL *wine_get_unix_file_name_t)( const WCHAR * );

static WCHAR *unix_to_dos( const char *path )
{
    static wine_get_dos_file_name_t pwine_get_dos_file_name;
    WCHAR *dos, *ret;
    size_t len;

    if (!pwine_get_dos_file_name)
        pwine_get_dos_file_name = (void *)GetProcAddress( GetModuleHandleW( L"kernel32.dll" ), "wine_get_dos_file_name" );
    if (!pwine_get_dos_file_name) return NULL;
    if (!(dos = pwine_get_dos_file_name( path ))) return NULL;
    len = wcslen( dos );
    if ((ret = malloc( (len + 1) * sizeof(WCHAR) ))) memcpy( ret, dos, (len + 1) * sizeof(WCHAR) );
    HeapFree( GetProcessHeap(), 0, dos );
    return ret;
}

static char *dos_to_unix( const WCHAR *path )
{
    static wine_get_unix_file_name_t pwine_get_unix_file_name;
    char *unix_path, *ret;

    if (!pwine_get_unix_file_name)
        pwine_get_unix_file_name = (void *)GetProcAddress( GetModuleHandleW( L"kernel32.dll" ), "wine_get_unix_file_name" );
    if (!pwine_get_unix_file_name || !path || !*path) return NULL;
    if (!(unix_path = pwine_get_unix_file_name( path ))) return NULL;
    ret = strdup( unix_path );
    HeapFree( GetProcessHeap(), 0, unix_path );
    return ret;
}

static BOOL path_has_extension( const WCHAR *path )
{
    const WCHAR *name = wcsrchr( path, '\\' ), *dot;
    name = name ? name + 1 : path;
    dot = wcsrchr( name, '.' );
    return dot && dot != name;
}

HRESULT picker_run_dialog( struct picker_request *request, WCHAR **paths )
{
    struct picker_show_params params = {0};
    char *title, *accept, *filters, *name, *folder;
    WCHAR *ret = NULL;
    size_t ret_len = 0;
    NTSTATUS status;
    HRESULT hr = S_OK;
    char *p;
    HWND owner = (HWND)(ULONG_PTR)request->window_id;
    BOOL restore_owner = FALSE;

    *paths = NULL;
    InitOnceExecuteOnce( &init_once, init_unixlib, NULL, NULL );
    if (!unix_ready) return E_NOTIMPL;

    switch (request->kind)
    {
    case PICKER_KIND_OPEN_MULTIPLE: params.mode = UNIX_PICKER_OPEN_MULTIPLE; break;
    case PICKER_KIND_SAVE: params.mode = UNIX_PICKER_SAVE; break;
    case PICKER_KIND_FOLDER: params.mode = UNIX_PICKER_FOLDER; break;
    default: params.mode = UNIX_PICKER_OPEN; break;
    }
    params.start_location = request->start_location;
    params.x11_window = get_x11_window( request->window_id );
    params.title = title = to_utf8( request->title );
    params.accept_label = accept = to_utf8( request->accept_label );
    params.filters = filters = to_utf8( request->filters );
    params.current_name = name = to_utf8( request->current_name );
    params.current_folder = folder = dos_to_unix( request->current_folder );
    params.result_size = RESULT_BUFFER_SIZE;
    if ((request->title && !title) || (request->accept_label && !accept) ||
        (request->filters && !filters) || (request->current_name && !name) ||
        !(params.result = malloc( params.result_size )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }

    /* The native dialog is outside Wine's window hierarchy. Prevent the game from
     * reclaiming keyboard focus or trapping the pointer while its modal picker is up.
     * EnableWindow also sends WM_CANCELMODE to the owner's UI thread. */
    if (owner && IsWindow( owner ) && IsWindowEnabled( owner ))
    {
        EnableWindow( owner, FALSE );
        restore_owner = TRUE;
        ClipCursor( NULL );
    }
    status = WINE_UNIX_CALL( unix_picker_show, &params );
    if (restore_owner && IsWindow( owner ))
    {
        EnableWindow( owner, TRUE );
        SetForegroundWindow( owner );
    }
    if (status)
    {
        WARN( "picker_show failed, status %#lx\n", status );
        if (status == STATUS_NOT_IMPLEMENTED) hr = E_NOTIMPL;
        else if (status == STATUS_NO_MEMORY) hr = E_OUTOFMEMORY;
        else hr = HRESULT_FROM_WIN32( RtlNtStatusToDosError( status ) );
        goto done;
    }
    if (!params.result_len)
    {
        TRACE( "cancelled\n" );
        goto done;
    }

    /* NUL-terminated Unix paths -> double-NUL-terminated Windows path list. */
    for (p = params.result; p < params.result + params.result_len; p += strlen( p ) + 1)
    {
        WCHAR *dos, *tmp;
        size_t len;

        if (!(dos = unix_to_dos( p )))
        {
            WARN( "cannot convert %s to a Windows path\n", debugstr_a(p) );
            hr = E_FAIL;
            goto done;
        }
        if (request->kind == PICKER_KIND_SAVE && request->default_ext && !path_has_extension( dos ))
        {
            len = wcslen( dos ) + wcslen( request->default_ext ) + 2;
            if (!(tmp = malloc( len * sizeof(WCHAR) )))
            {
                free( dos );
                hr = E_OUTOFMEMORY;
                goto done;
            }
            swprintf( tmp, len, L"%s%s%s", dos, request->default_ext[0] == '.' ? L"" : L".", request->default_ext );
            free( dos );
            dos = tmp;
        }
        TRACE( "picked %s -> %s\n", debugstr_a(p), debugstr_w(dos) );
        len = wcslen( dos ) + 1;
        if (!(tmp = realloc( ret, (ret_len + len + 1) * sizeof(WCHAR) )))
        {
            free( dos );
            hr = E_OUTOFMEMORY;
            goto done;
        }
        ret = tmp;
        memcpy( ret + ret_len, dos, len * sizeof(WCHAR) );
        ret_len += len;
        ret[ret_len] = 0;
        free( dos );
    }

done:
    free( params.result );
    free( title );
    free( accept );
    free( filters );
    free( name );
    free( folder );
    if (FAILED(hr)) free( ret );
    else *paths = ret;
    return hr;
}
