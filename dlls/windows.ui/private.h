/* WinRT Windows.UI Implementation
 *
 * Copyright (C) 2023 Mohamad Al-Jaf
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

#ifndef __WINE_WINDOWS_UI_PRIVATE_H
#define __WINE_WINDOWS_UI_PRIVATE_H

#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winstring.h"

#include "activation.h"
#include "corewindow.h"

#define WIDL_using_Windows_Devices_Input
#define WIDL_using_Windows_UI_Input
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_System
#define WIDL_using_Windows_Foundation
#include "windows.foundation.h"
#define WIDL_using_Windows_UI
#include "windows.ui.h"
#define WIDL_using_Windows_UI_Core
#include "windows.ui.core.h"
#define WIDL_using_Windows_UI_ViewManagement
#include "windows.ui.viewmanagement.h"

extern IActivationFactory *accessibilitysettings_factory;
extern IActivationFactory *uisettings_factory;
extern IActivationFactory *uiviewsettings_factory;
extern IActivationFactory *inputpane_factory;
extern IActivationFactory *corewindow_factory;
extern IActivationFactory *corecursor_factory;
HRESULT corecursor_create(CoreCursorType type, UINT32 id, ICoreCursor **out);
HRESULT corecursor_handle(ICoreCursor *cursor, HCURSOR *out);
HRESULT pointer_args_create(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, IPointerEventArgs **out);
extern IActivationFactory *navigation_factory;
extern IActivationFactory *pointervisualization_factory;
extern IActivationFactory *mouse_factory;
extern IActivationFactory *mousecapabilities_factory;
extern IActivationFactory *keyboardcapabilities_factory;
extern IActivationFactory *touchcapabilities_factory;
HRESULT mouse_create(HWND hwnd, IMouseDevice **out);
HRESULT corewindow_get_mouse(IMouseDevice **out);
void mouse_close(IMouseDevice *iface);
void mouse_input(IMouseDevice *iface, HRAWINPUT input);
HRESULT pointervisualization_create(IPointerVisualizationSettings **out);
HRESULT corewindow_get_pointervisualization(IPointerVisualizationSettings **out);
struct dispatched_action;
struct dispatcher_queue { SRWLOCK lock; HWND hwnd; struct dispatched_action *head; CoreDispatcherPriority current_priority; };
#define WINE_WM_DISPATCH (WM_APP + 0x317)
HRESULT dispatcher_queue_add(struct dispatcher_queue *queue, CoreDispatcherPriority priority, IDispatchedHandler *callback, IAsyncAction **out);
BOOL dispatcher_queue_should_yield(struct dispatcher_queue *queue, CoreDispatcherPriority priority);
void dispatcher_queue_dispatch(struct dispatcher_queue *queue);
void dispatcher_queue_close(struct dispatcher_queue *queue);
#define WINE_SC_BACK 0xeff0
HRESULT navigation_create(HWND hwnd, ISystemNavigationManager **out);
void navigation_close(ISystemNavigationManager *iface);
BOOL navigation_back(ISystemNavigationManager *iface);
HRESULT corewindow_get_navigation(ISystemNavigationManager **out);
void corewindow_load_metadata(WCHAR *title, UINT capacity, HICON *small_icon, HICON *large_icon);

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
