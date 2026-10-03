#!/usr/bin/env python3
"""Exercise the Explorer fallback, including its UI, without an XDG portal."""
from contextlib import contextmanager
import os
from pathlib import Path
import subprocess
import tempfile

@contextmanager
def missing_portal_bus(root):
    config = root / 'bus.conf'
    config.write_text('<busconfig><type>session</type><listen>unix:tmpdir=/tmp</listen>'
                      '<policy context="default"><allow send_destination="*"/>'
                      '<allow receive_sender="*"/><allow own="*"/></policy></busconfig>')
    bus = subprocess.Popen(['dbus-daemon', '--nofork', '--print-address=1', '--config-file=' + str(config)],
                           stdout=subprocess.PIPE, text=True)
    try:
        yield bus.stdout.readline().strip()
    finally:
        bus.terminate()
        bus.wait(timeout=5)

repo = Path(__file__).resolve().parents[2]
if not os.environ.get('WINEPREFIX'):
    raise SystemExit('Set WINEPREFIX to a disposable test prefix')
with tempfile.TemporaryDirectory(prefix='picker-fallback-', dir=repo / 'build/uwp-diagnostics') as temporary:
    root = Path(temporary)
    paths = [root / name for name in ('world with space.mcworld', 'mundo-ação.mcworld')]
    for path in paths:
        path.write_bytes(b'WineGDK fallback fixture\n')
    with missing_portal_bus(root) as bus_address:
        for arch in ('x86_64', 'i686'):
            for api in ('sdk', 'uwp'):
                executable = root / f'fallback-{api}-{arch}.exe'
                output = root / f'{api}-{arch}'
                output.mkdir()
                subprocess.run([arch + '-w64-mingw32-gcc', '-Wall', '-Wextra', '-Werror',
                                '-I' + str(repo / 'include'), '-I' + str(repo / 'include/msvcrt'),
                                '-I' + str(repo / 'dlls/microsoft.windows.storage.pickers'),
                                '-D__WINE_PE_BUILD', '-D_UCRT', *(['-DTEST_UWP'] if api == 'uwp' else []), '-municode',
                                str(repo / 'tools/uwp/picker-fallback-probe.c'), '-o', str(executable),
                                '-lruntimeobject', '-lole32', '-luuid', '-luser32'], check=True)
                print('Testing fallback:', api, arch, flush=True)
                arguments = ['Z:' + str(path).replace('/', '\\') for path in [*paths, output]]
                subprocess.run([str(repo / 'build/bin/wine'), str(executable), *arguments],
                               env=dict(os.environ, DBUS_SESSION_BUS_ADDRESS=(bus_address if api == 'uwp' else 'unix:path=' + str(root / 'missing-bus')),
                                        WINEDEBUG=os.environ.get('PICKER_TEST_DEBUG', '-all'),
                                        WINEDLLOVERRIDES='winemenubuilder.exe,winedbg.exe=d'), timeout=120, check=True)
