# vibe-acp målelab

Måleresultater og scripts fra arbejdet med vejkortet for Skrynja mod `vibe-acp`
(ACP-klient, jf. `../vejkort.md` Rev. 13).

**To maskiner, to versioner:** måleserien er delt mellem **målmaskinen** (sandbox — `vibe-acp` **2.9.4**, runde 1-2, adfærdsmålinger før telefonen kom ind) og **telefonen** (Jolla Phone — `mistral_vibe` **3.0.2**, runde 3-4, wire-målinger og kodelinje-verifikationer). Hver måling hører til én af dem; «Hovedfund»-afsnittene nedenfor er mærket tilsvarende.

## Indhold

| Fil | Beskrivelse |
|---|---|
| `maalejournal-runde1.md` | Første målerunde: M0.0, M0.1, M0.15, M0.14 (delvist) |
| `maalejournal-runde1-tilfoejelse.md` | Afslutning af M0.15/M0.1 + forberedelse af M0.16 |
| `maalejournal-runde2.md` | Anden målerunde: M0.0b (integritet), M0.1b (fuld frame), M0.16 (bro, halvt), versionsaudit |
| `maalejournal-runde3-wire.md` | Tredje målerunde: wire-bekræftelser fra telefonen via relæ — N8 lukket, L5 korrigeret, N6 åben, N7 lukket |
| `analyse-mistral-vibe-302-kode.md` | Kodelæsning af mistral-vibe 3.0.2 sdist: R30 svar (ikke rettet), betinget `_reasoning_effort()`, glm-modeller ikke i statisk katalog, ACP configOptions-validering |
| `analyse-harness-r30-r42-verifikation.md` | Verifikation af R30–R42 i både harness- og kernepakken |
| `analyse-r32-katalog-vs-kode.md` | R32 afgørende: kataloglaget kasserer `auto_compact_threshold`; `experiment_eval_cache.json` er GrowthBook; harness `_core_config` har disabled kompaktion (senere korrigeret, se næste) |
| `analyse-n5-n6-n7-growthbook-usage.md` | N5/N6/N7: GrowthBook injicerer modeller uden om `_CATALOG_FIELDS`; N7 omformuleret til `usage_update`; N8 nyt (`size` = tærskel) |
| `analyse-sentinel-l5-size-guard.md` | Sentinel-analyse: `-1` kan ikke lække i `size`, men `0` kan; L5 udvidet med `size <= 0`-guard |
| `analyse-permission-always-allowlist.md` | `permission = "always"` + allowlist-mekanismen; mode-spørgsmålet lukket |
| `analyse-n6-korrigeret-derive-config.md` | N6 korrigeret: kompaktion er **automatic** ved 800000 (ikke disabled) — `_core_config` er template, aktiv config kommer fra `derive_config` via `configure_runtime`; lærd lektion tilføjet som R11 |
| `analyse-n8-kaede-context-window.md` | N8 kæde lukket: `usage_update.size` kommer fra `RuntimeSnapshot.context_window = auto_compact_threshold` (`_runtime.py:2202`); navnekollision med `max_context_length` på linje 1584; `size` måler afstand til kompaktion |
| `maalejournal-afsluttende-status.md` | **Afsluttende status:** måleserien færdig — N5 filbevis, 18→2 korrektion, R12 nyt risiko, klientregler tabel, implementationsrækkefølge, risikoregister endelig tilstand |
| `analyse-session-cancel-sdk-dispatch.md` | `session/cancel` er fuldt understøttet (SDK dispatcher selv — tekstsøgning var ikke evidens); `session/resume` er unstable men implementeret uden historik-replay — `session/load` er metoden til genåbning med historik; lærd lektion R13 |
| `analyse-arkitekturvalg-a1-a2-set-mode.md` | **Arkitekturvalg vedtaget:** A1 (cancel-rækkefølge m. cancelled-svar først), A2 (`session/load` med historik, `session/new` fallback); kun `session/set_config_option` — aldrig `session/set_mode` (ingen configOptions i svar, tavst None ved fejl) |
| `analyse-a7-korrigeret-thinking-validering.md` | **A7 korrigeret:** ingen agent-lag afviser `low` før API'et — ACP tjekker kun generisk `THINKING_LEVELS`, model-laget accepterer `low` fordi zai-glm selv erklærer det; først harness-adapteren mapper `low→"none"` → `3051`. Fravalgslisten er **model-niveau**, ikke kun to aliaser |
| `analyse-beslutning-thinking-ui-fravalgsliste.md` | **Beslutning:** `medium`/`max` vises med «samme som High» (skjules ikke); A7 som 3-lags tilstand — hardcoded standard + `Skrynja.conf` override + indlært ved `3051` |
| `analyse-lag3-positiv-evidens-r34.md` | **Lag 3 korrigeret:** lærer kun ved positiv evidens (`3051` + entydigt tekstmønster + første prompt efter skift til `low`); synlig «Nulstil»-vej. **R34 afgjort:** aktiv sti er `~/.config/org.skrynja/Skrynja/Skrynja.conf` |
| `analyse-lag3-bekraeftet-laering-r41.md` | **Lag 3 endelig:** bekræftet læring (dialog ved `3051` efter skift til `low` — ingen tekstmatch, falsk positiv umulig). **R41 afgjort:** katalog-cache er flaggated (`experimental_enable_model_catalog=False` default), ikke en fejl |
| `maalejournal-r34-konfigsti-lukket.md` | **R34 ● lukket på telefonen:** ekstra `~/.config/harbour-skrynja/` slettet (backup i `/tmp/skrynja-konf-backup-2026-10-27`); én aktiv sti — lag 2+3 ligger utvetydigt i `~/.config/org.skrynja/Skrynja/Skrynja.conf`. Nummereringsafklaring: relæets «R42» kaldes R34 her (R42 = tom katalog, allerede optaget) |
| `gennemgang-journal-foer-kode.md` | **Gennemgang før kodning:** ID-konsistens ●, wire-format spec for Trin E, UTF-8/buffer-regler, Trin E-testplan. Afklaring: måleseriens «Trin E» ≠ vejkortets Trin E (Planer) |
| `fixture-trin-e.ndjson` | **Trin E fixture:** 13 komplette målte wire-linjer + 1 delvis linje (EOF-test). Trin E bygges og testes mod denne fil fra første linje |
| `fixture-trin-e-sammenklaebning.ndjson` | **Trin E edge-case:** to JSON-objekter på én linje uden `\n` (id=7, id=8) + delvis linje — tester parserens objektgrænsehåndtering |
| `analyse-fixture-rettelse-objektgraenser.md` | **Fixture rettet** (sammenklæbning var min fejl) + **parser-beslutning:** del på `\n`, ved «Extra data» brug `QJsonParseError::offset` til at udtrække flere objekter fra samme linje (Qt 5.6-ækvivalent til `raw_decode`). Testplan **før tredje fixture** (`-ugyldig`): 15 succes-parses, 2 forventede EOF-fejl, 0 crashes |
| `fixture-trin-e-ugyldig.ndjson` | **Trin E edge-case:** gyldigt objekt + ugyldig resten (×2), normalt objekt, delvis linje — tester regel 2 (forkast hele linjen) |
| `analyse-parser-spec-komplet-a6-buffer.md` | **Parser-spec komplet ●:** tre sikkerhedsregler (kun ExtraData splitter; valider første objekt eller forkast hele linjen; værn mod nul-fremgang) + A6 buffergrænse. Endelig testplan **efter tredje fixture** (`-ugyldig`): 16 udleveringer, 4 forventede forkastninger/EOF, 0 crashes |
| `analyse-a6-begrundet-forkast-ikke-draeb.md` | **A6 begrundet ●:** `ACP_MAX_LINE_BYTES` = **8 MiB** (margin: bash 16 KB, grep 64 KB, hooks 1 MB). Reaktion differentieret: **A6a** (komplet linje over grænsen → forkast én linje, forbindelse består) / **A6b** (buffer uden `\n` over grænsen → protokolkorruption, luk agent) |
| `analyse-svarfrist-transportlag-a8.md` | **A8 — generel svarfrist ●** *(erstattet: fejlkoden er siden rettet til `-33000`, se næste rækker)*: hver anmodning med id får frist; ved udløb syntetisk fejl `-32000`, UI frigøres. Lukker A6a-følgen (forkastet svar-linje) samt procesdød og hængende agent |
| `analyse-a8-progressbaseret-fork-unstable.md` | **A8 endelig ●** *(erstattet: `-32000` → `-33000`, se næste række)*: `session/prompt` får **progress-baseret** frist (forny ved hver besked for sessionen — långsomme turns drabtes ikke); resten 120 s fast; permission-svar har agentens egen frist. `session/fork` verificeret (unstable, aktiv, ikke kaldt i v1). `-32000` markeret syntetisk. Replay-sti rettet til `vibe/acp/session_updates.py` |
| `analyse-a8-komplet-rutekort-kode.md` | **A8 komplet ●** *(titel overdrevet: kun 10 af 11 request-ruter havde frist — `session/resume` manglede; rettet i næste række)*: alle request-ruter undtagen `session/resume` har frist (`session/list`, `set_mode`, `authenticate` tilføjet). Fejlkode **`-33000`** syntetisk (uden for JSON-RPC's reserverede server-bånd). Progress-fornyelse **sessionsbundet** (kun samme sessionId; fornyes ved chunk/tool_call/tool_call_update/request_permission). Sene svar efter timeout: log + forkast. Kapabiliteter `close`/`list`/`fork` annonceres i `initialize` ●. `unstable`-mekanisme præciseret: afviser med `-32601` hvis ikke aktiveret — inaktiv på Skrynjas forbindelse |
| `analyse-a8-korrigeret-resume-frist-kapabiliteter.md` | **A8 korrigeret ●:** `session/resume` får 120 s fast (spec'et, ikke kaldt i v1 — A2). Kapabilitetsmængder adskilt: annonceret `{close,list,fork}` / unstable-ruter `{close,fork,resume}` / schema `{list,delete,additionalDirectories,fork,resume,close}` — `resume` er unstable-rute **uden** annonceret kapabilitet. `callback_kinds` præciseret: `["approval"]` **plus** `"user_input"` ved `elicitation.form`. `-32601` har **tre** årsager. `session/list` = nej i v1 |
| `analyse-a9-indgaaende-anmodninger-kapabiliteter.md` | **A9 ●** *(erstattet af næste række: én `terminal/*`-række → `-32601` var forkert — ruterne er `optional=True` i SDK'et)*: Skrynjas svartid på indgående anmodninger. `clientCapabilities: {}` er korrekt v1 (målt på wire) — ingen fs/terminal/elicitation. Skrynja implementerer kun `request_permission` + `session/update`; afviser resten øjeblikkeligt med `-32601`. Ingen klient-timeout på permission (brugeren beslutter). Elicitation udeladt → ingen unstable-eksponering. Rutekort fuldt målt på **begge** sider |
| `analyse-a9-korrigeret-terminal-optional.md` | **A9 korrigeret ● + bekræftelsesrunde 2 fra telefonen:** `terminal/*` er `optional=True` i SDK'et — to rækker, ikke én (`create`/`output`/`wait_for_exit` → `null`; `release`/`kill` → `{}`). `_resolve_handler` verificeret (fanger manglende attribut → `_make_func` returnerer `None` → `handle` svarer `default_result`; omvendt signatur-fallback for legacy_api). Agent-null sporet: `model_validate(None)` → `ValidationError` → værktøjsfejl, **ikke hæng**. `terminal/*`/`fs/*` unåelige i v1 på **begge** værtsveje (kapabiliteter **og** ingen `client_tools` i `LocalHarnessOptions`). Skrynjas egen C++-klient svarer alligevel `-32601` på alle fem |
| `maaling_m00_m01.py` | Script: stdio `initialize`-udveksling mod `vibe-acp` (M0.1) |
| `maaling_m01b_fuld_frame.py` | Script: `initialize` med `protocolVersion` 99/2/0 — måler versionshåndtering (M0.1b) |
| `m016-bro/` | Rust reference-bro: `AcpHttpServer` + `AcpAgent` spawner `vibe-acp`, loopback roundtrip (M0.16) |
| `maaling_m00_versionscheck.py` | Script: `vibe-acp --version` + pakke-licenser (M0.0) |
| `maaling_m03_sessioner.py` | Script: `session/new`, `session/load`, `session/prompt` uden API-nøgle |
| `maaling_m14_fejlkoder.py` | Script: fremprovokerede fejlkoder (ukendt metode, ugyldige params, ukendt session) |
| `malelab-sitecustomize.py` | Shim der får `vibe-acp` 2.9.4 til at starte med `mcp` 2.3.0 (to inkompatible API'er) |
| `vejkort-rev13-arbejdskopi.md` | Arbejdskopi af vejkort Rev. 13 (som vedhæftet) |

## Kørsel af scripts

Scripts forventer at `vibe-acp` er installeret og kører med shim'en:

```sh
export PYTHONPATH=<denne mappe>   # aktiverer malelab-sitecustomize.py
python3 maaling_m00_m01.py
```

## Hovedfund (runde 3–4 — målt på telefonen via relæ)

Telefonen kører `mistral_vibe` **3.0.2** (identisk med PyPI), `agent_client_protocol` **0.11.0**, `mistralai` **2.6.0**; harness-pakken er indlejret i kernepakken (ingen egen dist-info).

- **N8 ● lukket på wire og i kode:** for `zai-glm-5-3-flash` (`max_context_length=1048576`, `auto_compact_threshold=800000`) rapporterer `usage_update.size = 800000` — tærsklen, ikke vinduet. Kæden er sporet: `acp/agent.py:1808` → `_runtime_resources.py:380` → `client_state.py:80` → `_runtime.py:2202` (`build_unified_runtime_snapshot`, kaldt fra `derive_config:1439`). En procent mod `size` måler **afstand til kompaktion**. `max_context_length` (det reelle vindue) når klienten ikke via ACP.
- **L5 ● korrigeret:** `used = 0` optræder med positiv `size` både før og efter gyldigt forbrug — filteret er på `used == 0` alene, plus guard mod `size <= 0`.
- **N6 ● falsificeret:** `mode: automatic, token_threshold: 800000` målt på wire. Min tidligere læsning af `_core_config` (`_host.py`) var en template, ikke aktiv konfiguration — den reelle konfiguration kommer fra `derive_config` (`app_server/_runtime.py:1385-1389`) via `configure_runtime`. Lærd lektion tilføjet som **R11**.
- **R32 ● lukket:** 800000 bekræftet tre veje — wire `token_threshold`, wire `usage_update.size`, GrowthBook-payload `auto_compact_threshold`. Kompaktionsrisikoen på `glm-5-3` er reel.
- **Tilladelsesdialog ● lukket:** årsagen til udeblevne dialoger er filkonfiguration (`permission = "always"` i to `[tools.*]`-sektioner + `"id"` i allowlist i `~/.vibe/config.toml`), ikke mode. Begge veje ender i `decision="allow"`.
- **R30 ● to uafhængige forekomster:** `low→"none"`-tabellen findes både i `mistralai_vibe_local_harness/vibe/adapters/mistral.py` og `vibe/core/llm/backend/mistral.py`. Ikke rettet i 3.0.2; kan ikke afhjælpes klientside — kun undgås (A7).
- **R34 ● lukket:** målt på telefonen — `~/.config/harbour-skrynja/` slettet (dens `Skrynja.conf` havde ingen effekt; kun `contextMessageLimit=0` fandtes kun dér, intet at overføre). Én aktiv sti: `~/.config/org.skrynja/Skrynja/Skrynja.conf` — lag 2+3 utvetydige. **R41 ◐ afgjort:** katalog-cache er flaggated (`experimental_enable_model_catalog=False` default i `vibe_schema.py:732`), ikke en fejl — ikke-blokerende.

## Implementationsklar backlog (fra måleserien — endelig)

Se `maalejournal-afsluttende-status.md` for den fulde klientregel-tabel og risikoregister.

1. **Trin E** — tolerant NDJSON-parser over `stdio` (første skridt) — bygges mod `fixture-trin-e.ndjson`; spec i `gennemgang-journal-foer-kode.md`
2. **Transport + session** — `initialize`/`session/new`/`session/prompt` + `cancel`
3. **A7** — undertryk `low` for modeller på fravalgslisten (3 lag: hardcoded standard + `Skrynja.conf` override + **bekræftet** læring via dialog ved `3051` efter skift til `low`); vis `medium`/`max` med «samme som High» — ingen agent-lag afviser `low` før API'et giver `3051`
4. **L5** — filtrér `used == 0`, guard `size <= 0` før procentberegning
5. **N8 + R12** — mærk `size` som «kompaktionsgrænse»; advar kun ved `used → size` når vindue er kendt og > tærskel, ellers «ukendt» — aldrig udled vinduet af `size`
6. **A5/H2** — vis altid `tool_call`/`tool_call_update`; læs og vis `permission` og `allowlist` fra `~/.vibe/config.toml`
7. **R37/R42** — `configOptions` er fornyelig; tomt modelkatalog = fejl, ikke tom menu
8. **`request_permission`** — defensiv håndtering (betinget kald; filkonfig kan undertrykke)

## Arkitekturvalg (vedtaget ●)

- **A1 — afbrydelse:** svar `cancelled` på udestående permission → send `session/cancel` → luk dialog lokalt → sæt tool calls `cancelled` lokalt → procesgenstart som fallback efter frist
- **A2 — genoptagelse:** `session/load` (med historik-replay); `session/new` ved `SessionNotFoundError`; `session/resume` fravalgt (ingen replay)
- **Mode/model/thinking:** kun via `session/set_config_option` — aldrig `session/set_mode`; svaret erstatter hele lokale `configOptions`; `thinking`-menu minus A7-forbudte par

## Hovedfund (runde 2)

- **Versionsaudit:** nyeste `mistral-vibe` på PyPI er **3.0.2** (kræver `agent-client-protocol==0.11.0`, `mcp==1.28.1`) — målmaskinen kører **2.9.4**, én major-version bagud. Alle adfærdsmålinger gælder 2.9.4.
- **M0.0b ●:** `pip check` viser hård konflikt `mistralai 3.2.0` vs krævet `==2.4.4`; `mcp 2.3.0` opfylder kravet `>=1.14.0` men har inkompatibel API — **det er mcp-konflikten der forårsager shim-behovet**.
- **M0.1b ●:** agenten afviser **ikke** `protocolVersion: 99/2/0` — svarer altid `protocolVersion: 1` uden fejl. **K1 må håndhæves klientside** (klienten disconnecter selv ved mismatch).
- **M0.16 ◐ halvt:** Rust-bro (`AcpHttpServer` + `AcpAgent`) spawner `vibe-acp` og loopback roundtrip virker (HTTP → bro → stdio → svar). "Bro foran en fremmed proces" er **ikke** målt — kræver custom `ConnectTo`.

## Hovedfund (runde 1)

- `vibe-acp` 2.9.4 (ikke 2.26.1 som vejkortet antog), Apache-2.0.
- Agenten crasher ved opstart med `mcp` 2.3.0 uden shim — to inkompatible API'er.
- Agenten forhandler **kun protokolversion 1** (`PROTOCOL_VERSION = 1` i Python SDK 0.9.0).
- Rammeformat: **newline-delimited JSON** (NDJSON), ikke Content-Length.
- Fejlkoder målt: `-32601`, `-32602`, `-32000` — ikke `-32022`/`-32800` som vejkortet antog (†).
- `session/new` og `session/load` kræver `MISTRAL_API_KEY` — uden den er M0.2–M0.8 og M0.11 blokeret.
- Skrynja kan bygges med Rust SDK 3.5.0 / schema 1.12.0 i alle relevante feature-profiler (M0.15 ●).
