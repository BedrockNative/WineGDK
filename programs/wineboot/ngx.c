/* Discover the host NVIDIA NGX driver without redistributing or copying it.
 * Licensed under the GNU LGPL, version 2.1 or later; see COPYING.LIB.
 */
#include <stdlib.h>
#include <stdio.h>
#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <wine/debug.h>

WINE_DEFAULT_DEBUG_CHANNEL(wineboot);

/* Inspect headers, never execute a library during prefix setup. */
#ifdef _WIN64
static BOOL ngx_driver_exists(const WCHAR *directory)
{
    const WCHAR *names[] = {L"_nvngx.dll", L"nvngx.dll"};
    WCHAR filename[32768];
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(names); ++i)
    {
        IMAGE_DOS_HEADER dos;
        IMAGE_NT_HEADERS64 nt;
        HANDLE file;
        DWORD read;
        BOOL valid = FALSE;
        if (wcslen(directory) + wcslen(names[i]) + 2 > ARRAY_SIZE(filename)) continue;
        swprintf(filename, ARRAY_SIZE(filename), L"%s\\%s", directory, names[i]);
        file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, 0, NULL);
        if (file == INVALID_HANDLE_VALUE) continue;
        if (ReadFile(file, &dos, sizeof(dos), &read, NULL) && read == sizeof(dos) &&
            dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew >= sizeof(dos) &&
            dos.e_lfanew < 1024 * 1024 &&
            SetFilePointer(file, dos.e_lfanew, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
            ReadFile(file, &nt, sizeof(nt), &read, NULL) && read == sizeof(nt))
            valid = nt.Signature == IMAGE_NT_SIGNATURE &&
                    nt.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
                    (nt.FileHeader.Characteristics & IMAGE_FILE_DLL) &&
                    nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        CloseHandle(file);
        if (valid) return TRUE;
    }
    return FALSE;
}
#endif

void setup_ngx_driver(void)
{
#ifdef _WIN64
    static const char *paths[] = {
        "/usr/lib/nvidia/wine", "/usr/lib64/nvidia/wine",
        "/usr/lib/x86_64-linux-gnu/nvidia/wine",
        "/usr/lib/x86_64-linux-gnu/nvidia/current/wine"
    };
    WCHAR * (CDECL *get_dos_path)(const char *);
    const char *override = getenv("NVIDIA_WINE_DLL_DIR");
    WCHAR *directory = NULL;
    unsigned int i;
    HKEY key;

    /* Keep user/third-party discovery settings, including explicit disables. */
    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\NVIDIA Corporation\\Global\\NGXCore",
                      0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key))
    {
        LONG status = RegQueryValueExW(key, L"FullPath", NULL, NULL, NULL, NULL);
        RegCloseKey(key);
        if (status != ERROR_FILE_NOT_FOUND) return;
    }
    get_dos_path = (void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "wine_get_dos_file_name");
    if (!get_dos_path) return;
    /* An explicit path is authoritative; don't silently substitute another driver. */
    for (i = 0; i < (override ? 1 : ARRAY_SIZE(paths)); ++i)
    {
        const char *path = override ? override : paths[i];
        if (path[0] != '/') continue;
        directory = get_dos_path(path);
        if (directory && ngx_driver_exists(directory)) break;
        HeapFree(GetProcessHeap(), 0, directory);
        directory = NULL;
    }
    if (!directory) return;
    if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\NVIDIA Corporation\\Global\\NGXCore",
                        0, NULL, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                        NULL, &key, NULL))
    {
        if (RegQueryValueExW(key, L"FullPath", NULL, NULL, NULL, NULL) == ERROR_FILE_NOT_FOUND)
        {
            RegSetValueExW(key, L"FullPath", 0, REG_SZ, (const BYTE *)directory,
                           (wcslen(directory) + 1) * sizeof(WCHAR));
            TRACE("Registered host NGX driver directory %s\n", debugstr_w(directory));
        }
        RegCloseKey(key);
    }
    HeapFree(GetProcessHeap(), 0, directory);
#endif
}
