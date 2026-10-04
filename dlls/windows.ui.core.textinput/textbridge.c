/* Desktop character input for a focused CoreTextEditContext.
 * This synchronous bridge does not implement TSF/IME composition or deferrals. */
#include "editcontext.h"
#include "roapi.h"
#include "winuser.h"
#include "commctrl.h"

WINE_DEFAULT_DEBUG_CHANNEL(textinput);

struct text_update
{
    ICoreTextTextUpdatingEventArgs iface;
    LONG ref;
    CoreTextRange range, selection;
    HSTRING text;
    CoreTextTextUpdatingResult result;
};
static HRESULT WINAPI update_qi(ICoreTextTextUpdatingEventArgs *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) &&
        !IsEqualGUID(iid, &IID_IAgileObject) && !IsEqualGUID(iid, &IID_ICoreTextTextUpdatingEventArgs)) return E_NOINTERFACE;
    *out = iface; ICoreTextTextUpdatingEventArgs_AddRef(iface); return S_OK;
}
static ULONG WINAPI update_addref(ICoreTextTextUpdatingEventArgs *iface)
{ return InterlockedIncrement(&((struct text_update *)iface)->ref); }
static ULONG WINAPI update_release(ICoreTextTextUpdatingEventArgs *iface)
{
    struct text_update *update = (void *)iface;
    ULONG ref = InterlockedDecrement(&update->ref);
    if (!ref) { WindowsDeleteString(update->text); free(update); }
    return ref;
}
static HRESULT WINAPI update_iids(ICoreTextTextUpdatingEventArgs *iface, ULONG *count, IID **iids)
{
    if (!count || !iids) return E_POINTER;
    *count = 0;
    if (!(*iids = CoTaskMemAlloc(sizeof(**iids)))) return E_OUTOFMEMORY;
    **iids = IID_ICoreTextTextUpdatingEventArgs; *count = 1; return S_OK;
}
static HRESULT WINAPI update_name(ICoreTextTextUpdatingEventArgs *iface, HSTRING *out)
{ const WCHAR *name = L"Windows.UI.Text.Core.CoreTextTextUpdatingEventArgs"; return WindowsCreateString(name, wcslen(name), out); }
static HRESULT WINAPI update_trust(ICoreTextTextUpdatingEventArgs *iface, TrustLevel *out)
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }
static HRESULT WINAPI update_range(ICoreTextTextUpdatingEventArgs *iface, CoreTextRange *out)
{ if (!out) return E_POINTER; *out = ((struct text_update *)iface)->range; return S_OK; }
static HRESULT WINAPI update_text(ICoreTextTextUpdatingEventArgs *iface, HSTRING *out)
{ return WindowsDuplicateString(((struct text_update *)iface)->text, out); }
static HRESULT WINAPI update_selection(ICoreTextTextUpdatingEventArgs *iface, CoreTextRange *out)
{ if (!out) return E_POINTER; *out = ((struct text_update *)iface)->selection; return S_OK; }
static HRESULT WINAPI update_language(ICoreTextTextUpdatingEventArgs *iface, ILanguage **out)
{ if (!out) return E_POINTER; *out = NULL; return E_NOTIMPL; }
static HRESULT WINAPI update_result(ICoreTextTextUpdatingEventArgs *iface, CoreTextTextUpdatingResult *out)
{ if (!out) return E_POINTER; *out = ((struct text_update *)iface)->result; return S_OK; }
static HRESULT WINAPI update_put_result(ICoreTextTextUpdatingEventArgs *iface, CoreTextTextUpdatingResult value)
{
    if (value != CoreTextTextUpdatingResult_Succeeded && value != CoreTextTextUpdatingResult_Failed) return E_INVALIDARG;
    ((struct text_update *)iface)->result = value; return S_OK;
}
static HRESULT WINAPI update_canceled(ICoreTextTextUpdatingEventArgs *iface, boolean *out)
{ if (!out) return E_POINTER; *out = FALSE; return S_OK; }
static HRESULT WINAPI update_deferral(ICoreTextTextUpdatingEventArgs *iface, IDeferral **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    ((struct text_update *)iface)->result = CoreTextTextUpdatingResult_Failed;
    return E_NOTIMPL;
}
static const ICoreTextTextUpdatingEventArgsVtbl update_vtbl =
{
    update_qi, update_addref, update_release, update_iids, update_name, update_trust,
    update_range, update_text, update_selection, update_language, update_result, update_put_result,
    update_canceled, update_deferral
};

static BOOL insert_character(struct core_text_edit_context *context, WCHAR character)
{
    struct text_update *update;
    CoreTextRange range = context->selection;
    unsigned int version = context->selection_version;
    HRESULT hr;
    BOOL subscribed;
    UINT32 length = 1;

    AcquireSRWLockShared(&context->events[3].lock);
    subscribed = !!context->events[3].head;
    ReleaseSRWLockShared(&context->events[3].lock);
    if (!subscribed) return FALSE;
    if (context->read_only) return TRUE;
    /* Editing commands such as Enter, Tab and Escape remain with the application. */
    if (character < 0x20 || character == 0x7f) return FALSE;
    if (range.StartCaretPosition == INT_MAX) return TRUE;
    if (!(update = calloc(1, sizeof(*update)))) return TRUE;
    update->iface.lpVtbl = &update_vtbl; update->ref = 1;
    update->range = range;
    update->selection.StartCaretPosition = update->selection.EndCaretPosition = range.StartCaretPosition + length;
    if (FAILED(WindowsCreateString(&character, length, &update->text))) { free(update); return TRUE; }
    TRACE("context %p, replacement range %d-%d, length %u\n", context, range.StartCaretPosition, range.EndCaretPosition, length);
    hr = winrt_event_notify(&context->events[3], &context->ICoreTextEditContext_iface, &update->iface);
    if (SUCCEEDED(hr) && update->result == CoreTextTextUpdatingResult_Succeeded &&
        context->selection_version == version) context->selection = update->selection;
    ICoreTextTextUpdatingEventArgs_Release(&update->iface);
    return TRUE;
}

static LRESULT CALLBACK text_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                        UINT_PTR id, DWORD_PTR data)
{
    struct core_text_edit_context *context = (void *)data;
    LRESULT result = 0;
    BOOL handled = FALSE;
    ICoreTextEditContext_AddRef(&context->ICoreTextEditContext_iface);
    if (msg == WM_CHAR) handled = insert_character(context, wparam);
    if (msg == WM_NCDESTROY)
    {
        edit_context_blur(context);
        winrt_event_notify(&context->events[8], &context->ICoreTextEditContext_iface, NULL);
    }
    if (!handled) result = DefSubclassProc(hwnd, msg, wparam, lparam);
    ICoreTextEditContext_Release(&context->ICoreTextEditContext_iface);
    return result;
}

void edit_context_blur(struct core_text_edit_context *context)
{
    HWND hwnd = context->hwnd;
    if (!hwnd) return;
    context->hwnd = NULL;
    RemoveWindowSubclass(hwnd, text_window_proc, 1);
    ICoreTextEditContext_Release(&context->ICoreTextEditContext_iface); /* subclass ownership */
}

HRESULT edit_context_focus(struct core_text_edit_context *context)
{
    const WCHAR *name = L"Windows.UI.Core.CoreWindow";
    ICoreWindowStatic *statics;
    ICoreWindow *window;
    ICoreWindowInterop *interop;
    struct core_text_edit_context *previous;
    DWORD_PTR data;
    HSTRING string;
    HWND hwnd;
    HRESULT hr;
    if (context->thread != GetCurrentThreadId()) return RPC_E_WRONG_THREAD;
    if (context->hwnd) return S_OK;
    if (FAILED(hr = WindowsCreateString(name, wcslen(name), &string))) return hr;
    hr = RoGetActivationFactory(string, &IID_ICoreWindowStatic, (void **)&statics);
    WindowsDeleteString(string);
    if (FAILED(hr)) return hr;
    hr = ICoreWindowStatic_GetForCurrentThread(statics, &window);
    ICoreWindowStatic_Release(statics);
    if (FAILED(hr)) return hr;
    if (window)
    {
        hr = ICoreWindow_QueryInterface(window, &IID_ICoreWindowInterop, (void **)&interop);
        ICoreWindow_Release(window);
        if (FAILED(hr)) return hr;
        hr = ICoreWindowInterop_get_WindowHandle(interop, &hwnd);
        ICoreWindowInterop_Release(interop);
        if (FAILED(hr)) return hr;
    }
    else
    {
        /* Desktop applications can use CoreText without owning a CoreWindow.
         * Attach to their keyboard target, not another thread's foreground window. */
        hwnd = GetFocus();
        if (!hwnd) hwnd = GetActiveWindow();
        if (!hwnd) return E_ILLEGAL_METHOD_CALL;
        if (GetWindowThreadProcessId(hwnd, NULL) != context->thread) return RPC_E_WRONG_THREAD;
    }
    if (GetWindowSubclass(hwnd, text_window_proc, 1, &data))
    {
        previous = (void *)data;
        ICoreTextEditContext_AddRef(&previous->ICoreTextEditContext_iface);
        edit_context_blur(previous);
        winrt_event_notify(&previous->events[8], &previous->ICoreTextEditContext_iface, NULL);
        ICoreTextEditContext_Release(&previous->ICoreTextEditContext_iface);
    }
    if (!SetWindowSubclass(hwnd, text_window_proc, 1, (DWORD_PTR)context)) return E_FAIL;
    context->hwnd = hwnd;
    ICoreTextEditContext_AddRef(&context->ICoreTextEditContext_iface);
    TRACE("context %p focused on hwnd %p\n", context, hwnd);
    return S_OK;
}
