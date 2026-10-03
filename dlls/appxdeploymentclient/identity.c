/* Process identity for unpacked applications with an AppxManifest.xml.
 * Copyright 2026 WineGDK contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "windef.h"
#include "winbase.h"
#include "appmodel.h"
#include "shlwapi.h"
#include "bcrypt.h"
#include "initguid.h"
#include "xmllite.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(appx);
#define PATH_CAPACITY 32768

struct package_identity
{
    WCHAR path[PATH_CAPACITY], name[128], publisher[2048], version[32], arch[16], resource[128];
    WCHAR application[128], family[160], full[320], aumid[320];
};
static struct package_identity *identity;
static INIT_ONCE identity_once = INIT_ONCE_STATIC_INIT;

static BOOL attribute(IXmlReader *reader, const WCHAR *name, WCHAR *out, UINT capacity)
{
    const WCHAR *value;
    UINT length;
    BOOL ret = FALSE;
    *out = 0;
    if (IXmlReader_MoveToAttributeByName(reader, name, NULL) != S_OK) return FALSE;
    if (SUCCEEDED(IXmlReader_GetValue(reader, &value, &length)) && length < capacity)
    {
        memcpy(out, value, length * sizeof(WCHAR));
        out[length] = 0;
        ret = TRUE;
    }
    IXmlReader_MoveToElement(reader);
    return ret;
}

static BOOL parse_manifest(const WCHAR *root, const WCHAR *exe, struct package_identity *parsed)
{
    IXmlReader *reader = NULL;
    IStream *stream = NULL;
    WCHAR *path, relative[1024], publisher_id[14];
    const WCHAR *name, *ns;
    XmlNodeType type;
    UINT depth, i, bit, value, length;
    BYTE hash[32];
    BOOL found = FALSE, have_identity = FALSE;
    HRESULT hr = E_FAIL;

    if (!(path = malloc(PATH_CAPACITY * sizeof(WCHAR)))) return FALSE;
    if (wcslen(root) + 18 >= PATH_CAPACITY) goto done;
    swprintf(path, PATH_CAPACITY, L"%s\\AppxManifest.xml", root);
    if (FAILED(hr = SHCreateStreamOnFileEx(path, STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, NULL, &stream))) goto done;
    if (FAILED(hr = CreateXmlReader(&IID_IXmlReader, (void **)&reader, NULL))) goto done;
    if (FAILED(hr = IXmlReader_SetProperty(reader, XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit))) goto done;
    if (FAILED(hr = IXmlReader_SetInput(reader, (IUnknown *)stream))) goto done;
    memset(parsed, 0, sizeof(*parsed));
    while ((hr = IXmlReader_Read(reader, &type)) == S_OK)
    {
        if (type != XmlNodeType_Element) continue;
        IXmlReader_GetDepth(reader, &depth);
        IXmlReader_GetLocalName(reader, &name, NULL);
        IXmlReader_GetNamespaceUri(reader, &ns, NULL);
        if (wcscmp(ns, L"http://schemas.microsoft.com/appx/manifest/foundation/windows10") &&
            wcscmp(ns, L"http://schemas.microsoft.com/appx/2010/manifest")) continue;
        if (depth == 1 && !wcscmp(name, L"Identity"))
        {
            have_identity = attribute(reader, L"Name", parsed->name, ARRAY_SIZE(parsed->name)) &&
                            attribute(reader, L"Publisher", parsed->publisher, ARRAY_SIZE(parsed->publisher)) &&
                            attribute(reader, L"Version", parsed->version, ARRAY_SIZE(parsed->version));
            if (!attribute(reader, L"ProcessorArchitecture", parsed->arch, ARRAY_SIZE(parsed->arch)))
                wcscpy(parsed->arch, L"neutral");
            attribute(reader, L"ResourceId", parsed->resource, ARRAY_SIZE(parsed->resource));
        }
        else if (depth == 2 && !wcscmp(name, L"Application") &&
                 attribute(reader, L"Executable", relative, ARRAY_SIZE(relative)))
        {
            if (!*relative || *relative == '/' || *relative == '\\' || wcschr(relative, ':')) continue;
            for (i = 0; relative[i]; ++i) if (relative[i] == '/') relative[i] = '\\';
            if (wcslen(root) + wcslen(relative) + 2 >= PATH_CAPACITY) continue;
            swprintf(path, PATH_CAPACITY, L"%s\\%s", root, relative);
            /* Executable is package-relative; resolve dot components before comparing. */
            length = GetFullPathNameW(path, PATH_CAPACITY, path, NULL);
            if (length && length < PATH_CAPACITY && !wcsicmp(path, exe) &&
                attribute(reader, L"Id", parsed->application, ARRAY_SIZE(parsed->application))) found = TRUE;
        }
    }
    if (hr != S_FALSE || !found || !have_identity || !*parsed->name || !*parsed->publisher || !*parsed->application)
    { found = FALSE; goto done; }
    /* PublisherId uses the first 64 SHA-256 bits, encoded with the package alphabet. */
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, NULL, 0, (BYTE *)parsed->publisher,
                   wcslen(parsed->publisher) * sizeof(WCHAR), hash, sizeof(hash)))
    { found = FALSE; goto done; }
    for (i = 0; i < 13; ++i)
    {
        value = 0;
        for (bit = 0; bit < 5; ++bit)
        {
            UINT index = i * 5 + bit;
            value = (value << 1) | (index < 64 ? ((hash[index / 8] >> (7 - index % 8)) & 1) : 0);
        }
        publisher_id[i] = L"0123456789abcdefghjkmnpqrstvwxyz"[value];
    }
    publisher_id[13] = 0;
    wcscpy(parsed->path, root);
    swprintf(parsed->family, ARRAY_SIZE(parsed->family), L"%s_%s", parsed->name, publisher_id);
    swprintf(parsed->full, ARRAY_SIZE(parsed->full), L"%s_%s_%s_%s_%s", parsed->name,
             parsed->version, parsed->arch, parsed->resource, publisher_id);
    swprintf(parsed->aumid, ARRAY_SIZE(parsed->aumid), L"%s!%s", parsed->family, parsed->application);
    TRACE("Discovered package %s, application %s.\n", debugstr_w(parsed->full), debugstr_w(parsed->application));
done:
    if (reader) IXmlReader_Release(reader);
    if (stream) IStream_Release(stream);
    free(path);
    return found && hr == S_FALSE;
}

static BOOL CALLBACK discover_identity(INIT_ONCE *once, void *param, void **context)
{
    WCHAR *exe, *root, *separator;
    struct package_identity *parsed;
    if (!(exe = malloc(2 * PATH_CAPACITY * sizeof(WCHAR)))) return FALSE;
    if (!(parsed = calloc(1, sizeof(*parsed)))) { free(exe); return FALSE; }
    root = exe + PATH_CAPACITY;
    if (GetModuleFileNameW(NULL, exe, PATH_CAPACITY) < PATH_CAPACITY)
    {
        wcscpy(root, exe);
        while ((separator = wcsrchr(root, '\\')) && separator > root + 2)
        {
            *separator = 0;
            if (parse_manifest(root, exe, parsed)) { identity = parsed; break; }
        }
    }
    if (!identity) free(parsed);
    free(exe);
    return TRUE;
}

LONG WINAPI __wine_get_package_property(const WCHAR *property, UINT32 *length, WCHAR *buffer)
{
    const WCHAR *value;
    UINT32 capacity;
    if (!length || (*length && !buffer)) return ERROR_INVALID_PARAMETER;
    if (!InitOnceExecuteOnce(&identity_once, discover_identity, NULL, NULL)) return ERROR_NOT_ENOUGH_MEMORY;
    if (!identity) return APPMODEL_ERROR_NO_PACKAGE;
    if (!wcscmp(property, L"PackageFamilyName")) value = identity->family;
    else if (!wcscmp(property, L"PackageFullName")) value = identity->full;
    else if (!wcscmp(property, L"PackagePublisher")) value = identity->publisher;
    else if (!wcscmp(property, L"Path")) value = identity->path;
    else if (!wcscmp(property, L"ApplicationUserModelId")) value = identity->aumid;
    else return ERROR_INVALID_PARAMETER;
    capacity = *length;
    *length = wcslen(value) + 1;
    if (capacity < *length) return ERROR_INSUFFICIENT_BUFFER;
    memcpy(buffer, value, *length * sizeof(WCHAR));
    return ERROR_SUCCESS;
}
