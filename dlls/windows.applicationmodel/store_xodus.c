/* Store bridge to the local Xodus service. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <ctype.h>
#include "winsock2.h"
#include "afunix.h"
#include "store_private.h"
#include "appmodel.h"
#include "initguid.h"
#include "xmllite.h"
#include "shlwapi.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(model);
void store_info_clear(struct store_info *info)
{
    unsigned int i;
    for (i = 0; i < STORE_FIELD_COUNT; ++i) WindowsDeleteString(info->fields[i]);
    memset(info, 0, sizeof(*info));
}
static HRESULT append_text(HSTRING *field, const WCHAR *text, UINT length)
{
    UINT old_size;
    const WCHAR *old = WindowsGetStringRawBuffer(*field, &old_size);
    HSTRING replacement;
    WCHAR *buffer;
    HRESULT hr;
    if (length > 65535 || old_size > 65535 - length) return E_INVALIDARG;
    if (!(buffer = malloc((old_size + length + 1) * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    memcpy(buffer, old, old_size * sizeof(WCHAR));
    memcpy(buffer + old_size, text, length * sizeof(WCHAR));
    hr = WindowsCreateString(buffer, old_size + length, &replacement);
    free(buffer);
    if (SUCCEEDED(hr)) { WindowsDeleteString(*field); *field = replacement; }
    return hr;
}
static HRESULT read_store(const BYTE *data, UINT size, struct store_info *info)
{
    static const WCHAR *const names[] = {L"Status", L"AppId", L"StoreId", L"PackageFamilyName", L"Receipt",
        L"IsActive", L"IsTrial", L"ExpirationDate", L"Name", L"Description", L"FormattedPrice", L"Currency", L"AgeRating", L"CurrentMarket"};
    IXmlReader *reader = NULL;
    IStream *stream;
    XmlNodeType type;
    const WCHAR *name, *value;
    WCHAR *end;
    HSTRING *field = NULL;
    UINT depth, length, i, seen = 0;
    BOOL root = FALSE;
    HRESULT hr;
    if (!(stream = SHCreateMemStream(data, size))) return E_OUTOFMEMORY;
    hr = CreateXmlReader(&IID_IXmlReader, (void **)&reader, NULL);
    if (SUCCEEDED(hr)) hr = IXmlReader_SetProperty(reader, XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
    if (SUCCEEDED(hr)) hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    while (SUCCEEDED(hr) && (hr = IXmlReader_Read(reader, &type)) == S_OK)
    {
        IXmlReader_GetDepth(reader, &depth);
        if (type == XmlNodeType_Element)
        {
            field = NULL;
            IXmlReader_GetLocalName(reader, &name, &length);
            if (!depth)
            {
                if (root || wcscmp(name, L"StoreResponse")) { hr = E_INVALIDARG; break; }
                root = TRUE;
            }
            else if (depth == 1)
            {
                for (i = 0; i < ARRAY_SIZE(names); ++i) if (!wcscmp(name, names[i])) break;
                if (i < ARRAY_SIZE(names))
                {
                    if (seen & (1u << i)) { hr = E_INVALIDARG; break; }
                    seen |= 1u << i; field = &info->fields[i];
                }
            }
            else { hr = E_INVALIDARG; break; }
        }
        else if ((type == XmlNodeType_Text || type == XmlNodeType_CDATA) && field)
        {
            hr = IXmlReader_GetValue(reader, &value, &length);
            if (SUCCEEDED(hr)) hr = append_text(field, value, length);
        }
        else if (type == XmlNodeType_EndElement) field = NULL;
    }
    if (reader) IXmlReader_Release(reader);
    IStream_Release(stream);
    if (hr == S_FALSE)
    {
        value = WindowsGetStringRawBuffer(info->fields[STORE_STATUS], &length);
        if (!root || !length) hr = E_INVALIDARG;
        else { hr = wcstoul(value, &end, 10); if (*end) hr = E_INVALIDARG; }
    }
    if (FAILED(hr)) store_info_clear(info);
    return hr;
}
static HRESULT socket_transfer(SOCKET socket,char *data,int size,BOOL write)
{
    int count;
    while(size) {
        count=write?send(socket,data,size,0):recv(socket,data,size,0);
        if(count==SOCKET_ERROR) return HRESULT_FROM_WIN32(WSAGetLastError());
        if(!count) return HRESULT_FROM_WIN32(ERROR_BROKEN_PIPE);
        data+=count; size-=count;
    }
    return S_OK;
}
HRESULT store_request(const WCHAR *operation, struct store_info *info)
{
    WCHAR pfn[256], market[8], language[LOCALE_NAME_MAX_LENGTH];
    char family_utf8[256], market_utf8[16], language_utf8[128], operation_utf8[32];
    char runtime[512], socket_name[100], socket_path[600], xml[640];
    UINT32 family_size = ARRAY_SIZE(pfn);
    DWORD timeout = 45000;
    WSADATA wsa;
    SOCKET socket_fd = INVALID_SOCKET;
    SOCKADDR_UN address = {AF_UNIX};
    BYTE header[8], *data = NULL;
    UINT size, i;
    BOOL started = FALSE;
    HRESULT hr;
    LONG status;
    memset(info, 0, sizeof(*info));
    status = GetCurrentPackageFamilyName(&family_size, pfn);
    if (status) return HRESULT_FROM_WIN32(status);
    if (!GetUserDefaultLocaleName(language, ARRAY_SIZE(language))) wcscpy(language, L"en-US");
    if (!GetLocaleInfoEx(language, LOCALE_SISO3166CTRYNAME, market, ARRAY_SIZE(market))) wcscpy(market, L"US");
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, pfn, -1, family_utf8, sizeof(family_utf8), NULL, NULL) ||
        !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, market, -1, market_utf8, sizeof(market_utf8), NULL, NULL) ||
        !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, language, -1, language_utf8, sizeof(language_utf8), NULL, NULL) ||
        !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, operation, -1, operation_utf8, sizeof(operation_utf8), NULL, NULL)) return E_INVALIDARG;
    /* Only identifier characters are accepted here; XML delimiters are excluded. */
    for (i = 0; family_utf8[i]; ++i) if (!isalnum((unsigned char)family_utf8[i]) && !strchr("._-", family_utf8[i])) return E_INVALIDARG;
    for (i = 0; market_utf8[i]; ++i) if (!isalpha((unsigned char)market_utf8[i])) return E_INVALIDARG;
    for (i = 0; language_utf8[i]; ++i) if (!isalnum((unsigned char)language_utf8[i]) && language_utf8[i] != '-') return E_INVALIDARG;
    if (wcscmp(operation, L"AppReceipt") && wcscmp(operation, L"License") && wcscmp(operation, L"Listing")) return E_INVALIDARG;
    size = snprintf(xml, sizeof(xml), "<StoreRequest><PackageFamilyName>%s</PackageFamilyName><Market>%s</Market><Language>%s</Language><Operation>%s</Operation></StoreRequest>", family_utf8, market_utf8, language_utf8, operation_utf8);
    if (size >= sizeof(xml)) return E_INVALIDARG;
    {
    DWORD socket_length=GetEnvironmentVariableA("XODUS_SOCKET",socket_path,sizeof(socket_path));
    if(socket_length>=sizeof(socket_path)) { hr=E_INVALIDARG; goto done; }
    if(!socket_length) {
        DWORD length=GetEnvironmentVariableA("XDG_RUNTIME_DIR",runtime,sizeof(runtime));
        if(!length) length=GetEnvironmentVariableA("WINE_HOST_XDG_RUNTIME_DIR",runtime,sizeof(runtime));
        if(!length || length>=sizeof(runtime)) { hr=HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND); goto done; }
        length=GetEnvironmentVariableA("XODUS_SOCK_NAME",socket_name,sizeof(socket_name));
        if(!length) strcpy(socket_name,"xodus.sock");
        else if(length>=sizeof(socket_name)) { hr=E_INVALIDARG; goto done; }
        snprintf(socket_path,sizeof(socket_path),"%s/%s",runtime,socket_name);
    }
    }
    if(strlen(socket_path)+3>=sizeof(address.sun_path)) { hr=HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE); goto done; }
    snprintf(address.sun_path,sizeof(address.sun_path),"%s%s",socket_path[0]=='/'?"Z:":"",socket_path);
    if(WSAStartup(MAKEWORD(2,2),&wsa)) { hr=E_FAIL; goto done; } started=TRUE;
    socket_fd=socket(AF_UNIX,SOCK_STREAM,0);
    if(socket_fd==INVALID_SOCKET) { hr=HRESULT_FROM_WIN32(WSAGetLastError()); goto done; }
    setsockopt(socket_fd,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout,sizeof(timeout));
    setsockopt(socket_fd,SOL_SOCKET,SO_SNDTIMEO,(char *)&timeout,sizeof(timeout));
    if(connect(socket_fd,(struct sockaddr *)&address,sizeof(address))) { hr=HRESULT_FROM_WIN32(WSAGetLastError()); goto done; }
    memcpy(header,"XSDX",4); header[4]=9; header[5]=0; header[6]=size; header[7]=size>>8;
    hr=socket_transfer(socket_fd,(char *)header,sizeof(header),TRUE);
    if(SUCCEEDED(hr)) hr=socket_transfer(socket_fd,xml,size,TRUE);
    if(SUCCEEDED(hr)) hr=socket_transfer(socket_fd,(char *)header,sizeof(header),FALSE);
    if(FAILED(hr)) goto done;
    if(memcmp(header,"XSDX",4) || header[4]!=10 || header[5]) { hr=E_INVALIDARG; goto done; }
    size=header[6]|(header[7]<<8);
    if(!size) { hr=HRESULT_FROM_WIN32(ERROR_INVALID_DATA); goto done; }
    if(!(data=malloc(size))) { hr=E_OUTOFMEMORY; goto done; }
    if(SUCCEEDED(hr=socket_transfer(socket_fd,(char *)data,size,FALSE))) hr=read_store(data,size,info);
    SecureZeroMemory(data,size);
done:
    if (socket_fd != INVALID_SOCKET) closesocket(socket_fd);
    if (started) WSACleanup();
    free(data);
    if (SUCCEEDED(hr) && wcscmp(WindowsGetStringRawBuffer(info->fields[STORE_PFN], NULL), pfn))
    { store_info_clear(info); hr = E_INVALIDARG; }
    TRACE("Xodus Store %s completed: %#lx\n", debugstr_w(operation), hr);
    return hr;
}
