/* Integration test: run with an unavailable XDG portal, in a disposable prefix. */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Storage
#include "initguid.h"
#ifdef TEST_UWP
#define WIDL_using_Windows_Storage_Pickers
#include "windows.storage.pickers.h"
#define CLASS_PREFIX L"Windows.Storage.Pickers."
#define SingleOp IAsyncOperation_StorageFile
#define SingleResults IAsyncOperation_StorageFile_GetResults
#define SingleRelease IAsyncOperation_StorageFile_Release
#define MultipleOp IAsyncOperation_IVectorView_StorageFile
#define MultipleResults IAsyncOperation_IVectorView_StorageFile_GetResults
#define MultipleRelease IAsyncOperation_IVectorView_StorageFile_Release
#define Result IStorageFile
#define Results IVectorView_StorageFile
#define ResultsSize IVectorView_StorageFile_get_Size
#define ResultsAt IVectorView_StorageFile_GetAt
#define ResultsRelease IVectorView_StorageFile_Release
#else
#define WIDL_using_Microsoft_UI
#define WIDL_using_Microsoft_Windows_Storage_Pickers
#include "microsoft.windows.storage.pickers.h"
#define CLASS_PREFIX L"Microsoft.Windows.Storage.Pickers."
#define SingleOp IAsyncOperation_PickFileResult
#define SingleResults IAsyncOperation_PickFileResult_GetResults
#define SingleRelease IAsyncOperation_PickFileResult_Release
#define MultipleOp IAsyncOperation_IVectorView_PickFileResult
#define MultipleResults IAsyncOperation_IVectorView_PickFileResult_GetResults
#define MultipleRelease IAsyncOperation_IVectorView_PickFileResult_Release
#define Result IPickFileResult
#define Results IVectorView_PickFileResult
#define ResultsSize IVectorView_PickFileResult_get_Size
#define ResultsAt IVectorView_PickFileResult_GetAt
#define ResultsRelease IVectorView_PickFileResult_Release
#endif
#include "roapi.h"
#include "winstring.h"
#include "dlgs.h"
#include <stdio.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %u: %s\n",__LINE__,#x); ++failures; } } while (0)
static HSTRING string(const WCHAR *text)
{
    HSTRING value = NULL;
    CHECK(WindowsCreateString(text,wcslen(text),&value) == S_OK);
    return value;
}
static void wait_for(IInspectable *operation)
{
    IAsyncInfo *info = NULL;
    AsyncStatus status = Started;
    unsigned i;
    CHECK(IInspectable_QueryInterface(operation,&IID_IAsyncInfo,(void **)&info) == S_OK);
    if (!info) return;
    for (i = 0; i < 1500 && status == Started; ++i)
    { CHECK(IAsyncInfo_get_Status(info,&status) == S_OK); Sleep(10); }
    CHECK(status != Started);
    IAsyncInfo_Release(info);
}
static void check_path(IInspectable *result, const WCHAR *expected)
{
    HSTRING path = NULL;
#ifdef TEST_UWP
    IStorageItem *item = NULL;
    CHECK(IInspectable_QueryInterface(result,&IID_IStorageItem,(void **)&item) == S_OK);
    if (!item) return;
    CHECK(IStorageItem_get_Path(item,&path) == S_OK);
    IStorageItem_Release(item);
#else
    CHECK(IPickFileResult_get_Path((IPickFileResult *)result,&path) == S_OK);
#endif
    if (wcscmp(WindowsGetStringRawBuffer(path,NULL),expected))
        wprintf(L"path: '%ls', expected '%ls'\n",WindowsGetStringRawBuffer(path,NULL),expected);
    CHECK(!wcscmp(WindowsGetStringRawBuffer(path,NULL),expected));
    WindowsDeleteString(path);
}
struct automation { const WCHAR *filename; IInspectable *cancel; HWND window; };
static BOOL CALLBACK find_dialog(HWND hwnd, LPARAM param)
{
    struct automation *automation = (void *)param;
    WCHAR cls[32]; DWORD pid;
    GetWindowThreadProcessId(hwnd,&pid);
    GetClassNameW(hwnd,cls,32);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd) && !wcscmp(cls,L"#32770") && GetDlgItem(hwnd,edt1))
    { automation->window = hwnd; return FALSE; }
    return TRUE;
}
static DWORD WINAPI automate(void *param)
{
    struct automation *automation = param;
    unsigned i;
    for (i = 0; i < 1000 && !automation->window; ++i)
    { EnumWindows(find_dialog,(LPARAM)automation); Sleep(10); }
    if (!automation->window) return 1;
    if (automation->cancel)
    {
        IAsyncInfo *info = NULL;
        if (FAILED(IInspectable_QueryInterface(automation->cancel,&IID_IAsyncInfo,(void **)&info))) return 2;
        IAsyncInfo_Cancel(info);
        IAsyncInfo_Release(info);
    }
    else if (!automation->filename) PostMessageW(automation->window,WM_COMMAND,IDCANCEL,0);
    else
    {
        SetDlgItemTextW(automation->window,edt1,automation->filename);
        PostMessageW(automation->window,WM_COMMAND,IDOK,0);
    }
    for (i = 0; i < 1000 && IsWindow(automation->window); ++i) Sleep(10);
    return IsWindow(automation->window) ? 3 : 0;
}
static void finish_automation(HANDLE thread)
{
    DWORD code = 0;
    CHECK(WaitForSingleObject(thread,22000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(thread,&code) && !code);
    CloseHandle(thread);
}
static void *activate(const WCHAR *suffix, REFIID iid)
{
    WCHAR name[100]; HSTRING cls;
    IInspectable *instance = NULL;
    void *result = NULL;
    swprintf(name,100,L"%ls%ls",CLASS_PREFIX,suffix);
    cls = string(name);
#ifdef TEST_UWP
    CHECK(RoActivateInstance(cls,&instance) == S_OK && instance);
#else
    /* All three factories share the CreateInstance(WindowId, out) ABI. */
    IFileOpenPickerFactory *factory = NULL;
    const GUID *factory_iid = !wcscmp(suffix,L"FileOpenPicker") ? &IID_IFileOpenPickerFactory :
        !wcscmp(suffix,L"FileSavePicker") ? &IID_IFileSavePickerFactory : &IID_IFolderPickerFactory;
    WindowId window = {0};
    CHECK(RoGetActivationFactory(cls,factory_iid,(void **)&factory) == S_OK && factory);
    if (factory)
    {
        CHECK(IFileOpenPickerFactory_CreateInstance(factory,window,(IFileOpenPicker **)&instance) == S_OK);
        IFileOpenPickerFactory_Release(factory);
    }
#endif
    WindowsDeleteString(cls);
    if (instance)
    {
        CHECK(IInspectable_QueryInterface(instance,iid,&result) == S_OK);
        IInspectable_Release(instance);
    }
    return result;
}
int wmain(int argc, WCHAR **argv)
{
    IFileOpenPicker *open;
    IFileSavePicker *save;
    IVector_HSTRING *filter = NULL;
    IMap_HSTRING_IVector_HSTRING *choices = NULL;
    SingleOp *operation = NULL;
    Result *result = NULL;
    HSTRING extension, label;
    WCHAR filename[32768], expected[32768];
    HANDLE thread;
    unsigned i;
    boolean replaced;
    setvbuf(stdout,NULL,_IONBF,0);
    if (argc != 4) return 2;
    CHECK(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
    open = activate(L"FileOpenPicker",&IID_IFileOpenPicker);
    save = activate(L"FileSavePicker",&IID_IFileSavePicker);
    if (!open || !save) return 1;
    extension = string(L".mcworld"); label = string(L"Minecraft worlds");
    CHECK(IFileOpenPicker_get_FileTypeFilter(open,&filter) == S_OK && filter);
    CHECK(IVector_HSTRING_Append(filter,extension) == S_OK);
    CHECK(IFileSavePicker_get_FileTypeChoices(save,&choices) == S_OK && choices);
    CHECK(IMap_HSTRING_IVector_HSTRING_Insert(choices,label,filter,&replaced) == S_OK);
    CHECK(IFileSavePicker_put_DefaultFileExtension(save,extension) == S_OK);
    IMap_HSTRING_IVector_HSTRING_Release(choices);
    IVector_HSTRING_Release(filter);
    WindowsDeleteString(extension); WindowsDeleteString(label);
    /* One file, cancel button, async cancellation, then save without a suffix. */
    for (i = 0; i < 4; ++i)
    {
        struct automation automation = {0};
        printf("single scenario %u\n",i);
        swprintf(filename,32768,L"%ls\\saved-%u",argv[3],(unsigned)(sizeof(void *)*8));
        swprintf(expected,32768,L"%ls.mcworld",filename);
        CHECK((i == 3 ? IFileSavePicker_PickSaveFileAsync(save,&operation) :
            IFileOpenPicker_PickSingleFileAsync(open,&operation)) == S_OK && operation);
        if (!operation) continue;
        automation.filename = i == 0 ? argv[1] : i == 3 ? filename : NULL;
        if (i == 2) automation.cancel = (IInspectable *)operation;
        thread = CreateThread(NULL,0,automate,&automation,0,NULL);
        wait_for((IInspectable *)operation);
        finish_automation(thread);
        if (i == 2) CHECK(SingleResults(operation,&result) == E_ABORT && !result);
        else
        {
            CHECK(SingleResults(operation,&result) == S_OK);
            if (i == 1) CHECK(!result);
            else
            {
                CHECK(result != NULL);
                if (result) check_path((IInspectable *)result,i == 0 ? argv[1] : expected);
            }
        }
        if (result) IInspectable_Release((IInspectable *)result);
        result = NULL;
        SingleRelease(operation); operation = NULL;
    }
    /* Quoted absolute paths exercise spaces, Unicode and multiple results. */
    {
        MultipleOp *multiple = NULL;
        Results *results = NULL;
        UINT32 count = 0;
        struct automation automation = {0};
        printf("multiple scenario\n");
        swprintf(filename,32768,L"\"%ls\" \"%ls\"",argv[1],argv[2]);
        automation.filename = filename;
        CHECK(IFileOpenPicker_PickMultipleFilesAsync(open,&multiple) == S_OK && multiple);
        if (multiple)
        {
            thread = CreateThread(NULL,0,automate,&automation,0,NULL);
            wait_for((IInspectable *)multiple); finish_automation(thread);
            CHECK(MultipleResults(multiple,&results) == S_OK && results);
            if (results)
            {
                CHECK(ResultsSize(results,&count) == S_OK && count == 2);
                for (i = 0; i < count; ++i)
                {
                    CHECK(ResultsAt(results,i,&result) == S_OK && result);
                    if (result) { check_path((IInspectable *)result,argv[i+1]); IInspectable_Release((IInspectable *)result); }
                }
                ResultsRelease(results);
            }
            MultipleRelease(multiple);
        }
    }
#ifndef TEST_UWP
    {
        IFolderPicker *folder = activate(L"FolderPicker",&IID_IFolderPicker);
        IAsyncOperation_PickFolderResult *folder_op = NULL;
        IPickFolderResult *folder_result = NULL;
        struct automation automation = {.filename = argv[3]};
        HSTRING path = NULL;
        printf("folder scenario\n");
        if (folder)
        {
            CHECK(IFolderPicker_PickSingleFolderAsync(folder,&folder_op) == S_OK && folder_op);
            if (folder_op)
            {
                thread = CreateThread(NULL,0,automate,&automation,0,NULL);
                wait_for((IInspectable *)folder_op); finish_automation(thread);
                CHECK(IAsyncOperation_PickFolderResult_GetResults(folder_op,&folder_result) == S_OK && folder_result);
                if (folder_result)
                {
                    CHECK(IPickFolderResult_get_Path(folder_result,&path) == S_OK);
                    CHECK(!wcscmp(WindowsGetStringRawBuffer(path,NULL),argv[3]));
                    WindowsDeleteString(path); IPickFolderResult_Release(folder_result);
                }
                IAsyncOperation_PickFolderResult_Release(folder_op);
            }
            IFolderPicker_Release(folder);
        }
    }
#endif
    IFileSavePicker_Release(save); IFileOpenPicker_Release(open);
    RoUninitialize();
    printf("fallback: %u failures\n",failures);
    return !!failures;
}
