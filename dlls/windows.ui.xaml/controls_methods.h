static HRESULT WINAPI nodefn_IDependencyObject_GetValue(struct node_iface *iface, IInspectable * dp, IInspectable ** result__)
{
    FIXME("IDependencyObject.GetValue not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IDependencyObject_SetValue(struct node_iface *iface, IInspectable * dp, IInspectable * value)
{
    FIXME("IDependencyObject.SetValue not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IDependencyObject_ClearValue(struct node_iface *iface, IInspectable * dp)
{
    FIXME("IDependencyObject.ClearValue not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IDependencyObject_ReadLocalValue(struct node_iface *iface, IInspectable * dp, IInspectable ** result__)
{
    FIXME("IDependencyObject.ReadLocalValue not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IDependencyObject_GetAnimationBaseValue(struct node_iface *iface, IInspectable * dp, IInspectable ** result__)
{
    FIXME("IDependencyObject.GetAnimationBaseValue not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IDependencyObject_Dispatcher(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Dispatcher", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_DesiredSize(struct node_iface *iface, Size * result__)
{
    return node_get(iface, L"DesiredSize", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_AllowDrop(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"AllowDrop", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetAllowDrop(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"AllowDrop", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_Opacity(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"Opacity", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetOpacity(struct node_iface *iface, DOUBLE value)
{
    return node_set(iface, L"Opacity", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_Clip(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Clip", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetClip(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Clip", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_RenderTransform(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"RenderTransform", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetRenderTransform(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"RenderTransform", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_Projection(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Projection", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetProjection(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Projection", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_RenderTransformOrigin(struct node_iface *iface, Point * result__)
{
    return node_get(iface, L"RenderTransformOrigin", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetRenderTransformOrigin(struct node_iface *iface, Point value)
{
    return node_set(iface, L"RenderTransformOrigin", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_IsHitTestVisible(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsHitTestVisible", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetIsHitTestVisible(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsHitTestVisible", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_Visibility(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"Visibility", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetVisibility(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"Visibility", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_RenderSize(struct node_iface *iface, Size * result__)
{
    return node_get(iface, L"RenderSize", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_UseLayoutRounding(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"UseLayoutRounding", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetUseLayoutRounding(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"UseLayoutRounding", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_Transitions(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Transitions", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetTransitions(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Transitions", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_CacheMode(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"CacheMode", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetCacheMode(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"CacheMode", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_IsTapEnabled(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsTapEnabled", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetIsTapEnabled(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsTapEnabled", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_IsDoubleTapEnabled(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsDoubleTapEnabled", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetIsDoubleTapEnabled(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsDoubleTapEnabled", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_IsRightTapEnabled(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsRightTapEnabled", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetIsRightTapEnabled(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsRightTapEnabled", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_IsHoldingEnabled(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsHoldingEnabled", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetIsHoldingEnabled(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsHoldingEnabled", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_ManipulationMode(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"ManipulationMode", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_SetManipulationMode(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"ManipulationMode", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUIElement_PointerCaptures(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"PointerCaptures", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUIElement_KeyUp(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"KeyUp", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveKeyUp(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"KeyUp", token);
}
static HRESULT WINAPI nodefn_IUIElement_KeyDown(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"KeyDown", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveKeyDown(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"KeyDown", token);
}
static HRESULT WINAPI nodefn_IUIElement_GotFocus(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"GotFocus", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveGotFocus(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"GotFocus", token);
}
static HRESULT WINAPI nodefn_IUIElement_LostFocus(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"LostFocus", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveLostFocus(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"LostFocus", token);
}
static HRESULT WINAPI nodefn_IUIElement_DragEnter(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"DragEnter", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveDragEnter(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"DragEnter", token);
}
static HRESULT WINAPI nodefn_IUIElement_DragLeave(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"DragLeave", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveDragLeave(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"DragLeave", token);
}
static HRESULT WINAPI nodefn_IUIElement_DragOver(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"DragOver", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveDragOver(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"DragOver", token);
}
static HRESULT WINAPI nodefn_IUIElement_Drop(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"Drop", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveDrop(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"Drop", token);
}
static HRESULT WINAPI nodefn_IUIElement_PointerPressed(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"PointerPressed", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemovePointerPressed(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"PointerPressed", token);
}
static HRESULT WINAPI nodefn_IUIElement_PointerMoved(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"PointerMoved", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemovePointerMoved(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"PointerMoved", token);
}
static HRESULT WINAPI nodefn_IUIElement_PointerReleased(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"PointerReleased", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemovePointerReleased(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"PointerReleased", token);
}
static HRESULT WINAPI nodefn_IUIElement_PointerEntered(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"PointerEntered", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemovePointerEntered(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"PointerEntered", token);
}
static HRESULT WINAPI nodefn_IUIElement_PointerExited(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"PointerExited", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemovePointerExited(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"PointerExited", token);
}
static HRESULT WINAPI nodefn_IUIElement_PointerCaptureLost(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"PointerCaptureLost", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemovePointerCaptureLost(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"PointerCaptureLost", token);
}
static HRESULT WINAPI nodefn_IUIElement_PointerCanceled(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"PointerCanceled", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemovePointerCanceled(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"PointerCanceled", token);
}
static HRESULT WINAPI nodefn_IUIElement_PointerWheelChanged(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"PointerWheelChanged", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemovePointerWheelChanged(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"PointerWheelChanged", token);
}
static HRESULT WINAPI nodefn_IUIElement_Tapped(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"Tapped", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveTapped(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"Tapped", token);
}
static HRESULT WINAPI nodefn_IUIElement_DoubleTapped(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"DoubleTapped", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveDoubleTapped(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"DoubleTapped", token);
}
static HRESULT WINAPI nodefn_IUIElement_Holding(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"Holding", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveHolding(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"Holding", token);
}
static HRESULT WINAPI nodefn_IUIElement_RightTapped(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"RightTapped", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveRightTapped(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"RightTapped", token);
}
static HRESULT WINAPI nodefn_IUIElement_ManipulationStarting(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"ManipulationStarting", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveManipulationStarting(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"ManipulationStarting", token);
}
static HRESULT WINAPI nodefn_IUIElement_ManipulationInertiaStarting(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"ManipulationInertiaStarting", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveManipulationInertiaStarting(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"ManipulationInertiaStarting", token);
}
static HRESULT WINAPI nodefn_IUIElement_ManipulationStarted(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"ManipulationStarted", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveManipulationStarted(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"ManipulationStarted", token);
}
static HRESULT WINAPI nodefn_IUIElement_ManipulationDelta(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"ManipulationDelta", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveManipulationDelta(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"ManipulationDelta", token);
}
static HRESULT WINAPI nodefn_IUIElement_ManipulationCompleted(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"ManipulationCompleted", handler, result__);
}
static HRESULT WINAPI nodefn_IUIElement_RemoveManipulationCompleted(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"ManipulationCompleted", token);
}
static HRESULT WINAPI nodefn_IUIElement_Measure(struct node_iface *iface, Size availablesize)
{
    return node_layout(iface);
}
static HRESULT WINAPI nodefn_IUIElement_Arrange(struct node_iface *iface, Rect finalrect)
{
    return node_layout(iface);
}
static HRESULT WINAPI nodefn_IUIElement_CapturePointer(struct node_iface *iface, IInspectable * value, boolean * result__)
{
    FIXME("IUIElement.CapturePointer not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IUIElement_ReleasePointerCapture(struct node_iface *iface, IInspectable * value)
{
    FIXME("IUIElement.ReleasePointerCapture not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IUIElement_ReleasePointerCaptures(struct node_iface *iface)
{
    FIXME("IUIElement.ReleasePointerCaptures not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IUIElement_AddHandler(struct node_iface *iface, IInspectable * routedevent, IInspectable * handler, boolean handledeventstoo)
{
    FIXME("IUIElement.AddHandler not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IUIElement_RemoveHandler(struct node_iface *iface, IInspectable * routedevent, IInspectable * handler)
{
    FIXME("IUIElement.RemoveHandler not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IUIElement_TransformToVisual(struct node_iface *iface, IInspectable * visual, IInspectable ** result__)
{
    FIXME("IUIElement.TransformToVisual not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IUIElement_InvalidateMeasure(struct node_iface *iface)
{
    return node_layout(iface);
}
static HRESULT WINAPI nodefn_IUIElement_InvalidateArrange(struct node_iface *iface)
{
    return node_layout(iface);
}
static HRESULT WINAPI nodefn_IUIElement_UpdateLayout(struct node_iface *iface)
{
    return node_layout(iface);
}
static HRESULT WINAPI nodefn_IFrameworkElement_Triggers(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Triggers", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_Resources(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Resources", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetResources(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Resources", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_Tag(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Tag", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetTag(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Tag", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_Language(struct node_iface *iface, HSTRING * result__)
{
    return node_get(iface, L"Language", NODE_STRING, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetLanguage(struct node_iface *iface, HSTRING value)
{
    return node_set(iface, L"Language", NODE_STRING, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_ActualWidth(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"ActualWidth", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_ActualHeight(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"ActualHeight", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_Width(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"Width", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetWidth(struct node_iface *iface, DOUBLE value)
{
    return node_set(iface, L"Width", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_Height(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"Height", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetHeight(struct node_iface *iface, DOUBLE value)
{
    return node_set(iface, L"Height", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_MinWidth(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"MinWidth", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetMinWidth(struct node_iface *iface, DOUBLE value)
{
    return node_set(iface, L"MinWidth", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_MaxWidth(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"MaxWidth", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetMaxWidth(struct node_iface *iface, DOUBLE value)
{
    return node_set(iface, L"MaxWidth", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_MinHeight(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"MinHeight", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetMinHeight(struct node_iface *iface, DOUBLE value)
{
    return node_set(iface, L"MinHeight", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_MaxHeight(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"MaxHeight", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetMaxHeight(struct node_iface *iface, DOUBLE value)
{
    return node_set(iface, L"MaxHeight", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_HorizontalAlignment(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"HorizontalAlignment", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetHorizontalAlignment(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"HorizontalAlignment", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_VerticalAlignment(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"VerticalAlignment", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetVerticalAlignment(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"VerticalAlignment", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_Margin(struct node_iface *iface, struct node_thickness * result__)
{
    return node_get(iface, L"Margin", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetMargin(struct node_iface *iface, struct node_thickness value)
{
    return node_set(iface, L"Margin", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_Name(struct node_iface *iface, HSTRING * result__)
{
    return node_get(iface, L"Name", NODE_STRING, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetName(struct node_iface *iface, HSTRING value)
{
    return node_set(iface, L"Name", NODE_STRING, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_BaseUri(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"BaseUri", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_DataContext(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"DataContext", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetDataContext(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"DataContext", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_Style(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Style", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetStyle(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Style", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_Parent(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Parent", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_FlowDirection(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"FlowDirection", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetFlowDirection(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"FlowDirection", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFrameworkElement_Loaded(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"Loaded", handler, result__);
}
static HRESULT WINAPI nodefn_IFrameworkElement_RemoveLoaded(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"Loaded", token);
}
static HRESULT WINAPI nodefn_IFrameworkElement_Unloaded(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"Unloaded", handler, result__);
}
static HRESULT WINAPI nodefn_IFrameworkElement_RemoveUnloaded(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"Unloaded", token);
}
static HRESULT WINAPI nodefn_IFrameworkElement_SizeChanged(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"SizeChanged", handler, result__);
}
static HRESULT WINAPI nodefn_IFrameworkElement_RemoveSizeChanged(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"SizeChanged", token);
}
static HRESULT WINAPI nodefn_IFrameworkElement_LayoutUpdated(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"LayoutUpdated", handler, result__);
}
static HRESULT WINAPI nodefn_IFrameworkElement_RemoveLayoutUpdated(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"LayoutUpdated", token);
}
static HRESULT WINAPI nodefn_IFrameworkElement_FindName(struct node_iface *iface, HSTRING name, IInspectable ** result__)
{
    return node_find_name(iface, name, result__);
}
static HRESULT WINAPI nodefn_IFrameworkElement_SetBinding(struct node_iface *iface, IInspectable * dp, IInspectable * binding)
{
    FIXME("IFrameworkElement.SetBinding not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_IControl_FontSize(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"FontSize", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetFontSize(struct node_iface *iface, DOUBLE value)
{
    return node_set(iface, L"FontSize", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_FontFamily(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"FontFamily", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetFontFamily(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"FontFamily", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_FontWeight(struct node_iface *iface, struct node_weight * result__)
{
    return node_get(iface, L"FontWeight", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetFontWeight(struct node_iface *iface, struct node_weight value)
{
    return node_set(iface, L"FontWeight", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_FontStyle(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"FontStyle", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetFontStyle(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"FontStyle", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_FontStretch(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"FontStretch", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetFontStretch(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"FontStretch", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_CharacterSpacing(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"CharacterSpacing", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetCharacterSpacing(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"CharacterSpacing", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_Foreground(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Foreground", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetForeground(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Foreground", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_IsTabStop(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsTabStop", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetIsTabStop(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsTabStop", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_IsEnabled(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsEnabled", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetIsEnabled(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsEnabled", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_TabIndex(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"TabIndex", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetTabIndex(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"TabIndex", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_TabNavigation(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"TabNavigation", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetTabNavigation(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"TabNavigation", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_Template(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Template", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetTemplate(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Template", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_Padding(struct node_iface *iface, struct node_thickness * result__)
{
    return node_get(iface, L"Padding", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetPadding(struct node_iface *iface, struct node_thickness value)
{
    return node_set(iface, L"Padding", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_HorizontalContentAlignment(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"HorizontalContentAlignment", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetHorizontalContentAlignment(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"HorizontalContentAlignment", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_VerticalContentAlignment(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"VerticalContentAlignment", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetVerticalContentAlignment(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"VerticalContentAlignment", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_Background(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Background", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetBackground(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Background", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_BorderThickness(struct node_iface *iface, struct node_thickness * result__)
{
    return node_get(iface, L"BorderThickness", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetBorderThickness(struct node_iface *iface, struct node_thickness value)
{
    return node_set(iface, L"BorderThickness", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_BorderBrush(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"BorderBrush", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_SetBorderBrush(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"BorderBrush", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IControl_FocusState(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"FocusState", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_IsEnabledChanged(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"IsEnabledChanged", handler, result__);
}
static HRESULT WINAPI nodefn_IControl_RemoveIsEnabledChanged(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"IsEnabledChanged", token);
}
static HRESULT WINAPI nodefn_IControl_ApplyTemplate(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"ApplyTemplate", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IControl_Focus(struct node_iface *iface, INT32 value, boolean * result__)
{
    return node_focus(iface, value, result__);
}
static HRESULT WINAPI nodefn_IPage_Frame(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Frame", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IPage_NavigationCacheMode(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"NavigationCacheMode", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IPage_SetNavigationCacheMode(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"NavigationCacheMode", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IPage_TopAppBar(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"TopAppBar", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IPage_SetTopAppBar(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"TopAppBar", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IPage_BottomAppBar(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"BottomAppBar", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IPage_SetBottomAppBar(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"BottomAppBar", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IUserControl_Content(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Content", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IUserControl_SetContent(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Content", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IContentControl_Content(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Content", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IContentControl_SetContent(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Content", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IContentControl_ContentTemplate(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"ContentTemplate", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IContentControl_SetContentTemplate(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"ContentTemplate", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IContentControl_ContentTemplateSelector(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"ContentTemplateSelector", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IContentControl_SetContentTemplateSelector(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"ContentTemplateSelector", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IContentControl_ContentTransitions(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"ContentTransitions", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IContentControl_SetContentTransitions(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"ContentTransitions", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IPanel_Children(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Children", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IPanel_Background(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Background", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IPanel_SetBackground(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Background", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IPanel_IsItemsHost(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsItemsHost", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IPanel_ChildrenTransitions(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"ChildrenTransitions", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IPanel_SetChildrenTransitions(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"ChildrenTransitions", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ISwapChainPanel_CompositionScaleX(struct node_iface *iface, FLOAT * result__)
{
    return node_get(iface, L"CompositionScaleX", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ISwapChainPanel_CompositionScaleY(struct node_iface *iface, FLOAT * result__)
{
    return node_get(iface, L"CompositionScaleY", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ISwapChainPanel_CompositionScaleChanged(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"CompositionScaleChanged", handler, result__);
}
static HRESULT WINAPI nodefn_ISwapChainPanel_RemoveCompositionScaleChanged(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"CompositionScaleChanged", token);
}
static HRESULT WINAPI nodefn_ISwapChainPanel_CreateCoreIndependentInputSource(struct node_iface *iface, INT32 devicetypes, IInspectable ** result__)
{
    return node_create_input(iface, devicetypes, result__);
}
static HRESULT WINAPI nodefn_ITextBox_Text(struct node_iface *iface, HSTRING * result__)
{
    return node_get(iface, L"Text", NODE_STRING, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetText(struct node_iface *iface, HSTRING value)
{
    return node_set(iface, L"Text", NODE_STRING, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_SelectedText(struct node_iface *iface, HSTRING * result__)
{
    return node_get(iface, L"SelectedText", NODE_STRING, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetSelectedText(struct node_iface *iface, HSTRING value)
{
    return node_set(iface, L"SelectedText", NODE_STRING, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_SelectionLength(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"SelectionLength", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetSelectionLength(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"SelectionLength", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_SelectionStart(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"SelectionStart", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetSelectionStart(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"SelectionStart", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_MaxLength(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"MaxLength", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetMaxLength(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"MaxLength", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_IsReadOnly(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsReadOnly", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetIsReadOnly(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsReadOnly", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_AcceptsReturn(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"AcceptsReturn", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetAcceptsReturn(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"AcceptsReturn", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_TextAlignment(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"TextAlignment", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetTextAlignment(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"TextAlignment", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_TextWrapping(struct node_iface *iface, INT32 * result__)
{
    return node_get(iface, L"TextWrapping", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetTextWrapping(struct node_iface *iface, INT32 value)
{
    return node_set(iface, L"TextWrapping", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_IsSpellCheckEnabled(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsSpellCheckEnabled", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetIsSpellCheckEnabled(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsSpellCheckEnabled", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_IsTextPredictionEnabled(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsTextPredictionEnabled", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetIsTextPredictionEnabled(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsTextPredictionEnabled", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_InputScope(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"InputScope", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox_SetInputScope(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"InputScope", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox_TextChanged(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"TextChanged", handler, result__);
}
static HRESULT WINAPI nodefn_ITextBox_RemoveTextChanged(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"TextChanged", token);
}
static HRESULT WINAPI nodefn_ITextBox_SelectionChanged(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"SelectionChanged", handler, result__);
}
static HRESULT WINAPI nodefn_ITextBox_RemoveSelectionChanged(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"SelectionChanged", token);
}
static HRESULT WINAPI nodefn_ITextBox_ContextMenuOpening(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"ContextMenuOpening", handler, result__);
}
static HRESULT WINAPI nodefn_ITextBox_RemoveContextMenuOpening(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"ContextMenuOpening", token);
}
static HRESULT WINAPI nodefn_ITextBox_Select(struct node_iface *iface, INT32 start, INT32 length)
{
    return node_select(iface, start, length);
}
static HRESULT WINAPI nodefn_ITextBox_SelectAll(struct node_iface *iface)
{
    FIXME("ITextBox.SelectAll not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_ITextBox_GetRectFromCharacterIndex(struct node_iface *iface, INT32 charindex, boolean trailingedge, Rect * result__)
{
    FIXME("ITextBox.GetRectFromCharacterIndex not implemented.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI nodefn_ITextBox2_Header(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Header", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox2_SetHeader(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Header", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox2_HeaderTemplate(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"HeaderTemplate", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox2_SetHeaderTemplate(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"HeaderTemplate", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox2_PlaceholderText(struct node_iface *iface, HSTRING * result__)
{
    return node_get(iface, L"PlaceholderText", NODE_STRING, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox2_SetPlaceholderText(struct node_iface *iface, HSTRING value)
{
    return node_set(iface, L"PlaceholderText", NODE_STRING, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox2_SelectionHighlightColor(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"SelectionHighlightColor", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox2_SetSelectionHighlightColor(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"SelectionHighlightColor", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox2_PreventKeyboardDisplayOnProgrammaticFocus(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"PreventKeyboardDisplayOnProgrammaticFocus", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox2_SetPreventKeyboardDisplayOnProgrammaticFocus(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"PreventKeyboardDisplayOnProgrammaticFocus", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox2_IsColorFontEnabled(struct node_iface *iface, boolean * result__)
{
    return node_get(iface, L"IsColorFontEnabled", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ITextBox2_SetIsColorFontEnabled(struct node_iface *iface, boolean value)
{
    return node_set(iface, L"IsColorFontEnabled", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_ITextBox2_Paste(struct node_iface *iface, IInspectable * handler, EventRegistrationToken * result__)
{
    return node_event_add(iface, L"Paste", handler, result__);
}
static HRESULT WINAPI nodefn_ITextBox2_RemovePaste(struct node_iface *iface, EventRegistrationToken token)
{
    return node_event_remove(iface, L"Paste", token);
}
static HRESULT WINAPI nodefn_ISolidColorBrush_Color(struct node_iface *iface, Color * result__)
{
    return node_get(iface, L"Color", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_ISolidColorBrush_SetColor(struct node_iface *iface, Color value)
{
    return node_set(iface, L"Color", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IFontFamily_Source(struct node_iface *iface, HSTRING * result__)
{
    return node_get(iface, L"Source", NODE_STRING, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IBrush_Opacity(struct node_iface *iface, DOUBLE * result__)
{
    return node_get(iface, L"Opacity", NODE_DATA, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IBrush_SetOpacity(struct node_iface *iface, DOUBLE value)
{
    return node_set(iface, L"Opacity", NODE_DATA, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IBrush_Transform(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"Transform", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IBrush_SetTransform(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"Transform", NODE_OBJECT, &value, sizeof(value));
}
static HRESULT WINAPI nodefn_IBrush_RelativeTransform(struct node_iface *iface, IInspectable ** result__)
{
    return node_get(iface, L"RelativeTransform", NODE_OBJECT, result__, sizeof(*result__));
}
static HRESULT WINAPI nodefn_IBrush_SetRelativeTransform(struct node_iface *iface, IInspectable * value)
{
    return node_set(iface, L"RelativeTransform", NODE_OBJECT, &value, sizeof(value));
}
static const struct node_IDependencyObject_vtbl node_IDependencyObject_vtbl = {NODE_BASE, nodefn_IDependencyObject_GetValue, nodefn_IDependencyObject_SetValue, nodefn_IDependencyObject_ClearValue, nodefn_IDependencyObject_ReadLocalValue, nodefn_IDependencyObject_GetAnimationBaseValue, nodefn_IDependencyObject_Dispatcher};
static const struct node_IUIElement_vtbl node_IUIElement_vtbl = {NODE_BASE, nodefn_IUIElement_DesiredSize, nodefn_IUIElement_AllowDrop, nodefn_IUIElement_SetAllowDrop, nodefn_IUIElement_Opacity, nodefn_IUIElement_SetOpacity, nodefn_IUIElement_Clip, nodefn_IUIElement_SetClip, nodefn_IUIElement_RenderTransform, nodefn_IUIElement_SetRenderTransform, nodefn_IUIElement_Projection, nodefn_IUIElement_SetProjection, nodefn_IUIElement_RenderTransformOrigin, nodefn_IUIElement_SetRenderTransformOrigin, nodefn_IUIElement_IsHitTestVisible, nodefn_IUIElement_SetIsHitTestVisible, nodefn_IUIElement_Visibility, nodefn_IUIElement_SetVisibility, nodefn_IUIElement_RenderSize, nodefn_IUIElement_UseLayoutRounding, nodefn_IUIElement_SetUseLayoutRounding, nodefn_IUIElement_Transitions, nodefn_IUIElement_SetTransitions, nodefn_IUIElement_CacheMode, nodefn_IUIElement_SetCacheMode, nodefn_IUIElement_IsTapEnabled, nodefn_IUIElement_SetIsTapEnabled, nodefn_IUIElement_IsDoubleTapEnabled, nodefn_IUIElement_SetIsDoubleTapEnabled, nodefn_IUIElement_IsRightTapEnabled, nodefn_IUIElement_SetIsRightTapEnabled, nodefn_IUIElement_IsHoldingEnabled, nodefn_IUIElement_SetIsHoldingEnabled, nodefn_IUIElement_ManipulationMode, nodefn_IUIElement_SetManipulationMode, nodefn_IUIElement_PointerCaptures, nodefn_IUIElement_KeyUp, nodefn_IUIElement_RemoveKeyUp, nodefn_IUIElement_KeyDown, nodefn_IUIElement_RemoveKeyDown, nodefn_IUIElement_GotFocus, nodefn_IUIElement_RemoveGotFocus, nodefn_IUIElement_LostFocus, nodefn_IUIElement_RemoveLostFocus, nodefn_IUIElement_DragEnter, nodefn_IUIElement_RemoveDragEnter, nodefn_IUIElement_DragLeave, nodefn_IUIElement_RemoveDragLeave, nodefn_IUIElement_DragOver, nodefn_IUIElement_RemoveDragOver, nodefn_IUIElement_Drop, nodefn_IUIElement_RemoveDrop, nodefn_IUIElement_PointerPressed, nodefn_IUIElement_RemovePointerPressed, nodefn_IUIElement_PointerMoved, nodefn_IUIElement_RemovePointerMoved, nodefn_IUIElement_PointerReleased, nodefn_IUIElement_RemovePointerReleased, nodefn_IUIElement_PointerEntered, nodefn_IUIElement_RemovePointerEntered, nodefn_IUIElement_PointerExited, nodefn_IUIElement_RemovePointerExited, nodefn_IUIElement_PointerCaptureLost, nodefn_IUIElement_RemovePointerCaptureLost, nodefn_IUIElement_PointerCanceled, nodefn_IUIElement_RemovePointerCanceled, nodefn_IUIElement_PointerWheelChanged, nodefn_IUIElement_RemovePointerWheelChanged, nodefn_IUIElement_Tapped, nodefn_IUIElement_RemoveTapped, nodefn_IUIElement_DoubleTapped, nodefn_IUIElement_RemoveDoubleTapped, nodefn_IUIElement_Holding, nodefn_IUIElement_RemoveHolding, nodefn_IUIElement_RightTapped, nodefn_IUIElement_RemoveRightTapped, nodefn_IUIElement_ManipulationStarting, nodefn_IUIElement_RemoveManipulationStarting, nodefn_IUIElement_ManipulationInertiaStarting, nodefn_IUIElement_RemoveManipulationInertiaStarting, nodefn_IUIElement_ManipulationStarted, nodefn_IUIElement_RemoveManipulationStarted, nodefn_IUIElement_ManipulationDelta, nodefn_IUIElement_RemoveManipulationDelta, nodefn_IUIElement_ManipulationCompleted, nodefn_IUIElement_RemoveManipulationCompleted, nodefn_IUIElement_Measure, nodefn_IUIElement_Arrange, nodefn_IUIElement_CapturePointer, nodefn_IUIElement_ReleasePointerCapture, nodefn_IUIElement_ReleasePointerCaptures, nodefn_IUIElement_AddHandler, nodefn_IUIElement_RemoveHandler, nodefn_IUIElement_TransformToVisual, nodefn_IUIElement_InvalidateMeasure, nodefn_IUIElement_InvalidateArrange, nodefn_IUIElement_UpdateLayout};
static const struct node_IFrameworkElement_vtbl node_IFrameworkElement_vtbl = {NODE_BASE, nodefn_IFrameworkElement_Triggers, nodefn_IFrameworkElement_Resources, nodefn_IFrameworkElement_SetResources, nodefn_IFrameworkElement_Tag, nodefn_IFrameworkElement_SetTag, nodefn_IFrameworkElement_Language, nodefn_IFrameworkElement_SetLanguage, nodefn_IFrameworkElement_ActualWidth, nodefn_IFrameworkElement_ActualHeight, nodefn_IFrameworkElement_Width, nodefn_IFrameworkElement_SetWidth, nodefn_IFrameworkElement_Height, nodefn_IFrameworkElement_SetHeight, nodefn_IFrameworkElement_MinWidth, nodefn_IFrameworkElement_SetMinWidth, nodefn_IFrameworkElement_MaxWidth, nodefn_IFrameworkElement_SetMaxWidth, nodefn_IFrameworkElement_MinHeight, nodefn_IFrameworkElement_SetMinHeight, nodefn_IFrameworkElement_MaxHeight, nodefn_IFrameworkElement_SetMaxHeight, nodefn_IFrameworkElement_HorizontalAlignment, nodefn_IFrameworkElement_SetHorizontalAlignment, nodefn_IFrameworkElement_VerticalAlignment, nodefn_IFrameworkElement_SetVerticalAlignment, nodefn_IFrameworkElement_Margin, nodefn_IFrameworkElement_SetMargin, nodefn_IFrameworkElement_Name, nodefn_IFrameworkElement_SetName, nodefn_IFrameworkElement_BaseUri, nodefn_IFrameworkElement_DataContext, nodefn_IFrameworkElement_SetDataContext, nodefn_IFrameworkElement_Style, nodefn_IFrameworkElement_SetStyle, nodefn_IFrameworkElement_Parent, nodefn_IFrameworkElement_FlowDirection, nodefn_IFrameworkElement_SetFlowDirection, nodefn_IFrameworkElement_Loaded, nodefn_IFrameworkElement_RemoveLoaded, nodefn_IFrameworkElement_Unloaded, nodefn_IFrameworkElement_RemoveUnloaded, nodefn_IFrameworkElement_SizeChanged, nodefn_IFrameworkElement_RemoveSizeChanged, nodefn_IFrameworkElement_LayoutUpdated, nodefn_IFrameworkElement_RemoveLayoutUpdated, nodefn_IFrameworkElement_FindName, nodefn_IFrameworkElement_SetBinding};
static const struct node_IControl_vtbl node_IControl_vtbl = {NODE_BASE, nodefn_IControl_FontSize, nodefn_IControl_SetFontSize, nodefn_IControl_FontFamily, nodefn_IControl_SetFontFamily, nodefn_IControl_FontWeight, nodefn_IControl_SetFontWeight, nodefn_IControl_FontStyle, nodefn_IControl_SetFontStyle, nodefn_IControl_FontStretch, nodefn_IControl_SetFontStretch, nodefn_IControl_CharacterSpacing, nodefn_IControl_SetCharacterSpacing, nodefn_IControl_Foreground, nodefn_IControl_SetForeground, nodefn_IControl_IsTabStop, nodefn_IControl_SetIsTabStop, nodefn_IControl_IsEnabled, nodefn_IControl_SetIsEnabled, nodefn_IControl_TabIndex, nodefn_IControl_SetTabIndex, nodefn_IControl_TabNavigation, nodefn_IControl_SetTabNavigation, nodefn_IControl_Template, nodefn_IControl_SetTemplate, nodefn_IControl_Padding, nodefn_IControl_SetPadding, nodefn_IControl_HorizontalContentAlignment, nodefn_IControl_SetHorizontalContentAlignment, nodefn_IControl_VerticalContentAlignment, nodefn_IControl_SetVerticalContentAlignment, nodefn_IControl_Background, nodefn_IControl_SetBackground, nodefn_IControl_BorderThickness, nodefn_IControl_SetBorderThickness, nodefn_IControl_BorderBrush, nodefn_IControl_SetBorderBrush, nodefn_IControl_FocusState, nodefn_IControl_IsEnabledChanged, nodefn_IControl_RemoveIsEnabledChanged, nodefn_IControl_ApplyTemplate, nodefn_IControl_Focus};
static const struct node_IPage_vtbl node_IPage_vtbl = {NODE_BASE, nodefn_IPage_Frame, nodefn_IPage_NavigationCacheMode, nodefn_IPage_SetNavigationCacheMode, nodefn_IPage_TopAppBar, nodefn_IPage_SetTopAppBar, nodefn_IPage_BottomAppBar, nodefn_IPage_SetBottomAppBar};
static const struct node_IUserControl_vtbl node_IUserControl_vtbl = {NODE_BASE, nodefn_IUserControl_Content, nodefn_IUserControl_SetContent};
static const struct node_IContentControl_vtbl node_IContentControl_vtbl = {NODE_BASE, nodefn_IContentControl_Content, nodefn_IContentControl_SetContent, nodefn_IContentControl_ContentTemplate, nodefn_IContentControl_SetContentTemplate, nodefn_IContentControl_ContentTemplateSelector, nodefn_IContentControl_SetContentTemplateSelector, nodefn_IContentControl_ContentTransitions, nodefn_IContentControl_SetContentTransitions};
static const struct node_IPanel_vtbl node_IPanel_vtbl = {NODE_BASE, nodefn_IPanel_Children, nodefn_IPanel_Background, nodefn_IPanel_SetBackground, nodefn_IPanel_IsItemsHost, nodefn_IPanel_ChildrenTransitions, nodefn_IPanel_SetChildrenTransitions};
static const struct node_ICanvas_vtbl node_ICanvas_vtbl = {NODE_BASE, };
static const struct node_ISwapChainPanel_vtbl node_ISwapChainPanel_vtbl = {NODE_BASE, nodefn_ISwapChainPanel_CompositionScaleX, nodefn_ISwapChainPanel_CompositionScaleY, nodefn_ISwapChainPanel_CompositionScaleChanged, nodefn_ISwapChainPanel_RemoveCompositionScaleChanged, nodefn_ISwapChainPanel_CreateCoreIndependentInputSource};
static const struct node_ITextBox_vtbl node_ITextBox_vtbl = {NODE_BASE, nodefn_ITextBox_Text, nodefn_ITextBox_SetText, nodefn_ITextBox_SelectedText, nodefn_ITextBox_SetSelectedText, nodefn_ITextBox_SelectionLength, nodefn_ITextBox_SetSelectionLength, nodefn_ITextBox_SelectionStart, nodefn_ITextBox_SetSelectionStart, nodefn_ITextBox_MaxLength, nodefn_ITextBox_SetMaxLength, nodefn_ITextBox_IsReadOnly, nodefn_ITextBox_SetIsReadOnly, nodefn_ITextBox_AcceptsReturn, nodefn_ITextBox_SetAcceptsReturn, nodefn_ITextBox_TextAlignment, nodefn_ITextBox_SetTextAlignment, nodefn_ITextBox_TextWrapping, nodefn_ITextBox_SetTextWrapping, nodefn_ITextBox_IsSpellCheckEnabled, nodefn_ITextBox_SetIsSpellCheckEnabled, nodefn_ITextBox_IsTextPredictionEnabled, nodefn_ITextBox_SetIsTextPredictionEnabled, nodefn_ITextBox_InputScope, nodefn_ITextBox_SetInputScope, nodefn_ITextBox_TextChanged, nodefn_ITextBox_RemoveTextChanged, nodefn_ITextBox_SelectionChanged, nodefn_ITextBox_RemoveSelectionChanged, nodefn_ITextBox_ContextMenuOpening, nodefn_ITextBox_RemoveContextMenuOpening, nodefn_ITextBox_Select, nodefn_ITextBox_SelectAll, nodefn_ITextBox_GetRectFromCharacterIndex};
static const struct node_ITextBox2_vtbl node_ITextBox2_vtbl = {NODE_BASE, nodefn_ITextBox2_Header, nodefn_ITextBox2_SetHeader, nodefn_ITextBox2_HeaderTemplate, nodefn_ITextBox2_SetHeaderTemplate, nodefn_ITextBox2_PlaceholderText, nodefn_ITextBox2_SetPlaceholderText, nodefn_ITextBox2_SelectionHighlightColor, nodefn_ITextBox2_SetSelectionHighlightColor, nodefn_ITextBox2_PreventKeyboardDisplayOnProgrammaticFocus, nodefn_ITextBox2_SetPreventKeyboardDisplayOnProgrammaticFocus, nodefn_ITextBox2_IsColorFontEnabled, nodefn_ITextBox2_SetIsColorFontEnabled, nodefn_ITextBox2_Paste, nodefn_ITextBox2_RemovePaste};
static const struct node_IButton_vtbl node_IButton_vtbl = {NODE_BASE, };
static const struct node_ISolidColorBrush_vtbl node_ISolidColorBrush_vtbl = {NODE_BASE, nodefn_ISolidColorBrush_Color, nodefn_ISolidColorBrush_SetColor};
static const struct node_IFontFamily_vtbl node_IFontFamily_vtbl = {NODE_BASE, nodefn_IFontFamily_Source};
static const struct node_IBrush_vtbl node_IBrush_vtbl = {NODE_BASE, nodefn_IBrush_Opacity, nodefn_IBrush_SetOpacity, nodefn_IBrush_Transform, nodefn_IBrush_SetTransform, nodefn_IBrush_RelativeTransform, nodefn_IBrush_SetRelativeTransform};
static const void *const node_vtables[] = {&node_IDependencyObject_vtbl, &node_IUIElement_vtbl, &node_IFrameworkElement_vtbl, &node_IControl_vtbl, &node_IPage_vtbl, &node_IUserControl_vtbl, &node_IContentControl_vtbl, &node_IPanel_vtbl, &node_ICanvas_vtbl, &node_ISwapChainPanel_vtbl, &node_ITextBox_vtbl, &node_ITextBox2_vtbl, &node_IButton_vtbl, &node_ISolidColorBrush_vtbl, &node_IFontFamily_vtbl, &node_IBrush_vtbl};
