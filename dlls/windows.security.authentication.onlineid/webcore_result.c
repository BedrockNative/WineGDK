/* Web account and token results backed by authenticated Xodus responses.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include "webcore_private.h"
WINE_DEFAULT_DEBUG_CHANNEL(onlineid);
static HRESULT insert_property(IMap_HSTRING_HSTRING *map,const WCHAR *key,HSTRING value)
{
    HSTRING name; boolean replaced; HRESULT hr=string_from_wide(key,&name);
    if(SUCCEEDED(hr)) { hr=IMap_HSTRING_HSTRING_Insert(map,name,value,&replaced); WindowsDeleteString(name); }
    return hr;
}
struct account {
    IWebAccount iface;
    IWebAccount2 IWebAccount2_iface;
    LONG ref;
    IWebAccountProvider *provider;
    HSTRING id,name;
    IMapView_HSTRING_HSTRING *properties;
};
static void account_destroy(struct account *impl)
{
    if(impl->provider) IWebAccountProvider_Release(impl->provider);
    WindowsDeleteString(impl->id); WindowsDeleteString(impl->name);
    if(impl->properties) IMapView_HSTRING_HSTRING_Release(impl->properties);
}
OBJECT_METHODS(account,IWebAccount,struct account,account_destroy(impl),L"Windows.Security.Credentials.WebAccount")
static HRESULT WINAPI account_qi(IWebAccount *iface,REFIID iid,void **out)
{
    struct account *impl=(void *)iface; if(!out) return E_POINTER; *out=NULL;
    if(BASE_QI(IWebAccount)) *out=iface;
    else if(IsEqualGUID(iid,&IID_IWebAccount2)) *out=&impl->IWebAccount2_iface;
    else { FIXME("account interface %s unsupported\n",debugstr_guid(iid)); return E_NOINTERFACE; }
    account_addref(iface); return S_OK;
}
static HRESULT WINAPI account_provider(IWebAccount *iface,IWebAccountProvider **out)
{ if(!out) return E_POINTER; *out=((struct account *)iface)->provider; IWebAccountProvider_AddRef(*out); return S_OK; }
static HRESULT WINAPI account_name(IWebAccount *iface,HSTRING *out) { return WindowsDuplicateString(((struct account *)iface)->name,out); }
static HRESULT WINAPI account_state(IWebAccount *iface,WebAccountState *out) { if(!out) return E_POINTER; *out=WebAccountState_Connected; return S_OK; }
static const IWebAccountVtbl account_vtbl={OBJECT_VTBL(account),account_provider,account_name,account_state};
DEFINE_IINSPECTABLE(account2,IWebAccount2,struct account,iface)
static HRESULT WINAPI account_id(IWebAccount2 *iface,HSTRING *out) { return WindowsDuplicateString(impl_from_IWebAccount2(iface)->id,out); }
static HRESULT WINAPI account_properties(IWebAccount2 *iface,IMapView_HSTRING_HSTRING **out)
{ if(!out) return E_POINTER; *out=impl_from_IWebAccount2(iface)->properties; IMapView_HSTRING_HSTRING_AddRef(*out); return S_OK; }
static HRESULT WINAPI account_picture(IWebAccount2 *iface,WebAccountPictureSize size,IAsyncOperation_IRandomAccessStream **out) { if(!out) return E_POINTER; *out=NULL; return E_NOTIMPL; }
static HRESULT WINAPI account_signout(IWebAccount2 *iface,IAsyncAction **out) { if(!out) return E_POINTER; *out=NULL; return E_NOTIMPL; }
static HRESULT WINAPI account_signout_client(IWebAccount2 *iface,HSTRING client,IAsyncAction **out) { return account_signout(iface,out); }
static const IWebAccount2Vtbl account2_vtbl={account2_QueryInterface,account2_AddRef,account2_Release,account2_GetIids,account2_GetRuntimeClassName,account2_GetTrustLevel,account_id,account_properties,account_picture,account_signout,account_signout_client};
static SRWLOCK account_lock=SRWLOCK_INIT;
static IWebAccount *cached_account;
HRESULT web_account_find(IWebAccountProvider *provider,HSTRING id,IWebAccount **out)
{
    IWebAccount2 *account2=NULL; HSTRING cached_id=NULL,requested_provider=NULL,cached_provider=NULL;
    IWebAccountProvider *account_provider=NULL; INT32 order; HRESULT hr=S_OK;
    if(!out) return E_POINTER; *out=NULL;
    if(!provider) return E_INVALIDARG;
    AcquireSRWLockShared(&account_lock);
    if(cached_account && SUCCEEDED(IWebAccount_QueryInterface(cached_account,&IID_IWebAccount2,(void **)&account2))) {
        hr=IWebAccount2_get_Id(account2,&cached_id);
        if(SUCCEEDED(hr)) hr=IWebAccount_get_WebAccountProvider(cached_account,&account_provider);
        if(SUCCEEDED(hr)) hr=IWebAccountProvider_get_Id(account_provider,&cached_provider);
        if(SUCCEEDED(hr)) hr=IWebAccountProvider_get_Id(provider,&requested_provider);
        if(SUCCEEDED(hr) && SUCCEEDED(WindowsCompareStringOrdinal(cached_id,id,&order)) && !order &&
            SUCCEEDED(WindowsCompareStringOrdinal(cached_provider,requested_provider,&order)) && !order) {
            *out=cached_account; IWebAccount_AddRef(*out);
        }
    }
    ReleaseSRWLockShared(&account_lock);
    if(account_provider) IWebAccountProvider_Release(account_provider); if(account2) IWebAccount2_Release(account2);
    WindowsDeleteString(cached_id); WindowsDeleteString(cached_provider); WindowsDeleteString(requested_provider);
    return hr;
}
static HRESULT account_create(IWebTokenRequest *request,struct xodus_token *token,IMap_HSTRING_HSTRING *properties,IWebAccount **out)
{
    struct account *impl; HRESULT hr; *out=NULL;
    if(!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl=&account_vtbl; impl->IWebAccount2_iface.lpVtbl=&account2_vtbl; impl->ref=1;
    hr=IWebTokenRequest_get_WebAccountProvider(request,&impl->provider);
    if(SUCCEEDED(hr)) hr=WindowsDuplicateString(token->xuid,&impl->id);
    if(SUCCEEDED(hr)) hr=WindowsDuplicateString(token->user_name,&impl->name);
    if(SUCCEEDED(hr)) hr=IMap_HSTRING_HSTRING_GetView(properties,&impl->properties);
    if(FAILED(hr)) { account_release(&impl->iface); return hr; }
    AcquireSRWLockExclusive(&account_lock);
    if(cached_account) IWebAccount_Release(cached_account);
    cached_account=&impl->iface; IWebAccount_AddRef(cached_account);
    ReleaseSRWLockExclusive(&account_lock);
    *out=&impl->iface; return S_OK;
}
struct response { IWebTokenResponse iface; LONG ref; HSTRING token; IWebAccount *account; IMap_HSTRING_HSTRING *properties; };
static void response_destroy(struct response *impl)
{
    WindowsDeleteString(impl->token);
    if(impl->account) IWebAccount_Release(impl->account);
    if(impl->properties) IMap_HSTRING_HSTRING_Release(impl->properties);
}
OBJECT_METHODS(response,IWebTokenResponse,struct response,response_destroy(impl),L"Windows.Security.Authentication.Web.Core.WebTokenResponse")
static HRESULT WINAPI response_qi(IWebTokenResponse *iface,REFIID iid,void **out)
{ if(!out) return E_POINTER; *out=NULL; if(!BASE_QI(IWebTokenResponse)) return E_NOINTERFACE; *out=iface; response_addref(iface); return S_OK; }
static HRESULT WINAPI response_token(IWebTokenResponse *iface,HSTRING *out) { TRACE("read response token\n"); return WindowsDuplicateString(((struct response *)iface)->token,out); }
static HRESULT WINAPI response_error(IWebTokenResponse *iface,IWebProviderError **out) { if(!out) return E_POINTER; *out=NULL; return S_OK; }
static HRESULT WINAPI response_account(IWebTokenResponse *iface,IWebAccount **out)
{ if(!out) return E_POINTER; *out=((struct response *)iface)->account; IWebAccount_AddRef(*out); return S_OK; }
static HRESULT WINAPI response_properties(IWebTokenResponse *iface,IMap_HSTRING_HSTRING **out)
{ if(!out) return E_POINTER; *out=((struct response *)iface)->properties; IMap_HSTRING_HSTRING_AddRef(*out); return S_OK; }
static const IWebTokenResponseVtbl response_vtbl={OBJECT_VTBL(response),response_token,response_error,response_account,response_properties};
static HRESULT response_create(IWebTokenRequest *request,struct xodus_token *token,IWebTokenResponse **out)
{
    struct response *impl; HSTRING environment = NULL; HRESULT hr; *out=NULL;
    if(!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl=&response_vtbl; impl->ref=1;
    hr=WindowsDuplicateString(token->token,&impl->token);
    if(SUCCEEDED(hr)) hr=string_map_create(&impl->properties);
    /* Xodus authenticates against the production Xbox Live endpoints. Older
     * Xbox SDKs require this property even when using the RETAIL sandbox. */
    if(SUCCEEDED(hr)) hr=string_from_wide(L"prod",&environment);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"Environment",environment);
    WindowsDeleteString(environment);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"Sandbox",token->sandbox);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"Signature",token->signature);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"XboxUserId",token->xuid);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"Gamertag",token->gamertag);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"Expiry",token->expiry);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"UserHash",token->user_hash);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"AgeGroup",token->age_group);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"ModernGamertag",token->modern_gamertag);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"ModernGamertagSuffix",token->modern_gamertag_suffix);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"UniqueModernGamertag",token->unique_modern_gamertag);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"Privileges",token->privileges);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"Enforcement",token->enforcement);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"Restrictions",token->restrictions);
    if(SUCCEEDED(hr)) hr=insert_property(impl->properties,L"TitleRestrictions",token->title_restrictions);
    if(SUCCEEDED(hr)) hr=account_create(request,token,impl->properties,&impl->account);
    if(FAILED(hr)) { response_release(&impl->iface); return hr; }
    *out=&impl->iface; return S_OK;
}
struct token_result { IWebTokenRequestResult iface; LONG ref; IVectorView_WebTokenResponse *responses; };
OBJECT_METHODS(result,IWebTokenRequestResult,struct token_result,if(impl->responses) IVectorView_WebTokenResponse_Release(impl->responses),L"Windows.Security.Authentication.Web.Core.WebTokenRequestResult")
static HRESULT WINAPI result_qi(IWebTokenRequestResult *iface,REFIID iid,void **out)
{ if(!out) return E_POINTER; *out=NULL; if(!BASE_QI(IWebTokenRequestResult)) return E_NOINTERFACE; *out=iface; result_addref(iface); return S_OK; }
static HRESULT WINAPI result_data(IWebTokenRequestResult *iface,IVectorView_WebTokenResponse **out)
{ TRACE("read response collection\n"); if(!out) return E_POINTER; *out=((struct token_result *)iface)->responses; IVectorView_WebTokenResponse_AddRef(*out); return S_OK; }
static HRESULT WINAPI result_status(IWebTokenRequestResult *iface,WebTokenRequestStatus *out) { TRACE("read response status\n"); if(!out) return E_POINTER; *out=WebTokenRequestStatus_Success; return S_OK; }
static HRESULT WINAPI result_error(IWebTokenRequestResult *iface,IWebProviderError **out) { if(!out) return E_POINTER; *out=NULL; return S_OK; }
static HRESULT WINAPI result_invalidate(IWebTokenRequestResult *iface,IAsyncAction **out) { if(!out) return E_POINTER; *out=NULL; return E_NOTIMPL; }
static const IWebTokenRequestResultVtbl result_vtbl={OBJECT_VTBL(result),result_data,result_status,result_error,result_invalidate};
HRESULT web_token_result_create(IWebTokenRequest *request,struct xodus_token *token,IWebTokenRequestResult **out)
{
    static const struct vector_iids iids={&IID_IVector_WebTokenResponse,&IID_IVectorView_WebTokenResponse,&IID_IIterable_WebTokenResponse,&IID_IIterator_WebTokenResponse};
    struct token_result *impl; IVector_WebTokenResponse *vector=NULL; IWebTokenResponse *response=NULL; HRESULT hr;
    if(!out) return E_POINTER; *out=NULL;
    if(!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl=&result_vtbl; impl->ref=1;
    hr=response_create(request,token,&response);
    if(SUCCEEDED(hr)) hr=vector_create(&iids,(void **)&vector);
    if(SUCCEEDED(hr)) hr=IVector_WebTokenResponse_Append(vector,response);
    if(SUCCEEDED(hr)) hr=IVector_WebTokenResponse_GetView(vector,&impl->responses);
    if(vector) IVector_WebTokenResponse_Release(vector); if(response) IWebTokenResponse_Release(response);
    if(FAILED(hr)) { result_release(&impl->iface); return hr; }
    *out=&impl->iface; return S_OK;
}
