#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Måling: request_permission i ask-mode (telefonen, 2026-11-02).

Kører hele kæden automatisk mod vibe-acp over stdio:
  initialize -> session/new -> session/set_config_option mode=ask
  -> session/prompt (skrivende kald) -> request_permission besvares "cancelled"

Udskriver kun få linjer: SID, PERM_REQ (id + params-nøgler + options),
RESULT (prompt-svar), om filen blev oprettet, og exit-kode.
Alle frames (ind og ud) gemmes rå i /tmp/acp-perm-wire.ndjson.

Bemærk: agenten kræver pipe/socket på stdin/stdout ("Pipe transport is only
for pipes, sockets and character devices", rc=1 ved redirect til fil) —
derfor må stdout altid gå gennem et pipe (tee), hvis man kører den manuelt.

Kørsel på telefonen (ingen lang kommando at kopiere):
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
    threading.Timer(60, p.kill).start()  # watchdog: hæng aldrig
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

    def until_id(want):
        for _ in range(100):
            m = read()
            if m is None:
                return None
            if m.get("id") == want:
                return m
        return None

    if os.path.exists(TESTFILE):
        os.remove(TESTFILE)

    S({"jsonrpc": "2.0", "id": 1, "method": "initialize",
       "params": {"protocolVersion": 1, "clientCapabilities": {},
                  "clientInfo": {"name": "maaling", "version": "0"}}})
    until_id(1)
    S({"jsonrpc": "2.0", "id": 2, "method": "session/new",
       "params": {"cwd": os.path.expanduser("~"), "mcpServers": []}})
    r = until_id(2)
    sid = r["result"]["sessionId"] if r and "result" in r else None
    print("SID:", sid)
    S({"jsonrpc": "2.0", "id": 4, "method": "session/set_config_option",
       "params": {"sessionId": sid, "configId": "mode", "value": "ask"}})
    S({"jsonrpc": "2.0", "id": 5, "method": "session/prompt",
       "params": {"sessionId": sid, "prompt": [
           {"type": "text", "text": "Opret filen %s med teksten hei" % TESTFILE}]}})

    answered = False
    for _ in range(200):
        m = read()
        if m is None:
            break
        if m.get("method") == "request_permission":
            pr = m.get("params", {})
            tc = pr.get("toolCall") or {}
            print("PERM_REQ id=%s keys=%s tool=%s options=%s" % (
                m.get("id"), sorted(pr.keys()),
                tc.get("title") or pr.get("toolName"),
                [o.get("optionId") for o in pr.get("options", [])]))
            S({"jsonrpc": "2.0", "id": m["id"],
               "result": {"outcome": {"outcome": "cancelled"}}})
            answered = True
        if m.get("id") == 5:
            print("RESULT:", json.dumps(m)[:200])
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
