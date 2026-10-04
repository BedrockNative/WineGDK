#!/usr/bin/env bash
# Opt-in, host-local regression test; private prefix, no GPU or credentials needed.
set -euo pipefail
runtime=$(realpath "${1:?usage: ngx-discovery.sh /absolute/installed/wine}")
test_root=$(mktemp -d /tmp/winegdk-ngx-test.XXXXXXXX)
export WINEPREFIX="$test_root/prefix" WINEDEBUG=-all WINEBOOT_HIDE_DIALOG=1
export WINEDLLOVERRIDES='mscoree,mshtml='
key='HKLM\Software\NVIDIA Corporation\Global\NGXCore'
trap '"$runtime/bin/wineserver" -k || true' EXIT
mkdir -p "$test_root/driver with spaces" "$test_root/wrong-arch"
cp "$runtime/lib/wine/native/x86_64-windows/nvapi64.dll" "$test_root/driver with spaces/_nvngx.dll"
cp "$runtime/lib/wine/native/i386-windows/nvapi.dll" "$test_root/wrong-arch/_nvngx.dll"
export NVIDIA_WINE_DLL_DIR="$test_root/missing"
"$runtime/bin/wineboot" -u > "$test_root/boot.log" 2>&1
"$runtime/bin/wineserver" -w
if "$runtime/bin/wine" reg query "$key" /v FullPath > "$test_root/query.log" 2>&1; then
    echo 'FAIL: invalid explicit path fell back to a host driver'; exit 1
fi
"$runtime/bin/wineserver" -w
export NVIDIA_WINE_DLL_DIR="$test_root/driver with spaces"
"$runtime/bin/wineboot" -u >> "$test_root/boot.log" 2>&1
"$runtime/bin/wineserver" -w
"$runtime/bin/wine" reg query "$key" /v FullPath > "$test_root/query.log" 2>&1
grep -F 'driver with spaces' "$test_root/query.log"
"$runtime/bin/wine" reg add "$key" /v FullPath /t REG_SZ /d 'C:\custom-ngx' /f
"$runtime/bin/wineserver" -w
"$runtime/bin/wineboot" -u >> "$test_root/boot.log" 2>&1
"$runtime/bin/wineserver" -w
"$runtime/bin/wine" reg query "$key" /v FullPath > "$test_root/query.log" 2>&1
grep -F 'C:\custom-ngx' "$test_root/query.log"
"$runtime/bin/wine" reg delete "$key" /v FullPath /f
"$runtime/bin/wineserver" -w
export NVIDIA_WINE_DLL_DIR="$test_root/wrong-arch"
"$runtime/bin/wineboot" -u >> "$test_root/boot.log" 2>&1
"$runtime/bin/wineserver" -w
if "$runtime/bin/wine" reg query "$key" /v FullPath > "$test_root/query.log" 2>&1; then
    echo 'FAIL: x86 DLL accepted as x64 NGX'; exit 1
fi
echo "PASS: absent driver, explicit path with spaces, preserved custom registry, rejected wrong architecture. Evidence: $test_root"
