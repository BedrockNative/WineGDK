/* Persistent desktop placement for packaged CoreWindow applications.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "private.h"
#include <stdio.h>
#include "winuser.h"
#include "winreg.h"
#include "appmodel.h"
#include "window_state.h"

struct saved_state { DWORD version; RECT normal; DWORD maximized, fullscreen; };

static void fit_monitor(RECT *rect, const RECT *work)
{
    LONG width = min(rect->right - rect->left, work->right - work->left);
    LONG height = min(rect->bottom - rect->top, work->bottom - work->top);
    rect->left = max(work->left, min(rect->left, work->right - width));
    rect->top = max(work->top, min(rect->top, work->bottom - height));
    rect->right = rect->left + width; rect->bottom = rect->top + height;
}

void corewindow_state_init(struct corewindow_state *state, RECT *rect)
{
    MONITORINFO monitor = {sizeof(monitor)};
    WCHAR identity[256], executable[32768];
    struct saved_state saved;
    UINT32 length = ARRAY_SIZE(identity);
    DWORD size = sizeof(saved), type;
    ULONGLONG hash = 14695981039346656037ULL;
    HKEY key;
    unsigned int i;
    POINT origin = {0, 0};
    LONG width, height;

    if (!GetCurrentApplicationUserModelId(&length, identity) && !wcschr(identity, '\\') && !wcschr(identity, '/'))
        swprintf(state->key, ARRAY_SIZE(state->key), L"Software\\Wine\\CoreWindow\\%s", identity);
    else
    {
        length = GetModuleFileNameW(NULL, executable, ARRAY_SIZE(executable));
        if (length && length < ARRAY_SIZE(executable))
        {
            for (i = 0; i < length; ++i) { hash ^= towlower(executable[i]); hash *= 1099511628211ULL; }
            swprintf(state->key, ARRAY_SIZE(state->key), L"Software\\Wine\\CoreWindow\\Executable-%016I64x", hash);
        }
    }
    GetMonitorInfoW(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &monitor);
    width = (monitor.rcMonitor.right - monitor.rcMonitor.left) / 2;
    height = (monitor.rcMonitor.bottom - monitor.rcMonitor.top) / 2;
    SetRect(rect, 0, 0, max(width, 1), max(height, 1));
    AdjustWindowRectEx(rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
    width = rect->right - rect->left; height = rect->bottom - rect->top;
    rect->left = monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2;
    rect->top = monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2;
    rect->right = rect->left + width; rect->bottom = rect->top + height;
    if (state->key[0] && !RegOpenKeyExW(HKEY_CURRENT_USER, state->key, 0, KEY_QUERY_VALUE, &key))
    {
        if (!RegQueryValueExW(key, L"Placement", NULL, &type, (BYTE *)&saved, &size) &&
            type == REG_BINARY && size == sizeof(saved) && saved.version == 1 &&
            saved.normal.left >= -32768 && saved.normal.top >= -32768 &&
            saved.normal.right <= 32767 && saved.normal.bottom <= 32767 &&
            saved.normal.right > saved.normal.left && saved.normal.bottom > saved.normal.top &&
            saved.maximized <= 1 && saved.fullscreen <= 1)
        {
            *rect = saved.normal;
            state->maximized = saved.maximized;
            state->restore_fullscreen = saved.fullscreen;
            GetMonitorInfoW(MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST), &monitor);
        }
        RegCloseKey(key);
    }
    fit_monitor(rect, &monitor.rcWork);
    state->normal = *rect;
}

void corewindow_state_update(struct corewindow_state *state, HWND hwnd)
{
    if (!state->ready || state->changing || state->fullscreen || IsIconic(hwnd)) return;
    state->maximized = IsZoomed(hwnd);
    if (!state->maximized) GetWindowRect(hwnd, &state->normal);
    SetTimer(hwnd, COREWINDOW_SAVE_TIMER, 250, NULL);
}

void corewindow_state_save(struct corewindow_state *state, HWND hwnd)
{
    struct saved_state saved = {1, state->normal, state->maximized, state->fullscreen};
    HKEY key;
    KillTimer(hwnd, COREWINDOW_SAVE_TIMER);
    if (!state->ready || state->changing || !state->key[0]) return;
    if (!RegCreateKeyExW(HKEY_CURRENT_USER, state->key, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL))
    {
        RegSetValueExW(key, L"Placement", 0, REG_BINARY, (BYTE *)&saved, sizeof(saved));
        RegCloseKey(key);
    }
}

HRESULT corewindow_state_fullscreen(struct corewindow_state *state, HWND hwnd, BOOL fullscreen)
{
    MONITORINFO monitor = {sizeof(monitor)};
    DWORD error;
    BOOL ret;
    if (!!fullscreen == state->fullscreen) return S_OK;
    if (state->changing) return E_ILLEGAL_METHOD_CALL;
    if (fullscreen)
    {
        state->placement.length = sizeof(state->placement);
        if (!GetWindowPlacement(hwnd, &state->placement) ||
            !GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
            return HRESULT_FROM_WIN32(GetLastError());
        if (state->restore_fullscreen && !state->activated)
            state->placement.showCmd = state->maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
        else
            corewindow_state_update(state, hwnd);
        state->style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        state->changing = TRUE; state->fullscreen = TRUE;
        SetWindowLongPtrW(hwnd, GWL_STYLE, state->style & ~WS_OVERLAPPEDWINDOW);
        ret = SetWindowPos(hwnd, NULL, monitor.rcMonitor.left, monitor.rcMonitor.top,
            monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        error = ret ? ERROR_SUCCESS : GetLastError();
        if (!ret)
        {
            SetWindowLongPtrW(hwnd, GWL_STYLE, state->style);
            SetWindowPlacement(hwnd, &state->placement);
            state->fullscreen = FALSE;
        }
    }
    else
    {
        state->changing = TRUE; state->fullscreen = FALSE;
        SetWindowLongPtrW(hwnd, GWL_STYLE, state->style);
        ret = SetWindowPlacement(hwnd, &state->placement);
        error = ret ? ERROR_SUCCESS : GetLastError();
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    state->changing = FALSE;
    corewindow_state_save(state, hwnd);
    return HRESULT_FROM_WIN32(error);
}
