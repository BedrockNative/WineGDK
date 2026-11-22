/* Exercise the NT relative-open and rename paths as well as the Win32 APIs. */
#include <windows.h>
#include <winternl.h>

#define CHECK(expr) do { if (!(expr)) ExitProcess(__LINE__); } while (0)

static HANDLE relative_open(HANDLE parent, WCHAR *name, ULONG disposition, ULONG options)
{
    UNICODE_STRING string;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    HANDLE result;

    RtlInitUnicodeString(&string, name);
    InitializeObjectAttributes(&attr, &string, OBJ_CASE_INSENSITIVE, parent, NULL);
    CHECK(!NtCreateFile(&result, GENERIC_READ | GENERIC_WRITE | DELETE | SYNCHRONIZE,
                        &attr, &io, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        disposition, options | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0));
    return result;
}

void mainCRTStartup(void)
{
    WCHAR appdata[MAX_PATH], buf[8];
    WIN32_FIND_DATAW find;
    HANDLE parent, dir, file, search;
    DWORD bytes;
    IO_STATUS_BLOCK io;
    union { FILE_RENAME_INFORMATION info; BYTE storage[256]; } rename;
    WCHAR target[] = L"renamed.dat";

    CHECK(GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH));
    CHECK(SetCurrentDirectoryW(appdata));
    parent = CreateFileW(appdata, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(parent != INVALID_HANDLE_VALUE);
    dir = relative_open(parent, L"MiNeCrAfT bEdRoCk", FILE_OPEN, FILE_DIRECTORY_FILE);
    file = relative_open(dir, L"scratch.dat", FILE_CREATE, FILE_NON_DIRECTORY_FILE);
    CHECK(WriteFile(file, "ok", 2, &bytes, NULL) && bytes == 2);
    rename.info.ReplaceIfExists = FALSE;
    rename.info.RootDirectory = dir;
    rename.info.FileNameLength = sizeof(target) - sizeof(WCHAR);
    CopyMemory(rename.info.FileName, target, sizeof(target));
    CHECK(!NtSetInformationFile(file, &io, &rename, sizeof(rename), FileRenameInformation));
    CloseHandle(file);
    CloseHandle(dir);
    CloseHandle(parent);

    search = FindFirstFileW(L"Minecraft Bedrock Preview\\*.dat", &find);
    CHECK(search != INVALID_HANDLE_VALUE);
    FindClose(search);
    file = CreateFileW(L"Minecraft Bedrock Preview\\RENAMED.DAT", GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, 0, NULL);
    CHECK(file != INVALID_HANDLE_VALUE);
    CHECK(ReadFile(file, buf, 2, &bytes, NULL) && bytes == 2 && !memcmp(buf, "ok", 2));
    CloseHandle(file);
    CHECK(DeleteFileW(L"Minecraft Bedrock\\renamed.dat"));
    CHECK(CreateDirectoryW(L"Minecraft Bedrock Preview\\World", NULL));
    CHECK(MoveFileW(L"Minecraft Bedrock\\World", L"Minecraft Bedrock\\SavedWorld"));
    CHECK(CreateDirectoryW(L"Minecraft Bedrock Extra", NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    ExitProcess(0);
}
