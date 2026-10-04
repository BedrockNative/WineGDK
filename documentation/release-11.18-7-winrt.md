# WineGDK 11.18-7-winrt

Bundle DXVK-NVAPI 0.9.2 alongside DXVK and VKD3D-Proton, so new prefixes receive the NVAPI DLLs automatically.

- Include `nvapi64.dll` and `nvofapi64.dll` for 64-bit Windows applications and `nvapi.dll` for 32-bit applications. Prefix setup supplies `native,builtin` overrides when no override already exists.
- Install missing companions with `wineboot -u`, preserving existing native DLLs and user overrides.
- Include the optional x86_64 Linux Vulkan Reflex layer, upstream documentation, license and checksum manifest. Preserve native-package data subdirectories during installation so the Vulkan layer manifest stays separate from the package checksum manifest.

Validation: full build; fresh-prefix installation and native DLL loading for both Windows architectures; matching source/installed/prefix checksums; preservation of a customized native DLL and override during a prefix update; restoration of a missing DLL; and discovery of the Reflex layer by the Vulkan loader. NVIDIA-specific rendering features were not validated on NVIDIA hardware.

Extract `wine-11.18-7-winrt.tar.gz` and run `bin/wine /path/to/game.exe`, optionally with a dedicated `WINEPREFIX`. Existing prefixes can be updated with `bin/wineboot -u` after their applications are closed. Both 64-bit and WoW64 Windows applications are supported. Xbox login requires a running, authenticated Xodus service.

For games requiring DXVK's NVAPI compatibility mode, set `DXVK_ENABLE_NVAPI=1`. NVIDIA features still require compatible hardware and drivers; DLSS driver libraries are not included.

Vulkan Reflex is optional. Set `DXVK_NVAPI_VKREFLEX=1` and add the extracted archive's `share/wine/native/dxvk-nvapi/layer` directory, as an absolute path, to `VK_ADD_IMPLICIT_LAYER_PATH`. Preserve existing paths by separating them with `:`. This requires Vulkan loader 1.3.296 or newer. The layer is not enabled globally.

Includes the GDK text-field crash fix from [11.18-6-winrt](https://github.com/BedrockNative/WineGDK/releases/tag/11.18-6-winrt) and the existing GDK/UWP support, graphics/runtime companions and launcher diagnostics. Historical startup benchmarks remain in the [11.18-5-winrt notes](https://github.com/BedrockNative/WineGDK/releases/tag/11.18-5-winrt); this release makes no new startup performance claim.

The archive includes third-party license notices. No game files, prefixes or credentials are included.
