#!/usr/bin/env python3
"""Drives a game the way its bridge entry says, and checks each result's schema.

    tests/bridge_entry.py PROJECT_DIR GODOT

Runs, from PROJECT_DIR/baker-bridge.json: the check, the vocabulary, a scenario run, and
the console (one request). Prints one line per check; exits 1 if any fails.
"""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

project, godot = sys.argv[1], sys.argv[2]
entry = json.load(open(os.path.join(project, "baker-bridge.json")))
failures = 0


def check(ok, what):
    global failures
    print(("ok   " if ok else "FAIL ") + what)
    failures += 0 if ok else 1


def fill(args, **values):
    values.setdefault("godot", godot)
    out = []
    for a in args:
        for k, v in values.items():
            a = a.replace("{" + k + "}", str(v))
        out.append(a)
    return out


def run(args):
    return subprocess.run(args, cwd=project, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                          timeout=120).returncode


tmp = tempfile.mkdtemp()
tool = entry["tool"]
for name, schema in (("check", "baker.check/"), ("vocabulary", None)):
    out = os.path.join(tmp, name + ".json")
    code = run(fill(tool["command"] + tool["commands"][name] + tool["out"], out=out))
    d = json.load(open(out)) if os.path.exists(out) else {}
    ok = code == 0 and (d.get("schema", "").startswith(schema) if schema else
                        {"reference", "tweaks", "shapes"} <= d.keys())
    check(ok, f"bridge entry: {name}")

sim = entry["sim"]
out = os.path.join(tmp, "sim")
run(fill(sim["command"] + sim["scenario"] + sim["out"], name="jump", out=out))
results = fill([sim["results"]], out=out)[0]
d = json.load(open(results)) if os.path.exists(results) else {}
check(d.get("schema") == sim["schema"] and d.get("status") == "passed", "bridge entry: sim")

with socket.socket() as s:
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
console = entry["console"]
game = subprocess.Popen(fill(console["command"], port=port), cwd=project,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    host, _, p = fill([console["address"]], port=port)[0].partition(":")
    end = time.time() + 30
    while True:
        try:
            sock = socket.create_connection((host, int(p)), timeout=10)
            break
        except OSError:
            if time.time() > end:
                raise
            time.sleep(0.2)
    sock.sendall(b"(1 {config.layers})\n")
    data = b""
    while not data.endswith(b"\n\n"):
        chunk = sock.recv(65536)
        if not chunk:
            break
        data += chunk
    reply = json.loads(data.decode().strip())
    check(reply.get("schema", "").startswith("baker.reply/") and reply.get("ok"),
          "bridge entry: console")
finally:
    game.kill()

print(f"{failures} failure(s)")
sys.exit(1 if failures else 0)
