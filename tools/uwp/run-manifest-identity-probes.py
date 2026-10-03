#!/usr/bin/env python3
"""Exercise automatic loose-package identity with no AppDefaults registry values."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
if not os.environ.get('WINEPREFIX'):
    raise SystemExit('Set WINEPREFIX to a disposable test prefix')
publisher = 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US'
with tempfile.TemporaryDirectory(dir=repo / 'build/uwp-diagnostics', prefix='manifest-test-') as temporary:
    root = Path(temporary)
    for arch in ('x86_64', 'i686'):
        binary = root / (arch + '.exe')
        subprocess.run([arch + '-w64-mingw32-gcc', '-Wall', '-Wextra', '-Werror', '-municode',
                        '-I' + str(repo / 'include'), '-I' + str(repo / 'include/msvcrt'),
                        '-D__WINE_PE_BUILD', '-D_UCRT', str(repo / 'tools/uwp/manifest-identity-probe.c'),
                        '-o', str(binary)], check=True)
        for case in ('release', 'preview', 'nested', 'mismatch', 'malformed'):
            folder = root / (arch + '-' + case)
            folder.mkdir()
            subfolder = folder / 'sub' if case == 'nested' else folder
            subfolder.mkdir(exist_ok=True)
            executable = subfolder / 'Minecraft.Windows.exe'
            shutil.copyfile(binary, executable)
            name = 'Microsoft.MinecraftWindowsBeta' if case == 'preview' else 'Microsoft.MinecraftUWP'
            version = '1.21.12020.0' if case == 'preview' else '1.21.3.0'
            relative = 'sub/Minecraft.Windows.exe' if case == 'nested' else 'Minecraft.Windows.exe'
            if case == 'mismatch': relative = 'Another.exe'
            manifest = f'''<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10">
<Identity Name="{name}" Version="{version}" ProcessorArchitecture="x64" Publisher="{publisher}"/>
<Applications><Application Id="App" Executable="{relative}"/></Applications></Package>'''
            if case == 'malformed': manifest += '<broken'
            (folder / 'AppxManifest.xml').write_text(manifest)
            full = f'{name}_{version}_x64__8wekyb3d8bbwe'
            family = f'{name}_8wekyb3d8bbwe'
            if case in ('mismatch', 'malformed'): full = 'none'
            print(arch, case, flush=True)
            subprocess.run([str(repo / 'build/bin/wine'), str(executable), full, family, family + '!App', publisher],
                           env=dict(os.environ, WINEDEBUG='-all', WINEDLLOVERRIDES='winemenubuilder.exe,winedbg.exe=d'),
                           check=True, timeout=30)
