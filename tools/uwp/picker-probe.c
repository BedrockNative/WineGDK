/* UWP picker integration probe; use the private XDG portal fixture. */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Storage
#define WIDL_using_Windows_Storage_Streams
#define WIDL_using_Windows_Storage_AccessCache
#define WIDL_using_Windows_Storage_Provider
#define WIDL_using_Windows_Storage_Pickers
#include "initguid.h"
#include "windows.storage.pickers.h"
#include "windows.storage.accesscache.h"
#include "robuffer.h"
#include "windows.storage.streams.h"
#include "roapi.h"
#include "winstring.h"
#include <stdio.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %u: %s\n", __LINE__, #x); ++failures; } } while (0)
static HRESULT wait_for(IInspectable *operation)
{
    IAsyncInfo *info = NULL;
    AsyncStatus status = Started;
    HRESULT hr;
    unsigned i;
    hr = IInspectable_QueryInterface(operation, &IID_IAsyncInfo, (void **)&info);
    if (FAILED(hr)) return hr;
    for (i = 0; i < 1000 && status == Started; ++i)
    { hr = IAsyncInfo_get_Status(info, &status); if (FAILED(hr)) break; if (status == Started) Sleep(10); }
    CHECK(status != Started);
    IAsyncInfo_Release(info);
    return hr;
}
static void check_file(IStorageFile *file, const WCHAR *expected)
{
    IStorageItem *item = NULL;
    HSTRING path = NULL;
    CHECK(file != NULL);
    if (!file) return;
    CHECK(IStorageFile_QueryInterface(file, &IID_IStorageItem, (void **)&item) == S_OK);
    if (item)
    {
        CHECK(IStorageItem_get_Path(item, &path) == S_OK);
        CHECK(!wcscmp(WindowsGetStringRawBuffer(path, NULL), expected));
        CHECK(GetFileAttributesW(WindowsGetStringRawBuffer(path, NULL)) != INVALID_FILE_ATTRIBUTES);
        WindowsDeleteString(path);
        IStorageItem_Release(item);
    }
}
static void check_cached_file(IStorageFile *file)
{
    ICachedFileManagerStatics *statics = NULL;
    IAsyncOperation_FileUpdateStatus *operation = NULL;
    FileUpdateStatus status = FileUpdateStatus_Incomplete;
    HSTRING name;
    HRESULT hr;
    CHECK(WindowsCreateString(L"Windows.Storage.CachedFileManager", 33, &name) == S_OK);
    hr = RoGetActivationFactory(name, &IID_ICachedFileManagerStatics, (void **)&statics);
    WindowsDeleteString(name);
    CHECK(hr == S_OK && statics);
    if (!statics) return;
    CHECK(ICachedFileManagerStatics_DeferUpdates(statics, NULL) == E_INVALIDARG);
    CHECK(ICachedFileManagerStatics_CompleteUpdatesAsync(statics, file, NULL) == E_POINTER);
    CHECK(ICachedFileManagerStatics_CompleteUpdatesAsync(statics, NULL, &operation) == E_INVALIDARG && !operation);
    CHECK(ICachedFileManagerStatics_DeferUpdates(statics, file) == S_OK);
    CHECK(ICachedFileManagerStatics_CompleteUpdatesAsync(statics, file, &operation) == S_OK && operation);
    if (operation)
    {
        CHECK(wait_for((IInspectable *)operation) == S_OK);
        CHECK(IAsyncOperation_FileUpdateStatus_GetResults(operation, &status) == S_OK && status == FileUpdateStatus_Complete);
        CHECK(IAsyncOperation_FileUpdateStatus_GetResults(operation, &status) == S_OK && status == FileUpdateStatus_Complete);
        IAsyncOperation_FileUpdateStatus_Release(operation);
    }
    ICachedFileManagerStatics_Release(statics);
}
#include "file-stream-probe.h"
static void check_copy(IStorageFile *source, const WCHAR *source_path)
{
    IStorageFolderStatics *statics = NULL;
    IAsyncOperation_StorageFolder *folder_op = NULL;
    IStorageFolder *folder = NULL;
    IAsyncOperation_StorageFile *operation = NULL;
    IStorageFile *copy = NULL;
    WCHAR directory[32768], destination[32768];
    HSTRING class_name = NULL, path = NULL, name = NULL;
    HRESULT hr;
    HANDLE handle;
    DWORD read;
    char contents[64];
    unsigned i;
    static const NameCollisionOption options[] = {NameCollisionOption_FailIfExists,
        NameCollisionOption_FailIfExists, NameCollisionOption_ReplaceExisting, NameCollisionOption_GenerateUniqueName};

    swprintf(directory, _countof(directory), L"%s.copies-%u", source_path, (unsigned)(8 * sizeof(void *)));
    CHECK(CreateDirectoryW(directory, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    CHECK(WindowsCreateString(L"Windows.Storage.StorageFolder",29,&class_name) == S_OK);
    hr = RoGetActivationFactory(class_name,&IID_IStorageFolderStatics,(void **)&statics);
    CHECK(hr == S_OK && statics);
    WindowsDeleteString(class_name);
    if (!statics) return;
    CHECK(WindowsCreateString(directory,wcslen(directory),&path) == S_OK);
    hr = IStorageFolderStatics_GetFolderFromPathAsync(statics,path,&folder_op);
    CHECK(hr == S_OK && folder_op);
    WindowsDeleteString(path);
    IStorageFolderStatics_Release(statics);
    if (!folder_op) return;
    CHECK(wait_for((IInspectable *)folder_op) == S_OK);
    CHECK(IAsyncOperation_StorageFolder_GetResults(folder_op,&folder) == S_OK && folder);
    IAsyncOperation_StorageFolder_Release(folder_op);
    if (!folder) return;
    CHECK(WindowsCreateString(L"copied.mcworld",14,&name) == S_OK);
    for (i = 0; i < _countof(options); ++i)
    {
        CHECK(IStorageFile_CopyOverload(source,folder,name,options[i],&operation) == S_OK && operation);
        if (!operation) break;
        CHECK(wait_for((IInspectable *)operation) == S_OK);
        copy = NULL;
        hr = IAsyncOperation_StorageFile_GetResults(operation,&copy);
        if (i == 1) CHECK((hr == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS) || hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) && !copy);
        else
        {
            CHECK(hr == S_OK && copy);
            swprintf(destination, _countof(destination), L"%s\\%s", directory, i == 3 ? L"copied (2).mcworld" : L"copied.mcworld");
            if (copy) check_file(copy,destination);
            handle = CreateFileW(destination,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
            CHECK(handle != INVALID_HANDLE_VALUE);
            if (handle != INVALID_HANDLE_VALUE)
            {
                CHECK(ReadFile(handle,contents,sizeof(contents),&read,NULL));
                CHECK(read == sizeof("WineGDK picker test\n") - 1 && !memcmp(contents,"WineGDK picker test\n",read));
                CloseHandle(handle);
            }
            if (copy) IStorageFile_Release(copy);
        }
        IAsyncOperation_StorageFile_Release(operation); operation = NULL;
    }
    WindowsDeleteString(name);
    CHECK(WindowsCreateString(L"..\\escape.mcworld",17,&name) == S_OK);
    CHECK(IStorageFile_CopyOverloadDefaultOptions(source,folder,name,&operation) == E_INVALIDARG && !operation);
    WindowsDeleteString(name);
    CHECK(IStorageFile_CopyOverloadDefaultNameAndOptions(source,folder,&operation) == S_OK && operation);
    if (operation)
    {
        CHECK(wait_for((IInspectable *)operation) == S_OK);
        CHECK(IAsyncOperation_StorageFile_GetResults(operation,&copy) == S_OK && copy);
        if (copy) IStorageFile_Release(copy);
        IAsyncOperation_StorageFile_Release(operation);
    }
    check_file_streams(folder);
    check_file_io(folder);
    IStorageFolder_Release(folder);
}

int wmain(int argc, WCHAR **argv)
{
    IInspectable *instance = NULL;
    IFileOpenPicker *picker = NULL;
    IVector_HSTRING *filter = NULL;
    IAsyncOperation_StorageFile *single = NULL;
    IAsyncOperation_IVectorView_StorageFile *multi = NULL;
    IVectorView_StorageFile *files = NULL;
    IStorageFile *file = NULL;
    HSTRING name = NULL, extension = NULL;
    HRESULT hr;
    UINT32 count, i;
    setvbuf(stdout,NULL,_IONBF,0);
    if (argc != 3) return 2;
    CHECK(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
    CHECK(WindowsCreateString(L"Windows.Storage.Pickers.FileOpenPicker",38,&name) == S_OK);
    hr = RoActivateInstance(name, &instance);
    printf("FileOpenPicker activation: %#lx\n", hr);
    CHECK(hr == S_OK && instance);
    if (!instance) return 1;
    CHECK(IInspectable_QueryInterface(instance, &IID_IFileOpenPicker, (void **)&picker) == S_OK);
    IInspectable_Release(instance);
    if (!picker) return 1;
    CHECK(IFileOpenPicker_get_FileTypeFilter(picker, &filter) == S_OK);
    CHECK(IFileOpenPicker_PickSingleFileAsync(picker, &single) == E_INVALIDARG && !single);
    CHECK(WindowsCreateString(L".mcworld",8,&extension) == S_OK);
    CHECK(IVector_HSTRING_Append(filter, extension) == S_OK);
    WindowsDeleteString(extension);
    CHECK(IFileOpenPicker_put_CommitButtonText(picker, name) == S_OK);
    CHECK(IFileOpenPicker_put_ViewMode(picker, PickerViewMode_Thumbnail) == S_OK);
    CHECK(IFileOpenPicker_put_SuggestedStartLocation(picker, PickerLocationId_Downloads) == S_OK);
    for (i = 0; i < 7; ++i)
    {
        BOOL multiple = i == 1 || i == 3;
        hr = multiple ? IFileOpenPicker_PickMultipleFilesAsync(picker,&multi) : IFileOpenPicker_PickSingleFileAsync(picker,&single);
        CHECK(hr == S_OK);
        if (FAILED(hr)) break;
        if (i == 6)
        {
            IAsyncInfo *info = NULL;
            AsyncStatus status;
            CHECK(IAsyncOperation_StorageFile_QueryInterface(single, &IID_IAsyncInfo, (void **)&info) == S_OK);
            if (info)
            {
                Sleep(200);
                CHECK(IAsyncInfo_Cancel(info) == S_OK);
                Sleep(300);
                CHECK(IAsyncInfo_get_Status(info,&status) == S_OK && status == Canceled);
                CHECK(IAsyncOperation_StorageFile_GetResults(single,&file) == E_ABORT && !file);
                IAsyncInfo_Release(info);
            }
            IAsyncOperation_StorageFile_Release(single); single=NULL;
            continue;
        }
        CHECK(wait_for((IInspectable *)(multiple ? (void *)multi : (void *)single)) == S_OK);
        if (multiple)
        {
            CHECK(IAsyncOperation_IVectorView_StorageFile_GetResults(multi,&files) == S_OK && files);
            if (files)
            {
                CHECK(IVectorView_StorageFile_get_Size(files,&count) == S_OK && count == (i == 1 ? 2u : 0u));
                for (UINT32 j = 0; j < count; ++j)
                { CHECK(IVectorView_StorageFile_GetAt(files,j,&file) == S_OK); check_file(file,argv[j+1]); if(file) IStorageFile_Release(file); file=NULL; }
                IVectorView_StorageFile_Release(files); files=NULL;
            }
            IAsyncOperation_IVectorView_StorageFile_Release(multi); multi=NULL;
        }
        else
        {
            file = NULL;
            hr = IAsyncOperation_StorageFile_GetResults(single,&file);
            if (i >= 4) CHECK(FAILED(hr) && !file);
            else if (i == 2) CHECK(hr == S_OK && !file);
            else { CHECK(hr == S_OK); check_file(file,argv[1]); if (file) { check_copy(file,argv[1]); check_cached_file(file); check_access_cache(file,argv[1]); } }
            if (file) IStorageFile_Release(file);
            file=NULL;
            IAsyncOperation_StorageFile_Release(single); single=NULL;
        }
        printf("scenario %u complete\n",i);
    }
    IVector_HSTRING_Release(filter);
    IFileOpenPicker_Release(picker);
    WindowsDeleteString(name);
    RoUninitialize();
    printf("picker: %u failures\n",failures);
    return !!failures;
}
