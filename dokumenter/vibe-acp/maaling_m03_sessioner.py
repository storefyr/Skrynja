"""M0.3 delvist: session/new uden prompt — observer hvad agenten udsender.

Agenten kræver en API-nøgle for at køre prompts, så vi måler hvad vi kan:
- session/new svar (sessionId?)
- session/load adfærd
- hvad der sker ved session/prompt uden nøgle (fejlkode)
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

def reader():
    for line in proc.stdout:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
            s = json.dumps(msg, ensure_ascii=False)
            print("<< RECV:", s[:1500])
        except json.JSONDecodeError:
            print("<< RAW :", line[:300])
        if stop.is_set():
            break

threading.Thread(target=reader, daemon=True).start()

def send(obj):
    line = json.dumps(obj, ensure_ascii=False)
    print(">> SEND:", line[:600])
    proc.stdin.write(line + "\n")
    proc.stdin.flush()

time.sleep(2.0)

send({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
    "protocolVersion": 1,
    "clientCapabilities": {},
    "clientInfo": {"name": "skrynja-malelab", "version": "0.0.1"},
}})
time.sleep(3.0)

send({"jsonrpc": "2.0", "method": "notifications/initialized"})
time.sleep(1.0)

send({"jsonrpc": "2.0", "id": 2, "method": "session/new", "params": {"cwd": "/workspace", "mcpServers": []}})
time.sleep(4.0)

# M0.11: session/load med et ukendt id
send({"jsonrpc": "2.0", "id": 3, "method": "session/load", "params": {"sessionId": "ikke-eksisterende", "cwd": "/workspace", "mcpServers": []}})
time.sleep(4.0)

# session/prompt uden API-nøgle — forventer fejl, måler fejlkode (M0.14 delvist)
send({"jsonrpc": "2.0", "id": 4, "method": "session/prompt", "params": {
    "sessionId": "test-session-1",
    "prompt": [{"type": "text", "text": "sig hej"}],
}})
time.sleep(8.0)

stop.set()
proc.terminate()
try:
    proc.wait(timeout=5)
except subprocess.TimeoutExpired:
    proc.kill()

err = proc.stderr.read() if proc.stderr else ""
print("\n=== STDERR (tail) ===")
print(err[-800:] if err else "(tom)")
