# WineGDK 11.18-6-winrt

Fix a crash when focusing text fields in Minecraft for Windows GDK 26.52.03.

- Allow CoreText text input to attach to a desktop application's focused Win32 window when the thread has no UWP CoreWindow. Previously, `NotifyFocusEnter` returned `0x8000000E`, which Minecraft propagated as an unhandled C++ exception.
- Preserve the existing UWP CoreWindow input path and restrict the desktop fallback to windows owned by the calling thread.
- Add regression coverage for entering and leaving focus, repeated focus notifications, character delivery, and window destruction. The test reproduces the failure with the previous DLL and passes with the fix on both 32-bit and 64-bit Wine. Typing was also confirmed working in Minecraft GDK 26.52.03.
- Preserve local build/tool exclusions when regenerating the top-level `.gitignore`.

Includes the GDK/UWP support, startup improvements, launcher diagnostics, and bundled graphics/runtime companions from [11.18-5-winrt](https://github.com/BedrockNative/WineGDK/releases/tag/11.18-5-winrt). Historical startup benchmarks remain in that release's notes; this release does not claim a new startup performance improvement.

Extract the Linux x86_64 archive and run `bin/wine /path/to/game.exe`, optionally with a dedicated `WINEPREFIX`. Both 64-bit and WoW64 Windows applications are supported. Xbox login requires a running, authenticated Xodus service.

The archive includes third-party license notices. No game files, prefixes, or credentials are included.
