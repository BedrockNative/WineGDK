/* WinRT Windows.UI.Xaml Implementation
 *
 * Copyright (C) 2025 Mohamad Al-Jaf
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

#ifndef __WINE_WINDOWS_UI_XAML_PRIVATE_H
#define __WINE_WINDOWS_UI_XAML_PRIVATE_H

#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winstring.h"

#include "activation.h"

#include "wine/debug.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_UI
#include "windows.ui.h"

#define WIDL_using_Windows_UI_Core
#include "windows.ui.core.h"
struct dispatched_action;
struct dispatcher_queue { SRWLOCK lock; HWND hwnd; struct dispatched_action *head; CoreDispatcherPriority current_priority; };
#define WINE_WM_DISPATCH (WM_APP + 0x317)
HRESULT dispatcher_queue_add(struct dispatcher_queue *queue, CoreDispatcherPriority priority, IDispatchedHandler *callback, IAsyncAction **out);
BOOL dispatcher_queue_should_yield(struct dispatcher_queue *queue, CoreDispatcherPriority priority);
void dispatcher_queue_dispatch(struct dispatcher_queue *queue);
void dispatcher_queue_close(struct dispatcher_queue *queue);
HRESULT xaml_input_create(ICoreWindow *window, UINT32 devices, IInspectable **out);
extern IActivationFactory *color_helper_factory;
extern IActivationFactory *application_factory;
extern IActivationFactory *window_factory;
extern IActivationFactory *composition_factory;
void xaml_composition_clear(void);
HRESULT xaml_get_core_window(IInspectable **out);
void xaml_window_clear(void);
HRESULT xaml_window_layout(void);
boolean xaml_control_character(UINT32 character);
HRESULT xaml_control_layout(IInspectable *object, Size available);
HRESULT xaml_load_component(IInspectable *component, IUriRuntimeClass *uri);
HRESULT xaml_control_set_string(IInspectable *object, const WCHAR *name, HSTRING value);
HRESULT xaml_control_set_double(IInspectable *object, const WCHAR *name, DOUBLE value);
HRESULT xaml_control_set_int(IInspectable *object, const WCHAR *name, INT32 value);
HRESULT xaml_control_set_content(IInspectable *object, IInspectable *value);
HRESULT xaml_control_add_child(IInspectable *object, IInspectable *value);
struct vector_iids { const GUID *vector, *view, *iterable, *iterator; };
HRESULT vector_create(const struct vector_iids *iids, void **out);
HRESULT xaml_control_factory(const WCHAR *name, IActivationFactory **out);

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

#endif
