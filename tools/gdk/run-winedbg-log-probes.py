#!/usr/bin/env python3
"""Exercise automatic WineDbg reports without touching a game process."""
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import time

repo = Path(__file__).resolve().parents[2]
if not os.environ.get("WINEPREFIX"):
    raise SystemExit("Set WINEPREFIX to a disposable initialized test prefix")
wine = os.environ.get("WINE_BIN", str(repo / "wine"))
root = repo / "build/gdk-multiplayer/winedbg-log-tests"
root.mkdir(parents=True, exist_ok=True)
prefix_entry = ("WINEPREFIX=" + os.environ["WINEPREFIX"]).encode()


def report_ok(data):
    return all(part in data.lower() for part in (b"e0421001", b"backtrace", b"modules:", b"threads:"))


def run_case(exe, target, name):
    env = dict(os.environ, WINEDBG_LOG=target, WINEDEBUG="+process,+console")
    log = root / (name + ".log")
    consoles = set()
    with log.open("wb") as output:
        process = subprocess.Popen([wine, str(exe)], env=env, stdout=output, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 40
        while process.poll() is None and time.monotonic() < deadline:
            for entry in Path("/proc").iterdir():
                if not entry.name.isdigit():
                    continue
                try:
                    if "conhost" in (entry / "comm").read_text().lower() and \
                            prefix_entry in (entry / "environ").read_bytes().split(b"\0"):
                        consoles.add(entry.name)
                except (FileNotFoundError, ProcessLookupError, PermissionError):
                    pass
            time.sleep(0.02)
        if process.poll() is None:
            process.kill()
            process.wait()
            raise AssertionError((name, "debugger did not finish", str(log)))
    # WineDbg may handle the synthetic exception and let the fixture return 42.
    # The report and debugger launch below prove that the exception was captured.
    assert process.returncode != 0, (name, process.returncode)
    assert not consoles, (name, "unexpected console", consoles)
    data = log.read_bytes()
    assert b"starting debugger" in data, (name, "crash was not handled by debugger")
    return data


for arch in ("x86_64", "i686"):
    exe = root / (arch + ".exe")
    subprocess.run([arch + "-w64-mingw32-gcc", "-Wall", "-Wextra", "-Werror", "-mwindows",
                    str(repo / "tools/gdk/winedbg-log-probe.c"), "-o", str(exe)], check=True)
    file = root / (arch + " crash relatório.log")
    marker = b"existing report must survive\n"
    file.write_bytes(marker)
    run_case(exe, str(file), arch + "-file")
    report = file.read_bytes()
    assert report.startswith(marker) and report_ok(report), str(file)
    run_case(exe, "Z:" + str(file).replace("/", "\\"), arch + "-append")
    appended = file.read_bytes()
    assert appended.startswith(report) and report_ok(appended[len(report):]), str(file)
    print(arch, "Unix/Windows file paths, append, no console: PASS", flush=True)

    # AF_UNIX path length is limited by the host, so use a short private directory.
    with tempfile.TemporaryDirectory(prefix="winedbg-log-") as tmp:
        path = str(Path(tmp) / "report.sock")
        chunks = []
        errors = []
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
            listener.bind(path)
            listener.listen(1)
            listener.settimeout(35)

            def receive():
                try:
                    connection, _ = listener.accept()
                    with connection:
                        connection.settimeout(35)
                        while data := connection.recv(65536):
                            chunks.append(data)
                except Exception as error:
                    errors.append(repr(error))

            reader = threading.Thread(target=receive, daemon=True)
            reader.start()
            run_case(exe, "unix:" + path, arch + "-socket")
            reader.join(timeout=36)
            assert not reader.is_alive() and not errors, errors
        data = b"".join(chunks)
        (root / (arch + "-socket-report.log")).write_bytes(data)
        assert report_ok(data), "incomplete socket report"
        print(arch, "Unix stream socket, full report, connection closes: PASS", flush=True)

        data = run_case(exe, "unix:" + str(Path(tmp) / "missing.sock"), arch + "-missing-socket")
        assert report_ok(data) and b"Cannot open WINEDBG_LOG" in data
        data = run_case(exe, str(Path(tmp) / "missing-directory/report.log"), arch + "-missing-file")
        assert report_ok(data) and b"Cannot open WINEDBG_LOG" in data
        print(arch, "unavailable destinations fall back to stderr without console: PASS", flush=True)
