/* UWP FileOpenPicker backed by the XDG FileChooser portal.
 * Copyright 2026 WineGDK contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Storage
#define WIDL_using_Windows_Storage_Pickers
#include "windows.storage.h"
#include "activation.h"
#include "async_private.h"
#include "windows.storage.pickers.h"
#include "initguid.h"
#define PICKERS_UWP_ONLY
#include "private.h"
#include "winuser.h"

/* Shared collection GUIDs are defined by main.c; define only UWP-specific GUIDs here. */
DEFINE_GUID(IID___x_ABI_CWindows_CStorage_CPickers_CIFileOpenPicker, 0x2ca8278a, 0x12c5, 0x4c5f, 0x89,0x77, 0x94,0x54,0x77,0x93,0xc2,0x41);
DEFINE_GUID(IID___FIIterator_1_Windows__CStorage__CStorageFile, 0x43e29f53, 0x0298, 0x55aa, 0xa6,0xc8, 0x4e,0xdd,0x32,0x3d,0x95,0x98);
DEFINE_GUID(IID___FIIterable_1_Windows__CStorage__CStorageFile, 0x9ac00304, 0x83ea, 0x5688, 0x87,0xb6, 0xae,0x38,0xaa,0xb6,0x5d,0x0b);
DEFINE_GUID(IID___FIVector_1_Windows__CStorage__CStorageFile, 0xfcbc8b8b, 0x6103, 0x5b4e, 0xba,0x00, 0x4b,0xc2,0xce,0xdb,0x6a,0x35);

DEFINE_GUID(IID_IInitializeWithWindow, 0x3e68d4bd, 0x7135, 0x4d10, 0x80, 0x18, 0x9f, 0xb6, 0xd9, 0xf3, 0x3f, 0xa1);
typedef struct IInitializeWithWindow IInitializeWithWindow;
typedef struct IInitializeWithWindowVtbl {
    HRESULT (WINAPI *QueryInterface)(IInitializeWithWindow *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IInitializeWithWindow *);
    ULONG (WINAPI *Release)(IInitializeWithWindow *);
    HRESULT (WINAPI *Initialize)(IInitializeWithWindow *, HWND);
} IInitializeWithWindowVtbl;
struct IInitializeWithWindow { const IInitializeWithWindowVtbl *lpVtbl; };

/* Keep the UWP and Windows App SDK interfaces distinct. */
typedef __x_ABI_CWindows_CStorage_CPickers_CIFileOpenPicker UwpOpenPicker;
typedef __x_ABI_CWindows_CStorage_CPickers_CIFileOpenPickerVtbl UwpOpenPickerVtbl;
typedef __x_ABI_CWindows_CStorage_CPickers_CPickerViewMode UwpViewMode;
typedef __x_ABI_CWindows_CStorage_CPickers_CPickerLocationId UwpLocation;
#define IID_UwpOpenPicker IID___x_ABI_CWindows_CStorage_CPickers_CIFileOpenPicker

struct open_picker
{
    UwpOpenPicker iface;
    IInitializeWithWindow init;
    LONG ref;
    CRITICAL_SECTION lock;
    HWND hwnd;
    UwpViewMode view;
    UwpLocation location;
    HSTRING settings, commit;
    IVector_HSTRING *filter;
};

static ULONG WINAPI picker_addref( UwpOpenPicker *iface )
{ return InterlockedIncrement( &((struct open_picker *)iface)->ref ); }

static ULONG WINAPI picker_release( UwpOpenPicker *iface )
{
    struct open_picker *impl = (struct open_picker *)iface;
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->filter) IVector_HSTRING_Release( impl->filter );
        WindowsDeleteString( impl->settings );
        WindowsDeleteString( impl->commit );
        DeleteCriticalSection( &impl->lock );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI picker_qi( UwpOpenPicker *iface, REFIID iid, void **out )
{
    struct open_picker *impl = (struct open_picker *)iface;
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_UwpOpenPicker)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IInitializeWithWindow)) *out = &impl->init;
    else return E_NOINTERFACE;
    picker_addref( iface );
    return S_OK;
}

static HRESULT WINAPI picker_iids( UwpOpenPicker *iface, ULONG *count, IID **out )
{
    if (!count || !out) return E_POINTER;
    *count = 0;
    if (!(*out = CoTaskMemAlloc(sizeof(IID)))) return E_OUTOFMEMORY;
    **out = IID_UwpOpenPicker;
    *count = 1;
    return S_OK;
}
static HRESULT WINAPI picker_name( UwpOpenPicker *iface, HSTRING *out )
{
    const WCHAR name[] = L"Windows.Storage.Pickers.FileOpenPicker";
    return WindowsCreateString( name, ARRAY_SIZE(name) - 1, out );
}
static HRESULT WINAPI picker_trust( UwpOpenPicker *iface, TrustLevel *out )
{ if (!out) return E_POINTER; *out = BaseTrust; return S_OK; }

#define ENUM_PROPERTY(name, field, type, maximum) \
static HRESULT WINAPI picker_get_##name( UwpOpenPicker *iface, type *out ) \
{ \
    struct open_picker *impl = (struct open_picker *)iface; \
    if (!out) return E_POINTER; \
    EnterCriticalSection(&impl->lock); *out = impl->field; LeaveCriticalSection(&impl->lock); return S_OK; \
} \
static HRESULT WINAPI picker_put_##name( UwpOpenPicker *iface, type value ) \
{ \
    struct open_picker *impl = (struct open_picker *)iface; \
    if ((unsigned int)value > maximum) return E_INVALIDARG; \
    EnterCriticalSection(&impl->lock); impl->field = value; LeaveCriticalSection(&impl->lock); return S_OK; \
}
ENUM_PROPERTY(ViewMode, view, UwpViewMode, 1)
ENUM_PROPERTY(SuggestedStartLocation, location, UwpLocation, 9)
#undef ENUM_PROPERTY

#define STRING_PROPERTY(name, field) \
static HRESULT WINAPI picker_get_##name( UwpOpenPicker *iface, HSTRING *out ) \
{ \
    struct open_picker *impl = (struct open_picker *)iface; HRESULT hr; \
    EnterCriticalSection(&impl->lock); hr = WindowsDuplicateString(impl->field, out); LeaveCriticalSection(&impl->lock); return hr; \
} \
static HRESULT WINAPI picker_put_##name( UwpOpenPicker *iface, HSTRING value ) \
{ \
    struct open_picker *impl = (struct open_picker *)iface; HSTRING copy, old; HRESULT hr; \
    if (FAILED(hr = WindowsDuplicateString(value, &copy))) return hr; \
    EnterCriticalSection(&impl->lock); old = impl->field; impl->field = copy; LeaveCriticalSection(&impl->lock); \
    WindowsDeleteString(old); return S_OK; \
}
STRING_PROPERTY(SettingsIdentifier, settings)
STRING_PROPERTY(CommitButtonText, commit)
#undef STRING_PROPERTY

static HRESULT WINAPI picker_filter( UwpOpenPicker *iface, IVector_HSTRING **out )
{
    if (!out) return E_POINTER;
    *out = ((struct open_picker *)iface)->filter;
    IVector_HSTRING_AddRef( *out );
    return S_OK;
}

/* Validate before serializing: the portal bridge uses tabs/newlines as delimiters. */
static HRESULT make_filters( IVector_HSTRING *filter, WCHAR **out )
{
    UINT32 count, i, len, j;
    HSTRING item;
    const WCHAR *value;
    WCHAR *result = NULL, *next;
    SIZE_T used = 0;
    HRESULT hr;
    *out = NULL;
    if (FAILED(hr = IVector_HSTRING_get_Size(filter, &count))) return hr;
    if (!count) return E_INVALIDARG;
    for (i = 0; i < count; ++i)
    {
        if (FAILED(hr = IVector_HSTRING_GetAt(filter, i, &item))) goto failed;
        value = WindowsGetStringRawBuffer( item, &len );
        if (!(len == 1 && value[0] == '*'))
        {
            if (len < 2 || value[0] != '.') { hr = E_INVALIDARG; goto item_failed; }
            for (j = 1; j < len; ++j)
                if (value[j] <= ' ' || wcschr(L"\\/:;*?[]", value[j])) { hr = E_INVALIDARG; goto item_failed; }
        }
        if (len > 1024 || used > 65536) { hr = E_INVALIDARG; goto item_failed; }
        if (!(next = realloc(result, (used + len + 3) * sizeof(WCHAR)))) { hr = E_OUTOFMEMORY; goto item_failed; }
        result = next;
        if (used) result[used++] = ';';
        if (value[0] == '.') result[used++] = '*';
        memcpy( result + used, value, len * sizeof(WCHAR) );
        used += len;
        result[used] = 0;
        WindowsDeleteString( item );
    }
    if (!(*out = malloc((used + 32) * sizeof(WCHAR)))) { hr = E_OUTOFMEMORY; goto failed; }
    swprintf( *out, used + 32, L"Supported files\t%s\n", result );
    free( result );
    return S_OK;
item_failed:
    WindowsDeleteString( item );
failed:
    free( result );
    return hr;
}

static HRESULT pick_files( IUnknown *invoker, IUnknown *param, PROPVARIANT *result, BOOL called_async )
{
    static const struct vector_iids iids = {&IID_IIterable_StorageFile, &IID_IIterator_StorageFile,
                                           &IID_IVector_StorageFile, &IID_IVectorView_StorageFile};
    struct picker_request *request = CONTAINING_RECORD(param, struct picker_request, IUnknown_iface);
    IVector_IInspectable *files = NULL;
    IVectorView_IInspectable *view;
    WCHAR *paths, *path;
    IUnknown *file;
    DWORD attrs;
    HRESULT hr;
    if (!called_async) return STATUS_PENDING;
    if (FAILED(hr = picker_run_dialog(request, &paths))) return hr;
    if (request->kind == PICKER_KIND_OPEN_MULTIPLE)
        if (FAILED(hr = vector_inspectable_create(&iids, &files))) { free(paths); return hr; }
    for (path = paths; path && *path; path += wcslen(path) + 1)
    {
        attrs = GetFileAttributesW(path);
        if (attrs == INVALID_FILE_ATTRIBUTES) { hr = HRESULT_FROM_WIN32(GetLastError()); break; }
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) { hr = HRESULT_FROM_WIN32(ERROR_DIRECTORY); break; }
        if (FAILED(hr = storage_file_create_object(path, &file))) break;
        if (!files) { result->vt = VT_UNKNOWN; result->punkVal = file; break; }
        hr = IVector_IInspectable_Append(files, (IInspectable *)file);
        IUnknown_Release(file);
        if (FAILED(hr)) break;
    }
    free(paths);
    if (files)
    {
        if (SUCCEEDED(hr) && SUCCEEDED(hr = IVector_IInspectable_GetView(files, &view)))
        { result->vt = VT_UNKNOWN; result->punkVal = (IUnknown *)view; }
        IVector_IInspectable_Release(files);
    }
    return hr;
}

static HRESULT picker_pick( UwpOpenPicker *iface, BOOL multiple, IAsyncOperation_IInspectable **out )
{
    struct open_picker *impl = (struct open_picker *)iface;
    struct picker_request *request = NULL;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    EnterCriticalSection(&impl->lock);
    hr = picker_request_create(multiple ? PICKER_KIND_OPEN_MULTIPLE : PICKER_KIND_OPEN_SINGLE,
                               (UINT64)(ULONG_PTR)impl->hwnd, &request);
    if (SUCCEEDED(hr))
    {
        request->start_location = impl->location;
        request->accept_label = hstring_dup(impl->commit);
        if (WindowsGetStringLen(impl->commit) && !request->accept_label) hr = E_OUTOFMEMORY;
    }
    LeaveCriticalSection(&impl->lock);
    if (FAILED(hr)) { if (request) IUnknown_Release(&request->IUnknown_iface); return hr; }
    if (SUCCEEDED(hr = make_filters(impl->filter, &request->filters)))
        hr = async_operation_inspectable_create(multiple ? &IID_IAsyncOperation_IVectorView_StorageFile : &IID_IAsyncOperation_StorageFile,
                                               (IUnknown *)iface, &request->IUnknown_iface, pick_files, out);
    IUnknown_Release(&request->IUnknown_iface);
    return hr;
}
static HRESULT WINAPI picker_single( UwpOpenPicker *iface, IAsyncOperation_StorageFile **out )
{ return picker_pick(iface, FALSE, (IAsyncOperation_IInspectable **)out); }
static HRESULT WINAPI picker_multiple( UwpOpenPicker *iface, IAsyncOperation_IVectorView_StorageFile **out )
{ return picker_pick(iface, TRUE, (IAsyncOperation_IInspectable **)out); }

static struct open_picker *from_init( IInitializeWithWindow *iface )
{ return CONTAINING_RECORD(iface, struct open_picker, init); }
static HRESULT WINAPI init_qi( IInitializeWithWindow *iface, REFIID iid, void **out )
{ return picker_qi(&from_init(iface)->iface, iid, out); }
static ULONG WINAPI init_addref( IInitializeWithWindow *iface ) { return picker_addref(&from_init(iface)->iface); }
static ULONG WINAPI init_release( IInitializeWithWindow *iface ) { return picker_release(&from_init(iface)->iface); }
static HRESULT WINAPI init_window( IInitializeWithWindow *iface, HWND hwnd )
{
    struct open_picker *impl = from_init(iface);
    DWORD pid;
    if (!IsWindow(hwnd)) return E_INVALIDARG;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return E_INVALIDARG;
    EnterCriticalSection(&impl->lock); impl->hwnd = hwnd; LeaveCriticalSection(&impl->lock);
    return S_OK;
}
static const IInitializeWithWindowVtbl init_vtbl = {init_qi, init_addref, init_release, init_window};
static const UwpOpenPickerVtbl picker_vtbl = {picker_qi, picker_addref, picker_release, picker_iids,
    picker_name, picker_trust, picker_get_ViewMode, picker_put_ViewMode, picker_get_SettingsIdentifier,
    picker_put_SettingsIdentifier, picker_get_SuggestedStartLocation, picker_put_SuggestedStartLocation,
    picker_get_CommitButtonText, picker_put_CommitButtonText, picker_filter, picker_single, picker_multiple};

static HRESULT WINAPI factory_qi( IActivationFactory *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IInspectable) &&
        !IsEqualGUID(iid, &IID_IActivationFactory) && !IsEqualGUID(iid, &IID_IAgileObject)) return E_NOINTERFACE;
    *out = iface; IActivationFactory_AddRef(iface); return S_OK;
}
static ULONG WINAPI factory_addref( IActivationFactory *iface ) { return 2; }
static ULONG WINAPI factory_release( IActivationFactory *iface ) { return 1; }
static HRESULT WINAPI factory_iids( IActivationFactory *iface, ULONG *count, IID **out )
{ if (!count || !out) return E_POINTER; *count = 0; *out = NULL; return S_OK; }
static HRESULT WINAPI factory_name( IActivationFactory *iface, HSTRING *out ) { return picker_name(NULL, out); }
static HRESULT WINAPI factory_trust( IActivationFactory *iface, TrustLevel *out ) { return picker_trust(NULL, out); }
static HRESULT WINAPI factory_activate( IActivationFactory *iface, IInspectable **out )
{
    struct open_picker *impl;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl = &picker_vtbl;
    impl->init.lpVtbl = &init_vtbl;
    impl->ref = 1;
    InitializeCriticalSectionEx(&impl->lock, 0, 0);
    if (FAILED(hr = vector_hstring_create(&impl->filter))) { picker_release(&impl->iface); return hr; }
    *out = (IInspectable *)&impl->iface;
    return S_OK;
}
static const IActivationFactoryVtbl factory_vtbl = {factory_qi, factory_addref, factory_release,
    factory_iids, factory_name, factory_trust, factory_activate};
static IActivationFactory factory = {&factory_vtbl};
IActivationFactory *uwp_open_picker_factory = &factory;
