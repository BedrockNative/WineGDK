#!/usr/bin/env python3
"""Validate console frontend selection, fallback, keyboard input, resizing and ANSI."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

repo = Path(__file__).resolve().parents[2]
if not os.environ.get('WINEPREFIX'):
    raise SystemExit('Set WINEPREFIX to a disposable initialized prefix')
candidates = {
    'xdg-terminal-exec': ['--'], 'xdg-terminal': 'string',
    'x-terminal-emulator': ['-e'], 'sensible-terminal': ['-e'],
    'i3-sensible-terminal': ['-e'], 'rofi-sensible-terminal': ['-e'],
    'exo-open': ['--launch', 'TerminalEmulator'],
    'konsole': ['--separate', '-e'], 'gnome-terminal': ['--wait', '--'],
    'kgx': ['--'], 'ptyxis': ['--new-window', '--'],
    'xfce4-terminal': ['--disable-server', '-x'], 'mate-terminal': ['--disable-factory', '-x'],
    'qterminal': ['-e'], 'tilix': ['-e'], 'kitty': ['--'], 'foot': ['--'], 'alacritty': ['-e'],
    'wezterm': ['start', '--always-new-process', '--'], 'ghostty': ['-e'],
    'terminator': ['-x'], 'xterm': ['-e'], 'urxvt': ['-e'], 'rxvt': ['-e'],
}
with tempfile.TemporaryDirectory(prefix="terminal test ' ação-", dir=repo / 'build/uwp-diagnostics') as tmp:
    root = Path(tmp)
    for arch in ('x86_64', 'i686'):
        exe = root / (arch + '.exe')
        subprocess.run([arch + '-w64-mingw32-gcc', '-Wall', '-Wextra', '-Werror', '-mwindows',
                        str(repo / 'tools/uwp/console-probe.c'), '-o', str(exe)], check=True)
        cases = [(key, {key: value}, 'native', 'resize') for key, value in candidates.items()]
        cases += [('missing', {}, 'fallback', 'output'),
                  ('failed', {'xdg-terminal-exec': 'fail'}, 'fallback', 'output'),
                  ('forced', {'xdg-terminal-exec': ['--']}, 'fallback', 'output'),
                  ('retry', {'xdg-terminal-exec': 'fail', 'x-terminal-emulator': ['-e']}, 'native', 'resize'),
                  ('late', {'xdg-terminal-exec': 'late', 'x-terminal-emulator': ['-e']}, 'native', 'resize')]
        for name, launchers, frontend, mode in cases:
            case = root / (arch + '-' + name)
            case.mkdir()
            (case / 'fixture.json').write_text(json.dumps({'launchers': launchers}))
            (case / 'env').symlink_to('/usr/bin/env')
            for launcher in launchers:
                shutil.copy(repo / 'tools/uwp/terminal-fixture.py', case / launcher)
                (case / launcher).chmod(0o755)
            env = dict(os.environ, PATH=str(case), WINEDEBUG='-all',
                       WINECONSOLE='conhost' if name == 'forced' else '')
            report = case / 'report.txt'
            with (case / 'wine.log').open('w') as log:
                run = subprocess.run([str(repo / 'build/bin/wine'), str(exe),
                                      'Z:' + str(report).replace('/', '\\'), frontend, mode],
                                     env=env, stdout=log, stderr=subprocess.STDOUT, timeout=25)
            text = report.read_text() if report.exists() else (case / 'wine.log').read_text()
            print(arch, name, text.strip(), flush=True)
            if run.returncode or '0 failures' not in text:
                saved = repo / 'build/uwp-diagnostics/console-probe-failure'
                shutil.copytree(case, saved, dirs_exist_ok=True)
                raise RuntimeError((name, run.returncode, text, str(saved)))
            if name == 'forced':
                assert not (case / 'invocations').exists()
            # Let late, unacknowledged children prove they exit without claiming the console.
            if name == 'late': time.sleep(2)
    print('Console probes: 58 cases passed')
