# WineGDK 11.18-12-winrt

Fix a Minecraft crash after selecting a file in the native file picker. The
chooser returned the correct Windows path, but delivering its completion back
to the game's main thread could fail with a missing COM IPID and a fail-fast
exception (`0xc0000602`).

COM context creation now starts remoting on the owning apartment. This makes
its callback endpoint available when the first ContextCallback arrives from
another thread, without requiring a prior same-thread callback to initialize it.
The ole32 regression test now exercises this cross-apartment-first ordering.

Validation: a minimal STA/MTA reproducer failed before the change and succeeds
with it in both 32-bit and 64-bit runs, verifying callback execution on the
originating thread. The ole32 compobj suite also passed in both architectures:
1,229 checks each, zero failures (27 existing todo checks). The Minecraft GDK test session subsequently completed picker
callbacks successfully. Temporary picker/COM stack traces and HTTP buffer
instrumentation have been removed; standard Wine diagnostics remain available.

The potential intermittent HTTP buffer issue is still under investigation. This
release does not claim to fix it or change HTTP behavior. The 11.18-11 heap and
HTTP allocation improvements are retained, along with DXVK, VKD3D-Proton and
DXVK-NVAPI.

Extract `wine-11.18-12-winrt.tar.gz` and run `bin/wine /path/to/game.exe` with your
WINEPREFIX. Xbox login requires a running, authenticated Xodus service.
