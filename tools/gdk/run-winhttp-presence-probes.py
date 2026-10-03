#!/usr/bin/env python3
"""Check optional, single and split writes against a local validating server."""
import http.server
import os
from pathlib import Path
import subprocess
import threading

repo = Path(__file__).resolve().parents[2]
if not os.environ.get('WINEPREFIX'):
    raise SystemExit('Set WINEPREFIX to a disposable test prefix')
expected = b'{"state":"active","activity":{"description":"untouched request body"}}'

class Handler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        body = self.rfile.read(int(self.headers['Content-Length']))
        valid = (body == expected and self.headers.get('Content-Type') == 'application/example'
                 and self.headers.get('x-xbl-contract-version') == '42')
        self.send_response(200 if valid else 400)
        self.send_header('Content-Length', '0')
        self.end_headers()
    def log_message(self, *args):
        pass

with http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler) as server:
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        for arch in ('x86_64', 'i686'):
            exe = repo / 'build/gdk-startup' / f'winhttp-presence-{arch}.exe'
            exe.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run([f'{arch}-w64-mingw32-gcc', '-Wall', '-Wextra', '-Werror',
                            str(repo / 'tools/gdk/winhttp-presence-probe.c'), '-o', str(exe), '-lwinhttp'], check=True)
            print(arch, flush=True)
            subprocess.run([os.environ.get('WINE_BIN', str(repo / 'wine')), str(exe), str(server.server_port)],
                           env=dict(os.environ, WINEDEBUG='-all'), check=True, timeout=30)
    finally:
        server.shutdown()
        thread.join()
