/*
 * XThreading task queue and async tests
 *
 * Copyright 2026 the Wine project authors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <xasyncprovider.h>
#include "wine/test.h"

static IXThreadingImpl *threading;
static const unsigned int identity;

struct async_context
{
    unsigned int begins, work, completion, cleanup, cancels;
    UINT32 delay;
    BOOL schedule;
};

static HRESULT CALLBACK async_provider( XAsyncOp op, const XAsyncProviderData *data )
{
    struct async_context *context = data->context;

    switch (op)
    {
    case XAsyncOp_Begin:
        ++context->begins;
        if (context->schedule)
            return IXThreadingImpl_XAsyncSchedule( threading, data->async, context->delay );
        break;
    case XAsyncOp_DoWork:
        ++context->work;
        IXThreadingImpl_XAsyncComplete( threading, data->async, S_OK, sizeof(unsigned int) );
        break;
    case XAsyncOp_GetResult:
        ok( data->bufferSize >= sizeof(unsigned int), "Result buffer too small.\n" );
        if (data->bufferSize >= sizeof(unsigned int)) *(unsigned int *)data->buffer = 0x12345678;
        break;
    case XAsyncOp_Cancel:
        ++context->cancels;
        IXThreadingImpl_XAsyncComplete( threading, data->async, E_ABORT, 0 );
        break;
    case XAsyncOp_Cleanup:
        ++context->cleanup;
        break;
    }
    return S_OK;
}

static void CALLBACK async_completed( XAsyncBlock *async )
{
    struct async_context *context = async->context;
    ++context->completion;
}

static void test_async_lifetime(void)
{
    struct
    {
        XAsyncBlock async;
        BYTE guard[512];
    } block;
    struct async_context context;
    XTaskQueueHandle queue, duplicate;
    unsigned int iteration, i, damaged, result;
    SIZE_T size, used;
    HRESULT hr;
    BOOLEAN dispatched;

    for (iteration = 0; iteration < 128; ++iteration)
    {
        winetest_push_context( "iteration %u", iteration );
        hr = IXThreadingImpl_XTaskQueueCreate( threading, XTaskQueueDispatchMode_Manual,
                XTaskQueueDispatchMode_Manual, &queue );
        ok( hr == S_OK, "Create returned %#lx.\n", hr );
        if (FAILED(hr)) { winetest_pop_context(); break; }
        hr = IXThreadingImpl_XTaskQueueDuplicateHandle( threading, queue, &duplicate );
        ok( hr == S_OK, "Duplicate returned %#lx.\n", hr );
        if (FAILED(hr))
        {
            IXThreadingImpl_XTaskQueueCloseHandle( threading, queue );
            winetest_pop_context();
            break;
        }
        memset( &block, 0, sizeof(block) );
        memset( block.guard, 0xa5, sizeof(block.guard) );
        memset( &context, 0, sizeof(context) );
        context.schedule = TRUE;
        context.delay = iteration == 0 ? 10 : 0;
        block.async.queue = queue;
        block.async.context = &context;
        block.async.callback = async_completed;
        hr = IXThreadingImpl_XAsyncBegin( threading, &block.async, &context, &identity,
                "async_lifetime", async_provider );
        ok( hr == S_OK, "Begin returned %#lx.\n", hr );
        damaged = 0;
        for (i = 0; i < sizeof(block.guard); ++i) damaged += block.guard[i] != 0xa5;
        ok( !damaged, "Begin overwrote %u bytes after XAsyncBlock.\n", damaged );
        ok( context.begins == 1, "Begin called %u times.\n", context.begins );
        IXThreadingImpl_XTaskQueueCloseHandle( threading, queue );
        if (SUCCEEDED(hr))
        {
            hr = IXThreadingImpl_XAsyncGetStatus( threading, &block.async, FALSE );
            ok( hr == E_PENDING, "Initial status %#lx.\n", hr );
            dispatched = IXThreadingImpl_XTaskQueueDispatch( threading, duplicate, XTaskQueuePort_Work, 1000 );
            ok( dispatched, "Work did not dispatch.\n" );
            ok( context.work == 1, "Work ran %u times.\n", context.work );
            ok( !context.completion, "Manual completion ran without dispatch.\n" );
            dispatched = IXThreadingImpl_XTaskQueueDispatch( threading, duplicate, XTaskQueuePort_Completion, 1000 );
            ok( dispatched, "Completion did not dispatch.\n" );
            ok( context.completion == 1, "Completion ran %u times.\n", context.completion );
            hr = IXThreadingImpl_XAsyncGetStatus( threading, &block.async, FALSE );
            ok( hr == S_OK, "Completed status %#lx.\n", hr );
            size = 0;
            hr = IXThreadingImpl_XAsyncGetResultSize( threading, &block.async, &size );
            ok( hr == S_OK, "GetResultSize returned %#lx.\n", hr );
            ok( size == sizeof(result), "Unexpected result size %Iu.\n", size );
            result = 0;
            used = 0;
            hr = IXThreadingImpl_XAsyncGetResult( threading, &block.async, &identity, sizeof(result), &result, &used );
            ok( hr == S_OK, "GetResult returned %#lx.\n", hr );
            ok( used == sizeof(result) && result == 0x12345678, "Unexpected result %#x, size %Iu.\n", result, used );
            ok( context.cleanup == 1, "Cleanup ran %u times.\n", context.cleanup );
        }
        IXThreadingImpl_XTaskQueueCloseHandle( threading, duplicate );
        winetest_pop_context();
    }
}

static void test_async_cancel(void)
{
    struct async_context context = {0};
    struct { XAsyncBlock async; BYTE guard[512]; } block = {0};
    XTaskQueueHandle queue;
    HRESULT hr;

    hr = IXThreadingImpl_XTaskQueueCreate( threading, XTaskQueueDispatchMode_Manual,
            XTaskQueueDispatchMode_Manual, &queue );
    ok( hr == S_OK, "Create returned %#lx.\n", hr );
    if (FAILED(hr)) return;
    block.async.queue = queue;
    block.async.context = &context;
    block.async.callback = async_completed;
    hr = IXThreadingImpl_XAsyncBegin( threading, &block.async, &context, &identity,
            "async_cancel", async_provider );
    ok( hr == S_OK, "Begin returned %#lx.\n", hr );
    if (SUCCEEDED(hr))
    {
        IXThreadingImpl_XAsyncCancel( threading, &block.async );
        ok( IXThreadingImpl_XTaskQueueDispatch( threading, queue, XTaskQueuePort_Completion, 1000 ),
                "Canceled completion not dispatched.\n" );
        hr = IXThreadingImpl_XAsyncGetStatus( threading, &block.async, FALSE );
        ok( hr == E_ABORT, "Canceled status %#lx.\n", hr );
        ok( context.cancels == 1 && context.completion == 1 && context.cleanup == 1,
                "Unexpected cancel/completion/cleanup counts %u/%u/%u.\n",
                context.cancels, context.completion, context.cleanup );
        IXThreadingImpl_XAsyncCancel( threading, &block.async );
        ok( context.cancels == 1, "Cancel invoked provider twice.\n" );
    }
    IXThreadingImpl_XTaskQueueCloseHandle( threading, queue );
}

struct queue_context
{
    HANDLE event;
    LONG calls, canceled;
};

static HRESULT CALLBACK async_work( XAsyncBlock *async )
{
    struct queue_context *context = async->context;
    InterlockedIncrement( &context->calls );
    return S_OK;
}

static void CALLBACK async_work_completed( XAsyncBlock *async )
{
    struct queue_context *context = async->context;
    SetEvent( context->event );
}

static void CALLBACK queue_callback( void *arg, BOOLEAN canceled )
{
    struct queue_context *context = arg;
    if (canceled) InterlockedIncrement( &context->canceled );
    InterlockedIncrement( &context->calls );
    SetEvent( context->event );
}

static void test_dispatch_modes(void)
{
    XTaskQueueDispatchMode work, completion, mode;
    XTaskQueuePort port;
    struct queue_context context;
    struct { XAsyncBlock async; BYTE guard[512]; } block;
    XTaskQueueHandle queue;
    unsigned int i;
    HRESULT hr;
    DWORD wait;

    context.event = CreateEventW( NULL, FALSE, FALSE, NULL );
    ok( !!context.event, "CreateEvent failed.\n" );
    if (!context.event) return;
    for (work = 0; work <= XTaskQueueDispatchMode_Immediate; ++work)
    for (completion = 0; completion <= XTaskQueueDispatchMode_Immediate; ++completion)
    {
        winetest_push_context( "modes %u/%u", work, completion );
        hr = IXThreadingImpl_XTaskQueueCreate( threading, work, completion, &queue );
        ok( hr == S_OK, "Create returned %#lx.\n", hr );
        if (SUCCEEDED(hr))
        {
            for (port = XTaskQueuePort_Work; port <= XTaskQueuePort_Completion; ++port)
            {
                context.calls = context.canceled = 0;
                mode = port == XTaskQueuePort_Work ? work : completion;
                hr = IXThreadingImpl_XTaskQueueSubmitCallback( threading, queue, port, &context, queue_callback );
                ok( hr == S_OK, "Submit returned %#lx.\n", hr );
                if (mode == XTaskQueueDispatchMode_Manual)
                {
                    ok( !context.calls, "Manual callback ran before dispatch.\n" );
                    ok( IXThreadingImpl_XTaskQueueDispatch( threading, queue, port, 1000 ), "No callback to dispatch.\n" );
                }
                wait = WaitForSingleObject( context.event, 1000 );
                ok( wait == WAIT_OBJECT_0, "Callback timed out: %#lx.\n", wait );
                ok( context.calls == 1 && !context.canceled, "Calls %ld, canceled %ld.\n", context.calls, context.canceled );
            }
            for (i = 0; i < 32; ++i)
            {
                memset( &block, 0, sizeof(block) );
                block.async.queue = queue;
                block.async.context = &context;
                block.async.callback = async_work_completed;
                context.calls = 0;
                hr = IXThreadingImpl_XAsyncRun( threading, &block.async, async_work );
                ok( hr == S_OK, "AsyncRun returned %#lx.\n", hr );
                if (work == XTaskQueueDispatchMode_Manual)
                    ok( IXThreadingImpl_XTaskQueueDispatch( threading, queue, XTaskQueuePort_Work, 1000 ),
                            "Async work not dispatched.\n" );
                if (completion == XTaskQueueDispatchMode_Manual)
                    ok( IXThreadingImpl_XTaskQueueDispatch( threading, queue, XTaskQueuePort_Completion, 1000 ),
                            "Async completion not dispatched.\n" );
                wait = WaitForSingleObject( context.event, 1000 );
                ok( wait == WAIT_OBJECT_0, "AsyncRun timed out: %#lx.\n", wait );
                ok( context.calls == 1, "Async work called %ld times.\n", context.calls );
                hr = IXThreadingImpl_XAsyncGetStatus( threading, &block.async, FALSE );
                ok( hr == S_OK, "AsyncRun status %#lx.\n", hr );
            }
            IXThreadingImpl_XTaskQueueCloseHandle( threading, queue );
        }
        winetest_pop_context();
    }
    CloseHandle( context.event );
}

static void CALLBACK queue_terminated( void *arg )
{
    SetEvent( arg );
}

static void test_termination(void)
{
    struct queue_context context = {0};
    XTaskQueueHandle queue;
    HANDLE terminated;
    unsigned int i;
    DWORD wait;
    HRESULT hr;

    context.event = CreateEventW( NULL, FALSE, FALSE, NULL );
    terminated = CreateEventW( NULL, TRUE, FALSE, NULL );
    ok( !!context.event && !!terminated, "CreateEvent failed.\n" );
    if (!context.event || !terminated) goto done;
    hr = IXThreadingImpl_XTaskQueueCreate( threading, XTaskQueueDispatchMode_Manual,
            XTaskQueueDispatchMode_Manual, &queue );
    ok( hr == S_OK, "Create returned %#lx.\n", hr );
    if (FAILED(hr)) goto done;
    hr = IXThreadingImpl_XTaskQueueSubmitDelayedCallback( threading, queue, XTaskQueuePort_Work,
            60000, &context, queue_callback );
    ok( hr == S_OK, "Submit delayed callback returned %#lx.\n", hr );
    hr = IXThreadingImpl_XTaskQueueTerminate( threading, queue, FALSE, terminated, queue_terminated );
    ok( hr == S_OK, "Terminate returned %#lx.\n", hr );
    for (i = 0; i < 20 && WaitForSingleObject( terminated, 0 ) == WAIT_TIMEOUT; ++i)
    {
        IXThreadingImpl_XTaskQueueDispatch( threading, queue, XTaskQueuePort_Work, 10 );
        IXThreadingImpl_XTaskQueueDispatch( threading, queue, XTaskQueuePort_Completion, 10 );
    }
    wait = WaitForSingleObject( terminated, 0 );
    ok( wait == WAIT_OBJECT_0, "Termination did not complete: %#lx.\n", wait );
    ok( context.calls == 1 && context.canceled == 1, "Calls %ld, canceled %ld.\n", context.calls, context.canceled );
    hr = IXThreadingImpl_XTaskQueueSubmitCallback( threading, queue, XTaskQueuePort_Work, &context, queue_callback );
    ok( FAILED(hr), "Terminated queue accepted new work.\n" );
    IXThreadingImpl_XTaskQueueCloseHandle( threading, queue );
done:
    if (context.event) CloseHandle( context.event );
    if (terminated) CloseHandle( terminated );
}

static DWORD WINAPI time_sensitive_worker( void *arg )
{
    HRESULT hr = IXThreadingImpl_XThreadVerifyNotTimeSensitive( threading );
    ok( hr == S_OK, "New thread inherited time sensitivity: %#lx.\n", hr );
    return 0;
}

static void test_time_sensitive(void)
{
    HANDLE worker;
    HRESULT hr;

    hr = IXThreadingImpl_XThreadVerifyNotTimeSensitive( threading );
    ok( hr == S_OK, "Initial verification returned %#lx.\n", hr );
    hr = IXThreadingImpl_XThreadSetTimeSensitive( threading, TRUE );
    ok( hr == S_OK, "SetTimeSensitive returned %#lx.\n", hr );
    hr = IXThreadingImpl_XThreadVerifyNotTimeSensitive( threading );
    ok( hr == HRESULT_FROM_WIN32(ERROR_TIME_SENSITIVE_THREAD), "Sensitive verification returned %#lx.\n", hr );
    worker = CreateThread( NULL, 0, time_sensitive_worker, NULL, 0, NULL );
    ok( !!worker, "CreateThread failed.\n" );
    if (worker)
    {
        ok( WaitForSingleObject( worker, 1000 ) == WAIT_OBJECT_0, "Worker timed out.\n" );
        CloseHandle( worker );
    }
    hr = IXThreadingImpl_XThreadSetTimeSensitive( threading, FALSE );
    ok( hr == S_OK, "ClearTimeSensitive returned %#lx.\n", hr );
    hr = IXThreadingImpl_XThreadVerifyNotTimeSensitive( threading );
    ok( hr == S_OK, "Verification after clear returned %#lx.\n", hr );
}

START_TEST(xthreading)
{
    HRESULT (WINAPI *query)( const GUID *, REFIID, void ** );
    HMODULE module = LoadLibraryA( "xgameruntime.dll" );
    HRESULT hr;

    if (!module) { win_skip( "xgameruntime.dll unavailable.\n" ); return; }
    query = (void *)GetProcAddress( module, "QueryApiImpl" );
    if (!query) { win_skip( "QueryApiImpl unavailable.\n" ); FreeLibrary( module ); return; }
    hr = query( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void **)&threading );
    ok( hr == S_OK, "QueryApiImpl returned %#lx.\n", hr );
    if (SUCCEEDED(hr) && threading)
    {
        test_async_lifetime();
        test_async_cancel();
        test_dispatch_modes();
        test_termination();
        test_time_sensitive();
        IXThreadingImpl_Release( threading );
    }
    FreeLibrary( module );
}
