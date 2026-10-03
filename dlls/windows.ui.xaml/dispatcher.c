/* UI-thread dispatched actions. LGPL-2.1-or-later. */
#include "private.h"
#include "winuser.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(xaml);

struct dispatched_action
{
    IAsyncAction IAsyncAction_iface;
    IAsyncInfo IAsyncInfo_iface;
    LONG ref;
    SRWLOCK lock;
    AsyncStatus status;
    HRESULT result;
    BOOL closed, assigned, running;
    UINT32 id;
    IDispatchedHandler *callback;
    IAsyncActionCompletedHandler *completed;
    CoreDispatcherPriority priority;
    struct dispatched_action *next;
};
static LONG next_id;
static struct dispatched_action *impl_from_IAsyncAction(IAsyncAction *iface)
{ return CONTAINING_RECORD(iface,struct dispatched_action,IAsyncAction_iface); }
static HRESULT WINAPI action_QueryInterface(IAsyncAction *iface, REFIID iid, void **out)
{
    struct dispatched_action *impl=impl_from_IAsyncAction(iface);
    if (!out) return E_POINTER;
    *out=NULL;
    if (IsEqualGUID(iid,&IID_IAsyncInfo)) *out=&impl->IAsyncInfo_iface;
    else if (IsEqualGUID(iid,&IID_IUnknown) || IsEqualGUID(iid,&IID_IInspectable) || IsEqualGUID(iid,&IID_IAgileObject) || IsEqualGUID(iid,&IID_IAsyncAction)) *out=iface;
    else return E_NOINTERFACE;
    IAsyncAction_AddRef(iface); return S_OK;
}
static ULONG WINAPI action_AddRef(IAsyncAction *iface)
{ return InterlockedIncrement(&impl_from_IAsyncAction(iface)->ref); }
static ULONG WINAPI action_Release(IAsyncAction *iface)
{
    struct dispatched_action *impl=impl_from_IAsyncAction(iface);
    ULONG ref=InterlockedDecrement(&impl->ref);
    if (!ref)
    {
        if (impl->callback) IDispatchedHandler_Release(impl->callback);
        if (impl->completed) IAsyncActionCompletedHandler_Release(impl->completed);
        free(impl);
    }
    return ref;
}
static HRESULT WINAPI action_GetIids(IAsyncAction *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count=0;
    if (!(*iids=CoTaskMemAlloc(2*sizeof(**iids)))) return E_OUTOFMEMORY;
    (*iids)[0]=IID_IAsyncAction; (*iids)[1]=IID_IAsyncInfo; *count=2; return S_OK;
}
static HRESULT WINAPI action_GetRuntimeClassName(IAsyncAction *iface, HSTRING *name)
{ const WCHAR *str=L"Windows.Foundation.IAsyncAction"; return WindowsCreateString(str,wcslen(str),name); }
static HRESULT WINAPI action_GetTrustLevel(IAsyncAction *iface, TrustLevel *trust)
{ if (!trust) return E_POINTER; *trust=BaseTrust; return S_OK; }
static HRESULT WINAPI action_put_Completed(IAsyncAction *iface, IAsyncActionCompletedHandler *handler)
{
    struct dispatched_action *impl=impl_from_IAsyncAction(iface);
    AsyncStatus status;
    HRESULT hr=S_OK;
    if (handler) IAsyncActionCompletedHandler_AddRef(handler);
    AcquireSRWLockExclusive(&impl->lock);
    if (impl->closed) hr=RO_E_CLOSED;
    else if (impl->assigned) hr=E_ILLEGAL_DELEGATE_ASSIGNMENT;
    else { impl->assigned=TRUE; impl->completed=handler; }
    status=impl->status;
    if (SUCCEEDED(hr) && handler && status!=Started) IAsyncActionCompletedHandler_AddRef(handler);
    ReleaseSRWLockExclusive(&impl->lock);
    if (FAILED(hr)) { if (handler) IAsyncActionCompletedHandler_Release(handler); return hr; }
    if (handler && status!=Started)
    {
        IAsyncAction_AddRef(iface);
        IAsyncActionCompletedHandler_Invoke(handler,iface,status);
        IAsyncActionCompletedHandler_Release(handler);
        IAsyncAction_Release(iface);
    }
    return S_OK;
}
static HRESULT WINAPI action_get_Completed(IAsyncAction *iface, IAsyncActionCompletedHandler **handler)
{
    struct dispatched_action *impl=impl_from_IAsyncAction(iface);
    HRESULT hr=S_OK;
    if (!handler) return E_POINTER;
    *handler=NULL;
    AcquireSRWLockShared(&impl->lock);
    if (impl->closed) hr=RO_E_CLOSED;
    else if ((*handler=impl->completed)) IAsyncActionCompletedHandler_AddRef(*handler);
    ReleaseSRWLockShared(&impl->lock); return hr;
}
static HRESULT WINAPI action_GetResults(IAsyncAction *iface)
{
    struct dispatched_action *impl=impl_from_IAsyncAction(iface);
    HRESULT hr;
    AcquireSRWLockShared(&impl->lock);
    hr=impl->closed ? RO_E_CLOSED : impl->status==Started ? E_ILLEGAL_METHOD_CALL : impl->result;
    ReleaseSRWLockShared(&impl->lock); return hr;
}
static const IAsyncActionVtbl action_vtbl={action_QueryInterface,action_AddRef,action_Release,action_GetIids,
    action_GetRuntimeClassName,action_GetTrustLevel,action_put_Completed,action_get_Completed,action_GetResults};
DEFINE_IINSPECTABLE(info,IAsyncInfo,struct dispatched_action,IAsyncAction_iface)
static HRESULT WINAPI info_get_Id(IAsyncInfo *iface, UINT32 *id)
{
    struct dispatched_action *impl=impl_from_IAsyncInfo(iface);
    HRESULT hr=S_OK;
    if (!id) return E_POINTER;
    AcquireSRWLockShared(&impl->lock);
    if (impl->closed) hr=RO_E_CLOSED; else *id=impl->id;
    ReleaseSRWLockShared(&impl->lock); return hr;
}
static HRESULT WINAPI info_get_Status(IAsyncInfo *iface, AsyncStatus *status)
{
    struct dispatched_action *impl=impl_from_IAsyncInfo(iface);
    HRESULT hr=S_OK;
    if (!status) return E_POINTER;
    AcquireSRWLockShared(&impl->lock);
    if (impl->closed) hr=RO_E_CLOSED; else *status=impl->status;
    ReleaseSRWLockShared(&impl->lock); return hr;
}
static HRESULT WINAPI info_get_ErrorCode(IAsyncInfo *iface, HRESULT *error)
{
    struct dispatched_action *impl=impl_from_IAsyncInfo(iface);
    HRESULT hr=S_OK;
    if (!error) return E_POINTER;
    AcquireSRWLockShared(&impl->lock);
    if (impl->closed) hr=RO_E_CLOSED; else *error=impl->result;
    ReleaseSRWLockShared(&impl->lock); return hr;
}
static void action_finish(struct dispatched_action *impl, AsyncStatus status, HRESULT result)
{
    IAsyncActionCompletedHandler *handler=NULL;
    AcquireSRWLockExclusive(&impl->lock);
    if (impl->status==Started)
    {
        impl->status=status; impl->result=result;
        if ((handler=impl->completed)) IAsyncActionCompletedHandler_AddRef(handler);
    }
    ReleaseSRWLockExclusive(&impl->lock);
    if (handler)
    {
        IAsyncAction_AddRef(&impl->IAsyncAction_iface);
        IAsyncActionCompletedHandler_Invoke(handler,&impl->IAsyncAction_iface,status);
        IAsyncActionCompletedHandler_Release(handler);
        IAsyncAction_Release(&impl->IAsyncAction_iface);
    }
}
static HRESULT WINAPI info_Cancel(IAsyncInfo *iface)
{
    struct dispatched_action *impl=impl_from_IAsyncInfo(iface);
    BOOL closed;
    AcquireSRWLockShared(&impl->lock); closed=impl->closed; ReleaseSRWLockShared(&impl->lock);
    if (closed) return RO_E_CLOSED;
    action_finish(impl,Canceled,HRESULT_FROM_WIN32(ERROR_CANCELLED)); return S_OK;
}
static HRESULT WINAPI info_Close(IAsyncInfo *iface)
{
    struct dispatched_action *impl=impl_from_IAsyncInfo(iface);
    IAsyncActionCompletedHandler *handler=NULL;
    HRESULT hr=S_OK;
    AcquireSRWLockExclusive(&impl->lock);
    if (impl->status==Started) hr=E_ILLEGAL_STATE_CHANGE;
    else { impl->closed=TRUE; handler=impl->completed; impl->completed=NULL; }
    ReleaseSRWLockExclusive(&impl->lock);
    if (handler) IAsyncActionCompletedHandler_Release(handler);
    return hr;
}
static const IAsyncInfoVtbl info_vtbl={info_QueryInterface,info_AddRef,info_Release,info_GetIids,
    info_GetRuntimeClassName,info_GetTrustLevel,info_get_Id,info_get_Status,info_get_ErrorCode,info_Cancel,info_Close};
HRESULT dispatcher_queue_add(struct dispatcher_queue *queue, CoreDispatcherPriority priority, IDispatchedHandler *callback, IAsyncAction **out)
{
    struct dispatched_action *impl,**cursor;
    HRESULT hr=S_OK;
    TRACE("queue %p, hwnd %p, priority %d, callback %p.\n", queue, queue->hwnd, priority, callback);
    if (!out) return E_POINTER;
    *out=NULL;
    if (!callback) return E_INVALIDARG;
    if (priority<CoreDispatcherPriority_Idle || priority>CoreDispatcherPriority_High) return E_INVALIDARG;
    if (!(impl=calloc(1,sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IAsyncAction_iface.lpVtbl=&action_vtbl; impl->IAsyncInfo_iface.lpVtbl=&info_vtbl;
    impl->ref=1; impl->id=InterlockedIncrement(&next_id); impl->priority=priority;
    impl->callback=callback; IDispatchedHandler_AddRef(callback);
    AcquireSRWLockExclusive(&queue->lock);
    if (!queue->hwnd) hr=RO_E_CLOSED;
    else if (!PostMessageW(queue->hwnd,WINE_WM_DISPATCH,0,0)) hr=HRESULT_FROM_WIN32(GetLastError());
    else
    {
        for (cursor=&queue->head; *cursor && (*cursor)->priority>=priority; cursor=&(*cursor)->next) {}
        impl->next=*cursor; *cursor=impl;
        *out=&impl->IAsyncAction_iface; IAsyncAction_AddRef(*out);
    }
    ReleaseSRWLockExclusive(&queue->lock);
    if (FAILED(hr)) IAsyncAction_Release(&impl->IAsyncAction_iface);
    return hr;
}
void dispatcher_queue_dispatch(struct dispatcher_queue *queue)
{
    struct dispatched_action *impl;
    BOOL run;
    HRESULT hr;
    CoreDispatcherPriority saved=queue->current_priority;
    AcquireSRWLockExclusive(&queue->lock);
    if ((impl=queue->head)) queue->head=impl->next;
    ReleaseSRWLockExclusive(&queue->lock);
    if (!impl) return;
    AcquireSRWLockExclusive(&impl->lock);
    run=impl->status==Started;
    impl->running=run;
    ReleaseSRWLockExclusive(&impl->lock);
    if (run)
    {
        queue->current_priority=impl->priority;
        hr=IDispatchedHandler_Invoke(impl->callback);
        TRACE("callback %p completed, hr %#lx.\n", impl->callback, hr);
        queue->current_priority=saved;
        action_finish(impl,FAILED(hr) ? Error : Completed,hr);
    }
    IDispatchedHandler_Release(impl->callback); impl->callback=NULL;
    IAsyncAction_Release(&impl->IAsyncAction_iface);
}
void dispatcher_queue_close(struct dispatcher_queue *queue)
{
    struct dispatched_action *impl,*next;
    AcquireSRWLockExclusive(&queue->lock);
    queue->hwnd=NULL; impl=queue->head; queue->head=NULL;
    ReleaseSRWLockExclusive(&queue->lock);
    for (;impl;impl=next)
    {
        next=impl->next;
        action_finish(impl,Canceled,HRESULT_FROM_WIN32(ERROR_CANCELLED));
        IDispatchedHandler_Release(impl->callback); impl->callback=NULL;
        IAsyncAction_Release(&impl->IAsyncAction_iface);
    }
}

BOOL dispatcher_queue_should_yield(struct dispatcher_queue *queue, CoreDispatcherPriority priority)
{
    BOOL yield;
    AcquireSRWLockShared(&queue->lock);
    yield=queue->head && queue->head->priority>=priority;
    ReleaseSRWLockShared(&queue->lock);
    return yield || (priority<=CoreDispatcherPriority_Normal && (HIWORD(GetQueueStatus(QS_INPUT|QS_PAINT|QS_SENDMESSAGE))!=0));
}
