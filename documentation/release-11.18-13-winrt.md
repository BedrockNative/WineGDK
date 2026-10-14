# WineGDK 11.18-13-winrt

Reduce repeated Xbox authentication work during game startup:

- Reuse recent default-user handles only after Xodus confirms the account-bound session. Cache identity includes the MSA application, title and trust mode; expired sessions and account changes fall back to authentication.
- Reuse the Xbox Live authorization returned by SISU when its XUID and expiry are valid, keeping each token paired with its own user hash.
- Coalesce simultaneous XSTS requests for the same relying party/title context. Different relying parties can exchange tokens concurrently, and refresh invalidates in-flight results.

Shared SRW lock release avoids a wake call when no exclusive thread is waiting. This changes the uncontended lock path, not the allocator.

Opt-in `WINEDEBUG=+cursor` now includes application clipping, fullscreen clipping, X11 focus transitions and pointer-grab outcomes. Mouse capture behavior is unchanged; this release does not claim to fix the intermittent focus issue. Fix duplicate GUID definitions that prevented the complete ThreadPoolTimer and XThreading test builds from linking.

Validation:

- Complete `make -j12` build passed.
- SISU admission, account/session reuse and XSTS concurrency regression tests passed, including refresh races and shared failures; the concurrency harness runs under AddressSanitizer and UndefinedBehaviorSanitizer.
- XThreading: 4,759 checks passed in each of 32-bit and 64-bit runs.
- ThreadPoolTimer: 16 checks passed in each architecture.
- Kernel32 synchronization: 455,933 checks (64-bit) and 384,384 checks (32-bit), with the same two existing event-wait failures at sync.c:393 and sync.c:397 recorded in the pre-change baseline. This suite is not fully green.

DXVK, VKD3D-Proton and DXVK-NVAPI remain bundled. Extract `wine-11.18-13-winrt.tar.gz` and run `bin/wine /path/to/game.exe` with your WINEPREFIX. Xbox login still requires an authenticated Xodus service. No new end-to-end Minecraft login or native Wayland compatibility claim is made by this release.
