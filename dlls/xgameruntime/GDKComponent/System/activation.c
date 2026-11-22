/* GDK file activation via a per-session launcher inbox. LGPL-2.1-or-later. */
#include "private.h"
#include <stdio.h>
#include <wchar.h>

WINE_DEFAULT_DEBUG_CHANNEL(gdkc);

static const GUID activation_class = {0x7f0fe8b8,0xe075,0x49ab,{0x9a,0xa7,0xa1,0xe0,0x65,0x48,0x9a,0x9e}};
static const GUID activation_iid = {0x2e4f76fe,0x0fc7,0x461e,{0xab,0x4d,0xa4,0x49,0x94,0x34,0xc3,0xcf}};
HRESULT WINAPI QueryApiImpl(const GUID *, REFIID, void **);

struct activation_info { UINT32 type; const char *file; };
typedef void (WINAPI *activation_callback)(void *, const struct activation_info *);

struct subscription {
    IXThreadingImpl *threading;
    XTaskQueueHandle queue;
    XTaskQueueRegistrationToken waiter;
    HANDLE change;
    WCHAR dir[MAX_PATH];
    ULONG_PTR id;
    activation_callback callback;
    void *context;
    LONG refs;
    unsigned int callbacks;
};
static SRWLOCK lock = SRWLOCK_INIT;
static CONDITION_VARIABLE idle = CONDITION_VARIABLE_INIT;
static struct subscription *current;
static ULONG_PTR serial;
static __declspec(thread) struct subscription *dispatching;

static void release_subscription(struct subscription *s)
{
    if (InterlockedDecrement(&s->refs)) return;
    if (s->queue) IXThreadingImpl_XTaskQueueCloseHandle(s->threading, s->queue);
    if (s->threading) IXThreadingImpl_Release(s->threading);
    if (s->change != INVALID_HANDLE_VALUE) FindCloseChangeNotification(s->change);
    free(s);
}

static void deliver(struct subscription *s, const WCHAR *name)
{
    WCHAR request[MAX_PATH], claimed[MAX_PATH], reply[MAX_PATH];
    HANDLE f;
    DWORD size, read;
    char *path = NULL;
    BOOL delivered = FALSE;
    struct activation_info event;

    if (wcslen(name) != 40 || wcscmp(name + 36, L".req")) return;
    swprintf(request, ARRAY_SIZE(request), L"%ls\\%ls", s->dir, name);
    wcscpy(claimed, request);
    wcscpy(claimed + wcslen(claimed) - 3, L"run");
    if (!MoveFileW(request, claimed)) return;
    f = CreateFileW(claimed, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE)
    {
        size = GetFileSize(f, NULL);
        if (size && size <= 131072 && (path = malloc(size + 1)) &&
            ReadFile(f, path, size, &read, NULL) && read == size && !memchr(path, 0, size))
        {
            path[size] = 0;
            event.type = 1; /* XGameActivationType::File */
            event.file = path;
            s->callback(s->context, &event);
            delivered = TRUE;
        }
        CloseHandle(f);
    }
    free(path);
    wcscpy(reply, request);
    wcscpy(reply + wcslen(reply) - 3, delivered ? L"ack" : L"err");
    MoveFileW(claimed, reply);
}

static void WINAPI changes(void *token, BOOLEAN cancelled)
{
    struct subscription *s, *previous;
    WIN32_FIND_DATAW data;
    WCHAR pattern[MAX_PATH];
    HANDLE search;
    BOOL registered;
    AcquireSRWLockExclusive(&lock);
    s = current;
    if (cancelled || !s || s->id != (ULONG_PTR)token)
    {
        ReleaseSRWLockExclusive(&lock);
        return;
    }
    InterlockedIncrement(&s->refs);
    ++s->callbacks;
    ReleaseSRWLockExclusive(&lock);
    previous = dispatching;
    dispatching = s;
    /* Rearm before scanning so a rename during enumeration schedules another pass. */
    FindNextChangeNotification(s->change);
    swprintf(pattern, ARRAY_SIZE(pattern), L"%ls\\*.req", s->dir);
    if ((search = FindFirstFileW(pattern, &data)) != INVALID_HANDLE_VALUE)
    {
        do {
            AcquireSRWLockShared(&lock);
            registered = current == s;
            ReleaseSRWLockShared(&lock);
            if (!registered) break;
            deliver(s, data.cFileName);
        } while (FindNextFileW(search, &data));
        FindClose(search);
    }
    dispatching = previous;
    AcquireSRWLockExclusive(&lock);
    --s->callbacks;
    WakeAllConditionVariable(&idle);
    ReleaseSRWLockExclusive(&lock);
    release_subscription(s);
}

static HRESULT WINAPI subscribe(void *iface, XTaskQueueHandle queue, void *context, activation_callback callback,
                                 XTaskQueueRegistrationToken *token)
{
    struct subscription *s;
    DWORD len;
    HRESULT hr;
    WCHAR ready[MAX_PATH];
    HANDLE marker;
    if (!callback || !token) return E_INVALIDARG;
    if (!(s = calloc(1, sizeof(*s)))) return E_OUTOFMEMORY;
    s->change = INVALID_HANDLE_VALUE;
    s->refs = 1;
    s->callback = callback;
    s->context = context;
    len = GetEnvironmentVariableW(L"WINEGDK_FILE_ACTIVATION_DIR", s->dir, ARRAY_SIZE(s->dir));
    if (!len || len + 42 >= ARRAY_SIZE(s->dir)) { hr = E_INVALIDARG; goto fail; }
    s->change = FindFirstChangeNotificationW(s->dir, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME);
    if (s->change == INVALID_HANDLE_VALUE) { hr = HRESULT_FROM_WIN32(GetLastError()); goto fail; }
    hr = QueryApiImpl(&CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void **)&s->threading);
    if (FAILED(hr)) goto fail;
    if (queue) hr = IXThreadingImpl_XTaskQueueDuplicateHandle(s->threading, queue, &s->queue);
    else if (!IXThreadingImpl_XTaskQueueGetCurrentProcessTaskQueue(s->threading, &s->queue)) hr = E_FAIL;
    if (FAILED(hr)) goto fail;
    swprintf(ready, ARRAY_SIZE(ready), L"%ls\\ready", s->dir);
    AcquireSRWLockExclusive(&lock);
    if (current) hr = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
    else {
        s->id = ++serial;
        hr = IXThreadingImpl_XTaskQueueRegisterWaiter(s->threading, s->queue, XTaskQueuePort_Completion,
                                                      s->change, (void *)s->id, changes, &s->waiter);
        if (SUCCEEDED(hr)) {
            marker = CreateFileW(ready, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (marker != INVALID_HANDLE_VALUE) {
                CloseHandle(marker);
                current = s;
                token->token = s->id;
            } else {
                hr = HRESULT_FROM_WIN32(GetLastError());
                ReleaseSRWLockExclusive(&lock);
                IXThreadingImpl_XTaskQueueUnregisterWaiter(s->threading, s->queue, s->waiter);
                goto fail;
            }
        }
    }
    ReleaseSRWLockExclusive(&lock);
    if (SUCCEEDED(hr)) return S_OK;
fail:
    WARN("file activation subscription failed: %#lx\n", hr);
    release_subscription(s);
    return hr;
}

static BOOLEAN WINAPI unsubscribe(void *iface, XTaskQueueRegistrationToken token, BOOLEAN wait)
{
    struct subscription *s;
    WCHAR path[MAX_PATH];
    AcquireSRWLockExclusive(&lock);
    s = current;
    if (!s || s->id != token.token) { ReleaseSRWLockExclusive(&lock); return FALSE; }
    current = NULL;
    swprintf(path, ARRAY_SIZE(path), L"%ls\\ready", s->dir);
    DeleteFileW(path);
    while (wait && s->callbacks > (dispatching == s ? 1u : 0u))
        SleepConditionVariableSRW(&idle, &lock, INFINITE, 0);
    ReleaseSRWLockExclusive(&lock);
    IXThreadingImpl_XTaskQueueUnregisterWaiter(s->threading, s->queue, s->waiter);
    release_subscription(s);
    return TRUE;
}

static HRESULT WINAPI accept_invite(void *iface, const char *uri) { return E_NOTIMPL; }
static HRESULT WINAPI query(void *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &activation_iid) && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    *out = iface;
    return S_OK;
}
static ULONG WINAPI addref(void *iface) { return 2; }
static ULONG WINAPI release(void *iface) { return 1; }
static const struct {
    HRESULT (WINAPI *query)(void *, REFIID, void **);
    ULONG (WINAPI *addref)(void *), (WINAPI *release)(void *);
    HRESULT (WINAPI *subscribe)(void *, XTaskQueueHandle, void *, activation_callback, XTaskQueueRegistrationToken *);
    BOOLEAN (WINAPI *unsubscribe)(void *, XTaskQueueRegistrationToken, BOOLEAN);
    HRESULT (WINAPI *accept_invite)(void *, const char *);
} vtable = {query, addref, release, subscribe, unsubscribe, accept_invite};
static const void *instance = &vtable;
HRESULT activation_query(REFIID cls, REFIID iid, void **out)
{
    if (!IsEqualGUID(cls, &activation_class) || !GetEnvironmentVariableW(L"WINEGDK_FILE_ACTIVATION_DIR", NULL, 0))
        return E_NOINTERFACE;
    return query(&instance, iid, out);
}
