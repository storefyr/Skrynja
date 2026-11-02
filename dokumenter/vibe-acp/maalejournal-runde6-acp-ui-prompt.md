# Målejournal runde 6: første prompt gennem app-UI'et (ACP clean slate)

**Maskine:** telefonen (Jolla Phone 2026, aarch64). **Dato:** 2026-11-02. **Byg:** PR #19 gren `vibe/acp-alpha-ea9aca` @ `5448808` (clean slate: uden mistralApi, nyt AcpChatPage), installeret via `sh test-pr.sh 19` (`pr-19-rpm`, 283 843 byte). CI run 41443576360 grøn før install.

## Metode

App startet i baggrund fra terminalen med log til fil; besked skrevet i AcpChatPage-QML; log udtrukket efter svar:

    harbour-skrynja >/tmp/skrynja-ui.log 2>&1 &
    # i UI: "Svar kun med ordet OK" -> svar vist i appen: "OK"
    grep '\[acp\]' /tmp/skrynja-ui.log | cut -c1-220 | tail -12
    pkill -f harbour-skrynja

## Rå log (uddrag, verbatim, beskåret til 220 tegn)

    [acp][update] "user_message_chunk"
    [acp][update] "usage_update"          (før prompt: used 0)
    [acp][in] session/update ... "content":{"text":"OK","type":"text"},"messageId":... "agent_message_chunk"
    [acp][update] "usage_update"          (flere gange undervejs)
    [acp][in] {"id":3,"jsonrpc":"2.0","result":{"stopReason":"end_turn","usage":{"inputTokens":7973,"outputTokens":3,"totalTokens":7976}}}
    [acp] result id 3 stopReason "end_turn"

(Opstart i samme kørsel, jf. runde 5: initialize id 1 -> session/new id 2 -> `[acp] session ready: "229c1f2b-2dac-7fec-088f-be41f154b712"`; denne prompt-kørsel havde sessionId `015a3a3b-1c0f-de32-37d7-5a7b0562f51d` — én ny session pr. app-start.)

## Fund

| # | Fund | Markør |
|---|---|---|
| 1 | App-UI (AcpChatPage) -> `acpClient.sendPrompt` -> `session/prompt` id 3 -> svar `result` med `stopReason "end_turn"` | [M] 2026-11-02, telefonen |
| 2 | Svar-tekst ankom som `session/update` `agent_message_chunk` med `content.text = "OK"` og blev vist i UI via conversationModel (`addAssistantMessage` + `updateLastAssistantMessage`) | [M] |
| 3 | `usage` i prompt-result: inputTokens 7973, outputTokens 3, totalTokens 7976 — identisk med REPL-målingen samme dag (samme prompt, samme model) | [M] |
| 4 | `usage_update` sendes flere gange pr. turn (også med used=0 først) — L5-reglen (filtrer used==0, guard size<=0) gælder også her; size målt 800000 igen | [M] |
| 5 | `user_message_chunk` spejler brugerens prompt tilbage som update — UI må ikke vise den som ekstra besked (kun agent_message_chunk akkumuleres; det gør koden) | [M] |
| 6 | session/new-resultat indeholder `configOptions` (mode/model/thinking med currentValue) og `workspace_trust: untrusted` — relevant for set_config_option/A7 | [M] |
| 7 | Ingen `request_permission` ved denne runde (mode accept-edits, kun læsning) | [M] |

## Runde 6c: transport-fund (samme dag, REPL på telefonen)

- Agenten **kræver pipe/socket/char-device på stdin/stdout**: ved redirect af stdout til en almindelig fil (`> /tmp/va.out`) crasher den ved opstart med `ValueError: Pipe transport is only for pipes, sockets and character devices`, rc=1, ingen output. Derfor: stdout skal gennem et pipe (`| tee fil`). QProcess i appen bruger pipes — upåvirket. [M] 2026-11-02, telefonen
- Konsekvens: målescripts må ikke redirecte agentens stdout til fil; de skal pipe den (scriptet `maaling-ask-permission.py` håndterer det selv via subprocess.PIPE).
- **Anmodningens id er `0`:** `session/request_permission` kommer med `"id":0` (ikke sekventielt klient-id) — klienten skal svare på netop dette id. Første script-version læste anmodningen mens den ventede på et andet svar og fik den aldrig besvaret. [M] 2026-11-02, telefonen (`/tmp/acp-perm-wire.ndjson`: `"id":0,"method":"session/request_permission"`, toolCall title "Writing /tmp/acp-perm-test.txt")
- **Metodenavn målt:** agentens tilladelsesanmodning kommer som **`session/request_permission`** (namespaced) — ikke `request_permission`. Scriptet matchede først forkert og så aldrig anmodningen, hvorfor agenten hang til watchdog. [M] 2026-11-02, telefonen (`/tmp/acp-perm-wire.ndjson`, 21 frames, därav 1 `session/request_permission`, 13 `session/update`)
- Scriptet `maaling-ask-permission.py` (i dette repo) kører hele ask-mode-kæden automatisk og besvarer request_permission med "cancelled"; rå wire i `/tmp/acp-perm-wire.ndjson`. Hentes på telefonen med én curl-linje (ingen lang kode at kopiere).

## Runde 6d: ask-mode komplet målt (maaling-ask-permission.py, 2026-11-02, telefonen)

Kørsel: `curl -sfL <raw-url på denne gren>/maaling-ask-permission.py -o /tmp/perm.py && python3 /tmp/perm.py`. Output verbatim:

    SID: 7974165b-9762-e2b5-d11b-510971a9368e
    CFG: {"jsonrpc": "2.0", "id": 4, "result": {"configOptions": [{"currentValue": "ask", "options": [{"value": "ask", "name": "Ask", "description": ...}]...   (svaret erstatter hele configOptions-listen — jf. vedtaget om set_config_option)
    PERM_REQ id=0 keys=['options', 'sessionId', 'toolCall'] tool=None options=['allow_once', 'allow_session_exact', 'allow_permanent_exact', 'reject_once']
    PERM_REQ id=1 keys=['options', 'sessionId', 'toolCall'] tool=None options=['allow_once', 'allow_session_exact', 'allow_permanent_exact', 'reject_once']
    RESULT: {"jsonrpc": "2.0", "id": 5, "result": {"stopReason": "end_turn", "usage": {"totalTokens": 24493, "inputTokens": 24208, "outputTokens": 285, "cachedReadTokens": 18432}}}
    answered_permission: True
    file_exists: False
    exit: 0

Fund:

- `session/set_config_option` mode=ask accepteres; svaret er `result.configOptions` (hele listen, currentValue "ask") [M]
- Ved skrivende prompt i ask-mode sender agenten **to** `session/request_permission`-anmodninger (id 0 og id 1 — ikke sekventielle klient-id'er); params har nøglerne `options`, `sessionId`, `toolCall`; options er målt: `allow_once`, `allow_session_exact`, `allow_permanent_exact`, `reject_once` [M]
- Besvares anmodningen med `{"outcome":{"outcome":"cancelled"}}`, bliver filen IKKE oprettet (`file_exists: False`) og prompt ender `stopReason: "end_turn"` [M]
- Efter stdin-luk: `exit: 0` også med besvaret permission [M]
- **App-kode konsekvens (rettet på PR #19 samme dag):** routing i `harbour-skrynja.cpp` matcher nu både `session/request_permission` og `request_permission`, logger tool/options og auto-svarer `cancelled` indtil en dialog findes; `AcpClient::sendResult(id, result)` skriver svarrammen (id 0 målt). [M]+kode
- Script-fund (ikke agent): watchdog-`threading.Timer` uden daemon=True holdt python-processen ~90 s i live efter `exit: 0` — rettet i scriptet (daemon + cancel)

## Runde 6e: grid-start fejlede, terminal-start virkede — årsag: X-Sailjail i desktop-filen

Symptom 2026-11-02: app fra grid viste "Venter paa agent..." (sessionId aldrig sat), samme byg fra terminalen fik `[acp] session ready`. Årsag: `harbour-skrynja.desktop` havde stadig en `[X-Sailjail] Permissions=...`-sektion, så grid-start kørte appen indespærret — i strid med punkt 122 (lukket [B] storefyr 2026-10-31, spec v3 §9.2e: appen kører IKKE indespærret). Rettet på PR #19: sektionen fjernet fra desktop-filen; `setOrganizationName` i koden er uændret, så config-stien bliver. [M]+kode

## Runde 6b: tool-kald målt gennem UI (samme byg, samme dag)

Prompt i AcpChatPage: "Kør ls i /home/defaultuser og fortæl hvad der ligger der" — agenten svarede med mappens indhold (vist i UI; kopiering fra UI ikke muligt endnu, wire-log er kilden). Udtræk af `/tmp/skrynja-ui2.log` (93 linjer, 17 `[acp]`-linjer, sessionId `1611d73e-...`):

    36 sessionUpdate":"agent_message_chunk"
     2 sessionUpdate":"agent_thought_chunk"
     1 sessionUpdate":"available_commands_update"
     2 sessionUpdate":"session_info_update"
     1 sessionUpdate":"tool_call"
     1 sessionUpdate":"tool_call_update"
     7 sessionUpdate":"usage_update"
     1 sessionUpdate":"user_message_chunk"
    request_permission: 0   stopReason: "end_turn"

Fund: læsning via tool blev auto-approvet i accept-edits (ingen request_permission); `agent_thought_chunk` findes som egen update-type (ikke vist i UI endnu); kun `agent_message_chunk` akkumuleres til boblen — tool_call vises kun i loggen (alpha-begrænsning).

## Ikke målt endnu

- request_permission ved mode ask eller risikabelt kald (set_config_option mål for mode måles først)
- session/load ved genåbning af app (loadSession: true annonceret)
- billede i prompt (image: true annonceret)
- kopiering af tekst fra dialogen i UI (ikke muligt endnu — wire-log bruges)
