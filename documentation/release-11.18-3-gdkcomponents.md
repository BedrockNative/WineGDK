# WineGDK 11.18-3-gdkcomponents

Extracted Minecraft UWP packages can launch directly with `bin/wine /path/to/game.exe`.
Keep the executable, `AppxManifest.xml`, `xboxservices.config` and package assets together.
A new prefix no longer needs a preparation script, imported package registrations,
Microsoft runtime DLLs or manual overrides for the tested launch paths.

- Add automatic loose-package identity, packaged runtime aliases, CoreWindow/XAML
  hosting and the bundled DXVK CoreWindow/composition bridge. Tests reached gameplay
  in Minecraft 1.21.0.3 and the legacy 0.14.0.1 XAML client, and launched Preview
  1.21.120.20 during development.
- Connect UWP Xbox/Store APIs to Xodus, add FreeThreadedHTTP and WebSocket support,
  and correct input, text, pointer capture and local audio-stream paths. Use Xodus
  0.7.1 or newer for the legacy client's sign-in and runtime-license handling.
- Support world import/export through StorageFile copying, writable streams,
  binary FileIO and persistent package-scoped future-access lists. A real 0.14
  world was exported and imported; all 13 imported files matched the archive.
- Prefer XDG file chooser portals, with automatic Wine Explorer-style dialogs
  when D-Bus or FileChooser support is unavailable. The shared fallback supports
  single/multiple file selection, save dialogs and folder selection. Canceling a
  portal does not open another dialog; pending Wine dialogs can also be canceled.
- Derive window titles and icons from package metadata, preserve icon channels,
  and add initial centered sizing, saved placement and fullscreen switching.
- Launch AllocConsole in common desktop terminals, with conhost fallback. Preserve
  line breaks, ANSI colors, cursor operations, input and resize behavior.
- Correct the legacy achievements-call ABI and return an unsupported result for
  the unimplemented Xbox achievements UI instead of crashing.

Validation includes a dual-architecture `make -j$(nproc)` and install, a fresh-prefix
UWP regression run, 32/64-bit portal and Explorer picker tests, HTTP fixtures,
manifest identity and icon checks, and the console regression suite. The archive
contains Wine and bundled open-source components; game packages, prefixes,
credentials and diagnostics are excluded.

This remains an experimental compatibility layer. General XAML rendering, file
activation from `.mcworld`/`.mcpack`, purchase checkout, some Store APIs, IME,
multiview, suspend/resume and AppContainer isolation are incomplete. An authenticated
account and valid entitlement are required. See `tools/uwp/README.md` in the source
for build instructions and detailed limitations.

The release continues the fork's `11.18-*-gdkcomponents` naming scheme; the inherited
Wine `VERSION` file is unchanged. Wine, DXVK and VKD3D retain their respective
licenses and authorship. The DXVK source patch is included in the source repository.
