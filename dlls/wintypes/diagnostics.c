/* WinRT diagnostics objects for hosts without a WinRT tracing session.
 * Channels remain disabled; payloads are neither emitted nor retained.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include "diagnostics_private.h"
#include "wine/winrt_events.h"
WINE_DEFAULT_DEBUG_CHANNEL(wintypes);
static const GUID empty_guid;
struct options { ILoggingChannelOptions iface; LONG ref; GUID group; };
OBJECT_METHODS(options,ILoggingChannelOptions,struct options,(void)impl,L"Windows.Foundation.Diagnostics.LoggingChannelOptions")
static HRESULT WINAPI options_qi(ILoggingChannelOptions *iface,REFIID iid,void **out)
{ if(!out) return E_POINTER; *out=NULL; if(!BASE_QI(ILoggingChannelOptions)) return E_NOINTERFACE; *out=iface; options_addref(iface); return S_OK; }
static HRESULT WINAPI options_get(ILoggingChannelOptions *iface,GUID *out) { if(!out) return E_POINTER; *out=((struct options *)iface)->group; return S_OK; }
static HRESULT WINAPI options_put(ILoggingChannelOptions *iface,GUID group) { ((struct options *)iface)->group=group; return S_OK; }
static const ILoggingChannelOptionsVtbl options_vtbl={OBJECT_VTBL(options),options_get,options_put};
static HRESULT options_create(GUID group,ILoggingChannelOptions **out)
{
 struct options *impl; if(!out) return E_POINTER; *out=NULL;
 if(!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
 impl->iface.lpVtbl=&options_vtbl; impl->ref=1; impl->group=group; *out=&impl->iface; return S_OK;
}
struct channel {
 ILoggingChannel iface; ILoggingChannel2 ILoggingChannel2_iface; ILoggingTarget ILoggingTarget_iface; IClosable IClosable_iface;
 LONG ref; HSTRING name; GUID id; BOOL closed; struct winrt_event enabled;
};
OBJECT_METHODS(channel,ILoggingChannel,struct channel,WindowsDeleteString(impl->name); winrt_event_clear(&impl->enabled),L"Windows.Foundation.Diagnostics.LoggingChannel")
static HRESULT WINAPI channel_qi(ILoggingChannel *iface,REFIID iid,void **out)
{
 struct channel *impl=(void *)iface; if(!out) return E_POINTER; *out=NULL;
 if(BASE_QI(ILoggingChannel)) *out=iface;
 else if(IsEqualGUID(iid,&IID_ILoggingChannel2)) *out=&impl->ILoggingChannel2_iface;
 else if(IsEqualGUID(iid,&IID_ILoggingTarget)) *out=&impl->ILoggingTarget_iface;
 else if(IsEqualGUID(iid,&IID_IClosable)) *out=&impl->IClosable_iface;
 else { FIXME("diagnostic interface %s unsupported\n",debugstr_guid(iid)); return E_NOINTERFACE; }
 channel_addref(iface); return S_OK;
}
static HRESULT WINAPI channel_name(ILoggingChannel *iface,HSTRING *out) { return WindowsDuplicateString(((struct channel *)iface)->name,out); }
static HRESULT WINAPI channel_enabled(ILoggingChannel *iface,boolean *out) { if(!out) return E_POINTER; *out=FALSE; return S_OK; }
static HRESULT WINAPI channel_level(ILoggingChannel *iface,LoggingLevel *out) { if(!out) return E_POINTER; *out=LoggingLevel_Critical; return S_OK; }
static HRESULT WINAPI channel_message(ILoggingChannel *iface,HSTRING message) { return ((struct channel *)iface)->closed?RO_E_CLOSED:S_OK; }
static HRESULT WINAPI channel_message_level(ILoggingChannel *iface,HSTRING message,LoggingLevel level) { return channel_message(iface,message); }
static HRESULT WINAPI channel_pair(ILoggingChannel *iface,HSTRING name,INT32 value) { return channel_message(iface,name); }
static HRESULT WINAPI channel_pair_level(ILoggingChannel *iface,HSTRING name,INT32 value,LoggingLevel level) { return channel_message(iface,name); }
static HRESULT WINAPI channel_add(ILoggingChannel *iface,ITypedEventHandler_LoggingChannel_IInspectable *handler,EventRegistrationToken *token) { return winrt_event_add(&((struct channel *)iface)->enabled,(IUnknown *)handler,token); }
static HRESULT WINAPI channel_remove(ILoggingChannel *iface,EventRegistrationToken token) { return winrt_event_remove(&((struct channel *)iface)->enabled,token); }
static const ILoggingChannelVtbl channel_vtbl={OBJECT_VTBL(channel),channel_name,channel_enabled,channel_level,channel_message,channel_message_level,channel_pair,channel_pair_level,channel_add,channel_remove};
DEFINE_IINSPECTABLE(channel2,ILoggingChannel2,struct channel,iface)
static HRESULT WINAPI channel_id(ILoggingChannel2 *iface,GUID *out) { if(!out) return E_POINTER; *out=impl_from_ILoggingChannel2(iface)->id; return S_OK; }
static const ILoggingChannel2Vtbl channel2_vtbl={channel2_QueryInterface,channel2_AddRef,channel2_Release,channel2_GetIids,channel2_GetRuntimeClassName,channel2_GetTrustLevel,channel_id};
DEFINE_IINSPECTABLE(closable,IClosable,struct channel,iface)
static HRESULT WINAPI channel_close(IClosable *iface) { struct channel *impl=impl_from_IClosable(iface); impl->closed=TRUE; winrt_event_clear(&impl->enabled); return S_OK; }
static const IClosableVtbl closable_vtbl={closable_QueryInterface,closable_AddRef,closable_Release,closable_GetIids,closable_GetRuntimeClassName,closable_GetTrustLevel,channel_close};
DEFINE_IINSPECTABLE(target,ILoggingTarget,struct channel,iface)
static HRESULT WINAPI target_enabled(ILoggingTarget *iface,boolean *out) { return channel_enabled(&impl_from_ILoggingTarget(iface)->iface,out); }
static HRESULT WINAPI target_enabled_level(ILoggingTarget *iface,LoggingLevel level,boolean *out) { return target_enabled(iface,out); }
static HRESULT WINAPI target_enabled_keywords(ILoggingTarget *iface,LoggingLevel level,INT64 keywords,boolean *out) { return target_enabled(iface,out); }
static HRESULT WINAPI target_event(ILoggingTarget *iface,HSTRING name) { return channel_message(&impl_from_ILoggingTarget(iface)->iface,name); }
static HRESULT WINAPI target_fields(ILoggingTarget *iface,HSTRING name,IInspectable *fields) { return target_event(iface,name); }
static HRESULT WINAPI target_level(ILoggingTarget *iface,HSTRING name,IInspectable *fields,LoggingLevel level) { return target_event(iface,name); }
static HRESULT WINAPI target_options(ILoggingTarget *iface,HSTRING name,IInspectable *fields,LoggingLevel level,IInspectable *options) { return target_event(iface,name); }
static HRESULT WINAPI target_activity(ILoggingTarget *iface,HSTRING name,IInspectable **out) { if(!out) return E_POINTER; *out=NULL; return E_NOTIMPL; }
static HRESULT WINAPI target_activity_fields(ILoggingTarget *iface,HSTRING name,IInspectable *fields,IInspectable **out) { return target_activity(iface,name,out); }
static HRESULT WINAPI target_activity_level(ILoggingTarget *iface,HSTRING name,IInspectable *fields,LoggingLevel level,IInspectable **out) { return target_activity(iface,name,out); }
static HRESULT WINAPI target_activity_options(ILoggingTarget *iface,HSTRING name,IInspectable *fields,LoggingLevel level,IInspectable *options,IInspectable **out) { return target_activity(iface,name,out); }
static const ILoggingTargetVtbl target_vtbl={target_QueryInterface,target_AddRef,target_Release,target_GetIids,target_GetRuntimeClassName,target_GetTrustLevel,target_enabled,target_enabled_level,target_enabled_keywords,target_event,target_fields,target_level,target_options,target_activity,target_activity_fields,target_activity_level,target_activity_options};
static HRESULT channel_create(HSTRING name,GUID id,ILoggingChannel **out)
{
 struct channel *impl; HRESULT hr; if(!out) return E_POINTER; *out=NULL; if(!WindowsGetStringLen(name)) return E_INVALIDARG;
 if(!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
 impl->iface.lpVtbl=&channel_vtbl; impl->ILoggingChannel2_iface.lpVtbl=&channel2_vtbl;
 impl->ILoggingTarget_iface.lpVtbl=&target_vtbl; impl->IClosable_iface.lpVtbl=&closable_vtbl; impl->ref=1; impl->id=id;
 if(FAILED(hr=WindowsDuplicateString(name,&impl->name))) { channel_release(&impl->iface); return hr; }
 *out=&impl->iface; return S_OK;
}
struct log_options { ILoggingOptions iface; LONG ref; SRWLOCK lock; INT64 Keywords; INT32 Tags; INT16 Task; LoggingOpcode Opcode; GUID ActivityId; GUID RelatedActivityId; };
OBJECT_METHODS(log_options,ILoggingOptions,struct log_options,(void)impl,L"Windows.Foundation.Diagnostics.LoggingOptions")
static HRESULT WINAPI log_options_qi(ILoggingOptions *iface,REFIID iid,void **out)
{ if(!out) return E_POINTER; *out=NULL; if(!BASE_QI(ILoggingOptions)) return E_NOINTERFACE; *out=iface; log_options_addref(iface); return S_OK; }
static HRESULT WINAPI log_get_Keywords(ILoggingOptions *iface,INT64 *out) { struct log_options *impl=(void *)iface; if(!out) return E_POINTER; AcquireSRWLockShared(&impl->lock); *out=impl->Keywords; ReleaseSRWLockShared(&impl->lock); return S_OK; }
static HRESULT WINAPI log_put_Keywords(ILoggingOptions *iface,INT64 value) { struct log_options *impl=(void *)iface; AcquireSRWLockExclusive(&impl->lock); impl->Keywords=value; ReleaseSRWLockExclusive(&impl->lock); return S_OK; }
static HRESULT WINAPI log_get_Tags(ILoggingOptions *iface,INT32 *out) { struct log_options *impl=(void *)iface; if(!out) return E_POINTER; AcquireSRWLockShared(&impl->lock); *out=impl->Tags; ReleaseSRWLockShared(&impl->lock); return S_OK; }
static HRESULT WINAPI log_put_Tags(ILoggingOptions *iface,INT32 value) { struct log_options *impl=(void *)iface; AcquireSRWLockExclusive(&impl->lock); impl->Tags=value; ReleaseSRWLockExclusive(&impl->lock); return S_OK; }
static HRESULT WINAPI log_get_Task(ILoggingOptions *iface,INT16 *out) { struct log_options *impl=(void *)iface; if(!out) return E_POINTER; AcquireSRWLockShared(&impl->lock); *out=impl->Task; ReleaseSRWLockShared(&impl->lock); return S_OK; }
static HRESULT WINAPI log_put_Task(ILoggingOptions *iface,INT16 value) { struct log_options *impl=(void *)iface; AcquireSRWLockExclusive(&impl->lock); impl->Task=value; ReleaseSRWLockExclusive(&impl->lock); return S_OK; }
static HRESULT WINAPI log_get_Opcode(ILoggingOptions *iface,LoggingOpcode *out) { struct log_options *impl=(void *)iface; if(!out) return E_POINTER; AcquireSRWLockShared(&impl->lock); *out=impl->Opcode; ReleaseSRWLockShared(&impl->lock); return S_OK; }
static HRESULT WINAPI log_put_Opcode(ILoggingOptions *iface,LoggingOpcode value) { struct log_options *impl=(void *)iface; AcquireSRWLockExclusive(&impl->lock); impl->Opcode=value; ReleaseSRWLockExclusive(&impl->lock); return S_OK; }
static HRESULT WINAPI log_get_ActivityId(ILoggingOptions *iface,GUID *out) { struct log_options *impl=(void *)iface; if(!out) return E_POINTER; AcquireSRWLockShared(&impl->lock); *out=impl->ActivityId; ReleaseSRWLockShared(&impl->lock); return S_OK; }
static HRESULT WINAPI log_put_ActivityId(ILoggingOptions *iface,GUID value) { struct log_options *impl=(void *)iface; AcquireSRWLockExclusive(&impl->lock); impl->ActivityId=value; ReleaseSRWLockExclusive(&impl->lock); return S_OK; }
static HRESULT WINAPI log_get_RelatedActivityId(ILoggingOptions *iface,GUID *out) { struct log_options *impl=(void *)iface; if(!out) return E_POINTER; AcquireSRWLockShared(&impl->lock); *out=impl->RelatedActivityId; ReleaseSRWLockShared(&impl->lock); return S_OK; }
static HRESULT WINAPI log_put_RelatedActivityId(ILoggingOptions *iface,GUID value) { struct log_options *impl=(void *)iface; AcquireSRWLockExclusive(&impl->lock); impl->RelatedActivityId=value; ReleaseSRWLockExclusive(&impl->lock); return S_OK; }
static const ILoggingOptionsVtbl log_options_vtbl={OBJECT_VTBL(log_options),log_get_Keywords,log_put_Keywords,log_get_Tags,log_put_Tags,log_get_Task,log_put_Task,log_get_Opcode,log_put_Opcode,log_get_ActivityId,log_put_ActivityId,log_get_RelatedActivityId,log_put_RelatedActivityId};
static HRESULT log_options_create(INT64 keywords,ILoggingOptions **out)
{
 struct log_options *impl; if(!out) return E_POINTER; *out=NULL;
 if(!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
 impl->iface.lpVtbl=&log_options_vtbl; impl->ref=1; impl->Keywords=keywords; *out=&impl->iface; return S_OK;
}
struct factory {
 IActivationFactory iface; ILoggingChannelOptionsFactory ILoggingChannelOptionsFactory_iface;
 ILoggingChannelFactory ILoggingChannelFactory_iface; ILoggingChannelFactory2 ILoggingChannelFactory2_iface; LONG ref; UINT options; ILoggingOptionsFactory ILoggingOptionsFactory_iface;
};
static HRESULT WINAPI factory_qi(IActivationFactory *iface,REFIID iid,void **out)
{
 struct factory *impl=(void *)iface; if(!out) return E_POINTER; *out=NULL;
 if(BASE_QI(IActivationFactory)) *out=iface;
 else if(impl->options==1 && IsEqualGUID(iid,&IID_ILoggingChannelOptionsFactory)) *out=&impl->ILoggingChannelOptionsFactory_iface;
 else if(!impl->options && IsEqualGUID(iid,&IID_ILoggingChannelFactory)) *out=&impl->ILoggingChannelFactory_iface;
 else if(!impl->options && IsEqualGUID(iid,&IID_ILoggingChannelFactory2)) *out=&impl->ILoggingChannelFactory2_iface;
 else if(impl->options==2 && IsEqualGUID(iid,&IID_ILoggingOptionsFactory)) *out=&impl->ILoggingOptionsFactory_iface;
 else return E_NOINTERFACE;
 IActivationFactory_AddRef(iface); return S_OK;
}
static ULONG WINAPI factory_addref(IActivationFactory *iface) { return InterlockedIncrement(&((struct factory *)iface)->ref); }
static ULONG WINAPI factory_release(IActivationFactory *iface) { return InterlockedDecrement(&((struct factory *)iface)->ref); }
static HRESULT WINAPI factory_iids(IActivationFactory *iface,ULONG *count,IID **out) { if(!count || !out) return E_POINTER; *count=0; *out=NULL; return E_NOTIMPL; }
static HRESULT WINAPI factory_class(IActivationFactory *iface,HSTRING *out) { return string_from_wide(((struct factory *)iface)->options==2?L"Windows.Foundation.Diagnostics.LoggingOptions":((struct factory *)iface)->options?L"Windows.Foundation.Diagnostics.LoggingChannelOptions":L"Windows.Foundation.Diagnostics.LoggingChannel",out); }
static HRESULT WINAPI factory_trust(IActivationFactory *iface,TrustLevel *out) { if(!out) return E_POINTER; *out=BaseTrust; return S_OK; }
static HRESULT WINAPI factory_activate(IActivationFactory *iface,IInspectable **out) { if(!out) return E_POINTER; *out=NULL; if(!((struct factory *)iface)->options) return E_ILLEGAL_METHOD_CALL; if(((struct factory *)iface)->options==2) return log_options_create(0,(ILoggingOptions **)out); return options_create(empty_guid,(ILoggingChannelOptions **)out); }
static const IActivationFactoryVtbl factory_vtbl={OBJECT_VTBL(factory),factory_activate};
DEFINE_IINSPECTABLE(options_factory,ILoggingChannelOptionsFactory,struct factory,iface)
static HRESULT WINAPI options_factory_create(ILoggingChannelOptionsFactory *iface,GUID group,ILoggingChannelOptions **out) { return options_create(group,out); }
static const ILoggingChannelOptionsFactoryVtbl options_factory_vtbl={options_factory_QueryInterface,options_factory_AddRef,options_factory_Release,options_factory_GetIids,options_factory_GetRuntimeClassName,options_factory_GetTrustLevel,options_factory_create};
DEFINE_IINSPECTABLE(channel_factory,ILoggingChannelFactory,struct factory,iface)
static HRESULT WINAPI channel_factory_create(ILoggingChannelFactory *iface,HSTRING name,ILoggingChannel **out) { return channel_create(name,empty_guid,out); }
static const ILoggingChannelFactoryVtbl channel_factory_vtbl={channel_factory_QueryInterface,channel_factory_AddRef,channel_factory_Release,channel_factory_GetIids,channel_factory_GetRuntimeClassName,channel_factory_GetTrustLevel,channel_factory_create};
DEFINE_IINSPECTABLE(channel_factory2,ILoggingChannelFactory2,struct factory,iface)
static HRESULT WINAPI channel_factory_options(ILoggingChannelFactory2 *iface,HSTRING name,ILoggingChannelOptions *options,ILoggingChannel **out) { return channel_create(name,empty_guid,out); }
static HRESULT WINAPI channel_factory_id(ILoggingChannelFactory2 *iface,HSTRING name,ILoggingChannelOptions *options,GUID id,ILoggingChannel **out) { return channel_create(name,id,out); }
static const ILoggingChannelFactory2Vtbl channel_factory2_vtbl={channel_factory2_QueryInterface,channel_factory2_AddRef,channel_factory2_Release,channel_factory2_GetIids,channel_factory2_GetRuntimeClassName,channel_factory2_GetTrustLevel,channel_factory_options,channel_factory_id};
DEFINE_IINSPECTABLE(log_options_factory,ILoggingOptionsFactory,struct factory,iface)
static HRESULT WINAPI log_options_factory_create(ILoggingOptionsFactory *iface,INT64 keywords,ILoggingOptions **out) { return log_options_create(keywords,out); }
static const ILoggingOptionsFactoryVtbl log_options_factory_vtbl={log_options_factory_QueryInterface,log_options_factory_AddRef,log_options_factory_Release,log_options_factory_GetIids,log_options_factory_GetRuntimeClassName,log_options_factory_GetTrustLevel,log_options_factory_create};
static struct factory channel_factory_instance={{&factory_vtbl},{&options_factory_vtbl},{&channel_factory_vtbl},{&channel_factory2_vtbl},1,0,{&log_options_factory_vtbl}};
static struct factory options_factory_instance={{&factory_vtbl},{&options_factory_vtbl},{&channel_factory_vtbl},{&channel_factory2_vtbl},1,1,{&log_options_factory_vtbl}};
IActivationFactory *logging_channel_factory=&channel_factory_instance.iface,*logging_channel_options_factory=&options_factory_instance.iface;

static struct factory log_options_factory_instance={{&factory_vtbl},{&options_factory_vtbl},{&channel_factory_vtbl},{&channel_factory2_vtbl},1,2,{&log_options_factory_vtbl}};
IActivationFactory *logging_options_factory=&log_options_factory_instance.iface;
