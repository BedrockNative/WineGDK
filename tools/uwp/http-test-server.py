#!/usr/bin/env python3
"""Local fixtures for the IXMLHTTPRequest2 transport probe; no external requests."""
import gzip
import http.server
import time
import zlib

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *_):
        pass
    def do_GET(self):
        if self.path == "/continue":
            self.wfile.write(b"HTTP/1.1 100 Continue\r\n\r\n")
        if self.path == "/continue-headers":
            self.wfile.write(b"HTTP/1.1 100 Continue\r\nX-Interim: yes\r\n\r\n")
        if self.path == "/slow":
            time.sleep(2)
        body = b"transport-ok"
        code = 200
        if self.path == "/missing":
            code, body = 404, b"not-found"
        if self.path.startswith("/large"):
            body = b"0123456789abcdef" * 8193
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/get")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        chunked = self.path.startswith("/chunked-")
        encoding = None
        if self.path in ("/gzip", "/chunked-gzip", "/chunked-gzip-delayed", "/large-gzip"):
            body = gzip.compress(body)
            encoding = "gzip"
        if self.path.endswith("deflate"):
            compressor = zlib.compressobj(wbits=-15 if "raw-deflate" in self.path else 15)
            body = compressor.compress(body) + compressor.flush()
            encoding = "deflate"
        self.send_response(code)
        if chunked:
            self.send_header("Transfer-Encoding", "chunked")
        else:
            self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Probe", "wine-http")
        if encoding:
            self.send_header("Content-Encoding", encoding)
        self.end_headers()
        try:
            if chunked:
                chunk_size = len(body) if self.path == "/chunked-gzip-delayed" else 1
                for pos in range(0, len(body), chunk_size):
                    chunk = body[pos:pos + chunk_size]
                    self.wfile.write(f"{len(chunk):x}\r\n".encode() + chunk + b"\r\n")
                    self.wfile.flush()
                if self.path == "/chunked-gzip-delayed":
                    time.sleep(0.15)
                self.wfile.write(b"0\r\n\r\n")
            else:
                self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass
    def do_POST(self):
        data = self.rfile.read(int(self.headers["Content-Length"]))
        self.send_response(201)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("X-Probe", self.headers.get("X-Request", "missing"))
        self.end_headers()
        self.wfile.write(data)

if __name__ == "__main__":
    import sys
    server = http.server.ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), Handler)
    if len(sys.argv) > 2:
        from pathlib import Path
        Path(sys.argv[2]).write_text(str(server.server_address[1]))
    print("HTTP fixture ready", flush=True)
    server.serve_forever()
