/* GUI crash fixture: the debugger must not allocate a console when logging.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>

int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,char *command,int show)
{
    (void)instance;
    (void)previous;
    (void)command;
    (void)show;
    RaiseException(0xe0421001,EXCEPTION_NONCONTINUABLE,0,NULL);
    return 42;
}
