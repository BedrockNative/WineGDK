/*
 * Microsoft.Windows.Storage.Pickers - XDG Desktop Portal client
 *
 * Copyright 2026 OrionBE contributors
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
#include <dlfcn.h>
#include <pthread.h>
#ifdef SONAME_LIBDBUS_1
# include <dbus/dbus.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/debug.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(pickers);

#ifdef SONAME_LIBDBUS_1

#define PORTAL_BUS "org.freedesktop.portal.Desktop"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_CHOOSER "org.freedesktop.portal.FileChooser"
#define PORTAL_REQUEST "org.freedesktop.portal.Request"

#define DBUS_FUNCS \
    DO_FUNC(dbus_bus_add_match); \
    DO_FUNC(dbus_bus_get_private); \
    DO_FUNC(dbus_connection_close); \
    DO_FUNC(dbus_connection_pop_message); \
    DO_FUNC(dbus_connection_read_write); \
    DO_FUNC(dbus_connection_send_with_reply_and_block); \
    DO_FUNC(dbus_connection_set_exit_on_disconnect); \
    DO_FUNC(dbus_connection_unref); \
    DO_FUNC(dbus_error_free); \
    DO_FUNC(dbus_error_is_set); \
    DO_FUNC(dbus_message_get_args); \
    DO_FUNC(dbus_message_get_path); \
    DO_FUNC(dbus_message_get_sender); \
    DO_FUNC(dbus_message_has_signature); \
    DO_FUNC(dbus_message_is_signal); \
    DO_FUNC(dbus_message_iter_append_basic); \
    DO_FUNC(dbus_message_iter_append_fixed_array); \
    DO_FUNC(dbus_message_iter_close_container); \
    DO_FUNC(dbus_message_iter_get_arg_type); \
    DO_FUNC(dbus_message_iter_get_basic); \
    DO_FUNC(dbus_message_iter_init); \
    DO_FUNC(dbus_message_iter_init_append); \
    DO_FUNC(dbus_message_iter_next); \
    DO_FUNC(dbus_message_iter_open_container); \
    DO_FUNC(dbus_message_iter_recurse); \
    DO_FUNC(dbus_message_new_method_call); \
    DO_FUNC(dbus_message_unref); \
    DO_FUNC(dbus_threads_init_default)

#define DO_FUNC(f) static typeof(f) *p_##f
DBUS_FUNCS;
#undef DO_FUNC

static pthread_once_t dbus_once = PTHREAD_ONCE_INIT;
static BOOL dbus_available;

static void load_dbus(void)
{
    void *handle;

    if (!(handle = dlopen( SONAME_LIBDBUS_1, RTLD_NOW ))) return;
#define DO_FUNC(f) if (!(p_##f = dlsym( handle, #f ))) goto failed
    DBUS_FUNCS;
#undef DO_FUNC
    if (!p_dbus_threads_init_default()) goto failed;
    dbus_available = TRUE;
    return;
failed:
    dlclose( handle );
}

static BOOL append_option( DBusMessageIter *options, const char *key, int type,
                           const char *signature, const void *value )
{
    DBusMessageIter entry, variant;

    return p_dbus_message_iter_open_container( options, DBUS_TYPE_DICT_ENTRY, NULL, &entry ) &&
           p_dbus_message_iter_append_basic( &entry, DBUS_TYPE_STRING, &key ) &&
           p_dbus_message_iter_open_container( &entry, DBUS_TYPE_VARIANT, signature, &variant ) &&
           p_dbus_message_iter_append_basic( &variant, type, value ) &&
           p_dbus_message_iter_close_container( &entry, &variant ) &&
           p_dbus_message_iter_close_container( options, &entry );
}

static BOOL append_string_option( DBusMessageIter *options, const char *key, const char *value )
{
    return !value || !*value || append_option( options, key, DBUS_TYPE_STRING, "s", &value );
}

static BOOL append_folder( DBusMessageIter *options, const char *folder )
{
    const char *key = "current_folder";
    DBusMessageIter entry, variant, array;

    if (!folder || !*folder) return TRUE;
    return p_dbus_message_iter_open_container( options, DBUS_TYPE_DICT_ENTRY, NULL, &entry ) &&
           p_dbus_message_iter_append_basic( &entry, DBUS_TYPE_STRING, &key ) &&
           p_dbus_message_iter_open_container( &entry, DBUS_TYPE_VARIANT, "ay", &variant ) &&
           p_dbus_message_iter_open_container( &variant, DBUS_TYPE_ARRAY, "y", &array ) &&
           p_dbus_message_iter_append_fixed_array( &array, DBUS_TYPE_BYTE, &folder, strlen(folder) + 1 ) &&
           p_dbus_message_iter_close_container( &variant, &array ) &&
           p_dbus_message_iter_close_container( &entry, &variant ) &&
           p_dbus_message_iter_close_container( options, &entry );
}

/* "Label\tpattern;pattern\n" -> a(sa(us)), using glob (type 0) filters. */
static BOOL append_filters( DBusMessageIter *options, const char *filters )
{
    DBusMessageIter entry, variant, array, filter, patterns, pattern;
    const char *key = "filters", *line, *end, *tab, *p, *q;
    dbus_uint32_t glob = 0;
    char *label = NULL, *value = NULL;
    BOOL ret = FALSE;

    if (!filters || !*filters) return TRUE;
    if (!p_dbus_message_iter_open_container( options, DBUS_TYPE_DICT_ENTRY, NULL, &entry ) ||
        !p_dbus_message_iter_append_basic( &entry, DBUS_TYPE_STRING, &key ) ||
        !p_dbus_message_iter_open_container( &entry, DBUS_TYPE_VARIANT, "a(sa(us))", &variant ) ||
        !p_dbus_message_iter_open_container( &variant, DBUS_TYPE_ARRAY, "(sa(us))", &array )) goto done;

    for (line = filters; *line; line = *end ? end + 1 : end)
    {
        if (!(end = strchr( line, '\n' ))) end = line + strlen( line );
        if (end == line) continue;
        if (!(tab = memchr( line, '\t', end - line ))) tab = line;
        if (!(label = strndup( line, tab - line ))) goto done;
        if (!p_dbus_message_iter_open_container( &array, DBUS_TYPE_STRUCT, NULL, &filter ) ||
            !p_dbus_message_iter_append_basic( &filter, DBUS_TYPE_STRING, &label ) ||
            !p_dbus_message_iter_open_container( &filter, DBUS_TYPE_ARRAY, "(us)", &patterns )) goto done;
        free( label );
        label = NULL;
        for (p = tab == line ? line : tab + 1; p < end; p = q < end ? q + 1 : end)
        {
            while (p < end && (*p == ';' || *p == ' ')) p++;
            if (p == end) break;
            for (q = p; q < end && *q != ';'; q++) ;
            if (!(value = strndup( p, q - p ))) goto done;
            if (!p_dbus_message_iter_open_container( &patterns, DBUS_TYPE_STRUCT, NULL, &pattern ) ||
                !p_dbus_message_iter_append_basic( &pattern, DBUS_TYPE_UINT32, &glob ) ||
                !p_dbus_message_iter_append_basic( &pattern, DBUS_TYPE_STRING, &value ) ||
                !p_dbus_message_iter_close_container( &patterns, &pattern )) goto done;
            free( value );
            value = NULL;
        }
        if (!p_dbus_message_iter_close_container( &filter, &patterns ) ||
            !p_dbus_message_iter_close_container( &array, &filter )) goto done;
    }
    ret = p_dbus_message_iter_close_container( &variant, &array ) &&
          p_dbus_message_iter_close_container( &entry, &variant ) &&
          p_dbus_message_iter_close_container( options, &entry );
done:
    free( label );
    free( value );
    return ret;
}

/* Read the desktop's user-dirs.dirs without executing shell code. */
static char *get_start_folder( UINT32 location )
{
    static const char *keys[] = { "DOCUMENTS", NULL, "DESKTOP", "DOWNLOAD", NULL,
                                 "MUSIC", "PICTURES", "VIDEOS" };
    static const char *defaults[] = { "Documents", NULL, "Desktop", "Downloads", NULL,
                                     "Music", "Pictures", "Videos" };
    const char *home = getenv( "HOME" ), *config = getenv( "XDG_CONFIG_HOME" );
    char filename[4096], key[64], line[4096], *p, *q, *folder = NULL;
    FILE *file;

    if (location == 1) return strdup( "/" ); /* ComputerFolder */
    if (!home || !*home || location >= ARRAY_SIZE(keys) || !keys[location]) return NULL;
    if (config && config[0] == '/') snprintf( filename, sizeof(filename), "%s/user-dirs.dirs", config );
    else snprintf( filename, sizeof(filename), "%s/.config/user-dirs.dirs", home );
    snprintf( key, sizeof(key), "XDG_%s_DIR=", keys[location] );
    if ((file = fopen( filename, "r" )))
    {
        while (fgets( line, sizeof(line), file ))
        {
            p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp( p, key, strlen(key) )) continue;
            p += strlen(key);
            if (*p++ != '"') continue;
            if (!(q = strrchr( p, '"' ))) continue;
            *q = 0;
            if (!strncmp( p, "$HOME/", 6 ))
            {
                if ((folder = malloc( strlen(home) + strlen(p + 5) + 1 )))
                    sprintf( folder, "%s%s", home, p + 5 );
            }
            else if (!strcmp( p, "$HOME" )) folder = strdup( home );
            else if (*p == '/') folder = strdup( p );
            if (folder)
            {
                /* Unescape quoted backslashes, quotes and dollar signs. */
                for (p = q = folder; *p; p++, q++)
                {
                    if (*p == '\\' && (p[1] == '\\' || p[1] == '"' || p[1] == '$' || p[1] == '`')) p++;
                    *q = *p;
                }
                *q = 0;
                break;
            }
        }
        fclose( file );
    }
    if (!folder && (folder = malloc( strlen(home) + strlen(defaults[location]) + 2 )))
        sprintf( folder, "%s/%s", home, defaults[location] );
    return folder;
}

static DBusMessage *build_request( const struct picker_show_params *params )
{
    const char *method = params->mode == UNIX_PICKER_SAVE ? "SaveFile" : "OpenFile";
    const char *title = params->title ? params->title : "";
    const char *parent;
    char window[64] = "", *folder = NULL;
    DBusMessage *message;
    DBusMessageIter args, options;
    dbus_bool_t multiple = params->mode == UNIX_PICKER_OPEN_MULTIPLE;
    dbus_bool_t directory = params->mode == UNIX_PICKER_FOLDER;
    dbus_bool_t modal = TRUE;
    BOOL ret;

    if (!(message = p_dbus_message_new_method_call( PORTAL_BUS, PORTAL_PATH, PORTAL_CHOOSER, method ))) return NULL;
    if (params->x11_window) snprintf( window, sizeof(window), "x11:%llx", (unsigned long long)params->x11_window );
    parent = window;
    if (!params->current_folder || !*params->current_folder) folder = get_start_folder( params->start_location );
    p_dbus_message_iter_init_append( message, &args );
    ret = p_dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &parent ) &&
          p_dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &title ) &&
          p_dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "{sv}", &options ) &&
          append_option( &options, "modal", DBUS_TYPE_BOOLEAN, "b", &modal ) &&
          append_string_option( &options, "accept_label", params->accept_label ) &&
          append_filters( &options, params->filters ) &&
          append_folder( &options, folder ? folder : params->current_folder );
    if (ret && params->mode == UNIX_PICKER_SAVE)
        ret = append_string_option( &options, "current_name", params->current_name );
    else if (ret)
        ret = append_option( &options, "multiple", DBUS_TYPE_BOOLEAN, "b", &multiple ) &&
              append_option( &options, "directory", DBUS_TYPE_BOOLEAN, "b", &directory );
    if (ret) ret = p_dbus_message_iter_close_container( &args, &options );
    free( folder );
    if (ret) return message;
    p_dbus_message_unref( message );
    return NULL;
}

static int hex_value( char c )
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Portal URIs are local file:// URIs. Preserve UTF-8 and decode percent escapes once. */
static NTSTATUS append_uri( struct picker_show_params *params, const char *uri )
{
    const char *p;
    char *path, *q;
    size_t len;
    int hi, lo;

    if (!strncmp( uri, "file:///", 8 )) p = uri + 7;
    else if (!strncmp( uri, "file://localhost/", 17 )) p = uri + 16;
    else return STATUS_INVALID_PARAMETER;
    if (!(path = malloc( strlen(p) + 1 ))) return STATUS_NO_MEMORY;
    for (q = path; *p; p++, q++)
    {
        if (*p == '%')
        {
            if (!p[1] || !p[2] || (hi = hex_value(p[1])) < 0 || (lo = hex_value(p[2])) < 0 || !(hi || lo))
                goto invalid;
            *q = (hi << 4) | lo;
            p += 2;
        }
        else if (*p == '?' || *p == '#') goto invalid;
        else *q = *p;
    }
    *q = 0;
    len = q - path + 1;
    if (len > params->result_size - params->result_len)
    {
        free( path );
        return STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( params->result + params->result_len, path, len );
    params->result_len += len;
    free( path );
    return STATUS_SUCCESS;
invalid:
    free( path );
    return STATUS_INVALID_PARAMETER;
}

static NTSTATUS parse_response( DBusMessage *message, struct picker_show_params *params )
{
    DBusMessageIter args, results, entry, variant, uris;
    const char *key, *uri;
    dbus_uint32_t response;
    NTSTATUS status;
    BOOL found = FALSE;

    if (!p_dbus_message_has_signature( message, "ua{sv}" )) return STATUS_INVALID_PARAMETER;
    p_dbus_message_iter_init( message, &args );
    p_dbus_message_iter_get_basic( &args, &response );
    if (response == 1) return STATUS_SUCCESS; /* User cancelled. */
    if (response) return STATUS_UNSUCCESSFUL;
    p_dbus_message_iter_next( &args );
    p_dbus_message_iter_recurse( &args, &results );
    while (p_dbus_message_iter_get_arg_type( &results ) == DBUS_TYPE_DICT_ENTRY)
    {
        p_dbus_message_iter_recurse( &results, &entry );
        p_dbus_message_iter_get_basic( &entry, &key );
        p_dbus_message_iter_next( &entry );
        p_dbus_message_iter_recurse( &entry, &variant );
        if (!strcmp( key, "uris" ))
        {
            if (found || p_dbus_message_iter_get_arg_type( &variant ) != DBUS_TYPE_ARRAY)
                return STATUS_INVALID_PARAMETER;
            found = TRUE;
            p_dbus_message_iter_recurse( &variant, &uris );
            while (p_dbus_message_iter_get_arg_type( &uris ) != DBUS_TYPE_INVALID)
            {
                if (p_dbus_message_iter_get_arg_type( &uris ) != DBUS_TYPE_STRING) return STATUS_INVALID_PARAMETER;
                if (params->result_len && params->mode != UNIX_PICKER_OPEN_MULTIPLE) return STATUS_INVALID_PARAMETER;
                p_dbus_message_iter_get_basic( &uris, &uri );
                if ((status = append_uri( params, uri ))) return status;
                p_dbus_message_iter_next( &uris );
            }
        }
        p_dbus_message_iter_next( &results );
    }
    return found && params->result_len ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
}

static NTSTATUS portal_show( struct picker_show_params *params )
{
    DBusError error = DBUS_ERROR_INIT;
    DBusConnection *connection;
    DBusMessage *request = NULL, *reply = NULL, *message;
    const char *path, *owner, *name, *old_owner, *new_owner;
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;

    pthread_once( &dbus_once, load_dbus );
    if (!dbus_available) return STATUS_NOT_IMPLEMENTED;
    if (!(connection = p_dbus_bus_get_private( DBUS_BUS_SESSION, &error ))) goto done;
    p_dbus_connection_set_exit_on_disconnect( connection, FALSE );

    /* Subscribe before calling: a fast Response may precede the method reply. A private
     * connection lets us queue all portal responses, then match the returned handle. */
    p_dbus_bus_add_match( connection, "type='signal',sender='" PORTAL_BUS "',interface='"
                         PORTAL_REQUEST "',member='Response'", &error );
    if (p_dbus_error_is_set( &error )) goto done;
    p_dbus_bus_add_match( connection, "type='signal',sender='org.freedesktop.DBus',"
                         "interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='"
                         PORTAL_BUS "'", &error );
    if (p_dbus_error_is_set( &error )) goto done;
    if (!(request = build_request( params )))
    {
        status = STATUS_NO_MEMORY;
        goto done;
    }
    /* Only service startup / method dispatch is bounded; the user can take as long as
     * needed in the chooser. Bus disconnect or portal owner loss ends the wait. */
    if (!(reply = p_dbus_connection_send_with_reply_and_block( connection, request, 5000, &error ))) goto done;
    status = STATUS_INVALID_PARAMETER;
    if (!p_dbus_message_has_signature( reply, "o" ) ||
        !p_dbus_message_get_args( reply, NULL, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID ) ||
        !(owner = p_dbus_message_get_sender( reply ))) goto done;

    for (;;)
    {
        if (!(message = p_dbus_connection_pop_message( connection )))
        {
            if (p_dbus_connection_read_write( connection, -1 )) continue;
            status = STATUS_NOT_IMPLEMENTED;
            break;
        }
        if (p_dbus_message_is_signal( message, PORTAL_REQUEST, "Response" ) &&
            p_dbus_message_get_sender( message ) && !strcmp( p_dbus_message_get_sender( message ), owner ) &&
            !strcmp( p_dbus_message_get_path( message ), path ))
        {
            status = parse_response( message, params );
            p_dbus_message_unref( message );
            break;
        }
        if (p_dbus_message_is_signal( message, DBUS_INTERFACE_DBUS, "NameOwnerChanged" ) &&
            p_dbus_message_get_sender( message ) &&
            !strcmp( p_dbus_message_get_sender( message ), DBUS_SERVICE_DBUS ) &&
            p_dbus_message_get_args( message, NULL, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &old_owner,
                                    DBUS_TYPE_STRING, &new_owner, DBUS_TYPE_INVALID ) &&
            !strcmp( name, PORTAL_BUS ) && !strcmp( old_owner, owner ) && strcmp( new_owner, owner ))
        {
            status = STATUS_NOT_IMPLEMENTED;
            p_dbus_message_unref( message );
            break;
        }
        p_dbus_message_unref( message );
    }
done:
    if (p_dbus_error_is_set( &error ))
    {
        WARN( "file chooser portal unavailable: %s: %s\n", error.name, error.message );
        if (!strcmp( error.name, DBUS_ERROR_NO_MEMORY )) status = STATUS_NO_MEMORY;
        p_dbus_error_free( &error );
    }
    if (reply) p_dbus_message_unref( reply );
    if (request) p_dbus_message_unref( request );
    if (connection)
    {
        p_dbus_connection_close( connection );
        p_dbus_connection_unref( connection );
    }
    return status;
}

#endif /* SONAME_LIBDBUS_1 */

static NTSTATUS picker_show( void *args )
{
    struct picker_show_params *params = args;
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;

    params->result_len = 0;
    if (params->result_size) params->result[0] = 0;
#ifdef SONAME_LIBDBUS_1
    status = portal_show( params );
#endif
    if (status)
    {
        params->result_len = 0;
        if (params->result_size) params->result[0] = 0;
        WARN( "file chooser failed, status %#x\n", (unsigned int)status );
    }
    return status;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    picker_show,
};

C_ASSERT( ARRAY_SIZE(__wine_unix_call_funcs) == unix_funcs_count );
