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

## Ikke målt endnu

- tool_call / tool_call_update gennem UI (kræver et prompt der får agenten til at køre et værktøj)
- request_permission-dialog (kræver mode ask eller et risikabelt kald)
- session/load ved genåbning af app (loadSession: true annonceret)
- billede i prompt (image: true annonceret)
