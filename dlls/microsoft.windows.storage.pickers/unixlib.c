/*
 * Microsoft.Windows.Storage.Pickers - Unix side (file chooser broker client)
 *
 * Copyright 2026 OrionBE contributors
 *
 * The game runs inside the Steam Linux Runtime (pressure-vessel), which does not see the
 * host's dialog programs (kdialog, zenity, ...) nor necessarily the user's desktop portal.
 * So the dialog is not shown from here: the OrionBE launcher, running on the host, listens
 * on a unix domain socket (inside a directory bind-mounted into the container) and opens
 * the user's native Linux file dialog on the real filesystem. We connect, send one JSON
 * line and read one JSON line back:
 *
 *   -> {"v":1,"op":"open|openMany|save|folder","title":"..","acceptLabel":"..",
 *       "filters":[{"name":"..","patterns":["*.png",..]}],"startLocation":"PicturesLibrary",
 *       "startDir":"/unix/dir","suggestedName":"..","parentWindow":"x11:0x..."}
 *   <- {"ok":true,"paths":["/home/user/skin.png"]}   (ok:false / empty paths = cancelled)
 *
 * Socket: $ORIONBE_PICKER_SOCKET, else $HOME/OrionBE/cache/xodus-run/picker.sock.
 * When the broker is not reachable the pick completes as cancelled (with a warning), so the
 * game never crashes.
 *
 * Environment overrides (for testing / troubleshooting):
 *   ORIONBE_PICKER_TEST_RESULT  skip the broker: "cancel" (or empty) = user cancelled,
 *                               otherwise '|'-separated Unix paths returned as picked.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"

#include "wine/debug.h"

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(pickers);

#define MAX_RESPONSE (4 * 1024 * 1024)

/* growable string buffer */
struct strbuf
{
    char *data;
    size_t len;
    size_t size;
    BOOL failed;
};

static void strbuf_append_len( struct strbuf *buf, const char *str, size_t len )
{
    if (buf->failed) return;
    if (buf->len + len + 1 > buf->size)
    {
        size_t size = buf->size ? buf->size * 2 : 256;
        char *data;
        while (size < buf->len + len + 1) size *= 2;
        if (!(data = realloc( buf->data, size )))
        {
            buf->failed = TRUE;
            return;
        }
        buf->data = data;
        buf->size = size;
    }
    memcpy( buf->data + buf->len, str, len );
    buf->len += len;
    buf->data[buf->len] = 0;
}

static void strbuf_append( struct strbuf *buf, const char *str )
{
    strbuf_append_len( buf, str, strlen( str ) );
}

/* appends a JSON string literal (UTF-8 passes through, control chars are escaped) */
static void append_json_string( struct strbuf *buf, const char *str )
{
    const unsigned char *p;
    char tmp[8];

    strbuf_append( buf, "\"" );
    for (p = (const unsigned char *)str; *p; p++)
    {
        switch (*p)
        {
        case '"':  strbuf_append( buf, "\\\"" ); break;
        case '\\': strbuf_append( buf, "\\\\" ); break;
        case '\n': strbuf_append( buf, "\\n" ); break;
        case '\r': strbuf_append( buf, "\\r" ); break;
        case '\t': strbuf_append( buf, "\\t" ); break;
        default:
            if (*p < 0x20)
            {
                snprintf( tmp, sizeof(tmp), "\\u%04x", *p );
                strbuf_append( buf, tmp );
            }
            else strbuf_append_len( buf, (const char *)p, 1 );
        }
    }
    strbuf_append( buf, "\"" );
}

static void append_json_member( struct strbuf *buf, const char *name, const char *value )
{
    if (!value || !*value) return;
    strbuf_append( buf, ",\"" );
    strbuf_append( buf, name );
    strbuf_append( buf, "\":" );
    append_json_string( buf, value );
}

static const char *location_name( UINT32 location )
{
    static const char *names[] =
    {
        "DocumentsLibrary", "ComputerFolder", "Desktop", "Downloads", NULL,
        "MusicLibrary", "PicturesLibrary", "VideosLibrary", "Objects3D", "Unspecified",
    };
    return location < ARRAY_SIZE(names) ? names[location] : NULL;
}

static const char *mode_name( UINT32 mode )
{
    switch (mode)
    {
    case UNIX_PICKER_OPEN_MULTIPLE: return "openMany";
    case UNIX_PICKER_SAVE: return "save";
    case UNIX_PICKER_FOLDER: return "folder";
    default: return "open";
    }
}

/* filters: lines "Label\tpattern;pattern\n" -> [{"name":..,"patterns":[..]}] */
static void append_filters( struct strbuf *buf, const char *filters )
{
    const char *line = filters, *end, *tab, *p, *q;
    BOOL first = TRUE;
    char *tmp;

    if (!filters || !*filters) return;
    strbuf_append( buf, ",\"filters\":[" );
    for (; line && *line; line = *end ? end + 1 : end)
    {
        BOOL first_pattern = TRUE;

        if (!(end = strchr( line, '\n' ))) end = line + strlen( line );
        if (end == line) continue;
        if (!(tab = memchr( line, '\t', end - line ))) tab = line;

        strbuf_append( buf, first ? "{\"name\":" : ",{\"name\":" );
        first = FALSE;
        if (!(tmp = strndup( line, tab - line ))) { buf->failed = TRUE; return; }
        append_json_string( buf, tmp );
        free( tmp );
        strbuf_append( buf, ",\"patterns\":[" );
        for (p = tab == line ? line : tab + 1; p < end; p = q + 1)
        {
            while (p < end && (*p == ';' || *p == ' ')) p++;
            if (p >= end) break;
            for (q = p; q < end && *q != ';'; q++) ;
            if (!(tmp = strndup( p, q - p ))) { buf->failed = TRUE; return; }
            if (!first_pattern) strbuf_append( buf, "," );
            first_pattern = FALSE;
            append_json_string( buf, tmp );
            free( tmp );
        }
        strbuf_append( buf, "]}" );
    }
    strbuf_append( buf, "]" );
}

static char *build_request( struct picker_show_params *params )
{
    struct strbuf buf = {0};
    char tmp[64];

    strbuf_append( &buf, "{\"v\":1,\"op\":" );
    append_json_string( &buf, mode_name( params->mode ) );
    append_json_member( &buf, "title", params->title );
    append_json_member( &buf, "acceptLabel", params->accept_label );
    append_filters( &buf, params->filters );
    append_json_member( &buf, "startLocation", location_name( params->start_location ) );
    append_json_member( &buf, "startDir", params->current_folder );
    append_json_member( &buf, "suggestedName", params->current_name );
    if (params->x11_window)
    {
        snprintf( tmp, sizeof(tmp), "x11:0x%llx", (unsigned long long)params->x11_window );
        append_json_member( &buf, "parentWindow", tmp );
    }
    strbuf_append( &buf, "}\n" );
    if (buf.failed)
    {
        free( buf.data );
        return NULL;
    }
    return buf.data;
}

/*
 * minimal JSON reader for the response object
 */

static const char *skip_ws( const char *p )
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static int hex_value( char c )
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void append_utf8( struct strbuf *out, unsigned int cp )
{
    char tmp[4];
    if (cp < 0x80) { tmp[0] = cp; strbuf_append_len( out, tmp, 1 ); }
    else if (cp < 0x800)
    {
        tmp[0] = 0xc0 | (cp >> 6); tmp[1] = 0x80 | (cp & 0x3f);
        strbuf_append_len( out, tmp, 2 );
    }
    else if (cp < 0x10000)
    {
        tmp[0] = 0xe0 | (cp >> 12); tmp[1] = 0x80 | ((cp >> 6) & 0x3f); tmp[2] = 0x80 | (cp & 0x3f);
        strbuf_append_len( out, tmp, 3 );
    }
    else
    {
        tmp[0] = 0xf0 | (cp >> 18); tmp[1] = 0x80 | ((cp >> 12) & 0x3f);
        tmp[2] = 0x80 | ((cp >> 6) & 0x3f); tmp[3] = 0x80 | (cp & 0x3f);
        strbuf_append_len( out, tmp, 4 );
    }
}

static BOOL read_hex4( const char *p, unsigned int *value )
{
    int i, h;
    *value = 0;
    for (i = 0; i < 4; i++)
    {
        if ((h = hex_value( p[i] )) < 0) return FALSE;
        *value = (*value << 4) | h;
    }
    return TRUE;
}

/* parses a string literal at p (pointing at '"'); appends the decoded value to out when not NULL */
static const char *parse_string( const char *p, struct strbuf *out )
{
    if (*p != '"') return NULL;
    for (p++; *p && *p != '"'; p++)
    {
        if (*p != '\\')
        {
            if (out) strbuf_append_len( out, p, 1 );
            continue;
        }
        switch (*++p)
        {
        case '"': case '\\': case '/': if (out) strbuf_append_len( out, p, 1 ); break;
        case 'b': if (out) strbuf_append( out, "\b" ); break;
        case 'f': if (out) strbuf_append( out, "\f" ); break;
        case 'n': if (out) strbuf_append( out, "\n" ); break;
        case 'r': if (out) strbuf_append( out, "\r" ); break;
        case 't': if (out) strbuf_append( out, "\t" ); break;
        case 'u':
        {
            unsigned int cp, lo;
            if (!read_hex4( p + 1, &cp )) return NULL;
            p += 4;
            if (cp >= 0xd800 && cp < 0xdc00 && p[1] == '\\' && p[2] == 'u' && read_hex4( p + 3, &lo ) &&
                lo >= 0xdc00 && lo < 0xe000)
            {
                cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                p += 6;
            }
            if (out) append_utf8( out, cp );
            break;
        }
        default: return NULL;
        }
    }
    return *p == '"' ? p + 1 : NULL;
}

/* skips any JSON value */
static const char *skip_value( const char *p, int depth )
{
    p = skip_ws( p );
    if (depth > 32) return NULL;
    if (*p == '"') return parse_string( p, NULL );
    if (*p == '{' || *p == '[')
    {
        char close = *p == '{' ? '}' : ']';
        p = skip_ws( p + 1 );
        if (*p == close) return p + 1;
        for (;;)
        {
            if (close == '}')
            {
                if (!(p = parse_string( skip_ws( p ), NULL ))) return NULL;
                p = skip_ws( p );
                if (*p++ != ':') return NULL;
            }
            if (!(p = skip_value( p, depth + 1 ))) return NULL;
            p = skip_ws( p );
            if (*p == ',') { p++; continue; }
            return *p == close ? p + 1 : NULL;
        }
    }
    while (*p && !strchr( ",}] \t\r\n", *p )) p++;
    return p;
}

/* {"ok":bool,"paths":[..],...} -> '\n'-separated paths in out (only when ok) */
static BOOL parse_response( const char *json, struct strbuf *out, char *error, size_t error_size )
{
    const char *p = skip_ws( json );
    struct strbuf paths = {0};
    struct strbuf key = {0};
    BOOL ok = FALSE, ret = FALSE;

    *error = 0;
    if (*p++ != '{') goto done;
    p = skip_ws( p );
    if (*p == '}') goto done;
    for (;;)
    {
        key.len = 0;
        if (!(p = parse_string( skip_ws( p ), &key ))) goto done;
        p = skip_ws( p );
        if (*p++ != ':') goto done;
        p = skip_ws( p );

        if (key.data && !strcmp( key.data, "ok" ))
        {
            ok = !strncmp( p, "true", 4 );
            if (!(p = skip_value( p, 0 ))) goto done;
        }
        else if (key.data && !strcmp( key.data, "paths" ) && *p == '[')
        {
            p = skip_ws( p + 1 );
            while (*p == '"')
            {
                size_t start;
                if (paths.len) strbuf_append( &paths, "\n" );
                start = paths.len;
                if (!(p = parse_string( p, &paths ))) goto done;
                if (paths.len == start && start) paths.data[--paths.len] = 0; /* drop empty entries */
                p = skip_ws( p );
                if (*p == ',') p = skip_ws( p + 1 );
            }
            if (*p++ != ']') goto done;
        }
        else if (key.data && !strcmp( key.data, "error" ) && *p == '"')
        {
            struct strbuf err = {0};
            if (!(p = parse_string( p, &err ))) { free( err.data ); goto done; }
            if (err.data) snprintf( error, error_size, "%s", err.data );
            free( err.data );
        }
        else if (!(p = skip_value( p, 0 ))) goto done;

        p = skip_ws( p );
        if (*p == ',') { p++; continue; }
        if (*p != '}') goto done;
        break;
    }
    ret = !paths.failed;
    if (ok && paths.len) strbuf_append_len( out, paths.data, paths.len );

done:
    free( paths.data );
    free( key.data );
    return ret;
}

static char *get_socket_path(void)
{
    const char *env = getenv( "ORIONBE_PICKER_SOCKET" ), *home;
    char path[4096];

    if (env && *env) return strdup( env );
    if (!(home = getenv( "HOME" )) || !*home) return NULL;
    snprintf( path, sizeof(path), "%s/OrionBE/cache/xodus-run/picker.sock", home );
    WARN( "ORIONBE_PICKER_SOCKET not set, trying %s\n", debugstr_a(path) );
    return strdup( path );
}

static BOOL write_all( int fd, const char *data, size_t len )
{
    while (len)
    {
        ssize_t ret = send( fd, data, len, MSG_NOSIGNAL );
        if (ret < 0)
        {
            if (errno == EINTR) continue;
            return FALSE;
        }
        data += ret;
        len -= ret;
    }
    return TRUE;
}

/* reads one '\n'-terminated line (or up to EOF); blocks as long as the dialog is up */
static BOOL read_line( int fd, struct strbuf *out )
{
    char buf[4096];

    for (;;)
    {
        ssize_t ret = recv( fd, buf, sizeof(buf), 0 );
        char *nl;
        if (ret < 0)
        {
            if (errno == EINTR) continue;
            return FALSE;
        }
        if (!ret) return out->len > 0;
        if ((nl = memchr( buf, '\n', ret )))
        {
            strbuf_append_len( out, buf, nl - buf );
            return !out->failed;
        }
        strbuf_append_len( out, buf, ret );
        if (out->failed || out->len > MAX_RESPONSE) return FALSE;
    }
}

/* returns STATUS_SUCCESS (out holds the paths, empty = cancelled) or an error status */
static NTSTATUS ask_broker( struct picker_show_params *params, struct strbuf *out )
{
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    struct strbuf response = {0};
    char *path, *request, error[256];
    NTSTATUS status = STATUS_PIPE_BROKEN;
    int fd;

    if (!(path = get_socket_path()))
    {
        WARN( "no file picker broker socket (ORIONBE_PICKER_SOCKET / HOME unset); treating as cancelled\n" );
        return STATUS_OBJECT_PATH_NOT_FOUND;
    }
    if (strlen( path ) >= sizeof(addr.sun_path))
    {
        WARN( "broker socket path %s is too long for AF_UNIX; treating as cancelled\n", debugstr_a(path) );
        free( path );
        return STATUS_NAME_TOO_LONG;
    }
    strcpy( addr.sun_path, path );

    if (!(request = build_request( params )))
    {
        free( path );
        return STATUS_NO_MEMORY;
    }

    if ((fd = socket( AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0 )) < 0)
    {
        WARN( "socket() failed: %s\n", strerror( errno ) );
        goto done;
    }
    if (connect( fd, (struct sockaddr *)&addr, sizeof(addr) ) < 0)
    {
        WARN( "cannot reach the OrionBE launcher file picker broker at %s (%s); is the launcher running? "
              "treating as cancelled\n", debugstr_a(path), strerror( errno ) );
        status = STATUS_OBJECT_PATH_NOT_FOUND;
        goto done;
    }

    TRACE( "request %s", debugstr_a(request) );
    if (!write_all( fd, request, strlen( request ) ))
    {
        WARN( "sending the picker request failed: %s\n", strerror( errno ) );
        goto done;
    }

    if (!read_line( fd, &response ) || !response.data)
    {
        WARN( "the file picker broker closed the connection without an answer; treating as cancelled\n" );
        goto done;
    }
    TRACE( "response %s\n", debugstr_a(response.data) );

    if (!parse_response( response.data, out, error, sizeof(error) ))
    {
        WARN( "malformed broker response %s; treating as cancelled\n", debugstr_a(response.data) );
        out->len = 0;
        goto done;
    }
    if (*error) WARN( "broker: %s\n", debugstr_a(error) );
    status = out->failed ? STATUS_NO_MEMORY : STATUS_SUCCESS;

done:
    if (fd >= 0) close( fd );
    free( response.data );
    free( request );
    free( path );
    return status;
}

static BOOL test_override( struct picker_show_params *params, struct strbuf *out )
{
    const char *value = getenv( "ORIONBE_PICKER_TEST_RESULT" );
    const char *p;

    if (!value) return FALSE;
    WARN( "ORIONBE_PICKER_TEST_RESULT=%s set, not asking the broker\n", debugstr_a(value) );
    if (!*value || !strcmp( value, "cancel" )) return TRUE;
    for (p = value; *p; p++)
    {
        if (*p == '|')
        {
            if (params->mode != UNIX_PICKER_OPEN_MULTIPLE) break;
            strbuf_append( out, "\n" );
        }
        else strbuf_append_len( out, p, 1 );
    }
    return TRUE;
}

static NTSTATUS picker_show( void *args )
{
    struct picker_show_params *params = args;
    struct strbuf out = {0};
    NTSTATUS status = STATUS_SUCCESS;

    TRACE( "mode %u, location %u, window %#llx, title %s, accept %s, filters %s, name %s, folder %s\n",
           params->mode, params->start_location, (unsigned long long)params->x11_window, debugstr_a(params->title),
           debugstr_a(params->accept_label), debugstr_a(params->filters), debugstr_a(params->current_name),
           debugstr_a(params->current_folder) );

    params->result_len = 0;
    if (params->result_size) params->result[0] = 0;

    if (!test_override( params, &out ))
    {
        status = ask_broker( params, &out );
        /* every failure to reach the broker is a cancel for the game, never an error */
        if (status != STATUS_SUCCESS && status != STATUS_NO_MEMORY)
        {
            out.len = 0;
            status = STATUS_SUCCESS;
        }
    }
    if (out.failed) status = STATUS_NO_MEMORY;

    if (status == STATUS_SUCCESS && out.len && params->result_size)
    {
        /* keep only whole paths that fit */
        size_t len = out.len;
        while (len && len + 1 > params->result_size)
        {
            while (len && out.data[len - 1] != '\n') len--;
            if (len) len--; /* drop the separator */
        }
        if (len) memcpy( params->result, out.data, len );
        params->result[len] = 0;
        params->result_len = len;
    }
    free( out.data );
    return status;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    picker_show,
};

C_ASSERT( ARRAY_SIZE(__wine_unix_call_funcs) == unix_funcs_count );
