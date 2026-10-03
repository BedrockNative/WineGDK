/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "wine/unixlib.h"
struct terminal_info
{
    UINT32 input, output;
    UINT32 width, height;
};
enum terminal_unix_call { unix_terminal_info };
