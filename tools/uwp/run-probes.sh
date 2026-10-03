#!/usr/bin/env bash
set -euo pipefail

: "${WINEPREFIX:?Set WINEPREFIX to a disposable test prefix}"
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
probe_dir="$repo_dir/build/uwp-diagnostics"
wine_bin="${WINE_BIN:-$repo_dir/build/bin/wine}"
mkdir -p "$probe_dir"

for arch in x86_64 i686; do
    for probe in storage crypto delayload pnp core services package webcore platform logging-fields; do
        libs=()
        cflags=(-Wno-misleading-indentation)
        case "$probe" in
            storage) libs=(-lruntimeobject -lole32 -ladvapi32) ;;
            crypto) libs=(-lruntimeobject -lole32) ;;
            core|services)
                libs=(-lruntimeobject -lole32 -luser32)
                cflags=(-I"$repo_dir/include" -I"$repo_dir/include/msvcrt" -D__WINE_PE_BUILD -D_UCRT)
                ;;
            pnp|package|webcore|platform|logging-fields)
                libs=(-lruntimeobject -lole32 -ladvapi32)
                cflags=(-I"$repo_dir/include" -I"$repo_dir/include/msvcrt" -D__WINE_PE_BUILD -D_UCRT)
                ;;
        esac
        executable="$probe_dir/$probe-probe-$arch.exe"
        "$arch-w64-mingw32-gcc" -Wall -Wextra -Werror -Wno-misleading-indentation \
            "${cflags[@]}" "$repo_dir/tools/uwp/$probe-probe.c" -o "$executable" "${libs[@]}"
        printf '%s (%s)\n' "$probe" "$arch"
        WINEDEBUG=-all WINEDLLOVERRIDES=winemenubuilder.exe,winedbg.exe=d \
            timeout -k 5s 30s "$wine_bin" "$executable"
    done
done
