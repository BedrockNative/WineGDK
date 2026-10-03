/* SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_ApplicationModel_Store
#include <stdio.h>
#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "initguid.h"
#include "roapi.h"
#include "winstring.h"
#include "windows.applicationmodel.store.h"
static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %u: %s\n", __LINE__, #x); ++failures; } } while (0)
int main(int argc, char **argv)
{
    const WCHAR family[] = L"Wine.StoreProbe_test";
    const WCHAR name[] = L"Windows.ApplicationModel.Store.CurrentApp";
    WCHAR exe[MAX_PATH], key[MAX_PATH + 64], *base;
    HKEY reg;
    HSTRING cls;
    ICurrentApp *app = NULL;
    ILicenseInformation *license = NULL;
    HRESULT expected;
    boolean active, trial;
    DateTime expiration;
    if (argc != 3) return 2;
    expected = strtoul(argv[1], NULL, 16);
    CHECK(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
    GetModuleFileNameW(NULL, exe, ARRAYSIZE(exe));
    base = wcsrchr(exe, '\\'); base = base ? base + 1 : exe;
    swprintf(key, ARRAYSIZE(key), L"Software\\Wine\\AppDefaults\\%ls", base);
    CHECK(!RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &reg, NULL));
    CHECK(!RegSetValueExW(reg, L"PackageFamilyName", 0, REG_SZ, (const BYTE *)family, sizeof(family)));
    RegCloseKey(reg);
    WindowsCreateString(name, ARRAYSIZE(name) - 1, &cls);
    CHECK(RoGetActivationFactory(cls, &IID_ICurrentApp, (void **)&app) == S_OK);
    WindowsDeleteString(cls);
    if (app) CHECK(ICurrentApp_get_LicenseInformation(app, &license) == S_OK);
    if (license)
    {
        CHECK(ILicenseInformation_get_IsActive(license, NULL) == E_POINTER);
        CHECK(ILicenseInformation_get_IsTrial(license, NULL) == E_POINTER);
        CHECK(ILicenseInformation_get_ExpirationDate(license, NULL) == E_POINTER);
        CHECK(ILicenseInformation_get_IsActive(license, &active) == expected);
        CHECK(ILicenseInformation_get_IsTrial(license, &trial) == expected);
        CHECK(ILicenseInformation_get_ExpirationDate(license, &expiration) == expected);
        CHECK(active == (SUCCEEDED(expected) && atoi(argv[2])));
        CHECK(trial == FALSE);
        CHECK(expiration.UniversalTime == (SUCCEEDED(expected) ? 140000000000000000LL : 0));
        ILicenseInformation_Release(license);
    }
    if (app) ICurrentApp_Release(app);
    RegDeleteKeyW(HKEY_CURRENT_USER, key);
    RoUninitialize();
    printf("Store license: %u failures\n", failures);
    return !!failures;
}
