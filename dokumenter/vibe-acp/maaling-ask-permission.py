#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Måling: request_permission i ask-mode (telefonen, 2026-11-02).

Kører hele kæden automatisk mod vibe-acp over stdio:
  initialize -> session/new -> session/set_config_option mode=ask
  -> session/prompt (skrivende kald) -> session/request_permission besvares

Én løkke = én tilstandsmaskine: enhver indgående session/request_permission
besvares med det samme ("cancelled"), uanset hvilket trin vi står på.
Målt 2026-11-02: anmodningen kommer med "id":0 (ikke sekventielt id).

Udskriver få linjer: SID, CFG, PERM_REQ, RESULT, file_exists, exit.
Alle frames (ind og ud) gemmes rå i /tmp/acp-perm-wire.ndjson.

Bemærk: agenten kræver pipe/socket på stdin/stdout ("Pipe transport is only
for pipes, sockets and character devices", rc=1 ved redirect til fil) —
derfor må stdout gå gennem et pipe (tee), hvis man kører den manuelt.

Kørsel på telefonen (én kort linje):
  curl -sfL https://raw.githubusercontent.com/storefyr/Skrynja/vibe/acp-alpha-ea9aca/dokumenter/vibe-acp/maaling-ask-permission.py -o /tmp/perm.py && python3 /tmp/perm.py
"""
import json
import os
import subprocess
import threading

AGENT = "/home/defaultuser/.local/bin/vibe-acp"
WIRE = "/tmp/acp-perm-wire.ndjson"
TESTFILE = "/tmp/acp-perm-test.txt"


def main():
    p = subprocess.Popen([AGENT], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL, text=True, bufsize=1)
    threading.Timer(90, p.kill).start()  # watchdog: hæng aldrig
    wire = open(WIRE, "w", encoding="utf-8")

    def S(o):
        line = json.dumps(o)
        wire.write(line + "\n")
        wire.flush()
        p.stdin.write(line + "\n")
        p.stdin.flush()

    def read():
        l = p.stdout.readline()
        if not l:
            return None
        wire.write(l if l.endswith("\n") else l + "\n")
        wire.flush()
        return json.loads(l)

    if os.path.exists(TESTFILE):
        os.remove(TESTFILE)

    answered = False
    sid = None
    stage = 0  # 0: venter id1, 1: venter id2, 2: venter id4, 3: venter id5
    S({"jsonrpc": "2.0", "id": 1, "method": "initialize",
       "params": {"protocolVersion": 1, "clientCapabilities": {},
                  "clientInfo": {"name": "maaling", "version": "0"}}})
    for _ in range(400):
        m = read()
        if m is None:
            break
        meth = m.get("method")
        # Målt 2026-11-02: namespaced navn + "id":0 på anmodningen.
        if meth in ("session/request_permission", "request_permission"):
            pr = m.get("params", {})
            tc = pr.get("toolCall") or {}
            print("PERM_REQ id=%s keys=%s tool=%s options=%s" % (
                m.get("id"), sorted(pr.keys()),
                tc.get("title") or pr.get("toolName"),
                [o.get("optionId") for o in pr.get("options", [])]))
            S({"jsonrpc": "2.0", "id": m["id"],
               "result": {"outcome": {"outcome": "cancelled"}}})
            answered = True
            continue
        mid = m.get("id")
        if mid == 1 and stage == 0:
            S({"jsonrpc": "2.0", "id": 2, "method": "session/new",
               "params": {"cwd": os.path.expanduser("~"), "mcpServers": []}})
            stage = 1
        elif mid == 2 and stage == 1:
            sid = m["result"]["sessionId"] if "result" in m else None
            print("SID:", sid)
            S({"jsonrpc": "2.0", "id": 4, "method": "session/set_config_option",
               "params": {"sessionId": sid, "configId": "mode", "value": "ask"}})
            stage = 2
        elif mid == 4 and stage == 2:
            print("CFG:", json.dumps(m)[:140])
            S({"jsonrpc": "2.0", "id": 5, "method": "session/prompt",
               "params": {"sessionId": sid, "prompt": [
                   {"type": "text",
                    "text": "Opret filen %s med teksten hei" % TESTFILE}]}})
            stage = 3
        elif mid == 5 and stage == 3:
            print("RESULT:", json.dumps(m)[:220])
            break
    print("answered_permission:", answered)
    print("file_exists:", os.path.exists(TESTFILE))
    try:
        p.stdin.close()
    except Exception:
        pass
    try:
        p.wait(timeout=10)
    except Exception:
        p.kill()
    print("exit:", p.returncode)
    wire.close()


if __name__ == "__main__":
    main()
