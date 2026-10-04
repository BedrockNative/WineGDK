/*
 * System Windows Notification Facility states
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

#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include "ntstatus.h"
#include "windef.h"
#include "winternl.h"
#include "handle.h"
#include "thread.h"
#include "request.h"
#include "user.h"

#define WNF_SHEL_FOCUS_CHANGE ((unsigned __int64)0x0d83063ea3bc7875)

static struct list subscriptions = LIST_INIT(subscriptions);
static unsigned int focus_pid, focus_stamp;

struct wnf_subscription
{
    struct object obj;
    struct list entry;
    struct object *sync;
};

static void subscription_dump( struct object *obj, int verbose )
{
    fprintf( stderr, "WNF focus subscription\n" );
}

static struct object *subscription_get_sync( struct object *obj )
{
    struct wnf_subscription *subscription = (struct wnf_subscription *)obj;
    return grab_object( subscription->sync );
}

static void subscription_destroy( struct object *obj )
{
    struct wnf_subscription *subscription = (struct wnf_subscription *)obj;
    list_remove( &subscription->entry );
    if (subscription->sync) release_object( subscription->sync );
}

static const struct object_ops subscription_ops =
{
    .size = sizeof(struct wnf_subscription),
    .type = &no_type,
    .dump = subscription_dump,
    .get_sync = subscription_get_sync,
    .destroy = subscription_destroy,
};

void wnf_set_foreground_process( process_id_t pid )
{
    struct wnf_subscription *subscription;

    if (focus_pid == pid) return;
    focus_pid = pid;
    if (!++focus_stamp) ++focus_stamp;
    LIST_FOR_EACH_ENTRY( subscription, &subscriptions, struct wnf_subscription, entry )
        signal_sync( subscription->sync );
}

DECL_HANDLER(query_wnf_state)
{
    if (req->name != WNF_SHEL_FOCUS_CHANGE)
    {
        set_error( STATUS_OBJECT_NAME_NOT_FOUND );
        return;
    }
    reply->stamp = focus_stamp;
    reply->value = focus_pid;
}

DECL_HANDLER(subscribe_wnf_state)
{
    struct wnf_subscription *subscription;

    if (req->name != WNF_SHEL_FOCUS_CHANGE)
    {
        set_error( STATUS_OBJECT_NAME_NOT_FOUND );
        return;
    }
    if (!(subscription = alloc_object( &subscription_ops ))) return;
    list_init( &subscription->entry );
    if ((subscription->sync = create_internal_sync( 0, req->stamp != focus_stamp )))
    {
        list_add_tail( &subscriptions, &subscription->entry );
        reply->handle = alloc_handle( current->process, subscription, SYNCHRONIZE, 0 );
    }
    release_object( subscription );
}
