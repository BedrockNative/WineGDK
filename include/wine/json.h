/* Small UTF-8 JSON string encoder. Caller owns the returned buffer.
 * Copyright 2026 WineGDK contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __WINE_JSON_H
#define __WINE_JSON_H
#include <stdlib.h>
#include <string.h>

static inline char *wine_json_escape(const char *text)
{
    static const char hex[] = "0123456789abcdef";
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    size_t len = strlen((const char *)p);
    char *result, *out;

    if (len > ((size_t)-1 - 1) / 6 || !(result = malloc(len * 6 + 1))) return NULL;
    for (out = result; *p; p++)
    {
        if (*p == '"' || *p == '\\') { *out++ = '\\'; *out++ = *p; }
        else if (*p < 0x20)
        {
            *out++ = '\\'; *out++ = 'u'; *out++ = '0'; *out++ = '0';
            *out++ = hex[*p >> 4]; *out++ = hex[*p & 15];
        }
        else *out++ = *p;
    }
    *out = 0;
    return result;
}
#endif
