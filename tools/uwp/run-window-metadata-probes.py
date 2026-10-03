#!/usr/bin/env python3
"""Exercise manifest titles and PNG icons using hidden windows in a disposable prefix."""
import binascii
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib

repo = Path(__file__).resolve().parents[2]
if not os.environ.get("WINEPREFIX"):
    raise SystemExit("Set WINEPREFIX to a disposable test prefix")
wine = os.environ.get("WINE_BIN", str(repo / "build/bin/wine"))
env = dict(os.environ, WINEDEBUG="-all", WINEDLLOVERRIDES="winemenubuilder.exe,winedbg.exe=d")


def png(size, alpha=True):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6 if alpha else 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress((b"\0" + (b"\x20\xa0\x40\xff" if alpha else b"\x20\xa0\x40") * size) * size)) + chunk(b"IEND", b""))


def manifest(executable="probe.exe", title="Manifest &amp; title", logo="Assets\\Logo.png"):
    return f'''<?xml version="1.0"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
 xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10">
 <Applications>
  <Application Executable="other.exe"><uap:VisualElements DisplayName="Wrong app" Square44x44Logo="wrong.png" /></Application>
  <Application Executable="{executable}"><uap:VisualElements DisplayName="{title}" Square44x44Logo="{logo}" /></Application>
 </Applications>
</Package>'''


with tempfile.TemporaryDirectory(prefix="window-metadata-", dir=repo / "build/uwp-diagnostics") as temporary:
    base = Path(temporary)
    scenarios = [
        ("fallback", None, "probe", False, {}),
        ("scaled", manifest(), "Manifest & title", True, {"Assets/Logo.scale-200.png": 88}),
        ("targetsize", manifest(), "Manifest & title", True,
         {"Assets/Logo.targetsize-16.png": 16, "Assets/Logo.targetsize-32_altform-unplated.png": 32}),
        ("plain", manifest(), "Manifest & title", True, {"Assets/Logo.png": 44}),
        ("rgb", manifest(), "Manifest & title", True, {"Assets/Logo.png": 44}),
        ("unrelated", manifest("different.exe"), "probe", False, {"Assets/Logo.png": 44}),
        ("nested", manifest("bin\\probe.exe"), "Manifest & title", True, {"Assets/Logo.scale-100.png": 44}),
        ("malformed", manifest() + "<broken", "probe", False, {"Assets/Logo.png": 44}),
        ("resource", manifest(title="ms-resource:AppName"), "probe", True, {"Assets/Logo.png": 44}),
        ("escape", manifest(logo="..\\outside.png"), "Manifest & title", False, {}),
        ("missing", manifest(), "Manifest & title", False, {}),
    ]
    (base / "outside.png").write_bytes(png(44))
    for arch in ("x86_64", "i686"):
        compiled = base / (arch + ".exe")
        subprocess.run([arch + "-w64-mingw32-gcc", "-Wall", "-Wextra", "-Werror", "-I" + str(repo / "include"),
                        "-I" + str(repo / "include/msvcrt"), "-D__WINE_PE_BUILD", "-D_UCRT", "-municode",
                        str(repo / "tools/uwp/window-metadata-probe.c"), "-o", str(compiled),
                        "-lruntimeobject", "-lole32", "-luuid", "-luser32", "-lgdi32"], check=True)
        for name, xml, expected, icons, assets in scenarios:
            package = base / (arch + "-" + name)
            package.mkdir()
            executable = package / ("bin/probe.exe" if name == "nested" else "probe.exe")
            executable.parent.mkdir(exist_ok=True)
            shutil.copyfile(compiled, executable)
            if xml:
                (package / "AppxManifest.xml").write_text(xml, encoding="utf-8-sig")
            for asset, size in assets.items():
                path = package / asset
                path.parent.mkdir(exist_ok=True)
                path.write_bytes(png(size, alpha=name != "rgb"))
            print(f"{arch}: {name}", flush=True)
            subprocess.run([wine, str(executable), expected, "icons" if icons else "none", "20a040"],
                           env=env, check=True, timeout=30)
