/* Missing Xbox UI reports an HRESULT instead of terminating the process. */
#include <stdio.h>
#include "gamingtcui.h"
static LONG calls;
static void WINAPI completed(HRESULT result, void *context) { InterlockedIncrement(&calls); }
int main(void)
{
    HRESULT (WINAPI *show)(UINT32, GameUICompletionRoutine, void *);
    HMODULE module=LoadLibraryW(L"gamingtcui.dll");
    HRESULT hr;
    if(!module || !(show=(void *)GetProcAddress(module,"ShowTitleAchievementsUI"))) return 1;
    hr=show(896928775,NULL,NULL);
    if(hr!=E_INVALIDARG) return 2;
    hr=show(896928775,completed,&calls);
    if(hr!=E_NOTIMPL || calls) return 3;
    FreeLibrary(module); puts("gamingtcui: PASS (unsupported UI returns an error; no callback or exception)"); return 0;
}
