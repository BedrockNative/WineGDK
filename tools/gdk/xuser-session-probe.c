/* XUser startup regression fixture. No real accounts or network calls.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define INITGUID
#define http_request fixture_http_request
#include "../../dlls/xgameruntime/GDKComponent/System/XUser.c"
#undef http_request

char *msaAppId = "0000000040159362";
UINT32 titleId = 896928775;
BOOLEAN fullTrust, xodusAvailable, xodusSessionCacheAvailable;
static char *broker_data;
static LONGLONG broker_expiry;
HRESULT xodus_session_cache(const char *op, const char *puid, LONGLONG expiry,
                            const char *input, char **output, LONGLONG *returned_expiry)
{
    if (output) *output = NULL;
    if (returned_expiry) *returned_expiry = 0;
    if (!xodusSessionCacheAvailable || !puid) return S_FALSE;
    if (!strcmp(op, "put")) { free(broker_data); broker_data = strdup(input); broker_expiry = expiry; return S_OK; }
    if (!strcmp(op, "invalidate")) { free(broker_data); broker_data = NULL; return S_OK; }
    if (strcmp(op, "get") || !broker_data) return S_FALSE;
    *output = strdup(broker_data); *returned_expiry = broker_expiry;
    return S_OK;
}
IXodusService *xodus_service;
HRESULT WINAPI QueryApiImpl(const GUID *clsid, REFIID iid, void **out) { *out = NULL; return E_NOTIMPL; }
static unsigned requests;
static LONG failures, checks;
static BOOL fail_exchange, fail_sisu, expect_title_claims;
static const char *expiration = "2099-01-01T00:00:00.1234567Z";
#define check(c) do { InterlockedIncrement(&checks); if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); InterlockedIncrement(&failures); } } while(0)
static void check_json_field(const char *body, const WCHAR *parent, const WCHAR *field, const WCHAR *expected)
{
    IJsonObject *root = NULL, *object = NULL;
    HSTRING value = NULL;
    check(SUCCEEDED(parse_json(body, strlen(body), &root)));
    if (!root) return;
    if (parent) check(SUCCEEDED(get_json_object(root, parent, &object)));
    else { object = root; IJsonObject_AddRef(root); }
    if (object)
    {
        check(SUCCEEDED(get_json_string(object, field, &value)));
        check(!wcscmp(WindowsGetStringRawBuffer(value, NULL), expected));
        WindowsDeleteString(value);
        IJsonObject_Release(object);
    }
    IJsonObject_Release(root);
}
HRESULT fixture_http_request(const WCHAR *method, const WCHAR *host, const WCHAR *path, char *body,
                            const WCHAR *headers, const WCHAR **accept, UCHAR **buffer, SIZE_T *size)
{
    char data[2048];
    const char *response = NULL;
    ++requests;
    *buffer = NULL; *size = 0;
    if (fail_exchange) return E_FAIL;
    if (!wcscmp(host, L"user.auth.xboxlive.com"))
    {
        snprintf(data, sizeof(data), "{\"Token\":\"fixture-user\",\"NotAfter\":\"%s\"}", expiration);
        response = data;
    }
    else if (!wcscmp(host, L"xsts.auth.xboxlive.com"))
    {
        check(!!strstr(body, "DeviceToken") == expect_title_claims);
        check(!!strstr(body, "TitleToken") == expect_title_claims);
        snprintf(data, sizeof(data), "{\"Token\":\"fixture-xsts\",\"NotAfter\":\"%s\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"1\",\"xid\":\"123\"}]}}", expiration);
        response = data;
    }
    else if (!wcscmp(host, L"title.mgt.xboxlive.com")) response = "{\"SignaturePolicies\":[],\"EndPoints\":[]}";
    else if (!wcscmp(host, L"profile.xboxlive.com")) response = "{\"profileUsers\":[{\"settings\":[{\"value\":\"https://example.invalid/avatar\"},{\"value\":\"Fixture\"},{\"value\":\"Fixture\"},{\"value\":\"\"},{\"value\":\"Fixture\"}]}]}";
    else if (!wcscmp(host, L"device.auth.xboxlive.com"))
    {
        check_json_field(body, L"Properties", L"AuthMethod", L"RPS");
        check_json_field(body, L"Properties", L"RpsTicket", L"device\"\\\n");
        check_json_field(body, L"Properties", L"SiteName", L"user.auth.xboxlive.com");
        check(headers && wcsstr(headers, L"Signature: "));
        response = "{\"Token\":\"fixture-device\"}";
    }
    else if (!wcscmp(host, L"sisu.xboxlive.com"))
    {
        check_json_field(body, NULL, L"AccessToken", L"RST2%&");
        check_json_field(body, NULL, L"AppId", L"0000000040159362");
        check_json_field(body, NULL, L"DeviceToken", L"fixture-device");
        check_json_field(body, NULL, L"SiteName", L"user.auth.xboxlive.com");
        check(headers && wcsstr(headers, L"Signature: "));
        if (fail_sisu) return E_FAIL;
        response = "{\"TitleToken\":{\"Token\":\"fixture-title\"}}";
    }
    else { check(0); return E_FAIL; }
    *size = strlen(response);
    *buffer = (UCHAR *)strdup(response);
    return *buffer ? S_OK : E_OUTOFMEMORY;
}
static XUserHandle load(const char *token, const char *puid)
{
    XUserHandle user = NULL;
    HRESULT hr = LoadMsaUser(token, puid, NULL, &user);
    check(hr == S_OK && user);
    return user;
}
static DWORD WINAPI concurrent_load(void *out)
{
    RoInitialize(RO_INIT_MULTITHREADED);
    *(XUserHandle *)out = load("renewed-credential", "account-a");
    RoUninitialize();
    return 0;
}
static void test_token_urls(void)
{
    static const struct
    {
        const char *url, *host, *path, *query;
        INTERNET_SCHEME scheme;
        INTERNET_PORT port;
    } cases[] =
    {
        {"wss://rta.xboxlive.com/connect", "rta.xboxlive.com", "/connect", "", INTERNET_SCHEME_HTTPS, 443},
        {"WSS://rta.xboxlive.com:8443/connect?value=a%2Fb", "rta.xboxlive.com", "/connect", "?value=a%2Fb", INTERNET_SCHEME_HTTPS, 8443},
        {"ws://localhost:8080/events?q=1", "localhost", "/events", "?q=1", INTERNET_SCHEME_HTTP, 8080},
        {"https://sessiondirectory.xboxlive.com/handles/test/session", "sessiondirectory.xboxlive.com", "/handles/test/session", "", INTERNET_SCHEME_HTTPS, 443},
        {"http://localhost/path?x=1", "localhost", "/path", "?x=1", INTERNET_SCHEME_HTTP, 80},
    };
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(cases); ++i)
    {
        URL_COMPONENTSA uc = { .dwStructSize = sizeof(uc), .dwHostNameLength = -1,
                .dwUrlPathLength = -1, .dwExtraInfoLength = -1 };
        char *normalized;
        HRESULT hr = user_parse_token_url(cases[i].url, &uc, &normalized);
        check(hr == S_OK);
        if (SUCCEEDED(hr))
        {
            check(uc.nScheme == cases[i].scheme && uc.nPort == cases[i].port);
            check(uc.dwHostNameLength == strlen(cases[i].host) && !strncmp(uc.lpszHostName, cases[i].host, uc.dwHostNameLength));
            check(uc.dwUrlPathLength == strlen(cases[i].path) && !strncmp(uc.lpszUrlPath, cases[i].path, uc.dwUrlPathLength));
            check(uc.dwExtraInfoLength == strlen(cases[i].query) && (!uc.dwExtraInfoLength || !strncmp(uc.lpszExtraInfo, cases[i].query, uc.dwExtraInfoLength)));
        }
        free(normalized);
    }
    {
        URL_COMPONENTSA uc = { .dwStructSize = sizeof(uc), .dwHostNameLength = -1 };
        char *normalized;
        check(FAILED(user_parse_token_url("ftp://example.invalid/file", &uc, &normalized)));
        free(normalized);
    }
}
static void test_broker_sessions(void)
{
    static UCHAR hash[32] = {0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
                            0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    XUserHandle original, restored;
    char *encoded, *damaged;
    UCHAR signature[64];
    unsigned before, i;
    UINT32 saved_title = titleId;
    char *saved_client = msaAppId;
    ULONGLONG saved_expiry;
    clear_msa_user(); fail_exchange = FALSE; xodusSessionCacheAvailable = TRUE;
    original = load("original-oauth", "cache-account");
    check(broker_data != NULL);
    original->endpoints = calloc(2, sizeof(*original->endpoints)); original->endpointsLen = 2;
    original->endpoints[0].host = strdup("*.xboxlive.com");
    original->endpoints[0].path = strdup("/titles/current");
    original->endpoints[0].relyingParty = strdup("http://xboxlive.com");
    original->endpoints[1].host = strdup("example.invalid");
    original->policies = calloc(1, sizeof(*original->policies)); original->policiesLen = 1;
    original->policies[0].version = 1; original->policies[0].maxBodyBytes = 8192;
    encoded = export_user_session(original); check(encoded != NULL);
    if (!encoded) { IUser_Release(&original->IUser_iface); return; }
    restored = import_user_session(encoded, "cache-account"); check(restored != NULL);
    if (restored)
    {
        check(restored->xuid == original->xuid);
        check(!memcmp(restored->proofKey, original->proofKey, PROOF_KEY_SIZE));
        check(!strcmp(restored->deviceId, original->deviceId));
        check(restored->xstsExpiry == original->xstsExpiry && restored->userTokenExpiry == original->userTokenExpiry);
        check(restored->endpointsLen == 2 && !strcmp(restored->endpoints[0].path, "/titles/current"));
        check(!restored->endpoints[1].path && !restored->endpoints[1].relyingParty);
        check(restored->policiesLen == 1 && restored->policies[0].maxBodyBytes == 8192);
        check(SUCCEEDED(IUser_SignData(&restored->IUser_iface, 3, (UCHAR *)"abc", 64, signature)));
        check(NT_SUCCESS(BCryptVerifySignature(original->key, NULL, hash, 32, signature, 64, 0)));
        check(!restored->accessToken && !restored->deviceRps);
        IUser_Release(&restored->IUser_iface);
    }
    check(!import_user_session(encoded, "different-account"));
    ++titleId; check(!import_user_session(encoded, "cache-account")); titleId = saved_title;
    msaAppId = "different-client"; check(!import_user_session(encoded, "cache-account")); msaAppId = saved_client;
    fullTrust = TRUE; check(!import_user_session(encoded, "cache-account")); fullTrust = FALSE;
    damaged = strdup(encoded);
    for (i = 0; i < strlen(encoded); i += 4)
    {
        char saved = damaged[i]; damaged[i] = 0;
        check(!import_user_session(damaged, "cache-account")); damaged[i] = saved;
    }
    damaged[0] = '!'; check(!import_user_session(damaged, "cache-account")); free(damaged);
    saved_expiry = original->userTokenExpiry; original->userTokenExpiry = session_time();
    check(!export_user_session(original)); original->userTokenExpiry = saved_expiry;
    store_broker_session(original);
    clear_msa_user(); before = requests; fail_exchange = TRUE;
    check(SUCCEEDED(LoadMsaUser("fresh-oauth", "cache-account", "fresh-device", &restored)));
    check(restored && requests == before);
    if (restored)
    {
        check(!wcscmp(WindowsGetStringRawBuffer(restored->accessToken, NULL), L"fresh-oauth"));
        check(!wcscmp(WindowsGetStringRawBuffer(restored->deviceRps, NULL), L"fresh-device"));
        IUser_Release(&restored->IUser_iface);
    }
    clear_msa_user(); fail_exchange = FALSE;
    broker_expiry = 1; before = requests;
    restored = load("fresh-oauth", "cache-account"); check(requests == before + 4);
    IUser_Release(&restored->IUser_iface); clear_msa_user();
    free(broker_data); broker_data = strdup("invalid snapshot"); before = requests;
    restored = load("fresh-oauth", "cache-account"); check(requests == before + 4);
    IUser_Release(&restored->IUser_iface); clear_msa_user();
    restored = load("fresh-oauth", "cache-account");
    before = requests;
    check(SUCCEEDED(refresh_user_session(restored)));
    check(!broker_data && !msa_user && !restored->xstsToken && !restored->xstsCacheCount && requests == before + 1);
    check(SUCCEEDED(user_request_xsts_token(restored, "http://xboxlive.com", FALSE)) && requests == before + 2);
    restored->xstsCache[0].expiry = session_time();
    check(!user_xsts_cache_activate(restored, "http://xboxlive.com", FALSE));
    IUser_Release(&restored->IUser_iface); clear_msa_user();
    before = requests; xodusSessionCacheAvailable = FALSE;
    restored = load("old-service", "cache-account"); check(requests == before + 4);
    IUser_Release(&restored->IUser_iface); clear_msa_user();
    IUser_Release(&original->IUser_iface);
    free(encoded); free(broker_data); broker_data = NULL;
}

int main(void)
{
    XUserHandle first, next, concurrent[6] = {0};
    HANDLE threads[6];
    unsigned before, i;
    check(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
    test_token_urls();
    first = load("credential-1", "account-a");
    check(requests == 4);
    before = requests;
    next = load("credential-2", "account-a");
    check(first == next && requests == before);
    IUser_Release(&first->IUser_iface);
    IUser_Release(&next->IUser_iface);
    for (i = 0; i < 6; ++i) threads[i] = CreateThread(NULL, 0, concurrent_load, concurrent + i, 0, NULL);
    WaitForMultipleObjects(6, threads, TRUE, INFINITE);
    check(requests == before);
    for (i = 0; i < 6; ++i) { check(concurrent[i] == msa_user); IUser_Release(&concurrent[i]->IUser_iface); CloseHandle(threads[i]); }
    clear_msa_user();
    before = requests;
    for (i = 0; i < 6; ++i) threads[i] = CreateThread(NULL, 0, concurrent_load, concurrent + i, 0, NULL);
    WaitForMultipleObjects(6, threads, TRUE, INFINITE);
    check(requests == before + 4);
    for (i = 0; i < 6; ++i) { check(concurrent[i] == msa_user); IUser_Release(&concurrent[i]->IUser_iface); CloseHandle(threads[i]); }
    before = requests;
    next = load("credential-3", "account-b");
    check(requests == before + 4);
    IUser_Release(&next->IUser_iface);
    before = requests;
    msa_user_loaded -= 60001;
    next = load("credential-4", "account-b");
    check(requests == before + 4);
    IUser_Release(&next->IUser_iface);
    before = requests;
    { FILETIME now;
      GetSystemTimeAsFileTime(&now);
      msa_user_expires = (((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime) + 300000000;
    }
    next = load("near-expiry", "account-b");
    check(requests == before + 4);
    IUser_Release(&next->IUser_iface);
    before = requests;
    clear_msa_user();
    next = load("credential-5", "account-b");
    check(requests == before + 4);
    IUser_Release(&next->IUser_iface);
    before = requests;
    next = load("old-service", NULL); IUser_Release(&next->IUser_iface);
    next = load("old-service", NULL); IUser_Release(&next->IUser_iface);
    check(requests == before + 8 && !msa_user);
    expiration = "2000-01-01T00:00:00Z";
    next = load("expired", "account-c"); IUser_Release(&next->IUser_iface);
    before = requests;
    next = load("expired", "account-c"); IUser_Release(&next->IUser_iface);
    check(requests == before + 4);
    clear_msa_user();
    expiration = "malformed";
    next = load("bad-date", "account-c"); IUser_Release(&next->IUser_iface);
    before = requests;
    next = load("bad-date", "account-c"); IUser_Release(&next->IUser_iface);
    check(requests == before + 4);
    clear_msa_user();
    fail_exchange = TRUE;
    next = (void *)1;
    check(FAILED(LoadMsaUser("failure", "account-c", NULL, &next)) && !next && !msa_user);
    fail_exchange = FALSE;
    expiration = "2099-01-01T00:00:00Z";
    check(SUCCEEDED(LoadMsaUser("RST2%&", "rps-account", "device\"\\\n", &first)));
    before = requests;
    check(SUCCEEDED(user_ensure_device_and_title_tokens(first)));
    check(requests == before + 2 && first->deviceToken && first->titleToken);
    before = requests;
    check(SUCCEEDED(user_ensure_device_and_title_tokens(first)) && requests == before);
    /* Fetching a title-claim token must not publish presence on the game's behalf. */
    expect_title_claims = TRUE;
    before = requests;
    check(SUCCEEDED(user_request_xsts_token(first, "http://xboxlive.com", TRUE)));
    check(requests == before + 1);
    expect_title_claims = FALSE;
    IUser_Release(&first->IUser_iface);
    clear_msa_user();
    check(SUCCEEDED(LoadMsaUser("RST2%&", "rps-account", "device\"\\\n", &first)));
    fail_sisu = TRUE;
    before = requests;
    check(FAILED(user_ensure_device_and_title_tokens(first)));
    check(requests == before + 2 && !first->deviceToken && !first->titleToken);
    /* A rejected Windows ticket must not try another title identity. */
    fail_sisu = FALSE;
    fail_exchange = TRUE;
    before = requests;
    check(FAILED(user_ensure_device_and_title_tokens(first)) && requests == before + 1);
    IUser_Release(&first->IUser_iface);
    clear_msa_user();
    test_broker_sessions();
    RoUninitialize();
    printf("%ld checks, %ld failures, %u fixture HTTP requests\n", checks, failures, requests);
    return !!failures;
}
