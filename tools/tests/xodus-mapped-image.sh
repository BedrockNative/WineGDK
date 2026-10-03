#!/usr/bin/env bash
set -euo pipefail

wine_bin_dir="$(realpath -- "${1:?Pass the installed Wine bin directory}")"
test_sources="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
test_root="$(mktemp -d -t winegdk-mapping-XXXXXX)"
export WINEPREFIX="$test_root/prefix"
export WINESERVER="$wine_bin_dir/wineserver"
export WINEDEBUG=-all
export WINEBOOT_HIDE_DIALOG=1
unset WINE_DLL_FILE_MAP WINELOADER WINEDLLPATH WINEDLLOVERRIDES
trap '"$WINESERVER" -k; "$WINESERVER" -w' EXIT

cc -Wall -Wextra -Werror "$test_sources/xodus-mapped-image.c" -o "$test_root/helper"
x86_64-w64-mingw32-gcc -Wall -Wextra -Werror "$test_sources/xodus-mapped-image-pe.c" -o "$test_root/test.exe"
WINEDEBUG=+wineboot "$wine_bin_dir/wineboot" -u > "$test_root/prefix.log" 2>&1
grep -q 'Prefix update wait dialog suppressed by WINEBOOT_HIDE_DIALOG' "$test_root/prefix.log"
"$WINESERVER" -w

placeholder="$test_root/mapped game á.exe"
printf 'encrypted-placeholder-not-a-PE' > "$placeholder"
status=0
"$test_root/helper" "$wine_bin_dir/wine" "$test_root/test.exe" "$placeholder" > "$test_root/mapped.log" 2>&1 || status=$?
if [[ "$status" != 73 ]]; then
    printf 'Mapped-image test failed (%s). Logs: %s\n' "$status" "$test_root" >&2
    exit 1
fi
[[ "$(< "$placeholder")" == encrypted-placeholder-not-a-PE ]]
status=0
"$wine_bin_dir/wine" "$placeholder" 'literal value;$HOME' > "$test_root/unmapped.log" 2>&1 || status=$?
[[ "$status" != 73 ]]
printf 'Mapped-image execution passed; unchanged placeholder and literal arguments verified. Logs: %s\n' "$test_root"
