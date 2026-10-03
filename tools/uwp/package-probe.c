#define COBJMACROS
#define WIDL_using_Windows_ApplicationModel
#include <stdarg.h>
#include <stdio.h>
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "appmodel.h"
#include "roapi.h"
#include "winstring.h"
#include "initguid.h"
#include "windows.applicationmodel.h"
static int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n",__LINE__,#x); ++failures; } } while (0)
int main(void)
{
 WCHAR exe[MAX_PATH], key[MAX_PATH+48], buffer[256], *name;
 const WCHAR *full=L"WineGDK.PackageProbe_1.2.3.4_x64__8wekyb3d8bbwe", *family=L"WineGDK.PackageProbe_8wekyb3d8bbwe";
 HKEY reg;
 UINT32 size=0;
 HSTRING cls=NULL, string=NULL;
 IPackageStatics *statics=NULL;
 IPackage *package=NULL;
 IPackageId *id=NULL;
 PackageVersion version;
 PACKAGE_ID *native_id=NULL;
 CHECK(RoInitialize(RO_INIT_MULTITHREADED)==S_OK);
 CHECK(GetCurrentPackageFullName(&size,NULL)==APPMODEL_ERROR_NO_PACKAGE);
 CHECK(GetCurrentPackageFullName(NULL,NULL)==ERROR_INVALID_PARAMETER);
 GetModuleFileNameW(NULL,exe,ARRAYSIZE(exe)); name=wcsrchr(exe,'\\'); name=name ? name+1:exe;
 swprintf(key,ARRAYSIZE(key),L"Software\\Wine\\AppDefaults\\%ls",name);
 CHECK(RegCreateKeyExW(HKEY_CURRENT_USER,key,0,NULL,0,KEY_ALL_ACCESS,NULL,&reg,NULL)==ERROR_SUCCESS);
 CHECK(RegSetValueExW(reg,L"PackageFullName",0,REG_SZ,(const BYTE *)full,(wcslen(full)+1)*2)==ERROR_SUCCESS);
 CHECK(RegSetValueExW(reg,L"PackageFamilyName",0,REG_SZ,(const BYTE *)family,(wcslen(family)+1)*2)==ERROR_SUCCESS);
 CHECK(GetCurrentPackageFullName(&size,NULL)==ERROR_INSUFFICIENT_BUFFER && size==wcslen(full)+1);
 size=1; buffer[0]=0x1234;
 CHECK(GetCurrentPackageFullName(&size,buffer)==ERROR_INSUFFICIENT_BUFFER && buffer[0]==0x1234);
 size=ARRAYSIZE(buffer); CHECK(GetCurrentPackageFullName(&size,buffer)==ERROR_SUCCESS && !wcscmp(buffer,full));
 size=0; CHECK(GetCurrentPackageId(&size,NULL)==ERROR_INSUFFICIENT_BUFFER);
 native_id=HeapAlloc(GetProcessHeap(),0,size);
 CHECK(GetCurrentPackageId(&size,(BYTE *)native_id)==ERROR_SUCCESS);
 CHECK(native_id->version.Major==1 && native_id->version.Revision==4 && !wcscmp(native_id->name,L"WineGDK.PackageProbe"));
 HeapFree(GetProcessHeap(),0,native_id);
 CHECK(WindowsCreateString(L"Windows.ApplicationModel.Package",32,&cls)==S_OK);
 CHECK(RoGetActivationFactory(cls,&IID_IPackageStatics,(void **)&statics)==S_OK);
 WindowsDeleteString(cls);
 if(statics)
 {
  CHECK(IPackageStatics_get_Current(statics,&package)==S_OK);
  if(package)
  {
   CHECK(IPackage_get_Id(package,&id)==S_OK);
   if(id)
   {
    CHECK(IPackageId_get_FullName(id,&string)==S_OK && !wcscmp(WindowsGetStringRawBuffer(string,NULL),full)); WindowsDeleteString(string);
    CHECK(IPackageId_get_Version(id,&version)==S_OK && version.Major==1 && version.Minor==2 && version.Build==3 && version.Revision==4);
    CHECK(IPackageId_get_FamilyName(id,&string)==S_OK && !wcscmp(WindowsGetStringRawBuffer(string,NULL),family)); WindowsDeleteString(string);
    IPackageId_Release(id);
   }
   IPackage_Release(package);
  }
  IPackageStatics_Release(statics);
 }
 RegCloseKey(reg); RegDeleteKeyW(HKEY_CURRENT_USER,key);
 RoUninitialize(); printf("package: %s (%d failures)\n",failures ? "FAIL":"PASS",failures); return !!failures;
}
