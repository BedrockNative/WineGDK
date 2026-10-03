# WineGDK 11.18-2-gdkcomponents

- Support the Xodus `WINE_DLL_FILE_MAP` main-image protocol. This fixes launching
  an encrypted installation whose executable is supplied through an inherited
  in-memory file descriptor, without writing the decrypted executable to disk.
- Add `WINEBOOT_HIDE_DIALOG=1`: hide the prefix-update wait dialog without
  disabling graphics drivers, skipping initialization or discarding diagnostics.
- Add a Linux smoke test for descriptor-backed PE execution, malformed descriptor
  entries, paths with spaces/non-ASCII characters, and literal game arguments.

This is a continuation of the `11.18-1-gdkcomponents` fork release, not an upstream
Wine version bump. The inherited source `VERSION` file remains unchanged.

The image-mapping protocol is based on `xodus-gaming/wine`. Wine and all bundled
components retain their respective licenses and authorship.
