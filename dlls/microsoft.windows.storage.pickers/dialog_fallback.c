/* Explorer-style fallback for WinRT and Windows App SDK file pickers.
 * Copyright 2026 WineGDK contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "private.h"
#include "initguid.h"
#include "shobjidl.h"
#include "shlobj.h"
#include "knownfolders.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(pickers);

struct fallback_context
{
    struct picker_request *request;
    IFileDialog *dialog;
    WCHAR *paths;
    size_t length;
    HRESULT hr;
};
static LRESULT CALLBACK cancel_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct fallback_context *context = (void *)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if (message == WM_TIMER && context && InterlockedCompareExchange(&context->request->cancelled,0,0))
        IFileDialog_Close(context->dialog,HRESULT_FROM_WIN32(ERROR_CANCELLED));
    return DefWindowProcW(hwnd,message,wparam,lparam);
}
static HRESULT append_item(struct fallback_context *context, IShellItem *item)
{
    WCHAR *path, *buffer;
    size_t length;
    HRESULT hr = IShellItem_GetDisplayName(item,SIGDN_FILESYSPATH,&path);
    if (FAILED(hr)) return hr;
    length = wcslen(path)+1;
    buffer = realloc(context->paths,(context->length+length+1)*sizeof(WCHAR));
    if (!buffer) hr = E_OUTOFMEMORY;
    else
    {
        context->paths = buffer;
        memcpy(buffer+context->length,path,length*sizeof(WCHAR));
        context->length += length;
        buffer[context->length] = 0;
    }
    CoTaskMemFree(path);
    return hr;
}
static HRESULT configure_dialog(IFileDialog *dialog, struct picker_request *request)
{
    static const GUID * const locations[] = {&FOLDERID_Documents,NULL,&FOLDERID_Desktop,&FOLDERID_Downloads,
        NULL,&FOLDERID_Music,&FOLDERID_Pictures,&FOLDERID_Videos,&FOLDERID_Objects3D};
    COMDLG_FILTERSPEC *specs = NULL;
    WCHAR *filters = NULL, *line, *end, *tab;
    IShellItem *folder = NULL;
    FILEOPENDIALOGOPTIONS options;
    unsigned count = 0, capacity = 1;
    HRESULT hr;
    options = FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_PATHMUSTEXIST;
    if (request->kind == PICKER_KIND_SAVE) options |= FOS_OVERWRITEPROMPT;
    else if (request->kind == PICKER_KIND_FOLDER) options |= FOS_PICKFOLDERS;
    else options |= FOS_FILEMUSTEXIST;
    if (request->kind == PICKER_KIND_OPEN_MULTIPLE) options |= FOS_ALLOWMULTISELECT;
    if (FAILED(hr = IFileDialog_SetOptions(dialog,options))) return hr;
    if (request->title && FAILED(hr = IFileDialog_SetTitle(dialog,request->title))) return hr;
    if (request->accept_label && FAILED(hr = IFileDialog_SetOkButtonLabel(dialog,request->accept_label))) return hr;
    if (request->current_name && FAILED(hr = IFileDialog_SetFileName(dialog,request->current_name))) return hr;
    if (request->default_ext && FAILED(hr = IFileDialog_SetDefaultExtension(dialog,
            request->default_ext + (request->default_ext[0] == '.')))) return hr;
    if (request->current_folder)
        SHCreateItemFromParsingName(request->current_folder,NULL,&IID_IShellItem,(void **)&folder);
    else if ((unsigned)request->start_location < ARRAY_SIZE(locations) && locations[request->start_location])
        SHGetKnownFolderItem(locations[request->start_location],KF_FLAG_DEFAULT,NULL,&IID_IShellItem,(void **)&folder);
    if (folder)
    {
        hr = IFileDialog_SetFolder(dialog,folder);
        IShellItem_Release(folder);
        if (FAILED(hr)) return hr;
    }
    if (!request->filters || request->kind == PICKER_KIND_FOLDER) return S_OK;
    for (line = request->filters; *line; ++line) if (*line == '\n') ++capacity;
    if (!(filters = wcsdup(request->filters)) || !(specs = calloc(capacity,sizeof(*specs))))
    { free(filters); return E_OUTOFMEMORY; }
    for (line = filters; *line; line = end)
    {
        if ((end = wcschr(line,'\n'))) *end++ = 0;
        else end = line+wcslen(line);
        if (!(tab = wcschr(line,'\t')) || tab == line || !tab[1]) { hr = E_INVALIDARG; goto done; }
        *tab++ = 0;
        specs[count].pszName = line;
        specs[count++].pszSpec = tab;
    }
    hr = count ? IFileDialog_SetFileTypes(dialog,count,specs) : S_OK;
done:
    free(specs); free(filters);
    return hr;
}
static DWORD WINAPI fallback_thread(void *param)
{
    struct fallback_context *context = param;
    struct picker_request *request = context->request;
    IShellItemArray *items = NULL;
    IFileOpenDialog *open = NULL;
    IShellItem *item = NULL;
    HWND monitor = NULL, owner = (HWND)(ULONG_PTR)request->window_id;
    HRESULT hr, init_hr;
    DWORD count, i;
    init_hr = CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    if (FAILED(init_hr)) { context->hr = init_hr; return 0; }
    hr = CoCreateInstance(request->kind == PICKER_KIND_SAVE ? &CLSID_FileSaveDialog : &CLSID_FileOpenDialog,
        NULL,CLSCTX_INPROC_SERVER,&IID_IFileDialog,(void **)&context->dialog);
    if (FAILED(hr)) goto done;
    if (FAILED(hr = configure_dialog(context->dialog,request))) goto done;
    monitor = CreateWindowExW(0,L"STATIC",NULL,0,0,0,0,0,HWND_MESSAGE,NULL,NULL,NULL);
    if (!monitor) { hr = HRESULT_FROM_WIN32(GetLastError()); goto done; }
    SetWindowLongPtrW(monitor,GWLP_USERDATA,(LONG_PTR)context);
    SetWindowLongPtrW(monitor,GWLP_WNDPROC,(LONG_PTR)cancel_proc);
    if (!SetTimer(monitor,1,100,NULL)) { hr = HRESULT_FROM_WIN32(GetLastError()); goto done; }
    if (InterlockedCompareExchange(&request->cancelled,0,0)) { hr = S_OK; goto done; }
    TRACE("opening Explorer-style picker, kind %u, owner %p.\n",request->kind,owner);
    ClipCursor(NULL);
    hr = IFileDialog_Show(context->dialog,IsWindow(owner) ? owner : NULL);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) { hr = S_OK; goto done; }
    if (FAILED(hr)) goto done;
    if (request->kind == PICKER_KIND_OPEN_MULTIPLE)
    {
        if (FAILED(hr = IFileDialog_QueryInterface(context->dialog,&IID_IFileOpenDialog,(void **)&open))) goto done;
        if (FAILED(hr = IFileOpenDialog_GetResults(open,&items))) goto done;
        if (FAILED(hr = IShellItemArray_GetCount(items,&count))) goto done;
        for (i = 0; i < count; ++i)
        {
            if (FAILED(hr = IShellItemArray_GetItemAt(items,i,&item))) break;
            hr = append_item(context,item);
            IShellItem_Release(item); item = NULL;
            if (FAILED(hr)) break;
        }
    }
    else if (SUCCEEDED(hr = IFileDialog_GetResult(context->dialog,&item))) hr = append_item(context,item);
done:
    if (monitor) { KillTimer(monitor,1); DestroyWindow(monitor); }
    if (item) IShellItem_Release(item);
    if (items) IShellItemArray_Release(items);
    if (open) IFileOpenDialog_Release(open);
    if (context->dialog) IFileDialog_Release(context->dialog);
    CoUninitialize();
    context->hr = hr;
    return 0;
}
HRESULT picker_run_fallback_dialog(struct picker_request *request, WCHAR **paths)
{
    struct fallback_context context = {.request = request};
    HANDLE thread;
    *paths = NULL;
    if (InterlockedCompareExchange(&request->cancelled,0,0)) return S_OK;
    /* Async WinRT workers are MTA; the common dialog gets its own STA and message loop. */
    if (!(thread = CreateThread(NULL,0,fallback_thread,&context,0,NULL))) return HRESULT_FROM_WIN32(GetLastError());
    WaitForSingleObject(thread,INFINITE);
    CloseHandle(thread);
    if (FAILED(context.hr)) free(context.paths);
    else *paths = context.paths;
    return context.hr;
}
