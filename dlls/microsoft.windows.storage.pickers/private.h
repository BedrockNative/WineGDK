/* WinRT Microsoft.Windows.Storage.Pickers (Windows App SDK) implementation
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

#ifndef __WINE_MICROSOFT_WINDOWS_STORAGE_PICKERS_PRIVATE_H
#define __WINE_MICROSOFT_WINDOWS_STORAGE_PICKERS_PRIVATE_H

#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winstring.h"
#include "winternl.h"

#include "activation.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Microsoft_UI
#define WIDL_using_Microsoft_Windows_Storage_Pickers
#include "microsoft.windows.storage.pickers.h"

#include "async_private.h"

struct vector_iids
{
    const GUID *iterable;
    const GUID *iterator;
    const GUID *vector;
    const GUID *view;
};

HRESULT vector_hstring_create( IVector_HSTRING **out );
HRESULT vector_hstring_create_copy( IIterable_HSTRING *iterable, IVector_HSTRING **out );
HRESULT vector_inspectable_create( const struct vector_iids *iids, IVector_IInspectable **out );

HRESULT async_operation_inspectable_create( const GUID *iid, IUnknown *invoker, IUnknown *param, async_operation_callback callback,
                                            IAsyncOperation_IInspectable **out );

/* picker.c */
extern IActivationFactory *file_open_picker_factory;
extern IActivationFactory *file_save_picker_factory;
extern IActivationFactory *folder_picker_factory;

/* dialog.c */
enum picker_kind
{
    PICKER_KIND_OPEN_SINGLE,
    PICKER_KIND_OPEN_MULTIPLE,
    PICKER_KIND_SAVE,
    PICKER_KIND_FOLDER,
};

struct picker_request
{
    IUnknown IUnknown_iface;
    LONG ref;
    enum picker_kind kind;
    UINT64 window_id;
    PickerLocationId start_location;
    WCHAR *title;           /* may be NULL */
    WCHAR *accept_label;    /* may be NULL */
    WCHAR *filters;         /* "Label\t*.png;*.zip\n" lines, may be NULL */
    WCHAR *current_name;    /* save picker, may be NULL */
    WCHAR *current_folder;  /* save picker (Windows path), may be NULL */
    WCHAR *default_ext;     /* save picker (".ext"), may be NULL */
};

HRESULT picker_request_create( enum picker_kind kind, UINT64 window_id, struct picker_request **out );
WCHAR *hstring_dup( HSTRING str );
/* runs the dialog (blocking); returns a '\0'-separated, double-'\0'-terminated list of
 * Windows paths, or NULL when the user cancelled / no dialog backend is available */
WCHAR *picker_run_dialog( struct picker_request *request );

HRESULT pick_file_result_create( const WCHAR *path, IPickFileResult **out );
HRESULT pick_folder_result_create( const WCHAR *path, IPickFolderResult **out );

HRESULT file_type_choices_create( IMap_HSTRING_IVector_HSTRING **out );
/* builds the request filter lines from the choices map */
WCHAR *file_type_choices_to_filters( IMap_HSTRING_IVector_HSTRING *map );

#define DEFINE_IINSPECTABLE_( pfx, iface_type, impl_type, impl_from, iface_mem, expr )             \
    static inline impl_type *impl_from( iface_type *iface )                                        \
    {                                                                                              \
        return CONTAINING_RECORD( iface, impl_type, iface_mem );                                   \
    }                                                                                              \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_QueryInterface( (IInspectable *)(expr), iid, out );                    \
    }                                                                                              \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                          \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_AddRef( (IInspectable *)(expr) );                                      \
    }                                                                                              \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                         \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_Release( (IInspectable *)(expr) );                                     \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *iid_count, IID **iids )         \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetIids( (IInspectable *)(expr), iid_count, iids );                    \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *class_name )      \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetRuntimeClassName( (IInspectable *)(expr), class_name );             \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *trust_level )        \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetTrustLevel( (IInspectable *)(expr), trust_level );                  \
    }
#define DEFINE_IINSPECTABLE( pfx, iface_type, impl_type, base_iface )                              \
    DEFINE_IINSPECTABLE_( pfx, iface_type, impl_type, impl_from_##iface_type, iface_type##_iface, &impl->base_iface )
#define DEFINE_IINSPECTABLE_OUTER( pfx, iface_type, impl_type, outer_iface )                       \
    DEFINE_IINSPECTABLE_( pfx, iface_type, impl_type, impl_from_##iface_type, iface_type##_iface, impl->outer_iface )

#endif
