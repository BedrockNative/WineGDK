# NOTES FOR PEOPLE TRYING TO RUN MINECRAFT'S GDK BUILD

Microsoft Services is WIP.

New fork releases use `11.18-<number>-winrt`, reflecting support for both GDK
and UWP/Windows Runtime. The current release is `11.18-5-winrt`; existing
`*-gdkcomponents` tags keep their original names.

Set `XODUS_SOCK_NAME` to override the default `xodus.sock` filename under
`$XDG_RUNTIME_DIR` on Linux (`/tmp` on macOS). For example,
`XODUS_SOCK_NAME=my-xodus.sock ./build/bin/wine game.exe` connects to
`$XDG_RUNTIME_DIR/my-xodus.sock`. An unset or empty value uses `xodus.sock`.
The existing `XODUS_SOCKET` full-path override takes precedence when nonempty.

Xodus's `WINE_DLL_FILE_MAP` in-memory executable protocol is supported. Launchers
may set `WINEBOOT_HIDE_DIALOG=1` to hide only the prefix-update wait dialog while
retaining setup and diagnostic output. `WINEBOOT_LOG` exports silent boot progress,
`WINE_STARTUP_LOG` measures startup stages, and `WINEDBG_LOG` redirects automatic
crash reports without a new console. File and Unix socket destinations are supported;
see [launcher integration](documentation/xodus-launcher.md).

As of [3414250](https://github.com/Weather-OS/WineGDK/commit/341425050f4f9b968b807dbd61942dabca8f6af1), Online functionality has been implemented. To get it working, resort to [GDK-Proton](https://github.com/Weather-OS/GDK-Proton)

### NOTES ABOUT THIS PROJECT

Unfortunately, since I don't have the right conditions to be able to   
push my changes upstream, I've decided to declare every part of my contributions that isn't    
derived from other parts of the wine project, CC0 (A.K.A "Public Domain") (i.e xgameruntime).
**What this means**:  
You're allowed to derive, redistribute and reimplement my code at will,  
without any attributions.
**THIS ONLY APPLIES TO THE CODE I HAVE WRITTEN, NOT THE REST OF WINE'S PROJECT!**

**ADDITIONAL NOTES**: 
- Code authored by "Olivia Ryan" is not covered by this clause.
- [Xodus](<https://github.com/xodus-gaming/xodus>) interopability is not upstream safe. Please refrain from pushing changes that include any part of this feature upstream.
  - This includes all code that run within the `xodus` wine debug channel.

## INTRODUCTION

Wine is a program which allows running Microsoft Windows programs
(including DOS, Windows 3.x, Win32, and Win64 executables) on Unix.
It consists of a program loader which loads and executes a Microsoft
Windows binary, and a library (called Winelib) that implements Windows
API calls using their Unix, X11 or Mac equivalents.  The library may also
be used for porting Windows code into native Unix executables.

Wine is free software, released under the GNU LGPL; see the file
LICENSE for the details.


## QUICK START

To configure, build, and install both x86_64 and i386 into `./build` with
tests disabled, run `./build.sh`. The script uses `make -j"$(nproc)"` and
resolves the installation path from its own location, so it can be rerun
after moving the source directory.

From the top-level directory of the Wine source (which contains this file),
run:

```
./configure
make
```

Then either install Wine:

```
make install
```

The native packages in `dlls/xgameruntime.threading`, `dlls/dxvk`,
`dlls/dxvk-nvapi`, and `dlls/vkd3d-proton` are installed by `make install` into
`lib/wine/native/<architecture>-windows`. New prefixes receive the x64 DLLs
in `system32` and the i386 graphics DLLs in `syswow64`. XThreading is x64 only.
DXVK supplies Direct3D 8–11 and DXGI; VKD3D-Proton supplies Direct3D 12 and
includes the Minecraft patch from the local patched build. Each package has
a checksum manifest; the graphics packages also retain their upstream README
and license files, installed under `share/wine/native`.

DXVK-NVAPI 0.9.2 supplies `nvapi64.dll` and `nvofapi64.dll` for x64 and
`nvapi.dll` for i386. It follows the same prefix installation and override
rules as the other graphics packages. Set `DXVK_ENABLE_NVAPI=1` when launching
games that need DXVK's NVAPI compatibility mode. NVIDIA-specific features
still require a compatible GPU and driver; the package does not include DLSS
driver libraries.

NGX discovery: on prefix startup/update, WineGDK checks `NVIDIA_WINE_DLL_DIR`
(an absolute Unix directory) or common system NVIDIA Wine-driver directories.
If a Windows x64 NGX DLL is present, it registers that directory as
`HKLM\Software\NVIDIA Corporation\Global\NGXCore\FullPath`, allowing the
application's NGX loader to find the installed driver. Existing registry values
are preserved, even if empty or invalid. An explicit invalid directory does not
fall back to another driver. No proprietary libraries are copied into the prefix
or bundled in WineGDK, and no host packages are modified. After moving/removing a
driver installation, review an existing custom FullPath before using a new one.

For DLSS games, pass `DXVK_ENABLE_NVAPI=1` and `NVIDIA_WINE_DLL_DIR` to the game
as well as prefix initialization. Hybrid NVIDIA systems may also need PRIME
offload; this remains a launcher/user policy, not a global Wine default.
`DXVK_NVAPI_SET_NGX_DEBUG_OPTIONS=DLSSIndicator=0` clears a previously enabled
DLSS diagnostic watermark when NVAPI initializes. DLSS still requires a supported
NVIDIA GPU/driver and a game that actually integrates it. This is not a BetterRTX
shader compatibility patch; preset/game compatibility rules still apply.

Local regression test (private prefix, no game/GPU required):
`bash tools/tests/ngx-discovery.sh /absolute/path/to/installed/wine`.

The optional x86_64 Linux Vulkan Reflex layer is installed alongside its
manifest under `share/wine/native/dxvk-nvapi/layer`. To enable it, set
`DXVK_NVAPI_VKREFLEX=1` and add that directory's absolute path to
`VK_ADD_IMPLICIT_LAYER_PATH` (colon-separated, preserving any existing paths).
This discovery mechanism requires Vulkan loader 1.3.296 or newer. For a local
`./build.sh` installation, the directory is
`build/share/wine/native/dxvk-nvapi/layer`. It is not copied into Windows prefixes
or enabled globally for other Vulkan applications.

Prefix setup adds `native,builtin` graphics overrides only when no override
already exists. `wineboot -u` installs missing companions and replaces Wine's
builtin placeholders, preserving existing native DLLs and user overrides.
To build both architectures, use `./configure --enable-archs=x86_64,i386`
with the desired `--prefix` before running `make -j"$(nproc)"`.

Prefix setup enables automatic mouse capture in fullscreen windows
(`Software\Wine\X11 Driver\GrabFullscreen=Y`), preserving an existing setting
on updates. Minecraft's `Bedrock` window also receives a borderless style when
entering fullscreen, removing the leftover `WS_DLGFRAME` edge while retaining
normal decorations when returning to windowed mode.

`dlls/gameinput.redist` supplies the extracted GameInput runtime and
`loader/gameinput.inf.in` reproduces its file, device database, COM and service
registration on x64 prefix creation/update. The MSI is neither bundled nor run.
Wine's builtin GameInput stays the default. Setup leaves the native redist
service at demand start. The shell-focus WNF query/subscription path used by
that service is implemented; other native-service compatibility remains WIP.

Or run Wine directly from the build directory:

```
./wine notepad
```

Run programs as `wine program`. For more information and problem
resolution, read the rest of this file, the Wine man page, and
especially the wealth of information found at https://www.winehq.org.


## REQUIREMENTS

To compile and run Wine, you must have one of the following:

- Linux version 2.6.22 or later
- FreeBSD 12.4 or later
- Solaris x86 9 or later
- NetBSD-current
- macOS 10.15 or later

As Wine requires kernel-level thread support to run, only the operating
systems mentioned above are supported.  Other operating systems which
support kernel threads may be supported in the future.

**FreeBSD info**:
  See https://wiki.freebsd.org/Wine for more information.

**Solaris info**:
  You will most likely need to build Wine with the GNU toolchain
  (gcc, gas, etc.). Warning : installing gas does *not* ensure that it
  will be used by gcc. Recompiling gcc after installing gas or
  symlinking cc, as and ld to the gnu tools is said to be necessary.

**NetBSD info**:
  Make sure you have the USER_LDT, SYSVSHM, SYSVSEM, and SYSVMSG options
  turned on in your kernel.

**macOS info**:
  You need Xcode/Xcode Command Line Tools or Apple cctools.

**Supported file systems**:
  Wine should run on most file systems. A few compatibility problems
  have also been reported using files accessed through Samba. Also,
  NTFS does not provide all the file system features needed by some
  applications.  Using a native Unix file system is recommended.

**Basic requirements**:
  You need to have the X11 development include files installed
  (called xorg-dev in Debian and libX11-devel in Red Hat).
  Of course you also need make (most likely GNU make).
  You also need flex version 2.5.33 or later and bison.

**Optional support libraries**:
  Configure will display notices when optional libraries are not found
  on your system. See https://gitlab.winehq.org/wine/wine/-/wikis/Building-Wine
  for hints about the packages you should install. On 64-bit
  platforms, you have to make sure to install the 32-bit versions of
  these libraries.


## COMPILATION

To build Wine, do:

```
./configure
make
```

This will build the program "wine" and numerous support libraries/binaries.
The program "wine" will load and run Windows executables.
The library "libwine" ("Winelib") can be used to compile and link
Windows source code under Unix.

To see compile configuration options, do `./configure --help`.

For more information, see https://gitlab.winehq.org/wine/wine/-/wikis/Building-Wine


## SETUP

Once Wine has been built correctly, you can do `make install`; this
will install the wine executable and libraries, the Wine man page, and
other needed files.

Don't forget to uninstall any conflicting previous Wine installation
first.  Try either `dpkg -r wine` or `rpm -e wine` or `make uninstall`
before installing.

Once installed, you can run the `winecfg` configuration tool. See the
Support area at https://www.winehq.org/ for configuration hints.


## RUNNING PROGRAMS

When invoking Wine, you may specify the entire path to the executable,
or a filename only.

For example, to run Notepad:

```
wine notepad            (using the search Path as specified in
wine notepad.exe         the registry to locate the file)

wine c:\\windows\\notepad.exe      (using DOS filename syntax)

wine ~/.wine/drive_c/windows/notepad.exe  (using Unix filename syntax)

wine notepad.exe readme.txt          (calling program with parameters)
```

Wine is not perfect, so some programs may crash. If that happens you
will get a crash log that you should attach to your report when filing
a bug.


## GETTING MORE INFORMATION

- **WWW**: A great deal of information about Wine is available from WineHQ at
	https://www.winehq.org/ : various Wine Guides, application database,
	bug tracking. This is probably the best starting point.

- **FAQ**: The Wine FAQ is located at https://gitlab.winehq.org/wine/wine/-/wikis/FAQ

- **Wiki**: The Wine Wiki is located at https://gitlab.winehq.org/wine/wine/-/wikis/

- **Gitlab**: Wine development is hosted at https://gitlab.winehq.org

- **Mailing lists**:
	There are several mailing lists for Wine users and developers; see
	https://gitlab.winehq.org/wine/wine/-/wikis/Forums for more
	information.

- **Bugs**: Report bugs to Wine Bugzilla at https://bugs.winehq.org
	Please search the bugzilla database to check whether your
	problem is already known or fixed before posting a bug report.

- **IRC**: Online help is available at channel `#WineHQ` on irc.libera.chat.
