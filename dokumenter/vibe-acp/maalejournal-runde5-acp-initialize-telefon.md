# Målejournal runde 5: ACP initialize-håndtryk målt fra appen på telefonen

**Maskine:** telefonen (Jolla Phone 2026, aarch64). **Dato:** 2026-11-02, efter CI-run 41438556360 (opdateret 11:41 UTC). **Byg:** PR #19, gren `vibe/acp-alpha-ea9aca` @ `afe0df9` (committeret 2026-11-02T11:39:30Z), installeret på telefonen med `sh test-pr.sh 19` fra grenen `pr-19-rpm` (merge-commit `bb12ba9`, `skrynja-pr19.rpm` 314 641 byte).

## Metode

Appen startet fra terminalen med uddata fanget i fil, lukket igen efter få sekunder:

    harbour-skrynja 2>&1 | tee /tmp/skrynja-start.log
    grep '\[acp\]' /tmp/skrynja-start.log

Logfacon: `qWarning` skriver med prefix `[W] unknown:0 -` og citationstegn omkring strengen — det er Qt's debugformat for QString, ikke en del af framen. På wire er hver ramme kompakt JSON + præcis ét 0x0A (v3 §2.1). Terminalens linjebrydninger i udskriften nedenfor er kun skærmvisning.

## Rå log (verbatim fra telefonen)

    [W] unknown:0 - [acp] agent started
    [W] unknown:0 - [acp][out] "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"clientCapabilities\":{},\"clientInfo\":{\"name\":\"Skrynja\",\"version\":\"0.9.10\"},\"protocolVersion\":1}}\n"
    [W] unknown:0 - [acp] sent initialize, id 1
    [W] unknown:0 - [acp][in] "{\"id\":1,\"jsonrpc\":\"2.0\",\"result\":{\"agentCapabilities\":{\"loadSession\":true,\"promptCapabilities\":{\"audio\":false,\"embeddedContext\":true,\"image\":true},\"sessionCapabilities\":{\"close\":{},\"fork\":{},\"list\":{}}},\"agentInfo\":{\"name\":\"@mistralai/mistral-vibe\",\"title\":\"Mistral Vibe\",\"version\":\"3.0.2\"},\"authMethods\":[{\"description\":\"Sign into Mistral Vibe through your Mistral AI Studio account.\",\"id\":\"browser-auth\",\"name\":\"Sign in through Mistral AI Studio\"}],\"protocolVersion\":1}}"
    [W] unknown:0 - [acp] agent finished, exitCode 0 status 0

## Fund

| # | Fund | Markør |
|---|---|---|
| 1 | Spawn af `/home/defaultuser/.local/bin/vibe-acp` lykkedes: `[acp] agent started` | [M] 2026-11-02, telefonen |
| 2 | Udgående initialize: `id 1`, `protocolVersion 1`, `clientCapabilities {}`, `clientInfo {name: "Skrynja", version: "0.9.10"}`. Versionen "0.9.10" kommer fra spec-filens `VERSION=%{version}` (ikke fallback "0.9.8" i koden) | [M] |
| 3 | Indgående svar er `result` (ingen `error`) — håndtrykket blev accepteret | [M] |
| 4 | Agentens `protocolVersion` i svaret er **1** — samme som klientens; ingen versionsmismatch. (M0.1b på målmaskinen viste at agenten aldrig afviser et andet tal; her svarer den 1) | [M] |
| 5 | `agentInfo`: name `@mistralai/mistral-vibe`, title `Mistral Vibe`, **version 3.0.2** — punkt 124's binding (agent >= 3.0.2, afvigelse = log + fortsæt, besluttet 2026-11-02) er målt **opfyldt** på telefonen. Lukning af punktet er beslutning hos storefyr | [M] |
| 6 | `agentCapabilities`: `loadSession: true`; `promptCapabilities {audio: false, embeddedContext: true, image: true}`; `sessionCapabilities {close: {}, fork: {}, list: {}}` — svarer til det annoncerede i A8-korrigeret (`{close, list, fork}`) | [M] |
| 7 | `authMethods`: præcis én, `id "browser-auth"` ("Sign in through Mistral AI Studio") | [M] |
| 8 | Ved app-lukning: `[acp] agent finished, exitCode 0 status 0` — `NormalExit` efter `closeWriteChannel` i `~AcpClient()`; **ingen** 1003-hale i loggen, dvs. alle agentens rammer var 0x0A-terminerede | [M] |
| 9 | Ingen P1/P2/L1-hændelser (ingen koder 2001/3001/1001 i loggen) under håndtrykket | [M] |

## Ikke målt endnu

- `session/new` + første `session/prompt` (næste skridt i klienten)
- versionsafvigelses-betingelsen (punkt 123/124) — ikke udløst, agenten er 3.0.2
- store svar / `session/update`-strømme over wire (tool_call, permission) — kræver en aktiv session
- beteende ved agent-crash midt i linje (4002 + hale) — kræver fremkaldt fejlsituation
