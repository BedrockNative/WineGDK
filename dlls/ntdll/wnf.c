/*
 * Windows Notification Facility system-state subscriptions
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
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "ntstatus.h"
#include "windef.h"
#include "winternl.h"
#include "wine/debug.h"
#include "wine/list.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(wnf);

static RTL_SRWLOCK subscriptions_lock = RTL_SRWLOCK_INIT;
static struct list subscriptions = LIST_INIT(subscriptions);

struct subscription
{
    struct list entry;
    LONG refs;
    WNF_STATE_NAME name;
    WNF_CHANGE_STAMP stamp;
    PWNF_USER_CALLBACK callback;
    void *context;
    HANDLE event, callback_thread;
    TP_WAIT *wait;
    BOOL cancelled;
};

static void release_subscription( struct subscription *subscription )
{
    if (InterlockedDecrement( &subscription->refs )) return;
    NtClose( subscription->event );
    RtlFreeHeap( GetProcessHeap(), 0, subscription );
}

NTSTATUS WINAPI NtQueryWnfStateData( const WNF_STATE_NAME *name, const WNF_TYPE_ID *type,
                                   const void *scope, WNF_CHANGE_STAMP *stamp, void *buffer, ULONG *size )
{
    NTSTATUS status;
    ULONG value, new_stamp;

    TRACE( "%p %p %p %p %p %p\n", name, type, scope, stamp, buffer, size );
    if (!name || !stamp || !size) return STATUS_INVALID_PARAMETER;
    if (type || scope) return STATUS_NOT_SUPPORTED;

    SERVER_START_REQ( query_wnf_state )
    {
        req->name = *name;
        status = wine_server_call( req );
        value = reply->value;
        new_stamp = reply->stamp;
    }
    SERVER_END_REQ;
    if (status) return status;

    *stamp = new_stamp;
    if (*size < sizeof(value))
    {
        *size = sizeof(value);
        return STATUS_BUFFER_TOO_SMALL;
    }
    if (!buffer) return STATUS_INVALID_PARAMETER;
    memcpy( buffer, &value, sizeof(value) );
    *size = sizeof(value);
    return STATUS_SUCCESS;
}

static void CALLBACK state_changed( TP_CALLBACK_INSTANCE *instance, void *context, TP_WAIT *wait,
                                    TP_WAIT_RESULT result )
{
    struct subscription *subscription = context;
    LARGE_INTEGER retry_delay = { .QuadPart = -1000000 };
    WNF_CHANGE_STAMP stamp;
    ULONG value, size = sizeof(value);
    NTSTATUS status;
    BOOL retry = FALSE, deliver = FALSE;

    RtlAcquireSRWLockExclusive( &subscriptions_lock );
    InterlockedIncrement( &subscription->refs );
    subscription->callback_thread = NtCurrentTeb()->ClientId.UniqueThread;
    if (!subscription->cancelled &&
        !(status = NtQueryWnfStateData( &subscription->name, NULL, NULL, &stamp, &value, &size )) &&
        stamp != subscription->stamp)
        deliver = TRUE;
    RtlReleaseSRWLockExclusive( &subscriptions_lock );

    if (deliver)
    {
        status = subscription->callback( subscription->name, stamp, NULL, subscription->context,
                                         &value, sizeof(value) );
        retry = status == STATUS_RETRY || status == STATUS_NO_MEMORY;
    }

    RtlAcquireSRWLockExclusive( &subscriptions_lock );
    subscription->callback_thread = NULL;
    if (deliver && !retry) subscription->stamp = stamp;
    if (!subscription->cancelled)
        TpSetWait( wait, subscription->event, retry ? &retry_delay : NULL );
    RtlReleaseSRWLockExclusive( &subscriptions_lock );
    release_subscription( subscription );
}

NTSTATUS WINAPI RtlSubscribeWnfStateChangeNotification( void **out, WNF_STATE_NAME name,
                                                       WNF_CHANGE_STAMP stamp, PWNF_USER_CALLBACK callback,
                                                       void *context, const WNF_TYPE_ID *type,
                                                       ULONG group, ULONG flags )
{
    struct subscription *subscription;
    NTSTATUS status;

    TRACE( "%p %s %lu %p %p %p %lu %#lx\n", out, wine_dbgstr_longlong(name), stamp,
           callback, context, type, group, flags );
    if (!out) return STATUS_INVALID_PARAMETER;
    *out = NULL;
    if (!callback) return STATUS_INVALID_PARAMETER;
    if (type || group || flags) return STATUS_NOT_SUPPORTED;
    if (!(subscription = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*subscription) )))
        return STATUS_NO_MEMORY;
    subscription->refs = 1;
    subscription->name = name;
    subscription->stamp = stamp;
    subscription->callback = callback;
    subscription->context = context;

    SERVER_START_REQ( subscribe_wnf_state )
    {
        req->name = name;
        req->stamp = stamp;
        status = wine_server_call( req );
        subscription->event = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    if (status)
    {
        RtlFreeHeap( GetProcessHeap(), 0, subscription );
        return status;
    }
    if ((status = TpAllocWait( &subscription->wait, state_changed, subscription, NULL )))
    {
        release_subscription( subscription );
        return status;
    }

    RtlAcquireSRWLockExclusive( &subscriptions_lock );
    list_add_tail( &subscriptions, &subscription->entry );
    *out = subscription;
    TpSetWait( subscription->wait, subscription->event, NULL );
    RtlReleaseSRWLockExclusive( &subscriptions_lock );
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI RtlUnsubscribeWnfStateChangeNotification( void *handle )
{
    struct subscription *subscription, *found = NULL;
    BOOL self;

    TRACE( "%p\n", handle );
    RtlAcquireSRWLockExclusive( &subscriptions_lock );
    LIST_FOR_EACH_ENTRY( subscription, &subscriptions, struct subscription, entry )
    {
        if (subscription != handle) continue;
        found = subscription;
        break;
    }
    if (!found)
    {
        RtlReleaseSRWLockExclusive( &subscriptions_lock );
        return STATUS_INVALID_HANDLE;
    }
    list_remove( &subscription->entry );
    subscription->cancelled = TRUE;
    self = subscription->callback_thread == NtCurrentTeb()->ClientId.UniqueThread;
    TpSetWait( subscription->wait, NULL, NULL );
    RtlReleaseSRWLockExclusive( &subscriptions_lock );

    if (!self) TpWaitForWait( subscription->wait, TRUE );
    TpReleaseWait( subscription->wait );
    release_subscription( subscription );
    return STATUS_SUCCESS;
}
