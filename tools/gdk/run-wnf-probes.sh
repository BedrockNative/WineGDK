#!/usr/bin/env bash
set -euo pipefail
: "${WINEPREFIX:?Set WINEPREFIX to a disposable test prefix}"
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$repo_dir"
probe_dir="$repo_dir/build/gdk-multiplayer"
mkdir -p "$probe_dir"
for entry in x86_64:x86_64 i686:i386; do
    compiler="${entry%%:*}-w64-mingw32-gcc"
    arch="${entry##*:}"
    for name in wnf-focus rpc-thunks; do
        executable="$probe_dir/$name-$arch.exe"
        "$compiler" -Wall -Wextra -Werror -Wno-missing-field-initializers \
            "tools/gdk/$name-probe.c" -o "$executable" -luser32
        printf '%s %s\n' "$arch" "$name"
        WINEDEBUG=-all timeout -k 5s 45s "${WINE_BIN:-$repo_dir/wine}" "$executable"
    done
done
