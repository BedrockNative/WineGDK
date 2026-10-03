/* Character input must preserve layout output separately from gameplay keys. */
struct text_probe
{
    ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs iface;
    LONG ref;
    UINT32 codes[16];
    unsigned int count;
    ICharacterReceivedEventArgs *saved;
};
static HRESULT WINAPI text_qi(ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs *iface, REFIID iid, void **out)
{ (void)iid; *out = iface; ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs_AddRef(iface); return S_OK; }
static ULONG WINAPI text_addref(ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs *iface)
{ return InterlockedIncrement(&((struct text_probe *)iface)->ref); }
static ULONG WINAPI text_release(ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs *iface)
{ return InterlockedDecrement(&((struct text_probe *)iface)->ref); }
static HRESULT WINAPI text_invoke(ITypedEventHandler_CoreWindow_CharacterReceivedEventArgs *iface, ICoreWindow *window,
                                  ICharacterReceivedEventArgs *args)
{
    struct text_probe *probe = (void *)iface;
    ICoreWindowEventArgs *base;
    CorePhysicalKeyStatus status;
    UINT32 code;
    boolean handled;
    (void)window;
    CHECK(ICharacterReceivedEventArgs_get_KeyCode(args, &code) == S_OK);
    CHECK(ICharacterReceivedEventArgs_get_KeyStatus(args, &status) == S_OK && status.RepeatCount == 1);
    CHECK(probe->count < sizeof(probe->codes)/sizeof(*probe->codes));
    if (probe->count < sizeof(probe->codes)/sizeof(*probe->codes)) probe->codes[probe->count++] = code;
    CHECK(ICharacterReceivedEventArgs_QueryInterface(args, &IID_ICoreWindowEventArgs, (void **)&base) == S_OK);
    CHECK(ICoreWindowEventArgs_get_Handled(base, &handled) == S_OK && !handled);
    CHECK(ICoreWindowEventArgs_put_Handled(base, TRUE) == S_OK);
    ICoreWindowEventArgs_Release(base);
    if (!probe->saved) { probe->saved = args; ICharacterReceivedEventArgs_AddRef(args); }
    return S_OK;
}
static const ITypedEventHandler_CoreWindow_CharacterReceivedEventArgsVtbl text_vtbl =
{text_qi, text_addref, text_release, text_invoke};
static void test_text(ICoreWindow *window, ICoreDispatcher *dispatcher, HWND hwnd)
{
    struct text_probe probe = {{&text_vtbl}, 1, {0}, 0, NULL};
    EventRegistrationToken token;
    UINT32 saved;
    unsigned int i;
    const UINT32 chars[] = {'a', 0xe7, 0xe3, 0xd83d, 0xde00, '\b'};
    BYTE keyboard[256], neutral[256] = {0};
    CHECK(ICoreWindow_add_CharacterReceived(window, &probe.iface, &token) == S_OK && probe.ref == 2);
    SendMessageW(hwnd, WM_DEADCHAR, '^', 1);
    CHECK(probe.count == 0);
    for (i = 0; i < sizeof(chars)/sizeof(*chars); ++i) SendMessageW(hwnd, WM_CHAR, chars[i], 1);
    CHECK(probe.count == sizeof(chars)/sizeof(*chars));
    CHECK(!memcmp(probe.codes, chars, sizeof(chars)));
    CHECK(ICharacterReceivedEventArgs_get_KeyCode(probe.saved, &saved) == S_OK && saved == 'a');
    /* Exercise the dispatcher path from a virtual key through TranslateMessage. */
    GetKeyboardState(keyboard); SetKeyboardState(neutral);
    CHECK(PostMessageW(hwnd, WM_KEYDOWN, VK_SPACE, 1));
    CHECK(ICoreDispatcher_ProcessEvents(dispatcher, CoreProcessEventsOption_ProcessAllIfPresent) == S_OK);
    SetKeyboardState(keyboard);
    CHECK(probe.count == 7 && probe.codes[6] == ' ');
    CHECK(ICoreWindow_remove_CharacterReceived(window, token) == S_OK && probe.ref == 1);
    SendMessageW(hwnd, WM_CHAR, 'z', 1);
    CHECK(probe.count == 7);
    ICharacterReceivedEventArgs_Release(probe.saved);
}
