/* CoreWindow presentation metadata from the application's package manifest.
 *
 * Copyright 2026 WineGDK contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"
#include "winuser.h"
#include "appmodel.h"
#include "shlwapi.h"
#include "initguid.h"
#include "xmllite.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ui);

#define PATH_CAPACITY 32768

struct manifest_metadata
{
    WCHAR title[1024], logo[1024], tile[1024];
};

static BOOL read_attribute( IXmlReader *reader, const WCHAR *name, WCHAR *out, UINT capacity )
{
    const WCHAR *value;
    UINT length;
    BOOL ret = FALSE;
    out[0] = 0;
    if (IXmlReader_MoveToAttributeByName( reader, name, NULL ) != S_OK) return FALSE;
    if (SUCCEEDED(IXmlReader_GetValue( reader, &value, &length )) && length < capacity)
    {
        memcpy( out, value, length * sizeof(WCHAR) );
        out[length] = 0;
        ret = TRUE;
    }
    IXmlReader_MoveToElement( reader );
    return ret;
}

/* Treat manifest paths as package-relative, never as external filenames. */
static BOOL package_path( const WCHAR *root, const WCHAR *relative, WCHAR *out )
{
    WCHAR *combined;
    DWORD length;
    UINT root_length = wcslen(root), i;
    if (!*relative || *relative == '\\' || *relative == '/' || wcschr(relative, ':')) return FALSE;
    if (root_length + wcslen(relative) + 2 > PATH_CAPACITY) return FALSE;
    if (!(combined = malloc( PATH_CAPACITY * sizeof(WCHAR) ))) return FALSE;
    swprintf( combined, PATH_CAPACITY, L"%s\\%s", root, relative );
    for (i = 0; combined[i]; ++i) if (combined[i] == '/') combined[i] = '\\';
    length = GetFullPathNameW( combined, PATH_CAPACITY, out, NULL );
    free( combined );
    return length && length < PATH_CAPACITY && !wcsnicmp(root, out, root_length) && out[root_length] == '\\';
}

static BOOL read_manifest( const WCHAR *root, const WCHAR *exe, struct manifest_metadata *metadata )
{
    struct manifest_metadata parsed = {0};
    WCHAR attribute[1024], *path;
    IXmlReader *reader = NULL;
    IStream *stream = NULL;
    const WCHAR *name, *ns;
    XmlNodeType type;
    UINT depth;
    BOOL application = FALSE, found = FALSE;
    HRESULT hr;

    if (!(path = malloc( PATH_CAPACITY * sizeof(WCHAR) ))) return FALSE;
    if (!package_path( root, L"AppxManifest.xml", path )) goto done;
    if (FAILED(hr = SHCreateStreamOnFileEx( path, STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, NULL, &stream ))) goto done;
    if (FAILED(hr = CreateXmlReader( &IID_IXmlReader, (void **)&reader, NULL ))) goto done;
    if (FAILED(hr = IXmlReader_SetProperty( reader, XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit ))) goto done;
    if (FAILED(hr = IXmlReader_SetInput( reader, (IUnknown *)stream ))) goto done;
    while ((hr = IXmlReader_Read( reader, &type )) == S_OK)
    {
        IXmlReader_GetDepth( reader, &depth );
        if (type == XmlNodeType_EndElement && depth == 2) application = FALSE;
        if (type != XmlNodeType_Element) continue;
        IXmlReader_GetLocalName( reader, &name, NULL );
        IXmlReader_GetNamespaceUri( reader, &ns, NULL );
        if (depth == 2 && !wcscmp(name, L"Application") &&
            (!wcscmp(ns, L"http://schemas.microsoft.com/appx/manifest/foundation/windows10") ||
             !wcscmp(ns, L"http://schemas.microsoft.com/appx/2010/manifest")))
        {
            application = read_attribute( reader, L"Executable", attribute, ARRAY_SIZE(attribute) ) &&
                          package_path( root, attribute, path ) && !wcsicmp(path, exe);
        }
        else if (application && depth == 3 && !wcscmp(name, L"VisualElements") &&
                 (!wcscmp(ns, L"http://schemas.microsoft.com/appx/manifest/uap/windows10") ||
                  !wcscmp(ns, L"http://schemas.microsoft.com/appx/2010/manifest")))
        {
            read_attribute( reader, L"DisplayName", parsed.title, ARRAY_SIZE(parsed.title) );
            read_attribute( reader, L"Square44x44Logo", parsed.logo, ARRAY_SIZE(parsed.logo) );
            read_attribute( reader, L"Square150x150Logo", parsed.tile, ARRAY_SIZE(parsed.tile) );
            found = TRUE;
        }
    }
    /* Do not use partial metadata from an invalid XML document. */
    if (hr == S_FALSE && found) *metadata = parsed;
    else found = FALSE;

done:
    if (reader) IXmlReader_Release( reader );
    if (stream) IStream_Release( stream );
    free( path );
    return found;
}

static HICON load_png_icon( const WCHAR *path, UINT width, UINT height )
{
    static const BYTE signature[] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    BYTE *data;
    LARGE_INTEGER size;
    DWORD read, png_width, png_height;
    HICON icon = NULL;
    HANDLE file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    if (file == INVALID_HANDLE_VALUE) return NULL;
    if (GetFileSizeEx( file, &size ) && size.QuadPart >= 33 && size.QuadPart <= 8 * 1024 * 1024 &&
        (data = malloc( size.LowPart )))
    {
        if (ReadFile( file, data, size.LowPart, &read, NULL ) && read == size.LowPart &&
            !memcmp(data, signature, sizeof(signature)))
        {
            png_width = (DWORD)data[16] << 24 | (DWORD)data[17] << 16 | data[18] << 8 | data[19];
            png_height = (DWORD)data[20] << 24 | (DWORD)data[21] << 16 | data[22] << 8 | data[23];
            if (png_width && png_height && png_width <= 4096 && png_height <= 4096)
                icon = CreateIconFromResourceEx( data, read, TRUE, 0x30000, width, height, 0 );
        }
        free( data );
    }
    CloseHandle( file );
    return icon;
}

static UINT icon_score( UINT pixels, UINT wanted )
{
    /* Prefer downscaling the nearest larger asset to upscaling a smaller one. */
    return pixels >= wanted ? pixels - wanted : 100000 + wanted - pixels;
}

static HICON load_package_icon( const WCHAR *root, const WCHAR *relative, UINT nominal, UINT width, UINT height )
{
    WCHAR *path, *pattern, *filename, *extension, *qualifier, *end;
    WIN32_FIND_DATAW data;
    UINT score = ~0u, candidate, pixels, stem_length;
    HICON icon = NULL, next;
    HANDLE search;

    if (!(path = malloc( PATH_CAPACITY * 2 * sizeof(WCHAR) ))) return NULL;
    pattern = path + PATH_CAPACITY;
    if (!package_path( root, relative, path )) goto done;
    if ((icon = load_png_icon( path, width, height ))) score = icon_score( nominal, width );
    wcscpy( pattern, path );
    filename = wcsrchr( pattern, '\\' ) + 1;
    if (!(extension = wcsrchr( filename, '.' )) || wcsicmp(extension, L".png")) goto done;
    stem_length = extension - filename;
    if (extension - pattern + 7 >= PATH_CAPACITY) goto done;
    wcscpy( extension, L".*.png" );
    if ((search = FindFirstFileW( pattern, &data )) == INVALID_HANDLE_VALUE) goto done;
    do
    {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY || wcsnicmp(data.cFileName, filename, stem_length)) continue;
        qualifier = data.cFileName + stem_length;
        if (!wcsncmp(qualifier, L".targetsize-", 12))
        {
            pixels = wcstoul( qualifier + 12, &end, 10 );
            if (wcscmp(end, L".png") && wcscmp(end, L"_altform-unplated.png")) continue;
        }
        else if (!wcsncmp(qualifier, L".scale-", 7))
        {
            pixels = wcstoul( qualifier + 7, &end, 10 );
            if (wcscmp(end, L".png") || !pixels || pixels > 1000) continue;
            pixels = nominal * pixels / 100;
        }
        else continue;
        if (!pixels || pixels > 4096 || (candidate = icon_score(pixels, width)) >= score) continue;
        if (filename - pattern + wcslen(data.cFileName) >= PATH_CAPACITY) continue;
        memcpy( path, pattern, (filename - pattern) * sizeof(WCHAR) );
        wcscpy( path + (filename - pattern), data.cFileName );
        if ((next = load_png_icon( path, width, height )))
        {
            if (icon) DestroyIcon( icon );
            icon = next;
            score = candidate;
        }
    } while (FindNextFileW( search, &data ));
    FindClose( search );
done:
    free( path );
    return icon;
}

void corewindow_load_metadata( WCHAR *title, UINT capacity, HICON *small_icon, HICON *large_icon )
{
    struct manifest_metadata metadata;
    WCHAR *exe, *root, *name, *extension;
    UINT length = PATH_CAPACITY;
    BOOL found = FALSE;

    *small_icon = *large_icon = NULL;
    lstrcpynW( title, L"Application", capacity );
    if (!(exe = malloc( PATH_CAPACITY * 2 * sizeof(WCHAR) ))) return;
    root = exe + PATH_CAPACITY;
    if (!(length = GetModuleFileNameW( NULL, exe, PATH_CAPACITY )) || length >= PATH_CAPACITY) goto done;
    name = wcsrchr( exe, '\\' );
    lstrcpynW( title, name ? name + 1 : exe, capacity );
    if ((extension = wcsrchr( title, '.' ))) *extension = 0;
    length = PATH_CAPACITY;
    if (!GetCurrentPackagePath( &length, root )) found = read_manifest( root, exe, &metadata );
    if (!found)
    {
        /* Also handle unpacked packages launched directly, including executables in subdirectories. */
        wcscpy( root, exe );
        while ((name = wcsrchr( root, '\\' )) && name > root + 2)
        {
            *name = 0;
            if ((found = read_manifest( root, exe, &metadata ))) break;
        }
    }
    if (found)
    {
        /* PRI resource references need a resource resolver; never display the URI as a title. */
        if (*metadata.title && wcsnicmp(metadata.title, L"ms-resource:", 12)) lstrcpynW( title, metadata.title, capacity );
        *small_icon = load_package_icon( root, metadata.logo, 44, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON) );
        *large_icon = load_package_icon( root, metadata.logo, 44, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON) );
        if (!*small_icon) *small_icon = load_package_icon( root, metadata.tile, 150, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON) );
        if (!*large_icon) *large_icon = load_package_icon( root, metadata.tile, 150, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON) );
        TRACE( "package title %s, icons %p/%p\n", debugstr_w(title), *small_icon, *large_icon );
    }
done:
    free( exe );
}
