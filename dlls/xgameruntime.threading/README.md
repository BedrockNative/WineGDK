# Native XThreading companion

`x86_64-windows/xgameruntime.dll.threading` is the original native Microsoft
runtime supplied for this build, renamed so WineGDK can delegate XThreading
without replacing its own `xgameruntime.dll`. Its checksum is in `MANIFEST.json`.
There is no i386 companion in this package.

`make install` installs it into `lib/wine/native/x86_64-windows`. Installed
Wine copies it into a new prefix's `drive_c/windows/system32`; `wineboot -u`
also installs it into an existing prefix when absent. Existing native copies
are preserved. This binary is not covered by Wine's or WineGDK's source license.
