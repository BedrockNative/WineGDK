/* GDK XError callback regression tests. SPDX-License-Identifier: LGPL-2.1-or-later */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <xerror.h>
#include <xgameruntimefeature.h>

static HRESULT (WINAPI *report)(HRESULT,const char *);
static IXErrorImpl *errors;
static unsigned failures,checks,calls;
#define check(c) do { ++checks; if (!(c)) { ++failures; printf("FAIL %d: %s\n",__LINE__,#c); } } while(0)
static BOOLEAN WINAPI callback(HRESULT hr,const char *message,void *context)
{
    ++calls;
    check(hr==E_FAIL);
    check(!strcmp(message,"fixture"));
    check(context==&calls);
    /* Reentrant setters must not deadlock; recursive reports must not recurse. */
    IXErrorImpl_XErrorSetOptions(errors,XErrorOptions_None,XErrorOptions_None);
    check(report(E_FAIL,"recursive")==E_FAIL);
    return FALSE;
}
int main(void)
{
    HMODULE mod=LoadLibraryA("xgameruntime.dll");
    HRESULT (WINAPI *query)(const GUID*,REFIID,void**);
    IXGameRuntimeFeatureImpl *features = NULL;
    void *out=(void*)1;
    HRESULT hr;
    if (!mod) return 2;
    query=(void*)GetProcAddress(mod,"QueryApiImpl");
    report=(void*)GetProcAddress(mod,"XErrorReport");
    if (!query || !report) return 2;
    hr=query(&CLSID_XGameRuntimeFeatureImpl,&IID_IXGameRuntimeFeatureImpl,(void**)&features);
    check(hr==S_OK && features);
    if (features)
    {
        check(IXGameRuntimeFeatureImpl_XGameRuntimeIsFeatureAvailable(features,XGameRuntimeFeature_XError));
        check(IXGameRuntimeFeatureImpl_XGameRuntimeIsFeatureAvailable(features,XGameRuntimeFeature_XUser));
        check(IXGameRuntimeFeatureImpl_XGameRuntimeIsFeatureAvailable(features,XGameRuntimeFeature_XPackage));
        check(IXGameRuntimeFeatureImpl_XGameRuntimeIsFeatureAvailable(features,XGameRuntimeFeature_XStore));
        check(!IXGameRuntimeFeatureImpl_XGameRuntimeIsFeatureAvailable(features,(XGameRuntimeFeature)~0u));
        IXGameRuntimeFeatureImpl_Release(features);
    }
    hr=query(&CLSID_XErrorImpl,&IID_IXErrorImpl,(void**)&errors);
    check(hr==S_OK && errors);
    if (FAILED(hr)) return 1;
    check(IXErrorImpl_QueryInterface(errors,&IID_IUnknown,NULL)==E_POINTER);
    check(IXErrorImpl_QueryInterface(errors,&IID_IXGameRuntimeFeatureImpl,&out)==E_NOINTERFACE && !out);
    IXErrorImpl_XErrorSetOptions(errors,XErrorOptions_None,XErrorOptions_None);
    IXErrorImpl_XErrorSetCallback(errors,callback,&calls);
    check(report(S_OK,"success")==S_OK && !calls);
    check(report(E_FAIL,NULL)==E_INVALIDARG && !calls);
    check(report(E_FAIL,"fixture")==E_FAIL && calls==1);
    /* FALSE from callback must suppress even explicit fail-fast policy. */
    IXErrorImpl_XErrorSetOptions(errors,XErrorOptions_FailFastOnError,XErrorOptions_FailFastOnError);
    check(report(E_FAIL,"fixture")==E_FAIL && calls==2);
    IXErrorImpl_XErrorSetCallback(errors,NULL,NULL);
    check(report(E_FAIL,"unregistered")==E_FAIL && calls==2);
    IXErrorImpl_Release(errors);
    printf("%u checks, %u failures\n",checks,failures);
    return !!failures;
}
