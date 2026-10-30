"""M0.1b: initialize med protocolVersion: 99 — log HELE svaret verbatim.

Formål: afgøre om agenten afviser en ukendt version eller svarer med sin egen,
og om svaret indeholder protocolVersion-feltet. Dette afgør om K1 kan håndhæves klientside.
"""
import json
import os
import subprocess
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

stop = threading.Event()
raw_lines = []

def reader():
    for line in proc.stdout:
        line = line.rstrip("\n")
        if not line:
            continue
        raw_lines.append(line)
        print("<< RAW LINE:", line)
        if stop.is_set():
            break

threading.Thread(target=reader, daemon=True).start()

def send(obj):
    proc.stdin.write(json.dumps(obj, ensure_ascii=False) + "\n")
    proc.stdin.flush()

time.sleep(2.0)

# initialize med protocolVersion 99 — langt over hvad agenten kender
send({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
    "protocolVersion": 99,
    "clientCapabilities": {},
    "clientInfo": {"name": "skrynja-malelab", "version": "0.0.1"},
}})
time.sleep(4.0)

# og med protocolVersion 2 (næste kendte efter 1)
send({"jsonrpc": "2.0", "id": 2, "method": "initialize", "params": {
    "protocolVersion": 2,
    "clientCapabilities": {},
    "clientInfo": {"name": "skrynja-malelab", "version": "0.0.1"},
}})
time.sleep(4.0)

# og med protocolVersion 0
send({"jsonrpc": "2.0", "id": 3, "method": "initialize", "params": {
    "protocolVersion": 0,
    "clientCapabilities": {},
    "clientInfo": {"name": "skrynja-malelab", "version": "0.0.1"},
}})
time.sleep(4.0)

stop.set()
proc.terminate()
try:
    proc.wait(timeout=5)
except subprocess.TimeoutExpired:
    proc.kill()

print("\n=== ANALYSE ===")
for i, line in enumerate(raw_lines):
    try:
        msg = json.loads(line)
        if "result" in msg:
            res = msg["result"]
            print(f"id={msg.get('id')}: result keys={sorted(res.keys())}")
            print(f"  protocolVersion i svar: {res.get('protocolVersion', '<FELT MANGLER>')!r}")
        elif "error" in msg:
            print(f"id={msg.get('id')}: ERROR {msg['error']}")
    except json.JSONDecodeError:
        print(f"linje {i}: ikke JSON")
