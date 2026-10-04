/* Private bootstrap snapshot format. Only sent to the local Xodus broker.
 * No OAuth credentials are cached; a fresh MSA response confirms the account.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define SESSION_MAX_BYTES 45000
#define SESSION_FILETIME_EPOCH 116444736000000000ULL

struct session_header
{
    UINT32 magic, version, title, trust;
    UINT64 xuid, user_expiry, xsts_expiry;
    UINT32 endpoints, policies;
    UCHAR key[sizeof(BCRYPT_ECCKEY_BLOB) + 96];
    char device_id[40];
};

struct session_buffer { BYTE *data; DWORD size, offset; };

static BOOL session_write( struct session_buffer *buffer, const void *data, DWORD size )
{
    if (size > buffer->size - buffer->offset) return FALSE;
    if (size) memcpy( buffer->data + buffer->offset, data, size );
    buffer->offset += size;
    return TRUE;
}

static BOOL session_write_string( struct session_buffer *buffer, const char *value )
{
    DWORD size = value ? strlen(value) : 0;
    return session_write( buffer, &size, sizeof(size) ) && session_write( buffer, value, size );
}

static BOOL session_read( struct session_buffer *buffer, void *out, DWORD size )
{
    if (size > buffer->size - buffer->offset) return FALSE;
    if (size) memcpy( out, buffer->data + buffer->offset, size );
    buffer->offset += size;
    return TRUE;
}

static BOOL session_read_string( struct session_buffer *buffer, char **out )
{
    DWORD size;
    *out = NULL;
    if (!session_read( buffer, &size, sizeof(size) ) || size > buffer->size - buffer->offset ||
        memchr( buffer->data + buffer->offset, 0, size )) return FALSE;
    if (!(*out = malloc(size + 1))) return FALSE;
    memcpy( *out, buffer->data + buffer->offset, size );
    (*out)[size] = 0;
    buffer->offset += size;
    return TRUE;
}

static ULONGLONG session_time(void)
{
    FILETIME now;
    GetSystemTimeAsFileTime( &now );
    return ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime;
}

static char *export_user_session( struct XUser *impl )
{
    struct session_header header = {0};
    struct session_buffer buffer = {0};
    HSTRING strings[] = {impl->userHash, impl->userToken, impl->xstsToken, impl->publicGamerpic,
        impl->classicGamertag, impl->modernGamertag, impl->modernGamertagSuffix, impl->uniqueModernGamertag};
    DWORD length = 0, key_length, i;
    char *encoded = NULL, *value = NULL;
    if (!impl->brokerPuid || !msaAppId || !impl->key || !impl->xuid || impl->xstsWithTitle ||
        !impl->xstsRelyingParty || strcmp(impl->xstsRelyingParty, "http://xboxlive.com") ||
        min(impl->userTokenExpiry, impl->xstsExpiry) <= session_time() + 600000000) return NULL;
    if (!(buffer.data = calloc(1, SESSION_MAX_BYTES))) return NULL;
    buffer.size = SESSION_MAX_BYTES;
    header.magic = 0x534b4458; /* XDKS, little-endian, versioned independently of Wine. */
    header.version = 1;
    header.title = titleId;
    header.trust = !!fullTrust;
    header.xuid = impl->xuid;
    header.user_expiry = impl->userTokenExpiry;
    header.xsts_expiry = impl->xstsExpiry;
    header.endpoints = impl->endpointsLen;
    header.policies = impl->policiesLen;
    memcpy(header.device_id, impl->deviceId, sizeof(header.device_id));
    if (!NT_SUCCESS(BCryptExportKey(impl->key, NULL, BCRYPT_ECCPRIVATE_BLOB, header.key,
                                   sizeof(header.key), &key_length, 0)) || key_length != sizeof(header.key)) goto done;
    if (!session_write(&buffer, &header, sizeof(header)) ||
        !session_write_string(&buffer, impl->brokerPuid) || !session_write_string(&buffer, msaAppId)) goto done;
    for (i = 0; i < ARRAY_SIZE(strings); ++i)
    {
        if (FAILED(HSTRINGToMultiByte(strings[i], &value))) goto done;
        if (!session_write_string(&buffer, value)) goto done;
        free(value); value = NULL;
    }
    for (i = 0; i < impl->endpointsLen; ++i)
        if (!session_write_string(&buffer, impl->endpoints[i].host) ||
            !session_write_string(&buffer, impl->endpoints[i].path) ||
            !session_write_string(&buffer, impl->endpoints[i].relyingParty)) goto done;
    if (impl->policiesLen > SESSION_MAX_BYTES / sizeof(*impl->policies) ||
        !session_write(&buffer, impl->policies, impl->policiesLen * sizeof(*impl->policies))) goto done;
    if (!CryptBinaryToStringA(buffer.data, buffer.offset, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &length) ||
        !(encoded = malloc(length))) goto done;
    if (!CryptBinaryToStringA(buffer.data, buffer.offset, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded, &length))
    { free(encoded); encoded = NULL; }
done:
    free(value);
    SecureZeroMemory(&header, sizeof(header));
    SecureZeroMemory(buffer.data, buffer.size);
    free(buffer.data);
    return encoded;
}

static BOOL session_valid_base64( const char *value )
{
    SIZE_T i, length;
    if (!value || !(length = strlen(value)) || length > 60000 || length % 4) return FALSE;
    for (i = 0; i < length; ++i)
    {
        unsigned char c = value[i];
        if (c == '=') return (i == length - 1 || (i == length - 2 && value[i + 1] == '='));
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '+' || c == '/')) return FALSE;
    }
    return TRUE;
}

static struct XUser *import_user_session( const char *encoded, const char *puid )
{
    struct session_header header;
    struct session_buffer buffer = {0};
    BCRYPT_ECCKEY_BLOB key_header;
    BCRYPT_ALG_HANDLE algorithm = NULL;
    struct XUser *impl = NULL, *result = NULL;
    HSTRING *strings[8];
    char proof[PROOF_KEY_SIZE + 1] = {0}, *value = NULL;
    DWORD length = 0, i;
    if (!session_valid_base64(encoded) || !puid || !msaAppId ||
        !CryptStringToBinaryA(encoded, 0, CRYPT_STRING_BASE64, NULL, &length, NULL, NULL) ||
        length < sizeof(header) || length > SESSION_MAX_BYTES || !(buffer.data = malloc(length))) return NULL;
    buffer.size = length;
    if (!CryptStringToBinaryA(encoded, 0, CRYPT_STRING_BASE64, buffer.data, &length, NULL, NULL) ||
        !session_read(&buffer, &header, sizeof(header))) goto done;
    memcpy(&key_header, header.key, sizeof(key_header));
    if (header.magic != 0x534b4458 || header.version != 1 || header.title != titleId || header.trust != !!fullTrust ||
        !header.xuid || min(header.user_expiry, header.xsts_expiry) <= session_time() + 600000000 ||
        header.endpoints > SESSION_MAX_BYTES / 12 || header.policies > SESSION_MAX_BYTES / sizeof(struct policy) ||
        header.device_id[38] || header.device_id[0] != '{' || header.device_id[37] != '}' ||
        key_header.dwMagic != BCRYPT_ECDSA_PRIVATE_P256_MAGIC || key_header.cbKey != 32) goto done;
    if (!session_read_string(&buffer, &value) || strcmp(value, puid)) goto done;
    free(value); value = NULL;
    if (!session_read_string(&buffer, &value) || strcmp(value, msaAppId)) goto done;
    free(value); value = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) goto done;
    impl->IUser_iface.lpVtbl = &user_vtbl;
    impl->ref = 1;
    impl->xuid = header.xuid;
    impl->userTokenExpiry = header.user_expiry;
    impl->xstsExpiry = header.xsts_expiry;
    memcpy(impl->deviceId, header.device_id, sizeof(impl->deviceId));
    if (!(impl->brokerPuid = strdup(puid)) || !(impl->xstsRelyingParty = strdup("http://xboxlive.com"))) goto done;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM, NULL, 0)) ||
        !NT_SUCCESS(BCryptImportKeyPair(algorithm, NULL, BCRYPT_ECCPRIVATE_BLOB, &impl->key, header.key, sizeof(header.key), 0)) ||
        FAILED(user_format_proof_key(header.key, proof))) goto done;
    memcpy(impl->proofKey, proof, sizeof(impl->proofKey));
    strings[0] = &impl->userHash; strings[1] = &impl->userToken; strings[2] = &impl->xstsToken;
    strings[3] = &impl->publicGamerpic; strings[4] = &impl->classicGamertag;
    strings[5] = &impl->modernGamertag; strings[6] = &impl->modernGamertagSuffix; strings[7] = &impl->uniqueModernGamertag;
    for (i = 0; i < ARRAY_SIZE(strings); ++i)
    {
        if (!session_read_string(&buffer, &value) || (i < 3 && !*value) ||
            FAILED(MultiByteToHSTRING(value, strlen(value), strings[i]))) goto done;
        free(value); value = NULL;
    }
    if (header.endpoints && !(impl->endpoints = calloc(header.endpoints, sizeof(*impl->endpoints)))) goto done;
    impl->endpointsLen = header.endpoints;
    for (i = 0; i < impl->endpointsLen; ++i)
    {
        if (!session_read_string(&buffer, &impl->endpoints[i].host) ||
            !session_read_string(&buffer, &impl->endpoints[i].path) ||
            !session_read_string(&buffer, &impl->endpoints[i].relyingParty)) goto done;
        if (!*impl->endpoints[i].path) { free(impl->endpoints[i].path); impl->endpoints[i].path = NULL; }
        if (!*impl->endpoints[i].relyingParty) { free(impl->endpoints[i].relyingParty); impl->endpoints[i].relyingParty = NULL; }
    }
    if (header.policies && !(impl->policies = calloc(header.policies, sizeof(*impl->policies)))) goto done;
    impl->policiesLen = header.policies;
    if (!session_read(&buffer, impl->policies, impl->policiesLen * sizeof(*impl->policies)) || buffer.offset != buffer.size ||
        FAILED(user_xsts_cache_store(impl, "http://xboxlive.com", FALSE, impl->xstsToken, impl->userHash, impl->xstsExpiry))) goto done;
    result = impl;
    impl = NULL;
done:
    if (impl) IUser_Release(&impl->IUser_iface);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    free(value);
    SecureZeroMemory(&header, sizeof(header));
    SecureZeroMemory(buffer.data, buffer.size);
    free(buffer.data);
    return result;
}

static HRESULT restore_broker_session( struct XUser **user )
{
    struct XUser *restored = NULL, *impl = *user;
    char *encoded = NULL;
    LONGLONG expiry = 0;
    HRESULT hr = xodus_session_cache("get", impl->brokerPuid, 0, NULL, &encoded, &expiry);
    if (hr == S_OK && expiry > 0 && (ULONGLONG)expiry > (session_time() - SESSION_FILETIME_EPOCH) / 10000000 + 60)
        restored = import_user_session(encoded, impl->brokerPuid);
    if (encoded) { SecureZeroMemory(encoded, strlen(encoded)); free(encoded); }
    if (!restored) return S_FALSE;
    restored->accessToken = impl->accessToken; impl->accessToken = NULL;
    restored->deviceRps = impl->deviceRps; impl->deviceRps = NULL;
    IUser_Release(&impl->IUser_iface);
    *user = restored;
    TRACE_(gdk_session)("Restored Xbox bootstrap session from Xodus.\n");
    return S_OK;
}

static void store_broker_session( struct XUser *impl )
{
    char *encoded;
    HRESULT hr;
    if (!xodusSessionCacheAvailable || !(encoded = export_user_session(impl))) return;
    hr = xodus_session_cache("put", impl->brokerPuid,
            (min(impl->userTokenExpiry, impl->xstsExpiry) - SESSION_FILETIME_EPOCH) / 10000000, encoded, NULL, NULL);
    SecureZeroMemory(encoded, strlen(encoded));
    free(encoded);
    TRACE_(gdk_session)("Stored Xbox bootstrap session in Xodus, hr %#lx.\n", hr);
}
