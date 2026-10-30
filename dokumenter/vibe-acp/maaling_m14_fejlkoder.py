"""M0.14 delvist: fremprovoker fejlkoder og mål hvad agenten faktisk returnerer.

Mål: -32022, -32602, -32800 (ifølge vejkortet †), plus hvad vibe-acp faktisk bruger.
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
            mid = msg.get("id")
            if "result" in msg:
                print(f"<< id={mid} RESULT:", json.dumps(msg["result"], ensure_ascii=False)[:300])
            elif "error" in msg:
                print(f"<< id={mid} ERROR code={msg['error'].get('code')} msg={msg['error'].get('message')!r} data={msg['error'].get('data')}")
            else:
                print("<< NOTIF:", json.dumps(msg, ensure_ascii=False)[:300])
        except json.JSONDecodeError:
            pass
        if stop.is_set():
            break

threading.Thread(target=reader, daemon=True).start()

def send(obj):
    proc.stdin.write(json.dumps(obj, ensure_ascii=False) + "\n")
    proc.stdin.flush()

time.sleep(2.0)

# 1. Ukendt metode -> forvent -32601
send({"jsonrpc": "2.0", "id": 100, "method": "totally/unknown_method", "params": {}})
time.sleep(2.0)

# 2. initialize med ugyldig params (mangler protocolVersion) -> forvent -32602
send({"jsonrpc": "2.0", "id": 101, "method": "initialize", "params": {"clientCapabilities": {}}})
time.sleep(2.0)

# 3. session/prompt på ikke-eksisterende session -> mål kode
send({"jsonrpc": "2.0", "id": 102, "method": "session/prompt", "params": {"sessionId": "nope", "prompt": []}})
time.sleep(2.0)

# 4. session/new uden cwd -> mål om cwd er påkrævet
send({"jsonrpc": "2.0", "id": 103, "method": "session/new", "params": {"mcpServers": []}})
time.sleep(3.0)

# 5. set_session_config_option på ukendt session
send({"jsonrpc": "2.0", "id": 104, "method": "session/set_config_option", "params": {"sessionId": "nope", "configId": "model", "value": "x"}})
time.sleep(2.0)

# 6. Malformet JSON linje -> parse error?
proc.stdin.write("{ikke json\n")
proc.stdin.flush()
time.sleep(2.0)

# 7. initialize igen (anden version) for at se versionshåndtering
send({"jsonrpc": "2.0", "id": 105, "method": "initialize", "params": {"protocolVersion": 99, "clientCapabilities": {}, "clientInfo": {"name": "x", "version": "0"}}})
time.sleep(3.0)

stop.set()
proc.terminate()
try:
    proc.wait(timeout=5)
except subprocess.TimeoutExpired:
    proc.kill()
