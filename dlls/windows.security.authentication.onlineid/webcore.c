/* Web Account Manager bridge to Xodus. SPDX-License-Identifier: LGPL-2.1-or-later */
#include "webcore_private.h"
WINE_DEFAULT_DEBUG_CHANNEL(onlineid);
struct provider {
    IWebAccountProvider iface;
    IWebAccountProvider2 IWebAccountProvider2_iface;
    LONG ref;
    HSTRING id,authority;
};
OBJECT_METHODS(provider,IWebAccountProvider,struct provider,WindowsDeleteString(impl->id); WindowsDeleteString(impl->authority),L"Windows.Security.Credentials.WebAccountProvider")
static HRESULT WINAPI provider_qi(IWebAccountProvider *iface,REFIID iid,void **out)
{
    struct provider *impl=(void *)iface;
    if (!out) return E_POINTER; *out=NULL;
    if (BASE_QI(IWebAccountProvider)) *out=iface;
    else if (IsEqualGUID(iid,&IID_IWebAccountProvider2)) *out=&impl->IWebAccountProvider2_iface;
    else { FIXME("provider interface %s unsupported\n",debugstr_guid(iid)); return E_NOINTERFACE; }
    provider_addref(iface); return S_OK;
}
static HRESULT WINAPI provider_id(IWebAccountProvider *iface,HSTRING *out) { return WindowsDuplicateString(((struct provider *)iface)->id,out); }
static HRESULT WINAPI provider_name(IWebAccountProvider *iface,HSTRING *out) { return string_from_wide(L"Microsoft",out); }
static HRESULT WINAPI provider_icon(IWebAccountProvider *iface,IUriRuntimeClass **out) { if (!out) return E_POINTER; *out=NULL; return S_OK; }
static const IWebAccountProviderVtbl provider_vtbl={OBJECT_VTBL(provider),provider_id,provider_name,provider_icon};
DEFINE_IINSPECTABLE(provider2,IWebAccountProvider2,struct provider,iface)
static HRESULT WINAPI provider_purpose(IWebAccountProvider2 *iface,HSTRING *out) { return string_from_wide(L"Microsoft account",out); }
static HRESULT WINAPI provider_authority(IWebAccountProvider2 *iface,HSTRING *out) { return WindowsDuplicateString(impl_from_IWebAccountProvider2(iface)->authority,out); }
static const IWebAccountProvider2Vtbl provider2_vtbl={provider2_QueryInterface,provider2_AddRef,provider2_Release,provider2_GetIids,provider2_GetRuntimeClassName,provider2_GetTrustLevel,provider_purpose,provider_authority};
static HRESULT provider_create(HSTRING id,HSTRING authority,IWebAccountProvider **out)
{
    struct provider *impl; HRESULT hr;
    *out=NULL;
    if (!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl=&provider_vtbl; impl->IWebAccountProvider2_iface.lpVtbl=&provider2_vtbl; impl->ref=1;
    hr=WindowsDuplicateString(id,&impl->id);
    if (SUCCEEDED(hr)) hr=WindowsDuplicateString(authority,&impl->authority);
    if (FAILED(hr)) { provider_release(&impl->iface); return hr; }
    *out=&impl->iface; return S_OK;
}
struct request {
    IWebTokenRequest iface;
    IWebTokenRequest2 IWebTokenRequest2_iface;
    LONG ref;
    IWebAccountProvider *provider;
    HSTRING scope,client;
    WebTokenRequestPromptType prompt;
    IMap_HSTRING_HSTRING *properties,*app_properties;
};
static void request_destroy(struct request *impl)
{
    if(impl->provider) IWebAccountProvider_Release(impl->provider);
    WindowsDeleteString(impl->scope); WindowsDeleteString(impl->client);
    if(impl->properties) IMap_HSTRING_HSTRING_Release(impl->properties);
    if(impl->app_properties) IMap_HSTRING_HSTRING_Release(impl->app_properties);
}
OBJECT_METHODS(request,IWebTokenRequest,struct request,request_destroy(impl),L"Windows.Security.Authentication.Web.Core.WebTokenRequest")
static HRESULT WINAPI request_qi(IWebTokenRequest *iface,REFIID iid,void **out)
{
    struct request *impl=(void *)iface; if(!out) return E_POINTER; *out=NULL;
    if(BASE_QI(IWebTokenRequest)) *out=iface;
    else if(IsEqualGUID(iid,&IID_IWebTokenRequest2)) *out=&impl->IWebTokenRequest2_iface;
    else { FIXME("request interface %s unsupported\n",debugstr_guid(iid)); return E_NOINTERFACE; }
    request_addref(iface); return S_OK;
}
static HRESULT WINAPI request_provider(IWebTokenRequest *iface,IWebAccountProvider **out)
{ if(!out) return E_POINTER; *out=((struct request *)iface)->provider; IWebAccountProvider_AddRef(*out); return S_OK; }
static HRESULT WINAPI request_scope(IWebTokenRequest *iface,HSTRING *out) { return WindowsDuplicateString(((struct request *)iface)->scope,out); }
static HRESULT WINAPI request_client(IWebTokenRequest *iface,HSTRING *out) { return WindowsDuplicateString(((struct request *)iface)->client,out); }
static HRESULT WINAPI request_prompt(IWebTokenRequest *iface,WebTokenRequestPromptType *out) { if(!out) return E_POINTER; *out=((struct request *)iface)->prompt; return S_OK; }
static HRESULT WINAPI request_properties(IWebTokenRequest *iface,IMap_HSTRING_HSTRING **out)
{ if(!out) return E_POINTER; *out=((struct request *)iface)->properties; IMap_HSTRING_HSTRING_AddRef(*out); return S_OK; }
static const IWebTokenRequestVtbl request_vtbl={OBJECT_VTBL(request),request_provider,request_scope,request_client,request_prompt,request_properties};
DEFINE_IINSPECTABLE(request2,IWebTokenRequest2,struct request,iface)
static HRESULT WINAPI request_app_properties(IWebTokenRequest2 *iface,IMap_HSTRING_HSTRING **out)
{ if(!out) return E_POINTER; *out=impl_from_IWebTokenRequest2(iface)->app_properties; IMap_HSTRING_HSTRING_AddRef(*out); return S_OK; }
static const IWebTokenRequest2Vtbl request2_vtbl={request2_QueryInterface,request2_AddRef,request2_Release,request2_GetIids,request2_GetRuntimeClassName,request2_GetTrustLevel,request_app_properties};

struct broker_factory {
    IActivationFactory iface;
    IWebAuthenticationCoreManagerStatics IWebAuthenticationCoreManagerStatics_iface;
    IWebTokenRequestFactory IWebTokenRequestFactory_iface;
    LONG ref;
    BOOL request;
};
static HRESULT WINAPI factory_qi(IActivationFactory *iface,REFIID iid,void **out)
{
    struct broker_factory *impl=(void *)iface; if(!out) return E_POINTER; *out=NULL;
    if(BASE_QI(IActivationFactory)) *out=iface;
    else if(!impl->request && IsEqualGUID(iid,&IID_IWebAuthenticationCoreManagerStatics)) *out=&impl->IWebAuthenticationCoreManagerStatics_iface;
    else if(impl->request && IsEqualGUID(iid,&IID_IWebTokenRequestFactory)) *out=&impl->IWebTokenRequestFactory_iface;
    else { FIXME("broker factory interface %s unsupported\n",debugstr_guid(iid)); return E_NOINTERFACE; }
    IActivationFactory_AddRef(iface); return S_OK;
}
static ULONG WINAPI factory_addref(IActivationFactory *iface) { return InterlockedIncrement(&((struct broker_factory *)iface)->ref); }
static ULONG WINAPI factory_release(IActivationFactory *iface) { return InterlockedDecrement(&((struct broker_factory *)iface)->ref); }
static HRESULT WINAPI factory_iids(IActivationFactory *iface,ULONG *count,IID **out) { if(!count || !out) return E_POINTER; *count=0; *out=NULL; return E_NOTIMPL; }
static HRESULT WINAPI factory_class(IActivationFactory *iface,HSTRING *out)
{ return string_from_wide(((struct broker_factory *)iface)->request?L"Windows.Security.Authentication.Web.Core.WebTokenRequest":L"Windows.Security.Authentication.Web.Core.WebAuthenticationCoreManager",out); }
static HRESULT WINAPI factory_trust(IActivationFactory *iface,TrustLevel *out) { if(!out) return E_POINTER; *out=BaseTrust; return S_OK; }
static HRESULT WINAPI factory_activate(IActivationFactory *iface,IInspectable **out) { if(!out) return E_POINTER; *out=NULL; return E_ILLEGAL_METHOD_CALL; }
static const IActivationFactoryVtbl factory_vtbl={OBJECT_VTBL(factory),factory_activate};
DEFINE_IINSPECTABLE(create,IWebTokenRequestFactory,struct broker_factory,iface)
static HRESULT WINAPI create_prompt(IWebTokenRequestFactory *iface,IWebAccountProvider *provider,HSTRING scope,HSTRING client,WebTokenRequestPromptType prompt,IWebTokenRequest **out)
{
    struct request *impl; HRESULT hr;
    if(!out) return E_POINTER; *out=NULL;
    if(!provider || (prompt!=WebTokenRequestPromptType_Default && prompt!=WebTokenRequestPromptType_ForceAuthentication)) return E_INVALIDARG;
    TRACE("create request scope %s, client %s, prompt %u\n",debugstr_hstring(scope),debugstr_hstring(client),prompt);
    if(!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl=&request_vtbl; impl->IWebTokenRequest2_iface.lpVtbl=&request2_vtbl; impl->ref=1;
    impl->provider=provider; IWebAccountProvider_AddRef(provider); impl->prompt=prompt;
    hr=WindowsDuplicateString(scope,&impl->scope);
    if(SUCCEEDED(hr)) hr=WindowsDuplicateString(client,&impl->client);
    if(SUCCEEDED(hr)) hr=string_map_create(&impl->properties);
    if(SUCCEEDED(hr)) hr=string_map_create(&impl->app_properties);
    if(FAILED(hr)) { request_release(&impl->iface); return hr; }
    *out=&impl->iface; return S_OK;
}
static HRESULT WINAPI create_request(IWebTokenRequestFactory *iface,IWebAccountProvider *provider,HSTRING scope,HSTRING client,IWebTokenRequest **out) { return create_prompt(iface,provider,scope,client,WebTokenRequestPromptType_Default,out); }
static HRESULT WINAPI create_provider(IWebTokenRequestFactory *iface,IWebAccountProvider *provider,IWebTokenRequest **out) { return create_request(iface,provider,NULL,NULL,out); }
static HRESULT WINAPI create_scope(IWebTokenRequestFactory *iface,IWebAccountProvider *provider,HSTRING scope,IWebTokenRequest **out) { return create_request(iface,provider,scope,NULL,out); }
static const IWebTokenRequestFactoryVtbl create_vtbl={create_QueryInterface,create_AddRef,create_Release,create_GetIids,create_GetRuntimeClassName,create_GetTrustLevel,create_request,create_prompt,create_provider,create_scope};

DEFINE_IINSPECTABLE(manager,IWebAuthenticationCoreManagerStatics,struct broker_factory,iface)
static HRESULT return_object(IUnknown *invoker,IUnknown *param,PROPVARIANT *result,BOOL async)
{ result->vt=VT_UNKNOWN; result->punkVal=param; if(param) IUnknown_AddRef(param); return S_OK; }
static HRESULT WINAPI manager_provider_authority(IWebAuthenticationCoreManagerStatics *iface,HSTRING id,HSTRING authority,IAsyncOperation_WebAccountProvider **out)
{
    IWebAccountProvider *provider=NULL; HRESULT hr=S_OK; const WCHAR *name=WindowsGetStringRawBuffer(id,NULL),*auth=WindowsGetStringRawBuffer(authority,NULL);
    TRACE("find provider %s authority %s\n",debugstr_hstring(id),debugstr_hstring(authority));
    if(!out) return E_POINTER; *out=NULL;
    if(!wcscmp(name,L"https://xsts.auth.xboxlive.com") && (!*auth || !wcscmp(auth,L"consumers"))) hr=provider_create(id,authority,&provider);
    if(SUCCEEDED(hr)) hr=async_operation_inspectable_create(&IID_IAsyncOperation_WebAccountProvider,NULL,(IUnknown *)provider,return_object,(IAsyncOperation_IInspectable **)out);
    if(provider) IWebAccountProvider_Release(provider); return hr;
}
static HRESULT WINAPI manager_provider(IWebAuthenticationCoreManagerStatics *iface,HSTRING id,IAsyncOperation_WebAccountProvider **out) { return manager_provider_authority(iface,id,NULL,out); }
static HRESULT WINAPI manager_account(IWebAuthenticationCoreManagerStatics *iface,IWebAccountProvider *provider,HSTRING id,IAsyncOperation_WebAccount **out)
{
    IWebAccount *account=NULL; HRESULT hr=web_account_find(provider,id,&account);
    TRACE("find cached account: found %u, hr %#lx\n",!!account,hr);
    if(SUCCEEDED(hr)) hr=async_operation_inspectable_create(&IID_IAsyncOperation_WebAccount,NULL,(IUnknown *)account,return_object,(IAsyncOperation_IInspectable **)out);
    if(account) IWebAccount_Release(account); return hr;
}
static HRESULT token_callback(IUnknown *invoker,IUnknown *param,PROPVARIANT *result,BOOL async)
{
    struct xodus_token token;
    IWebTokenRequestResult *response=NULL;
    HRESULT hr;
    if (!async) return 0x103; /* STATUS_PENDING: run blocking IPC on the worker. */
    hr=xodus_request((IWebTokenRequest *)invoker,&token);
    if (SUCCEEDED(hr) && param)
    {
        IWebAccount2 *account2 = NULL;
        IWebAccountProvider *account_provider = NULL, *request_provider = NULL;
        HSTRING id = NULL, account_provider_id = NULL, request_provider_id = NULL;
        INT32 order = 1;
        hr = IWebAccount_QueryInterface((IWebAccount *)param, &IID_IWebAccount2, (void **)&account2);
        if (SUCCEEDED(hr)) hr = IWebAccount2_get_Id(account2, &id);
        if (SUCCEEDED(hr)) hr = WindowsCompareStringOrdinal(id, token.xuid, &order);
        if (SUCCEEDED(hr) && order) hr = E_INVALIDARG;
        if (SUCCEEDED(hr)) hr = IWebAccount_get_WebAccountProvider((IWebAccount *)param, &account_provider);
        if (SUCCEEDED(hr)) hr = IWebTokenRequest_get_WebAccountProvider((IWebTokenRequest *)invoker, &request_provider);
        if (SUCCEEDED(hr)) hr = IWebAccountProvider_get_Id(account_provider, &account_provider_id);
        if (SUCCEEDED(hr)) hr = IWebAccountProvider_get_Id(request_provider, &request_provider_id);
        if (SUCCEEDED(hr)) hr = WindowsCompareStringOrdinal(account_provider_id, request_provider_id, &order);
        if (SUCCEEDED(hr) && order) hr = E_INVALIDARG;
        WindowsDeleteString(id);
        WindowsDeleteString(account_provider_id);
        WindowsDeleteString(request_provider_id);
        if (account2) IWebAccount2_Release(account2);
        if (account_provider) IWebAccountProvider_Release(account_provider);
        if (request_provider) IWebAccountProvider_Release(request_provider);
    }
    if(SUCCEEDED(hr)) hr=web_token_result_create((IWebTokenRequest *)invoker,&token,&response);
    TRACE("token result created: %#lx\n",hr);
    xodus_token_clear(&token);
    if(SUCCEEDED(hr)) { result->vt=VT_UNKNOWN; result->punkVal=(IUnknown *)response; }
    return hr;
}
static HRESULT WINAPI manager_token(IWebAuthenticationCoreManagerStatics *iface,IWebTokenRequest *request,IAsyncOperation_WebTokenRequestResult **out)
{
    if(!request) return E_INVALIDARG;
    TRACE("request token\n");
    return async_operation_inspectable_create(&IID_IAsyncOperation_WebTokenRequestResult,(IUnknown *)request,NULL,token_callback,(IAsyncOperation_IInspectable **)out);
}
static HRESULT WINAPI manager_token_account(IWebAuthenticationCoreManagerStatics *iface,IWebTokenRequest *request,IWebAccount *account,IAsyncOperation_WebTokenRequestResult **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!request || !account) return E_INVALIDARG;
    return async_operation_inspectable_create(&IID_IAsyncOperation_WebTokenRequestResult,
        (IUnknown *)request, (IUnknown *)account, token_callback, (IAsyncOperation_IInspectable **)out);
}
static const IWebAuthenticationCoreManagerStaticsVtbl manager_vtbl={manager_QueryInterface,manager_AddRef,manager_Release,manager_GetIids,manager_GetRuntimeClassName,manager_GetTrustLevel,manager_token,manager_token_account,manager_token,manager_token_account,manager_account,manager_provider,manager_provider_authority};
static struct broker_factory manager_instance={{&factory_vtbl},{&manager_vtbl},{&create_vtbl},1,FALSE};
static struct broker_factory request_instance={{&factory_vtbl},{&manager_vtbl},{&create_vtbl},1,TRUE};
IActivationFactory *webcore_factory=&manager_instance.iface;
IActivationFactory *webrequest_factory=&request_instance.iface;
