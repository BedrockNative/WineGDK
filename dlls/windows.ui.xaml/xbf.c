/* Bounded XBF 2 object-graph reader for embedded package resources.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "private.h"
#include "appmodel.h"
WINE_DEFAULT_DEBUG_CHANNEL(xaml);
struct xbf_reader { const BYTE *pos, *end; HRESULT status; };
static UINT32 read_u32(struct xbf_reader *r)
{
    UINT32 value = 0;
    if (r->end - r->pos < 4) r->status = E_INVALIDARG;
    else { memcpy(&value, r->pos, 4); r->pos += 4; }
    return value;
}
static UINT16 read_u16(struct xbf_reader *r)
{
    UINT16 value = 0;
    if (r->end - r->pos < 2) r->status = E_INVALIDARG;
    else { memcpy(&value, r->pos, 2); r->pos += 2; }
    return value;
}
static BYTE read_u8(struct xbf_reader *r)
{
    if (r->pos == r->end) { r->status = E_INVALIDARG; return 0; }
    return *r->pos++;
}
static HSTRING read_string(struct xbf_reader *r)
{
    UINT32 length = read_u32(r);
    HSTRING value = NULL;
    if (length > (r->end - r->pos) / sizeof(WCHAR)) { r->status = E_INVALIDARG; return NULL; }
    if (SUCCEEDED(r->status)) r->status = WindowsCreateString((const WCHAR *)r->pos, length, &value);
    r->pos += length * sizeof(WCHAR);
    return value;
}
struct xbf_document
{
    HSTRING *strings, root_class;
    UINT32 string_count;
    UINT16 root_type;
    struct xbf_reader nodes;
};
static void document_clear(struct xbf_document *doc)
{
    UINT32 i;
    for (i = 0; doc->strings && i < doc->string_count; ++i) WindowsDeleteString(doc->strings[i]);
    free(doc->strings); WindowsDeleteString(doc->root_class); memset(doc, 0, sizeof(*doc));
}
static HRESULT document_open(const BYTE *data, SIZE_T size, struct xbf_document *doc)
{
    struct xbf_reader r = {data + 4, data + size, S_OK};
    UINT32 metadata, nodes, strings_offset, sections, start, end, i;
    BYTE op;
    if (size < 132 || memcmp(data, "XBF\0", 4)) return E_INVALIDARG;
    metadata = read_u32(&r); nodes = read_u32(&r);
    if (metadata > size - 12 || nodes > size - 12 - metadata || metadata < 120 || nodes < 12) return E_INVALIDARG;
    if (read_u32(&r) != 2 || read_u32(&r) > 1) return E_NOTIMPL;
    strings_offset = read_u32(&r);
    if (read_u32(&r) || strings_offset < 120 || strings_offset >= metadata) return E_INVALIDARG;
    r.pos = data + 12 + strings_offset; r.end = data + 12 + metadata;
    doc->string_count = read_u32(&r);
    if (doc->string_count > 65536) return E_INVALIDARG;
    if (!(doc->strings = calloc(doc->string_count, sizeof(*doc->strings)))) return E_OUTOFMEMORY;
    for (i = 0; i < doc->string_count && SUCCEEDED(r.status); ++i)
    {
        doc->strings[i] = read_string(&r);
        if (read_u16(&r)) r.status = E_INVALIDARG;
    }
    if (FAILED(r.status)) return r.status;
    r.pos = data + 12 + metadata; r.end = r.pos + nodes;
    sections = read_u32(&r);
    if (!sections || sections > (nodes - 4) / 8) return E_INVALIDARG;
    start = read_u32(&r); end = read_u32(&r);
    if (start > end || end > nodes - 4 - 8 * sections) return E_INVALIDARG;
    r.pos = data + 12 + metadata + 4 + 8 * sections + start;
    r.end = data + 12 + metadata + 4 + 8 * sections + end;
    while (r.pos < r.end && SUCCEEDED(r.status))
    {
        op = read_u8(&r);
        if (op == 0x12 || op == 0x03)
        {
            HSTRING prefix;
            read_u16(&r); prefix = read_string(&r); WindowsDeleteString(prefix);
        }
        else if (op == 0x0b)
        {
            if (doc->root_class) return E_INVALIDARG;
            doc->root_class = read_string(&r);
        }
        else if (op == 0x17)
        {
            doc->root_type = read_u16(&r); doc->nodes = r;
            return doc->root_class ? r.status : E_INVALIDARG;
        }
        else return E_NOTIMPL;
    }
    return E_INVALIDARG;
}
struct xbf_value { enum {VALUE_INT, VALUE_FLOAT, VALUE_STRING} kind; INT32 integer; FLOAT number; HSTRING string; };
static HRESULT read_value(struct xbf_document *doc, struct xbf_value *value)
{
    struct xbf_reader *r = &doc->nodes;
    UINT16 index;
    UINT32 bits;
    memset(value, 0, sizeof(*value));
    switch (read_u8(r))
    {
    case 0x03: value->kind = VALUE_FLOAT; bits = read_u32(r); memcpy(&value->number, &bits, sizeof(bits)); break;
    case 0x04: value->integer = read_u32(r); break;
    case 0x05:
        value->kind = VALUE_STRING; index = read_u16(r);
        if (index >= doc->string_count) return E_INVALIDARG;
        value->string = doc->strings[index]; break;
    case 0x0b: read_u16(r); value->integer = read_u32(r); break;
    default: return E_NOTIMPL;
    }
    return r->status;
}
static const WCHAR *type_name(UINT16 type)
{
    switch (type)
    {
    case 0x820d: return L"Windows.UI.Xaml.Controls.Page";
    case 0x8218: return L"Windows.UI.Xaml.Controls.SwapChainPanel";
    case 0x81b0: return L"Windows.UI.Xaml.Controls.Canvas";
    case 0x81e3: return L"Windows.UI.Xaml.Controls.TextBox";
    case 0x821c: return L"Windows.UI.Xaml.Controls.Button";
    default: return NULL;
    }
}
struct connector;
struct connector_vtbl
{
    IInspectableVtbl base;
    HRESULT (WINAPI *Connect)(struct connector *, INT32, IInspectable *);
};
struct connector { const struct connector_vtbl *lpVtbl; };
static const GUID connector_iid = {0xf6790987,0xe6e5,0x47f2,{0x92,0xc6,0xec,0xcc,0xe4,0xba,0x15,0x9a}};
static HRESULT document_load(struct xbf_document *doc, IInspectable *component)
{
    struct connector *connector = NULL;
    IInspectable *stack[64] = {component}, *ended = NULL, *object;
    IActivationFactory *factory;
    struct xbf_value value;
    unsigned int depth = 1, i;
    BOOL collections[64] = {FALSE};
    UINT16 id;
    const WCHAR *name;
    HRESULT hr;
    BYTE op;
    hr = IInspectable_QueryInterface(component, &connector_iid, (void **)&connector);
    if (FAILED(hr)) return hr;
    IInspectable_AddRef(component);
    while (doc->nodes.pos < doc->nodes.end && SUCCEEDED(hr))
    {
        op = read_u8(&doc->nodes);
        if (!depth) { hr = E_INVALIDARG; break; }
        switch (op)
        {
        case 0x14:
            id = read_u16(&doc->nodes); name = type_name(id);
            if (!name || depth == ARRAY_SIZE(stack)) { hr = E_NOTIMPL; break; }
            hr = xaml_control_factory(name, &factory);
            if (SUCCEEDED(hr))
            {
                hr = IActivationFactory_ActivateInstance(factory, &object);
                IActivationFactory_Release(factory);
                if (SUCCEEDED(hr)) { collections[depth] = FALSE; stack[depth++] = object; }
            }
            break;
        case 0x0c:
            hr = read_value(doc, &value);
            if (SUCCEEDED(hr) && value.kind != VALUE_INT) hr = E_INVALIDARG;
            if (SUCCEEDED(hr)) hr = connector->lpVtbl->Connect(connector, value.integer, stack[depth - 1]);
            break;
        case 0x0d:
            hr = read_value(doc, &value);
            if (SUCCEEDED(hr) && value.kind != VALUE_STRING) hr = E_INVALIDARG;
            if (SUCCEEDED(hr)) hr = xaml_control_set_string(stack[depth - 1], L"Name", value.string);
            break;
        case 0x13:
            if (read_u16(&doc->nodes) != 0x8288) hr = E_NOTIMPL;
            else if (collections[depth - 1]) hr = E_INVALIDARG;
            else collections[depth - 1] = TRUE;
            break;
        case 0x02:
            if (!collections[depth - 1] || ended) hr = E_INVALIDARG;
            else collections[depth - 1] = FALSE;
            break;
        case 0x21:
            if (ended || collections[depth - 1]) { hr = E_INVALIDARG; break; }
            ended = stack[--depth];
            break;
        case 0x08:
            if (!ended || !collections[depth - 1]) { hr = E_INVALIDARG; break; }
            hr = xaml_control_add_child(stack[depth - 1], ended);
            IInspectable_Release(ended); ended = NULL;
            break;
        case 0x07:
            id = read_u16(&doc->nodes);
            if (id != 0x83e7 || !ended) { hr = E_NOTIMPL; break; }
            hr = xaml_control_set_content(stack[depth - 1], ended);
            IInspectable_Release(ended); ended = NULL;
            break;
        case 0x1a:
            id = read_u16(&doc->nodes); hr = read_value(doc, &value);
            if (FAILED(hr)) break;
            name = id == 0x818a ? L"HorizontalAlignment" : id == 0x8198 ? L"VerticalAlignment" :
                id == 0x8224 ? L"FontSize" : id == 0x8189 ? L"Height" : NULL;
            if (!name) { hr = E_NOTIMPL; break; }
            if (value.kind == VALUE_FLOAT) hr = xaml_control_set_double(stack[depth - 1], name, value.number);
            else if (value.kind == VALUE_INT) hr = xaml_control_set_int(stack[depth - 1], name, value.integer);
            else hr = E_INVALIDARG;
            break;
        default: FIXME("Unsupported XBF node %#x\n", op); hr = E_NOTIMPL; break;
        }
        if (FAILED(doc->nodes.status)) hr = doc->nodes.status;
    }
    if (SUCCEEDED(hr) && depth) hr = E_INVALIDARG;
    for (i = 0; i < depth; ++i) IInspectable_Release(stack[i]);
    if (ended) IInspectable_Release(ended);
    IInspectable_Release((IInspectable *)connector);
    return hr;
}
HRESULT xaml_load_component(IInspectable *component, IUriRuntimeClass *uri)
{
    WCHAR *path = NULL;
    UINT32 length = 0;
    HANDLE file = INVALID_HANDLE_VALUE, mapping = NULL;
    LARGE_INTEGER size;
    BYTE *data = NULL;
    SIZE_T pos;
    HSTRING class_name = NULL, scheme = NULL;
    struct xbf_document found = {0};
    BOOL matched = FALSE;
    HRESULT hr;
    if (!component || !uri) return E_INVALIDARG;
    hr = IUriRuntimeClass_get_SchemeName(uri, &scheme);
    if (SUCCEEDED(hr) && wcscmp(WindowsGetStringRawBuffer(scheme, NULL), L"ms-appx")) hr = E_INVALIDARG;
    WindowsDeleteString(scheme);
    if (FAILED(hr)) return hr;
    if (FAILED(hr = IInspectable_GetRuntimeClassName(component, &class_name))) return hr;
    TRACE("loading component %s\n", debugstr_hstring(class_name));
    if (GetCurrentPackagePath(&length, NULL) != ERROR_INSUFFICIENT_BUFFER) { hr = E_FAIL; goto done; }
    if (!(path = malloc((length + 16) * sizeof(WCHAR)))) { hr = E_OUTOFMEMORY; goto done; }
    if (GetCurrentPackagePath(&length, path)) { hr = E_FAIL; goto done; }
    wcscat(path, L"\\resources.pri");
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) { hr = HRESULT_FROM_WIN32(GetLastError()); goto done; }
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 132 || size.QuadPart > 64 * 1024 * 1024)
    { hr = E_INVALIDARG; goto done; }
    mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!mapping || !(data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0)))
    { hr = HRESULT_FROM_WIN32(GetLastError()); goto done; }
    for (pos = 0; pos + 132 <= size.QuadPart; ++pos)
    {
        struct xbf_document candidate = {0};
        if (memcmp(data + pos, "XBF\0", 4)) continue;
        hr = document_open(data + pos, size.QuadPart - pos, &candidate);
        if (SUCCEEDED(hr) && !wcscmp(WindowsGetStringRawBuffer(candidate.root_class, NULL), WindowsGetStringRawBuffer(class_name, NULL)))
        {
            if (matched) { document_clear(&candidate); hr = E_INVALIDARG; goto done; }
            found = candidate; matched = TRUE;
        }
        else document_clear(&candidate);
    }
    hr = matched ? document_load(&found, component) : HRESULT_FROM_WIN32(ERROR_RESOURCE_NAME_NOT_FOUND);
done:
    document_clear(&found);
    if (data) UnmapViewOfFile(data);
    if (mapping) CloseHandle(mapping);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    free(path); WindowsDeleteString(class_name);
    TRACE("XBF load completed %#lx\n", hr);
    return hr;
}
