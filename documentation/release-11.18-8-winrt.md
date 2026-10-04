# WineGDK 11.18-8-winrt

## Automatic NVIDIA NGX discovery

WineGDK now registers the host NVIDIA NGX driver directory during prefix startup
and `wineboot -u`. This supplies the `NGXCore\FullPath` discovery entry used by
games whose DLSS loader does not find the driver merely from DLLs in `system32`.

- Inspect `NVIDIA_WINE_DLL_DIR` (absolute Unix directory), or common NVIDIA Wine
  driver locations used by Linux distributions. Check for a Windows x64 PE DLL
  without executing it during setup.
- Preserve existing `FullPath` values, including explicit empty/custom values.
  An explicit invalid directory never falls back to a different driver version.
- Reference installed host driver files directly. No proprietary NVIDIA driver
  DLLs, game files, credentials or user prefixes are included or copied.
- Keep DXVK-NVAPI 0.9.2 and all runtime fixes/components from 11.18-7-winrt.

For an existing prefix, close its applications and run `bin/wineboot -u`.
For DLSS games, pass `DXVK_ENABLE_NVAPI=1` and `NVIDIA_WINE_DLL_DIR` to the game.
Hybrid NVIDIA systems may need PRIME offload. These are per-process launcher
settings, not changes to global driver configuration. A custom existing FullPath
must be reviewed explicitly if its driver location has changed.

Orion applies the same runtime defaults for either BetterRTX or Vanilla RTX,
while retaining their separate assets, preferences and compatibility checks.
This change does not fix incompatible BetterRTX shaders or grant DLSS support to
AMD/Intel GPUs. DLSS requires a supported NVIDIA GPU/driver and game integration.

For normal play without the validation watermark, use
`DXVK_NVAPI_SET_NGX_DEBUG_OPTIONS=DLSSIndicator=0`. This clears the persistent
indicator when NVAPI initializes; restarting an already running game may be needed.

## Validation

The manual discovery fix was validated in-world with Minecraft 26.21.01,
Vanilla RTX 1.26.14, RTX 4070 Laptop, NVIDIA 615.71.09 and the game's original
DLSS 2.1.16. The DLSS overlay showed actual internal/output resolutions, upscaling
became toggleable, and the user reported stable gameplay. That result is specific
to this combination and does not establish the root cause of earlier crashes.

The local regression harness is `tools/tests/ngx-discovery.sh`; it uses a private
prefix and tests missing drivers, explicit paths with spaces, existing custom
registry preservation and rejection of 32-bit DLLs.

The new build passed that regression harness. A standalone D3D12/NGX probe also
passed using the automatically registered host directory, with no NGX symlinks
in `system32`: device creation and NGX initialization succeeded,
`SuperSampling.Available=1`, `NeedsUpdatedDriver=0`, `FeatureInitResult=1`.
Developer-tool GPU restrictions were removed only from the test process (Intel
ICD/Mesa selection and an empty CUDA-visible-device list). The empty CUDA list
caused a driver-side device-creation fault with both old and new WineGDK; this is
not claimed as a WineGDK regression or a driver bug fixed by this release.

The new runtime was then validated in the same Minecraft world through Orion's
automatic setup, without `prime-run` or manual NGX links. The game loaded both
host NGX DLLs and its original DLSS DLL, reported successful NVAPI cubin shader
execution, and rendered the ray-traced world with the DLSS indicator disabled.
This was a short regression check, not a new long-duration stability benchmark
or validation of every BetterRTX preset.

Extract `wine-11.18-8-winrt.tar.gz` and run `bin/wine /path/to/game.exe`, normally
with a dedicated `WINEPREFIX`. Both x64 and WoW64 Windows applications are supported.
Xbox login still requires a running, authenticated Xodus service. Third-party
licenses remain in the archive. No new startup-performance claim is made.
