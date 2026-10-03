/* Run only against a disposable prefix and an authenticated local Xodus service. */
#define COBJMACROS
#define CONST_VTABLE
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_ApplicationModel_Store
#include <stdio.h>
#include "windef.h"
#include "winbase.h"
#include "initguid.h"
#include "roapi.h"
#include "winstring.h"
#include "winreg.h"
#include "windows.applicationmodel.store.h"
static int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); failures++; } } while(0)
int main(void)
{
    const WCHAR family[] = L"Microsoft.MinecraftUWP_8wekyb3d8bbwe";
    const WCHAR class_name[] = L"Windows.ApplicationModel.Store.CurrentApp";
    WCHAR executable[MAX_PATH], key[MAX_PATH + 64], *name;
    HKEY app_key;
    HSTRING cls, receipt;
    ICurrentApp *app = NULL;
    IAsyncOperation_HSTRING *operation;
    IAsyncInfo *info;
    AsyncStatus status;
    HRESULT hr;
    DWORD start, size = sizeof(family);
    unsigned int i;
    setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
    GetModuleFileNameW(NULL, executable, ARRAYSIZE(executable));
    name = wcsrchr(executable, '\\'); name = name ? name + 1 : executable;
    swprintf(key, ARRAYSIZE(key), L"Software\\Wine\\AppDefaults\\%ls", name);
    CHECK(RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &app_key, NULL) == ERROR_SUCCESS);
    CHECK(RegSetValueExW(app_key, L"PackageFamilyName", 0, REG_SZ, (const BYTE *)family, size) == ERROR_SUCCESS);
    RegCloseKey(app_key);
    CHECK(WindowsCreateString(class_name, ARRAYSIZE(class_name)-1, &cls) == S_OK);
    CHECK(RoGetActivationFactory(cls, &IID_ICurrentApp, (void **)&app) == S_OK);
    WindowsDeleteString(cls);
    if (app) for (i = 0; i < 2; ++i)
    {
        operation = NULL; receipt = NULL;
        start = GetTickCount();
        hr = ICurrentApp_GetAppReceiptAsync(app, &operation);
        CHECK(hr == S_OK && operation);
        if (!operation) break;
        CHECK(IAsyncOperation_HSTRING_QueryInterface(operation, &IID_IAsyncInfo, (void **)&info) == S_OK);
        do {
            CHECK(IAsyncInfo_get_Status(info, &status) == S_OK);
            if (status != Started) break;
            Sleep(10);
        } while (GetTickCount() - start < 45000);
        CHECK(status == Completed);
        IAsyncInfo_get_ErrorCode(info, &hr);
        printf("Receipt async result %#lx, elapsed %lu ms\n", hr, GetTickCount() - start);
        hr = IAsyncOperation_HSTRING_GetResults(operation, &receipt);
        CHECK(hr == S_OK && WindowsGetStringLen(receipt));
        if (receipt)
        {
            const WCHAR *xml = WindowsGetStringRawBuffer(receipt, NULL);
            CHECK(wcsstr(xml, L"<Receipt") != NULL);
            CHECK(wcsstr(xml, L"<AppReceipt") != NULL);
            CHECK(wcsstr(xml, L"<Signature") != NULL);
            CHECK(wcsstr(xml, L"Microsoft.MinecraftUWP_8wekyb3d8bbwe") != NULL);
            printf("Receipt XML: %u characters; contents kept private\n", WindowsGetStringLen(receipt));
            WindowsDeleteString(receipt);
        }
        IAsyncInfo_Close(info); IAsyncInfo_Release(info);
        IAsyncOperation_HSTRING_Release(operation);
    }
    if (app) ICurrentApp_Release(app);
    RegDeleteKeyW(HKEY_CURRENT_USER, key);
    RoUninitialize();
    printf("Store probe: %u failures\n", failures);
    return !!failures;
}
