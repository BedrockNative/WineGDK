#!/usr/bin/env python3
"""Test Store license response handling using a private local broker fixture."""
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import threading
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[2]
out = repo / 'build/uwp-diagnostics'
prefix = os.environ['WINEPREFIX']

def read(sock, size):
    result = b''
    while len(result) < size:
        data = sock.recv(size - len(result))
        if not data:
            raise RuntimeError('short broker request')
        result += data
    return result

for arch in ('x86_64', 'i686'):
    exe = out / f'store-license-probe-{arch}.exe'
    subprocess.run([f'{arch}-w64-mingw32-gcc', '-I' + str(repo / 'include'),
        '-I' + str(repo / 'include/msvcrt'), '-D__WINE_PE_BUILD', '-D_UCRT',
        str(repo / 'tools/uwp/store-license-probe.c'), '-o', str(exe),
        '-lruntimeobject', '-lole32', '-ladvapi32'], check=True)
    cases = [('active', 'true', '140000000000000000', '0', '0', '1'),
             ('inactive', 'false', '140000000000000000', '0', '0', '0'),
             ('provider error', 'false', '0', '2147942405', '80070005', '0'),
             ('bad boolean', 'invalid', '140000000000000000', '0', '80070057', '0'),
             ('bad expiration', 'true', '99999999999999999999999', '0', '80070057', '0')]
    for label, active, expiry, status, expected, value in cases:
        with tempfile.TemporaryDirectory(prefix='wine-store-fixture-') as tmp:
            path = str(Path(tmp) / 'broker.sock')
            errors = []
            server = socket.socket(socket.AF_UNIX)
            server.bind(path)
            server.listen(1)
            server.settimeout(30)
            def serve():
                try:
                    conn, _ = server.accept()
                    with conn:
                        conn.settimeout(10)
                        magic, kind, size = struct.unpack('<4sHH', read(conn, 8))
                        assert magic == b'XSDX' and kind == 9
                        request = ET.fromstring(read(conn, size))
                        assert request.findtext('Operation') == 'License'
                        assert request.findtext('PackageFamilyName') == 'Wine.StoreProbe_test'
                        xml = (f'<StoreResponse><Status>{status}</Status>'
                               '<PackageFamilyName>Wine.StoreProbe_test</PackageFamilyName>'
                               f'<IsActive>{active}</IsActive><IsTrial>false</IsTrial>'
                               f'<ExpirationDate>{expiry}</ExpirationDate></StoreResponse>').encode()
                        conn.sendall(struct.pack('<4sHH', b'XSDX', 10, len(xml)) + xml)
                except Exception as error:
                    errors.append(error)
            worker = threading.Thread(target=serve, daemon=True)
            worker.start()
            print(arch, label, flush=True)
            result = subprocess.run([str(repo / 'build/bin/wine'), str(exe), expected, value],
                env=dict(os.environ, WINEPREFIX=prefix, XODUS_SOCKET=path, WINEDEBUG='-all'), timeout=35)
            worker.join(2)
            server.close()
            assert not worker.is_alive() and not errors, errors
            assert result.returncode == 0, result.returncode
