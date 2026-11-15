# WineGDK 11.18-15-winrt

Route DualSense controllers (`054c:0ce6`) through SDL/XInput by default so native
GameInput can receive gamepad readings. On the tested Bluetooth connection,
the previous raw-HID route exposed the device to Wine but left all XInput slots
disconnected and native GameInput without a gamepad reading. The SDL route
allowed both APIs to detect the controller and GameInput to return a reading.

Prefix setup adds `Hidraw=0` under
`HKLM\System\CurrentControlSet\Services\winebus\Devices\054c/0ce6`. Existing
values are preserved on prefix updates. This default targets the standard
DualSense; other controller models retain their existing routing.

Shared GDK initialization globals now declare C linkage, fixing unresolved
symbols when building the C and C++ callers with the current Clang toolchain.

Validation:

- Complete local `make -j12` build and staged installation passed for x86_64 and i386.
- Fresh-prefix setup installs the DualSense default; prefix updates preserve an
  explicit override and restore the default when the value is missing.
- Both 64-bit and WoW64 `cmd /c ver` startup checks passed.
- The built runtime detects the Bluetooth DualSense through XInput and returns
  a successful native GameInput gamepad reading.
- The existing GDK configuration-parser regression test passed.
- Bundled graphics and GameInput binaries match their source copies.

DXVK, VKD3D-Proton and DXVK-NVAPI remain bundled. Extract
`wine-11.18-15-winrt.tar.gz` and run `bin/wine /path/to/game.exe` with your
WINEPREFIX. Existing prefixes can be updated with `bin/wineboot -u` after their
applications are closed. Xbox login still requires an authenticated Xodus
service. Controller API detection was verified; end-to-end Minecraft controls
have not been validated for this release.
