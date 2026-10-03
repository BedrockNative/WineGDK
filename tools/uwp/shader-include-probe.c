/* Standard shader includes with Windows and slash-separated source paths.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>

static int write_file(const WCHAR *path, const char *data)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written;
    BOOL result;
    if (file == INVALID_HANDLE_VALUE) return 0;
    result = WriteFile(file, data, strlen(data), &written, NULL);
    CloseHandle(file);
    return result && written == strlen(data);
}
int main(void)
{
    static const char shader[] = "#include \"value.h\"\nfloat4 main() : SV_Target { return color; }\n";
    static const char header[] = "static const float4 color = float4(1,0,0,1);\n";
    WCHAR temp[MAX_PATH], dir[MAX_PATH], source[MAX_PATH], current[MAX_PATH];
    ID3DBlob *blob, *errors;
    unsigned int i, j, failures = 0;
    HRESULT hr;
    GetCurrentDirectoryW(MAX_PATH, current);
    GetTempPathW(MAX_PATH, temp);
    if (!GetTempFileNameW(temp, L"hls", 0, dir)) return 1;
    DeleteFileW(dir);
    if (!CreateDirectoryW(dir, NULL) || !SetCurrentDirectoryW(dir)) return 1;
    if (!CreateDirectoryW(L"nested", NULL) || !write_file(L"nested\\source.hlsl", shader) ||
        !write_file(L"nested\\value.h", header)) return 1;
    for (i = 0; i < 4; ++i)
    {
        if (i < 2) wcscpy(source, L"nested\\source.hlsl");
        else { GetFullPathNameW(L"nested\\source.hlsl", MAX_PATH, source, NULL); }
        if (i & 1) for (j = 0; source[j]; ++j) if (source[j] == '\\') source[j] = '/';
        blob = errors = NULL;
        hr = D3DCompileFromFile(source, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_4_0", 0, 0, &blob, &errors);
        printf("include path variant %u: %#lx %s\n", i, hr, SUCCEEDED(hr) && blob ? "PASS" : "FAIL");
        if (FAILED(hr) || !blob) ++failures;
        if (errors) { if (FAILED(hr)) printf("%s\n", (char *)ID3D10Blob_GetBufferPointer(errors)); ID3D10Blob_Release(errors); }
        if (blob) ID3D10Blob_Release(blob);
    }
    DeleteFileW(L"nested\\source.hlsl"); DeleteFileW(L"nested\\value.h"); RemoveDirectoryW(L"nested");
    SetCurrentDirectoryW(current); RemoveDirectoryW(dir);
    return failures ? 1 : 0;
}
