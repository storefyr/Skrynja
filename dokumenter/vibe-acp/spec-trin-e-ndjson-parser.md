# Trin E — normativ specifikation for NDJSON-fortolkeren ● (konsolideret og gældende)

*(Skrevet mod revision `a58804bc342c595972af7b4ccd48854d1d4130d8` — se §9. Relæ `83268bd1`.)*

## 0 · Formål, scope og selvstændighed

**Formål:** Denne specifikation er det samlede, normative grundlag for implementeringen af Trin E — den tolerante NDJSON-fortolker over `stdio` i Skrynjas ACP-klient. Trin E kan kodes direkte mod dette dokument + de tre fixtures i §7; ingen anden fil skal åbnes sideløbende.

**Scope — parserens ansvar ●:**
- Læse en byte-strøm fra agentens stdout (`QProcess::readyRead`)
- Akkumulere rå bytes, splitte på `\n`, udtrække komplette JSON-objekter — inklusive flere objekter på én linje (offset-loop)
- Håndtere en delvis linje ved EOF (procesdød)
- Håndtere buffergrænsen A6 (8 MiB, A6a/A6b)
- Være tolerant: aldrig crash, aldrig uendelig løkke; logge og fortsætte ved ugyldige linjer

**Uden for scope (applikationslaget over parseren):**
- L5: filtrér `usage_update` med `used == 0`; guard mod `size <= 0`
- N8/R12: mærk `usage_update.size` som kompaktionsgrænse, aldrig «kontekstvindue»
- R37/R42: `configOptions` er fornyelig; tomt katalog = fejl
- A8: svarfrister; A9: indgående anmodninger (permission-dialog, `-32601`-afvisninger)

Parseren udleverer rå JSON-objekter; beslutninger om indhold træffes i laget over.

**Selvstændighed ●:** Alle målinger, begrundelser og beslutninger, som specifikationen bygger på, er gengivet her. Kilderne (fem definitionsfiler + én prøvefil) er konsolideret — se §1 og §9.

## 1 · Statusdeklaration — konsolidering

Dette dokument er en **konsolidering**: det samler den gældende specifikation for Trin E, som før var spredt over flere filer med indbyrdes krydsreferencer. Det er hverken en ren tilføjelse (der manglede en spec) eller en ren erstatning (kilderne bevares); det er en samling, der gør én fil gældende.

### 1.1 Status pr. kildefil

| Fil | Status efter denne konsolidering | Konsekvens |
|---|---|---|
| **`spec-trin-e-ndjson-parser.md`** (dette dokument) | **Gældende normativ specifikation for Trin E ●** | Trin E kodes mod dette dokument + fixtures i §7 |
| `analyse-parser-spec-komplet-a6-buffer.md` | **Erstattet som gældende spec** — konsolideret her (inkl. runde 11-rettelsen: 8 MiB, A6a/A6b) | Bevares uændret som revisionsspor; README-rækken markeres «erstattet af»; statusnote appenderet |
| `analyse-a6-begrundet-forkast-ikke-draeb.md` | **Bevares — begrundelsesdokument.** Beslutningen (8 MiB, A6a/A6b) uændret og gældende; begrundelsen står dér og gengives i §5 | Normativt indhold konsolideret her; statusnote appenderet; README-rækken opdateret |
| `analyse-fixture-rettelse-objektgraenser.md` | **Bevares som historik.** Fixture-rettelsen og offset-beslutningen uændret og gældende; konsolideret i §4 | Dens testplan (15 parses + 2 EOF, før tredje fixture) afløst af den endelige i §8; statusnote appenderet |
| `gennemgang-journal-foer-kode.md` | **Bevares uændret — før-kode-review, ikke spec.** §5's buffer-regel («ingen antagelse om maksimal linjelængde») var et forslag fra før A6-beslutningen og er **lukket af A6** | Statusnote appenderet, så §5 ikke læses som gældende for buffer; wire-format-fakta og UTF-8-regler konsolideret i §2-§3 |
| `README.md` | **In-place (levende indeks)** | Ny række for dette dokument; parser-spec-rækken markeres «erstattet»; backlog-peg opdateret (§1.2) |
| `fixture-trin-e.ndjson`, `fixture-trin-e-sammenklaebning.ndjson`, `fixture-trin-e-ugyldig.ndjson` | **Uændrede — gældende testdata** | Ingen ændring |

### 1.2 Gældende status og referenceopdateringer

**Dette dokument afløser `analyse-parser-spec-komplet-a6-buffer.md` som gældende parser-specifikation ●.** Fra og med denne commit må kun ét dokument gøre krav på status som gældende spec for Trin E — dette.

Nødvendige referenceopdateringer (alle udført i samme commit):

1. README indholdstabel: ny række for dette dokument; rækken for `analyse-parser-spec-komplet-a6-buffer.md` markeres «erstattet af» dette dokument.
2. README backlog punkt 1 («Trin E»): peg opdateres fra `analyse-parser-spec-komplet-a6-buffer.md` til dette dokument.
3. Statusnoter appenderet i de fire ældre dokumenter (se §1.1), så ingen af dem læses som gældende uden at åbne dette dokument.

Verificeret ved `a58804b`: ingen andre aktive peg findes på `analyse-parser-spec-komplet-a6-buffer.md` — ældre filers statuslinjer («Parser-spec + transport-spec komplet») er daterede påstande om daværende tilstand, ikke peg, og ændres ikke.

### 1.3 Arkitekturvalg og «lukket»-erklæringer

**Ingen erklæring ophæves. Alle «lukket»- og «ingen arkitekturvalg genåbnes»-erklæringer i ældre filer gælder fortsat ●.** Konsolidering flytter ikke beslutninger — den samler dem. «Lukket» betyder «ikke genåbnes uden ny måling eller ny beslutningsrunde»; en konsolidering er ingen af delene.

Konkret:

- **A6 (8 MiB, A6a/A6b)** — besluttet i `analyse-a6-begrundet-forkast-ikke-draeb.md` (`804b4ae`), erklæret lukket dér: gældende uændret; værdierne står i §5.
- **Parser-strategi (offset-loop ved «Extra data»)** — vedtaget i `analyse-fixture-rettelse-objektgraenser.md`: gældende uændret; algoritmen står i §4.
- **Trin E som tolerant parser** — vedtaget i måleseriens arbejdsliste: gældende uændret.

Hvor dette dokument **præciserer** frem for blot at samle, er præciseringen inden for den eksisterende beslutning — ikke en genåbning:

- Scope-afgrænsningen i §0 (parser vs applikationslag) præciserer, hvad Trin E omfatter.
- EOF-formuleringen i §6 præciserer før-kode-review'ens «sidste linje uden `\n` behandles som komplet» → «forsøges parset; afbrudt logges, ikke fejl».
- §5 erklærer udtrykkeligt, at før-kode-review'ens §5-forslag «ingen antagelse om maksimal linjelængde» er lukket af A6.

Hvis en fremtidig runde vil ændre en vedtaget værdi (f.eks. hæve 8 MiB ved en målt større legitim linje på telefonen), kræver det ny måling — samme regel som før konsolideringen.

## 2 · Wire-format og kontekst (målt baggrund)

Parseren opererer på agent → klient-retningen af ACP over stdio. Følgende er målt (runde 1-2 målmaskine, runde 3-4 telefon) og indgår som kontekst:

| Faktum | Måling |
|---|---|
| Rammeformat | NDJSON: ét JSON-objekt pr. linje, `\n`-termineret ● |
| Framing | Ingen `Content-Length`-headers ● |
| Protokolversion | Agenten svarer `protocolVersion: 1` uanset forespørgsel (99/2/0) ● |
| `id`-type | Heltals-`id` (f.eks. `request_permission` med `id: 0`); streng-id afvises af agenten. **Parseren skal bevare heltals-id — må ikke konvertere til streng** ● |
| Fejlkoder | `-32601` / `-32602` / `-32603` målt; **ikke** `-32022`/`-32800` ● |
| Rækkefølge | `session/update` kan komme før svar på samme `id` ● |
| Tolerance | Ukendte `sessionUpdate`-varianter («fremtidig_variant») forekommer — parseren må ikke fejle; objektet udleveres, applikationen logger og ignorerer ● |
| Tekstkodning | Wire er ASCII-sikker (Python `ensure_ascii=True`), men klienten må ikke *antage* det: `_meta`-felter og fremtidige udvidelser kan bære rå UTF-8 ● |

## 3 · Byte-akkumulering og UTF-8 ●

1. Akkumulér **rå bytes** i en `QByteArray`-buffer ved hver `QProcess::readyRead`. Konvertér **ikke** pr. read — en delt UTF-8-sekvens på tværs af to reads håndteres naturligt af `fromUtf8` på den akkumulerede buffer.
2. Find `\n` i bufferen; kun **komplette linjer** konverteres med `QString::fromUtf8` og parses.
3. Ved ugyldig UTF-8: erstat med U+FFFD (Qt's default), log, fortsæt — **aldrig crash**.
4. Arbejd på **bytes** hele vejen gennem split/offset (§4): `QJsonParseError::offset` er et byte-offset. For ASCII-sikker wire er byte-offset = char-offset; ved rå UTF-8 i `_meta` kan de afvige — objekt-split på `QByteArray`, konverter hvert fundet objekt med `fromUtf8` før `fromJson`.

## 4 · Linjesplit og objektudtrækning ●

**Vedtaget strategi ●** (tre valg vurderet; tredje vedtaget — genbruger JSON-parseren, ingen egen tokenizer): Del bufferen på `\n`; ved «Extra data»-fejl forsøg iterativ udtrækning svarende til Python's `json.JSONDecoder().raw_decode`. Qt 5.6-ækvivalenten er `QJsonParseError::offset` (siden Qt 5.0): positionen hvor parse fejlede = slutningen af første komplette objekt.

### Udleveringsform: strøm ●

Dette underafsnit besvarer ét spørgsmål: om et objekt gøres tilgængeligt for
kalderen i det øjeblik, det er udtrukket, eller først når linjen er behandlet
til ende. **Valgt: strøm ●** — objektet udleveres ved udtrækket, også når
linjen senere forkastes.

Valget er ikke en ny beslutning: den komplette algoritme nedenfor udleverer
allerede ved hvert vellykket udtræk (`udlever objekt; return`,
`udlever obj1-objekt`) og har ingen opsamling. Underafsnittet fastslår, at
algoritmen er normativ, og at de sætninger i §4, §6 og §8, der udsagde det
modsatte, var fejl — disse er rettet herefter. §7's beskrivelse af linje 1
udsagde ikke et kontrolflow; den var faktuel forkert og er rettet i B7.

Afgrænsning: underafsnittet ændrer ikke «Vedtaget strategi», ikke de tre
regeloverskrifter og ikke rækkevidden af deres ●-mærker. Ordet «forkast»
beholder sin betydning; at det dækker to hændelser (før og efter
udlevering), afgøres ikke her.

### Den komplette algoritme (som skal kodes) ●

```
BUFFER = QByteArray (akkumulerer rå bytes fra QProcess::readyRead)
MAX_LINE = 8 MiB

ved hver read:
  BUFFER += nye bytes
  hvis BUFFER.size() > MAX_LINE og BUFFER ikke indeholder '\n':
    → A6b: eksplicit transport-fejl, luk agent, stop
  loop:
    pos = BUFFER.indexOf('\n')
    hvis pos == -1: break (vent på flere bytes)
    linje = BUFFER.left(pos)
    BUFFER = BUFFER.mid(pos + 1)
    hvis linje.size() > MAX_LINE:
      → A6a: log «linje forkastet (>8 MiB)», fortsæt loop (næste linje)
    ellers: parseLinje(linje)   // se nedenfor

parseLinje(linje: QByteArray):
  rest = linje
  loop:
    doc, err = fromJson(rest)
    hvis OK: udlever objekt; return
    hvis err != ExtraData: log + return (forkast linje)          // regel 1;
               gælder også senere gennemløb; udleverede objekter tilbagekaldes
               ikke
    n = err.offset
    hvis n <= 0: log + return (forkast)                          // regel 3
    obj1 = rest.left(n)
    hvis fromJson(obj1) fejler: log + return (forkast HELE)      // regel 2
    udlever obj1-objekt
    rest = rest.mid(n)
    hvis rest.trimmed().isEmpty(): return
    // loop igen på resten
```

### Regel 1 — kun `ExtraData` giver split-forsøg ●

`fromJson(rest)` fejler: hvis fejlen ikke er `ExtraData` → log + forkast linjen, færdig; udleverede objekter tilbagekaldes ikke. Ved første gennemløb er intet udleveret; ved andet gennemløb og frem kan objekter allerede være udleveret. Hvis `ExtraData` → gå til split.

**Begrundelse ●:** `QJsonParseError::offset` er kun meningsfuld som objektgrænse ved `ExtraData` (parseren læste et komplet objekt, så fandt mere). Ved alle andre fejl (`UnterminatedString`, `ExpectingValue`, `IllegalValue`…) peger offset midt i noget ugyldigt — `linje[0:offset]` ville ikke selv parse, og resten ville være meningsløs.

### Regel 2 — valider første objekt; ved fejl forkast HELE linjen ●

Split ved offset n: `obj1 = linje[0:n]`; hvis `fromJson(obj1)` fejler → log + forkast **hele** linjen som kilde til videre udtrækning, bryd løkken. Allerede udleverede objekter tilbagekaldes ikke. Ellers udlever obj1-objektet, fortsæt med `linje[n:]`.

**Begrundelse ●:** Uden reglen kører løkken videre på resten efter et ugyldigt første objekt — og kan gentage fejlen i tavshed eller miste meddelelser. Verificeret på `fixture-trin-e-ugyldig.ndjson` linje 1: linjen giver `[10, None, 'FEJL']` — to parsebare objekter før skraldet (id=10 og et id-løst objekt `{"ikke":"gyldig json"}`), tredje udtræk fejler med en fejl, der ikke er ExtraData, og rammer returstenen i regel 1 anvendt på resten → **hele linjen forkastes som kilde til videre udtrækning**. De to objekter blev udtrukket og udleveret, før fejlen indtraf, og tilbagekaldes ikke. Det id-løse mellemobjekt er ikke en parser-fejl — det er fixture'ens pointe: et gyldigt objekt udleveres, når det er udtrukket, uanset at linjen derefter forkastes som helhed.

### Regel 3 — værn mod nul-fremgang (uendelig løkke) ●

Hvert split skal give n > 0; hvis n == 0 → log + forkast, bryd løkken.

**Begrundelse ●:** En parser-fejl uden fremgang ville ellers loope uendeligt på samme linje.

## 5 · A6 — buffergrænse: 8 MiB, to tilstande ●

*(Besluttet i `analyse-a6-begrundet-forkast-ikke-draeb.md`, `804b4ae`; begrundelsen nedenfor er konsolideret fra samme fil. Status: ◐ delvist begrundet — se note.)*

**Specifikation ●:**

| Parameter | Værdi | Begrundelse |
|---|---|---|
| `ACP_MAX_LINE_BYTES` | **8 MiB (8 388 608 bytes)** | 2× margin over største kendte legitime linje (~4 MB escaped ved replay) — se beregning nedenfor |
| Måling | Akkumuleret bufferstørrelse pr. linje, ikke pr. read | En linje kan komme i mange små reads |
| Kvota-tjek | Placér tjekket **før** hver append til linje-bufferen — ikke efter | En append må aldrig overskride grænsen uden at blive fanget |

**To forskellige A6-tilstande ●:**

| Tilstand | Udgangspunkt | Reaktion |
|---|---|---|
| **A6a — komplet linje over grænsen** | Linje med `\n` fundet, men længde > 8 MiB | Log «linje forkastet, over 8 MiB», kassér linjen, fortsæt med næste. Forbindelsen består |
| **A6b — akkumuleret buffer over grænsen uden `\n`** | Ingen `\n` i buffer, størrelse > 8 MiB | Log «buffer over 8 MiB uden linjeskift — protokolkorruption», eksplicit transport-fejl, luk agenten (procesgenstart ved næste session) |

A6a = tab af én meddelelse. A6b = tab af sessionen, men kun når synkronisering er umulig: strømmen er selv-synkroniserende på `\n`, så efter en forkastet linje ved parseren præcis, hvor næste linje starter — men en buffer der vokser uden `\n` kan ikke synkroniseres igen.

**Begrundelse af værdien ● — hvad kan én wire-linje faktisk fylde?**

Den største linje på ACP-wire kommer fra `tool_call`/`tool_call_update` med `rawOutput` (råt output fra et tool-kald, `CompletedEffectState.output`) og akkumulerede `content[]`-deltaer. Størrelsen afhænger af tool'ets egen cap:

| Tool | Cap-kilde | Værdi |
|---|---|---|
| `bash` | `BashToolConfig.max_output_bytes` | default 16 000 bytes (stdout + stderr hver) |
| `grep` | `GrepToolConfig.max_output_bytes` | default 64 000 bytes |
| `read_file` | `was_truncated` + `max_bytes`-param | linje/byte-begrænset |
| hooks | `_MAX_HOOK_OUTPUT_BYTES` | 1 000 000 bytes |
| sandbox-helper | `MAX_OUTPUT_BYTES` | 512 KiB (head+tail) |

Største kendte enkelt-output: **~1 MB** (hook output). Bemærk: ACP `rawOutput` er `Optional[Any]` — **ingen protokol-garanti** for størrelse; og harness-pakken har ingen `max_output_bytes` i sin Python-kode (caps ligger i Rust-kernen der).

JSON-escaping: Python `json.dumps` escaper kontroltegn som `\u00XX` (6 bytes pr. tegn) og `\n` → `\\n` (2 bytes). Worst case ~6× for binær-agtig tekst, realistisk ~2×.

```
Beregning ●:
Største realistiske linje = rawOutput (1 MB hook, worst case) + content-deltaer (samme output igen, 1 MB)
                           + JSON-escaping (realistisk 2×, worst case 6×)
                           ≈ 2 MB råt → ~4 MB escaped (worst case ~12 MB)
Helt sikker øvre grænse for LEGITIME linjer: ~2 MB råt → ~4 MB escaped
Replay-linjer (session-replay bærer både content[] og output): ~4 MB escaped
```

**Valgt grænse: 8 MiB** (ikke 4, ikke 32): 2× margin over største kendte legitime linje (~4 MB escaped ved replay); 8× margin over normal drift (største målte cap 1 MB hook → ~2 MB escaped); stadig langt under hvad der kan tømme en telefon (en enkelt 8 MB `QByteArray` er ingenting på en Jolla med flere GB RAM); værn mod **protokolkorruption** (uendelig løkke uden `\n`, eller én besked der fylder hundrede MB) — ikke mod legitime beskeder.

**Status ◐ delvist begrundet:** Caps for bash/grep/hooks er kodelæste ●, men harness' Rust-kernede caps er ikke læsbare fra Python-sdist'en. 8 MiB er én konstant; **hæves kun ved en målt større legitim linje på telefonen**.

## 6 · EOF-håndtering ●

Ved `QProcess::finished` (agentens proces er gået ned):

```
hvis BUFFER ikke er tom:
  // delvis linje uden \n — forventet ved afbrydelse
  forsøg parseLinje(BUFFER)
  hvis den sidste del fejler: log «afbrudt ved EOF» (ikke en fejl-tilstand)
  er der udleveret objekter under forsøget, tilbagekaldes de ikke — de to
  grene kan indtræffe samtidigt
BUFFER.clear()
```

Præcisering (lukker en tvetydighed i før-kode-review'en): «sidste linje uden `\n` behandles som komplet» betyder **forsøges parset** — ikke «antages komplet uden parse». En afbrudt linje (typisk `UnterminatedString` midt i objektet) logges som «afbrudt ved EOF» og er ikke en fejl-tilstand.

## 7 · Fixtures ● (gældende testdata — uændrede)

| Fil | Størrelse | Indhold | Slutning | Teststi(er) |
|---|---|---|---|---|
| `fixture-trin-e.ndjson` | 4 587 bytes | 13 komplette linjer + 1 delvis (id=6, 28 bytes) | **uden** `\n` | 13 normale + **EOF-sti** (afbrudt ved procesdød) |
| `fixture-trin-e-sammenklaebning.ndjson` | 275 bytes | linje 1: to objekter uden `\n` mellem dem (id=7, id=8); linje 2: delvis (id=9, 28 bytes) | **uden** `\n` | **ExtraData-split** + **EOF-sti** |
| `fixture-trin-e-ugyldig.ndjson` | 237 bytes | linje 1: to gyldige objekter (id=10 + id-løst `{"ikke":"gyldig json"}`) + ét `}` skrald (`Extra data`, resten ugyldig); linje 2: gyldigt + `}}}`; linje 3: normalt; linje 4: delvis (id=13) | **uden** `\n` | **regel 1's retursti** (forkast hele linjen) + EOF-sti |

Alle tre filer slutter uden `\n`, så EOF-stien er entydig; `UnterminatedString`-stien dækkes af hovedfixturens delvise linje.

Hovedfixturens 13 linjer (målt/afledt indhold): `initialize`-svar (id=1, `protocolVersion: 1`), `session/new`-svar (id=2, `sessionId` + `configOptions`), `agent_message_chunk`, `tool_call` (`toolCallId` bevaret), `usage_update` `used:0` (L5), `tool_call_update` (`rawOutput` bevaret), `usage_update` `used:12345, size:800000` (N8), `request_permission` (heltals-id 0), `config_option_update` (R37), fejl `-32603` + `data.code:"3051"`, svar `stopReason:"cancelled"` med nul-usage, ukendt `sessionUpdate` («fremtidig_variant» — tolerance), svar `stopReason:"end_turn"` med reel usage; linje 14 delvis (id=6).

## 8 · Testplan — endelig ●

Parser-niveau (normative krav — hvad fortolkeren skal):

| Fixture | Linje | Forventet | Regel/sti |
|---|---|---|---|
| hoved | 1–13 | ● 13 objekter udleveret | normal |
| hoved | 14 (delvis, id=6) | ◐ EOF: «afbrudt ved EOF», ikke udleveret, ikke crash | EOF-sti |
| sammenklæbning | 1 (id=7+8) | ● **begge** udleveret | ExtraData-split |
| sammenklæbning | 2 (delvis, id=9) | ◐ EOF: «afbrudt ved EOF» | EOF-sti |
| ugyldig | 1 (id=10 + id-løst objekt + skrald) | ◐ id=10 **og** det id-løse objekt udleveret, derefter **hele linjen forkastes** | regel 1's retursti |
| ugyldig | 2 (id=11 + `}}}`) | ◐ id=11 udleveret, derefter **hele linjen forkastes** | regel 1's retursti |
| ugyldig | 3 (id=12) | ● udleveret | normal (efter forkastede) |
| ugyldig | 4 (delvis, id=13) | ◐ EOF: «afbrudt ved EOF» | EOF-sti |
| syntetisk A6a | linje på 9 MiB med `\n` | ◐ linje forkastes, logges; næste linje parses normalt; forbindelse består | A6a |
| syntetisk A6b | 9 MiB uden `\n` i buffer | ◐ transport-fejl, agent lukkes; ingen crash, ingen uendelig vækst | A6b |

**Samlede kriterier ●:** 19 succes-udleveringer (13 + 2 + 1 = 16 som før, plus 2 udleveret på ugyldig linje 1 og 1 på ugyldig linje 2 før forkastningen: 16 + 3 = 19), 6 forventede forkastninger/EOF (ikke crashes; tre EOF-rækker — hoved 14, sammenklæbning 2, ugyldig 4 — plus to forkastelsesrækker, ugyldig 1 og ugyldig 2, plus syntetisk A6a; A6b er transport-fejl og tælles ikke), 0 uendelige løkker, 0 crashes, A6a ved komplet linje >8 MiB (forkast, forbindelse består), A6b ved buffer >8 MiB uden `\n` (transport-fejl, luk agent). Tallene tælles på to akser: ugyldig linje 1 og 2 indgår i begge, så 19 + 6 er ikke antallet af udfald.

Applikations-niveau (kontekst — ikke parser-krav, men forventet adfærd i laget over, når Trin E er kodet): `usage_update` med `used:0` filtreres (L5); `size` mærkes «kompaktionsgrænse» (N8); heltals-id bevares til permission-svar (A9); `config_option_update` erstatter lokale `configOptions` (R37); ukendt `sessionUpdate` logges og ignoreres.

Denne testplan **afløser** den tidligere i `analyse-fixture-rettelse-objektgraenser.md` (15 succes-parses + 2 EOF-fejl, før tredje fixture) — tallet steg ved tredje fixture (`-ugyldig`): 19 udleveringer (16 + 2 + 1: to objekter på ugyldig linje 1, ét på ugyldig linje 2, udleveret før forkastningen) + 6 forkastninger/EOF (tre EOF-rækker + to forkastelsesrækker + syntetisk A6a; A6b ikke medregnet).

## 9 · Konsolideringsnoter og revisionsangivelse

**Kilder (alle på `vibe/vibe-acp-maalejournal-ea9aca` ved `a58804b`):**

- `analyse-parser-spec-komplet-a6-buffer.md` — algoritme, tre regler, fixtures, endelig testplan, A6 (efter runde 11-kropsrettelsen: 8 MiB, A6a/A6b)
- `analyse-a6-begrundet-forkast-ikke-draeb.md` — A6-beslutning + begrundelse (caps, margin, escaping, replay)
- `analyse-fixture-rettelse-objektgraenser.md` — offset-beslutning (tre strategier vurderet), fixture-rettelse, ældre testplan
- `gennemgang-journal-foer-kode.md` — wire-format-fakta, UTF-8-regler, fixture-forslag; §5's buffer-forslag lukket af A6
- `README.md` — peg (opdateret i samme commit)
- `fixture-trin-e.ndjson` — prøvefil (indhold verificeret: 14 dele, sidste delvis uden `\n`, 4 587 bytes)

**Rettelser undervejs (konsolideringen ændrer ingen beslutning, men bringer to ældre formuleringer i overensstemmelse):**

1. Før-kode-review'ens §5 «ingen antagelse om maksimal linjelængde» var et forslag fra før A6-beslutningen — **lukket af A6 (8 MiB)**; erklæret eksplicit i §5, så review'en ikke læses som gældende for buffer.
2. Før-kode-review'ens «sidste linje uden `\n` behandles som komplet» præciseret i §6: forsøges parset; afbrudt logges, ikke fejl.

**Revisionsangivelse ●:** Dette dokument er skrevet mod revision `a58804bc342c595972af7b4ccd48854d1d4130d8` (runde 11). Når kilderne senere ændres, stedfæstes dette dokument mod denne revision; ændringer i dette dokument får nye commits med `docs(vibe-acp):` og runde-nummer.

**Status ●:** Gældende normativ specifikation for Trin E. Konsoliderer fem definitionsfiler; erstatter `analyse-parser-spec-komplet-a6-buffer.md` som gældende (den bevares som revisionsspor). **Ingen arkitekturvalg genåbnet** — A6 (8 MiB, A6a/A6b, ◐ delvist begrundet) og offset-strategien gældende uændret. Trin E kan kodes direkte mod dette dokument + de tre fixtures.
