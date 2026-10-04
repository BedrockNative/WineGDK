/*
 * Optional file and Unix stream log output
 *
 * Copyright 2026 WineGDK contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * In addition to the permissions in the GNU Lesser General Public License,
 * the authors give you unlimited permission to link the compiled version
 * of this file with other programs, and to distribute those programs
 * without any restriction coming from the use of this file.  (The GNU
 * Lesser General Public License restrictions do apply in other respects;
 * for example, they cover modification of the file, and distribution when
 * not linked into another program.)
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

#ifdef __WINE_PE_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winsock2.h"
#include "afunix.h"
#include "wine/debug.h"
#include "wine/log_output.h"
#include "wine/json.h"

WINE_DEFAULT_DEBUG_CHANNEL(log_output);

static void close_output_socket(struct wine_log_output *output)
{
    if (output->socket == INVALID_SOCKET) return;
    closesocket(output->socket);
    output->socket = INVALID_SOCKET;
    WSACleanup();
}

void wine_log_output_close(struct wine_log_output *output)
{
    close_output_socket(output);
    if (output->file != INVALID_HANDLE_VALUE) CloseHandle(output->file);
    output->file = INVALID_HANDLE_VALUE;
    output->enabled = FALSE;
}

static BOOL wait_output_socket(struct wine_log_output *output)
{
    struct timeval timeout = {2, 0};
    fd_set writable, errors;
    int error = 0, size = sizeof(error);

    FD_ZERO(&writable);
    FD_ZERO(&errors);
    FD_SET(output->socket, &writable);
    FD_SET(output->socket, &errors);
    if (select(0, NULL, &writable, &errors, &timeout) <= 0) return FALSE;
    return FD_ISSET(output->socket, &writable) &&
           !getsockopt(output->socket, SOL_SOCKET, SO_ERROR, (char *)&error, &size) && !error;
}

static BOOL open_output_socket(struct wine_log_output *output, const WCHAR *path)
{
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    WSADATA data;
    ULONG nonblocking = 1;

    if (!WideCharToMultiByte(CP_ACP, 0, path, -1, address.sun_path, sizeof(address.sun_path), NULL, NULL))
        return FALSE;
    if (WSAStartup(MAKEWORD(2, 2), &data)) return FALSE;
    output->socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (output->socket == INVALID_SOCKET)
    {
        WSACleanup();
        return FALSE;
    }
    if (ioctlsocket(output->socket, FIONBIO, &nonblocking)) goto failed;
    if (connect(output->socket, (struct sockaddr *)&address, sizeof(address)) &&
        (WSAGetLastError() != WSAEWOULDBLOCK || !wait_output_socket(output))) goto failed;
    return TRUE;
failed:
    close_output_socket(output);
    return FALSE;
}

/* A configured but unavailable destination still opts in to silent output. */
BOOL wine_log_output_open(struct wine_log_output *output, const WCHAR *variable)
{
    WCHAR *destination, *path, *allocated = NULL;
    DWORD size = GetEnvironmentVariableW(variable, NULL, 0), length;
    BOOL socket_target, opened = FALSE;

    output->file = INVALID_HANDLE_VALUE;
    output->socket = INVALID_SOCKET;
    output->fallback = GetStdHandle(STD_ERROR_HANDLE);
    output->variable = variable;
    output->enabled = size > 1;
    if (!output->enabled) return FALSE;
    if (!(destination = malloc(size * sizeof(WCHAR)))) return TRUE;
    length = GetEnvironmentVariableW(variable, destination, size);
    if (!length || length >= size) { free(destination); return TRUE; }
    socket_target = !wcsncmp(destination, L"unix:", 5);
    path = destination + (socket_target ? 5 : 0);
    if (*path == '/')
    {
        /* The unix device also works when the prefix has no Z: drive. */
        if (!(allocated = malloc((wcslen(path) + 9) * sizeof(WCHAR)))) goto done;
        wcscpy(allocated, L"\\\\?\\unix");
        wcscat(allocated, path);
        path = allocated;
    }
    if (socket_target)
    {
        WCHAR *p;
        for (p = path; *p; p++) if (*p == '/') *p = '\\';
        opened = open_output_socket(output, path);
    }
    else if ((output->file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE)
    {
        opened = TRUE;
    }
done:
    if (!opened) WINE_ERR("Cannot open %ls %s; continuing on stderr.\n", variable, wine_dbgstr_w(destination));
    free(allocated);
    free(destination);
    return TRUE;
}

void wine_log_output_write(struct wine_log_output *output, const char *buffer, DWORD length)
{
    DWORD written;
    int sent;

    while (length)
    {
        if (output->socket != INVALID_SOCKET)
        {
            sent = send(output->socket, buffer, length, 0);
            if (sent > 0) { buffer += sent; length -= sent; continue; }
            if (sent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK && wait_output_socket(output)) continue;
            WINE_ERR("%s socket disconnected or timed out; continuing on stderr.\n", wine_dbgstr_w(output->variable));
            close_output_socket(output);
        }
        if (!WriteFile(output->file != INVALID_HANDLE_VALUE ? output->file : output->fallback, buffer, length, &written, NULL) || !written)
        {
            WINE_MESSAGE("%.*s", (int)length, buffer);
            break;
        }
        buffer += written;
        length -= written;
    }
}

/* Event and stage names are internal ASCII constants; prefix paths are escaped. */
void wine_log_output_event(struct wine_log_output *output, const char *source,
                           const char *event, const char *stage, DWORD code)
{
    const WCHAR *prefix;
    FILETIME ft;
    ULARGE_INTEGER timestamp;
    char *utf8, *escaped = NULL, *line = NULL;
    DWORD size, saved_error = GetLastError();
    int length;

    if (!output->enabled) return;
    prefix = _wgetenv(L"WINEPREFIX");
    if (!prefix) prefix = _wgetenv(L"WINECONFIGDIR");
    if (!prefix) prefix = L"";
    size = WideCharToMultiByte(CP_UTF8, 0, prefix, -1, NULL, 0, NULL, NULL);
    if (!(utf8 = malloc(size))) goto done;
    WideCharToMultiByte(CP_UTF8, 0, prefix, -1, utf8, size, NULL, NULL);
    escaped = wine_json_escape(utf8);
    free(utf8);
    if (!escaped) goto done;
    size = strlen(escaped) + strlen(source) + strlen(event) + strlen(stage) + 256;
    if (!(line = malloc(size))) goto done;
    GetSystemTimeAsFileTime(&ft);
    timestamp.LowPart = ft.dwLowDateTime;
    timestamp.HighPart = ft.dwHighDateTime;
    length = snprintf(line, size,
                      "{\"version\":1,\"source\":\"%s\",\"event\":\"%s\",\"stage\":\"%s\","
                      "\"time_unix_ms\":%I64u,\"tick_ms\":%I64u,\"pid_windows\":%lu,"
                      "\"prefix\":\"%s\",\"code\":%lu}\n",
                      source, event, stage, (timestamp.QuadPart - 116444736000000000) / 10000,
                      GetTickCount64(), GetCurrentProcessId(), escaped, code);
    if (length > 0 && length < size) wine_log_output_write(output, line, length);
done:
    free(line);
    free(escaped);
    SetLastError(saved_error);
}

#endif
