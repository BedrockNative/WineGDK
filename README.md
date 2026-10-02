# NOTES FOR PEOPLE TRYING TO RUN MINECRAFT'S GDK BUILD

Microsoft Services have not been added yet. This is because `XUser` hasn't been implemented in `xgameruntime.dll` yet.

As of [3414250](https://github.com/Weather-OS/WineGDK/commit/341425050f4f9b968b807dbd61942dabca8f6af1), Online functionality has been implemented. To get it working, resort to [GDK-Proton](https://github.com/Weather-OS/GDK-Proton)

### NOTES ABOUT THIS PROJECT

Unfortunately, since I don't have the right conditions to be able to   
push my changes upstream, I've decided to declare every part of my contributions that isn't    
derived from other parts of the wine project, CC0 (A.K.A "Public Domain") (i.e xgameruntime).
**What this means**:  
You're allowed to derive, redistribute and reimplement my code at will,  
without any attributions.
**THIS ONLY APPLIES TO THE CODE I HAVE WRITTEN, NOT THE REST OF WINE'S PROJECT!**

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

The native packages in `dlls/xgameruntime.threading`, `dlls/dxvk`, and
`dlls/vkd3d-proton` are installed by `make install` into
`lib/wine/native/<architecture>-windows`. New prefixes receive the x64 DLLs
in `system32` and the i386 graphics DLLs in `syswow64`. XThreading is x64 only.
DXVK supplies Direct3D 8–11 and DXGI; VKD3D-Proton supplies Direct3D 12 and
includes the Minecraft patch from the local patched build. Each package has
a checksum manifest; the graphics packages also retain their upstream README
and license files, installed under `share/wine/native`.

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
Wine's builtin GameInput stays the default: the native redist service currently
requires the unimplemented `ntdll.NtQueryWnfStateData`, so setup leaves it at
demand start and does not start it automatically. See the package README for
the remaining native-service compatibility limitation.

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
- macOS 10.12 or later

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
  You need Xcode/Xcode Command Line Tools or Apple cctools.  The
  minimum requirements for compiling Wine are clang 3.8 with the
  MacOSX10.13.sdk and mingw-w64 v12 for 32-bit wine.  The
  MacOSX10.14.sdk and later can build 64-bit wine.

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
