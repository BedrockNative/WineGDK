/* WNF focus query and cross-process notification regression test.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#define FOCUS_NAME ((ULONGLONG)0x0d83063ea3bc7875)
#define STATUS_BUFFER_TOO_SMALL ((LONG)0xc0000023)
typedef LONG (WINAPI *callback_t)(ULONGLONG,ULONG,void *,void *,const void *,ULONG);
static LONG (WINAPI *query)(const ULONGLONG *,const void *,const void *,ULONG *,void *,ULONG *);
static LONG (WINAPI *subscribe)(void **,ULONGLONG,ULONG,callback_t,void *,const void *,ULONG,ULONG);
static LONG (WINAPI *unsubscribe)(void *);
static unsigned int checks, failures;
#define check(c) do { ++checks; if (!(c)) { ++failures; printf("FAIL %u: %s\n",__LINE__,#c); } } while (0)

struct callback_data
{
    void *subscription;
    HANDLE event;
    volatile LONG calls, pid, bad_data;
    BOOL self_unsubscribe;
    LONG unsubscribe_status;
};

static LONG WINAPI callback(ULONGLONG name,ULONG stamp,void *type,void *context,const void *buffer,ULONG size)
{
    struct callback_data *data = context;
    if (name != FOCUS_NAME || !stamp || type || size != sizeof(ULONG) || !buffer)
        InterlockedIncrement(&data->bad_data);
    else InterlockedExchange(&data->pid,*(const ULONG *)buffer);
    InterlockedIncrement(&data->calls);
    if (data->self_unsubscribe) data->unsubscribe_status = unsubscribe(data->subscription);
    SetEvent(data->event);
    return 0;
}

static LRESULT CALLBACK window_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    if (msg == WM_APP)
    {
        ShowWindow(hwnd,SW_SHOW);
        SetForegroundWindow(hwnd);
        return 0;
    }
    if (msg == WM_APP + 1) { SetForegroundWindow((HWND)lp); return 0; }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcA(hwnd,msg,wp,lp);
}

static void pump(DWORD ms)
{
    DWORD start = GetTickCount();
    MSG msg;
    do
    {
        while (PeekMessageA(&msg,NULL,0,0,PM_REMOVE)) DispatchMessageA(&msg);
        Sleep(5);
    } while (GetTickCount() - start < ms);
}

static BOOL wait_focus(DWORD pid)
{
    ULONGLONG name = FOCUS_NAME;
    ULONG value, stamp, size;
    unsigned int i;
    for (i = 0; i < 400; ++i)
    {
        size = sizeof(value);
        if (!query(&name,NULL,NULL,&stamp,&value,&size) && value == pid) return TRUE;
        pump(5);
    }
    return FALSE;
}

int main(int argc,char **argv)
{
    WNDCLASSA cls = {0};
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    struct callback_data a = {0}, b = {0};
    ULONGLONG name = FOCUS_NAME, unknown = 0;
    ULONG stamp, value, size, saved_stamp;
    char path[MAX_PATH], command[MAX_PATH + 30];
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    HWND hwnd, child = NULL;
    unsigned int i;
    LONG calls;
    MSG msg;
    (void)argv;

    cls.lpfnWndProc = window_proc;
    cls.hInstance = GetModuleHandleA(NULL);
    cls.lpszClassName = argc > 1 ? "WineWnfProbeChild" : "WineWnfProbeParent";
    RegisterClassA(&cls);
    hwnd = CreateWindowA(cls.lpszClassName,"Wine WNF test",WS_OVERLAPPEDWINDOW,40,40,260,100,
                         NULL,NULL,cls.hInstance,NULL);
    if (!hwnd) return 2;
    if (argc > 1)
    {
        while (GetMessageA(&msg,NULL,0,0) > 0) DispatchMessageA(&msg);
        return 0;
    }
    query = (void *)GetProcAddress(ntdll,"NtQueryWnfStateData");
    subscribe = (void *)GetProcAddress(ntdll,"RtlSubscribeWnfStateChangeNotification");
    unsubscribe = (void *)GetProcAddress(ntdll,"RtlUnsubscribeWnfStateChangeNotification");
    if (!query || !subscribe || !unsubscribe) return 2;

    ShowWindow(hwnd,SW_SHOW);
    SetForegroundWindow(hwnd);
    check(wait_focus(GetCurrentProcessId()));
    size = 0;
    check(query(&name,NULL,NULL,&stamp,NULL,&size) == STATUS_BUFFER_TOO_SMALL && size == 4);
    value = 0xdeadbeef;
    size = 3;
    check(query(&name,NULL,NULL,&stamp,&value,&size) == STATUS_BUFFER_TOO_SMALL &&
          size == 4 && value == 0xdeadbeef);
    size = sizeof(value);
    check(query(&name,NULL,NULL,&stamp,&value,&size) == 0 && value == GetCurrentProcessId());
    saved_stamp = stamp;
    check(query(&unknown,NULL,NULL,&stamp,&value,&size) < 0);
    check(query(NULL,NULL,NULL,&stamp,&value,&size) < 0);
    a.event = CreateEventA(NULL,FALSE,FALSE,NULL);
    b.event = CreateEventA(NULL,FALSE,FALSE,NULL);
    check(subscribe(&a.subscription,name,saved_stamp,callback,&a,NULL,0,0) == 0);
    check(subscribe(&b.subscription,name,saved_stamp,callback,&b,NULL,0,0) == 0);
    pump(100);
    check(!a.calls && !b.calls);

    GetModuleFileNameA(NULL,path,sizeof(path));
    snprintf(command,sizeof(command),"\"%s\" --child",path);
    check(CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi));
    if (!pi.hProcess) return 2;
    for (i = 0; i < 400 && !child; ++i) { child = FindWindowA("WineWnfProbeChild",NULL); pump(5); }
    check(child != NULL);
    AllowSetForegroundWindow(ASFW_ANY);
    PostMessageA(child,WM_APP,0,0);
    check(wait_focus(pi.dwProcessId));
    check(WaitForSingleObject(a.event,2000) == WAIT_OBJECT_0);
    check(WaitForSingleObject(b.event,2000) == WAIT_OBJECT_0);
    check(a.pid == (LONG)pi.dwProcessId && b.pid == (LONG)pi.dwProcessId && !a.bad_data && !b.bad_data);

    check(unsubscribe(a.subscription) == 0);
    check(unsubscribe(a.subscription) == (LONG)STATUS_INVALID_HANDLE);
    calls = a.calls;
    b.self_unsubscribe = TRUE;
    PostMessageA(child,WM_APP + 1,0,(LPARAM)hwnd);
    check(wait_focus(GetCurrentProcessId()));
    check(WaitForSingleObject(b.event,2000) == WAIT_OBJECT_0 && b.unsubscribe_status == 0);
    pump(100);
    check(a.calls == calls);
    check(unsubscribe(b.subscription) == (LONG)STATUS_INVALID_HANDLE);

    /* A stale stamp must deliver the current state even without another change. */
    a.calls = 0;
    check(subscribe(&a.subscription,name,saved_stamp,callback,&a,NULL,0,0) == 0);
    check(WaitForSingleObject(a.event,2000) == WAIT_OBJECT_0 && a.pid == (LONG)GetCurrentProcessId());
    check(unsubscribe(a.subscription) == 0);
    PostMessageA(child,WM_CLOSE,0,0);
    check(WaitForSingleObject(pi.hProcess,3000) == WAIT_OBJECT_0);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(a.event);
    CloseHandle(b.event);
    DestroyWindow(hwnd);
    printf("%u checks, %u failures\n",checks,failures);
    return !!failures;
}
