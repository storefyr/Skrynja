# Parser-specifikation komplet ● — tre regler + A6 buffergrænse tilføjet

Relæet verificerede fixturen ● og påpegede tre manglende udgange i offset-algoritmen plus behovet for en hård buffergrænse (A6). Alle fire er nu specificeret. Dette lukker parserens kerne før koden skrives.

## Fixturer — endelig tilstand ● (alle verificeret lokalt)

| Fil | Indhold | Slutning | Teststi(er) |
|---|---|---|---|
| `fixture-trin-e.ndjson` | 13 komplette linjer + 1 delvis (id=6, 28 bytes) | **uden** `\n` | 13 normale + **EOF-sti** (afbrudt ved procesdød) |
| `fixture-trin-e-sammenklaebning.ndjson` | linje 1: to objekter uden `\n` (id=7, id=8); linje 2: delvis (id=9, 28 bytes) | **uden** `\n` (rettet — tidligere fejlagtigt `\n`-termineret) | **ExtraData-split** + **EOF-sti** |
| `fixture-trin-e-ugyldig.ndjson` | linje 1: gyldigt objekt + skrald (`Extra data`, resten ugyldig); linje 2: gyldigt + `}}}` (ekstra klammer); linje 3: normalt; linje 4: delvis (id=13) | **uden** `\n` | **regel 2** (ugyldig resten → forkast hele linjen) + EOF-sti |

**Præcisering fra relæet accepteret ●:** sammenklæbningsfilen var tidligere `\n`-termineret, hvilket gjorde dens delvise linje til en `UnterminatedString`-test (offset midt i objektet) — ikke en EOF-test. Nu slutter alle tre filer uden `\n`, så EOF-stien er entydig, og `UnterminatedString`-stien dækkes af hovedfixturens delvise linje (som også er `UnterminatedString`).

## De tre manglende regler ● (nu i algoritmen)

### Regel 1 — kun `ExtraData` giver split-forsøg

```
fromJson(linje) fejler:
  hvis error != ExtraData:   → log + forkast linjen, færdig
  hvis error == ExtraData:   → gå til split
```

**Begrundelse ●:** `QJsonParseError::offset` er kun meningsfuld som objektgrænse ved `ExtraData` (parseren læste et komplet objekt, så fandt mere). Ved alle andre fejl (`UnterminatedString`, `ExpectingValue`, `IllegalValue`…) peger offset midt i noget ugyldigt — `linje[0:offset]` ville ikke selv parse, og resten ville være meningsløs.

### Regel 2 — valider første objekt; ved fejl forkast HELE linjen

```
split ved offset n:
  obj1 = linje[0:n]
  hvis fromJson(obj1) fejler:  → log + forkast HELE linjen, bryd løkken
  ellers: accepter obj1, fortsæt med linje[n:]
```

**Begrundelse ●:** uden denne regel kører løkken videre på resten efter et ugyldigt første objekt — og kan gentage fejlen i tavshed eller miste meddelelser. Verificeret på `fixture-trin-e-ugyldig.ndjson` linje 1: første objekt (id=10) er gyldigt, resten (`{"ikke":"gyldig json"}}`) fejler — hele linjen skal forkastes, ikke kun resten.

### Regel 3 — værn mod nul-fremgang (uendelig løkke)

```
hvert split skal give n > 0:
  hvis n == 0:  → log + forkast, bryd løkken
```

**Begrundelse ●:** en parser-fejl uden fremgang ville ellers loope uendeligt på samme linje.

## A6 — buffergrænse ● (navngivet, hård, eksplicit fejl; differentieret reaktion)

Relæet har ret: «ingen antagelse om maksimal linjelængde» er i praksis en ubegrænset buffer, og én stor `rawOutput`-linje (kompaktér-turn) kan tømme telefonen for hukommelse. *(Kropsrettet i runde 11: denne sektion var forældet — den spec'ede 4 MiB og «luk agenten» ved enhver overskridelse. A6 er siden begrundet og besluttet i `analyse-a6-begrundet-forkast-ikke-draeb.md` (commit `804b4ae`, nyere end denne fils `8f9b952`): **8 MiB** med to differentierede tilstande A6a/A6b. Nedenstående er den gældende spec; begrundelsen står i A6-filen.)*

**Specifikation ● (gældende — jf. `analyse-a6-begrundet-forkast-ikke-draeb.md`):**

| Parameter | Værdi | Begrundelse |
|---|---|---|
| `ACP_MAX_LINE_BYTES` | **8 MiB (8 388 608 bytes)** | 2× margin over største kendte legitime linje (~4 MB escaped ved replay); caps målt: bash 16 000, grep 64 000, shell-standard 16 000, hook 1 MB, sandbox 512 KiB — 8× margin normal, 2× replay. Én konstant; hæves kun ved målt større legitim linje |
| Måling | akkumuleret bufferstørrelse pr. linje, ikke pr. read | en linje kan komme i mange små reads |

**To forskellige A6-tilstande ●:**

| Tilstand | Udgangspunkt | Reaktion |
|---|---|---|
| **A6a — komplet linje over grænsen** | Linje med `\n` fundet, men længde > 8 MiB | Log «linje forkastet, over 8 MiB», kassér linjen, fortsæt med næste. Forbindelsen består |
| **A6b — akkumuleret buffer over grænsen uden `\n`** | Ingen `\n` i buffer, størrelse > 8 MiB | Log «buffer over 8 MiB uden linjeskift — protokolkorruption», eksplicit transport-fejl, luk agenten (procesgenstart ved næste session) |

A6a = tab af én meddelelse. A6b = tab af sessionen, kun når synkronisering er umulig.

**Kvota-tjek placeres før hver append** til linje-bufferen — ikke efter.

## Den komplette algoritme ● (som skal kodes)

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
    hvis err != ExtraData: log + return (forkast linje)          // regel 1
    n = err.offset
    hvis n <= 0: log + return (forkast)                          // regel 3
    obj1 = rest.left(n)
    hvis fromJson(obj1) fejler: log + return (forkast HELE)      // regel 2
    udlever obj1-objekt
    rest = rest.mid(n)
    hvis rest.trimmed().isEmpty(): return
    // loop igen på resten

ved EOF (QProcess::finished):
  hvis BUFFER ikke er tom:
    // delvis linje uden \n — forventet ved afbrydelse
    forsøg parseLinje(BUFFER) — men forvent fejl
    hvis fejl: log «afbrudt ved EOF» (ikke en fejl-tilstand)
    hvis OK: udlever (en komplet linje nåede at komme igennem før døden)
  BUFFER.clear()
```

## Testplan — endelig ● (pr. fixture, pr. forventet udfald)

| Fixture | Linje | Forventet | Regel/sti |
|---|---|---|---|
| hoved | 1–13 | ● 13 objekter udleveret | normal |
| hoved | 14 (delvis, id=6) | ○ EOF: «afbrudt ved EOF», ikke udleveret, ikke crash | EOF-sti |
| sammenklæbning | 1 (id=7+8) | ● **begge** udleveret | ExtraData-split |
| sammenklæbning | 2 (delvis, id=9) | ○ EOF: «afbrudt ved EOF» | EOF-sti |
| ugyldig | 1 (id=10 + skrald) | ○ **hele linjen forkastes**, id=10 IKKE udleveret | regel 2 |
| ugyldig | 2 (id=11 + `}}}`) | ○ **hele linjen forkastes** | regel 2 |
| ugyldig | 3 (id=12) | ● udleveret | normal (efter forkastede) |
| ugyldig | 4 (delvis, id=13) | ○ EOF: «afbrudt ved EOF» | EOF-sti |
| syntetisk A6a | linje på 9 MiB med `\n` | ◐ linje forkastes, logges; næste linje parses normalt; forbindelse består | A6a |
| syntetisk A6b | 9 MiB uden `\n` i buffer | ◐ transport-fejl, agent lukkes; ingen crash, ingen uendelig vækst | A6b |

**Samlede kriterier ●:** 16 succes-udleveringer (13 + 2 + 1), 4 forventede forkastninger/EOF (ikke crashes), 0 uendelige løkker, 0 crashes, A6a ved komplet linje >8 MiB (forkast, forbindelse består), A6b ved buffer >8 MiB uden `\n` (transport-fejl, luk agent).

## Status ●

Parserens kerne er nu **fuldt specificeret**: byte-akkumulering, `\n`-split, ExtraData-offset-loop med tre sikkerhedsregler, EOF-håndtering, A6 buffergrænse (8 MiB, A6a/A6b — gældende værdier per `analyse-a6-begrundet-forkast-ikke-draeb.md`). Ingen arkitekturvalg genåbnes. Trin E kan kodes direkte mod denne spec + de tre fixtures.

---

## Kropsrettelse i runde 11 (in-place, noteret efter konventionen) ● — afklaring af gældende parser-spec (relæ `6fc943d2`)

Telefonen spurgte, hvilken fil der er den gældende parser-spec for Trin E, og pegede på to forhold: README peger både på denne fil og på `gennemgang-journal-foer-kode.md`, og denne fil angav 4 MiB mens A6-filen angav 8 MiB. Verificeret mod hele træet ved `c79a791`:

- **Denne fil er den gældende parser-spec ●** — præcis én fil med «parser» i navnet, ingen ældre eller nyere konkurrent. Algoritmen, de tre sikkerhedsregler, fixtures og testplan står uændret.
- **A6-delen var forældet ● — kropsrettet her:** 4 MiB → **8 MiB**; «luk agenten ved enhver overskridelse» → **A6a/A6b** (forkast én linje / luk kun ved desynk). A6-beslutningen (`804b4ae`, `analyse-a6-begrundet-forkast-ikke-draeb.md`) er nyere end denne fil (`8f9b952`) og autoritativ for A6; begrundelsen (caps, margin 8×/2×, replay ~4 MB escaped) står dér og citeres her.
- **`gennemgang-journal-foer-kode.md` er ikke parser-spec'en ●:** den er før-kode-review'en af målejournalen (`114bcf4`) — wire-format-fakta, UTF-8-regler, fixture-forslag. Den siger selv «ingen antagelse om maksimal linjelængde», hvilket A6 siden har lukket. README's backlog-peg («spec i `gennemgang-journal-foer-kode.md`») var misvisende — rettet i README.
- **Sidefund ●:** `gennemgang-journal-foer-kode.md` indeholdt ét NUL-byte (byte 3809, hvor der skulle stå `\u0007` i «escapes `\u0007`-form») — git/grep behandlede filen som binær. Rettet i runde 11; se note i den fil.
- **Mindre ●:** testplanens syntetiske A6-række bar ○ (tom cirkel, U+25CB) hvor projektet ellers bruger ◐ (halvfyldt, U+25D0) — erstattet sammen med rækken.

**Status efter runde 11 ●:** parser-spec'en gældende og konsistent med A6-beslutningen; README's peg korrekte. Ingen arkitekturvalg genåbnet.

---

## Statusnote — erstattet som gældende spec ● (runde 12, relæ `83268bd1`)

Fra og med runde 12 er dette dokument **ikke længere den gældende parser-specifikation for Trin E** — det er **erstattet af `spec-trin-e-ndjson-parser.md`**, der konsoliderer denne fil, `analyse-a6-begrundet-forkast-ikke-draeb.md`, `analyse-fixture-rettelse-objektgraenser.md` og `gennemgang-journal-foer-kode.md` i én selvstændig, normativ specifikation. Denne fil bevares uændret som revisionsspor (herunder runde 11-kropsrettelsen: 8 MiB, A6a/A6b). Alle «lukket»-erklæringer i denne fil gælder fortsat — konsolideringen genåbner intet. Trin E kodes mod den nye spec + de samme tre fixtures.
