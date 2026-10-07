/*
 * HTTP/2 transport for WinHTTP, using the statically linked nghttp2 library.
 *
 * Copyright 2026 WineGDK contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <nghttp2/nghttp2.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "ws2tcpip.h"
#include "winhttp.h"
#include "wine/debug.h"
#include "winhttp_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(http2);

#define MAX_HTTP2_HEADER_BYTES (1024 * 1024)

static const struct data_stream_vtbl http2_stream_vtbl;

static nghttp2_session *session_for( struct netconn *conn )
{
    return conn->http2_session;
}

static WCHAR *header_to_wide( const uint8_t *text, size_t len )
{
    WCHAR *ret;
    int count;

    if (!len)
    {
        if ((ret = malloc( sizeof(*ret) ))) *ret = 0;
        return ret;
    }
    if (len > INT_MAX || !(count = MultiByteToWideChar( CP_UTF8, 0, (const char *)text, len, NULL, 0 ))) return NULL;
    if (!(ret = malloc( (count + 1) * sizeof(*ret) ))) return NULL;
    MultiByteToWideChar( CP_UTF8, 0, (const char *)text, len, ret, count );
    ret[count] = 0;
    return ret;
}

static char *header_to_utf8( const WCHAR *text )
{
    char *ret;
    int count;

    if (!(count = WideCharToMultiByte( CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL ))) return NULL;
    if (!(ret = malloc( count ))) return NULL;
    WideCharToMultiByte( CP_UTF8, 0, text, -1, ret, count, NULL, NULL );
    return ret;
}

static int on_header( nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name,
                      size_t name_len, const uint8_t *value, size_t value_len, uint8_t flags, void *user_data )
{
    struct request *request = nghttp2_session_get_stream_user_data( session, frame->hd.stream_id );
    struct http2_stream *stream;
    WCHAR *field, *contents;
    DWORD ret;

    if (!request || frame->hd.type != NGHTTP2_HEADERS) return 0;
    stream = &request->http2_stream;
    if (name_len > MAX_HTTP2_HEADER_BYTES - stream->header_bytes ||
        value_len > MAX_HTTP2_HEADER_BYTES - stream->header_bytes - name_len)
    {
        stream->error = ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    stream->header_bytes += name_len + value_len;

    if (name_len == 7 && !memcmp( name, ":status", 7 ))
    {
        if (value_len != 3 || value[0] < '1' || value[0] > '5' ||
            value[1] < '0' || value[1] > '9' || value[2] < '0' || value[2] > '9')
        {
            stream->error = ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
            return NGHTTP2_ERR_CALLBACK_FAILURE;
        }
        stream->informational = value[0] == '1';
        if (stream->informational) return 0;
        stream->final_status_seen = TRUE;
        if (!(contents = header_to_wide( value, value_len ))) goto oom;
        ret = process_header( request, L"Status", contents,
                              WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE, FALSE );
        free( contents );
        if (ret) stream->error = ret;
        return ret ? NGHTTP2_ERR_CALLBACK_FAILURE : 0;
    }
    if (name_len && name[0] == ':') return 0;
    if (stream->informational) return 0;
    if (!(field = header_to_wide( name, name_len ))) goto oom;
    if (!(contents = header_to_wide( value, value_len )))
    {
        free( field );
        goto oom;
    }
    ret = process_header( request, field, contents, WINHTTP_ADDREQ_FLAG_ADD, FALSE );
    free( field );
    free( contents );
    if (ret) stream->error = ret;
    return ret ? NGHTTP2_ERR_CALLBACK_FAILURE : 0;

oom:
    stream->error = ERROR_OUTOFMEMORY;
    return NGHTTP2_ERR_CALLBACK_FAILURE;
}

static int on_frame_recv( nghttp2_session *session, const nghttp2_frame *frame, void *user_data )
{
    struct netconn *conn = user_data;
    struct request *request = nghttp2_session_get_stream_user_data( session, frame->hd.stream_id );
    if (frame->hd.type == NGHTTP2_GOAWAY)
    {
        /* Existing streams up to last_stream_id can finish.  The connection
         * must not be offered to another request, even while they do. */
        TRACE( "HTTP/2 GOAWAY last stream %d, error %#x\n", frame->goaway.last_stream_id,
               frame->goaway.error_code );
        InterlockedExchange( &conn->http2_draining, TRUE );
        WakeAllConditionVariable( &conn->http2_cv );
    }
    if (request && frame->hd.type == NGHTTP2_HEADERS && request->http2_stream.final_status_seen)
        request->http2_stream.headers_received = TRUE;
    return 0;
}

static int on_data_chunk_recv( nghttp2_session *session, uint8_t flags, int32_t stream_id,
                               const uint8_t *data, size_t len, void *user_data )
{
    struct request *request = nghttp2_session_get_stream_user_data( session, stream_id );
    struct http2_stream *stream;
    BYTE *new_body;
    size_t new_capacity;

    if (!len) return 0;
    /* Keep the connection window open for other streams, while each stream's
     * own window limits data buffered on behalf of a slow reader. */
    if (nghttp2_session_consume_connection( session, len )) return NGHTTP2_ERR_CALLBACK_FAILURE;
    if (!request) return 0;
    stream = &request->http2_stream;
    if (stream->body_pos == stream->body_size) stream->body_pos = stream->body_size = 0;
    if (stream->body_pos && stream->body_size + len > stream->body_capacity)
    {
        memmove( stream->body, stream->body + stream->body_pos, stream->body_size - stream->body_pos );
        stream->body_size -= stream->body_pos;
        stream->body_pos = 0;
    }
    if (len > SIZE_MAX - stream->body_size)
    {
        stream->error = ERROR_OUTOFMEMORY;
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    if (stream->body_size + len > stream->body_capacity)
    {
        new_capacity = max( stream->body_size + len, stream->body_capacity * 2 );
        if (!(new_body = realloc( stream->body, new_capacity )))
        {
            stream->error = ERROR_OUTOFMEMORY;
            return NGHTTP2_ERR_CALLBACK_FAILURE;
        }
        stream->body = new_body;
        stream->body_capacity = new_capacity;
    }
    memcpy( stream->body + stream->body_size, data, len );
    stream->body_size += len;
    return 0;
}

static int on_stream_close( nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_data )
{
    struct request *request = nghttp2_session_get_stream_user_data( session, stream_id );
    if (request)
    {
        request->http2_stream.closed = TRUE;
        if (error_code && !request->http2_stream.error)
            request->http2_stream.error = error_code == NGHTTP2_REFUSED_STREAM ?
                                          ERROR_WINHTTP_RESEND_REQUEST : ERROR_WINHTTP_CONNECTION_ERROR;
    }
    return 0;
}

DWORD http2_init_connection( struct netconn *conn )
{
    nghttp2_session_callbacks *callbacks;
    nghttp2_option *option;
    nghttp2_session *session;
    int ret;

    if (conn->http2_session) return ERROR_SUCCESS;
    if (nghttp2_session_callbacks_new( &callbacks )) return ERROR_OUTOFMEMORY;
    nghttp2_session_callbacks_set_on_header_callback( callbacks, on_header );
    nghttp2_session_callbacks_set_on_frame_recv_callback( callbacks, on_frame_recv );
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback( callbacks, on_data_chunk_recv );
    nghttp2_session_callbacks_set_on_stream_close_callback( callbacks, on_stream_close );
    if (nghttp2_option_new( &option ))
    {
        nghttp2_session_callbacks_del( callbacks );
        return ERROR_OUTOFMEMORY;
    }
    nghttp2_option_set_no_auto_window_update( option, TRUE );
    ret = nghttp2_session_client_new2( &session, callbacks, conn, option );
    nghttp2_option_del( option );
    nghttp2_session_callbacks_del( callbacks );
    if (ret) return ERROR_OUTOFMEMORY;
    if (nghttp2_submit_settings( session, NGHTTP2_FLAG_NONE, NULL, 0 ))
    {
        nghttp2_session_del( session );
        return ERROR_OUTOFMEMORY;
    }
    conn->http2_session = session;
    InitializeCriticalSection( &conn->http2_cs );
    InitializeConditionVariable( &conn->http2_cv );
    return ERROR_SUCCESS;
}

void http2_destroy_connection( struct netconn *conn )
{
    if (conn->http2_session) nghttp2_session_del( session_for( conn ) );
    conn->http2_session = NULL;
}

static DWORD flush_session( struct netconn *conn )
{
    const uint8_t *data;
    nghttp2_ssize length;
    DWORD ret;
    int sent;

    while ((length = nghttp2_session_mem_send2( session_for( conn ), &data )) > 0)
    {
        if ((ret = netconn_send( conn, data, length, &sent, NULL ))) return ret;
        if (sent != length) return ERROR_WINHTTP_CONNECTION_ERROR;
    }
    return length < 0 ? ERROR_WINHTTP_CONNECTION_ERROR : ERROR_SUCCESS;
}

static void fail_connection( struct netconn *conn, DWORD error )
{
    InterlockedCompareExchange( &conn->http2_error, error, 0 );
    WakeAllConditionVariable( &conn->http2_cv );
}

static DWORD pump_session( struct request *request )
{
    struct netconn *conn = request->netconn;
    uint8_t buffer[16384];
    nghttp2_ssize consumed;
    DWORD ret = ERROR_SUCCESS;
    int received = 0;

    /* Called with http2_cs held.  One thread reads the TLS socket while
     * other requests may submit streams and wait for their own response. */
    if (conn->http2_error) return conn->http2_error;
    if (conn->http2_reading)
    {
        SleepConditionVariableCS( &conn->http2_cv, &conn->http2_cs, INFINITE );
        return conn->http2_error;
    }
    conn->http2_reading = TRUE;
    ret = flush_session( conn );
    if (!ret)
    {
        LeaveCriticalSection( &conn->http2_cs );
        ret = netconn_recv( conn, buffer, sizeof(buffer), 0, &received );
        EnterCriticalSection( &conn->http2_cs );
    }
    conn->http2_reading = FALSE;
    if (!ret && !received) ret = ERROR_WINHTTP_CONNECTION_ERROR;
    if (ret)
    {
        fail_connection( conn, ret );
        return ret;
    }
    request->reply_len += received;
    if ((consumed = nghttp2_session_mem_recv2( session_for( conn ), buffer, received )) != received)
        ret = request->http2_stream.error ? request->http2_stream.error : ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
    else ret = flush_session( conn );
    if (ret) fail_connection( conn, ret );
    else WakeAllConditionVariable( &conn->http2_cv );
    if (ret) return ret;
    return request->http2_stream.error;
}

void http2_abort_request( struct request *request )
{
    struct netconn *conn = request->netconn;
    struct http2_stream *stream = &request->http2_stream;
    DWORD ret;

    if (!conn || !conn->http2_session) return;
    EnterCriticalSection( &conn->http2_cs );
    if (stream->id > 0 && !stream->closed)
    {
        nghttp2_session_set_stream_user_data( session_for( conn ), stream->id, NULL );
        nghttp2_submit_rst_stream( session_for( conn ), NGHTTP2_FLAG_NONE, stream->id, NGHTTP2_CANCEL );
        if ((ret = flush_session( conn ))) fail_connection( conn, ret );
        stream->closed = TRUE;
    }
    LeaveCriticalSection( &conn->http2_cs );
}

static nghttp2_ssize upload_data( nghttp2_session *session, int32_t stream_id, uint8_t *buf,
                                  size_t length, uint32_t *flags, nghttp2_data_source *source, void *user_data )
{
    struct request *request = source->ptr;
    struct http2_stream *stream = &request->http2_stream;
    size_t available = stream->upload_size - stream->upload_pos;
    size_t count = min( length, available );

    if (count)
    {
        memcpy( buf, stream->upload + stream->upload_pos, count );
        stream->upload_pos += count;
        if (stream->upload_pos == stream->upload_size) stream->upload_pos = stream->upload_size = 0;
    }
    if ((stream->upload_complete || (request->send_total_len &&
                                     request->bytes_written == request->send_total_len)) && !stream->upload_size)
        *flags |= NGHTTP2_DATA_FLAG_EOF;
    else if (!count) return NGHTTP2_ERR_DEFERRED;
    return count;
}

static DWORD append_upload( struct request *request, const void *data, DWORD len )
{
    struct http2_stream *stream = &request->http2_stream;
    BYTE *new_upload;
    size_t capacity;

    if (!len) return ERROR_SUCCESS;
    if (stream->upload_pos && stream->upload_size + len > stream->upload_capacity)
    {
        memmove( stream->upload, stream->upload + stream->upload_pos, stream->upload_size - stream->upload_pos );
        stream->upload_size -= stream->upload_pos;
        stream->upload_pos = 0;
    }
    if (len > SIZE_MAX - stream->upload_size) return ERROR_OUTOFMEMORY;
    if (stream->upload_size + len > stream->upload_capacity)
    {
        capacity = max( stream->upload_size + len, stream->upload_capacity * 2 );
        if (!(new_upload = realloc( stream->upload, capacity ))) return ERROR_OUTOFMEMORY;
        stream->upload = new_upload;
        stream->upload_capacity = capacity;
    }
    memcpy( stream->upload + stream->upload_size, data, len );
    stream->upload_size += len;
    return ERROR_SUCCESS;
}

static BOOL skip_header( const WCHAR *name )
{
    return !wcsicmp( name, L"Connection" ) || !wcsicmp( name, L"Host" ) ||
           !wcsicmp( name, L"Transfer-Encoding" ) || !wcsicmp( name, L"Upgrade" ) ||
           !wcsicmp( name, L"Keep-Alive" ) || !wcsicmp( name, L"Proxy-Connection" ) ||
           !wcsicmp( name, L"TE" );
}

static void set_nv( nghttp2_nv *nv, const char *name, const char *value )
{
    nv->name = (uint8_t *)(ULONG_PTR)name;
    nv->value = (uint8_t *)(ULONG_PTR)value;
    nv->namelen = strlen( name );
    nv->valuelen = strlen( value );
    nv->flags = NGHTTP2_NV_FLAG_NONE;
}

DWORD http2_send_request( struct request *request, const void *optional, DWORD optional_len, DWORD *sent )
{
    struct http2_stream *stream = &request->http2_stream;
    nghttp2_data_provider2 provider = {{0}, upload_data};
    nghttp2_nv *headers;
    char *host = NULL, *path = NULL, *method = NULL;
    size_t i, count = 0;
    DWORD path_len, ret = ERROR_OUTOFMEMORY;
    BOOL has_upload;
    int32_t id;

    *sent = 0;
    memset( stream, 0, sizeof(*stream) );
    stream->data_stream.vtbl = &http2_stream_vtbl;
    request->data_stream = &stream->data_stream;
    if (!(headers = calloc( request->num_headers + 4, sizeof(*headers) ))) return ret;
    if (!(method = header_to_utf8( request->verb )) || !(path = build_wire_path( request, &path_len, FALSE ))) goto done;
    has_upload = !!request->send_total_len || !wcsicmp( request->verb, L"POST" ) ||
                 !wcsicmp( request->verb, L"PUT" ) || !wcsicmp( request->verb, L"PATCH" );
    for (i = 0; i < request->num_headers; i++)
    {
        if (request->headers[i].is_request && !wcsicmp( request->headers[i].field, L"Transfer-Encoding" ))
            has_upload = TRUE;
    }
    for (i = 0; i < request->num_headers; i++)
        if (request->headers[i].is_request && !wcsicmp( request->headers[i].field, L"Host" ))
        {
            free( host );
            host = header_to_utf8( request->headers[i].value );
        }
    if (!host) host = header_to_utf8( request->connect->hostname );
    if (!host) goto done;
    set_nv( &headers[count++], ":method", method );
    set_nv( &headers[count++], ":scheme", "https" );
    set_nv( &headers[count++], ":authority", host );
    set_nv( &headers[count++], ":path", path );
    for (i = 0; i < request->num_headers; i++)
    {
        char *name, *value;
        if (!request->headers[i].is_request || skip_header( request->headers[i].field )) continue;
        if (!(name = header_to_utf8( request->headers[i].field )) ||
            !(value = header_to_utf8( request->headers[i].value )))
        {
            free( name );
            goto done;
        }
        set_nv( &headers[count++], name, value );
    }
    if (optional_len > request->send_total_len && request->send_total_len) { ret = ERROR_INVALID_PARAMETER; goto done; }
    if ((ret = append_upload( request, optional, optional_len ))) goto done;
    request->bytes_written = optional_len;
    provider.source.ptr = request;
    stream->has_upload = has_upload;
    EnterCriticalSection( &request->netconn->http2_cs );
    if (request->netconn->http2_draining)
    {
        ret = ERROR_WINHTTP_RESEND_REQUEST;
        LeaveCriticalSection( &request->netconn->http2_cs );
        goto done;
    }
    if (request->netconn->http2_error)
    {
        ret = request->netconn->http2_error;
        LeaveCriticalSection( &request->netconn->http2_cs );
        goto done;
    }
    id = nghttp2_submit_request2( session_for( request->netconn ), NULL, headers, count,
                                   has_upload ? &provider : NULL, request );
    if (id < 0)
    {
        if (id == NGHTTP2_ERR_START_STREAM_NOT_ALLOWED || id == NGHTTP2_ERR_SESSION_CLOSING)
        {
            InterlockedExchange( &request->netconn->http2_draining, TRUE );
            ret = ERROR_WINHTTP_RESEND_REQUEST;
        }
        else ret = id == NGHTTP2_ERR_NOMEM ? ERROR_OUTOFMEMORY : ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
        LeaveCriticalSection( &request->netconn->http2_cs );
        goto done;
    }
    stream->id = id;
    ret = flush_session( request->netconn );
    if (ret) fail_connection( request->netconn, ret );
    LeaveCriticalSection( &request->netconn->http2_cs );
    if (!ret) *sent = optional_len;
    TRACE( "HTTP/2 stream %d started\n", id );

done:
    if (ret && !stream->id) stream->closed = TRUE;
    for (i = 4; i < count; i++)
    {
        free( headers[i].name );
        free( headers[i].value );
    }
    free( headers );
    free( host );
    free( path );
    free( method );
    return ret;
}

DWORD http2_write_data( struct request *request, const void *data, DWORD len, DWORD *written )
{
    struct netconn *conn = request->netconn;
    DWORD ret;
    int resumed;
    *written = 0;
    EnterCriticalSection( &conn->http2_cs );
    if (conn->http2_error) ret = conn->http2_error;
    else if (!request->http2_stream.has_upload) ret = ERROR_INVALID_PARAMETER;
    else ret = append_upload( request, data, len );
    if (ret) goto done;
    request->bytes_written += len;
    resumed = nghttp2_session_resume_data( session_for( conn ), request->http2_stream.id );
    if (resumed < 0 && (resumed != NGHTTP2_ERR_INVALID_ARGUMENT ||
        nghttp2_session_get_stream_user_data( session_for( conn ), request->http2_stream.id ) != request))
        ret = ERROR_WINHTTP_CONNECTION_ERROR;
    else ret = flush_session( conn );
    if (ret) fail_connection( conn, ret );
    else *written = len;
done:
    LeaveCriticalSection( &conn->http2_cs );
    return ret;
}

static DWORD make_raw_headers( struct request *request )
{
    WCHAR *raw, *ptr;
    const WCHAR *status = NULL;
    size_t i, length = 14;

    for (i = 0; i < request->num_headers; i++)
    {
        struct header *header = &request->headers[i];
        if (header->is_request) continue;
        if (!wcsicmp( header->field, L"Status" )) status = header->value;
        else length += wcslen( header->field ) + wcslen( header->value ) + 4;
    }
    if (!status) return ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
    if (!(raw = malloc( (length + 1) * sizeof(*raw) ))) return ERROR_OUTOFMEMORY;
    ptr = raw;
    memcpy( ptr, L"HTTP/2 ", 7 * sizeof(*ptr) ); ptr += 7;
    memcpy( ptr, status, 3 * sizeof(*ptr) ); ptr += 3;
    memcpy( ptr, L"\r\n", 2 * sizeof(*ptr) ); ptr += 2;
    for (i = 0; i < request->num_headers; i++)
    {
        struct header *header = &request->headers[i];
        size_t len;
        if (header->is_request || !wcsicmp( header->field, L"Status" )) continue;
        len = wcslen( header->field ); memcpy( ptr, header->field, len * sizeof(*ptr) ); ptr += len;
        memcpy( ptr, L": ", 2 * sizeof(*ptr) ); ptr += 2;
        len = wcslen( header->value ); memcpy( ptr, header->value, len * sizeof(*ptr) ); ptr += len;
        memcpy( ptr, L"\r\n", 2 * sizeof(*ptr) ); ptr += 2;
    }
    memcpy( ptr, L"\r\n", 3 * sizeof(*ptr) );
    free( request->raw_headers ); request->raw_headers = raw;
    free( request->version ); request->version = wcsdup( L"HTTP/2" );
    free( request->status_text ); request->status_text = wcsdup( L"" );
    return request->version && request->status_text ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

static DWORD http2_fill_buffer( struct data_stream *base, struct request *request, struct read_buffer *buf )
{
    struct http2_stream *stream = &request->http2_stream;
    struct netconn *conn = request->netconn;
    DWORD ret = ERROR_SUCCESS;
    size_t count;

    if (buf->pos)
    {
        if (buf->size) memmove( buf->buf, buf->buf + buf->pos, buf->size );
        buf->pos = 0;
    }
    if (buf->size == sizeof(buf->buf)) return ERROR_SUCCESS;
    if (!conn) return ERROR_SUCCESS;
    EnterCriticalSection( &conn->http2_cs );
    while (stream->body_pos == stream->body_size && !stream->closed)
        if ((ret = pump_session( request ))) goto done;
    if (stream->error) { ret = stream->error; goto done; }
    count = min( sizeof(buf->buf) - buf->size, stream->body_size - stream->body_pos );
    if (count)
    {
        memcpy( buf->buf + buf->size, stream->body + stream->body_pos, count );
        buf->size += count;
        stream->body_pos += count;
        if (stream->body_pos == stream->body_size) stream->body_pos = stream->body_size = 0;
        if (!stream->closed && nghttp2_session_consume_stream( session_for( conn ), stream->id, count ))
            ret = ERROR_WINHTTP_CONNECTION_ERROR;
        else if (!stream->closed) ret = flush_session( conn );
        if (ret) fail_connection( conn, ret );
    }
done:
    LeaveCriticalSection( &conn->http2_cs );
    return ret;
}

static BOOL http2_end_of_data( struct data_stream *base, struct request *request )
{
    struct http2_stream *stream = &request->http2_stream;
    BOOL result;
    if (!request->netconn) return stream->closed && stream->body_pos == stream->body_size;
    EnterCriticalSection( &request->netconn->http2_cs );
    result = stream->closed && stream->body_pos == stream->body_size;
    LeaveCriticalSection( &request->netconn->http2_cs );
    return result;
}

static DWORD http2_drain_data( struct data_stream *base, struct request *request )
{
    DWORD ret;
    while (!http2_end_of_data( base, request ))
    {
        if ((ret = http2_fill_buffer( base, request, &request->read ))) return ret;
        request->content_read += request->read.size;
        request->read.pos = request->read.size = 0;
    }
    return ERROR_SUCCESS;
}

static void http2_destroy_stream( struct data_stream *base )
{
    struct http2_stream *stream = (struct http2_stream *)base;
    free( stream->body );
    free( stream->upload );
    stream->body = stream->upload = NULL;
    stream->body_pos = stream->body_size = stream->body_capacity = 0;
    stream->upload_pos = stream->upload_size = stream->upload_capacity = 0;
}

static const struct data_stream_vtbl http2_stream_vtbl =
{
    http2_fill_buffer, http2_end_of_data, http2_drain_data, http2_destroy_stream
};

DWORD http2_read_reply( struct request *request )
{
    struct http2_stream *stream = &request->http2_stream;
    struct netconn *conn = request->netconn;
    DWORD ret = ERROR_SUCCESS;

    EnterCriticalSection( &conn->http2_cs );
    if (request->send_total_len && request->bytes_written < request->send_total_len)
    {
        ret = ERROR_WINHTTP_INCORRECT_HANDLE_STATE;
        goto done;
    }
    if (stream->has_upload && !request->send_total_len && !stream->upload_complete)
    {
        stream->upload_complete = TRUE;
        nghttp2_session_resume_data( session_for( request->netconn ), stream->id );
        if ((ret = flush_session( conn ))) goto done;
    }

    while (!stream->headers_received && !stream->closed)
        if ((ret = pump_session( request ))) goto done;
    if (stream->error) ret = stream->error;
    else if (!stream->headers_received) ret = ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
    else ret = make_raw_headers( request );
done:
    if (ret == ERROR_WINHTTP_CONNECTION_ERROR) fail_connection( conn, ret );
    LeaveCriticalSection( &conn->http2_cs );
    return ret;
}
