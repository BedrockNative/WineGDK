/* Local Xodus protocol transport. SPDX-License-Identifier: LGPL-2.1-or-later */
#include "winsock2.h"
#include "afunix.h"
#include "webcore_private.h"
#include "winreg.h"
#include "appmodel.h"
#include <ctype.h>
#include "xmllite.h"
#include "shlwapi.h"
WINE_DEFAULT_DEBUG_CHANNEL(onlineid);
void xodus_token_clear(struct xodus_token *token)
{
    WindowsDeleteString(token->sandbox); WindowsDeleteString(token->signature); WindowsDeleteString(token->token); WindowsDeleteString(token->expiry); WindowsDeleteString(token->puid);
    WindowsDeleteString(token->user_name); WindowsDeleteString(token->xuid); WindowsDeleteString(token->gamertag); WindowsDeleteString(token->user_hash);
    WindowsDeleteString(token->age_group);
    WindowsDeleteString(token->modern_gamertag);
    WindowsDeleteString(token->modern_gamertag_suffix);
    WindowsDeleteString(token->unique_modern_gamertag);
    WindowsDeleteString(token->privileges);
    WindowsDeleteString(token->enforcement);
    WindowsDeleteString(token->restrictions);
    WindowsDeleteString(token->title_restrictions);
    memset(token,0,sizeof(*token));
}
static HRESULT read_token(const BYTE *data,UINT size,struct xodus_token *token)
{
    IXmlReader *reader=NULL; IStream *stream=NULL; XmlNodeType type; HRESULT hr;
    const WCHAR *name,*value; UINT length,depth; HSTRING *field=NULL; BOOL root=FALSE;
    if(!(stream=SHCreateMemStream(data,size))) return E_OUTOFMEMORY;
    hr=CreateXmlReader(&IID_IXmlReader,(void **)&reader,NULL);
    if(SUCCEEDED(hr)) hr=IXmlReader_SetProperty(reader,XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit);
    if(SUCCEEDED(hr)) hr=IXmlReader_SetInput(reader,(IUnknown *)stream);
    while(SUCCEEDED(hr) && (hr=IXmlReader_Read(reader,&type))==S_OK) {
        IXmlReader_GetDepth(reader,&depth);
        if(type==XmlNodeType_Element) {
            IXmlReader_GetLocalName(reader,&name,&length); field=NULL;
            if(!depth) { if(root || wcscmp(name,L"XboxTokenResponse")) { hr=E_INVALIDARG; break; } root=TRUE; }
            else if(depth==1) {
                if(!wcscmp(name,L"Token")) field=&token->token;
                else if(!wcscmp(name,L"Sandbox")) field=&token->sandbox;
                else if(!wcscmp(name,L"Signature")) field=&token->signature;
                else if(!wcscmp(name,L"Expiry")) field=&token->expiry;
                else if(!wcscmp(name,L"Puid")) field=&token->puid;
                else if(!wcscmp(name,L"UserName")) field=&token->user_name;
                else if(!wcscmp(name,L"Xuid")) field=&token->xuid;
                else if(!wcscmp(name,L"Gamertag")) field=&token->gamertag;
                else if(!wcscmp(name,L"UserHash")) field=&token->user_hash;
                else if(!wcscmp(name,L"AgeGroup")) field=&token->age_group;
                else if(!wcscmp(name,L"ModernGamertag")) field=&token->modern_gamertag;
                else if(!wcscmp(name,L"ModernGamertagSuffix")) field=&token->modern_gamertag_suffix;
                else if(!wcscmp(name,L"UniqueModernGamertag")) field=&token->unique_modern_gamertag;
                else if(!wcscmp(name,L"Privileges")) field=&token->privileges;
                else if(!wcscmp(name,L"Enforcement")) field=&token->enforcement;
                else if(!wcscmp(name,L"Restrictions")) field=&token->restrictions;
                else if(!wcscmp(name,L"TitleRestrictions")) field=&token->title_restrictions;
                if(field && *field) { hr=E_INVALIDARG; break; }
            }
            else { hr=E_INVALIDARG; break; }
        } else if(type==XmlNodeType_Text && field) {
            hr=IXmlReader_GetValue(reader,&value,&length);
            if(SUCCEEDED(hr)) hr=WindowsCreateString(value,length,field);
        } else if(type==XmlNodeType_EndElement) field=NULL;
    }
    if(reader) IXmlReader_Release(reader); IStream_Release(stream);
    if(hr==S_FALSE) hr=root && WindowsGetStringLen(token->token) && WindowsGetStringLen(token->xuid)?S_OK:E_INVALIDARG;
    if(FAILED(hr)) xodus_token_clear(token);
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
static HRESULT get_property(IMap_HSTRING_HSTRING *map,const WCHAR *key,HSTRING *out)
{
    HSTRING name; HRESULT hr=string_from_wide(key,&name);
    if(SUCCEEDED(hr)) { hr=IMap_HSTRING_HSTRING_Lookup(map,name,out); WindowsDeleteString(name); }
    return hr;
}
static BOOL package_title_id(DWORD *title)
{
    WCHAR *path;
    UINT32 length = 0;
    HANDLE file;
    DWORD size, read;
    char data[16384], *value, *end;
    unsigned long long id;
    BOOL found = FALSE;
    if (GetCurrentPackagePath(&length, NULL) != ERROR_INSUFFICIENT_BUFFER) return FALSE;
    if (!(path = malloc((length + 32) * sizeof(WCHAR)))) return FALSE;
    if (!GetCurrentPackagePath(&length, path))
    {
        wcscat(path, L"\\xboxservices.config");
        file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (file != INVALID_HANDLE_VALUE)
        {
            size = GetFileSize(file, NULL);
            if (size < sizeof(data) && ReadFile(file, data, size, &read, NULL) && read == size)
            {
                data[size] = 0;
                /* Xbox services emits TitleId as an unsigned decimal JSON number. */
                if ((value = strstr(data, "\"TitleId\"")))
                {
                    value += 9;
                    while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') ++value;
                    if (*value++ == ':')
                    {
                        while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') ++value;
                        if (*value >= '0' && *value <= '9')
                        {
                            id = strtoull(value, &end, 10);
                            while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
                            if (id && id <= 0xffffffff && (*end == ',' || *end == '}'))
                            { *title = id; found = TRUE; }
                        }
                    }
                }
            }
            CloseHandle(file);
        }
    }
    free(path);
    return found;
}

/* Older packaged Xbox clients leave WebTokenRequest.ClientId empty, relying on
 * the Windows broker's package registration. Loose packages have no such catalog.
 * Recognize an unambiguous legacy public OAuth ID embedded in their image; never
 * inspect user credentials or select one when multiple IDs are present. */
static BOOL public_client_id(const WCHAR *executable, WCHAR *client)
{
    HANDLE file, mapping;
    LARGE_INTEGER size;
    const BYTE *data;
    SIZE_T i, j;
    char candidate[17] = {0};
    BOOL found = FALSE;
    file = CreateFileW(executable, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 16 || size.QuadPart > 1024 * 1024 * 1024)
    { CloseHandle(file); return FALSE; }
    mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    CloseHandle(file);
    if (!mapping) return FALSE;
    if ((data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0)))
    {
        for (i = 0; i + 16 <= size.QuadPart; ++i)
        {
            if (memcmp(data + i, "000000004", 9)) continue;
            if (i && isxdigit(data[i - 1])) continue;
            for (j = 9; j < 16 && isxdigit(data[i + j]); ++j) {}
            if (j != 16 || (i + 16 < size.QuadPart && isxdigit(data[i + 16]))) continue;
            if (*candidate && memcmp(candidate, data + i, 16)) { found = FALSE; break; }
            memcpy(candidate, data + i, 16);
            found = TRUE;
            i += 15;
        }
        UnmapViewOfFile(data);
    }
    CloseHandle(mapping);
    if (found) for (i = 0; i <= 16; ++i) client[i] = candidate[i];
    return found;
}

static HRESULT app_identity(IWebTokenRequest *request, WCHAR *client, DWORD *title)
{
    WCHAR path[MAX_PATH], key[MAX_PATH+64], *exe;
    HSTRING client_id = NULL;
    UINT32 length;
    const WCHAR *value;
    DWORD size;
    LONG status;
    BOOL have_client = FALSE, have_title = package_title_id(title);
    client[0] = 0;
    if (SUCCEEDED(IWebTokenRequest_get_ClientId(request, &client_id)))
    {
        value = WindowsGetStringRawBuffer(client_id, &length);
        if (length && length < 64)
        { memcpy(client, value, (length + 1) * sizeof(WCHAR)); have_client = TRUE; }
        WindowsDeleteString(client_id);
    }
    if (have_client && have_title) return S_OK;
    /* Explicit overrides remain available for diagnostic programs without a package. */
    if(!GetModuleFileNameW(NULL,path,ARRAY_SIZE(path))) return HRESULT_FROM_WIN32(GetLastError());
    exe=wcsrchr(path,'\\'); exe=exe?exe+1:path;
    swprintf(key,ARRAY_SIZE(key),L"Software\\Wine\\AppDefaults\\%s",exe);
    if (!have_client)
    {
        size=64*sizeof(WCHAR);
        status=RegGetValueW(HKEY_CURRENT_USER,key,L"XboxClientId",RRF_RT_REG_SZ,NULL,client,&size);
        if (status && !public_client_id(path, client))
        {
            client[0] = 0;
            /* Let the host broker resolve older package registrations. */
            if (!have_title || (status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND)) return HRESULT_FROM_WIN32(status);
        }
    }
    if (have_title) return S_OK;
    size=sizeof(*title);
    status=RegGetValueW(HKEY_CURRENT_USER,key,L"XboxTitleId",RRF_RT_REG_DWORD,NULL,title,&size);
    return HRESULT_FROM_WIN32(status);
}
/* These protocol values are escaped even though the current Xbox relying party is a URL. */
static char *xml_escape(HSTRING string)
{
    const WCHAR *wide=WindowsGetStringRawBuffer(string,NULL); char *utf8,*out,*p; int size,i;
    size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,NULL,0,NULL,NULL);
    if(!size || size>8192) return NULL;
    if(!(utf8=malloc(size))) return NULL;
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,utf8,size,NULL,NULL);
    if(!(out=malloc(size*6))) { free(utf8); return NULL; }
    p=out;
    for(i=0;i<size-1;i++) {
        const char *entity=NULL;
        switch(utf8[i]) { case '&': entity="&amp;"; break; case '<': entity="&lt;"; break; case '>': entity="&gt;"; break; case '"': entity="&quot;"; break; case '\'': entity="&apos;"; break; }
        if(entity) { strcpy(p,entity); p+=strlen(entity); } else *p++=utf8[i];
    }
    *p=0; free(utf8); return out;
}
HRESULT xodus_request(IWebTokenRequest *request,struct xodus_token *token)
{
    IMap_HSTRING_HSTRING *properties=NULL; HSTRING url=NULL,target=NULL,policy=NULL,client_string=NULL,method=NULL,headers=NULL,family_string=NULL;
    WCHAR client[64],family[128]; UINT32 family_length=ARRAY_SIZE(family); DWORD title=0,timeout=45000; WSADATA wsa; SOCKET socket_fd=INVALID_SOCKET;
    SOCKADDR_UN address={AF_UNIX}; char runtime[512],socket_name[100],socket_path[600],*escaped_url=NULL,*escaped_client=NULL,*escaped_method=NULL,*escaped_headers=NULL,*escaped_family=NULL,*xml=NULL;
    BYTE header[8],*data=NULL; UINT size; HRESULT hr; BOOL started=FALSE;
    memset(token,0,sizeof(*token));
    if(FAILED(hr=IWebTokenRequest_get_Properties(request,&properties))) goto done;
    if(FAILED(hr=get_property(properties,L"Url",&url))) goto done;
    if(FAILED(hr=get_property(properties,L"Target",&target))) goto done;
    if(FAILED(hr=get_property(properties,L"Policy",&policy))) goto done;
    if(wcscmp(WindowsGetStringRawBuffer(target,NULL),L"user.auth.xboxlive.com") || wcscmp(WindowsGetStringRawBuffer(policy,NULL),L"MBI_SSL")) { hr=E_NOTIMPL; goto done; }
    if(FAILED(hr=app_identity(request,client,&title))) goto done;
    if (GetCurrentPackageFamilyName(&family_length, family)) family[0] = 0;
    if (FAILED(hr = string_from_wide(family, &family_string))) goto done;
    if (!(escaped_family = xml_escape(family_string))) { hr = E_OUTOFMEMORY; goto done; }
    if(FAILED(hr=string_from_wide(client,&client_string))) goto done;
    if(!(escaped_url=xml_escape(url)) || !(escaped_client=xml_escape(client_string))) { hr=E_OUTOFMEMORY; goto done; }
    hr=get_property(properties,L"HttpMethod",&method);
    if(FAILED(hr) && hr!=E_BOUNDS) goto done;
    hr=get_property(properties,L"RequestHeaders",&headers);
    if(FAILED(hr) && hr!=E_BOUNDS) goto done;
    if(!(escaped_method=xml_escape(method)) || !(escaped_headers=xml_escape(headers))) { hr=E_OUTOFMEMORY; goto done; }
    size=strlen(escaped_url)+strlen(escaped_client)+strlen(escaped_method)+strlen(escaped_headers)+strlen(escaped_family)+384;
    if(!(xml=malloc(size))) { hr=E_OUTOFMEMORY; goto done; }
    size=snprintf(xml,size,"<XboxTokenRequest><ClientId>%s</ClientId><TitleId>%lu</TitleId><PackageFamilyName>%s</PackageFamilyName><RelyingParty>%s</RelyingParty><HttpMethod>%s</HttpMethod><RequestHeaders>%s</RequestHeaders></XboxTokenRequest>",escaped_client,title,escaped_family,escaped_url,escaped_method,escaped_headers);
    if(size>65535) { hr=E_INVALIDARG; goto done; }
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
    memcpy(header,"XSDX",4); header[4]=7; header[5]=0; header[6]=size; header[7]=size>>8;
    hr=socket_transfer(socket_fd,(char *)header,sizeof(header),TRUE);
    if(SUCCEEDED(hr)) hr=socket_transfer(socket_fd,xml,size,TRUE);
    if(SUCCEEDED(hr)) hr=socket_transfer(socket_fd,(char *)header,sizeof(header),FALSE);
    if(FAILED(hr)) goto done;
    if(memcmp(header,"XSDX",4) || header[4]!=8 || header[5]) { hr=E_INVALIDARG; goto done; }
    size=header[6]|(header[7]<<8);
    if(!size) { hr=HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE); goto done; }
    if(!(data=malloc(size))) { hr=E_OUTOFMEMORY; goto done; }
    if(SUCCEEDED(hr=socket_transfer(socket_fd,(char *)data,size,FALSE))) hr=read_token(data,size,token);
    SecureZeroMemory(data,size);
done:
    TRACE("Xodus Xbox exchange completed: %#lx\n",hr);
    if(socket_fd!=INVALID_SOCKET) closesocket(socket_fd); if(started) WSACleanup();
    free(escaped_method); free(escaped_headers); WindowsDeleteString(method); WindowsDeleteString(headers);
    free(escaped_family); WindowsDeleteString(family_string);
    free(data); free(xml); free(escaped_url); free(escaped_client);
    WindowsDeleteString(url); WindowsDeleteString(target); WindowsDeleteString(policy); WindowsDeleteString(client_string);
    if(properties) IMap_HSTRING_HSTRING_Release(properties); return hr;
}
