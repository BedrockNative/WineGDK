# Minecraft UWP on WineGDK

This fork supports running extracted Minecraft UWP packages directly. Keep
`AppxManifest.xml`, `xboxservices.config`, the executable and the package assets
together. From an installed WineGDK distribution:

```sh
bin/wine /path/to/Minecraft-release-1.21.0.3-uwp/Minecraft.Windows.exe
```

For an isolated, initially empty prefix:

```sh
WINEPREFIX="$(mktemp -d /tmp/minecraft-wine-XXXXXX)" \
    bin/wine /path/to/Minecraft-release-1.21.0.3-uwp/Minecraft.Windows.exe
```

No `prepare-prefix.sh`, native Microsoft VCLibs installation, package registry
entries or DLL overrides are required for this path. Existing prefixes receive
new class registrations through Wine's normal update; `bin/wineboot -u` can
explicitly refresh an existing prefix. Restart applications after installing
updated DLLs.

## Build and dependencies

Use the repository's `build.sh`, or configure Wine with an installation prefix
and the supported architectures, then build and install:

```sh
./configure --prefix="$PWD/build" --enable-archs=x86_64,i386
make -j"$(nproc)"
make install
build/bin/wine /path/to/Minecraft.Windows.exe
```

The build requires Wine's normal Linux, MinGW and graphics dependencies. The
bundled DXVK/VKD3D components require a compatible Vulkan driver. DXVK's
CoreWindow bridge is included for both architectures; its source patch is
`tools/uwp/dxvk-corewindow.patch`, targeting DXVK v3.1.1. Microsoft runtime
binaries and game assets are not redistributed.

Xbox authentication additionally requires Xodus 0.7.0 or newer, an account
already signed into Xodus, and a running `xodus-service`. The default socket is
`$XDG_RUNTIME_DIR/xodus.sock`; `XODUS_SOCKET` overrides it. The Wine bridge uses
Xbox protocol messages 7/8 and Store messages 9/10. Authentication does not
establish Store ownership or multiplayer compatibility with obsolete clients.

File dialogs use `xdg-desktop-portal` and a desktop portal backend implementing
FileChooser. A session D-Bus connection must be available to Wine.

## Native console windows

New visible consoles (including `AllocConsole`) first try the desktop's default
terminal launchers: `xdg-terminal-exec`, legacy `xdg-terminal`,
`x-terminal-emulator`, `sensible-terminal`, the i3/rofi wrappers and XFCE's
`exo-open`. If none succeeds, common installed emulators are tried with their
respective execution arguments: Konsole, GNOME Terminal/Console, Ptyxis, XFCE,
MATE, QTerminal, Tilix, Kitty, Foot, Alacritty, WezTerm, Ghostty, Terminator,
XTerm and rxvt/urxvt. Installing `xdg-terminal-exec` is recommended when the
choice should follow desktop preferences across multiple installed terminals.

The existing conhost window remains the automatic fallback. A launcher must
establish a real terminal and acknowledge the console handoff within five
seconds; failed or late launches cannot take ownership of another attempt.
`WINECONSOLE=conhost` explicitly selects the existing window. Hidden consoles,
existing Unix terminals and pseudoconsoles retain their current behavior.
Windows console APIs and command interpreters continue to run inside Wine;
only the visible terminal frontend changes. Native terminal size changes are
reported back to the Windows console.

VT-enabled output now consumes split ANSI color/cursor/erase sequences instead
of storing them as printable characters. Extended SGR colors map to the Win32
16-color screen-buffer palette. CR/LF, tab and backspace are handled in both
frontends, including controls arriving inside a split ANSI sequence. This is
not a complete implementation of every VT extension.

Applications must retain `ENABLE_PROCESSED_OUTPUT` when enabling VT, and retain
`ENABLE_WRAP_AT_EOL_OUTPUT` if they want automatic wrapping. Wine's UCRT now
matches the Microsoft runtime when redirecting streams in GUI applications:
`freopen` and descriptor closure preserve the Win32 standard handles. Previously
Wine replaced the readable console handle with a write-only handle, breaking
Amethyst Proxy 1.2.0's console mode query. Console applications still update the
standard handles. Applications should check `GetConsoleMode` for failure and
initialize mode flags; querying a write-only handle is unsupported. Wine
continues to honor explicitly requested literal output modes.

## Automatic package setup

Wine reads the manifest associated with the running executable, including
executables in package subdirectories. Package name, publisher, version,
architecture, application ID, family name and package root provide the Win32
package APIs and per-package application storage. `CoreApplication.Run` selects
the package directory for relative asset loading.

The Xbox title ID comes from `xboxservices.config`. An explicit application
client ID takes precedence; otherwise the bridge discovers the unique public
Xbox client ID embedded in the executable. Ambiguous values are rejected.
Legacy per-executable registry settings remain available as a fallback.

CoreWindow derives its title and PNG icons from the matching manifest
`VisualElements`. Scale, target-size and unplated icon variants are supported,
with corrected RGB/BGR conversion. `ms-resource:` titles still fall back to the
executable name because PRI string resolution is incomplete.

Built-in App VCLibs forward to Wine's C++ runtime implementations. The C++/CX
runtime includes zero-initialized object allocations, command-line arguments,
object context handling and the startup interfaces exercised by these games.
This is not complete support for every C++/CX feature.

`launch-minecraft.sh` and `prepare-prefix.sh` are optional diagnostic helpers.
Their default package is Minecraft release 1.21.0.3. Set `MINECRAFT_UWP_DIR` to
select another package; the helpers' default paths are specific to this
checkout's development environment.

## Implemented paths and observations

Minecraft 1.21.0.3 reached a rendered menu and completed Xbox token exchanges in
an initially clean prefix using the direct executable command. Earlier
interactive testing confirmed gameplay controls, audio, a faster friends menu
following token caching, and Marketplace content downloads. These observations
are not an automated full-game compatibility guarantee. Preview 1.21.120.20
also completed authentication requests in a separate clean prefix.

The implementation includes:

- CoreApplication/CoreWindow lifecycle, native window interop, dispatcher,
  mouse buttons, cursor capture/hiding, relative motion and text input.
- Audio device enumeration, package storage, Pnp properties, power/memory
  queries, resource-context languages and disabled diagnostic logging channels.
- `LoggingFields` activation and owned scalar/array/structure values, fixing a
  reproduced missing-class path encountered during gameplay.
- `FreeThreadedXMLHTTP60`, asynchronous HTTP callbacks, cancellation, redirects,
  gzip/deflate handling and corrected connection reuse after chunked responses.
- WinRT WebSockets, stream adapters and Xbox real-time endpoint authentication.
- Xodus account/token brokerage, signed Store receipt retrieval and legacy
  license-state queries. License errors are propagated; ownership is not inferred
  from successful Xbox login.
- XDG open/save dialogs and StorageFile copying, including collision handling.
  StorageFolder async results now preserve object ownership and callback lifetime.

Minecraft 0.14.0.1 uses an older XAML host. Its compatibility path includes
Application/Window activation, a bounded XBF 2 resource reader, basic controls,
a full-window SwapChainPanel, independent pointer events, TextBox character
input, extended execution and thread-pool timers. Standard shader includes
accept both slash and backslash source paths. Tests reached Xbox login, created
a world, entered text in its name and chat, and saved/reopened it. A second empty prefix also reached the signed-in menu
using only the installed `bin/wine` and the original executable, without
`prepare-prefix`, DLL overrides or manual runtime installation.

This is an experimental XAML subset, not a general XAML renderer. The XBF loader
selects an embedded component by its runtime class; complete PRI URI/qualifier
resolution is not implemented. Composition supports a single full-window surface
with an identity transform. General control rendering, routed input, complex
layouts, transforms and full text editing/IME remain incomplete.

For 0.14, use Xodus 0.7.1 or newer for legacy UWP title registration and Store
runtime leases. An authenticated account and a valid Store license remain required.

## Import and remaining limitations

Minecraft 0.14 export/import was validated with a real world: the exported
`.mcworld` archive passed its CRC check, appeared as a second world after import,
and all 13 imported files matched the archive byte for byte. This path includes
`StorageFolder.CreateFileAsync`, binary `FileIO` reads/writes, writable random
access streams and a persistent, package-scoped `FutureAccessList`.

All implemented UWP and Windows App SDK pickers prefer the XDG desktop portal.
If D-Bus or the FileChooser portal is unavailable, they automatically open Wine's
Explorer-style common dialog. The shared fallback handles opening one or several
files, saving with filters/default extensions, and selecting a folder. Canceling
the portal returns cancellation without opening another dialog. Async cancellation
also closes an open Wine dialog. Portal and fallback probes pass in 32 and 64 bits.
Text/line `FileIO` methods and the access-cache recent-items list are still incomplete.

Opening a `.mcpack` or `.mcworld` as an external file activation is not
implemented. Purchase flows, product licenses, complete Store listing APIs,
interactive account switching, WNS push delivery, package deployment and
AppContainer isolation are also incomplete. Realms can reject older clients
with an update requirement. Full IME composition, multiview and suspend/resume
integration remain incomplete.

## Regression probes

After configuring/building Wine, run these from the source root with a
disposable prefix. Both MinGW GCC compilers and generated headers are required.

```sh
export WINEPREFIX="$PWD/build/uwp-clean-prefix"
tools/uwp/run-probes.sh
tools/uwp/run-http-probes.sh
python3 tools/uwp/run-picker-probes.py
python3 tools/uwp/run-picker-fallback-probes.py
python3 tools/uwp/run-manifest-identity-probes.py
python3 tools/uwp/run-window-metadata-probes.py
python3 tools/uwp/run-console-probes.py
```

These exercise API results and lifetimes, cancellation, text/pointer events,
HTTP fixtures, portal fixtures, copy collisions, malformed manifests, package
identity and icon pixels. Console probes cover 32/64-bit callers, native TTY
input, resizing, split ANSI output, launcher selection and failure/late-launch
fallback using isolated pseudo-terminals. They do not cover world persistence, purchases or
multiplayer sessions. Runtime allocation regression coverage is also included
in `dlls/vccorlib140/tests/vccorlib.c`. Timer completion, cancellation and
delegate lifetime are covered by `dlls/threadpoolwinrt/tests/timer.c`;
`tools/uwp/shader-include-probe.c` checks relative and absolute include paths
with both separators.

Diagnostic logs, game prefixes, account data and locally supplied Microsoft
binaries under `build/` are development artifacts and must not be included in
source commits or release archives.
