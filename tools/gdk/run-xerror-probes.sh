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
    executable="$probe_dir/xerror-$arch.exe"
    "$compiler" -Wall -Wextra -Werror -o "$executable" -Iinclude -Iinclude/msvcrt \
        -D__WINESRC__ -D__WINE_PE_BUILD -D_UCRT tools/gdk/xerror-probe.c -luuid
    printf '%s\n' "$arch"
    WINEDEBUG=-all timeout -k 5s 30s "${WINE_BIN:-$repo_dir/build/bin/wine}" "$executable"
done
