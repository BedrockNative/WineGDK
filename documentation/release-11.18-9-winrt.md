# WineGDK 11.18-9-winrt

This release enables WineGDK's built-in GDK task queues, adds self-contained
WinHTTP HTTP/2 support, improves CoreInputView compatibility, and reduces work
in file lookups. It also retains the NVIDIA NGX discovery from 11.18-8-winrt.

## GDK task queues and async operations

- Use the built-in XThreading implementation by default on x64 and WoW64.
  `WINEGDK_BUILTIN_XTHREADING=0` selects the native companion for comparison.
- Fix an XAsync initialization overflow: the x64 path cleared 256 bytes through
  a 32-byte internal buffer, overwriting 224 bytes beyond the async block.
- Remove a diagnostic dereference after releasing an async-state reference,
  validate null queue handles, and implement `XThreadVerifyNotTimeSensitive`.

## Networking and WinRT

- Negotiate HTTP/2 through TLS ALPN when requested through WinHTTP's protocol
  option. nghttp2 1.70.0 is linked statically; no additional HTTP/2 DLL or host
  shared library is required.
- Support concurrent streams, uploads, response flow control, cancellation,
  connection reuse, and bounded retries for streams the server did not accept.
  HTTP/1.1 remains available, including for WebSocket upgrades.
- Reuse CoreInputView for the current thread/fiber, manage event registrations
  and handler lifetimes, and return initialized results for supported methods.
- Expose DataTransferManager's support query and report that the share UI is
  unavailable. Improve the corresponding WinRT metadata queries.
- Correct Brazilian Portuguese keyboard layout mapping.

## File access and diagnostics

- Skip repeated traversal of existing ancestors when a current lookup confirms
  the exact parent directory. Final-component case folding, short-name lookup,
  and reparse handling remain active. No negative file cache is introduced.
- Skip unreturned DOS attributes and reparse metadata for name-only directory
  enumeration while retaining existence and symlink checks.
- Remove hardcoded authentication dumps and demote routine missing-file,
  nonblocking socket, and thread-name messages.
- This binary keeps the tested quiet build configuration: Wine TRACE, WARN and
  FIXME messages are compiled out; ERR messages remain. `WINEDEBUG` cannot
  restore compiled-out messages. Graphics-library logging is separate. Normal
  source builds retain their usual logging unless the same flags are supplied.

## Validation and limits

XThreading passed 4,759 checks on each built-in architecture and against the
native x64 companion. File/path checks passed on x64 and WoW64; the focused
directory-enumeration suite passed 7,624 checks. Existing expected failures and
WoW64-specific skips remain; this is not a claim that the entire Wine test suite
passes. HTTP/2 and CoreInputView smoke tests and the NGX discovery regression
harness were also run on the release build.

Minecraft GDK 26.52.03 reached the world-selection screen using the built-in
runtime. No new load-time benchmark or long-duration world-reload stability
claim is made. The `RegionPolicyEvaluator` message was traced to an optional
availability probe handled by XCurl/PlayFab; no fabricated implementation was
added for that class.

Extract `wine-11.18-9-winrt.tar.gz` and run `bin/wine /path/to/game.exe`, normally
with a dedicated `WINEPREFIX`. Existing prefixes can be updated with
`bin/wineboot -u` after closing their applications. Xbox login requires a running,
authenticated Xodus service.

The archive contains both Windows architectures, the existing graphics/runtime
companions, source/build information, and third-party license notices. Historical
startup benchmarks remain in the [11.18-5-winrt release notes](https://github.com/BedrockNative/WineGDK/releases/tag/11.18-5-winrt).
