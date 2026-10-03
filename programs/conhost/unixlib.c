/* Native terminal geometry and validation. SPDX-License-Identifier: LGPL-2.1-or-later */
#if 0
#pragma makedep unix
#endif
#include "config.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/server.h"
#include "unixlib.h"

static NTSTATUS terminal_info(void *args)
{
    struct terminal_info *info = args;
    struct winsize size;
    NTSTATUS status;
    int input, output;
    if ((status = wine_server_handle_to_fd(ULongToHandle(info->input), GENERIC_READ, &input, NULL))) return status;
    if ((status = wine_server_handle_to_fd(ULongToHandle(info->output), GENERIC_WRITE, &output, NULL)))
    { close(input); return status; }
    status = STATUS_INVALID_HANDLE;
    if (isatty(input) && isatty(output) && !ioctl(output, TIOCGWINSZ, &size))
    {
        info->width = size.ws_col ? size.ws_col : 80;
        info->height = size.ws_row ? size.ws_row : 24;
        status = STATUS_SUCCESS;
    }
    close(input);
    close(output);
    return status;
}
const unixlib_entry_t __wine_unix_call_funcs[] = { terminal_info };
#ifdef _WIN64
/* The call structure contains only 32-bit integers, in both architectures. */
const unixlib_entry_t __wine_unix_call_wow64_funcs[] = { terminal_info };
#endif
