/* Native desktop terminal frontend for newly allocated consoles.
 * Copyright 2026 WineGDK contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "conhost.h"
#include "wine/debug.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(console);

static char *unix_environment_path(const WCHAR *name)
{
    WCHAR *value;
    char *path = NULL;
    DWORD size = GetEnvironmentVariableW(name, NULL, 0);
    if (!size || !(value = malloc(size * sizeof(*value)))) return NULL;
    if (GetEnvironmentVariableW(name, value, size)) path = wine_get_unix_file_name(value);
    free(value);
    return path;
}

/* Legacy xdg-terminal accepts a single command string. Quote every argument
 * using POSIX single quotes, including embedded quotes and shell metacharacters. */
static char *quote_terminal_command(const char * const *args)
{
    size_t size = 1;
    const char * const *arg;
    const char *p;
    char *result, *out;
    for (arg = args; *arg; ++arg) size += 4 * strlen(*arg) + 3;
    if (!(result = malloc(size))) return NULL;
    out = result;
    for (arg = args; *arg; ++arg)
    {
        if (arg != args) *out++ = ' ';
        *out++ = '\'';
        for (p = *arg; *p; ++p)
        {
            if (*p == '\'') { memcpy(out, "'\\''", 4); out += 4; }
            else *out++ = *p;
        }
        *out++ = '\'';
    }
    *out = 0;
    return result;
}

/* Named rendezvous objects prevent a late terminal from accepting recycled handles
 * after timeout. Only the acknowledged child may consume console requests. */
BOOL start_xdg_terminal(HANDLE server)
{
    static const char * const candidates[][5] =
    {
        /* Desktop/distribution policy takes precedence over individual emulators. */
        {"xdg-terminal-exec", "--"},
        {"xdg-terminal"},
        {"x-terminal-emulator", "-e"},
        {"sensible-terminal", "-e"},
        {"i3-sensible-terminal", "-e"},
        {"rofi-sensible-terminal", "-e"},
        {"exo-open", "--launch", "TerminalEmulator"},
        {"konsole", "--separate", "-e"},
        {"gnome-terminal", "--wait", "--"},
        {"kgx", "--"},
        {"ptyxis", "--new-window", "--"},
        {"xfce4-terminal", "--disable-server", "-x"},
        {"mate-terminal", "--disable-factory", "-x"},
        {"qterminal", "-e"},
        {"tilix", "-e"},
        {"kitty", "--"},
        {"foot", "--"},
        {"alacritty", "-e"},
        {"wezterm", "start", "--always-new-process", "--"},
        {"ghostty", "-e"},
        {"terminator", "-x"},
        {"xterm", "-e"},
        {"urxvt", "-e"},
        {"rxvt", "-e"},
    };
    WCHAR name[128], accept_name[136], setting[32];
    HANDLE ready, accept;
    char *loader = NULL, *prefix = NULL, *prefix_arg = NULL;
    char parent[24], handle[24], event[128];
    const char *argv[16];
    BOOL success = FALSE;
    DWORD count;
    unsigned int i, n;
    char *command;

    count = GetEnvironmentVariableW(L"WINECONSOLE", setting, ARRAY_SIZE(setting));
    if (count && count < ARRAY_SIZE(setting) && !wcscmp(setting, L"conhost")) return FALSE;
    if (!(loader = unix_environment_path(L"WINELOADER")) ||
        !(prefix = unix_environment_path(L"WINECONFIGDIR"))) goto done;
    if (!(prefix_arg = malloc(strlen(prefix) + sizeof("WINEPREFIX=")))) goto done;
    sprintf(prefix_arg, "WINEPREFIX=%s", prefix);
    snprintf(parent, sizeof(parent), "%lu", GetCurrentProcessId());
    snprintf(handle, sizeof(handle), "%u", condrv_handle(server));

    for (i = 0; i < ARRAY_SIZE(candidates) && !success; ++i)
    {
        swprintf(name, ARRAY_SIZE(name), L"Local\\WineConsoleTerminal-%08lx-%I64x-%u",
                GetCurrentProcessId(), GetTickCount64(), i);
        swprintf(accept_name, ARRAY_SIZE(accept_name), L"%s-accept", name);
        if (!(ready = CreateEventW(NULL, TRUE, FALSE, name))) break;
        if (!(accept = CreateEventW(NULL, TRUE, FALSE, accept_name))) { CloseHandle(ready); break; }
        WideCharToMultiByte(CP_UTF8, 0, name, -1, event, sizeof(event), NULL, NULL);

        /* Pass argv directly, never interpolate a command or prefix into shell code. */
        for (n = 0; candidates[i][n]; ++n) argv[n] = candidates[i][n];
        argv[n++] = "env";
        argv[n++] = prefix_arg;
        argv[n++] = loader;
        argv[n++] = "C:\\windows\\system32\\conhost.exe";
        argv[n++] = "--xdg-terminal";
        argv[n++] = parent;
        argv[n++] = handle;
        argv[n++] = event;
        argv[n] = NULL;
        command = NULL;
        if (!strcmp(candidates[i][0], "xdg-terminal"))
        {
            if (!(command = quote_terminal_command(argv + 1)))
            { CloseHandle(ready); CloseHandle(accept); break; }
            argv[1] = command;
            argv[2] = NULL;
        }
        TRACE("Trying native terminal %s.\n", candidates[i][0]);
        if (!__wine_unix_spawnvp((char **)argv, FALSE))
        {
            if (WaitForSingleObject(ready, 5000) == WAIT_OBJECT_0) success = SetEvent(accept);
            else WARN("%s did not become ready; trying the next terminal.\n", candidates[i][0]);
        }
        free(command);
        CloseHandle(ready);
        CloseHandle(accept);
        if (success) TRACE("Console attached through %s.\n", candidates[i][0]);
    }
done:
    HeapFree(GetProcessHeap(), 0, loader);
    HeapFree(GetProcessHeap(), 0, prefix);
    free(prefix_arg);
    return success;
}

BOOL get_native_terminal_size(unsigned int *width, unsigned int *height)
{
    static BOOL initialized;
    struct terminal_info info = {condrv_handle(GetStdHandle(STD_INPUT_HANDLE)),
                                 condrv_handle(GetStdHandle(STD_OUTPUT_HANDLE))};
    if (!initialized)
    {
        if (__wine_init_unix_call()) return FALSE;
        initialized = TRUE;
    }
    if (WINE_UNIX_CALL(unix_terminal_info, &info)) return FALSE;
    *width = info.width; *height = info.height;
    return TRUE;
}

BOOL attach_xdg_terminal(DWORD parent_id, HANDLE remote_server, const WCHAR *name, HANDLE *server, int *width, int *height)
{
    WCHAR accept_name[136];
    HANDLE parent = NULL, ready = NULL, accept = NULL;
    unsigned int columns, rows;
    BOOL success = FALSE;

    *server = NULL;
    if (wcslen(name) > 120 || !get_native_terminal_size(&columns, &rows))
        return FALSE;
    swprintf(accept_name, ARRAY_SIZE(accept_name), L"%s-accept", name);
    if (!(ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, name)) ||
        !(accept = OpenEventW(SYNCHRONIZE, FALSE, accept_name))) goto done;
    if (!(parent = OpenProcess(PROCESS_DUP_HANDLE, FALSE, parent_id))) goto done;
    if (!DuplicateHandle(parent, remote_server, GetCurrentProcess(), server, 0, FALSE, DUPLICATE_SAME_ACCESS)) goto done;
    if (!SetEvent(ready) || WaitForSingleObject(accept, 5000) != WAIT_OBJECT_0) goto done;
    *width = columns; *height = rows;
    success = TRUE;
done:
    if (parent) CloseHandle(parent);
    if (ready) CloseHandle(ready);
    if (accept) CloseHandle(accept);
    if (!success && *server) { CloseHandle(*server); *server = NULL; }
    return success;
}
