#!/usr/bin/env python3
"""Run UWP picker probes against an isolated XDG FileChooser D-Bus fixture."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

repo = Path(__file__).resolve().parents[2]
if not os.environ.get('WINEPREFIX'):
    raise SystemExit('Set WINEPREFIX to a disposable test prefix')
if '--private-bus' not in sys.argv:
    raise SystemExit(subprocess.call(['dbus-run-session', '--', sys.executable, __file__, '--private-bus'],
                                     env=dict(os.environ, WINEGDK_PICKER_TEST_BUS='1')))
with tempfile.TemporaryDirectory(prefix='picker-test-', dir=repo / 'build/uwp-diagnostics') as temporary:
    root = Path(temporary)
    for arch in ('x86_64', 'i686'):
        executable = root / ('picker-' + arch + '.exe')
        subprocess.run([arch + '-w64-mingw32-gcc', '-Wall', '-Wextra', '-Werror', '-I' + str(repo / 'include'),
                        '-I' + str(repo / 'include/msvcrt'), '-D__WINE_PE_BUILD', '-D_UCRT', '-municode',
                        str(repo / 'tools/uwp/picker-probe.c'), '-o', str(executable),
                        '-lruntimeobject', '-lole32', '-luuid'], check=True)
        ready = root / 'ready'
        ready.unlink(missing_ok=True)
        (root / 'closed').unlink(missing_ok=True)
        fixture = subprocess.Popen([sys.executable, str(repo / 'tools/uwp/picker-portal-fixture.py'), str(root)])
        try:
            for _ in range(100):
                if ready.exists(): break
                if fixture.poll() is not None: raise RuntimeError('Portal fixture exited')
                time.sleep(0.05)
            else: raise RuntimeError('Portal fixture timed out')
            paths = ['Z:' + str(root / p).replace('/', '\\') for p in ('world with space.mcworld', 'mundo-ação.mcworld')]
            print('Testing', arch, flush=True)
            subprocess.run([str(repo / 'build/bin/wine'), str(executable), *paths],
                           env=dict(os.environ, WINEDEBUG=os.environ.get('PICKER_TEST_DEBUG', '-all'), WINEDLLOVERRIDES='winemenubuilder.exe,winedbg.exe=d'),
                           timeout=45, check=True)
            assert (root / "closed").exists(), "Cancel did not close the portal request"
        finally:
            fixture.terminate()
            fixture.wait(timeout=5)
