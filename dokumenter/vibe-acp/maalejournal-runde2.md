# Målejournal — Vibe-ACP vejkort Rev. 13, anden målerunde

**Dato:** 26.10.2026 · **Målmaskine:** denne sandbox (loopback) · **Målelab:** `/workspace/malelab/`
**Mærkning følger vejkort §0:** ● målt · ○ ikke målt · ◐ slutning fra målt materiale · † arvet/uverificeret

Denne runde dækker tre porte der kan lukkes uden API-nøgle: **M0.0b (integritet), M0.1b (fuld frame), M0.16 (broen)** — plus en versionsaudit af PyPI.

---

## Versionsaudit (grundlag for alle målinger) ●

| Pakke | Installeret i sandbox | Krævet af mistral-vibe 2.9.4 | **Nyeste på PyPI** | Afvigelse |
|---|---|---|---|---|
| `mistral-vibe` | 2.9.4 | — | **3.0.2** (2.9.6 næstnyeste) | målmaskinen kører **én major-version bagud** |
| `agent-client-protocol` (Python) | 0.9.0 | `==0.9.0` ✓ | **0.12.1** (1.0.0rc3 findes) | 3 minor-versioner bagud |
| `mcp` | 2.3.0 | `>=1.14.0` ✓ | nyere findes | OK ifølge krav, men inkompatibel API |
| `mistralai` | 3.2.0 | `==2.4.4` ✗ | 3.5.0 | **hård konflikt** (pip check) |

**Nyeste mistral-vibe 3.0.2 kræver:** `agent-client-protocol==0.11.0`, `mcp==1.28.1`, `mistralai==2.6.0`.

**Konsekvens (vigtig):** Alle protokoladfærdsmålinger i denne journal gælder **vibe-acp 2.9.4 + Python ACP SDK 0.9.0** — ikke den aktuelle udgivelse 3.0.2 med SDK 0.11.0. Vejkortets §3 versionsgrundlag skal udvides med en kolonne "nyeste udgivelse" og en advarsel om at målmaskinen kører en forældet agent.

---

## M0.0b — Integritet: `pip check` + krav-sammenholdning (L3-miljø) ●

`pip check` output:
```
mistral-vibe 2.9.4 has requirement mistralai==2.4.4, but you have mistralai 3.2.0.
```

| Konflikt | Analyse |
|---|---|
| **`mistralai 3.2.0` vs krævet `==2.4.4`** | Hård pin-brud. `mistralai` 3.x har haft breaking changes — dette er sandsynligvis årsagen til at dele af vibe kører, men ACP-delen crasher ved import. **Delvis forklaring på shim-behovet** ◐ |
| **`mcp 2.3.0` vs kode skrevet mod ældre API** | To inkompatible import-fejl (`streamablehttp_client` → `streamable_http_client`; `RequestContext` fjernet). Kravet er `mcp>=1.14.0` — 2.3.0 opfylder kravet formelt, men vibe 2.9.4's kode er ikke kompatibel med mcp 2.x API. **Dette er den direkte årsag til shim'en** ● |

**Forventet udfald fra planen var "to konflikter, hvoraf den ene forklarer shim'en fuldstændigt"** — bekræftet: mcp-konflikten forklarer shim'en; mistralai-konflikten er en separat latent risiko (kan bryde ved runtime, ikke ved import).

**Ekstra fund:** `mistralai-vibe-sdk 0.14.2` er også installeret — en separat pakke fra `mistralai`, ikke en afhængighed af mistral-vibe.

---

## M0.1b — Fuld frame: `initialize` med `protocolVersion: 99, 2, 0` (L3) ●

Metode: stdio NDJSON, tre separate `initialize`-anmodninger med `protocolVersion` 99, 2 og 0. Hele svaret logget verbatim.

| Anmodet version | Svar `protocolVersion` | Svar `error`? | Svar keys |
|---|---|---|---|
| **99** | **`1`** | nej | `agentCapabilities, agentInfo, authMethods, protocolVersion` |
| **2** | **`1`** | nej | samme |
| **0** | **`1`** | nej | samme |

**Verbatim svar (id=1, protocolVersion: 99 anmodet):**
```json
{"jsonrpc":"2.0","id":1,"result":{"agentCapabilities":{"loadSession":true,"promptCapabilities":{"audio":false,"embeddedContext":true,"image":false},"sessionCapabilities":{"close":{},"fork":{},"list":{}}},"agentInfo":{"name":"@mistralai/mistral-vibe","title":"Mistral Vibe","version":"2.9.4"},"authMethods":[],"protocolVersion":1}}
```

**Konklusioner:**
1. Agenten **afviser ikke** en ukendt/fremtidig version — den svarer altid med sin egen `protocolVersion: 1` ●
2. Svaret **indeholder altid** `protocolVersion`-feltet ●
3. Der er **ingen fejl ved version-mismatch** — ingen `-32xxx` error ●
4. **K1 kan IKKE håndhæves alene ved at afvise agenten** — men den **kan håndhæves klientside**: klienten skal sammenligne svarets `protocolVersion` med den ønskede og **selv disconnecte**, præcis som skemaet beskriver ("The client should disconnect, if it doesn't support this version"). Agenten gør det ikke for klienten ◐
5. Agenten ignorerer klientens `protocolVersion` fuldstændigt — den kender kun v1 ●

---

## M0.16 — Broen: `AcpHttpServer` + `AcpAgent::new("vibe-acp")`, loopback roundtrip (L2) ● (halvdelen)

### Opbygning

| Komponent | Kilde |
|---|---|
| Bro-binær (`bro`) | `AcpHttpServer::new(|| AcpAgent::new(AcpAgentConfig::new("/usr/local/bin/vibe-acp").env("PYTHONPATH", "/workspace/malelab")))` |
| Klient-binær (`klient`) | `HttpClient::new("http://127.0.0.1:18923/acp")` + `Client::default().builder().name("skrynja-malelab").connect_with(...)` |
| Klient-anmodning | `InitializeRequest::new(ProtocolVersion::V1)` |
| Projekt | `/workspace/malelab/m016-bro/` (bygger med begge binære) |

### Måleresultat (loopback roundtrip)

```
[klient] forbinder til http://127.0.0.1:18923/acp
[klient] initialize OK:
[klient]   protocol_version = ProtocolVersion(1)
[klient]   agent_info = Some(Implementation { name: "@mistralai/mistral-vibe", title: Some("Mistral Vibe"), version: "2.9.4", .. })
[klient]   agent_capabilities = AgentCapabilities { load_session: true, .. session_capabilities: { list, close, .. } }
[klient] roundtrip gennem bro fuldført
```

**Roundtrip: HTTP-klient → bro (axum) → spawnet `vibe-acp` over stdio → svar tilbage. VIRKER ●**

### Hvad M0.16 besvarer — og hvad ikke

| Spørgsmål | Svar |
|---|---|
| Kan broen **starte** agenten selv (spawn proces)? | **Ja** ● — `AcpAgent` spawner `vibe-acp` med shim-miljø |
| Kan broen **sættes foran en fremmed proces** (som Skrynja ville gøre — telefonen har agenten kørende separat)? | **Ikke målt** ○ — `AcpHttpServer::new` kræver `C: ConnectTo<Client>`; `AcpAgent` er én implementering der spawner. En fremmed proces kræver en egen `ConnectTo`-implementering over eksisterende stdio ◐ |
| Virker SSE-streaming (GET) gennem broen? | Ikke målt i denne runde ○ |
| Virker `$/cancel_request` gennem broen (K12)? | Ikke målt ○ (kræver M0.13) |

**M0.16: HALVT UDFØRT ◐** — "broen kan starte agenten" er bekræftet. "Broen foran en fremmed proces" er åben og kræver enten (a) at Skrynja selv spawner agenten (hvilket er realistisk på Sailfish — telefonen kører agenten lokalt), eller (b) en custom `ConnectTo` over en eksisterende proces' stdio.

**Praktisk konsekvens for Skrynja (L1):** Da Skrynja er en Sailfish-app der vil køre `vibe-acp` som lokal underproces, er "broen spawner agenten" faktisk den **relevante** halvdel for Trin B — men Skrynja er C++/Qt, ikke Rust, så Rust-broen er kun en reference-implementering, ikke produktionskoden.

---

## Samlet status efter runde 2

| Port | Status | Kommentar |
|---|---|---|
| M0.0 | ● | vibe-acp 2.9.4, men **3.0.2 er nyeste** — målmaskine er forældet |
| M0.0b | ● | pip check: mistralai-konflikt; mcp 2.3.0 inkompatibel API = shim-årsag |
| M0.1 | ● | svarer `protocolVersion: 1` |
| M0.1b | ● | afviser ikke version 99/2/0; svarer altid 1; **K1 håndhæves klientside** |
| M0.15 | ● | Skrynja bygger med alle feature-profiler |
| M0.16 | ◐ halvt | bro spawner agent og roundtrip virker; "fremmed proces" åben |
| M0.14 | ◐ delvist | fejlkoder -32601/-32602/-32000 målt |
| M0.2–M0.8, M0.11 | ○ blokeret | mangler MISTRAL_API_KEY |
| M0.9, M0.10, M0.12, M0.13 | ○ ikke udført | afventer nøgle |

## Anbefalinger til Rev. 14 af vejkortet

1. **Tilføj "nyeste udgivelse"-kolonne til §3** — målmaskinen kører 2.9.4, nyeste er 3.0.2; alle adfærdsmålinger er versionsspecifikke.
2. **K1 præciseres:** agenten afviser ikke version-mismatch; klienten må selv disconnecte når svarets `protocolVersion` ikke matcher. Skriv det ind som en klientregel.
3. **M0.16 deles op:** M0.16a (bro spawner agent — ● udført) og M0.16b (bro foran fremmed proces — ○ åben).
4. **Shim-årsag dokumenteres:** mcp 2.3.0 vs vibe 2.9.4-kode; overvej at pinne `mcp<2` på målmaskinen i stedet for shim.
5. **mistralai-konflikten** bør løses (nedgrader til 2.4.4 eller opgrader vibe) — latent runtime-risiko.
