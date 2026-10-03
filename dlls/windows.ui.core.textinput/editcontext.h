/* WinRT Windows.UI.Text.Core.CoreTextEditContext Implementation
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

#ifndef EDITCONTEXT_H
#define EDITCONTEXT_H

#include "private.h"
#include "wine/winrt_events.h"

struct core_text_edit_context
{
    ICoreTextEditContext ICoreTextEditContext_iface;
    LONG ref;
    DWORD thread;
    HWND hwnd;
    HSTRING name;
    CoreTextInputScope scope;
    CoreTextInputPaneDisplayPolicy policy;
    boolean read_only;
    CoreTextRange selection;
    unsigned int selection_version;
    struct winrt_event events[9];
};

HRESULT edit_context_focus(struct core_text_edit_context *context);
void edit_context_blur(struct core_text_edit_context *context);

#endif