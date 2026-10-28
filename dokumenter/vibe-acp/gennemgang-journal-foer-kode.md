# Gennemgang af journalen før kodning ● — klar til Trin E

Formål: sikre at journalen er konsistent og komplet nok til at fungere som specifikation og fixture-kilde for implementationen — før første linje C++ skrives. Gennemgangen er lokal; intet eksternt at hente.

## 1 · ID-konsistens ● med én afklaring

| ID-gruppe | Betydning | Status |
|---|---|---|
| A1, A2 | Arkitekturvalg (afbrydelse, genoptagelse) — **vedtaget** | ● låst |
| A5/H2, A7 | Arbejdslistepunkter — A7 omdefineret til 3-lags tilstand | ● låst |
| R11, R12, R13 | Lærd lektioner (template, feltnavn, tekstsøgning) | ● i risikoregister |
| R30, R31, R32 | Agent-interne risici (low→none, disable_reasoning, kompaktion) | ● låst |
| R33 | `Skrynja.conf` modelmenu forældet | ● låst (menu fra configOptions) |
| R34 | hvilken `Skrynja.conf`-sti er aktiv | ● **lukket på telefonen** (én aktiv sti) |
| R37 | `configOptions` fornyelig | ● håndter `config_option_update` |
| R40 | katalogcache-TTL 30 døgn | ○ information |
| R41 | `model_catalog_cache.json` skrives aldrig | ◐ afgjort (flaggate) |
| R42 | tom katalog er «authoritative» | ● vis fejl, ikke tom menu |
| N5–N8 | Måleseriens nye fund (GrowthBook, kompaktion, usage_update) | ● låst |
| L5 | nul-usage-filter + size-guard | ● låst |

**Afklaring ●:** «Trin E» bruges i journalen til **tolerant NDJSON-parser** (fra måleseriens arbejdsliste). Vejkort Rev. 13's Trin E er noget andet (Planer: E1–E6, v1/v2 plan-varianter). Ingen konflikt i praksis — vejkortets Trin E hører til den oprindelige Rust-planlægningsfase og er ikke aktiv; måleseriens «Trin E» er den operative betegnelse. Noteret for at undgå forvirring ved senere læsning.

**A6/B6 fra vejkortet ○:** A6 = måleprotokol for L3, B6 = WebSocket-gate. **Ingen af dem svarer til** «UTF-8/bufferstørrelse» eller «NDJSON-fixture» — de IDs findes ikke i journalen endnu. Se punkt 3.

## 2 · Hvad journalen dokumenterer om wire-formatet ● (input til Trin E)

| Faktum | Kilde | Status |
|---|---|---|
| NDJSON: ét JSON-objekt pr. linje, `\n`-termineret | runde 1 (`acp/connection.py:62`), telefon ● | ● |
| Ingen `Content-Length`-headers | runde 1, telefon ● | ● |
| `session/update` kommer **før** svar på samme `id` | telefon ● | ● |
| `cancel` er notifikation **uden** `id` | telefon ● | ● |
| `request_permission` har **heltals-**`id` (0); streng-id afvises | telefon ● | ● |
| `initialize` svarer `protocolVersion: 1` uanset forespørgsel (99/2/0) | runde 2 (M0.1b) ● | ● |
| Fejlkoder: `-32601`/`-32602`/`-32603`, **ikke** `-32022`/`-32800` | runde 1 (M0.14) ● | ● |
| `-32603` + `data.code:"3051"` + `raw_status_code:400` ved API-afvisning | telefon ● | ● |
| `stopReason:"cancelled"` med nul-`usage` ved cancel | telefon ● | ● |
| `usage_update`: `used` = faktiske tokens, `size` = kompaktionsgrænse, `cost` = USD | telefon + kode ● | ● |
| `configOptions` struktur: `{id,name,category,type:"select",currentValue,options:[{value,name,description}]}` | telefon ● | ● |
| `thinking.options` er modelspecifik og skifter ved modelvalg | telefon ● | ● |
| `tool_call`/`tool_call_update`: flad struktur, `toolCallId` nøgle | telefon ● | ● |

## 3 · Fundet i gennemgangen: A6 og B6 er ikke defineret i journalen ○

Relæ-AI'en nævnte «A6 (UTF-8/bufferstørrelse)» og «B6 (NDJSON-fixture)». I vejkort Rev. 13 har A6 og B6 **andre betydninger** (se punkt 1). Journalen mangler derfor to specifikationspunkter, som Trin E har brug for:

**A6' — tekstkodning og linjehåndtering ○ (forslag til ny betegnelse, da A6 er optaget):**

- stdio er en **byte-strøm**; agenten skriver UTF-8 (Python `json.dumps` default `ensure_ascii=True` → ren ASCII på wire, så escapes `\u0007`-form, ingen rå UTF-8 fra JSON-delen)
- **men:** `_meta`-felter og fremtidige udvidelser kan indeholde rå UTF-8; klienten skal håndtere begge
- linjeafgrænsning: `\n` (`\n`); en linje kan være **vilkårligt lang** (ingen protokol-maksimum målt) — parseren må akkumulere en delvis linje over flere `readyRead`-hændelser
- afslutning: agenten lukker strømmen ved procesdød — sidste linje uden `\n` skal behandles som komplet hvis processen er gået ned

**B6' — fixture-bank ○ (forslag):**

Journalen indeholder **én komplet rå wire-linje** (runde 2: `initialize`-svaret, `"jsonrpc":"2.0","id":1,"result":{…}`) plus fragmentariske gengivelser (runde 1: pretty-printet `initialize`-resultat uden `jsonrpc`-ramme; telefonens `request_permission`, `usage_update`-målinger, fejl-`-32603`).

**Konsekvens ○:** Trin E kan bygges mod **én ægte fixture** i dag. For at gøre parseren testbar fra første linje bør der oprettes en **fixture-fil** med et repræsentativt udsnit af en session — se punkt 4.

## 4 · Forslag: fixture-fil til Trin E ● (klar til vedtagelse)

Opret `dokumenter/vibe-acp/fixture-trin-e.ndjson` med følgende linjer, alle målt eller afledt fra målte:

| # | Linje | Kilde | Hvad den tester |
|---|---|---|---|
| 1 | `{"jsonrpc":"2.0","id":1,"result":{"protocolVersion":1,"agentCapabilities":{…},"agentInfo":{…},"authMethods":[]}}` | runde 2 verbatim ● | svar med id, komplet ramme |
| 2 | `{"jsonrpc":"2.0","method":"session/update","params":{"sessionId":"…","update":{"sessionUpdate":"agent_message_chunk","content":{"type":"text","text":"…"}}}}` | telefon ● (struktur målt) | notifikation uden id, før svar |
| 3 | `{"jsonrpc":"2.0","method":"session/update","params":{"sessionId":"…","update":{"sessionUpdate":"tool_call","toolCallId":"…","title":"…","kind":"shell","status":"in_progress"}}}` | telefon ● | tool_call, flad struktur |
| 4 | `{"jsonrpc":"2.0","method":"session/update","params":{"sessionId":"…","update":{"sessionUpdate":"usage_update","used":0,"size":800000}}}` | telefon ● | L5: filtrér used==0 |
| 5 | `{"jsonrpc":"2.0","method":"session/update","params":{"sessionId":"…","update":{"sessionUpdate":"usage_update","used":12345,"size":800000,"cost":{"amount":0.001,"currency":"USD"}}}}` | telefon ● + N8 | gyldig usage, size=kompaktionsgrænse |
| 6 | `{"jsonrpc":"2.0","id":0,"method":"session/request_permission","params":{"sessionId":"…","toolCall":{"toolCallId":"…"},"options":[…]}}` | telefon ● | heltals-id, permission |
| 7 | `{"jsonrpc":"2.0","method":"session/cancel","params":{"sessionId":"…"}}` | — (klient→agent, ikke målt som modtaget) | klientens egen udgående — ikke en parser-fixture, men dokumenteret |
| 8 | `{"jsonrpc":"2.0","id":2,"error":{"code":-32603,"message":"API error occurred: Status 400 …","data":{"code":"3051","raw_status_code":400}}}` | telefon ● | fejl med data.code |
| 9 | `{"jsonrpc":"2.0","id":3,"result":{"stopReason":"cancelled","usage":{"inputTokens":0,"outputTokens":0,"totalTokens":0}}}` | telefon ● | cancelled + nul-usage |
| 10 | (delvis linje uden `\n`, fil slutter) | syntetisk | buffer-akkumulering ved EOF |

**Trin E-test fra dag ét ●:** læs fixture-filen, parse hver linje, assertér:

- linje 1 → svar med id 1, `protocolVersion == 1`
- linje 2/3 → notifikation uden id, `sessionUpdate` kendt
- linje 4 → filtreres af L5 (`used == 0`)
- linje 5 → `used=12345`, `size=800000` accepteres; `size` mærkes «kompaktionsgrænse»
- linje 6 → heltals-id bevares (ikke streng)
- linje 8 → `error.code == -32603`, `data.code == "3051"`
- linje 9 → `stopReason == "cancelled"`, usage nul → L5
- linje 10 → ingen crash, delvis linje parses ved EOF
- **tolerant:** tilføj en linje med et ukendt `sessionUpdate` («fremtidig_variant») → parseren må ikke fejle, skal logge og fortsætte

## 5 · UTF-8-specifikation for Trin E ● (fra målt viden)

- **Wire fra agenten:** ASCII-sikker (`ensure_ascii=True` default i Python SDK) — men klienten må ikke *antage* det; `_meta` kan bære vilkårlig tekst
- **Qt 5.6:** `QProcess::readAllStandardOutput()` giver `QByteArray`; `QString::fromUtf8()` konverterer. En delt UTF-8-sekvens på tværs af to reads håndteres naturligt af `fromUtf8` på den akkumulerede buffer — **kun hvis** parseren akkumulerer bytes før linjeskift, ikke konverterer pr. read
- **Regel ●:** akkumulér rå bytes i en `QByteArray`-buffer; find `\n`; konverter kun komplette linjer med `QString::fromUtf8`; ved ugyldig UTF-8: erstat med U+FFFD (Qt's default), log, fortsæt — aldrig crash
- **Regel ●:** ingen antagelse om maksimal linjelængde; buffer vokser efter behov (en kompaktér-turn kan sende store `tool_call_update` med `rawOutput`)

## 6 · Konklusion ●

Journalen er **konsistent og komplet nok til at gå videre** — med to tilføjelser:

1. **Opret fixture-filen** `fixture-trin-e.ndjson` (punkt 4) — gør Trin E testbar fra første linje
2. **Definér UTF-8/buffer-reglerne** (punkt 5) — de er ikke eksplicit i journalen endnu

Begge er dokumentationsarbejde, ikke kode. Når de er på plads, er Trin E fuldt specificeret.

**Uændret låst ●:** A1, A2, kun `session/set_config_option`, A7 3-lags med bekræftet læring, thinking-UI, N8/R12, L5, R37/R42, Trin E (tolerant parser). R34 ●, R41 ◐.

---

## Kropsrettelse i runde 11 ● (relæ `6fc943d2`)

Filens byte 3809 var et råt NUL-byte (`\x00`), hvor der skulle stå `\u0007` i «escapes `\u0007`-form» — git/grep behandlede derfor filen som binær. Bytet er rettet in-place; ingen anden tekst er ændret, og filen dekoder nu som ren UTF-8.

---

## Statusnote — ikke gældende for buffer ● (runde 12, relæ `83268bd1`)

Denne fil er før-kode-review'en — ikke parser-spec'en. Wire-format-fakta (§2) og UTF-8-regler (§5) er konsolideret i `spec-trin-e-ndjson-parser.md` (§2-§3), som fra runde 12 er den gældende specifikation for Trin E. **§5's sidste regel («ingen antagelse om maksimal linjelængde») var et forslag fra før A6-beslutningen og er lukket af A6 (8 MiB, A6a/A6b)** — læs ikke denne regel som gældende for buffer. Fixture-forslaget i §4 er realiseret som de tre `fixture-trin-e-*.ndjson`-filer.
