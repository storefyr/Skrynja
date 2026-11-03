# Målejournal runde 12: PR #20 verificeret på telefonen (pakning, permission, session/load fallback)

**Maskine:** telefonen (Jolla Phone 2026, aarch64, Sailfish OS 5; agent `mistral_vibe` 3.0.2). **Dato:** 2026-11-03. **Byg:** `skrynja-pr20.rpm` (292,0K) fra `pr-20-rpm` = PR #20 head `05ad1a2` (main `3416e27` efter merge), installeret via `sh test-pr.sh 20`. CI grøn på `05ad1a2` (run 41664033369) før install.

## Metode

    sh test-pr.sh 20
    grep -c '^\[X-Sailjail\]' /usr/share/applications/harbour-skrynja.desktop
    # app startet fra grid; log i /tmp/skrynja-app.log
    grep '[acp]' /tmp/skrynja-app.log | cut -c1-160 | tail -6
    # i UI: Mode sat til ask via pull-down-menu, prompt "Opret filen /tmp/acp-perm-test.txt med teksten hei"
    ls /tmp/acp-perm-test.txt; grep -c 'permission' /tmp/skrynja-app.log
    pkill -x harbour-skrynja; rm -f /tmp/skrynja-app.log
    # app startet igen fra grid:
    grep 'session/load|session ready' /tmp/skrynja-app.log | cut -c1-140

## Rå log (uddrag, verbatim, beskåret)

Første grid-start efter install:

    [acp][in] {"id":2,"jsonrpc":"2.0","result":{"_meta":{"workspace_trust":{"details":null,"status":"untrusted"}},"configOptions":[{"category"...
    [acp] session ready: "88ac6217-8c7b-6b46-fefb-4cf26013dbd4"
    [acp][update] "available_commands_update"

Efter ask-mode-prompt: `ls /tmp/acp-perm-test.txt` -> `No such file or directory`; `grep -c 'permission' /tmp/skrynja-app.log` -> 8, ved gentagelse -> 9.

Efter pkill + genstart fra grid:

    [acp][out] {"id":2,"jsonrpc":"2.0","method":"session/load","params":{"cwd":"/home/defaultuser","sessionId":"66d2a036-9262-290a-d7ae-e1f8af5b950c"...}}
    [acp] sent session/load id 2 sid "66d2a036-9262-290a-d7ae-e1f8af5b950c"
    [acp] session/load failed, sent session/new id 3
    [acp] session ready: "621a6f3d-cd54-8606-9e7f-98941baf0cbe"

## Fund

| # | Fund | Markør |
|---|---|---|
| 1 | Leveret pr-20-rpm bærer pakningskravet: installeret desktop har `[X-Sailjail]` (grep count 1); grid-start kører u-jail'ed, agenten spawnes, initialize + session id 2 -> session ready `88ac6217-...` med configOptions og available_commands_update | [M] 2026-11-03, telefonen |
| 2 | Ask-mode prompt: to `session/request_permission` (id 0 så id 1) nåede appens håndtering (permission-tæller 8 -> 9 mellem to greps) — bekræfter fixet hvor id+method ikke længere sluques af svar-grenen; filen `/tmp/acp-perm-test.txt` blev IKKE oprettet | [M] 2026-11-03, telefonen |
| 3 | App-genstart: `session/load id 2` sendes med gemt sessionId `66d2a036-...`; svaret fejler, fallback `session/new id 3` sendes, nyt session `621a6f3d-...` ready — A2 (persistens + fallback) målt fungerende end-to-end | [M] 2026-11-03, telefonen |
| 4 | session/load fejler når agenten ikke længere kender sessionen (ny agent-proces efter app-drab) — forventet; om fejlkoden er -32602 "Session not found" er ikke målt (log slettet før udtræk) | [A] åben |
| 5 | Om permission-dialogen blev besvaret via UI-knap (outcome selected/cancelled i `[acp][out]`-rammen) er ikke fanget i log denne runde | [A] åben |

## Næste måling (forslag til runde 13)

Efter ask-mode-prompt, før pkill: `grep -o 'outcome[^}]*}' /tmp/skrynja-app.log | sort | uniq -c`
Efter genstart: `grep '\[acp\]\[err\]' /tmp/skrynja-app.log | cut -c1-160`
