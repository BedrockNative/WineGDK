/*
 * Microsoft.Windows.Storage.Pickers Unix interface
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

#ifndef __WINE_MICROSOFT_WINDOWS_STORAGE_PICKERS_UNIXLIB_H
#define __WINE_MICROSOFT_WINDOWS_STORAGE_PICKERS_UNIXLIB_H

#include "wine/unixlib.h"

enum unix_picker_mode
{
    UNIX_PICKER_OPEN = 0,
    UNIX_PICKER_OPEN_MULTIPLE = 1,
    UNIX_PICKER_SAVE = 2,
    UNIX_PICKER_FOLDER = 3,
};

/* All strings are UTF-8 and may be NULL.
 * filters: lines "Label\tpattern;pattern\n" (patterns are globs such as "*.png" or "*").
 * start_location: Microsoft.Windows.Storage.Pickers.PickerLocationId value.
 * current_folder: Unix path (overrides start_location when set).
 *
 * On STATUS_SUCCESS, result holds '\n'-separated Unix paths (result_len bytes, not
 * counting the terminating '\0'); result_len == 0 means the user cancelled. Paths that
 * do not fit in result_size are dropped (the dialog is never shown twice).
 * The dialog itself is shown by the OrionBE launcher on the host (file picker broker,
 * $ORIONBE_PICKER_SOCKET); an unreachable broker is reported as a cancel. */
struct picker_show_params
{
    UINT32 mode;
    UINT32 start_location;
    UINT64 x11_window;
    const char *title;
    const char *accept_label;
    const char *filters;
    const char *current_name;
    const char *current_folder;
    char *result;
    UINT32 result_size;
    UINT32 result_len;
};

enum unix_funcs
{
    unix_picker_show,
    unix_funcs_count,
};

#endif
