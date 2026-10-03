/* Regression for Minecraft's in-game LoggingFields activation and ABI. */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Diagnostics
#include "initguid.h"
#include "windows.foundation.diagnostics.h"
#include "roapi.h"
#include "winstring.h"
#include <stddef.h>
#include <stdio.h>

static unsigned int failures;
#define CHECK(test) do { if (!(test)) { printf("FAIL %d: %s\n", __LINE__, #test); ++failures; } } while (0)

#define CHECK_TYPE(suffix, type, expression) do { \
    type test_value = expression, test_values[2] = {test_value, test_value}; \
    CHECK(ILoggingFields_Add##suffix(fields, name, test_value) == S_OK); \
    CHECK(ILoggingFields_Add##suffix##WithFormat(fields, name, test_value, LoggingFieldFormat_Default) == S_OK); \
    CHECK(ILoggingFields_Add##suffix##WithFormatAndTags(fields, name, test_value, LoggingFieldFormat_Default, 1) == S_OK); \
    CHECK(ILoggingFields_Add##suffix##Array(fields, name, 2, test_values) == S_OK); \
    CHECK(ILoggingFields_Add##suffix##ArrayWithFormat(fields, name, 2, test_values, LoggingFieldFormat_Default) == S_OK); \
    CHECK(ILoggingFields_Add##suffix##ArrayWithFormatAndTags(fields, name, 2, test_values, LoggingFieldFormat_Default, 1) == S_OK); \
    CHECK(ILoggingFields_Add##suffix##Array(fields, name, 0, NULL) == S_OK); \
    CHECK(ILoggingFields_Add##suffix##Array(fields, name, 1, NULL) == E_POINTER); \
} while (0)

static HSTRING string(const WCHAR *value)
{
    HSTRING result = NULL;
    CHECK(WindowsCreateString(value, wcslen(value), &result) == S_OK);
    return result;
}

int main(void)
{
    IInspectable *instance = NULL;
    ILoggingFields *fields = NULL;
    IActivationFactory *factory = NULL;
    ILoggingChannelFactory *channel_factory = NULL;
    ILoggingChannel *channel = NULL;
    ILoggingTarget *target = NULL;
    IUnknown *identity = NULL;
    HSTRING classid, name, value, structure, runtime = NULL;
    GUID guid = {0};
    DateTime time = {123456789};
    TimeSpan duration = {10000};
    Point point = {1, 2};
    Size size = {3, 4};
    Rect rect = {1, 2, 3, 4};
    boolean enabled;
    HRESULT hr;
    unsigned int i;

    CHECK(SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED)));
    classid = string(L"Windows.Foundation.Diagnostics.LoggingFields");
    hr = RoActivateInstance(classid, &instance);
    printf("LoggingFields activation: %#lx, object present: %d\n", hr, !!instance);
    CHECK(hr == S_OK && instance);
    if (!instance) goto done;
    CHECK(IInspectable_QueryInterface(instance, &IID_ILoggingFields, (void **)&fields) == S_OK && fields);
    IInspectable_Release(instance);
    if (!fields) goto done;
    CHECK(offsetof(ILoggingFieldsVtbl, BeginStruct) == 7 * sizeof(void *));
    CHECK(offsetof(ILoggingFieldsVtbl, AddString) == 79 * sizeof(void *));
    CHECK(ILoggingFields_GetRuntimeClassName(fields, &runtime) == S_OK);
    CHECK(!wcscmp(WindowsGetStringRawBuffer(runtime, NULL), WindowsGetStringRawBuffer(classid, NULL)));
    WindowsDeleteString(runtime);
    CHECK(ILoggingFields_QueryInterface(fields, &IID_IAgileObject, (void **)&identity) == S_OK);
    if (identity) IUnknown_Release(identity);
    CHECK(RoGetActivationFactory(classid, &IID_IActivationFactory, (void **)&factory) == S_OK);
    if (factory) { CHECK(IActivationFactory_ActivateInstance(factory, NULL) == E_POINTER); IActivationFactory_Release(factory); }

    structure = string(L"PartB_Microsoft.XboxLive.InGame");
    name = string(L"name");
    value = string(L"WineGDK regression");
    /* The game crashes at BeginStruct immediately after unchecked activation. */
    CHECK(ILoggingFields_BeginStruct(fields, structure) == S_OK);
    CHECK(ILoggingFields_AddString(fields, name, value) == S_OK);
    CHECK(ILoggingFields_BeginStructWithTags(fields, name, 1) == S_OK);
    CHECK(ILoggingFields_AddEmpty(fields, name) == S_OK);
    CHECK(ILoggingFields_AddEmptyWithFormat(fields, name, LoggingFieldFormat_Default) == S_OK);
    CHECK(ILoggingFields_AddEmptyWithFormatAndTags(fields, name, LoggingFieldFormat_Default, 1) == S_OK);
    CHECK_TYPE(UInt8, BYTE, 42);
    CHECK_TYPE(Int16, INT16, -123);
    CHECK_TYPE(UInt16, UINT16, 123);
    CHECK_TYPE(Int32, INT32, -456);
    CHECK_TYPE(UInt32, UINT32, 456);
    CHECK_TYPE(Int64, INT64, -1234567890123LL);
    CHECK_TYPE(UInt64, UINT64, 1234567890123ULL);
    CHECK_TYPE(Single, FLOAT, 1.25f);
    CHECK_TYPE(Double, DOUBLE, 3.5);
    CHECK_TYPE(Char16, UINT16, 0x20ac);
    CHECK_TYPE(Boolean, boolean, TRUE);
    CHECK_TYPE(String, HSTRING, value);
    CHECK_TYPE(Guid, GUID, guid);
    CHECK_TYPE(DateTime, DateTime, time);
    CHECK_TYPE(TimeSpan, TimeSpan, duration);
    CHECK_TYPE(Point, Point, point);
    CHECK_TYPE(Size, Size, size);
    CHECK_TYPE(Rect, Rect, rect);
    CHECK(ILoggingFields_EndStruct(fields) == S_OK);
    CHECK(ILoggingFields_EndStruct(fields) == S_OK);
    CHECK(FAILED(ILoggingFields_EndStruct(fields)));
    CHECK(ILoggingFields_BeginStructWithTags(fields, name, 0xf0000000) == E_INVALIDARG);
    CHECK(ILoggingFields_BeginStruct(fields, name) == S_OK);
    CHECK(ILoggingFields_Clear(fields) == S_OK);
    CHECK(FAILED(ILoggingFields_EndStruct(fields)));
    for (i = 0; i < 1000; ++i)
    {
        CHECK(ILoggingFields_BeginStruct(fields, structure) == S_OK);
        CHECK(ILoggingFields_AddString(fields, name, value) == S_OK);
        CHECK(ILoggingFields_EndStruct(fields) == S_OK);
        CHECK(ILoggingFields_Clear(fields) == S_OK);
    }
    CHECK(ILoggingFields_AddString(fields, name, value) == S_OK);
    WindowsDeleteString(value);
    value = string(L"Windows.Foundation.Diagnostics.LoggingChannel");
    CHECK(RoGetActivationFactory(value, &IID_ILoggingChannelFactory, (void **)&channel_factory) == S_OK);
    if (channel_factory)
    {
        CHECK(ILoggingChannelFactory_Create(channel_factory, structure, &channel) == S_OK);
        ILoggingChannelFactory_Release(channel_factory);
        if (channel)
        {
            CHECK(ILoggingChannel_QueryInterface(channel, &IID_ILoggingTarget, (void **)&target) == S_OK);
            if (target)
            {
                CHECK(ILoggingTarget_IsEnabled(target, &enabled) == S_OK && !enabled);
                CHECK(ILoggingTarget_LogEventWithFields(target, name, (IInspectable *)fields) == S_OK);
                ILoggingTarget_Release(target);
            }
            ILoggingChannel_Release(channel);
        }
    }
    WindowsDeleteString(value);
    WindowsDeleteString(name);
    WindowsDeleteString(structure);
    ILoggingFields_Release(fields);
done:
    WindowsDeleteString(classid);
    RoUninitialize();
    printf("logging fields: %u failures\n", failures);
    return !!failures;
}
