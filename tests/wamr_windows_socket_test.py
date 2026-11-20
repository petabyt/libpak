#!/usr/bin/env python3
"""Run the WAMR Windows TCP/UDP loopback probe under Wine."""

import os
from pathlib import Path
import socket
import subprocess
import sys
import threading


def run(wine, iwasm, guest, udp):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM if udp else socket.SOCK_STREAM) as server:
        server.bind(("127.0.0.1", 0))
        server.settimeout(10)
        if not udp:
            server.listen(1)
        errors = []

        def echo():
            try:
                if udp:
                    data, peer = server.recvfrom(64)
                    server.sendto(data, peer)
                else:
                    with server.accept()[0] as conn:
                        conn.settimeout(10)
                        data = conn.recv(64)
                        conn.sendall(data)
            except OSError as error:
                errors.append(error)

        thread = threading.Thread(target=echo)
        thread.start()
        guest_path = "Z:" + str(Path(guest).resolve()).replace("/", "\\")
        result = subprocess.run(
            [wine, iwasm, "--addr-pool=127.0.0.0/8", "-f", "probe", guest_path,
             str(int(udp)), str(server.getsockname()[1])],
            capture_output=True, text=True, timeout=15, env=os.environ.copy(),
        )
        thread.join(timeout=12)
        if result.returncode or result.stdout.strip() != "0x0:i32" or errors or thread.is_alive():
            raise RuntimeError(f"{'UDP' if udp else 'TCP'} failed: {result.stdout!r} {result.stderr!r} {errors}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(f"usage: {sys.argv[0]} WINE IWASM.EXE GUEST.WASM")
    for datagram in (False, True):
        run(*sys.argv[1:], datagram)
    print("TCP and UDP loopback passed")
