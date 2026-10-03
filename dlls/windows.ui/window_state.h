/* Desktop placement for a CoreWindow. SPDX-License-Identifier: LGPL-2.1-or-later */
struct corewindow_state
{
    WCHAR key[512];
    RECT normal;
    WINDOWPLACEMENT placement;
    LONG_PTR style;
    BOOL ready, changing, fullscreen, maximized, activated, restore_fullscreen;
};
void corewindow_state_init(struct corewindow_state *state, RECT *rect);
void corewindow_state_update(struct corewindow_state *state, HWND hwnd);
void corewindow_state_save(struct corewindow_state *state, HWND hwnd);
HRESULT corewindow_state_fullscreen(struct corewindow_state *state, HWND hwnd, BOOL fullscreen);
#define COREWINDOW_SAVE_TIMER 0x57475354
