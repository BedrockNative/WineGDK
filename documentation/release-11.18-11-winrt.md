# WineGDK 11.18-11-winrt

Repeated Minecraft world reloads could progressively stall while the Wine heap
scanned thousands of undersized free blocks. The allocator now checks larger
size classes after 32 unsuccessful candidates. If those classes have no suitable
block, it completes the original search before growing the heap or failing an
allocation. Fixed-size heaps retain their fallback behavior.

The xgameruntime HTTP response helper now grows its buffer geometrically instead
of reallocating for every received fragment. A mocked 64 KiB response delivered
in 1 KiB fragments required five reallocations instead of 64. This is an
allocation reduction, not a measured network or login speedup. Authentication,
HTTP connection pooling, HTTP/2, and Xodus protocol behavior are unchanged.

Temporary heap counters, lock timing, and completed-wait stack tracing used in
the investigation are not included. Normal Wine diagnostics remain available
through WINEDEBUG. DXVK, VKD3D-Proton, and DXVK-NVAPI are included as usual.

## Validation

- 32-bit and 64-bit fixed-heap exhaustion tests found a suitable block beyond
  200 undersized entries; 50,000 mixed allocation/reallocation operations per
  architecture preserved contents, zeroing, and heap integrity.
- Eight concurrent threads completed 80,000 allocations with no failures.
- The actual HTTP helper, compiled with mocked WinHTTP calls in both architectures,
  passed fragmented responses, partial reads, empty responses, large fragments,
  and read/allocation/HTTP-status failure cleanup checks.
- Manual Minecraft D3D12 reload captures with the heap patch had maximum frame
  times of 320 ms and 274 ms in two sessions. Temporarily removing the patch
  produced a 2,727 ms maximum, including 28 frames over 500 ms. These were
  different-length manual sessions with evolving caches, not a controlled FPS
  benchmark. The measurements preceded removal of the disabled diagnostic hooks.

Extract `wine-11.18-11-winrt.tar.gz` and run `bin/wine /path/to/game.exe`,
normally with a dedicated WINEPREFIX. Xbox login requires a running,
authenticated Xodus service.
