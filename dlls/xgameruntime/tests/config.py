#!/usr/bin/env python3
"""Exercise the actual inline configuration parser with native ASan and UBSan.

Requires a C++ compiler, pkg-config, and libxml2 development files. Windows path
and lock APIs are mocked; Xodus is disabled. No Wine prefix or account is needed.
Run: python3 dlls/xgameruntime/tests/config.py
If tracing blocks LeakSanitizer, set ASAN_OPTIONS=detect_leaks=0.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

SOURCE = (Path(__file__).resolve().parents[1] / "GDKComponent/InitInternalGDKC.cpp").read_text()


def function(prefix):
    start = SOURCE.index(prefix)
    brace = SOURCE.index("{", start)
    depth = 0
    for token in re.finditer(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',
                             SOURCE[brace:], re.S):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if not depth:
                return SOURCE[start:brace + token.end()]
    raise ValueError(prefix)


PRELUDE = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <libxml/parser.h>
#include <libxml/tree.h>
using HRESULT = int32_t;
using CHAR = char;
using UINT32 = uint32_t;
using DWORD = uint32_t;
using BOOLEAN = uint8_t;
using SIZE_T = size_t;
#define WINAPI
#define XODUS_INTEROP 0
#define MAX_PATH 260
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define S_OK 0
#define E_INVALIDARG ((HRESULT)0x80070057)
#define E_OUTOFMEMORY ((HRESULT)0x8007000e)
#define E_GAME_MISSING_GAME_CONFIG ((HRESULT)0x87e5001f)
#define E_GAMERUNTIME_GAMECONFIG_BAD_FORMAT ((HRESULT)0x8924010b)
#define ERROR_INSUFFICIENT_BUFFER 122
#define HRESULT_FROM_WIN32(e) ((HRESULT)((e) ? (0x80070000u | (e)) : 0))
#define FALSE 0
#define TRUE 1
#define FAILED(hr) ((hr)<0)
#define SUCCEEDED(hr) ((hr)>=0)
struct INITIALIZE_OPTIONS { UINT32 unknown; BOOLEAN isInlineConfig; const char *gameConfig; };
char *msaAppId;
UINT32 titleId;
BOOLEAN fullTrust, initializeCalled;
int initialize_lock;
void AcquireSRWLockExclusive(int *lock) { assert(!(*lock)++); }
void ReleaseSRWLockExclusive(int *lock) { assert((*lock)-- == 1); }
DWORD GetModuleFileNameA(void *, char *, DWORD) { assert(!"unexpected non-inline path"); return 0; }
DWORD GetLastError() { return 1; }
bool PathFileExistsA(const char *) { assert(!"unexpected non-inline path"); return false; }
'''

TESTS = r'''
int main()
{
    const char *bad[] = {"", "<Game", "<Other/>", "<Game><TitleId>invalid</TitleId></Game>",
        "<Game><TitleId>100000000</TitleId></Game>", "<Game><TitleId>-1</TitleId></Game>",
        "<Game><TitleId>+1234567</TitleId></Game>", "<Game><TitleId> 1234567</TitleId></Game>"};
    INITIALIZE_OPTIONS options = {0, TRUE, nullptr};
    msaAppId = strdup("unchanged"); titleId = 123; fullTrust = TRUE;
    assert(InitializeGDKComponent(&options) == E_INVALIDARG);
    for (auto text : bad)
    {
        options.gameConfig = text;
        assert(InitializeGDKComponent(&options) == E_GAMERUNTIME_GAMECONFIG_BAD_FORMAT);
        assert(!initializeCalled && !initialize_lock);
        assert(titleId == 123 && fullTrust && !strcmp(msaAppId, "unchanged"));
    }
    options.gameConfig = "<Game><TitleId>FFFFFFFF</TitleId><MSAAppId>test</MSAAppId><MSAFullTrust>true</MSAFullTrust></Game>";
    assert(InitializeGDKComponent(&options) == S_OK);
    assert(initializeCalled && !initialize_lock);
    assert(titleId == 0xffffffffu && fullTrust && !strcmp(msaAppId, "test"));
    options.gameConfig = "<Wrong/>";
    assert(InitializeGDKComponent(&options) == S_OK);
    assert(titleId == 0xffffffffu && !strcmp(msaAppId, "test"));
    initializeCalled = FALSE;
    options.gameConfig = "<Game><TitleId>abcdef01</TitleId></Game>";
    assert(InitializeGDKComponent(&options) == S_OK);
    assert(titleId == 0xabcdef01u && !fullTrust && !msaAppId && !initialize_lock);
    xmlCleanupParser();
    puts("PASS: invalid configs, failed-state preservation, inline retry, hex title IDs, optional fields, idempotence");
}
'''

parts = [PRELUDE, function("static HRESULT WINAPI ObtainMsaAppId("),
         function("HRESULT WINAPI InitializeGDKComponent("), TESTS]
with tempfile.TemporaryDirectory(prefix="winegdk-config-") as tmp:
    src, exe = Path(tmp) / "config.cpp", Path(tmp) / "config"
    src.write_text("\n".join(parts))
    xml_flags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "libxml-2.0"], text=True))
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    subprocess.run(compiler + ["-std=c++17", "-g", "-Wall", "-Wextra", "-Werror",
                              "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                              str(src), "-o", str(exe)] + xml_flags, check=True)
    subprocess.run([str(exe)], check=True)
