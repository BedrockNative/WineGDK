#!/usr/bin/env python3
"""Test boot progress and GUI startup using disposable prefixes, never a game."""
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import time

repo = Path(__file__).resolve().parents[2]
wine = os.environ.get('WINE_BIN', str(repo / 'wine'))
root = repo / 'build/gdk-multiplayer/startup-events-tests'
root.mkdir(parents=True, exist_ok=True)
# This runner owns this uniquely created prefix; it never stops other prefixes.
prefix = Path(tempfile.mkdtemp(prefix='prefix relatório ', dir=root))
env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all')
for key in ('WINEBOOT_HIDE_DIALOG', 'WINEBOOT_LOG', 'WINE_STARTUP_LOG'):
    env.pop(key, None)
exe = root / 'startup-probe.exe'
subprocess.run(['x86_64-w64-mingw32-gcc', '-Wall', '-Wextra', '-Werror', '-mwindows',
                str(repo / 'tools/gdk/startup-events-probe.c'), '-o', str(exe)], check=True)


def read_events(path):
    return [json.loads(line) for line in path.read_text().splitlines()]


def run(name, arguments, **settings):
    with (root / (name + '.log')).open('wb') as log:
        started = time.monotonic()
        result = subprocess.run([wine, *map(str, arguments)], env=dict(env, **settings),
                                stdout=log, stderr=log, timeout=180)
        assert result.returncode == 0, (name, result.returncode)
    return time.monotonic() - started


def check_startup(events):
    windows = [e for e in events if e['event'] == 'first_window_shown']
    assert len(windows) == 1, windows
    pid = windows[0]['pid_unix']
    own = [e for e in events if e['pid_unix'] == pid]
    names = [e['event'] for e in own]
    expected = ['process_start', 'server_connected', 'prefix_wait_begin', 'prefix_wait_end',
                'executable_loaded', 'first_window_shown']
    assert names == expected, names
    assert all(e['prefix'] == str(prefix) for e in own)
    times = {e['event']: e['monotonic_ms'] for e in own}
    assert list(times.values()) == sorted(times.values()), times
    delay = times['first_window_shown'] - times['executable_loaded']
    assert delay >= 1100, delay
    print('  prefix wait:', times['prefix_wait_end'] - times['prefix_wait_begin'],
          'ms; executable to window:', delay, 'ms', flush=True)


startup = root / 'startup.jsonl'
boot = root / 'boot.jsonl'
startup.write_text('')
boot.write_text('')
print('Test prefix:', prefix, flush=True)
run('cold', [exe], WINEBOOT_LOG=str(boot), WINE_STARTUP_LOG=str(startup))
check_startup(read_events(startup))
boot_events = read_events(boot)
assert any(e['stage'] == 'prefix-update' and e['event'] == 'begin' for e in boot_events)
assert any(e['stage'] == 'boot' and e['event'] == 'end' for e in boot_events)
assert all(e['prefix'] == str(prefix) for e in boot_events)
print('Cold prefix creation, JSON escaping and delayed first window: PASS', flush=True)

# Keep this test wineserver alive so the warm launch cannot accidentally boot anew.
subprocess.run([wine, 'cmd.exe', '/c', 'exit'], env=env, stdout=subprocess.DEVNULL,
               stderr=subprocess.DEVNULL, check=True, timeout=60)
keeper = subprocess.Popen([wine, str(exe), 'hold'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
before = startup.read_bytes()
boot_before = boot.read_bytes()
run('warm', [exe], WINEBOOT_LOG=str(boot), WINE_STARTUP_LOG=str(startup))
assert startup.read_bytes().startswith(before)
warm = [json.loads(line) for line in startup.read_bytes()[len(before):].splitlines()]
check_startup(warm)
assert boot.read_bytes() == boot_before, 'Warm launch unexpectedly ran wineboot'
print('Warm launch skips prefix update and appends startup events: PASS', flush=True)

run('forced-update', ['wineboot', '-u'], WINEBOOT_LOG=str(boot), WINEDEBUG='+wineboot')
assert b'wait dialog suppressed' in (root / 'forced-update.log').read_bytes()
assert sum(e['event'] == 'begin' and e['stage'] == 'prefix-update' for e in read_events(boot)) == 2
print('Explicit update emits stages and suppresses wait dialog: PASS', flush=True)

with tempfile.TemporaryDirectory(prefix='wine-startup-socket-') as tmp:
    path = str(Path(tmp) / 'events.sock')
    events, errors, readers = [], [], []
    stopped = threading.Event()
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
        listener.bind(path)
        listener.listen(128)
        listener.settimeout(0.1)

        def receive(connection):
            try:
                with connection:
                    connection.settimeout(20)
                    data = b''
                    while chunk := connection.recv(65536):
                        data += chunk
                    events.extend(json.loads(line) for line in data.splitlines())
            except Exception as error:
                errors.append(repr(error))

        def accept():
            while not stopped.is_set():
                try:
                    connection, _ = listener.accept()
                except TimeoutError:
                    continue
                reader = threading.Thread(target=receive, args=(connection,))
                reader.start()
                readers.append(reader)

        worker = threading.Thread(target=accept)
        worker.start()
        try:
            run('socket-update', ['wineboot', '-u'], WINEBOOT_LOG='unix:' + path)
            run('socket-startup', [exe], WINE_STARTUP_LOG='unix:' + path)
        finally:
            time.sleep(0.2)
            stopped.set()
            worker.join()
            for reader in readers:
                reader.join(timeout=21)
        assert not errors, errors
        assert all(not r.is_alive() for r in readers)
        assert any(e['source'] == 'wineboot' and e['stage'] == 'boot' and e['event'] == 'end' for e in events)
        check_startup(sorted((e for e in events if e['source'] == 'startup'), key=lambda e: e['monotonic_ms']))
        (root / 'socket-events.jsonl').write_text(''.join(json.dumps(e) + '\n' for e in events))
    print('Shared Unix socket receives both progress and startup, all connections close: PASS', flush=True)

    elapsed = run('missing-listener', [exe], WINE_STARTUP_LOG='unix:' + path)
    fallback = [json.loads(line) for line in (root / 'missing-listener.log').read_text().splitlines()
                if line.startswith('{')]
    check_startup(fallback)
    assert elapsed < 20, elapsed
    run('missing-boot-listener', ['wineboot', '-u'], WINEBOOT_LOG='unix:' + path, WINEDEBUG='+wineboot')
    fallback = (root / 'missing-boot-listener.log').read_text()
    assert 'wait dialog suppressed' in fallback and '"stage":"boot"' in fallback
    print('Unavailable socket falls back to stderr and does not prevent boot/startup: PASS', flush=True)

print('All startup/progress probes passed; artifacts:', root, flush=True)

keeper.terminate()
keeper.wait(timeout=10)
