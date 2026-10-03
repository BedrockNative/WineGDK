#!/usr/bin/env bash
set -euo pipefail

: "${WINEPREFIX:?Set WINEPREFIX to a disposable test prefix}"
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
probe_dir="$repo_dir/build/uwp-diagnostics"
wine_bin="${WINE_BIN:-$repo_dir/build/bin/wine}"
fixture_dir="$(mktemp -d)"
fixture_pid=
cleanup() {
    if [[ -n "$fixture_pid" ]]; then
        kill "$fixture_pid" 2>/dev/null || true
        wait "$fixture_pid" 2>/dev/null || true
    fi
    rm -rf -- "$fixture_dir"
}
trap cleanup EXIT
mkdir -p "$probe_dir"
python3 "$repo_dir/tools/uwp/http-test-server.py" 0 "$fixture_dir/port" > "$fixture_dir/server.log" 2>&1 &
fixture_pid=$!
for ((i=0; i<100; ++i)); do
    [[ -s "$fixture_dir/port" ]] && break
    if ! kill -0 "$fixture_pid" 2>/dev/null; then cat "$fixture_dir/server.log" >&2; exit 1; fi
    sleep 0.05
done
[[ -s "$fixture_dir/port" ]] || { cat "$fixture_dir/server.log" >&2; exit 1; }
port="$(cat "$fixture_dir/port")"

for arch in x86_64 i686; do
    executable="$probe_dir/http-probe-$arch.exe"
    "$arch-w64-mingw32-gcc" -Wall -Wextra -Werror \
        -I"$repo_dir/include" -I"$repo_dir/include/msvcrt" -D__WINE_PE_BUILD -D_UCRT \
        "$repo_dir/tools/uwp/http-probe.c" -o "$executable" -lole32 -luuid
    printf 'HTTP (%s)\n' "$arch"
    WINEDEBUG=-all WINEDLLOVERRIDES=winemenubuilder.exe,winedbg.exe=d \
        timeout -k 5s 40s "$wine_bin" "$executable" "$port"
done
