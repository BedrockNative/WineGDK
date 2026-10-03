#!/usr/bin/env python3
"""Loopback RFC 6455 fixture; no external service or credentials required."""
import base64
import hashlib
import socketserver
import struct
import time


def read_exact(stream, size):
    data = bytearray()
    while len(data) < size:
        part = stream.read(size - len(data))
        if not part:
            raise EOFError
        data.extend(part)
    return bytes(data)


def frame(opcode, data=b'', final=True):
    head = bytes([opcode | (0x80 if final else 0)])
    if len(data) < 126:
        return head + bytes([len(data)]) + data
    if len(data) < 65536:
        return head + b'\x7e' + struct.pack('!H', len(data)) + data
    return head + b'\x7f' + struct.pack('!Q', len(data)) + data


class Handler(socketserver.StreamRequestHandler):
    def handle(self):
        try:
            self.connection.settimeout(15)
            request = self.rfile.readline(8192).decode('ascii')
            headers = {}
            while True:
                line = self.rfile.readline(8192)
                if line == b'\r\n':
                    break
                if not line:
                    return
                name, value = line.decode('ascii').split(':', 1)
                headers[name.lower()] = value.strip()
            if '/reject ' in request:
                self.wfile.write(b'HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n')
                return
            if '/stall ' in request:
                time.sleep(5)
                return
            nonce = headers['sec-websocket-key']
            assert len(base64.b64decode(nonce, validate=True)) == 16
            accept = base64.b64encode(hashlib.sha1((nonce + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest())
            response = b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ' + accept + b'\r\n'
            if 'wine-probe' in headers.get('sec-websocket-protocol', '').split(', '):
                response += b'Sec-WebSocket-Protocol: wine-probe\r\n'
            self.wfile.write(response + b'\r\n')
            while True:
                first, second = read_exact(self.rfile, 2)
                assert first & 0x80 and second & 0x80, 'client frames must be final and masked'
                size = second & 127
                if size == 126:
                    size = struct.unpack('!H', read_exact(self.rfile, 2))[0]
                elif size == 127:
                    size = struct.unpack('!Q', read_exact(self.rfile, 8))[0]
                assert size <= 1024 * 1024
                mask = read_exact(self.rfile, 4)
                data = bytes(x ^ mask[i % 4] for i, x in enumerate(read_exact(self.rfile, size)))
                opcode = first & 15
                if opcode == 8:
                    if '/ignore-close ' not in request:
                        self.wfile.write(frame(8, data))
                    else:
                        time.sleep(5)
                    return
                if opcode == 9:
                    self.wfile.write(frame(10, data))
                    continue
                if opcode == 10:
                    assert data == b'probe'
                    continue
                assert opcode in (1, 2)
                # Ping between fragments exercises control frames and reassembly.
                split = len(data) // 2
                self.wfile.write(frame(opcode, data[:split], False) + frame(9, b'probe') + frame(0, data[split:]))
        except (EOFError, ConnectionError, TimeoutError, OSError):
            pass


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=18766)
    args = parser.parse_args()
    with Server(('127.0.0.1', args.port), Handler) as server:
        print(f'WebSocket fixture ready on 127.0.0.1:{args.port}', flush=True)
        server.serve_forever()
