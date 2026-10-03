/* File creation and stream round trips, shared by the picker integration probe. */
static IBuffer *make_buffer(const char *data, UINT32 length)
{
    IBufferFactory *factory = NULL;
    IBuffer *buffer = NULL;
    IBufferByteAccess *access = NULL;
    HSTRING name;
    BYTE *bytes;
    CHECK(WindowsCreateString(L"Windows.Storage.Streams.Buffer",30,&name) == S_OK);
    CHECK(RoGetActivationFactory(name,&IID_IBufferFactory,(void **)&factory) == S_OK && factory);
    WindowsDeleteString(name);
    if (!factory) return NULL;
    CHECK(IBufferFactory_Create(factory,32,&buffer) == S_OK && buffer);
    IBufferFactory_Release(factory);
    if (!buffer) return NULL;
    CHECK(IBuffer_QueryInterface(buffer,&IID_IBufferByteAccess,(void **)&access) == S_OK && access);
    if (access)
    {
        CHECK(IBufferByteAccess_Buffer(access,&bytes) == S_OK);
        if (length) memcpy(bytes,data,length);
        IBufferByteAccess_Release(access);
    }
    CHECK(IBuffer_put_Length(buffer,length) == S_OK);
    return buffer;
}
static IStorageFile *create_test_file(IStorageFolder *folder, const WCHAR *name, CreationCollisionOption option, HRESULT expected)
{
    IAsyncOperation_StorageFile *operation = NULL;
    IStorageFile *file = NULL;
    HSTRING str;
    HRESULT hr;
    CHECK(WindowsCreateString(name,wcslen(name),&str) == S_OK);
    CHECK(IStorageFolder_CreateFileAsync(folder,str,option,&operation) == S_OK && operation);
    WindowsDeleteString(str); /* The worker must retain the name. */
    if (!operation) return NULL;
    CHECK(wait_for((IInspectable *)operation) == S_OK);
    hr = IAsyncOperation_StorageFile_GetResults(operation,&file);
    CHECK(hr == expected || (expected == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS) && hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)));
    CHECK(SUCCEEDED(expected) ? file != NULL : file == NULL);
    IAsyncOperation_StorageFile_Release(operation);
    return file;
}
static IRandomAccessStream *open_test_stream(IStorageFile *file, FileAccessMode mode)
{
    IAsyncOperation_IRandomAccessStream *operation = NULL;
    IRandomAccessStream *stream = NULL;
    CHECK(IStorageFile_OpenAsync(file,mode,&operation) == S_OK && operation);
    if (!operation) return NULL;
    CHECK(wait_for((IInspectable *)operation) == S_OK);
    CHECK(IAsyncOperation_IRandomAccessStream_GetResults(operation,&stream) == S_OK && stream);
    IAsyncOperation_IRandomAccessStream_Release(operation);
    return stream;
}
static void write_test_buffer(IOutputStream *stream, const char *data, UINT32 length)
{
    IBuffer *buffer = make_buffer(data,length);
    IAsyncOperationWithProgress_UINT32_UINT32 *operation = NULL;
    struct { UINT32 count, guard; } result = {0,0xaabbccdd};
    if (!buffer) return;
    CHECK(IOutputStream_WriteAsync(stream,buffer,&operation) == S_OK && operation);
    IBuffer_Release(buffer); /* Async operation owns the buffer until completion. */
    if (!operation) return;
    CHECK(wait_for((IInspectable *)operation) == S_OK);
    CHECK(IAsyncOperationWithProgress_UINT32_UINT32_GetResults(operation,NULL) == E_POINTER);
    CHECK(IAsyncOperationWithProgress_UINT32_UINT32_GetResults(operation,&result.count) == S_OK);
    CHECK(result.count == length && result.guard == 0xaabbccdd);
    CHECK(IAsyncOperationWithProgress_UINT32_UINT32_GetResults(operation,&result.count) == S_OK && result.count == length);
    IAsyncOperationWithProgress_UINT32_UINT32_Release(operation);
}
static void check_file_streams(IStorageFolder *folder)
{
    IStorageFile *file, *other;
    IRandomAccessStream *stream, *clone, *readonly;
    IOutputStream *output = NULL, *at = NULL;
    IInputStream *input = NULL;
    IClosable *closable = NULL;
    IAsyncOperation_boolean *flush = NULL;
    IAsyncOperationWithProgress_IBuffer_UINT32 *read = NULL;
    IAsyncOperationWithProgress_UINT32_UINT32 *write = NULL;
    IAsyncOperation_StorageFile *invalid = (void *)0xdeadbeef;
    IBuffer *buffer, *result = NULL;
    IBufferByteAccess *access;
    UINT64 size, position;
    UINT32 length;
    boolean can;
    BYTE *bytes;
    HSTRING name;
    struct { boolean value; BYTE guard[3]; } flushed = {FALSE,{0xab,0xcd,0xef}};
    static const WCHAR *invalid_names[] = {L"",L"../escape",L"x/y",L"x.",L"x ",L"a:b"};
    unsigned i;
    for (i = 0; i < _countof(invalid_names); ++i)
    {
        CHECK(WindowsCreateString(invalid_names[i],wcslen(invalid_names[i]),&name) == S_OK);
        CHECK(IStorageFolder_CreateFileAsync(folder,name,CreationCollisionOption_ReplaceExisting,&invalid) == E_INVALIDARG && !invalid);
        WindowsDeleteString(name);
    }
    file = create_test_file(folder,L"stream-test.bin",CreationCollisionOption_FailIfExists,S_OK);
    if (!file) return;
    stream = open_test_stream(file,FileAccessMode_ReadWrite);
    if (!stream) { IStorageFile_Release(file); return; }
    CHECK(IRandomAccessStream_get_CanWrite(stream,&can) == S_OK && can);
    CHECK(IRandomAccessStream_QueryInterface(stream,&IID_IOutputStream,(void **)&output) == S_OK && output);
    if (output) write_test_buffer(output,"abcDEF\n",7);
    CHECK(IRandomAccessStream_get_Size(stream,&size) == S_OK && size == 7);
    CHECK(IRandomAccessStream_CloneStream(stream,&clone) == S_OK && clone);
    if (clone)
    {
        CHECK(IRandomAccessStream_get_Position(clone,&position) == S_OK && !position);
        CHECK(IRandomAccessStream_Seek(clone,2) == S_OK);
        CHECK(IRandomAccessStream_get_Position(stream,&position) == S_OK && position == 7);
        IRandomAccessStream_Release(clone);
    }
    CHECK(IRandomAccessStream_GetOutputStreamAt(stream,3,&at) == S_OK && at);
    if (at) { write_test_buffer(at,"xyz",3); IOutputStream_Release(at); }
    CHECK(IRandomAccessStream_get_Position(stream,&position) == S_OK && position == 7);
    CHECK(IRandomAccessStream_put_Size(stream,5) == S_OK);
    CHECK(IRandomAccessStream_get_Position(stream,&position) == S_OK && position == 7);
    CHECK(IRandomAccessStream_get_Size(stream,&size) == S_OK && size == 5);
    if (output)
    {
        CHECK(IOutputStream_FlushAsync(output,&flush) == S_OK && flush);
        if (flush)
        {
            CHECK(wait_for((IInspectable *)flush) == S_OK);
            CHECK(IAsyncOperation_boolean_GetResults(flush,&flushed.value) == S_OK && flushed.value == TRUE);
            CHECK(flushed.guard[0] == 0xab && flushed.guard[1] == 0xcd && flushed.guard[2] == 0xef);
            IAsyncOperation_boolean_Release(flush);
        }
        IOutputStream_Release(output); output = NULL;
    }
    readonly = open_test_stream(file,FileAccessMode_Read);
    if (readonly)
    {
        CHECK(IRandomAccessStream_get_CanWrite(readonly,&can) == S_OK && !can);
        CHECK(IRandomAccessStream_put_Size(readonly,0) == E_ACCESSDENIED);
        CHECK(IRandomAccessStream_GetOutputStreamAt(readonly,0,&output) == E_ACCESSDENIED && !output);
        CHECK(IRandomAccessStream_QueryInterface(readonly,&IID_IInputStream,(void **)&input) == S_OK && input);
        buffer = make_buffer(NULL,0);
        if (input && buffer)
        {
            CHECK(IInputStream_ReadAsync(input,buffer,16,InputStreamOptions_None,&read) == S_OK && read);
            if (read)
            {
                CHECK(wait_for((IInspectable *)read) == S_OK);
                CHECK(IAsyncOperationWithProgress_IBuffer_UINT32_GetResults(read,&result) == S_OK && result);
                if (result)
                {
                    CHECK(IBuffer_get_Length(result,&length) == S_OK && length == 5);
                    CHECK(IBuffer_QueryInterface(result,&IID_IBufferByteAccess,(void **)&access) == S_OK);
                    CHECK(IBufferByteAccess_Buffer(access,&bytes) == S_OK && !memcmp(bytes,"abcxy",5));
                    IBufferByteAccess_Release(access); IBuffer_Release(result); result = NULL;
                }
                IAsyncOperationWithProgress_IBuffer_UINT32_Release(read); read = NULL;
            }
            CHECK(IInputStream_ReadAsync(input,buffer,16,InputStreamOptions_None,&read) == S_OK && read);
            if (read)
            {
                CHECK(wait_for((IInspectable *)read) == S_OK);
                CHECK(IAsyncOperationWithProgress_IBuffer_UINT32_GetResults(read,&result) == S_OK && result);
                if (result) { CHECK(IBuffer_get_Length(result,&length) == S_OK && !length); IBuffer_Release(result); result = NULL; }
                IAsyncOperationWithProgress_IBuffer_UINT32_Release(read); read = NULL;
            }
        }
        if (buffer) IBuffer_Release(buffer);
        if (input) IInputStream_Release(input);
        IRandomAccessStream_Release(readonly);
    }
    CHECK(IRandomAccessStream_QueryInterface(stream,&IID_IClosable,(void **)&closable) == S_OK && closable);
    if (closable) { CHECK(IClosable_Close(closable) == S_OK); CHECK(IClosable_Close(closable) == S_OK); IClosable_Release(closable); }
    CHECK(IRandomAccessStream_get_Size(stream,&size) == RO_E_CLOSED);
    CHECK(IRandomAccessStream_QueryInterface(stream,&IID_IOutputStream,(void **)&output) == S_OK && output);
    buffer = make_buffer("x",1);
    if (output && buffer)
    {
        CHECK(IOutputStream_WriteAsync(output,buffer,&write) == S_OK && write);
        if (write)
        {
            CHECK(wait_for((IInspectable *)write) == S_OK);
            CHECK(IAsyncOperationWithProgress_UINT32_UINT32_GetResults(write,&length) == RO_E_CLOSED);
            IAsyncOperationWithProgress_UINT32_UINT32_Release(write);
        }
    }
    if (buffer) IBuffer_Release(buffer);
    if (output) IOutputStream_Release(output);
    IRandomAccessStream_Release(stream); IStorageFile_Release(file);
    other = create_test_file(folder,L"stream-test.bin",CreationCollisionOption_OpenIfExists,S_OK);
    if (other) { stream = open_test_stream(other,FileAccessMode_Read); if (stream) { CHECK(IRandomAccessStream_get_Size(stream,&size) == S_OK && size == 5); IRandomAccessStream_Release(stream); } IStorageFile_Release(other); }
    {
        IStorageItem *item = NULL;
        HSTRING file_path = NULL;
        other = create_test_file(folder,L"stream-test.bin",CreationCollisionOption_OpenIfExists,S_OK);
        if (other)
        {
            CHECK(IStorageFile_QueryInterface(other,&IID_IStorageItem,(void **)&item) == S_OK);
            CHECK(IStorageItem_get_Path(item,&file_path) == S_OK);
            CHECK(SetFileAttributesW(WindowsGetStringRawBuffer(file_path,NULL),FILE_ATTRIBUTE_READONLY));
            IStorageFile_Release(other);
            other = create_test_file(folder,L"stream-test.bin",CreationCollisionOption_OpenIfExists,S_OK);
            if (other) IStorageFile_Release(other);
            CHECK(SetFileAttributesW(WindowsGetStringRawBuffer(file_path,NULL),FILE_ATTRIBUTE_NORMAL));
            WindowsDeleteString(file_path); IStorageItem_Release(item);
        }
    }
    other = create_test_file(folder,L"stream-test.bin",CreationCollisionOption_FailIfExists,HRESULT_FROM_WIN32(ERROR_FILE_EXISTS));
    if (other) IStorageFile_Release(other);
    other = create_test_file(folder,L"stream-test.bin",CreationCollisionOption_GenerateUniqueName,S_OK);
    if (other) IStorageFile_Release(other);
    other = create_test_file(folder,L"stream-test.bin",CreationCollisionOption_ReplaceExisting,S_OK);
    if (other) { stream = open_test_stream(other,FileAccessMode_Read); if (stream) { CHECK(IRandomAccessStream_get_Size(stream,&size) == S_OK && !size); IRandomAccessStream_Release(stream); } IStorageFile_Release(other); }
}
static void check_file_io(IStorageFolder *folder)
{
    IFileIOStatics *statics = NULL;
    IStorageFile *file = create_test_file(folder,L"fileio.bin",CreationCollisionOption_FailIfExists,S_OK);
    IAsyncAction *action = NULL;
    IAsyncOperation_IBuffer *read = NULL;
    IBuffer *buffer = NULL, *result = NULL;
    IBufferByteAccess *access;
    HSTRING name;
    BYTE bytes[] = {0,1,2,0xff,4,0,6}, *data;
    UINT32 length;
    unsigned i;
    CHECK(WindowsCreateString(L"Windows.Storage.FileIO",22,&name) == S_OK);
    CHECK(RoGetActivationFactory(name,&IID_IFileIOStatics,(void **)&statics) == S_OK && statics);
    WindowsDeleteString(name);
    if (!statics || !file) goto done;
    CHECK(IFileIOStatics_WriteBytesAsync(statics,file,1,NULL,&action) == E_INVALIDARG && !action);
    CHECK(IFileIOStatics_ReadBufferAsync(statics,NULL,&read) == E_INVALIDARG && !read);
    for (i = 0; i < 3; ++i)
    {
        if (!i) CHECK(IFileIOStatics_WriteBytesAsync(statics,file,sizeof(bytes),bytes,&action) == S_OK && action);
        else if (i == 1)
        {
            buffer = make_buffer("xy",2);
            CHECK(IFileIOStatics_WriteBufferAsync(statics,file,buffer,&action) == S_OK && action);
            if (buffer) { IBuffer_Release(buffer); buffer = NULL; }
        }
        else CHECK(IFileIOStatics_WriteBytesAsync(statics,file,0,NULL,&action) == S_OK && action);
        if (!action) break;
        CHECK(wait_for((IInspectable *)action) == S_OK);
        CHECK(IAsyncAction_GetResults(action) == S_OK);
        CHECK(IAsyncAction_GetResults(action) == S_OK);
        IAsyncAction_Release(action); action = NULL;
        CHECK(IFileIOStatics_ReadBufferAsync(statics,file,&read) == S_OK && read);
        if (!read) break;
        CHECK(wait_for((IInspectable *)read) == S_OK);
        CHECK(IAsyncOperation_IBuffer_GetResults(read,&result) == S_OK && result);
        if (result)
        {
            CHECK(IBuffer_get_Length(result,&length) == S_OK && length == (!i ? sizeof(bytes) : i == 1 ? 2 : 0));
            CHECK(IBuffer_QueryInterface(result,&IID_IBufferByteAccess,(void **)&access) == S_OK);
            CHECK(IBufferByteAccess_Buffer(access,&data) == S_OK);
            if (!i) CHECK(!memcmp(data,bytes,sizeof(bytes)));
            else if (i == 1) CHECK(!memcmp(data,"xy",2));
            IBufferByteAccess_Release(access); IBuffer_Release(result); result = NULL;
        }
        IAsyncOperation_IBuffer_Release(read); read = NULL;
    }
done:
    if (statics) IFileIOStatics_Release(statics);
    if (file) IStorageFile_Release(file);
}
static void check_access_cache(IStorageFile *file, const WCHAR *path)
{
    IStorageApplicationPermissionsStatics *statics = NULL;
    IStorageItemAccessList *list = NULL;
    IStorageItem *item = NULL;
    IStorageFile *cached = NULL;
    IAsyncOperation_StorageFile *operation = NULL;
    HSTRING name, token = NULL;
    boolean contains;
    UINT32 maximum;
    static const WCHAR cls[] = L"Windows.Storage.AccessCache.StorageApplicationPermissions";
    CHECK(WindowsCreateString(cls,_countof(cls)-1,&name) == S_OK);
    CHECK(RoGetActivationFactory(name,&IID_IStorageApplicationPermissionsStatics,(void **)&statics) == S_OK && statics);
    WindowsDeleteString(name);
    if (!statics) return;
    CHECK(IStorageApplicationPermissionsStatics_get_FutureAccessList(statics,&list) == S_OK && list);
    CHECK(IStorageFile_QueryInterface(file,&IID_IStorageItem,(void **)&item) == S_OK && item);
    if (!list || !item) goto done;
    CHECK(IStorageItemAccessList_get_MaximumItemsAllowed(list,&maximum) == S_OK && maximum == 1000);
    CHECK(IStorageItemAccessList_AddOverloadDefaultMetadata(list,item,&token) == S_OK && token);
    if (!token) goto done;
    CHECK(IStorageItemAccessList_ContainsItem(list,token,&contains) == S_OK && contains);
    IStorageItemAccessList_Release(list); list = NULL;
    CHECK(IStorageApplicationPermissionsStatics_get_FutureAccessList(statics,&list) == S_OK && list);
    if (!list) goto done;
    CHECK(IStorageItemAccessList_ContainsItem(list,token,&contains) == S_OK && contains);
    CHECK(IStorageItemAccessList_GetFileAsync(list,token,&operation) == S_OK && operation);
    if (operation)
    {
        CHECK(wait_for((IInspectable *)operation) == S_OK);
        CHECK(IAsyncOperation_StorageFile_GetResults(operation,&cached) == S_OK && cached);
        if (cached) { check_file(cached,path); IStorageFile_Release(cached); }
        IAsyncOperation_StorageFile_Release(operation); operation = NULL;
    }
    CHECK(IStorageItemAccessList_AddOrReplaceOverloadDefaultMetadata(list,token,item) == S_OK);
    CHECK(IStorageItemAccessList_Remove(list,token) == S_OK);
    CHECK(IStorageItemAccessList_ContainsItem(list,token,&contains) == S_OK && !contains);
    CHECK(IStorageItemAccessList_GetFileAsync(list,token,&operation) == S_OK && operation);
    if (operation)
    {
        CHECK(wait_for((IInspectable *)operation) == S_OK);
        cached = (void *)0xdeadbeef;
        CHECK(IAsyncOperation_StorageFile_GetResults(operation,&cached) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && !cached);
        IAsyncOperation_StorageFile_Release(operation);
    }
done:
    WindowsDeleteString(token);
    if (item) IStorageItem_Release(item);
    if (list) IStorageItemAccessList_Release(list);
    IStorageApplicationPermissionsStatics_Release(statics);
}
