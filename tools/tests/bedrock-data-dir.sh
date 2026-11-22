#!/usr/bin/env bash
# Pass a Wine bin directory and a Windows executable built from bedrock-data-dir.c.
set -euo pipefail
bin=$(realpath "$1")
test_exe=$(realpath "$2")
work=$(mktemp -d)
export WINEPREFIX="$work/prefix" WINEDEBUG=-all WINEBOOT_HIDE_DIALOG=1
unset WINEGDK_BEDROCK_DATA_DIR
cleanup() {
    "$bin/wineserver" -w
    rm -rf -- "$work"
}
trap cleanup EXIT
"$bin/wine" cmd /d /c ver
roaming="$WINEPREFIX/drive_c/users/$(id -un)/AppData/Roaming"
mkdir -p "$work/Profile A" "$work/Profile B"
# Both processes see the same apparent paths and create the same filenames.
WINEGDK_BEDROCK_DATA_DIR="$work/Profile A" "$bin/wine" "$test_exe" &
first=$!
WINEGDK_BEDROCK_DATA_DIR="$work/Profile B" "$bin/wine" "$test_exe" &
second=$!
wait "$first"
wait "$second"
for profile in "$work/Profile A" "$work/Profile B"; do
    test -d "$profile/SavedWorld"
    test ! -e "$profile/renamed.dat"
    test ! -e "$profile/Minecraft Bedrock Extra"
done
test -d "$roaming/Minecraft Bedrock Extra"
test ! -e "$roaming/Minecraft Bedrock"
test ! -e "$roaming/Minecraft Bedrock Preview"
# With the feature off, the Windows paths use their normal locations.
printf 'mkdir "%%APPDATA%%\Minecraft Bedrock"\r\nexit\r\n' | "$bin/wine" cmd /d /q
test -d "$roaming/Minecraft Bedrock"
test ! -L "$roaming/Minecraft Bedrock"
echo BEDROCK_DATA_DIR_PASS
