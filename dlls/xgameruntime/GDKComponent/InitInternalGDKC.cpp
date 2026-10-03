/*
 * Xbox Game runtime Library
 *  GDK Component: Internal Initialization
 * 
 * Written by Weather
 * Copyright 2026 Olivia Ryan
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

#include "InitInternalGDKC.h"
#include "../WineCoreUAP/Foundation/IWineAsync.hpp"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <ntstatus.h>
#include <shlwapi.h>
#include <errno.h>
#include <limits.h>

WINE_DEFAULT_DEBUG_CHANNEL(gdkct);

using namespace ABI::Windows::Foundation;

BOOLEAN initializeCalled = FALSE;
BOOLEAN xodusAvailable = FALSE;
static SRWLOCK initialize_lock = SRWLOCK_INIT;

char *msaAppId;
UINT32 titleId;
BOOLEAN fullTrust;

#if XODUS_INTEROP
static HRESULT WINAPI InitializeXodusService()
{
    IAsyncAction *action = nullptr;
    HRESULT hr;
    DWORD wait;
    NTSTATUS status;

    if ((status = __wine_init_unix_call())) return HRESULT_FROM_NT(status);
    status = __wine_unix_call(__wine_unixlib_handle, conn_socket, (void *)XODUS_SOCKET_SUFFIX);
    if (status) return HRESULT_FROM_NT(status);
    unixhandle = __wine_unixlib_handle;
    if (FAILED(hr = xodus_ipclayer->InitializeSocket())) return hr;
    if (FAILED(hr = xodus_service->Ping(&action))) return hr;
    wait = AsyncActionCompletedHandler::await_AsyncAction(action, IPC_REQUEST_TIMEOUT_MS);
    if (!wait) hr = action->GetResults();
    else hr = wait == STATUS_TIMEOUT ? HRESULT_FROM_WIN32(ERROR_TIMEOUT) : E_FAIL;
    action->Release();
    return hr;
}
#endif

static HRESULT WINAPI ObtainMsaAppId(INITIALIZE_OPTIONS *options)
{
    CHAR filename[MAX_PATH], *last, *app_id = nullptr;
    UINT32 parsed_title_id = 0;
    BOOLEAN parsed_full_trust = FALSE;
    xmlNodePtr root, child;
    xmlDocPtr config;
    HRESULT hr = S_OK;

    if (options)
    {
        if (!options->gameConfig) return E_INVALIDARG;
        if (options->isInlineConfig)
        {
            SIZE_T length = strlen(options->gameConfig);
            if (length > INT_MAX) return E_GAMERUNTIME_GAMECONFIG_BAD_FORMAT;
            config = xmlReadMemory(options->gameConfig, length, nullptr, nullptr, XML_PARSE_NONET);
        }
        else config = xmlReadFile(options->gameConfig, nullptr, XML_PARSE_NONET);
    }
    else
    {
        DWORD length = GetModuleFileNameA(nullptr, filename, ARRAY_SIZE(filename));
        if (!length) return HRESULT_FROM_WIN32(GetLastError());
        if (length >= ARRAY_SIZE(filename)) return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        for (;;)
        {
            if (!(last = strrchr(filename, '\\'))) return E_GAME_MISSING_GAME_CONFIG;
            last[1] = 0;
            if (strlen(filename) + strlen("MicrosoftGame.config") >= ARRAY_SIZE(filename))
                return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            strcat(filename, "MicrosoftGame.config");
            if (PathFileExistsA(filename)) break;
            *last = 0;
        }
        config = xmlReadFile(filename, nullptr, XML_PARSE_NONET);
    }
    if (!config) return E_GAMERUNTIME_GAMECONFIG_BAD_FORMAT;
    root = xmlDocGetRootElement(config);
    if (!root || xmlStrcmp(root->name, BAD_CAST "Game"))
    {
        hr = E_GAMERUNTIME_GAMECONFIG_BAD_FORMAT;
        goto cleanup;
    }
    for (child = root->children; child; child = child->next)
    {
        if (child->type != XML_ELEMENT_NODE) continue;
        if (!xmlStrcmp(child->name, BAD_CAST "MSAAppId"))
        {
            xmlChar *value = xmlNodeGetContent(child);
            if (!value) { hr = E_OUTOFMEMORY; goto cleanup; }
            free(app_id);
            app_id = strdup(reinterpret_cast<char *>(value));
            xmlFree(value);
            if (!app_id) { hr = E_OUTOFMEMORY; goto cleanup; }
        }
        else if (!xmlStrcmp(child->name, BAD_CAST "TitleId"))
        {
            xmlChar *value = xmlNodeGetContent(child);
            char *end;
            unsigned long id;
            if (!value) { hr = E_OUTOFMEMORY; goto cleanup; }
            errno = 0;
            id = strtoul(reinterpret_cast<char *>(value), &end, 16);
            if (errno || xmlStrlen(value) != 8 ||
                strspn(reinterpret_cast<char *>(value), "0123456789abcdefABCDEF") != 8 || *end) hr = E_GAMERUNTIME_GAMECONFIG_BAD_FORMAT;
            parsed_title_id = id;
            xmlFree(value);
            if (FAILED(hr)) goto cleanup;
        }
        else if (!xmlStrcmp(child->name, BAD_CAST "MSAFullTrust"))
        {
            xmlChar *value = xmlNodeGetContent(child);
            if (!value) { hr = E_OUTOFMEMORY; goto cleanup; }
            parsed_full_trust = !xmlStrcmp(value, BAD_CAST "true");
            xmlFree(value);
        }
    }
    free(msaAppId);
    msaAppId = app_id;
    app_id = nullptr;
    titleId = parsed_title_id;
    fullTrust = parsed_full_trust;
cleanup:
    free(app_id);
    xmlFreeDoc(config);
    return hr;
}

HRESULT WINAPI InitializeGDKComponent(INITIALIZE_OPTIONS *options)
{
    HRESULT hr = S_OK;

    AcquireSRWLockExclusive(&initialize_lock);
    if (initializeCalled) goto done;
    if (FAILED(hr = ObtainMsaAppId(options))) goto done;
#if XODUS_INTEROP
    hr = InitializeXodusService();
    xodusAvailable = SUCCEEDED(hr);
    if (FAILED(hr)) WARN("Xodus is unavailable, using the default user provider (hr %#lx).\n", hr);
#endif
    initializeCalled = TRUE;
    hr = S_OK;
done:
    ReleaseSRWLockExclusive(&initialize_lock);
    return hr;
}
