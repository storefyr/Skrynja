# Målejournal — Vibe-ACP vejkort Rev. 13, første målerunde

**Dato:** 26.10.2026 · **Målmaskine:** denne sandbox (loopback) · **Målelab:** `/workspace/malelab/`
**Mærkning følger vejkort §0:** ● målt · ○ ikke målt · ◐ slutning fra målt materiale · † arvet/uverificeret

---

## M0.0 — Findes `vibe-acp` på målmaskinen, version, licens? (L3) ●

| Observation | Værdi |
|---|---|
| Binær | `/usr/local/bin/vibe-acp` (og `/usr/local/bin/vibe`) |
| `vibe-acp --version` | `vibe-acp 2.9.4` |
| Pakke | `mistral-vibe 2.9.4`, **Apache-2.0**, site-packages `/usr/local/lib/python3.12/site-packages` |
| Python ACP SDK | `agent-client-protocol 0.9.0` (PyPI), **Apache-2.0** (LICENSE i dist-info) |
| MCP SDK | `mcp 2.3.0`, MIT |
| Andre | `mistralai 3.2.0`, `mistralai-vibe-sdk 0.14.2` |

**Afvigelse fra vejkort §3:** Vejkortet havde "v2.26.1 senest observeret" — målmaskinen kører **2.9.4**. Versionen i vejkortets §3 skal revideres.

**Kritisk fund (R1 realiseret):** `vibe-acp` **crasher ved opstart uden shim** — to inkompatible `mcp`-API'er:
1. `vibe/core/tools/mcp/tools.py:16` importerer `streamablehttp_client` (findes ikke i mcp 2.3.0; hedder `streamable_http_client`)
2. `vibe/core/tools/mcp_sampling.py:8` importerer `RequestContext` fra `mcp.shared.context` (fjernet i mcp 2.3.0)

Workaround i målelab: `sitecustomize.py` på `PYTHONPATH=/workspace/malelab` alias'er begge. **Agenten kører kun med denne shim.** Dette er en målmaskine-afhængig defekt, ikke en protokoladfærd — men det blokerer M0.2–M0.8 uden shim.

---

## M0.1 — Hvilken protokolversion svarer agenten på i `initialize`? (L3) ●

Metode: adfærdsmåling over stdio, newline-delimited JSON-RPC 2.0 (bekræfter også rammeformatet).

Sendt: `initialize` med `protocolVersion: 1`.

**Svar (verbatim):**
```json
{
  "agentCapabilities": {
    "loadSession": true,
    "promptCapabilities": {"audio": false, "embeddedContext": true, "image": false},
    "sessionCapabilities": {"close": {}, "fork": {}, "list": {}}
  },
  "agentInfo": {"name": "@mistralai/mistral-vibe", "title": "Mistral Vibe", "version": "2.9.4"},
  "authMethods": [],
  "protocolVersion": 1
}
```

| Påstand | Resultat |
|---|---|
| Agenten svarer `protocolVersion: 1` | ● bekræftet |
| `loadSession: true` (bool, ikke Option — matcher vejkort §5.2) | ● bekræftet |
| `sessionCapabilities` = `{close, fork, list}` — **bemærk: `fork` er til stede** selv uden annonceret flag | ● målt — afviger fra vejkortets §5.2 antagelse om syv felter uden fork-nævn; vibe annoncerer fork uden `unstable_session_fork` på klientsiden |
| `agentInfo.name` = `@mistralai/mistral-vibe` | ● |
| Ingen `providers` i agentCapabilities | ● (ikke annonceret — matcher K10/`unstable_llm_providers` gating) |
| Ingen plan-relaterede felter i agentCapabilities | ● bekræfter **K8** (planer annonceres kun af klienten) |

**Versionshåndtering ved `protocolVersion: 99`:** Agenten svarede igen med et fuldt `InitializeResponse` (samme capabilities) — **Bemærk:** svaret viste ikke et `protocolVersion`-felt i denne måling (afkortet log; bør genmåles). Vejkort K1 siger version låses pr. forbindelse; vibe ser ud til at svare, ikke afvise. ◐

---

## M0.15 — Kan Skrynja bygges med nødvendig SDK-funktionsprofil? (L1) ●

Rust toolchain: `cargo 1.99.0` til stede.

| Build-profil | Resultat |
|---|---|
| Alle tre crates pinnet `=1.12.0`/`=3.5.0`/`=3.5.0`, default features | ● bygger (25 s) |
| Samme, `default-features = false` (kun `schemars` på schema) | ● bygger |
| + `unstable` paraply på schema og SDK | ● bygger |
| + `unstable_protocol_v2` på schema og SDK | ● bygger |

**A-2/A-4 bekræftet ved kompilering:**
- `schema::v1::*` altid tilgængelig; `schema::v2::*` **kun** med `unstable_protocol_v2` (uden flag fejler `v2::` ikke kompilere) ●
- v1 og v2 er **adskilte typer** (`v1::agent::InitializeRequest` ≠ `v2::agent::InitializeRequest`) — bekræfter vejkort A-1 "ingen automatisk konvertering" ●
- `unstable` paraply tænder 7 tomme flag: `unstable_llm_providers`, `unstable_mcp_over_acp`, `unstable_plan_operations`, `unstable_session_fork`, `unstable_session_system_prompt`, `unstable_subagents`, `unstable_end_turn_token_usage` — **`unstable_protocol_v2` ligger UDEN for paraplyen** ● (matcher vejkort §4 præcis)
- SDK har **ingen default features** (`default = []`); `process`, `stdio`, `schemars`, `wasm_js` er separate tilvalg ● (matcher A-4)
- Schema default = `schemars` kun ●

**v1 vs v2 ToolCallUpdate (M0.3 forberedelse, målt i koden):**
- v1: `ToolCallUpdate { tool_call_id, fields: ToolCallUpdateFields, .. }` — **indlejret `fields`** ● (matcher vejkort §5.2)
- v2: `ToolCallUpdate { tool_call_id, name: MaybeUndefined<String>, title, kind, status, content, .. }` — **flade felter, alle `MaybeUndefined<T>`** ● (matcher §5.3 og K5)

---

## M0.14 (delvist) — Fejlkoder (L3) ◐

Fremprovokerede fejl (over stdio):

| Input | Kode målt | Besked | Vejkort påstand |
|---|---|---|---|
| Ukendt metode | `-32601` | `Method not found` | standard JSON-RPC ● |
| `initialize` uden `protocolVersion` | `-32602` | `Invalid params` + pydantic detaljer i `data.errors` | standard ● |
| `session/prompt` på ukendt session | `-32602` | `Session not found: nope` | **ikke** `-32022` |
| `session/new` uden `cwd` | `-32602` | `Invalid params` (`cwd` required) | — |
| `session/set_config_option` på ukendt session | `-32602` | `Session not found` | — |
| `session/new`/`session/load` uden API-nøgle | `-32000` | `Missing API key for mistral provider.` | — |
| Malformet JSON-linje | *(ingen svar observeret i vinduet)* | — | ◐ |

**Afvigelse fra vejkort †:** Vejkort M0.14 nævner `-32022`, `-32602`, `-32800` († arvet). Målt: agenten bruger **`-32601`, `-32602`, `-32000`** (JSON-RPC standard + server range) og **`-31001..-31004`** for applikationsfejl (rate limit, config, conversation limit, context too long — i koden, ikke målt i trafik). **`-32022` og `-32800` er ikke observeret.** Vejkortets fejlkodeliste bør revideres.

---

## Blokade for M0.2–M0.8, M0.11 (L3 trafikprotokollukker) ○

`session/new` og `session/load` fejler med `-32000 Missing API key` — **ingen `MISTRAL_API_KEY` i miljøet**, og config peger på `api_key_env_var = "MISTRAL_API_KEY"`. Uden nøgle kan der ikke køres en repræsentativ session, så følgende porte er **blokeret på målmaskinen**:

- M0.2 (SessionUpdate-varianter i trafik)
- M0.3 (indlejret vs flad tool_call_update i trafik — koden bekræfter forskellen, men agentens faktiske udsendelser er ikke målt)
- M0.4 (toleddet rettelsesregel)
- M0.5, M0.6, M0.7 (plan/notice i trafik)
- M0.11 (load_session semantik i trafik)

---

## Øvrige målinger (ikke i de 17 porte, men relevante)

| Observation | Værdi |
|---|---|
| Rammeformat | **newline-delimited JSON** (NDJSON), ikke Content-Length headers ● (`acp/connection.py:62`) |
| `SessionUpdate`-varianter i Python SDK 0.9.0 | **11** (agent_message_chunk, agent_thought_chunk, available_commands_update, config_option_update, current_mode_update, plan, session_info_update, tool_call, tool_call_update, usage_update, user_message_chunk) — afviger fra vejkortets v1=19/v2=24 (Rust-skema); Python SDK er ældre (0.9.0 vs Rust 1.12.0/3.5.0) ◐ |
| Agent-metoder (Python SDK meta) | 14: authenticate, initialize, session/cancel, session/close, session/fork, session/list, session/load, session/new, session/prompt, session/resume, session/set_config_option, session/set_mode, session/set_model |
| `session/resume` findes i agenten | ja (i meta) — vejkort nævner "genoptag" uden metode; dette er den sandsynlige kandidat ◐ |
| Python SDK `use_unstable_protocol` flag | eksisterer i router (`acp/router.py`) — metoder kan markeres unstable og afvises med advarsel hvis flaget er fra ● |
| `PROTOCOL_VERSION = 1` i Python SDK | ● (`acp/meta.py:29`) — **agenten understøtter kun v1** |

---

## Næste skridt (anbefaling)

1. **Skaffes en `MISTRAL_API_KEY`** (eller en mock agent) for at åbne M0.2–M0.8, M0.11.
2. **M0.3/M0.4 kan delvist måles uden nøgle** ved at bygge en minimal ACP-agent i Python der *simulerer* vibe-acp's tool_call-oprettelse — men det måler ikke L3, så det bør kun bruges som L1-referencetest.
3. **M0.16 (L2)** kan måles nu: kan `agent-client-protocol-http` crate bygges og køre foran en ekstern proces?
4. **Rev. 14 af vejkortet bør indarbejde:** version 2.9.4 (ikke 2.26.1), Python SDK 0.9.0 ≠ Rust 1.12.0, fejlkoder revideret, NDJSON-ramme bekræftet, shim-krav på målmaskinen.
