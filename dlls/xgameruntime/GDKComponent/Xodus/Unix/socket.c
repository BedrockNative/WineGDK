/*
 * Xbox Game runtime Library
 *  Xodus Interopability Layer -> Unix module -> Socket
 * 
 * Written by Weather
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

#ifndef WINE_UNIX_LIB
#define WINE_UNIX_LIB
#endif

#include "ntstatus.h"
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "initguid.h"

#include "wine/unixlib.h"
#include "wine/list.h"
#include "wine/unixlib.h"
#include "wine/debug.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_NET_IF_H
# define if_indextoname unix_if_indextoname
# define if_nametoindex unix_if_nametoindex
# include <net/if.h>
# undef if_indextoname
# undef if_nametoindex
#endif
#ifdef HAVE_IFADDRS_H
# include <ifaddrs.h>
#endif
#include <poll.h>

#ifdef HAVE_NETIPX_IPX_H
# include <netipx/ipx.h>
# define HAS_IPX
#elif defined(HAVE_LINUX_IPX_H)
# ifdef HAVE_ASM_TYPES_H
#  include <asm/types.h>
# endif
# ifdef HAVE_LINUX_TYPES_H
#  include <linux/types.h>
# endif
# include <linux/ipx.h>
# ifdef SOL_IPX
#  define HAS_IPX
# endif
#endif

#ifdef HAVE_LINUX_IRDA_H
# ifdef HAVE_LINUX_TYPES_H
#  include <linux/types.h>
# endif
# include <linux/irda.h>
# define HAS_IRDA
#endif

#define POLL_BUFFER_SIZE 0x10008 /* UINT16 payload plus IPC header */

WINE_DEFAULT_DEBUG_CHANNEL(xodus);

// Persist connection
static int sockfd = -1;

typedef struct _POLL_SOCKET_ARGS
{
    BYTE curr_buffer[POLL_BUFFER_SIZE];
    SIZE_T curr_buffer_size;
} POLL_SOCKET_ARGS;

typedef struct _IPCFrame
{
    UINT32 frameSize;
    BYTE* frame;
} IPCFrame;

static NTSTATUS conn_sock( void *args )
{
    struct sockaddr_un addr;
    LPCSTR socket_suffix = args;
    char *socket_path;
    size_t len;
    int error;

#ifdef __linux__
    const char *runtime = getenv( "XDG_RUNTIME_DIR" );
    if (!runtime || !*runtime) return STATUS_OBJECT_PATH_NOT_FOUND;
#else
    const char *runtime = "/tmp";
#endif

    if (!socket_suffix) return STATUS_INVALID_PARAMETER;
    len = strlen( runtime ) + strlen( socket_suffix ) + 2;
    if (len > sizeof(addr.sun_path)) return STATUS_NAME_TOO_LONG;
    if (sockfd >= 0) return STATUS_SUCCESS;
    if (!(socket_path = malloc( len ))) return STATUS_NO_MEMORY;
    memcpy( socket_path, runtime, strlen( runtime ) );
    socket_path[strlen( runtime )] = '/';
    memcpy( socket_path + strlen( runtime ) + 1, socket_suffix, strlen( socket_suffix ) + 1 );

    sockfd = socket( AF_UNIX, SOCK_STREAM, 0 );
    if (sockfd < 0)
    {
        free( socket_path );
        return STATUS_UNSUCCESSFUL;
    }

    memset( &addr, 0, sizeof(addr) );
    addr.sun_family = AF_UNIX;
    memcpy( addr.sun_path, socket_path, len );

    if (connect( sockfd, (struct sockaddr *)&addr, sizeof(addr) ) < 0)
    {
        error = errno;
        WARN( "Failed to connect to Xodus socket %s: %s.\n", socket_path, strerror( error ) );
        close( sockfd );
        sockfd = -1;
        free( socket_path );
        return STATUS_CONNECTION_REFUSED;
    }

#ifdef SO_NOSIGPIPE
    {
        int enabled = 1;
        setsockopt( sockfd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled) );
    }
#endif
    free( socket_path );
    return STATUS_SUCCESS;
}

// MUST BE CALLED IN AN ASYNCHRONOUS CONTEXT!
static NTSTATUS poll_sock( void *args )
{
    POLL_SOCKET_ARGS *socket_args = (POLL_SOCKET_ARGS *)args;
    struct pollfd fds[1];
    int ret;
    ssize_t n;

    TRACE( "args %p\n", args );

    if (sockfd < 0) return STATUS_CONNECTION_INVALID;
    if (socket_args->curr_buffer_size >= POLL_BUFFER_SIZE) return STATUS_BUFFER_TOO_SMALL;

    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    do {
        ret = poll( fds, 1, -1 );
    } while ( ret < 0 && errno == EINTR );

    if ( ret < 0 )
        return STATUS_CONNECTION_DISCONNECTED;

    if ( fds[0].revents & POLLIN ) 
    {
        do
            n = read( sockfd, socket_args->curr_buffer + socket_args->curr_buffer_size,
                      POLL_BUFFER_SIZE - socket_args->curr_buffer_size );
        while (n < 0 && errno == EINTR);
        if ( n <= 0 ) 
            return STATUS_CONNECTION_DISCONNECTED;

        socket_args->curr_buffer_size += n;
    }
    else if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
        return STATUS_CONNECTION_DISCONNECTED;

    return STATUS_SUCCESS;
}

static NTSTATUS send_frm( void *args )
{
    IPCFrame *frame = (IPCFrame *)args;
    ssize_t sent = 0;

    TRACE( "args %p\n", args );

    if (sockfd < 0) return STATUS_CONNECTION_INVALID;
    if (!frame || !frame->frame || frame->frameSize < 8 || frame->frameSize > 0xffff + 8)
        return STATUS_INVALID_PARAMETER;

    while ( sent < frame->frameSize )
    {
        ssize_t n;
#ifdef MSG_NOSIGNAL
        n = send( sockfd, (char *)frame->frame + sent, frame->frameSize - sent, MSG_NOSIGNAL );
#else
        n = send( sockfd, (char *)frame->frame + sent, frame->frameSize - sent, 0 );
#endif

        if ( n < 0 )
        {
            if ( errno == EINTR )
                continue;
            return STATUS_CONNECTION_RESET;
        }

        if (!n) return STATUS_CONNECTION_RESET;
        sent += n;
    }

    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    conn_sock,
    poll_sock,
    send_frm
};