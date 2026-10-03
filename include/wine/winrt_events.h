/* Internal WinRT event subscriptions
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

#ifndef __WINE_WINRT_EVENTS_H
#define __WINE_WINRT_EVENTS_H

struct winrt_event_entry
{
    struct winrt_event_entry *next;
    IUnknown *handler;
    EventRegistrationToken token;
};

struct winrt_event
{
    SRWLOCK lock;
    struct winrt_event_entry *head;
    INT64 next_token;
};

static inline HRESULT winrt_event_add( struct winrt_event *event, void *handler, EventRegistrationToken *token )
{
    struct winrt_event_entry *entry, **tail;
    if (!handler || !token) return E_POINTER;
    token->value = 0;
    if (!(entry = malloc( sizeof(*entry) ))) return E_OUTOFMEMORY;
    entry->handler = handler;
    entry->next = NULL;
    IUnknown_AddRef( entry->handler );
    AcquireSRWLockExclusive( &event->lock );
    entry->token.value = ++event->next_token;
    for (tail = &event->head; *tail; tail = &(*tail)->next) {}
    *tail = entry;
    *token = entry->token;
    ReleaseSRWLockExclusive( &event->lock );
    return S_OK;
}

static inline HRESULT winrt_event_remove( struct winrt_event *event, EventRegistrationToken token )
{
    struct winrt_event_entry *entry = NULL, **cursor;
    AcquireSRWLockExclusive( &event->lock );
    for (cursor = &event->head; *cursor; cursor = &(*cursor)->next)
        if ((*cursor)->token.value == token.value)
        {
            entry = *cursor;
            *cursor = entry->next;
            break;
        }
    ReleaseSRWLockExclusive( &event->lock );
    if (entry)
    {
        IUnknown_Release( entry->handler );
        free( entry );
    }
    return S_OK;
}

static inline void winrt_event_clear( struct winrt_event *event )
{
    struct winrt_event_entry *entry, *next;
    AcquireSRWLockExclusive( &event->lock );
    entry = event->head;
    event->head = NULL;
    ReleaseSRWLockExclusive( &event->lock );
    for (; entry; entry = next)
    {
        next = entry->next;
        IUnknown_Release( entry->handler );
        free( entry );
    }
}

/* EventHandler<T> and TypedEventHandler<T,U> share this delegate ABI.
 * Snapshot subscriptions so handlers can remove themselves during Invoke. */
static inline HRESULT winrt_event_notify( struct winrt_event *event, void *sender, void *args )
{
    ITypedEventHandler_IInspectable_IInspectable **handlers;
    struct winrt_event_entry *entry;
    unsigned int count = 0, i = 0;
    HRESULT hr = S_OK, tmp;
    AcquireSRWLockShared( &event->lock );
    for (entry = event->head; entry; entry = entry->next) ++count;
    if (!count) { ReleaseSRWLockShared( &event->lock ); return S_OK; }
    if (!(handlers = malloc( count * sizeof(*handlers) )))
    {
        ReleaseSRWLockShared( &event->lock );
        return E_OUTOFMEMORY;
    }
    for (entry = event->head; entry; entry = entry->next)
    {
        handlers[i] = (void *)entry->handler;
        IUnknown_AddRef( entry->handler );
        ++i;
    }
    ReleaseSRWLockShared( &event->lock );
    for (i = 0; i < count; ++i)
    {
        tmp = ITypedEventHandler_IInspectable_IInspectable_Invoke( handlers[i], sender, args );
        if (SUCCEEDED(hr)) hr = tmp;
        ITypedEventHandler_IInspectable_IInspectable_Release( handlers[i] );
    }
    free( handlers );
    return hr;
}
#endif
