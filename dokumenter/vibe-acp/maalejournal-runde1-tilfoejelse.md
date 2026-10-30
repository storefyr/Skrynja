# Målejournal — tilføjelse: M0.16 forberedelse + M0.1/M0.15 afslutning

**Dato:** 26.10.2026 · **Lab:** `/workspace/malelab/`

---

## M0.15 — afsluttende vurdering (L1) ●

| Spørgsmål | Svar |
|---|---|
| Kan Skrynja bygges med SDK 3.5.0 + schema 1.12.0? | **Ja** — alle feature-kombinationer bygger (default, no-default, `unstable`, `unstable_protocol_v2`, `process`+`stdio`) |
| Skal Skrynja bruge `process`/`stdio` features for stdio-transport? | Ja — de ligger bag SDK'ets egne features (A-4 bekræftet i praksis) |
| Er v1 og v2 adskilte? | Ja — `schema::v1::*` vs `schema::v2::*`, ingen fælles typer; v2 kræver `unstable_protocol_v2` |
| Matcher vejkortets A-2 paraply-afgrænsning? | Ja — `unstable` tænder 7 flag, `unstable_protocol_v2` og `unstable_mcp_over_acp` ligger udenfor |

**M0.15: UDFØRT ●** — Skrynja kan bygges med den nødvendige funktionsprofil. A-4 (SDK-funktion) og A-2 (modulflag) er målt som uafhængige akser.

---

## M0.1 — afsluttende vurdering (L3) ●

| Spørgsmål | Svar |
|---|---|
| Protokolversion i `initialize`-svar | **`protocolVersion: 1`** — agenten forhandler **kun v1** |
| Afviser agenten `protocolVersion: 99`? | Nej — den svarer igen med samme capabilities (ikke en hård afvisning) ◐ — bør genmåles præcist for `protocolVersion`-feltet i svaret |
| Python SDK's `PROTOCOL_VERSION` | `1` (`acp/meta.py:29`) — kilden til agentens version |

**Konsekvens for vejkortet:** Forbindelsen til denne agent er **låst til v1**. Alle v2-specifikke måleporte (M0.4's toleddede regel i v2, E1–E3, G3, G6) kan **ikke** måles mod denne agent — de kræver en agent der forhandler v2. Dette besvarer vejkortets åbne forbehold #9 delvist: **vibe-acp 2.9.4 forhandler ikke v1-vs-v2; den er hardcoded til v1.**

**M0.1: UDFØRT ●** — agenten svarer `protocolVersion: 1`.

---

## M0.16 — forberedelse: kan `agent-client-protocol-http` indsættes foran en ekstern proces? (L2) ◐

Kildeanalyse af `agent-client-protocol-http 3.5.0`:

| Aspekt | Fund |
|---|---|
| `AcpHttpServer::new(factory)` | Tager en **factory** `Fn() -> C where C: ConnectTo<Client>` — dvs. hver HTTP-forbindelse får en ny forbindelse til en agent-komponent |
| `ConnectTo<Client>` implementeringer | SDK'et tilbyder `AcpAgent` (i `acp_agent.rs`) som `ConnectTo<Client>` — **men den spawner selv processen** via `AcpAgentConfig::new(command)` + `spawn_process()` (`Command::new`, stdin/stdout piped) |
| Kan broen tilsluttes en **ekstern** proces (ikke selv-startet)? | **Ikke direkte via `AcpAgent`** — den ejer processen. Men `ConnectTo` er et trait; en Skrynja-bro kunne implementere `ConnectTo<Client>` over en eksisterende stdio/fd. Kræver egen implementering ◐ |
| HTTP-overflade | `POST` (JSON-RPC request), `GET` (SSE stream), `DELETE` (close) på én path; `/health` valgfrit; CORS default **fra** (kan slås til) |
| WebSocket | separat modul (`websocket_server.rs`), valgfri opgradering |
| Sessionsmapping | `ConnectionRegistry` — hver HTTP-forbindelse mappes til én agent-forbindelse; **ingen** session-multiplexing over flere agent-processer i broen selv ◐ |

**Foreløbig konklusion M0.16 ◐:** Broen kan køre foran en proces den selv starter (Rust-klient → HTTP → Rust-bro → stdio-agent). For at indsætte den foran en **vilkårlig ekstern** stdio-agent (som Skrynja ville gøre med vibe-acp) kræves enten (a) at broen selv starter agenten (ikke "foran en eksisterende"), eller (b) en egen `ConnectTo`-implementering der forbinder til eksisterende stdio. Dette matcher vejkortets R3.

**M0.16 er IKKE udført** — kræver et faktisk indsættelsesforsøg (byg en bro-binær, kør den foran `vibe-acp`, mål gennem HTTP).

---

## Blokade-status efter denne runde

| Port | Status | Kommentar |
|---|---|---|
| M0.0 | ● udført | vibe-acp 2.9.4, Apache-2.0; **kræver shim for mcp 2.3.0** |
| M0.15 | ● udført | Skrynja kan bygges; v1+v2 adskilte; A-2/A-4 målt |
| M0.1 | ● udført | agenten svarer `protocolVersion: 1` — **kun v1** |
| M0.14 | ◐ delvist | fejlkoder målt: -32601/-32602/-32000; **ikke** -32022/-32800 som vejkortet antog † |
| M0.2–M0.8, M0.11 | ○ blokeret | ingen `MISTRAL_API_KEY` på målmaskinen → ingen session-trafik |
| M0.9, M0.10, M0.12, M0.13, M0.16 | ○ ikke udført | afventer nøgle / bro-forsøg |

## Anbefaling til næste runde

1. **Skaffe `MISTRAL_API_KEY`** — uden den kan halvdelen af måleportene ikke køre.
2. **Genmål M0.1 præcist** med `protocolVersion: 99` og log **hele** svaret — afklar om agenten afviser eller akkommoderer.
3. **M0.16 indsættelsesforsøg**: byg en minimal Rust-bro (`AcpHttpServer` + `AcpAgent::new("vibe-acp")`) og mål HTTP→stdio roundtrip lokalt (loopback).
4. **Rev. 14 af vejkortet** bør indarbejde: version 2.9.4, kun-v1, fejlkoder, NDJSON-ramme, shim-krav, Python SDK 0.9.0 vs Rust 1.12.0.
