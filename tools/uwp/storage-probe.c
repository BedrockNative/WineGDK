/* Integration probe for WineGDK's loose-package ApplicationData support.
 * Run only in a disposable prefix; uses its own AppDefaults key and data folders.
 */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Storage
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.storage.h>
#include <stdio.h>

static int failures;
#define CHECK(test) do { if (!(test)) { printf("FAIL line %d: %s\n", __LINE__, #test); ++failures; } } while (0)

static void check_folder(IApplicationData *data, int which, const WCHAR *suffix)
{
    IStorageFolder *folder = NULL;
    IStorageItem *item = NULL;
    HSTRING path = NULL;
    HRESULT hr;
    WCHAR filename[1024];
    HANDLE file;
    const WCHAR *str;
    if (which == 0) hr = IApplicationData_get_LocalFolder(data, &folder);
    else if (which == 1) hr = IApplicationData_get_RoamingFolder(data, &folder);
    else hr = IApplicationData_get_TemporaryFolder(data, &folder);
    CHECK(hr == S_OK && folder);
    if (FAILED(hr) || !folder) return;
    hr = IStorageFolder_QueryInterface(folder, &IID_IStorageItem, (void **)&item);
    CHECK(hr == S_OK && item);
    IStorageFolder_Release(folder);
    if (FAILED(hr) || !item) return;
    hr = IStorageItem_get_Path(item, &path);
    CHECK(hr == S_OK && path);
    if (SUCCEEDED(hr) && path)
    {
        str = WindowsGetStringRawBuffer(path, NULL);
        CHECK(wcsstr(str, suffix) != NULL);
        CHECK(GetFileAttributesW(str) & FILE_ATTRIBUTE_DIRECTORY);
        swprintf(filename, 1024, L"%ls\\winegdk-probe.tmp", str);
        file = CreateFileW(filename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        CHECK(file != INVALID_HANDLE_VALUE);
        if (file != INVALID_HANDLE_VALUE) { CloseHandle(file); CHECK(DeleteFileW(filename)); }
        wprintf(L"Folder: %ls\n", str);
        WindowsDeleteString(path);
    }
    IStorageItem_Release(item);
}

int main(void)
{
    const WCHAR family[] = L"WineGDK.StorageProbe_123456789abcd";
    const WCHAR bad_family[] = L"..\\escape_123456789abcd";
    const WCHAR class_name[] = L"Windows.Storage.ApplicationData";
    WCHAR exe[MAX_PATH], key[MAX_PATH + 40], *name;
    IApplicationDataStatics *statics = NULL;
    IApplicationData *data = NULL;
    IStorageFolder *folder;
    HSTRING str;
    HKEY reg;
    HRESULT hr;
    CHECK(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    name = wcsrchr(exe, '\\');
    swprintf(key, MAX_PATH + 40, L"Software\\Wine\\AppDefaults\\%ls", name ? name + 1 : exe);
    CHECK(RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &reg, NULL) == ERROR_SUCCESS);
    CHECK(SUCCEEDED(WindowsCreateString(class_name, (UINT32)wcslen(class_name), &str)));
    hr = RoGetActivationFactory(str, &IID_IApplicationDataStatics, (void **)&statics);
    WindowsDeleteString(str);
    CHECK(hr == S_OK && statics);
    if (FAILED(hr)) return 1;
    hr = IApplicationDataStatics_get_Current(statics, &data);
    CHECK(hr == S_OK && data);
    if (FAILED(hr)) return 1;
    CHECK(IApplicationData_get_LocalFolder(data, NULL) == E_POINTER);
    RegDeleteValueW(reg, L"PackageFamilyName");
    folder = (IStorageFolder *)1;
    CHECK(FAILED(IApplicationData_get_LocalFolder(data, &folder)) && !folder);
    CHECK(RegSetValueExW(reg, L"PackageFamilyName", 0, REG_SZ, (const BYTE *)bad_family, sizeof(bad_family)) == ERROR_SUCCESS);
    folder = (IStorageFolder *)1;
    CHECK(IApplicationData_get_LocalFolder(data, &folder) == HRESULT_FROM_WIN32(ERROR_INVALID_NAME) && !folder);
    CHECK(RegSetValueExW(reg, L"PackageFamilyName", 0, REG_SZ, (const BYTE *)family, sizeof(family)) == ERROR_SUCCESS);
    check_folder(data, 0, L"\\Packages\\WineGDK.StorageProbe_123456789abcd\\LocalState");
    check_folder(data, 0, L"\\Packages\\WineGDK.StorageProbe_123456789abcd\\LocalState");
    check_folder(data, 1, L"\\Packages\\WineGDK.StorageProbe_123456789abcd\\RoamingState");
    check_folder(data, 2, L"\\Packages\\WineGDK.StorageProbe_123456789abcd\\TempState");
    IApplicationData_Release(data);
    IApplicationDataStatics_Release(statics);
    RegDeleteValueW(reg, L"PackageFamilyName");
    RegCloseKey(reg);
    RegDeleteKeyW(HKEY_CURRENT_USER, key);
    RoUninitialize();
    printf("Storage probe: %d failure(s)\n", failures);
    return failures != 0;
}
