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
