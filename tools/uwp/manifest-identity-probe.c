/* Probe automatic identity without registry overrides. */
#include <stdio.h>
#include <windows.h>
#include "appmodel.h"
static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %u: %s\n",__LINE__,#x); ++failures; } } while(0)
int wmain(int argc, WCHAR **argv)
{
    WCHAR value[32768];
    UINT32 size = 0;
    PACKAGE_ID *id;
    LONG ret;
    if (argc != 5) return 2;
    ret = GetCurrentPackageFullName(&size,NULL);
    if (!wcscmp(argv[1],L"none"))
    {
        CHECK(ret == APPMODEL_ERROR_NO_PACKAGE);
        printf("unpackaged: %u failures\n",failures);
        return !!failures;
    }
    CHECK(ret == ERROR_INSUFFICIENT_BUFFER && size == wcslen(argv[1]) + 1);
    size = 1; value[0] = 0x1234;
    CHECK(GetCurrentPackageFullName(&size,value) == ERROR_INSUFFICIENT_BUFFER && value[0] == 0x1234);
    size = _countof(value);
    CHECK(GetCurrentPackageFullName(&size,value) == 0 && !wcscmp(value,argv[1]));
    size = _countof(value);
    CHECK(GetCurrentPackageFamilyName(&size,value) == 0 && !wcscmp(value,argv[2]));
    size = _countof(value);
    CHECK(GetCurrentApplicationUserModelId(&size,value) == 0 && !wcscmp(value,argv[3]));
    size = _countof(value);
    CHECK(GetCurrentPackagePath(&size,value) == 0 && GetFileAttributesW(value) != INVALID_FILE_ATTRIBUTES);
    size = 0;
    CHECK(GetCurrentPackageId(&size,NULL) == ERROR_INSUFFICIENT_BUFFER);
    id = malloc(size);
    CHECK(id != NULL);
    if (id)
    {
        CHECK(GetCurrentPackageId(&size,(BYTE *)id) == 0);
        CHECK(id->publisher && !wcscmp(id->publisher,argv[4]));
        CHECK(!wcscmp(id->publisherId,L"8wekyb3d8bbwe"));
        free(id);
    }
    printf("manifest identity: %u failures\n",failures);
    return !!failures;
}
