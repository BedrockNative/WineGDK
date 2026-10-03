/* WinRT Windows.UI.Text.Core.CoreTextEditContext Implementation
 *
 * Written by Weather
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

#include "editcontext.h"

WINE_DEFAULT_DEBUG_CHANNEL(textinput);

static inline struct core_text_edit_context *impl_from_ICoreTextEditContext( ICoreTextEditContext *iface )
{
    return CONTAINING_RECORD( iface, struct core_text_edit_context, ICoreTextEditContext_iface );
}

static HRESULT WINAPI core_text_edit_context_QueryInterface( ICoreTextEditContext *iface, REFIID iid, void **out )
{
    struct core_text_edit_context *impl = impl_from_ICoreTextEditContext( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_ICoreTextEditContext ))
    {
        *out = &impl->ICoreTextEditContext_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI core_text_edit_context_AddRef( ICoreTextEditContext *iface )
{
    struct core_text_edit_context *impl = impl_from_ICoreTextEditContext( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI core_text_edit_context_Release( ICoreTextEditContext *iface )
{
    struct core_text_edit_context *impl = impl_from_ICoreTextEditContext( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref)
    {
        unsigned int i;
        for (i = 0; i < ARRAY_SIZE(impl->events); ++i) winrt_event_clear(&impl->events[i]);
        WindowsDeleteString(impl->name);
        free(impl);
    }
    return ref;
}

static HRESULT WINAPI core_text_edit_context_GetIids( ICoreTextEditContext *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI core_text_edit_context_GetRuntimeClassName( ICoreTextEditContext *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI core_text_edit_context_GetTrustLevel( ICoreTextEditContext *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p not stub!\n", iface, trust_level );
    *trust_level = FullTrust;
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_get_Name(ICoreTextEditContext *iface, HSTRING *value)
{
    return WindowsDuplicateString(impl_from_ICoreTextEditContext(iface)->name, value);
}

static HRESULT WINAPI core_text_edit_context_put_Name(ICoreTextEditContext *iface, HSTRING value)
{
    struct core_text_edit_context *impl = impl_from_ICoreTextEditContext(iface);
    HSTRING copy;
    HRESULT hr = WindowsDuplicateString(value, &copy);
    if (FAILED(hr)) return hr;
    WindowsDeleteString(impl->name); impl->name = copy;
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_get_InputScope(ICoreTextEditContext *iface, CoreTextInputScope *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_ICoreTextEditContext(iface)->scope;
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_put_InputScope(ICoreTextEditContext *iface, CoreTextInputScope value)
{
    impl_from_ICoreTextEditContext(iface)->scope = value;
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_get_IsReadOnly(ICoreTextEditContext *iface, boolean *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_ICoreTextEditContext(iface)->read_only;
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_put_IsReadOnly(ICoreTextEditContext *iface, boolean value)
{
    impl_from_ICoreTextEditContext(iface)->read_only = value;
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_get_InputPaneDisplayPolicy(ICoreTextEditContext *iface, CoreTextInputPaneDisplayPolicy *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_ICoreTextEditContext(iface)->policy;
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_put_InputPaneDisplayPolicy(ICoreTextEditContext *iface, CoreTextInputPaneDisplayPolicy value)
{
    impl_from_ICoreTextEditContext(iface)->policy = value;
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_add_TextRequested(ICoreTextEditContext *iface, ITypedEventHandler_CoreTextEditContext_CoreTextTextRequestedEventArgs *handler, EventRegistrationToken *cookie)
{
    return winrt_event_add(&impl_from_ICoreTextEditContext(iface)->events[0], handler, cookie);
}

static HRESULT WINAPI core_text_edit_context_remove_TextRequested(ICoreTextEditContext *iface, EventRegistrationToken cookie)
{
    return winrt_event_remove(&impl_from_ICoreTextEditContext(iface)->events[0], cookie);
}

static HRESULT WINAPI core_text_edit_context_add_SelectionRequested(ICoreTextEditContext *iface, ITypedEventHandler_CoreTextEditContext_CoreTextSelectionRequestedEventArgs *handler, EventRegistrationToken *cookie)
{
    return winrt_event_add(&impl_from_ICoreTextEditContext(iface)->events[1], handler, cookie);
}

static HRESULT WINAPI core_text_edit_context_remove_SelectionRequested(ICoreTextEditContext *iface, EventRegistrationToken cookie)
{
    return winrt_event_remove(&impl_from_ICoreTextEditContext(iface)->events[1], cookie);
}

static HRESULT WINAPI core_text_edit_context_add_LayoutRequested(ICoreTextEditContext *iface, ITypedEventHandler_CoreTextEditContext_CoreTextLayoutRequestedEventArgs *handler, EventRegistrationToken *cookie)
{
    return winrt_event_add(&impl_from_ICoreTextEditContext(iface)->events[2], handler, cookie);
}

static HRESULT WINAPI core_text_edit_context_remove_LayoutRequested(ICoreTextEditContext *iface, EventRegistrationToken cookie)
{
    return winrt_event_remove(&impl_from_ICoreTextEditContext(iface)->events[2], cookie);
}

static HRESULT WINAPI core_text_edit_context_add_TextUpdating(ICoreTextEditContext *iface, ITypedEventHandler_CoreTextEditContext_CoreTextTextUpdatingEventArgs *handler, EventRegistrationToken *cookie)
{
    return winrt_event_add(&impl_from_ICoreTextEditContext(iface)->events[3], handler, cookie);
}

static HRESULT WINAPI core_text_edit_context_remove_TextUpdating(ICoreTextEditContext *iface, EventRegistrationToken cookie)
{
    return winrt_event_remove(&impl_from_ICoreTextEditContext(iface)->events[3], cookie);
}

static HRESULT WINAPI core_text_edit_context_add_SelectionUpdating(ICoreTextEditContext *iface, ITypedEventHandler_CoreTextEditContext_CoreTextSelectionUpdatingEventArgs *handler, EventRegistrationToken *cookie)
{
    return winrt_event_add(&impl_from_ICoreTextEditContext(iface)->events[4], handler, cookie);
}

static HRESULT WINAPI core_text_edit_context_remove_SelectionUpdating(ICoreTextEditContext *iface, EventRegistrationToken cookie)
{
    return winrt_event_remove(&impl_from_ICoreTextEditContext(iface)->events[4], cookie);
}

static HRESULT WINAPI core_text_edit_context_add_FormatUpdating(ICoreTextEditContext *iface, ITypedEventHandler_CoreTextEditContext_CoreTextFormatUpdatingEventArgs *handler, EventRegistrationToken *cookie)
{
    return winrt_event_add(&impl_from_ICoreTextEditContext(iface)->events[5], handler, cookie);
}

static HRESULT WINAPI core_text_edit_context_remove_FormatUpdating(ICoreTextEditContext *iface, EventRegistrationToken cookie)
{
    return winrt_event_remove(&impl_from_ICoreTextEditContext(iface)->events[5], cookie);
}

/* CompositionStarted (flattened form) */
static HRESULT WINAPI core_text_edit_context_add_CompositionStarted(ICoreTextEditContext *iface, ITypedEventHandler_CoreTextEditContext_CoreTextCompositionStartedEventArgs *handler, EventRegistrationToken *cookie)
{
    return winrt_event_add(&impl_from_ICoreTextEditContext(iface)->events[6], handler, cookie);
}

static HRESULT WINAPI core_text_edit_context_remove_CompositionStarted(ICoreTextEditContext *iface, EventRegistrationToken cookie)
{
    return winrt_event_remove(&impl_from_ICoreTextEditContext(iface)->events[6], cookie);
}

/* CompositionCompleted (flattened form) */
static HRESULT WINAPI core_text_edit_context_add_CompositionCompleted(ICoreTextEditContext *iface, ITypedEventHandler_CoreTextEditContext_CoreTextCompositionCompletedEventArgs *handler, EventRegistrationToken *cookie)
{
    return winrt_event_add(&impl_from_ICoreTextEditContext(iface)->events[7], handler, cookie);
}

static HRESULT WINAPI core_text_edit_context_remove_CompositionCompleted(ICoreTextEditContext *iface, EventRegistrationToken cookie)
{
    return winrt_event_remove(&impl_from_ICoreTextEditContext(iface)->events[7], cookie);
}

/* FocusRemoved (flattened form) */
static HRESULT WINAPI core_text_edit_context_add_FocusRemoved(ICoreTextEditContext *iface, ITypedEventHandler_CoreTextEditContext_IInspectable *handler, EventRegistrationToken *cookie)
{
    return winrt_event_add(&impl_from_ICoreTextEditContext(iface)->events[8], handler, cookie);
}

static HRESULT WINAPI core_text_edit_context_remove_FocusRemoved(ICoreTextEditContext *iface, EventRegistrationToken cookie)
{
    return winrt_event_remove(&impl_from_ICoreTextEditContext(iface)->events[8], cookie);
}

/* Notifications */
static HRESULT WINAPI core_text_edit_context_NotifyFocusEnter(ICoreTextEditContext *iface)
{
    return edit_context_focus(impl_from_ICoreTextEditContext(iface));
}

static HRESULT WINAPI core_text_edit_context_NotifyFocusLeave(ICoreTextEditContext *iface)
{
    edit_context_blur(impl_from_ICoreTextEditContext(iface));
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_NotifyTextChanged(ICoreTextEditContext *iface,
                                                               CoreTextRange modifiedRange,
                                                               INT32 newLength,
                                                               CoreTextRange newSelection)
{
    struct core_text_edit_context *impl = impl_from_ICoreTextEditContext(iface);
    if (newLength < 0 || modifiedRange.StartCaretPosition < 0 ||
        modifiedRange.EndCaretPosition < modifiedRange.StartCaretPosition ||
        newSelection.StartCaretPosition < 0 || newSelection.EndCaretPosition < newSelection.StartCaretPosition) return E_INVALIDARG;
    impl->selection = newSelection;
    ++impl->selection_version;
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_NotifySelectionChanged(ICoreTextEditContext *iface,
                                                                    CoreTextRange selection)
{
    struct core_text_edit_context *impl = impl_from_ICoreTextEditContext(iface);
    if (selection.StartCaretPosition < 0 || selection.EndCaretPosition < selection.StartCaretPosition) return E_INVALIDARG;
    impl->selection = selection;
    ++impl->selection_version;
    return S_OK;
}

static HRESULT WINAPI core_text_edit_context_NotifyLayoutChanged(ICoreTextEditContext *iface)
{
    TRACE("iface %p\n", iface);
    return S_OK;
}

struct ICoreTextEditContextVtbl core_text_edit_context_vtbl =
{
    /* IUnknown */
    core_text_edit_context_QueryInterface,
    core_text_edit_context_AddRef,
    core_text_edit_context_Release,

    /* IInspectable */
    core_text_edit_context_GetIids,
    core_text_edit_context_GetRuntimeClassName,
    core_text_edit_context_GetTrustLevel,

    /* ICoreTextEditContext (in IDL order) */
    core_text_edit_context_get_Name,                      /* propget Name */
    core_text_edit_context_put_Name,                      /* propput Name */
    core_text_edit_context_get_InputScope,                /* propget InputScope */
    core_text_edit_context_put_InputScope,                /* propput InputScope */
    core_text_edit_context_get_IsReadOnly,                /* propget IsReadOnly */
    core_text_edit_context_put_IsReadOnly,                /* propput IsReadOnly */
    core_text_edit_context_get_InputPaneDisplayPolicy,    /* propget InputPaneDisplayPolicy */
    core_text_edit_context_put_InputPaneDisplayPolicy,    /* propput InputPaneDisplayPolicy */

    core_text_edit_context_add_TextRequested,             /* eventadd TextRequested */
    core_text_edit_context_remove_TextRequested,          /* eventremove TextRequested */
    core_text_edit_context_add_SelectionRequested,        /* eventadd SelectionRequested */
    core_text_edit_context_remove_SelectionRequested,     /* eventremove SelectionRequested */
    core_text_edit_context_add_LayoutRequested,           /* eventadd LayoutRequested */
    core_text_edit_context_remove_LayoutRequested,        /* eventremove LayoutRequested */
    core_text_edit_context_add_TextUpdating,              /* eventadd TextUpdating */
    core_text_edit_context_remove_TextUpdating,           /* eventremove TextUpdating */
    core_text_edit_context_add_SelectionUpdating,         /* eventadd SelectionUpdating */
    core_text_edit_context_remove_SelectionUpdating,      /* eventremove SelectionUpdating */
    core_text_edit_context_add_FormatUpdating,            /* eventadd FormatUpdating */
    core_text_edit_context_remove_FormatUpdating,         /* eventremove FormatUpdating */
    core_text_edit_context_add_CompositionStarted,        /* eventadd CompositionStarted */
    core_text_edit_context_remove_CompositionStarted,     /* eventremove CompositionStarted */
    core_text_edit_context_add_CompositionCompleted,      /* eventadd CompositionCompleted */
    core_text_edit_context_remove_CompositionCompleted,   /* eventremove CompositionCompleted */
    core_text_edit_context_add_FocusRemoved,              /* eventadd FocusRemoved */
    core_text_edit_context_remove_FocusRemoved,           /* eventremove FocusRemoved */
    core_text_edit_context_NotifyFocusEnter,              /* NotifyFocusEnter */
    core_text_edit_context_NotifyFocusLeave,              /* NotifyFocusLeave */
    core_text_edit_context_NotifyTextChanged,             /* NotifyTextChanged */
    core_text_edit_context_NotifySelectionChanged,        /* NotifySelectionChanged */
    core_text_edit_context_NotifyLayoutChanged            /* NotifyLayoutChanged */
};
