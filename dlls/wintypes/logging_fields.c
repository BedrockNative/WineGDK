/* WinRT diagnostic field collections.
 *
 * Copyright 2026 WineGDK contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Fields own their names and payloads. LoggingChannel currently has no enabled
 * tracing sessions, so these collections are not emitted to an ETW consumer.
 */

#include "diagnostics_private.h"

enum field_type
{
    FIELD_EMPTY, FIELD_STRUCT, FIELD_UINT8, FIELD_INT16, FIELD_UINT16,
    FIELD_INT32, FIELD_UINT32, FIELD_INT64, FIELD_UINT64, FIELD_SINGLE,
    FIELD_DOUBLE, FIELD_CHAR16, FIELD_BOOLEAN, FIELD_STRING, FIELD_GUID,
    FIELD_DATETIME, FIELD_TIMESPAN, FIELD_POINT, FIELD_SIZE, FIELD_RECT
};

struct field
{
    struct field *next;
    HSTRING name;
    enum field_type type;
    LoggingFieldFormat format;
    INT32 tags;
    UINT32 count, depth;
    BOOL array;
    void *data;
};

struct logging_fields
{
    ILoggingFields iface;
    LONG ref;
    SRWLOCK lock;
    struct field *head, *tail;
    UINT32 depth;
};

static void free_field( struct field *field )
{
    UINT32 i;
    if (field->type == FIELD_STRING)
        for (i = 0; i < field->count; ++i) WindowsDeleteString( ((HSTRING *)field->data)[i] );
    WindowsDeleteString( field->name );
    free( field->data );
    free( field );
}

static void clear_fields( struct logging_fields *impl )
{
    struct field *field, *next;
    for (field = impl->head; field; field = next)
    {
        next = field->next;
        free_field( field );
    }
    impl->head = impl->tail = NULL;
    impl->depth = 0;
}

OBJECT_METHODS(fields, ILoggingFields, struct logging_fields, clear_fields(impl),
               L"Windows.Foundation.Diagnostics.LoggingFields")

static HRESULT WINAPI fields_qi( ILoggingFields *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!BASE_QI(ILoggingFields)) return E_NOINTERFACE;
    *out = iface;
    fields_addref( iface );
    return S_OK;
}

static HRESULT add_field( ILoggingFields *iface, HSTRING name, enum field_type type,
                          BOOL array, UINT32 count, SIZE_T element_size, const void *data,
                          LoggingFieldFormat format, INT32 tags )
{
    struct logging_fields *impl = (struct logging_fields *)iface;
    struct field *field;
    HRESULT hr;
    UINT32 i;

    if (tags & 0xf0000000) return E_INVALIDARG;
    if (count && !data) return E_POINTER;
    if (element_size && count > ~(SIZE_T)0 / element_size) return E_OUTOFMEMORY;
    if (!(field = calloc( 1, sizeof(*field) ))) return E_OUTOFMEMORY;
    field->type = type;
    field->format = format;
    field->tags = tags;
    field->array = array;
    if (FAILED(hr = WindowsDuplicateString( name, &field->name ))) goto failed;
    if (count)
    {
        if (!(field->data = calloc( count, element_size ))) { hr = E_OUTOFMEMORY; goto failed; }
        if (type == FIELD_STRING)
        {
            for (i = 0; i < count; ++i)
            {
                hr = WindowsDuplicateString( ((const HSTRING *)data)[i], &((HSTRING *)field->data)[i] );
                if (FAILED(hr)) goto failed;
                ++field->count;
            }
        }
        else
        {
            memcpy( field->data, data, count * element_size );
            field->count = count;
        }
    }
    AcquireSRWLockExclusive( &impl->lock );
    field->depth = impl->depth;
    if (type == FIELD_STRUCT)
    {
        if (impl->depth == ~0u)
        {
            ReleaseSRWLockExclusive( &impl->lock );
            hr = E_BOUNDS;
            goto failed;
        }
        ++impl->depth;
    }
    if (impl->tail) impl->tail->next = field;
    else impl->head = field;
    impl->tail = field;
    ReleaseSRWLockExclusive( &impl->lock );
    return S_OK;

failed:
    free_field( field );
    return hr;
}

static HRESULT WINAPI fields_Clear( ILoggingFields *iface )
{
    struct logging_fields *impl = (struct logging_fields *)iface;
    AcquireSRWLockExclusive( &impl->lock );
    clear_fields( impl );
    ReleaseSRWLockExclusive( &impl->lock );
    return S_OK;
}

static HRESULT WINAPI fields_BeginStructWithTags( ILoggingFields *iface, HSTRING name, INT32 tags )
{
    return add_field( iface, name, FIELD_STRUCT, FALSE, 0, 0, NULL, LoggingFieldFormat_Default, tags );
}

static HRESULT WINAPI fields_BeginStruct( ILoggingFields *iface, HSTRING name )
{
    return fields_BeginStructWithTags( iface, name, 0 );
}

static HRESULT WINAPI fields_EndStruct( ILoggingFields *iface )
{
    struct logging_fields *impl = (struct logging_fields *)iface;
    HRESULT hr = S_OK;
    AcquireSRWLockExclusive( &impl->lock );
    if (impl->depth) --impl->depth;
    else hr = E_ILLEGAL_METHOD_CALL;
    ReleaseSRWLockExclusive( &impl->lock );
    return hr;
}

static HRESULT WINAPI fields_AddEmptyWithFormatAndTags( ILoggingFields *iface, HSTRING name,
                                                      LoggingFieldFormat format, INT32 tags )
{
    return add_field( iface, name, FIELD_EMPTY, FALSE, 0, 0, NULL, format, tags );
}

static HRESULT WINAPI fields_AddEmptyWithFormat( ILoggingFields *iface, HSTRING name, LoggingFieldFormat format )
{
    return fields_AddEmptyWithFormatAndTags( iface, name, format, 0 );
}

static HRESULT WINAPI fields_AddEmpty( ILoggingFields *iface, HSTRING name )
{
    return fields_AddEmptyWithFormatAndTags( iface, name, LoggingFieldFormat_Default, 0 );
}

#define FIELD_METHODS(suffix, type, kind) \
static HRESULT WINAPI fields_Add##suffix( ILoggingFields *iface, HSTRING name, type value ) \
{ return add_field( iface, name, kind, FALSE, 1, sizeof(value), &value, LoggingFieldFormat_Default, 0 ); } \
static HRESULT WINAPI fields_Add##suffix##WithFormat( ILoggingFields *iface, HSTRING name, type value, LoggingFieldFormat format ) \
{ return add_field( iface, name, kind, FALSE, 1, sizeof(value), &value, format, 0 ); } \
static HRESULT WINAPI fields_Add##suffix##WithFormatAndTags( ILoggingFields *iface, HSTRING name, type value, LoggingFieldFormat format, INT32 tags ) \
{ return add_field( iface, name, kind, FALSE, 1, sizeof(value), &value, format, tags ); } \
static HRESULT WINAPI fields_Add##suffix##Array( ILoggingFields *iface, HSTRING name, UINT32 count, type *values ) \
{ return add_field( iface, name, kind, TRUE, count, sizeof(*values), values, LoggingFieldFormat_Default, 0 ); } \
static HRESULT WINAPI fields_Add##suffix##ArrayWithFormat( ILoggingFields *iface, HSTRING name, UINT32 count, type *values, LoggingFieldFormat format ) \
{ return add_field( iface, name, kind, TRUE, count, sizeof(*values), values, format, 0 ); } \
static HRESULT WINAPI fields_Add##suffix##ArrayWithFormatAndTags( ILoggingFields *iface, HSTRING name, UINT32 count, type *values, LoggingFieldFormat format, INT32 tags ) \
{ return add_field( iface, name, kind, TRUE, count, sizeof(*values), values, format, tags ); }

FIELD_METHODS(UInt8, BYTE, FIELD_UINT8)
FIELD_METHODS(Int16, INT16, FIELD_INT16)
FIELD_METHODS(UInt16, UINT16, FIELD_UINT16)
FIELD_METHODS(Int32, INT32, FIELD_INT32)
FIELD_METHODS(UInt32, UINT32, FIELD_UINT32)
FIELD_METHODS(Int64, INT64, FIELD_INT64)
FIELD_METHODS(UInt64, UINT64, FIELD_UINT64)
FIELD_METHODS(Single, FLOAT, FIELD_SINGLE)
FIELD_METHODS(Double, DOUBLE, FIELD_DOUBLE)
FIELD_METHODS(Char16, UINT16, FIELD_CHAR16)
FIELD_METHODS(Boolean, boolean, FIELD_BOOLEAN)
FIELD_METHODS(String, HSTRING, FIELD_STRING)
FIELD_METHODS(Guid, GUID, FIELD_GUID)
FIELD_METHODS(DateTime, DateTime, FIELD_DATETIME)
FIELD_METHODS(TimeSpan, TimeSpan, FIELD_TIMESPAN)
FIELD_METHODS(Point, Point, FIELD_POINT)
FIELD_METHODS(Size, Size, FIELD_SIZE)
FIELD_METHODS(Rect, Rect, FIELD_RECT)
#undef FIELD_METHODS

#define FIELD_VTBL(name) fields_Add##name, fields_Add##name##WithFormat, fields_Add##name##WithFormatAndTags, \
                         fields_Add##name##Array, fields_Add##name##ArrayWithFormat, fields_Add##name##ArrayWithFormatAndTags
static const ILoggingFieldsVtbl fields_vtbl =
{
    OBJECT_VTBL(fields), fields_Clear, fields_BeginStruct, fields_BeginStructWithTags, fields_EndStruct,
    fields_AddEmpty, fields_AddEmptyWithFormat, fields_AddEmptyWithFormatAndTags,
    FIELD_VTBL(UInt8), FIELD_VTBL(Int16), FIELD_VTBL(UInt16), FIELD_VTBL(Int32), FIELD_VTBL(UInt32),
    FIELD_VTBL(Int64), FIELD_VTBL(UInt64), FIELD_VTBL(Single), FIELD_VTBL(Double), FIELD_VTBL(Char16),
    FIELD_VTBL(Boolean), FIELD_VTBL(String), FIELD_VTBL(Guid), FIELD_VTBL(DateTime), FIELD_VTBL(TimeSpan),
    FIELD_VTBL(Point), FIELD_VTBL(Size), FIELD_VTBL(Rect)
};
#undef FIELD_VTBL

struct fields_factory { IActivationFactory iface; LONG ref; };

static HRESULT WINAPI factory_qi( IActivationFactory *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!BASE_QI(IActivationFactory)) return E_NOINTERFACE;
    *out = iface;
    IActivationFactory_AddRef( iface );
    return S_OK;
}

static ULONG WINAPI factory_addref( IActivationFactory *iface )
{
    return InterlockedIncrement( &((struct fields_factory *)iface)->ref );
}

static ULONG WINAPI factory_release( IActivationFactory *iface )
{
    return InterlockedDecrement( &((struct fields_factory *)iface)->ref );
}

static HRESULT WINAPI factory_iids( IActivationFactory *iface, ULONG *count, IID **out )
{
    if (!count || !out) return E_POINTER;
    *count = 0;
    if (!(*out = CoTaskMemAlloc( sizeof(IID) ))) return E_OUTOFMEMORY;
    **out = IID_IActivationFactory;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI factory_class( IActivationFactory *iface, HSTRING *out )
{
    return string_from_wide( L"Windows.Foundation.Diagnostics.LoggingFields", out );
}

static HRESULT WINAPI factory_trust( IActivationFactory *iface, TrustLevel *out )
{
    if (!out) return E_POINTER;
    *out = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI factory_activate( IActivationFactory *iface, IInspectable **out )
{
    struct logging_fields *impl;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->iface.lpVtbl = &fields_vtbl;
    impl->ref = 1;
    *out = (IInspectable *)&impl->iface;
    return S_OK;
}

static const IActivationFactoryVtbl factory_vtbl = {OBJECT_VTBL(factory), factory_activate};
static struct fields_factory factory = {{&factory_vtbl}, 1};
IActivationFactory *logging_fields_factory = &factory.iface;
