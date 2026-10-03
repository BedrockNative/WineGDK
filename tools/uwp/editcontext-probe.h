/* Exercise a focused custom editor through native character messages. */
struct editor_probe
{
    ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgs iface;
    LONG ref;
    WCHAR text[64];
    BOOL reject;
    unsigned int calls;
};
static HRESULT WINAPI editor_qi(ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgs *iface, REFIID iid, void **out)
{ (void)iid; *out = iface; ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgs_AddRef(iface); return S_OK; }
static ULONG WINAPI editor_addref(ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgs *iface)
{ return InterlockedIncrement(&((struct editor_probe *)iface)->ref); }
static ULONG WINAPI editor_release(ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgs *iface)
{ return InterlockedDecrement(&((struct editor_probe *)iface)->ref); }
static HRESULT WINAPI editor_invoke(ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgs *iface,
                                    ICoreTextEditContext *context, ICoreTextTextUpdatingEventArgs *args)
{
    struct editor_probe *editor = (void *)iface;
    CoreTextRange range, selection;
    HSTRING text;
    const WCHAR *str;
    UINT32 len;
    size_t old_len = wcslen(editor->text);
    ++editor->calls;
    CHECK(ICoreTextTextUpdatingEventArgs_get_Range(args, &range) == S_OK);
    CHECK(ICoreTextTextUpdatingEventArgs_get_NewSelection(args, &selection) == S_OK);
    CHECK(ICoreTextTextUpdatingEventArgs_get_Text(args, &text) == S_OK);
    str = WindowsGetStringRawBuffer(text, &len);
    CHECK(range.StartCaretPosition >= 0 && (size_t)range.EndCaretPosition <= old_len);
    if (editor->reject)
        CHECK(ICoreTextTextUpdatingEventArgs_put_Result(args, CoreTextTextUpdatingResult_Failed) == S_OK);
    else if (range.StartCaretPosition >= 0 && (size_t)range.EndCaretPosition <= old_len && old_len + len < 64)
    {
        memmove(editor->text + range.StartCaretPosition + len, editor->text + range.EndCaretPosition,
                (old_len - range.EndCaretPosition + 1) * sizeof(WCHAR));
        memcpy(editor->text + range.StartCaretPosition, str, len * sizeof(WCHAR));
        CHECK(ICoreTextEditContext_NotifyTextChanged(context, range, len, selection) == S_OK);
        CHECK(ICoreTextTextUpdatingEventArgs_put_Result(args, CoreTextTextUpdatingResult_Succeeded) == S_OK);
    }
    WindowsDeleteString(text);
    return S_OK;
}
static const ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgsVtbl editor_vtbl =
{editor_qi, editor_addref, editor_release, editor_invoke};
static void test_editcontext(HWND hwnd)
{
    ICoreTextServicesManagerStatics *statics;
    ICoreTextServicesManager *manager;
    ICoreTextEditContext *context;
    struct editor_probe editor = {{&editor_vtbl}, 1, {0}, FALSE, 0};
    EventRegistrationToken token;
    CoreTextRange selection = {0, 0};
    HRESULT hr;
    CHECK(get_factory(L"Windows.UI.Text.Core.CoreTextServicesManager", &IID_ICoreTextServicesManagerStatics, (void **)&statics) == S_OK);
    CHECK(ICoreTextServicesManagerStatics_GetForCurrentView(statics, &manager) == S_OK);
    CHECK(ICoreTextServicesManager_CreateEditContext(manager, &context) == S_OK);
    CHECK(ICoreTextEditContext_add_TextUpdating(context, &editor.iface, &token) == S_OK && editor.ref == 2);
    CHECK(ICoreTextEditContext_NotifySelectionChanged(context, selection) == S_OK);
    hr = ICoreTextEditContext_NotifyFocusEnter(context);
    CHECK(hr == S_OK);
    if (SUCCEEDED(hr))
    {
        SendMessageW(hwnd, WM_CHAR, 'a', 1);
        SendMessageW(hwnd, WM_CHAR, 0xe7, 1);
        SendMessageW(hwnd, WM_CHAR, 'b', 1);
        CHECK(!wcscmp(editor.text, L"a\x00e7" L"b"));
        selection.StartCaretPosition = 1; selection.EndCaretPosition = 2;
        CHECK(ICoreTextEditContext_NotifySelectionChanged(context, selection) == S_OK);
        SendMessageW(hwnd, WM_CHAR, 'X', 1);
        CHECK(!wcscmp(editor.text, L"aXb"));
        editor.reject = TRUE;
        SendMessageW(hwnd, WM_CHAR, 'Y', 1);
        CHECK(!wcscmp(editor.text, L"aXb"));
        editor.reject = FALSE;
        SendMessageW(hwnd, WM_CHAR, 'Z', 1);
        CHECK(!wcscmp(editor.text, L"aXZb"));
        CHECK(ICoreTextEditContext_put_IsReadOnly(context, TRUE) == S_OK);
        SendMessageW(hwnd, WM_CHAR, 'Q', 1);
        CHECK(editor.calls == 6);
        CHECK(ICoreTextEditContext_NotifyFocusLeave(context) == S_OK);
        SendMessageW(hwnd, WM_CHAR, 'Q', 1);
        CHECK(editor.calls == 6);
    }
    CHECK(ICoreTextEditContext_remove_TextUpdating(context, token) == S_OK && editor.ref == 1);
    CHECK(ICoreTextEditContext_Release(context) == 0);
    ICoreTextServicesManager_Release(manager);
    ICoreTextServicesManagerStatics_Release(statics);
}
