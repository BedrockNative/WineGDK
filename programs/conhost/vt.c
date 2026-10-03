/* Virtual-terminal output parser.
 * Copyright 2026 WineGDK contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "conhost.h"

static unsigned int ansi_color(unsigned int color)
{
    return ((color & 1) ? FOREGROUND_RED : 0) | ((color & 2) ? FOREGROUND_GREEN : 0) |
           ((color & 4) ? FOREGROUND_BLUE : 0) | ((color & 8) ? FOREGROUND_INTENSITY : 0);
}

/* The Win32 screen buffer stores palette attributes, including for extended SGR colors. */
static unsigned int palette_color(struct screen_buffer *screen, unsigned int r, unsigned int g, unsigned int b)
{
    static const unsigned int defaults[16] =
    {
        0x000000,0x800000,0x008000,0x808000,0x000080,0x800080,0x008080,0xc0c0c0,
        0x808080,0xff0000,0x00ff00,0xffff00,0x0000ff,0xff00ff,0x00ffff,0xffffff,
    };
    unsigned int i, best = 0, distance = ~0u;
    BOOL custom = FALSE;
    for (i = 0; i < 16; ++i) if (screen->color_map[i]) custom = TRUE;
    for (i = 0; i < 16; ++i)
    {
        unsigned int color = custom ? screen->color_map[i] : defaults[i], d;
        int dr = (int)r - GetRValue(color), dg = (int)g - GetGValue(color), db = (int)b - GetBValue(color);
        d = dr * dr + dg * dg + db * db;
        if (d < distance) { best = i; distance = d; }
    }
    return best;
}

static void set_graphics(struct screen_buffer *screen)
{
    unsigned int *params = screen->vt_params, i, color, index, r, g, b;
    for (i = 0; i <= screen->vt_count; ++i)
    {
        switch (params[i])
        {
        case 0: screen->attr = 7; break;
        case 1: screen->attr |= FOREGROUND_INTENSITY; break;
        case 2: case 22: screen->attr &= ~FOREGROUND_INTENSITY; break;
        case 39: screen->attr = (screen->attr & ~0x0f) | 7; break;
        case 49: screen->attr &= ~0xf0; break;
        case 38: case 48:
            index = params[i];
            if (i + 2 <= screen->vt_count && params[i + 1] == 5)
            {
                color = params[i + 2]; i += 2;
                if (color > 255) break;
                if (color < 16) color = ansi_color(color);
                else
                {
                    if (color >= 232) r = g = b = 8 + (color - 232) * 10;
                    else
                    {
                        color -= 16;
                        r = color / 36; g = (color / 6) % 6; b = color % 6;
                        r = r ? 55 + r * 40 : 0; g = g ? 55 + g * 40 : 0; b = b ? 55 + b * 40 : 0;
                    }
                    color = palette_color(screen, r, g, b);
                }
            }
            else if (i + 4 <= screen->vt_count && params[i + 1] == 2)
            {
                r = params[i + 2]; g = params[i + 3]; b = params[i + 4]; i += 4;
                if (r > 255 || g > 255 || b > 255) break;
                color = palette_color(screen, r, g, b);
            }
            else break;
            if (index == 38) screen->attr = (screen->attr & ~0x0f) | color;
            else screen->attr = (screen->attr & ~0xf0) | (color << 4);
            break;
        default:
            color = params[i];
            if (color >= 30 && color <= 37) screen->attr = (screen->attr & ~7) | ansi_color(color - 30);
            else if (color >= 40 && color <= 47) screen->attr = (screen->attr & ~0x70) | (ansi_color(color - 40) << 4);
            else if (color >= 90 && color <= 97) screen->attr = (screen->attr & ~0x0f) | ansi_color(color - 90 + 8);
            else if (color >= 100 && color <= 107) screen->attr = (screen->attr & ~0xf0) | (ansi_color(color - 100 + 8) << 4);
            break;
        }
    }
}

static void csi_dispatch(struct screen_buffer *screen, WCHAR ch, RECT *update)
{
    unsigned int value = screen->vt_params[0], n = value ? value : 1, start, end, i;
    if (screen->vt_invalid) return;
    if (screen->vt_private)
    {
        if (value == 25 && (ch == 'h' || ch == 'l')) screen->cursor_visible = ch == 'h';
        return;
    }
    switch (ch)
    {
    case 'm': set_graphics(screen); break;
    case 'A': screen->cursor_y -= min(n, screen->cursor_y); break;
    case 'B': screen->cursor_y = min(screen->height - 1, screen->cursor_y + n); break;
    case 'C': screen->cursor_x = min(screen->width - 1, screen->cursor_x + n); break;
    case 'D': screen->cursor_x -= min(n, screen->cursor_x); break;
    case 'G': screen->cursor_x = min(screen->width, n) - 1; break;
    case 'd': screen->cursor_y = min(screen->height, n) - 1; break;
    case 'H': case 'f':
        screen->cursor_y = min(screen->height, n) - 1;
        n = screen->vt_count && screen->vt_params[1] ? screen->vt_params[1] : 1;
        screen->cursor_x = min(screen->width, n) - 1;
        break;
    case 's': screen->vt_saved_x = screen->cursor_x; screen->vt_saved_y = screen->cursor_y; break;
    case 'u':
        screen->cursor_x = min(screen->width - 1, screen->vt_saved_x);
        screen->cursor_y = min(screen->height - 1, screen->vt_saved_y);
        break;
    case 'J': case 'K':
        if (value > 2) break;
        start = ch == 'J' ? 0 : screen->cursor_y * screen->width;
        end = ch == 'J' ? screen->height * screen->width : start + screen->width;
        if (!value) start = screen->cursor_y * screen->width + get_bounded_cursor_x(screen);
        if (value == 1) end = screen->cursor_y * screen->width + get_bounded_cursor_x(screen) + 1;
        for (i = start; i < end; ++i) { screen->data[i].ch = ' '; screen->data[i].attr = screen->attr; }
        if (start < end)
        {
            update->left = 0; update->right = screen->width - 1;
            update->top = min(update->top, start / screen->width);
            update->bottom = max(update->bottom, (end - 1) / screen->width);
        }
        break;
    }
}

/* State lives with the screen buffer: escape sequences may span multiple writes. */
BOOL process_vt_char(struct screen_buffer *screen, WCHAR ch, RECT *update)
{
    /* C0 controls execute without terminating an incomplete escape sequence. */
    if (screen->vt_state < 3 && ch < 0x20 && ch != 0x1b)
    {
        if (ch == 0x18 || ch == 0x1a) { screen->vt_state = 0; return TRUE; }
        return FALSE;
    }
    if (ch == 0x1b && screen->vt_state < 3) { screen->vt_state = 1; return TRUE; }
    switch (screen->vt_state)
    {
    case 0: return FALSE;
    case 1:
        screen->vt_state = 0;
        if (ch == '[')
        {
            memset(screen->vt_params, 0, sizeof(screen->vt_params));
            screen->vt_state = 2; screen->vt_count = 0; screen->vt_private = screen->vt_invalid = FALSE;
        }
        else if (ch == ']') screen->vt_state = 3;
        else if (ch == '7') { screen->vt_saved_x = screen->cursor_x; screen->vt_saved_y = screen->cursor_y; }
        else if (ch == '8')
        {
            screen->cursor_x = min(screen->width - 1, screen->vt_saved_x);
            screen->cursor_y = min(screen->height - 1, screen->vt_saved_y);
        }
        return TRUE;
    case 2:
        if (ch >= '0' && ch <= '9')
        {
            unsigned int *value = &screen->vt_params[screen->vt_count];
            if (*value > 6553) screen->vt_invalid = TRUE;
            else *value = *value * 10 + ch - '0';
        }
        else if (ch == ';')
        {
            if (screen->vt_count + 1 < ARRAY_SIZE(screen->vt_params)) ++screen->vt_count;
            else screen->vt_invalid = TRUE;
        }
        else if (ch == '?' && !screen->vt_count && !screen->vt_params[0]) screen->vt_private = TRUE;
        else if (ch >= 0x40 && ch <= 0x7e)
        { csi_dispatch(screen, ch, update); screen->vt_state = 0; }
        else if (ch == 0x18 || ch == 0x1a) screen->vt_state = 0;
        else screen->vt_invalid = TRUE;
        return TRUE;
    case 3: /* OSC strings are consumed through their BEL or ST terminator. */
        if (ch == '\a') screen->vt_state = 0;
        else if (ch == 0x1b) screen->vt_state = 4;
        return TRUE;
    case 4:
        screen->vt_state = ch == '\\' || ch == '\a' ? 0 : ch == 0x1b ? 4 : 3;
        return TRUE;
    }
    return FALSE;
}
