#!/usr/bin/env python3
"""Sends lines to a running game's Baker console and prints each answer.

    tests/console.py PORT '(1 {config.explain player.jump.height})' ...

A request, (ID {command}), is answered with one line of JSON (baker.reply/1.1); each
answer ends with an empty line. Waits up to 30 s for the game to start listening.
"""
import socket
import sys
import time


def connect(port: int, seconds: float = 30.0) -> socket.socket:
    end = time.time() + seconds
    while True:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=10)
        except OSError:
            if time.time() > end:
                raise
            time.sleep(0.2)


def ask(sock: socket.socket, line: str) -> str:
    sock.sendall(line.encode() + b"\n")
    data = b""
    while not data.endswith(b"\n\n"):
        chunk = sock.recv(65536)
        if not chunk:
            break
        data += chunk
    return data.decode().rstrip("\n")


def main() -> None:
    sock = connect(int(sys.argv[1]))
    for line in sys.argv[2:]:
        print(ask(sock, line), flush=True)


if __name__ == "__main__":
    main()
