= Vibe-ACP vejkort rev 13

# Vejkort for Skrynja mod `vibe-acp` — Rev. 13

**Status:** erstatter Rev. 12. Indarbejder fund F15–F28 fra anden og tredje granskningsrunde samt de tre afklaringer (planunionen, transportcraten, agentens art).
**Dato:** 25.10.2026. **Næste revision:** efter måleport M0.1 og M0.15 er udført.

---

## 0. Læsevejledning

| Mærke | Betydning |
|---|---|
| **●** | Målt i denne eller foregående runde mod crate-metadata, skemadokumentation eller repository |
| **○** | Ikke målt. Ingen påstand i vejkortet må læses som bekræftet |
| **◐** | Slutning fra målt materiale. Angivet som slutning, ikke som kendsgerning |
| **†** | Arvet fra kilde, der ikke er genåbnet i denne runde. Uverificeret her |

Tre kilder bærer størstedelen af grundlaget: `agent-client-protocol-schema` 1.12.0, `agent-client-protocol` 3.5.0 og `agent-client-protocol-http` 3.5.0. Migreringsdokumentet (†) er **ikke** genåbnet; alle afsnit, der hviler på det, er markeret †.

---

## 1. Formål

Skrynja er en ACP-klient. Vejkortet fastlægger, hvilke protokolflader klienten skal implementere, i hvilken rækkefølge, mod hvilke versioner, og hvilke målinger der skal være udført, før en flade erklæres færdig. Vejkortet er ikke en implementeringsplan for agenten; det er en kontrakt- og måleplan for klienten.

---

## 2. Afgrænsning: tre lag, ikke ét

Granskningen har vist, at «agenten» er tre selvstændigt fejlende lag. Vejkortet fører dem hver for sig.

| Lag | Hvad det er | Hvad der kan afvige | Måles med |
|---|---|---|---|
| **L1 — Skrynjas protokollag** | Klientens typer og tilstandsmaskine | Skemaets gating, Skrynjas egen byggeprofil | Kompilering + enhedsprøver |
| **L2 — Broen** | Transport mellem klient og agent | Om `$/cancel_request` videresendes; om sessioner mappes korrekt | Måleport M0.16 |
| **L3 — `vibe-acp`** | Agenten selv | Protokolversion, faktisk udsendte felter, flagadfærd | Måleporte M0.0–M0.14 |

**L3 er ikke Rust.** `vibe-acp` er en Python-applikation i `mistralai/mistral-vibe` med `vibe-acp.spec` i roden ● [^2] [^3]. Den pakkes som eksekverbar sammen med `vibe` og `vibe-app-server` ● [^1]. Der findes **ingen cargo-funktionsflag** at aflæse i den. Enhver målemetode, der forudsætter `cargo metadata`, er uanvendelig på L3 (jf. F24).

---

## 3. Versionsgrundlag

| Størrelse | Version | Udgivet | Hentet | Status |
|---|---|---|---|---|
| `agent-client-protocol-schema` | 1.12.0 | 16.10.2026 | 24.–25.10.2026 | ● |
| `agent-client-protocol` (SDK) | 3.5.0 | 22.10.2026 | 24.–25.10.2026 | ● |
| `agent-client-protocol-http` | 3.5.0 | 22.10.2026 | 25.10.2026 | ● |
| SDK-forløber | 2.2.0 | 18.09.2026 | — | ● (forklarer versionsspringet) |
| `vibe-acp` | v2.26.1 senest observeret | — | 24.10.2026 | ● [^2] |

**Bindende regel:** SDK'et låser skemaet med `=1.12.0` ●, og transportcraten følger SDK'ets version ●. Et versionspin uden hentedato er ikke et pin — hver række ovenfor skal bære dato i alle fremtidige revisioner (jf. F15's ledsagefund: en URL er ikke en fast adresse, fordi wiki-indeksering kan skride).

**Governance:** `initialize` låser protokolversionen pr. forbindelse ●. To versioner kan ikke forhandles på samme forbindelse. v1 er fortsat den stabile vej: SDK'et beskriver `Proxy.builder()` som «the stable v1 entry point», mens v2 er udkast ●.

---

## 4. Fire akser plus én lagakse

| Akse | Indhold | Gælder for |
|---|---|---|
| **A-1 Protokolversion** | v1 / v2 — adskilte modulrum, ingen automatisk konvertering | L1 |
| **A-2 Modulflag** | `unstable` (paraply), `unstable_protocol_v2` (uden for paraplyen) | L1 |
| **A-3 Feltflag** | `unstable_plan_operations`, `unstable_llm_providers`, `unstable_mcp_over_acp` m.fl. | L1 |
| **A-4 SDK-funktion** | `process`, `stdio`, `schemars`, `wasm_js` — SDK'et har **ingen** standardfunktioner, og ustabile protokolfunktioner er uafhængige tilvalg ● | L1 |
| **A-5 Lag** | L1 / L2 / L3 — hvilket lag en måling vedrører | Alle |

A-4 er ny i Rev. 13 (F16). Den er nødvendig, fordi et programmel kan være bygget med `unstable_protocol_v2` i skemaet og alligevel mangle SDK'ets v2-indgang. A-5 er ny (F24/F25): en måling uden lagangivelse er ikke reproducerbar.

**Paraplyens afgrænsning ●:** `unstable` tænder syv flag, som alle er tomme — de virker udelukkende som kompileringstilstande. `unstable_protocol_v2` og `unstable_mcp_over_acp` står **uden for** paraplyen. Elleve flag i alt, ét standard (`schemars`).

---

## 5. Feltregister

Registret er den eneste sandhed om protokolfladen. Ti dimensioner pr. felt: modelnavn, ledningsnavn, sti, type, rettelsesregel, modulflag, feltflag, stabilitet, forpligtelse, SDK-funktion — plus kolonnen «L3 observeret» for agentens faktiske adfærd.

**Navnekonventionen skal skelne tre former** (F19): Rust-modelnavn (`config_options`, understregning), JSON-ledningsnavn (`configOptions`, camelCase) og sti (`session/update.update.configOptions`). Kolonnen «Felt» må ikke blande dem. ◐ At camelCase-konventionen gælder generelt er en slutning fra `config_options`/`configOptions` og `plan_id`/`planId`; den er ikke gennemprøvet felt for felt.

### 5.1 Versionsneutrale felter

| Model | Ledning | Type | Rettelsesregel | Stabilitet | Forpligtelse |
|---|---|---|---|---|---|
| `_meta` | `_meta` | objekt | nøglevis lapning på topniveau; `null` fjerner nøglen ● | stabil | valgfri |

`_meta`-reglen er målt i v2 ●; for v1 er den ○. Der må ikke antages symmetri.

### 5.2 v1 — udvalgte felter

| Model | Ledning | Sti | Type | Rettelsesregel | Flag | Stabilitet | Forpligtelse |
|---|---|---|---|---|---|---|---|
| `tool_call_id` | `toolCallId` | `…update.toolCallId` | String | — | — | stabil | påkrævet ● |
| `fields` | `fields` | `…update.fields` | `ToolCallUpdateFields` | **indlejret objekt**; hele objektet erstattes ◐ | — | stabil | påkrævet ● |
| `SessionUpdate::plan` | `plan` | `…update.plan` | `Plan` | erstattes | — | stabil | valgfri ● |
| `SessionUpdate::plan_update` | `planUpdate` | `…update.planUpdate` | `PlanUpdateContent` | erstattes | `unstable_plan_operations` | **UNSTABLE** | valgfri ● |
| `SessionUpdate::plan_removed` | `planRemoved` | `…update.planRemoved` | id | — | `unstable_plan_operations` | **UNSTABLE** | valgfri ● |
| `PlanUpdateContent` | — | — | enum, 3 varianter | — | **hele enumen** bag `unstable_plan_operations` | **UNSTABLE** | — ● |
| `AgentCapabilities::load_session` | `loadSession` | `agentCapabilities.loadSession` | `bool` (ikke `Option`) | — | — | stabil | påkrævet ● |
| `AgentCapabilities::session_capabilities` | `sessionCapabilities` | `agentCapabilities.sessionCapabilities` | struktur (ikke `Option`) | — | — | stabil | påkrævet ● |
| `AgentCapabilities::providers` | `providers` | `agentCapabilities.providers` | `Option` | udeladt/**`null`** = annoncerer ikke | `unstable_llm_providers` | **UNSTABLE** | valgfri ● |
| `ClientSessionCapabilities::compaction` | `compaction` | `clientCapabilities.sessionCapabilities.compaction` | objekt | `{}` = hele kontrakten; udeladt/**`null`** = ikke understøttet | — | stabil | valgfri ● |
| `ClientSessionCapabilities::config_options` | `configOptions` | `…sessionCapabilities.configOptions` | — | dækker **alle** indstillingsudvidelser | — | stabil | valgfri ● |
| `ClientSessionCapabilities::notices` | `notices` | `…sessionCapabilities.notices` | — | — | — | stabil | valgfri ● |

`AgentCapabilities` er `#[non_exhaustive]` og har syv felter: `load_session`, `prompt_capabilities`, `mcp_capabilities`, `session_capabilities`, `auth`, `providers`, `meta` ●.

### 5.3 v2 — udvalgte felter

| Model | Ledning | Type | Rettelsesregel | Flag | Stabilitet |
|---|---|---|---|---|---|
| `tool_call_update`-felter | flade, camelCase | `MaybeUndefined<T>` | se §6, K3 | `unstable_protocol_v2` | udkast ● |
| `SessionUpdate::plan_update` | `planUpdate` | `PlanUpdateContent` | erstattes | `unstable_protocol_v2` | stabil inden for v2 ● |
| `SessionUpdate::plan_removed` | `planRemoved` | id | — | + `unstable_plan_operations` | **UNSTABLE** ● |
| `SessionUpdate::notice` | `notice` | — | — | `unstable_protocol_v2` | kræver **ingen** klientevne ● |
| `SessionUpdate::other` | `other` | rå nyttelast | **bevares** ved lagring, replay og proxying ● | `unstable_protocol_v2` | udkast ● |
| `PlanUpdateContent::Other` | — | `OtherPlanUpdateContent` | **bevares** ved lagring, replay, proxying og videresendelse ● | `unstable_protocol_v2` | udkast ● |
| `ClientCapabilities` | — | tre felter, alle `Option` | udeladt = ikke annonceret | `unstable_protocol_v2` | udkast ● |
| `MaybeUndefined<T>` | — | fælles type i crate-roden | tretilstand: udeladt / `null` / værdi ● | ingen | **legitim undtagelse** fra forbuddet mod fælles typer |

### 5.4 Asymmetrier, der skal stå eksplicit

| Emne | v1 | v2 | Konsekvens |
|---|---|---|---|
| `SessionUpdate`-varianter | 19 ● | 24 (fjerner 3, tilføjer 8) ● | Ingen fælles model |
| `tool_call_update` | indlejret `fields` ● | fladt ● | Rettelseslag må ikke deles |
| Ukendt indhold | `other` findes ● | `other` findes ● | Symmetrisk |
| Ukendt **plan**indhold | **ingen `Other`** ● | `Other` findes ● | v1 fejler i parsningen ◐ (F22) |
| `PlanUpdateContent::Items` | bag `unstable_plan_operations` ● | ubetinget ● | `Items` er den **mindst** tilgængelige form i v1 |
| `File` / `Markdown` | bag samme flag ● | bag `unstable_plan_operations` ● | Samme flag, forskelligt udgangspunkt |
| `planId` | inde i indholdet ● | inde i indholdet ● | Byggemetoder: `items(plan_id, …)`, `file(plan_id, …)`, `markdown(plan_id, …)` ● |

---

## 6. Protokolkontrakt

**K1 — Version låses ved `initialize`.** Én version pr. forbindelse. Afvisning skal skelnes fra manglende v2-støtte; det kræver en selvstændig måling (M0.1), ikke en fortolkning af fejlkoden.

**K2 — `unstable` tænder ikke v2.** Paraplyflaget er ikke en genvej. `unstable_protocol_v2` skal sættes eksplicit.

**K3 — Rettelsesreglen for `tool_call_update` i v2 er toleddet (F17).**
- Kendt `tool_call_id`: udeladt felt = uændret; `null` = ryd; konkret værdi = erstat.
- **Ukendt `tool_call_id`: udeladte felter får klientstandarder**; for `name` betyder udeladelse eller `null`, at intet værktøjsnavn er tilgængeligt ●.

Da v2 opretter værktøjskaldet ved første `tool_call_update`, er det ukendte id den hyppigste situation i laget. Reglen må ikke forenkles til «udeladelse = uændret».

**K4 — Samlinger erstattes i v2.** `null` og `[]` rydder ●. Der findes ingen fletning af lister.

**K5 — `null` bevares som `null` gennem `apply_update`** ●. Tretilstanden må ikke kollapse til to.

**K6 — `_meta` patches nøglevis på topniveau i v2** ●. `null` fjerner nøglen. Ingen rekursiv fletning.

**K7 — Ukendt indhold bevares, hvor `other` findes.** Gælder `SessionUpdate` i begge versioner og `PlanUpdateContent` **kun i v2** ●. For v1-planindhold findes ingen catch-all ●; konsekvensen for parsningen er ◐ (F22) og skal måles (M0.8).

**K8 — Planer annonceres kun af klienten.** `AgentCapabilities` har intet planfelt ●. Om agenten vil sende planopdateringer, kan kun observeres (F27).

**K9 — MCP over ACP kræver `unstable_mcp_over_acp`,** både for at vedhæfte en MCP-server over ACP ● og — efter beslutning — for Skrynjas egen byggeprofil (F28).

**K10 — `compaction`, `configOptions` og `notices` er klientens kontrakt.** `{}` betyder «hele kontrakten»; udeladelse eller `null` betyder «ikke understøttet» ●. `configOptions` er paraplyen for alle indstillingsudvidelser ●.

**K11 — Transportvalget er ikke truffet.** Tre kandidater, se Trin B. `agent-client-protocol-http` er førsteparts (Apache-2.0, samme ejere som SDK'et) ● og leverer `AcpHttpServer` (axum, HTTP+SSE med valgfri WebSocket-opgradering) og `HttpClient`/`HttpClientBuilder` (reqwest, eventsource) ●. Klientsiden er Rust og dermed **ikke anvendelig for Skrynja**; kun brokersiden er relevant.

**K12 — Annullering er betinget af et flag på L2.** Uden `unstable_cancel_request` videresendes `$/cancel_request` ikke gennem transporten ●. Kravene til afbrydelse må derfor føres pr. lag.

---

## 7. Milepæle

Hver milepæl har leverance, afhængighed og tilhørende måleport. Intet trin erklæres færdigt uden udført måleport.

### Trin A — Grundlag og beslutninger

| # | Leverance | Afhængighed | Måleport |
|---|---|---|---|
| A1 | Versionspin med hentedato for alle tre crates (§3) | — | — |
| A2 | **Transportvalg med tre kandidater** (K11): direkte stdio; `agent-client-protocol-http` som bro; egen bro. Beslutningen træffes på ny med kendskab til førstepartscraten | A1 | M0.16 |
| A3 | Fire akser + lagakse indført i alle tabeller | A1 | — |
| A4 | Protokolkontrakten K1–K12 som selvstændigt dokument | A3 | — |
| A5 | Kilderegister med hentedato og genåbningsstatus | — | — |
| A6 | Måleprotokol: hvordan L3 måles adfærdsmæssigt | A2 | M0.0 |
| A7 | Sprog- og kørselsvalg for Skrynja, herunder om en WS-transport findes i det valgte sprog | A2 | — |
| A8 | Paraplyens afgrænsning skrevet ind i konfigurationsdokumentationen | A3 | — |
| A9 | Forbud mod fælles typer mellem v1 og v2 — med `MaybeUndefined` som navngivet undtagelse (F20) | A3 | — |
| A10 | Tre-formers navnekonvention (model / ledning / sti) | A3 | — |
| A11 | Rettelseslaget som toleddet funktion (K3), ikke som tabelopslag | A4 | — |
| A12 | Feltregister som eneste sandhed, inkl. SDK-funktionskolonne | A3, A10 | — |
| A13 | Fire akser + lag ført ind i granskningsskemaet | A3 | — |

### Trin B — Transport

| # | Leverance | Afhængighed | Måleport |
|---|---|---|---|
| B1 | JSON-RPC 2.0-ramme over stdio mod underproces ● | A2, A7 | M0.1 |
| B2 | `initialize` med version, klient Evner og sessionskontrakt (K10) | B1 | M0.1, M0.10 |
| B3 | Underproceslivscyklus: opstart, oprydning, fejltilstande | B1 | M0.0 |
| B4 | Afbrydelse: `$/cancel_request` og `session/cancel`, ført pr. lag (K12) | B2 | M0.13 |
| B5 | Bro, hvis A2 vælger den: CORS (fra som standard ●), sessionsmapping, videregivelse af annullering | A2, B4 | M0.16 |
| B6 | WebSocket kun hvis A2 kræver det og A7 kan levere det | B5 | — |

### Trin C — Sessionslivscyklus †

Opret, indlæs (`load_session` — `bool`, ikke `Option` ●), genoptag, luk. `state_update`, `replayFrom` og tilstandshåndtering hviler på migreringsdokumentet og er **uverificeret i denne runde**.

### Trin D — Værktøjskald og rettelseslag

| # | Leverance | Afhængighed | Måleport |
|---|---|---|---|
| D1 | v1: indlejret `fields`, hele objektet erstattes | A11 | M0.3 |
| D2 | v2: flade felter med tretilstand og **toleddet** regel (K3) | A11 | M0.4 |
| D3 | `null` bevares gennem `apply_update` (K5) | D2 | — |
| D4 | `_meta` nøglevis lapning (K6) | D2 | — |
| D5 | `From<ToolCallUpdate> for RequestPermissionSubject` ● som tilladelsesbro | D2 | — |
| D6 | v1's `TryFrom<ToolCallUpdate> for ToolCall` ● | D1 | — |

### Trin E — Planer

| # | Leverance | Afhængighed | Måleport |
|---|---|---|---|
| E1 | v2: fire varianter — `Items`, `File`, `Markdown`, `Other` ● | Trin D | M0.5 |
| E2 | v2: `Other` bevares (K7) | E1 | M0.8 |
| E3 | v2: `Items` ubetinget; `File`/`Markdown` bag `unstable_plan_operations` ● | E1 | — |
| E4 | v1: **hele** enumen bag `unstable_plan_operations` ●; ingen `Other` | — | M0.8 |
| E5 | `planId` læses inde i indholdet ● | E1, E4 | M0.5 |
| E6 | Agentens vilje til at sende planer observeres (K8) | E1 | M0.5, M0.6 |

### Trin F — Indhold, filer, terminaler, diff, samlinger †

Hviler på migreringsdokumentet. Uverificeret. Samlingsreglen fra K4 gælder, hvor samlinger optræder i v2.

### Trin G — Udvidelser

| # | Leverance | Afhængighed | Måleport |
|---|---|---|---|
| G1 | MCP over ACP: klientens flagvalg besluttet (K9) | A3 | M0.9 |
| G2 | `unstable_llm_providers`: `providers` er `Option`, udeladt/`null` = annoncerer ikke ● | G1 | M0.12 |
| G3 | `auth`, `elicitation` i v2's `ClientCapabilities` (tre felter, alle `Option` ●) | Trin B | M0.12 |
| G4 | Suffli-konfiguration via `configOptions` (K10) | Trin B | M0.10 |
| G5 | `compaction` og `notices` (K10) | Trin B | M0.10 |
| G6 | `notice` i v2 kræver ingen klientevne ● | Trin B | M0.7 |

### Trin H — Register, kildeføring, måleporte

| # | Leverance | Afhængighed |
|---|---|---|
| H1 | Kilderegister med hentedato, genåbningsstatus og udsnitsmarkering | A5 |
| H2 | Feltregister ført à jour ved hver revision; ingen felt uden sti og flag | A12 |
| H3 | Unionsregister over `SessionUpdate`- og `PlanUpdateContent`-varianter pr. version | Trin D, E |
| H4 | Alle 17 måleporte udført og journalført med lagangivelse | A6 |
| H5 | Ændringslog, der skelner «rettet» fra «tilføjet» fra «opdaget» | — |

---

## 8. Måleporte

Sytten porte. Ingen er udført. **M0.1 er metodeændret** i Rev. 13: den måler ikke funktionsflag, men adfærd (F24).

| Port | Spørgsmål | Lag | Metode |
|---|---|---|---|
| **M0.0** | Findes `vibe-acp` på målmaskinen, i hvilken version, under hvilken licens? | L3 | `--version`, pakke metadata ● (versionsangivelse findes i repoet [^2]) |
| **M0.1** | Hvilken protokolversion svarer agenten på i `initialize`? | L3 | Anmodning/ svar, ikke flagaflæsning |
| **M0.2** | Hvilke `SessionUpdate`-varianter udsender den faktisk? | L3 | Trafikprotokol over en repræsentativ session |
| **M0.3** | Er `tool_call_update` indlejret eller flad? | L3 | Trafikprotokol |
| **M0.4** | Følger agenten den **toleddede** rettelsesregel ved ukendt id? | L3 | Fremtvunget sekvens: opdatering før annoncering |
| **M0.5** | Sendes planopdateringer, og i hvilken form (`items`/`file`/`markdown`/`other`)? | L3 | Trafikprotokol |
| **M0.6** | Sendes `plan_removed`? | L3 | Trafikprotokol |
| **M0.7** | Sendes `notice`? | L3 | Trafikprotokol |
| **M0.8** | Bevares eller fejler ukendt planindhold — i v1 og i v2? | L1+L3 | Indsprøjtet ukendt variant |
| **M0.9** | Kræver agentens `acp`-mærke et flag, og skal Skrynja selv bygges med `unstable_mcp_over_acp`? | L1+L3 | Annoncering + beslutning (K9) |
| **M0.10** | Annonceres og respekteres `configOptions`, `compaction`, `notices`? | L1+L3 | Annonceringsudveksling |
| **M0.11** | Understøttes `load_session`, og med hvilken semantik? | L3 | Trafikprotokol |
| **M0.12** | Annonceres `auth`/`providers`, og håndhæves de? | L1+L3 | Annonceringsudveksling |
| **M0.13** | Virker afbrydelse — med og uden `unstable_cancel_request`? | L2+L3 | Målt gennem begge veje |
| **M0.14** | Genkendes fejlkoderne `-32022`, `-32602`, `-32800`? † | L3 | Fremtvungne fejl |
| **M0.15** | Kan **Skrynja** bygges med den nødvendige SDK-funktionsprofil (A-4)? | L1 | Byggeprofil, ikke agentmåling |
| **M0.16** | Kan `agent-client-protocol-http` indsættes foran en proces, den ikke selv starter? | L2 | Indsættelsesforsøg |

**Rækkefølge:** M0.0 → M0.15 → M0.1 → M0.3 → M0.4. De fem afgør, om resten kan planlægges.

---

## 9. Risikoregister

| # | Risiko | Sandsynlighed | Virkning | Afbødning |
|---|---|---|---|---|
| **R1** | Agentens adfærd følger ikke skemaets gating — en Python-implementering kan sende skjulte felter og udelade krævede | høj | høj | M0.2–M0.8; modellen er normativ for **typer**, agenten for **adfærd** (F25) |
| **R2** | Målemetoden var uanvendelig på L3 | indtruffet | høj | M0.1 omlagt til adfærdsmåling |
| **R3** | Broen findes i førstepart, men kun som Rust-server foran en agent, den selv starter | middel | høj | M0.16; A2 som reel beslutning, ikke arv |
| **R4** | v1-planindhold fejler på ukendt variant og river hele opdateringen med | middel | høj | M0.8; defensiv parsning på L1-uanset |
| **R5** | Rettelseslaget implementeres som «udeladelse = uændret» også ved nye id'er | middel | høj | K3 + D2 + enhedsprøve med ukendt id |
| **R6** | SDK-funktionsprofil og skemaflag forveksles | middel | middel | A-4 som egen akse; M0.15 som egen port |
| **R7** | Annullering antages at virke gennem broen | middel | middel | K12; M0.13 måler begge veje |
| **R8** | Kilder kan ikke genåbnes, og †-afsnit glider ind som kendsgerninger | høj | middel | H1; †-mærke i alle tabeller |
| **R9** | Agentversion og protokolversion forveksles (v2.26.1 er programmets, ikke protokollens) | middel | middel | M0.0 og M0.1 som **to** porte |
| **R10** | Planlagte flader antages at kunne annonceres af agenten (K8) | lav | middel | E6 |

---

## 10. Åbne forbehold

1. `v2::PlanItems`, `PlanFile`, `PlanMarkdown`, `OtherPlanUpdateContent` — feltformer og JSON-navne ○.
2. v2's tredje `ClientCapabilities`-felt er registreret som eksisterende, men ikke navngivet ○.
3. `v1::ToolCallUpdateFields`' feltliste og `name`-dokumentation ○; «v1: begge = uændret» er †.
4. `PromptCapabilities`, `McpCapabilities`, `AgentAuthCapabilities`, `Usage`, `InitializeRequest`/`-Response` ○.
5. v2's `AgentCapabilities` og `SessionCapabilities` ○.
6. `agent-client-protocol-http`s ledningsform: konvolutter, sessionsmapping, kompatibilitet med en vilkårlig stdio-agent ○.
7. Transportens samspil med `_proxy/*`-konvolutter ○.
8. Samtlige livscyklus-, fejl- og indholdsafsnit, der hviler på migreringsdokumentet †.
9. Om `vibe-acp` overhovedet forhandler v1 ○ — det mest konsekvensrige ubekræftede punkt.

---

## 11. Ændringslog Rev. 12 → Rev. 13

| # | Ændring | Oprindelse |
|---|---|---|
| 1 | Fjerde akse (SDK-funktion) og femte akse (lag) indført | F16, F24 |
| 2 | Rettelsesreglen gjort toleddet: kendt id / ukendt id | F17 |
| 3 | Trin E omskrevet: fire varianter i v2, tre i v1, ingen `Other` i v1 | F22, F23 |
| 4 | `Items`' flagstatus pr. version rettet | F18, F23 |
| 5 | Navnekonventionen skelner model, ledning og sti | F19 |
| 6 | `MaybeUndefined` som navngivet undtagelse | F20 |
| 7 | Transportcraten indført; Trin B og A2 omskrevet; K11 og K12 tilføjet | F15, F26 |
| 8 | M0.1 omlagt fra flagaflæsning til adfærdsmåling | F24 |
| 9 | Modellens autoritet præciseret: typer vs. adfærd | F25 |
| 10 | K8: planer annonceres kun af klienten | F27 |
| 11 | K9 og M0.9 udvidet til Skrynjas egen byggeprofil | F28 |
| 12 | Versionspin med hentedato; forbehold 15 (SDK-springet) udgår som besvaret | — |
| 13 | Kilderegister indført som H1 | M2 |
| 14 | 17 måleporte navngivet og nummereret; ingen udført | M1 |

---

## 12. Samlet status

Modelfladen er målt: varianter, felter, stier, flag, stabilitet og evnekrav holder på 36 efterprøvede påstande fordelt over tre runder uden afvigelse i den målte halvdel. Dokumentationshalvdelen (†) er ikke genåbnet, og kilderegisteret findes endnu ikke.

Det, der mangler, er ikke flere opslag. Det er **seksten uudførte måleporte mod L3 og L2**, en omlagt målemetode og et transportvalg, der skal træffes igen. Før M0.0, M0.15, M0.1, M0.3 og M0.4 er udført, er vejkortet en kontrakt uden modpart — præcis, men uafprøvet.

**References**

[^1]: [Releases · mistralai/mistral-vibe](https://github.com/mistralai/mistral-vibe/releases) (55%)
[^2]: [mistralai/mistral-vibe: Minimal CLI coding agent by ...](https://github.com/mistralai/mistral-vibe) (26%)
[^3]: [mistral-vibe/vibe-acp.spec at main · mistralai/mistral-vibe · GitHub](https://github.com/mistralai/mistral-vibe/blob/main/vibe-acp.spec) (19%)
