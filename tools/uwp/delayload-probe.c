/* Exercise delay-import resolution using a synthetic mapped PE image. */
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

struct image
{
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS nt;
    IMAGE_DELAYLOAD_DESCRIPTOR descriptors[2];
    HMODULE module;
    IMAGE_THUNK_DATA imports[3], iat[3];
    char dll[128];
    struct { WORD hint; char name[128]; } function;
};

static WORD get_ordinal(HMODULE module, const char *name)
{
    BYTE *base = (BYTE *)module;
    IMAGE_DOS_HEADER *dos = (void *)base;
    IMAGE_NT_HEADERS *nt = (void *)(base + dos->e_lfanew);
    IMAGE_EXPORT_DIRECTORY *exports = (void *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    DWORD *names = (void *)(base + exports->AddressOfNames);
    WORD *ordinals = (void *)(base + exports->AddressOfNameOrdinals);
    DWORD i;
    for (i = 0; i < exports->NumberOfNames; ++i)
        if (!strcmp((char *)base + names[i], name)) return (WORD)(exports->Base + ordinals[i]);
    return 0;
}

int main(void)
{
    LONG (WINAPI *resolve)(void *, const char *, ULONG);
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    struct image *image = VirtualAlloc(NULL, sizeof(*image), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    LONG status;
    int failures = 0;
#define CHECK(test) do { if (!(test)) { printf("FAIL line %d: %s\n", __LINE__, #test); ++failures; } } while (0)
    resolve = (void *)GetProcAddress(kernel32, "ResolveDelayLoadsFromDll");
    if (!resolve || !image) { puts("Resolver or allocation unavailable"); return 1; }
    image->dos.e_magic = IMAGE_DOS_SIGNATURE;
    image->dos.e_lfanew = offsetof(struct image, nt);
    image->nt.Signature = IMAGE_NT_SIGNATURE;
    image->nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR_MAGIC;
    image->nt.OptionalHeader.SizeOfImage = sizeof(*image);
    image->nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    image->nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].VirtualAddress = offsetof(struct image, descriptors);
    image->nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].Size = sizeof(image->descriptors);
    image->descriptors[0].Attributes.RvaBased = 1;
    image->descriptors[0].DllNameRVA = offsetof(struct image, dll);
    image->descriptors[0].ModuleHandleRVA = offsetof(struct image, module);
    image->descriptors[0].ImportNameTableRVA = offsetof(struct image, imports);
    image->descriptors[0].ImportAddressTableRVA = offsetof(struct image, iat);
    strcpy(image->dll, "kernel32.dll");
    strcpy(image->function.name, "GetCurrentProcessId");
    image->imports[0].u1.AddressOfData = offsetof(struct image, function);
    image->imports[1].u1.Ordinal = IMAGE_ORDINAL_FLAG | get_ordinal(kernel32, "GetTickCount");
    status = resolve(image, "KERNEL32.DLL", 0);
    CHECK(status == 0 && image->module);
    CHECK(image->iat[0].u1.Function == (ULONG_PTR)GetProcAddress(kernel32, "GetCurrentProcessId"));
    CHECK(image->iat[1].u1.Function == (ULONG_PTR)GetProcAddress(kernel32, "GetTickCount"));
    CHECK(resolve(image, "kernel32.dll", 0) == 0);
    CHECK(resolve(image, "not-in-import-table.dll", 0) == (LONG)0xc0000135);
    CHECK(resolve(image, "kernel32.dll", 1) == (LONG)0xc000000d);
    CHECK(resolve(NULL, "kernel32.dll", 0) == (LONG)0xc000000d);
    strcpy(image->function.name, "WineGDKMissingFunction");
    CHECK(resolve(image, "kernel32.dll", 0) == (LONG)0xc000007a);
    if (image->module) FreeLibrary(image->module);
    image->module = NULL;
    strcpy(image->dll, "WineGDKMissingLibrary.dll");
    CHECK(resolve(image, image->dll, 0) == (LONG)0xc0000135);
    VirtualFree(image, 0, MEM_RELEASE);
    printf("Delay-load probe: %d failure(s)\n", failures);
    return failures != 0;
}
