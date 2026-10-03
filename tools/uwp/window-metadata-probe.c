/* Hidden CoreWindow metadata probe. Run from a disposable fixture package. */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_UI_Core
#define WIDL_using_Windows_Devices_Input
#define WIDL_using_Windows_UI_Input
#include "initguid.h"
#include "windows.ui.core.h"
#include "corewindow.h"
#include "roapi.h"
#include "winuser.h"
#include "wingdi.h"
#include <stdio.h>

static unsigned int failures;
static BOOL check_color;
static DWORD expected_color;
#define CHECK(test) do { if (!(test)) { printf("FAIL line %d: %s\n", __LINE__, #test); ++failures; } } while (0)

static void check_icon(HWND hwnd, WPARAM type, BOOL expected, int width, int height)
{
    HICON icon = (HICON)SendMessageW(hwnd, WM_GETICON, type, 96);
    ICONINFO info;
    BITMAP bitmap;
    CHECK(!!icon == !!expected);
    if (!icon) return;
    if (!GetIconInfo(icon, &info)) { CHECK(FALSE); return; }
    if (info.hbmColor)
    {
        CHECK(GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap) == sizeof(bitmap));
        CHECK(bitmap.bmWidth == width && bitmap.bmHeight == height);
        if (check_color)
        {
            BITMAPINFO dib = {0};
            DWORD *pixels = calloc(width * height, sizeof(*pixels));
            HDC dc = CreateCompatibleDC(NULL);
            int i;
            dib.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            dib.bmiHeader.biWidth = width;
            dib.bmiHeader.biHeight = -height;
            dib.bmiHeader.biPlanes = 1;
            dib.bmiHeader.biBitCount = 32;
            dib.bmiHeader.biCompression = BI_RGB;
            CHECK(pixels && dc);
            if (pixels && dc)
            {
                CHECK(GetDIBits(dc, info.hbmColor, 0, height, pixels, &dib, DIB_RGB_COLORS) == height);
                for (i = 0; i < width * height; ++i)
                    if ((pixels[i] & 0xffffff) != expected_color)
                    {
                        printf("pixel %d: expected RGB %06lx, got %06lx\n", i, expected_color, pixels[i] & 0xffffff);
                        CHECK(FALSE);
                        break;
                    }
            }
            if (dc) DeleteDC(dc);
            free(pixels);
        }
        DeleteObject(info.hbmColor);
    }
    else CHECK(FALSE);
    if (info.hbmMask) DeleteObject(info.hbmMask);
}

int wmain(int argc, WCHAR **argv)
{
    HRESULT (WINAPI *create_window)(ICoreWindow **);
    HRESULT (WINAPI *destroy_window)(ICoreWindow *);
    ICoreWindow *window = NULL;
    ICoreWindowInterop *interop = NULL;
    HMODULE module;
    WCHAR title[1024];
    HWND hwnd = NULL;
    HRESULT hr;
    BOOL icons;
    if (argc != 3 && argc != 4) return 2;
    if (argc == 4) { check_color = TRUE; expected_color = wcstoul(argv[3], NULL, 16); }
    icons = !wcscmp(argv[2], L"icons");
    CHECK(SUCCEEDED(RoInitialize(RO_INIT_SINGLETHREADED)));
    module = LoadLibraryW(L"windows.ui.dll");
    CHECK(!!module);
    if (!module) return 1;
    create_window = (void *)GetProcAddress(module, "__wine_create_core_window");
    destroy_window = (void *)GetProcAddress(module, "__wine_destroy_core_window");
    CHECK(create_window && destroy_window);
    if (!create_window || !destroy_window) return 1;
    hr = create_window(&window);
    CHECK(hr == S_OK && window);
    if (window)
    {
        hr = ICoreWindow_QueryInterface(window, &IID_ICoreWindowInterop, (void **)&interop);
        CHECK(hr == S_OK && interop);
        if (interop)
        {
            CHECK(ICoreWindowInterop_get_WindowHandle(interop, &hwnd) == S_OK && hwnd);
            CHECK(!IsWindowVisible(hwnd));
            GetWindowTextW(hwnd, title, (sizeof(title) / sizeof(title[0])));
            CHECK(!wcscmp(title, argv[1]));
            check_icon(hwnd, ICON_SMALL, icons, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
            check_icon(hwnd, ICON_BIG, icons, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
            ICoreWindowInterop_Release(interop);
        }
        CHECK(destroy_window(window) == S_OK);
        ICoreWindow_Release(window);
    }
    FreeLibrary(module);
    RoUninitialize();
    printf("window metadata: %u failures\n", failures);
    return !!failures;
}
