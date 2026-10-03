/* Multi-process CoreWindow placement and fullscreen regression probe.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_UI_Core
#define WIDL_using_Windows_UI_ViewManagement
#include <stdio.h>
#include "initguid.h"
#include "roapi.h"
#include "winstring.h"
#include "windows.ui.core.h"
#include "windows.ui.viewmanagement.h"
#include "corewindow.h"
#include "winuser.h"
static unsigned int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %u: %s\n", __LINE__, #x); ++failures; } } while (0)
static void pump(void)
{
    MSG msg;
    unsigned int i;
    for (i=0;i<10;++i) { while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);} Sleep(10); }
}
int main(int argc, char **argv)
{
    HRESULT (WINAPI *create)(ICoreWindow **);
    HMODULE module;
    ICoreWindow *window=NULL;
    ICoreWindowInterop *interop=NULL;
    IApplicationViewStatics2 *statics=NULL;
    IApplicationView *view=NULL;
    IApplicationView3 *view3=NULL;
    HSTRING name;
    HWND hwnd;
    MONITORINFO monitor={sizeof(monitor)};
    POINT point={0,0};
    RECT rect,client,expected;
    boolean fullscreen=FALSE,success=FALSE;
    const char *mode=argc>1?argv[1]:"default";
    HRESULT hr;
    CHECK(SUCCEEDED(RoInitialize(RO_INIT_SINGLETHREADED)));
    module=LoadLibraryW(L"windows.ui.dll");
    create=(void *)GetProcAddress(module,"__wine_create_core_window");
    CHECK(!!create); if(!create) return 1;
    hr=create(&window); CHECK(hr==S_OK); if(FAILED(hr)) return 1;
    CHECK(ICoreWindow_QueryInterface(window,&IID_ICoreWindowInterop,(void **)&interop)==S_OK);
    CHECK(ICoreWindowInterop_get_WindowHandle(interop,&hwnd)==S_OK);
    ICoreWindowInterop_Release(interop);
    CHECK(ICoreWindow_Activate(window)==S_OK);pump();
    GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTOPRIMARY),&monitor);
    expected.left=monitor.rcWork.left+43;expected.top=monitor.rcWork.top+71;
    expected.right=expected.left+(monitor.rcWork.right-monitor.rcWork.left)/3;
    expected.bottom=expected.top+(monitor.rcWork.bottom-monitor.rcWork.top)/3;
    GetWindowRect(hwnd,&rect);GetClientRect(hwnd,&client);
    printf("%s: rect %ld,%ld,%ld,%ld client %ldx%ld\n",mode,rect.left,rect.top,rect.right,rect.bottom,client.right,client.bottom);
    if(!strcmp(mode,"default"))
    {
        CHECK(client.right==(monitor.rcMonitor.right-monitor.rcMonitor.left)/2);
        CHECK(client.bottom==(monitor.rcMonitor.bottom-monitor.rcMonitor.top)/2);
        CHECK(abs((rect.left+rect.right)-(monitor.rcWork.left+monitor.rcWork.right))<=2);
        CHECK(abs((rect.top+rect.bottom)-(monitor.rcWork.top+monitor.rcWork.bottom))<=2);
    }
    if(!strcmp(mode,"save") || !strcmp(mode,"maximize_save"))
    {
        CHECK(SetWindowPos(hwnd,NULL,expected.left,expected.top,expected.right-expected.left,expected.bottom-expected.top,SWP_NOZORDER));pump();
        if(!strcmp(mode,"maximize_save")){ShowWindow(hwnd,SW_MAXIMIZE);pump();}
    }
    if(!strcmp(mode,"restore") || !strcmp(mode,"fullscreen_save")) CHECK(EqualRect(&rect,&expected));
    if(!strcmp(mode,"maximize_restore") || !strcmp(mode,"maximize_fullscreen_save")) CHECK(IsZoomed(hwnd));
    WindowsCreateString(L"Windows.UI.ViewManagement.ApplicationView",wcslen(L"Windows.UI.ViewManagement.ApplicationView"),&name);
    hr=RoGetActivationFactory(name,&IID_IApplicationViewStatics2,(void **)&statics);WindowsDeleteString(name);
    CHECK(hr==S_OK);
    if(SUCCEEDED(hr))
    {
        CHECK(IApplicationViewStatics2_GetForCurrentView(statics,&view)==S_OK);
        if(view) CHECK(IApplicationView_QueryInterface(view,&IID_IApplicationView3,(void **)&view3)==S_OK);
        if(view3)
        {
            CHECK(IApplicationView3_get_IsFullScreenMode(view3,&fullscreen)==S_OK);
            CHECK(fullscreen==(!strcmp(mode,"fullscreen_restore") || !strcmp(mode,"maximize_fullscreen_restore")));
            if(!strcmp(mode,"fullscreen_restore") || !strcmp(mode,"maximize_fullscreen_restore"))
            {
                CHECK(EqualRect(&rect,&monitor.rcMonitor));
                CHECK(IApplicationView3_ExitFullScreenMode(view3)==S_OK);pump();
                GetWindowRect(hwnd,&rect);
                if(!strcmp(mode,"maximize_fullscreen_restore")) CHECK(IsZoomed(hwnd));
                else CHECK(EqualRect(&rect,&expected));
            }
            if(!strcmp(mode,"restore") || !strcmp(mode,"fullscreen_save") || !strcmp(mode,"maximize_restore") || !strcmp(mode,"maximize_fullscreen_save"))
            {
                CHECK(IApplicationView3_TryEnterFullScreenMode(view3,&success)==S_OK && success);pump();
                GetWindowRect(hwnd,&rect);CHECK(EqualRect(&rect,&monitor.rcMonitor));
                CHECK(IApplicationView3_get_IsFullScreenMode(view3,&fullscreen)==S_OK && fullscreen);
                if(strcmp(mode,"fullscreen_save") && strcmp(mode,"maximize_fullscreen_save"))
                {
                    SendMessageW(hwnd,WM_KEYDOWN,VK_F11,1);SendMessageW(hwnd,WM_KEYUP,VK_F11,0xc0000001);pump();
                    CHECK(IApplicationView3_get_IsFullScreenMode(view3,&fullscreen)==S_OK && !fullscreen);
                    if(!strcmp(mode,"maximize_restore")) CHECK(IsZoomed(hwnd));
                    else {GetWindowRect(hwnd,&rect);CHECK(EqualRect(&rect,&expected));}
                    SendMessageW(hwnd,WM_SYSKEYDOWN,VK_RETURN,0x20000001);pump();
                    CHECK(IApplicationView3_get_IsFullScreenMode(view3,&fullscreen)==S_OK && fullscreen);
                    SendMessageW(hwnd,WM_SYSKEYDOWN,VK_RETURN,0x60000001);
                    CHECK(IApplicationView3_get_IsFullScreenMode(view3,&fullscreen)==S_OK && fullscreen);
                    CHECK(IApplicationView3_ExitFullScreenMode(view3)==S_OK);pump();
                }
            }
            IApplicationView3_Release(view3);
        }
        if(view) IApplicationView_Release(view);
        IApplicationViewStatics2_Release(statics);
    }
    ICoreWindow_Close(window);ICoreWindow_Release(window);FreeLibrary(module);RoUninitialize();
    printf("window-state %s: %u failures\n",mode,failures);return !!failures;
}
