/* A delayed GUI startup, without game or account dependencies. */
#include <windows.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    WNDCLASSW cls = {0};
    HWND hwnd;
    MSG msg;
    DWORD deadline;

    (void)previous; (void)show;
    if (!lstrcmpA(command, "hold")) { Sleep(120000); return 0; }
    cls.lpfnWndProc = DefWindowProcW;
    cls.hInstance = instance;
    cls.lpszClassName = L"WineStartupProbe";
    if (!RegisterClassW(&cls)) return 1;
    Sleep(1200);
    hwnd = CreateWindowW(cls.lpszClassName, L"Wine startup telemetry test", WS_OVERLAPPEDWINDOW,
                         100, 100, 320, 200, NULL, NULL, instance, NULL);
    if (!hwnd) return 2;
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    deadline = GetTickCount() + 300;
    do
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    } while ((LONG)(deadline - GetTickCount()) > 0);
    /* A second show must not duplicate the first-window milestone. */
    ShowWindow(hwnd, SW_HIDE);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    DestroyWindow(hwnd);
    return 0;
}
