"""M0.0 + M0.1: start vibe-acp over stdio, send initialize, log svaret."""
import json
import os
import subprocess
import sys
import threading
import time

ENV = dict(os.environ)
ENV["PYTHONPATH"] = "/workspace/malelab"

proc = subprocess.Popen(
    ["/usr/local/bin/vibe-acp"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    env=ENV,
    text=True,
    bufsize=1,
)

lines_out = []
stop = threading.Event()

def reader():
    for line in proc.stdout:
        line = line.strip()
        if line:
            lines_out.append(line)
            try:
                msg = json.loads(line)
                print("<< RECV:", json.dumps(msg, ensure_ascii=False)[:2000])
            except json.JSONDecodeError:
                print("<< RAW :", line[:500])
        if stop.is_set():
            break

t = threading.Thread(target=reader, daemon=True)
t.start()

def send(obj):
    line = json.dumps(obj, ensure_ascii=False)
    print(">> SEND:", line[:800])
    proc.stdin.write(line + "\n")
    proc.stdin.flush()

time.sleep(2.0)

send({
    "jsonrpc": "2.0",
    "id": 1,
    "method": "initialize",
    "params": {
        "protocolVersion": 1,
        "clientCapabilities": {},
        "clientInfo": {"name": "skrynja-malelab", "version": "0.0.1"},
    },
})

time.sleep(5.0)

send({"jsonrpc": "2.0", "method": "notifications/initialized"})
time.sleep(2.0)

stop.set()
proc.terminate()
try:
    proc.wait(timeout=5)
except subprocess.TimeoutExpired:
    proc.kill()

err = proc.stderr.read() if proc.stderr else ""
print("\n=== STDERR (tail) ===")
print(err[-1500:] if err else "(tom)")
print("\n=== SUMMARY ===")
print("linjer modtaget:", len(lines_out))
