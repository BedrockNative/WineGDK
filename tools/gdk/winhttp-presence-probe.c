/* Regression coverage for signed request payload integrity.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char payload[] = "{\"state\":\"active\",\"activity\":{\"description\":\"untouched request body\"}}";
int main(int argc, char **argv)
{
    HINTERNET session, connection, request;
    DWORD status, size;
    unsigned mode, failures = 0;
    if (argc != 2) return 2;
    session = WinHttpOpen(L"WineGDK regression", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    connection = WinHttpConnect(session, L"127.0.0.1", atoi(argv[1]), 0);
    if (!session || !connection) return 2;
    for (mode = 0; mode < 3; ++mode)
    {
        BOOL ok;
        request = WinHttpOpenRequest(connection, L"POST", L"/users/xuid(123)/devices/current/titles/current", NULL, NULL, NULL, 0);
        ok = WinHttpSendRequest(request, L"Content-Type: application/example\r\nx-xbl-contract-version: 42\r\n", -1,
                               mode ? NULL : (void *)payload, mode ? 0 : sizeof(payload) - 1, sizeof(payload) - 1, 0);
        if (ok && mode == 1) ok = WinHttpWriteData(request, payload, sizeof(payload) - 1, &size);
        if (ok && mode == 2)
        {
            /* This first chunk used to be replaced and expanded to the total length. */
            ok = WinHttpWriteData(request, payload, 40, &size) && size == 40;
            if (ok) ok = WinHttpWriteData(request, payload + 40, sizeof(payload) - 1 - 40, &size);
        }
        if (ok) ok = WinHttpReceiveResponse(request, NULL);
        status = 0; size = sizeof(status);
        if (ok) ok = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
        if (!ok || status != 200) { ++failures; printf("FAIL mode %u: status %lu, error %lu\n", mode, status, GetLastError()); }
        WinHttpCloseHandle(request);
    }
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    printf("3 payload/header integrity cases, %u failures\n", failures);
    return !!failures;
}
