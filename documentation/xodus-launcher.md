# Xodus launcher integration

`WINE_DLL_FILE_MAP` accepts inherited descriptors containing decrypted PE images,
using Xodus's `fd:NT-path|fd:NT-path` format. The main-image loader uses the mapped
descriptor before the encrypted on-disk placeholder. Descriptors are process-local;
there is no decrypted executable written to disk. This protocol is compatible with
the implementation in `xodus-gaming/wine` (`dlls/ntdll/unix/loader.c`).

Malformed descriptor entries are skipped, including integer overflow. A matched
descriptor that cannot be opened/mapped fails rather than loading a different file.
This mapping is opt-in and leaves ordinary Wine launching unchanged.

Set `WINEBOOT_HIDE_DIALOG=1` to suppress only wineboot's prefix-update wait window.
Prefix initialization and diagnostics are unchanged; launchers should report their
own progress and capture stdout/stderr. Other values retain the normal window.

## Linux smoke test

Run `bash tools/tests/xodus-mapped-image.sh /absolute/installed/bin` to build and
exercise the fixtures with a temporary prefix. Test artifacts are retained for
inspection and only that prefix's Wine server is stopped.

Build `tools/tests/xodus-mapped-image-pe.c` with `x86_64-w64-mingw32-gcc` and
`tools/tests/xodus-mapped-image.c` with the host C compiler. Use a disposable Wine
prefix, run wineboot, and create a non-PE placeholder at an absolute path (also test
a path containing spaces and non-ASCII characters). Run the host helper with:

```
helper /absolute/bin/wine /absolute/test.exe /absolute/placeholder.exe
```

Exit code 73 confirms descriptor-backed execution and literal argument delivery.
The placeholder itself must still be invalid, and launching it without the mapping
must not return 73. Stop the test prefix's wineserver after testing.

## Silent progress and startup timing

Three optional environment variables provide separate output streams:

| Variable | Content | Destinations |
| --- | --- | --- |
| `WINEBOOT_LOG` | JSON lines for prefix checks, updates and boot stages; hides the prefix wait dialog | Absolute Unix or Windows file path, or `unix:/absolute/socket` |
| `WINE_STARTUP_LOG` | JSON lines for process startup milestones | Absolute Unix file path, or `unix:/absolute/socket` |
| `WINEDBG_LOG` | The automatic WineDbg crash report as plain text; no crash dialog or new console | Absolute Unix or Windows file path, or `unix:/absolute/socket` |

Files are appended to. Empty or unset variables retain the normal behavior.
`WINEBOOT_HIDE_DIALOG=1` continues to work independently. `WINE_STARTUP_LOG` alone
observes startup and does not suppress the prefix dialog. No shell command is
executed for any destination. `WINEDBG_LOG` applies to WineDbg's automatic
post-mortem mode; custom AeDebug debuggers and interactive WineDbg are unchanged.
These options do not redirect all `WINEDEBUG` output or installer dialogs.

For example, from the WineGDK build tree:

```sh
WINEPREFIX="$HOME/mcpfx" \
WINEBOOT_LOG=/tmp/minecraft-boot.jsonl \
WINE_STARTUP_LOG=/tmp/minecraft-startup.jsonl \
WINEDBG_LOG=/tmp/minecraft-crashes.log \
./wine /absolute/path/to/Minecraft.Windows.exe
```

Use `build/bin/wine` instead after installing the updated build. Existing game
processes do not pick up new environment variables or newly built modules.

### Reading startup events

Each line is an independent UTF-8 JSON object. All events have `version: 1`,
`source`, `event`, `time_unix_ms`, `prefix` and `code`. Unknown fields and event
names should be ignored for forward compatibility. `time_unix_ms` joins the two
streams chronologically; use monotonic fields for durations, because wall time
can change.

`source: "startup"` adds `pid_unix`, `monotonic_ms`, and `detail`. It reports:

| Event | Meaning |
| --- | --- |
| `process_start` | Wine's Unix loader has started this process; `detail` identifies the target executable |
| `server_connected` | The process has connected to the wineserver |
| `prefix_wait_begin` | The process is about to wait for boot; `detail` is `new-boot` or `existing-boot` |
| `prefix_wait_end` | The wait ended; `detail: "boot-signaled"` and `code: 0` identify the normal completion |
| `prefix_wait_error` | Creating the boot event or starting wineboot failed; `code` is NTSTATUS |
| `executable_loaded` | Initial parameters and the main executable image are loaded, before PE DLL initialization |
| `first_window_shown` | The process showed its first non-tool, top-level window with nonzero dimensions |

Group by `pid_unix`, selecting the target executable from its `process_start`
event. Services and child processes emit their own events and must not be counted
as the game's window. A process started by another Windows process may not have a
prefix-wait pair. The wait also occurs for an already active prefix and can take
zero milliseconds. A new boot does **not** necessarily mean a new prefix: services
can start without an update.

Measure `process_start` to `server_connected`, `prefix_wait_begin` to
`prefix_wait_end`, and `executable_loaded` to `first_window_shown` separately.
The last interval includes DLL initialization, graphics setup and application
code. It is not a pure game-code benchmark. A shown window may be a splash screen;
it does not prove the first frame was presented, login completed or the menu is
ready. Menu readiness needs a game-specific signal. Record the launcher spawn
time separately to include time before Wine's first milestone.

`source: "wineboot"` adds `pid_windows`, `tick_ms` and `stage`. Events are `begin`,
`end`, `skip`, or `error`. Stages include `boot`, `registry`, `services-start`,
`prefix-check`, `prefix-update`, `preinstall`, `install-native`, `install-wow64`,
`gameinput`, `devices`, and `user-profile`. `skip` on `prefix-update` means no
update was performed. No percentage is estimated. `end` means execution reached
the end of a stage, not that every nested operation succeeded; keep capturing
stderr for detailed diagnostics. Installation-process failures report exit codes,
launch failures report Win32 errors, and file/timestamp failures report CRT errno.
An `end` on `boot` precedes signaling the boot event. Only the loader's
`prefix_wait_end` confirms that the waiting process was released.

### Unix sockets

The launcher must create an **AF_UNIX / SOCK_STREAM listener** before launching
Wine. Use a private directory such as an application subdirectory of
`$XDG_RUNTIME_DIR`, and restrict access to the socket. For example:

```sh
WINEBOOT_LOG="unix:$XDG_RUNTIME_DIR/my-launcher/events.sock" \
WINE_STARTUP_LOG="unix:$XDG_RUNTIME_DIR/my-launcher/events.sock" \
./wine /absolute/path/to/game.exe
```

Accept concurrent connections. Wineboot keeps one connection for its boot
operation; startup telemetry opens a connection per event. Read until EOF and
split on newlines, retaining partial lines between reads. Reads are not message
boundaries. Multiple connections can arrive out of timestamp order. Keep crash
reports on a separate socket because `WINEDBG_LOG` sends plain text, not JSON.
WineDbg uses one connection per automatic report and closes it on exit.

Startup socket output is nonblocking: if the receiver cannot accept or consume
an event immediately, the event falls back to stderr. Wineboot/WineDbg socket
connection and stalled-write waits are limited to two seconds before falling
back to stderr. A missing or disconnected listener does not prevent startup or
re-enable a suppressed dialog. Receivers should discard an incomplete JSON line
at EOF. File and socket write failures leave diagnostics on stderr.

### Verification

`python3 tools/gdk/run-startup-events-probes.py` creates a disposable prefix and
checks cold boot, warm launch, forced update, UTF-8 JSON, append behavior, socket
streams and missing listeners using a small GUI fixture. No Minecraft instance
is launched or stopped. Artifacts are under
`build/gdk-multiplayer/startup-events-tests/`.

With an initialized disposable `WINEPREFIX`, run
`python3 tools/gdk/run-winedbg-log-probes.py` to exercise automatic crash reports
in both 32 and 64 bits, file append, Unix sockets and silent stderr fallback.

## Reusing GDK authentication across launches

An updated Xodus service advertises `GdkSessionCacheVersion=1` in its MSA reply.
Wine can then reuse the authenticated Xbox bootstrap state across game processes:
user/XSTS tokens, their matching proof key, profile and endpoint metadata. Each
launch still obtains an MSA response for the active account. Entries are scoped
by account, OAuth client, title and full-trust setting, and never outlive either
token. Metadata is refreshed at least hourly, with a 60-second expiry margin.
The state stays in Xodus's ephemeral token backend and is cleared on logout,
account changes or service restart. `ForceRefresh` invalidates the shared snapshot
and the process's token caches before requesting replacement tokens.

This is automatic and requires no environment variable. Old services do not
receive the new IPC message; a miss, invalid payload or unavailable cache uses
the existing authentication flow. Cache requests use a two-second response timeout.
The host network stack is reported initialized immediately, independently of Xbox
authentication; there is no artificial startup delay. Connectivity notifications
retain all registered observers and deliver an initial callback on each supplied
completion queue. This prevents fast startup from leaving subscribers offline.
It improves repeat launches; the first launch after a service restart still
performs the Xbox exchanges. UWP authentication uses its existing broker path.

The version-1 XML request/response use message IDs 11/12. `GdkSessionRequest`
contains `Operation` (`get`, `put`, `invalidate`), `Puid`, `ClientId`, `TitleId`,
`FullTrust`, `Expiry` (Unix seconds) and `Data` (base64, at most 60,000 bytes).
`GdkSessionResponse` contains `Status` (`hit`, `miss`, `stored`, `invalidated`),
`Expiry` and `Data`. The binary payload is internal, versioned, bounded and
validated before importing the proof key. Do not log or persist these payloads.
