# WineGDK 11.18-10-winrt

This release rebuilds WineGDK with the normal Wine diagnostic code enabled.
The `WINE_NO_TRACE_MSGS` and `WINE_NO_DEBUG_MSGS` build flags used for the
11.18-9-winrt binary are absent from this build. TRACE, WARN, FIXME, and ERR
messages are available again, subject to `WINEDEBUG` channel settings. For a
full Wine trace, launch with `WINEDEBUG=+all`; use specific channels to keep
the output manageable. Messages from third-party graphics libraries follow
their own logging settings.

The runtime otherwise includes the 11.18-9-winrt changes: built-in GDK task
queues, self-contained WinHTTP HTTP/2, CoreInputView fixes, file lookup
improvements, and NVIDIA NGX discovery. See the
[11.18-9-winrt notes](https://github.com/BedrockNative/WineGDK/releases/tag/11.18-9-winrt)
for details and known validation limits.

Extract `wine-11.18-10-winrt.tar.gz` and run `bin/wine /path/to/game.exe`,
normally with a dedicated `WINEPREFIX`. Existing prefixes can be updated with
`bin/wineboot -u` after closing their applications. Xbox login requires a
running, authenticated Xodus service.
