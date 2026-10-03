#!/usr/bin/python3
"""Controlled terminal launcher for the console regression probe; no desktop windows."""
import fcntl
import os
from pathlib import Path
import pty
import select
import shlex
import signal
import struct
import sys
import termios
import time

root = Path(__file__).resolve().parent
launcher = Path(sys.argv[0]).name
# The driver copies this script into a private PATH directory per test.
config = __import__('json').loads((root / 'fixture.json').read_text())
args = sys.argv[1:]
expected = config['launchers'][launcher]
if expected == 'fail':
    sys.exit(1)
if expected == 'late':
    time.sleep(6)
    expected = ['--']
if expected == 'string':
    assert len(args) == 1, args
    args = shlex.split(args[0])
else:
    assert args[:len(expected)] == expected, args
    args = args[len(expected):]
with (root / 'invocations').open('a') as output:
    output.write(launcher + '\n')
pid, fd = pty.fork()
if pid == 0:
    fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 120, 0, 0))
    os.execvp(args[0], args)
data = b''
sent = resized = False
finished = False
try:
    with (root / (launcher + '.output')).open('wb', buffering=0) as output:
        for _ in range(300):
            if select.select([fd], [], [], .1)[0]:
                try:
                    chunk = os.read(fd, 65536)
                except OSError:
                    break
                if not chunk:
                    break
                output.write(chunk)
                data += chunk
                if not resized and b'WineGDK terminal resize ready' in data:
                    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack('HHHH', 28, 92, 0, 0))
                    resized = True
                if not sent and b'WineGDK terminal input ready' in data:
                    time.sleep(.1)
                    os.write(fd, b'terminal-input\r')
                    sent = True
            if os.waitpid(pid, os.WNOHANG)[0]:
                finished = True
                break
finally:
    os.close(fd)
    if not finished:
        try:
            os.kill(pid, signal.SIGTERM)
            os.waitpid(pid, 0)
        except ProcessLookupError:
            pass
    (root / (launcher + '.done')).touch()
