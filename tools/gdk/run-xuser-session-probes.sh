#!/usr/bin/env bash
set -euo pipefail
: "${WINEPREFIX:?Set WINEPREFIX to a disposable test prefix}"
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$repo_dir"
probe_dir="$repo_dir/build/gdk-startup"
mkdir -p "$probe_dir"
for entry in x86_64:x86_64 i686:i386; do
    compiler="${entry%%:*}-w64-mingw32-gcc"
    arch="${entry##*:}"
    executable="$probe_dir/session-probe-$arch.exe"
    "$compiler" -o "$executable" -Iinclude -Iinclude/msvcrt -Idlls/xgameruntime \
        -D__WINESRC__ -D__WINE_PE_BUILD -D_UCRT \
        tools/gdk/xuser-session-probe.c dlls/xgameruntime/util.c \
        -L"dlls/combase/$arch-windows" -L"dlls/ntdll/$arch-windows" \
        -lcombase -ladvapi32 -lbcrypt -lcrypt32 -lwininet -lwinhttp -luuid -lntdll
    printf '%s\n' "$arch"
    WINEDEBUG=-all timeout -k 5s 30s "${WINE_BIN:-$repo_dir/build/bin/wine}" "$executable"
done
