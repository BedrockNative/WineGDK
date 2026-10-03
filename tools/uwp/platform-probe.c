#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Foundation_Diagnostics
#define WIDL_using_Windows_Foundation_Metadata
#define WIDL_using_Windows_Networking_PushNotifications
#define WIDL_using_Windows_System_Power
#define WIDL_using_Windows_ApplicationModel_Resources_Core
#include <stdio.h>
#include "windef.h"
#include "winbase.h"
#include "roapi.h"
#include "winstring.h"
#include "initguid.h"
#include "windows.foundation.metadata.h"
#include "windows.foundation.diagnostics.h"
#include "windows.system.power.h"
#include "windows.networking.pushnotifications.h"
#include "windows.applicationmodel.resources.core.h"
static int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n",__LINE__,#x); failures++; } } while(0)
static HRESULT get_factory(const WCHAR *name,REFIID iid,void **out)
{ HSTRING str=NULL; HRESULT hr=WindowsCreateString(name,wcslen(name),&str); if(SUCCEEDED(hr)) { hr=RoGetActivationFactory(str,iid,out); WindowsDeleteString(str); } return hr; }
int main(void)
{
 IApiInformationStatics *metadata=NULL; IPowerManagerStatics *power=NULL; IResourceContextStatics2 *resources=NULL;
 IResourceContext *context=NULL,*clone=NULL; IVectorView_HSTRING *languages=NULL; HSTRING type=NULL,language=NULL;
 SYSTEM_POWER_STATUS system; INT32 percent; boolean present; UINT32 count=0; BatteryStatus battery;
 CHECK(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
 CHECK(get_factory(L"Windows.Foundation.Metadata.ApiInformation",&IID_IApiInformationStatics,(void **)&metadata)==S_OK);
 if(metadata) {
  CHECK(WindowsCreateString(L"WineGDK.Unknown.Class",21,&type)==S_OK);
  CHECK(IApiInformationStatics_IsTypePresent(metadata,type,&present)==S_OK && !present); WindowsDeleteString(type);
  CHECK(WindowsCreateString(L"Windows.System.Power.PowerManager",33,&type)==S_OK);
  CHECK(IApiInformationStatics_IsTypePresent(metadata,type,&present)==S_OK && present); WindowsDeleteString(type);
  IApiInformationStatics_Release(metadata);
 }
 CHECK(get_factory(L"Windows.System.Power.PowerManager",&IID_IPowerManagerStatics,(void **)&power)==S_OK);
 if(power) {
  CHECK(GetSystemPowerStatus(&system));
  CHECK(IPowerManagerStatics_get_RemainingChargePercent(power,&percent)==S_OK && percent==(system.BatteryLifePercent==255?-1:system.BatteryLifePercent));
  CHECK(IPowerManagerStatics_get_BatteryStatus(power,&battery)==S_OK);
  if(system.BatteryFlag&128) CHECK(battery==BatteryStatus_NotPresent);
  IPowerManagerStatics_Release(power);
 }
 CHECK(get_factory(L"Windows.ApplicationModel.Resources.Core.ResourceContext",&IID_IResourceContextStatics2,(void **)&resources)==S_OK);
 if(resources) {
  CHECK(IResourceContextStatics2_GetForViewIndependentUse(resources,&context)==S_OK);
  if(context) {
   CHECK(IResourceContext_get_Languages(context,&languages)==S_OK && languages);
   if(languages) { CHECK(IVectorView_HSTRING_get_Size(languages,&count)==S_OK && count>0); IVectorView_HSTRING_Release(languages); languages=NULL; }
   CHECK(IResourceContext_Clone(context,&clone)==S_OK && clone); IResourceContext_Release(context);
   if(clone) {
    CHECK(IResourceContext_Reset(clone)==S_OK);
    CHECK(IResourceContext_get_Languages(clone,&languages)==S_OK && languages);
    if(languages) { CHECK(IVectorView_HSTRING_GetAt(languages,0,&language)==S_OK && WindowsGetStringLen(language)>0); WindowsDeleteString(language); IVectorView_HSTRING_Release(languages); }
    IResourceContext_Release(clone);
   }
  }
  IResourceContextStatics2_Release(resources);
 }
 {
  ILoggingChannelOptionsFactory *options_factory=NULL; ILoggingChannelFactory2 *channel_factory=NULL;
  ILoggingChannelOptions *options=NULL; ILoggingChannel *channel=NULL; ILoggingTarget *target=NULL;
  IClosable *closable=NULL; GUID group={.Data1=0x1234},copy; HSTRING name=NULL;
  CHECK(get_factory(L"Windows.Foundation.Diagnostics.LoggingChannelOptions",&IID_ILoggingChannelOptionsFactory,(void **)&options_factory)==S_OK);
  CHECK(get_factory(L"Windows.Foundation.Diagnostics.LoggingChannel",&IID_ILoggingChannelFactory2,(void **)&channel_factory)==S_OK);
  if(options_factory && channel_factory) {
   CHECK(ILoggingChannelOptionsFactory_Create(options_factory,group,&options)==S_OK);
   CHECK(ILoggingChannelOptions_get_Group(options,&copy)==S_OK && IsEqualGUID(&group,&copy));
   CHECK(WindowsCreateString(L"WineGDK.Probe",13,&name)==S_OK);
   CHECK(ILoggingChannelFactory2_CreateWithOptionsAndId(channel_factory,name,options,group,&channel)==S_OK);
   WindowsDeleteString(name);
   if(channel) {
    CHECK(ILoggingChannel_get_Enabled(channel,&present)==S_OK && !present);
    CHECK(ILoggingChannel_QueryInterface(channel,&IID_ILoggingTarget,(void **)&target)==S_OK);
    if(target) { CHECK(ILoggingTarget_IsEnabled(target,&present)==S_OK && !present); ILoggingTarget_Release(target); }
    CHECK(ILoggingChannel_QueryInterface(channel,&IID_IClosable,(void **)&closable)==S_OK);
    if(closable) { CHECK(IClosable_Close(closable)==S_OK); IClosable_Release(closable); }
    CHECK(ILoggingChannel_LogMessage(channel,NULL)==RO_E_CLOSED); ILoggingChannel_Release(channel);
   }
   ILoggingChannelOptions_Release(options);
  }
  if(options_factory) ILoggingChannelOptionsFactory_Release(options_factory);
  if(channel_factory) ILoggingChannelFactory2_Release(channel_factory);
 }
 {
  IPushNotificationChannelManagerStatics *manager=NULL; IAsyncOperation_PushNotificationChannel *op=NULL;
  IPushNotificationChannel *channel=NULL; IAsyncInfo *info=NULL; AsyncStatus status=Started; HRESULT error=S_OK;
  CHECK(get_factory(L"Windows.Networking.PushNotifications.PushNotificationChannelManager",&IID_IPushNotificationChannelManagerStatics,(void **)&manager)==S_OK);
  if(manager) {
   CHECK(IPushNotificationChannelManagerStatics_CreatePushNotificationChannelForApplicationAsync(manager,&op)==S_OK);
   if(op) {
    CHECK(IAsyncOperation_PushNotificationChannel_QueryInterface(op,&IID_IAsyncInfo,(void **)&info)==S_OK);
    if(info) {
     for(int i=0;i<100 && status==Started;i++) { CHECK(IAsyncInfo_get_Status(info,&status)==S_OK); if(status==Started) Sleep(10); }
     CHECK(status==Error); CHECK(IAsyncInfo_get_ErrorCode(info,&error)==S_OK && error==WPN_E_PLATFORM_UNAVAILABLE);
     IAsyncInfo_Release(info);
    }
    CHECK(IAsyncOperation_PushNotificationChannel_GetResults(op,&channel)==WPN_E_PLATFORM_UNAVAILABLE && !channel);
    IAsyncOperation_PushNotificationChannel_Release(op);
   }
   IPushNotificationChannelManagerStatics_Release(manager);
  }
 }
 RoUninitialize(); printf("platform: %s (%d failures)\n",failures?"FAIL":"PASS",failures); return !!failures;
}
