/* Integration probe for CryptographicBuffer.GenerateRandom. */
#define COBJMACROS
#define WIDL_using_Windows_Storage_Streams
#define WIDL_using_Windows_Security_Cryptography
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.security.cryptography.h>
#include <robuffer.h>
#include <stdio.h>

int main(void)
{
    const WCHAR name[] = L"Windows.Security.Cryptography.CryptographicBuffer";
    const UINT32 sizes[] = {0, 1, 64, 4096};
    ICryptographicBufferStatics *factory;
    IBufferByteAccess *access;
    IBuffer *buffer;
    HSTRING str, hex;
    HRESULT hr;
    UINT32 length, capacity, i;
    BYTE *bytes;
    int failures = 0;
#define CHECK(test) do { if (!(test)) { printf("FAIL line %d: %s\n", __LINE__, #test); ++failures; } } while (0)
    hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr)) return 1;
    WindowsCreateString(name, (UINT32)wcslen(name), &str);
    hr = RoGetActivationFactory(str, &IID_ICryptographicBufferStatics, (void **)&factory);
    WindowsDeleteString(str);
    if (FAILED(hr)) { printf("Factory: %08lx\n", hr); return 1; }
    CHECK(ICryptographicBufferStatics_GenerateRandom(factory, 64, NULL) == E_POINTER);
    for (i = 0; i < sizeof(sizes) / sizeof(*sizes); ++i)
    {
        buffer = NULL;
        hr = ICryptographicBufferStatics_GenerateRandom(factory, sizes[i], &buffer);
        CHECK(hr == S_OK && buffer);
        if (FAILED(hr) || !buffer) continue;
        CHECK(IBuffer_get_Length(buffer, &length) == S_OK && length == sizes[i]);
        CHECK(IBuffer_get_Capacity(buffer, &capacity) == S_OK && capacity >= length);
        CHECK(ICryptographicBufferStatics_EncodeToHexString(factory, buffer, NULL) == E_POINTER);
        hr = ICryptographicBufferStatics_EncodeToHexString(factory, buffer, &hex);
        CHECK(hr == S_OK);
        if (SUCCEEDED(hr))
        {
            CHECK(WindowsGetStringLen(hex) == length * 2);
            WindowsDeleteString(hex);
        }
        hr = IBuffer_QueryInterface(buffer, &IID_IBufferByteAccess, (void **)&access);
        CHECK(hr == S_OK);
        if (SUCCEEDED(hr))
        {
            CHECK(IBufferByteAccess_Buffer(access, &bytes) == S_OK);
            if (length >= 64)
            {
                UINT32 j;
                BYTE any = 0;
                for (j = 0; j < length; ++j) any |= bytes[j];
                CHECK(any != 0); /* Detect an unfilled buffer, not an entropy test. */
                bytes[0] = 0; bytes[1] = 0xab; bytes[2] = 0xff;
                CHECK(IBuffer_put_Length(buffer, 3) == S_OK);
                hr = ICryptographicBufferStatics_EncodeToHexString(factory, buffer, &hex);
                CHECK(hr == S_OK);
                if (SUCCEEDED(hr))
                {
                    CHECK(!wcscmp(WindowsGetStringRawBuffer(hex, NULL), L"00abff"));
                    WindowsDeleteString(hex);
                }
            }
            IBufferByteAccess_Release(access);
        }
        IBuffer_Release(buffer);
    }
    ICryptographicBufferStatics_Release(factory);
    RoUninitialize();
    printf("Crypto probe: %d failure(s)\n", failures);
    return failures != 0;
}
