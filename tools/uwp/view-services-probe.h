/* Included by core-probe.c: behavior checks inside a real CoreApplication view. */
struct dispatched_probe
{
    IDispatchedHandler callback;
    IAsyncActionCompletedHandler completed;
    LONG ref;
    DWORD thread;
    unsigned int calls, completions;
    AsyncStatus status;
    HRESULT result;
};
static HRESULT WINAPI dispatched_qi(IDispatchedHandler *iface, REFIID iid, void **out)
{
    (void)iid; *out=iface; IDispatchedHandler_AddRef(iface); return S_OK;
}
static ULONG WINAPI dispatched_addref(IDispatchedHandler *iface)
{ return InterlockedIncrement(&((struct dispatched_probe *)iface)->ref); }
static ULONG WINAPI dispatched_release(IDispatchedHandler *iface)
{ return InterlockedDecrement(&((struct dispatched_probe *)iface)->ref); }
static HRESULT WINAPI dispatched_invoke(IDispatchedHandler *iface)
{
    struct dispatched_probe *probe=(void *)iface;
    CHECK(GetCurrentThreadId()==probe->thread);
    ++probe->calls; return probe->result;
}
static struct dispatched_probe *from_completion(IAsyncActionCompletedHandler *iface)
{ return CONTAINING_RECORD(iface,struct dispatched_probe,completed); }
static HRESULT WINAPI completion_qi(IAsyncActionCompletedHandler *iface, REFIID iid, void **out)
{ return dispatched_qi(&from_completion(iface)->callback,iid,out); }
static ULONG WINAPI completion_addref(IAsyncActionCompletedHandler *iface)
{ return dispatched_addref(&from_completion(iface)->callback); }
static ULONG WINAPI completion_release(IAsyncActionCompletedHandler *iface)
{ return dispatched_release(&from_completion(iface)->callback); }
static HRESULT WINAPI completion_invoke(IAsyncActionCompletedHandler *iface, IAsyncAction *action, AsyncStatus status)
{
    struct dispatched_probe *probe=from_completion(iface);
    CHECK(action!=NULL); ++probe->completions; probe->status=status; return S_OK;
}
static const IDispatchedHandlerVtbl dispatched_vtbl={dispatched_qi,dispatched_addref,dispatched_release,dispatched_invoke};
static const IAsyncActionCompletedHandlerVtbl completion_vtbl={completion_qi,completion_addref,completion_release,completion_invoke};
static HRESULT get_factory(const WCHAR *str, REFIID iid, void **out)
{
    HSTRING name;
    HRESULT hr=WindowsCreateString(str,wcslen(str),&name);
    if (FAILED(hr)) return hr;
    hr=RoGetActivationFactory(name,iid,out); WindowsDeleteString(name); return hr;
}
struct enqueue_probe { ICoreDispatcher *dispatcher; struct dispatched_probe *probe; IAsyncAction *action; IAgileReference *agile; };
static DWORD WINAPI enqueue_thread(void *arg)
{
    struct enqueue_probe *probe=arg;
    ICoreWindow *window=NULL;
    Rect bounds;
    CHECK(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
    CHECK(IAgileReference_Resolve(probe->agile,&IID_ICoreWindow,(void **)&window)==S_OK);
    if (window)
    {
        CHECK(ICoreWindow_get_Bounds(window,&bounds)==RPC_E_WRONG_THREAD);
        ICoreWindow_Release(window);
    }
    CHECK(ICoreDispatcher_RunAsync(probe->dispatcher,CoreDispatcherPriority_Normal,&probe->probe->callback,&probe->action)==S_OK);
    RoUninitialize(); return 0;
}
static void test_dispatcher(ICoreWindow *window, ICoreDispatcher *dispatcher)
{
    struct dispatched_probe probes[3]={0};
    struct enqueue_probe enqueue={0};
    IAsyncAction *actions[3]={0};
    IAsyncInfo *info;
    ICoreDispatcherWithTaskPriority *priority;
    AsyncStatus status;
    HRESULT error;
    HANDLE thread;
    boolean flag;
    unsigned int i;
    CHECK(ICoreDispatcher_QueryInterface(dispatcher,&IID_ICoreDispatcherWithTaskPriority,(void **)&priority)==S_OK);
    CHECK(ICoreDispatcherWithTaskPriority_put_CurrentPriority(priority,(CoreDispatcherPriority)99)==E_INVALIDARG);
    for (i=0;i<3;i++)
    {
        probes[i].callback.lpVtbl=&dispatched_vtbl; probes[i].completed.lpVtbl=&completion_vtbl;
        probes[i].ref=1; probes[i].thread=GetCurrentThreadId();
    }
    probes[1].result=E_ACCESSDENIED;
    enqueue.dispatcher=dispatcher; enqueue.probe=&probes[0];
    CHECK(RoGetAgileReference(AGILEREFERENCE_DEFAULT,&IID_ICoreWindow,(IUnknown *)window,&enqueue.agile)==S_OK);
    thread=CreateThread(NULL,0,enqueue_thread,&enqueue,0,NULL);
    CHECK(thread && WaitForSingleObject(thread,5000)==WAIT_OBJECT_0); CloseHandle(thread);
    IAgileReference_Release(enqueue.agile); actions[0]=enqueue.action;
    for (i=1;i<3;i++) CHECK(ICoreDispatcher_RunAsync(dispatcher,CoreDispatcherPriority_Normal,&probes[i].callback,&actions[i])==S_OK);
    for (i=0;i<3;i++)
    {
        CHECK(actions[i]!=NULL && probes[i].calls==0);
        CHECK(IAsyncAction_GetResults(actions[i])==E_ILLEGAL_METHOD_CALL);
        CHECK(IAsyncAction_QueryInterface(actions[i],&IID_IAsyncInfo,(void **)&info)==S_OK);
        CHECK(IAsyncInfo_get_Status(info,&status)==S_OK && status==Started);
        CHECK(IAsyncInfo_Close(info)==E_ILLEGAL_STATE_CHANGE);
        if (i==2) CHECK(IAsyncInfo_Cancel(info)==S_OK);
        IAsyncInfo_Release(info);
        if (i!=1) CHECK(IAsyncAction_put_Completed(actions[i],&probes[i].completed)==S_OK);
    }
    CHECK(ICoreDispatcherWithTaskPriority_ShouldYieldToPriority(priority,CoreDispatcherPriority_Normal,&flag)==S_OK && flag);
    CHECK(ICoreDispatcher_ProcessEvents(dispatcher,CoreProcessEventsOption_ProcessAllIfPresent)==S_OK);
    CHECK(IAsyncAction_put_Completed(actions[1],&probes[1].completed)==S_OK); /* late registration */
    for (i=0;i<3;i++)
    {
        HRESULT expected=i==2 ? HRESULT_FROM_WIN32(ERROR_CANCELLED) : probes[i].result;
        CHECK(probes[i].calls==(i==2 ? 0u : 1u) && probes[i].completions==1);
        CHECK(probes[i].status==(i==2 ? Canceled : i==1 ? Error : Completed));
        CHECK(IAsyncAction_GetResults(actions[i])==expected);
        CHECK(IAsyncAction_put_Completed(actions[i],&probes[i].completed)==E_ILLEGAL_DELEGATE_ASSIGNMENT);
        CHECK(IAsyncAction_QueryInterface(actions[i],&IID_IAsyncInfo,(void **)&info)==S_OK);
        CHECK(IAsyncInfo_get_ErrorCode(info,&error)==S_OK && error==expected);
        CHECK(IAsyncInfo_Close(info)==S_OK);
        CHECK(IAsyncAction_GetResults(actions[i])==RO_E_CLOSED);
        IAsyncInfo_Release(info); IAsyncAction_Release(actions[i]);
        CHECK(probes[i].ref==1);
    }
    ICoreDispatcherWithTaskPriority_Release(priority);
}
static void test_view_services(ICoreWindow *window, ICoreDispatcher *dispatcher, HWND hwnd)
{
    ISystemNavigationManagerStatics *navigation_statics;
    ISystemNavigationManager *navigation,*again;
    ISystemNavigationManager2 *navigation2;
    IPointerVisualizationSettingsStatics *feedback_statics;
    IPointerVisualizationSettings *feedback;
    IDisplayInformationStatics *display_statics;
    IDisplayInformation *display;
    IDisplayInformation4 *display4;
    IApiInformationStatics *api;
    IHolographicApplicationPreviewStatics *holographic;
    IActivationFactory *factory;
    IMouseCapabilities *mouse;
    IMouseDeviceStatics *mouse_statics;
    IMouseDevice *mouse_device,*mouse_again;
    HSTRING type,property;
    boolean flag;
    INT32 present;
    UINT32 width,height;
    float dpi;
    AppViewBackButtonVisibility visibility;
    CHECK(get_factory(L"Windows.UI.Core.SystemNavigationManager",&IID_ISystemNavigationManagerStatics,(void **)&navigation_statics)==S_OK);
    CHECK(ISystemNavigationManagerStatics_GetForCurrentView(navigation_statics,&navigation)==S_OK);
    CHECK(ISystemNavigationManagerStatics_GetForCurrentView(navigation_statics,&again)==S_OK && again==navigation);
    ISystemNavigationManager_Release(again);
    CHECK(ISystemNavigationManager_QueryInterface(navigation,&IID_ISystemNavigationManager2,(void **)&navigation2)==S_OK);
    CHECK(ISystemNavigationManager2_put_AppViewBackButtonVisibility(navigation2,AppViewBackButtonVisibility_Visible)==S_OK);
    CHECK(ISystemNavigationManager2_get_AppViewBackButtonVisibility(navigation2,&visibility)==S_OK && visibility==AppViewBackButtonVisibility_Visible);
    CHECK(ISystemNavigationManager2_put_AppViewBackButtonVisibility(navigation2,AppViewBackButtonVisibility_Collapsed)==S_OK);
    ISystemNavigationManager2_Release(navigation2); ISystemNavigationManager_Release(navigation); ISystemNavigationManagerStatics_Release(navigation_statics);
    CHECK(get_factory(L"Windows.UI.Input.PointerVisualizationSettings",&IID_IPointerVisualizationSettingsStatics,(void **)&feedback_statics)==S_OK);
    CHECK(IPointerVisualizationSettingsStatics_GetForCurrentView(feedback_statics,&feedback)==S_OK);
    CHECK(IPointerVisualizationSettings_put_IsContactFeedbackEnabled(feedback,FALSE)==S_OK);
    CHECK(IPointerVisualizationSettings_get_IsContactFeedbackEnabled(feedback,&flag)==S_OK && !flag);
    CHECK(IPointerVisualizationSettings_put_IsContactFeedbackEnabled(feedback,TRUE)==E_NOTIMPL);
    IPointerVisualizationSettings_Release(feedback); IPointerVisualizationSettingsStatics_Release(feedback_statics);
    CHECK(get_factory(L"Windows.Graphics.Display.DisplayInformation",&IID_IDisplayInformationStatics,(void **)&display_statics)==S_OK);
    CHECK(IDisplayInformationStatics_GetForCurrentView(display_statics,&display)==S_OK);
    CHECK(IDisplayInformation_get_LogicalDpi(display,&dpi)==S_OK && dpi==GetDpiForWindow(hwnd));
    CHECK(IDisplayInformation_QueryInterface(display,&IID_IDisplayInformation4,(void **)&display4)==S_OK);
    CHECK(IDisplayInformation4_get_ScreenWidthInRawPixels(display4,&width)==S_OK && width>0);
    CHECK(IDisplayInformation4_get_ScreenHeightInRawPixels(display4,&height)==S_OK && height>0);
    IDisplayInformation4_Release(display4); IDisplayInformation_Release(display); IDisplayInformationStatics_Release(display_statics);
    CHECK(get_factory(L"Windows.Devices.Input.MouseCapabilities",&IID_IActivationFactory,(void **)&factory)==S_OK);
    CHECK(IActivationFactory_ActivateInstance(factory,(IInspectable **)&mouse)==S_OK);
    CHECK(IMouseCapabilities_get_MousePresent(mouse,&present)==S_OK && present==GetSystemMetrics(SM_MOUSEPRESENT));
    CHECK(IMouseCapabilities_get_NumberOfButtons(mouse,&width)==S_OK && width==(UINT32)GetSystemMetrics(SM_CMOUSEBUTTONS));
    IMouseCapabilities_Release(mouse); IActivationFactory_Release(factory);
    CHECK(get_factory(L"Windows.Devices.Input.MouseDevice",&IID_IMouseDeviceStatics,(void **)&mouse_statics)==S_OK);
    CHECK(IMouseDeviceStatics_GetForCurrentView(mouse_statics,&mouse_device)==S_OK);
    CHECK(IMouseDeviceStatics_GetForCurrentView(mouse_statics,&mouse_again)==S_OK && mouse_again==mouse_device);
    IMouseDevice_Release(mouse_again); IMouseDevice_Release(mouse_device); IMouseDeviceStatics_Release(mouse_statics);
    CHECK(get_factory(L"Windows.Foundation.Metadata.ApiInformation",&IID_IApiInformationStatics,(void **)&api)==S_OK);
    WindowsCreateString(L"Windows.Foundation.UniversalApiContract",39,&type);
    CHECK(IApiInformationStatics_IsApiContractPresentByMajorAndMinor(api,type,5,0,&flag)==S_OK && flag);
    CHECK(IApiInformationStatics_IsApiContractPresentByMajorAndMinor(api,type,65535,0,&flag)==S_OK && !flag);
    WindowsDeleteString(type);
    WindowsCreateString(L"Windows.UI.Core.CoreWindow",26,&type); WindowsCreateString(L"ActivationMode",14,&property);
    CHECK(IApiInformationStatics_IsReadOnlyPropertyPresent(api,type,property,&flag)==S_OK && !flag);
    WindowsDeleteString(property); WindowsCreateString(L"Bounds",6,&property);
    CHECK(IApiInformationStatics_IsReadOnlyPropertyPresent(api,type,property,&flag)==S_OK && flag);
    WindowsDeleteString(type); WindowsDeleteString(property); IApiInformationStatics_Release(api);
    CHECK(get_factory(L"Windows.ApplicationModel.Preview.Holographic.HolographicApplicationPreview",&IID_IHolographicApplicationPreviewStatics,(void **)&holographic)==S_OK);
    CHECK(IHolographicApplicationPreviewStatics_IsCurrentViewPresentedOnHolographicDisplay(holographic,&flag)==S_OK && !flag);
    IHolographicApplicationPreviewStatics_Release(holographic);
    test_dispatcher(window,dispatcher);
}
