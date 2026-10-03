#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Security_Credentials
#define WIDL_using_Windows_Security_Authentication_Web_Core
#include <stdio.h>
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "roapi.h"
#include "winstring.h"
#include "initguid.h"
#include "windows.security.authentication.web.core.h"
static int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n",__LINE__,#x); failures++; } } while(0)
static HSTRING str(const WCHAR *s) { HSTRING h=NULL; CHECK(WindowsCreateString(s,wcslen(s),&h)==S_OK); return h; }
static HRESULT factory(const WCHAR *s,REFIID iid,void **out) { HSTRING h=str(s); HRESULT hr=RoGetActivationFactory(h,iid,out); WindowsDeleteString(h); return hr; }
static HRESULT wait_async(IUnknown *op)
{
 IAsyncInfo *info=NULL; HRESULT hr; AsyncStatus status=Started; DWORD start=GetTickCount();
 if(FAILED(hr=IUnknown_QueryInterface(op,&IID_IAsyncInfo,(void **)&info))) return hr;
 while(status==Started && GetTickCount()-start<50000) { hr=IAsyncInfo_get_Status(info,&status); if(FAILED(hr)) break; Sleep(10); }
 if(SUCCEEDED(hr)) hr=status==Completed?S_OK:E_FAIL;
 if(status==Error) IAsyncInfo_get_ErrorCode(info,&hr);
 IAsyncInfo_Release(info); return hr;
}
static void insert(IMap_HSTRING_HSTRING *map,const WCHAR *key,const WCHAR *value)
{ HSTRING k=str(key),v=str(value); boolean replaced; CHECK(IMap_HSTRING_HSTRING_Insert(map,k,v,&replaced)==S_OK); WindowsDeleteString(k); WindowsDeleteString(v); }
int main(int argc,char **argv)
{
 IWebAuthenticationCoreManagerStatics *manager=NULL; IWebTokenRequestFactory *request_factory=NULL;
 IAsyncOperation_WebAccountProvider *op=NULL; IWebAccountProvider *provider=NULL; IWebTokenRequest *request=NULL;
 IMap_HSTRING_HSTRING *map=NULL; IMapView_HSTRING_HSTRING *view=NULL; HSTRING key=NULL,value=NULL; UINT32 size;
 BOOL real=argc>1 && !strcmp(argv[1],"--real"); HRESULT hr;
 CHECK(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
 hr=factory(L"Windows.Security.Authentication.Web.Core.WebAuthenticationCoreManager",&IID_IWebAuthenticationCoreManagerStatics,(void **)&manager); CHECK(hr==S_OK); if(FAILED(hr)) goto done;
 key=str(L"https://unsupported.invalid");
 CHECK(IWebAuthenticationCoreManagerStatics_FindAccountProviderAsync(manager,key,&op)==S_OK); WindowsDeleteString(key);
 if(op) { CHECK(wait_async((IUnknown *)op)==S_OK); CHECK(IAsyncOperation_WebAccountProvider_GetResults(op,&provider)==S_OK && !provider); IAsyncOperation_WebAccountProvider_Release(op); op=NULL; }
 key=str(L"https://xsts.auth.xboxlive.com");
 CHECK(IWebAuthenticationCoreManagerStatics_FindAccountProviderAsync(manager,key,&op)==S_OK); WindowsDeleteString(key);
 if(!op) goto done;
 CHECK(wait_async((IUnknown *)op)==S_OK); CHECK(IAsyncOperation_WebAccountProvider_GetResults(op,&provider)==S_OK && provider); IAsyncOperation_WebAccountProvider_Release(op); op=NULL;
 if(!provider) goto done;
 CHECK(factory(L"Windows.Security.Authentication.Web.Core.WebTokenRequest",&IID_IWebTokenRequestFactory,(void **)&request_factory)==S_OK);
 if(!request_factory) goto done;
 CHECK(IWebTokenRequestFactory_CreateWithProvider(request_factory,provider,&request)==S_OK); if(!request) goto done;
 CHECK(IWebTokenRequest_get_Properties(request,&map)==S_OK); if(!map) goto done;
 insert(map,L"snapshot",L"before"); CHECK(IMap_HSTRING_HSTRING_GetView(map,&view)==S_OK); insert(map,L"snapshot",L"after");
 key=str(L"snapshot"); CHECK(IMapView_HSTRING_HSTRING_Lookup(view,key,&value)==S_OK && !wcscmp(WindowsGetStringRawBuffer(value,NULL),L"before")); WindowsDeleteString(value); WindowsDeleteString(key);
 CHECK(IMap_HSTRING_HSTRING_get_Size(map,&size)==S_OK && size==1); CHECK(IMap_HSTRING_HSTRING_Clear(map)==S_OK);
 if(real) {
  WCHAR exe[MAX_PATH],regpath[MAX_PATH+48],*name; HKEY reg=NULL; DWORD title=896928775;
  const WCHAR *client=L"0000000040159362"; IAsyncOperation_WebTokenRequestResult *token_op=NULL; IWebTokenRequestResult *result=NULL;
  IVectorView_WebTokenResponse *responses=NULL; IWebTokenResponse *response=NULL; WebTokenRequestStatus status;
  GetModuleFileNameW(NULL,exe,ARRAYSIZE(exe)); name=wcsrchr(exe,'\\'); name=name?name+1:exe;
  swprintf(regpath,ARRAYSIZE(regpath),L"Software\\Wine\\AppDefaults\\%ls",name);
  CHECK(RegCreateKeyExW(HKEY_CURRENT_USER,regpath,0,NULL,0,KEY_ALL_ACCESS,NULL,&reg,NULL)==ERROR_SUCCESS);
  CHECK(RegSetValueExW(reg,L"XboxClientId",0,REG_SZ,(const BYTE *)client,(wcslen(client)+1)*2)==ERROR_SUCCESS);
  CHECK(RegSetValueExW(reg,L"XboxTitleId",0,REG_DWORD,(const BYTE *)&title,sizeof(title))==ERROR_SUCCESS);
  insert(map,L"Url",L"https://xboxlive.com"); insert(map,L"Target",L"user.auth.xboxlive.com"); insert(map,L"Policy",L"MBI_SSL");
  CHECK(IWebAuthenticationCoreManagerStatics_GetTokenSilentlyAsync(manager,request,&token_op)==S_OK);
  if(token_op) {
   hr=wait_async((IUnknown *)token_op); printf("Exchange HRESULT %#lx\n",hr); CHECK(hr==S_OK);
   CHECK(IAsyncOperation_WebTokenRequestResult_GetResults(token_op,&result)==S_OK && result);
   if(result) {
    CHECK(IWebTokenRequestResult_get_ResponseStatus(result,&status)==S_OK && status==WebTokenRequestStatus_Success);
    CHECK(IWebTokenRequestResult_get_ResponseData(result,&responses)==S_OK);
    if(responses) {
     CHECK(IVectorView_WebTokenResponse_get_Size(responses,&size)==S_OK && size==1);
     CHECK(IVectorView_WebTokenResponse_GetAt(responses,0,&response)==S_OK);
     if(response) {
      IWebAccount *account=NULL; IAsyncOperation_WebTokenRequestResult *account_op=NULL;
      IWebTokenRequestResult *account_result=NULL;
      IMap_HSTRING_HSTRING *properties=NULL;
      CHECK(IWebTokenResponse_get_Properties(response,&properties)==S_OK && properties);
      if(properties) {
       key=str(L"Environment"); value=NULL;
       CHECK(IMap_HSTRING_HSTRING_Lookup(properties,key,&value)==S_OK);
       CHECK(value && !wcscmp(WindowsGetStringRawBuffer(value,NULL),L"prod"));
       WindowsDeleteString(value); WindowsDeleteString(key);
       IMap_HSTRING_HSTRING_Release(properties);
      }
      value=NULL; CHECK(IWebTokenResponse_get_Token(response,&value)==S_OK && WindowsGetStringLen(value)>0); WindowsDeleteString(value);
      CHECK(IWebTokenResponse_get_WebAccount(response,&account)==S_OK && account);
      CHECK(IWebAuthenticationCoreManagerStatics_GetTokenSilentlyWithWebAccountAsync(manager,request,NULL,&account_op)==E_INVALIDARG && !account_op);
      if(account) {
       CHECK(IWebAuthenticationCoreManagerStatics_GetTokenSilentlyWithWebAccountAsync(manager,request,account,&account_op)==S_OK);
       if(account_op) {
        CHECK(wait_async((IUnknown *)account_op)==S_OK);
        CHECK(IAsyncOperation_WebTokenRequestResult_GetResults(account_op,&account_result)==S_OK && account_result);
        if(account_result) IWebTokenRequestResult_Release(account_result);
        IAsyncOperation_WebTokenRequestResult_Release(account_op);
       }
       IWebAccount_Release(account);
      }
      IWebTokenResponse_Release(response);
     }
     IVectorView_WebTokenResponse_Release(responses);
    }
    IWebTokenRequestResult_Release(result);
   }
   IAsyncOperation_WebTokenRequestResult_Release(token_op);
  }
  if(reg) RegCloseKey(reg); RegDeleteKeyW(HKEY_CURRENT_USER,regpath);
 }
done:
 if(view) IMapView_HSTRING_HSTRING_Release(view); if(map) IMap_HSTRING_HSTRING_Release(map);
 if(request) IWebTokenRequest_Release(request); if(request_factory) IWebTokenRequestFactory_Release(request_factory);
 if(provider) IWebAccountProvider_Release(provider); if(manager) IWebAuthenticationCoreManagerStatics_Release(manager);
 RoUninitialize(); printf("webcore: %s (%d failures)\n",failures?"FAIL":"PASS",failures); return !!failures;
}
