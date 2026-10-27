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

## A6 — buffergrænse ● (navngivet, hård, eksplicit fejl)

Relæet har ret: «ingen antagelse om maksimal linjelængde» er i praksis en ubegrænset buffer, og én stor `rawOutput`-linje (kompaktér-turn) kan tømme telefonen for hukommelse.

**Specifikation ●:**

| Parameter | Værdi | Begrundelse |
|---|---|---|
| `ACP_MAX_LINE_BYTES` | **4 MiB (4 194 304 bytes)** | rigeligt til selv store `tool_call_update` med `rawOutput`; Sailfish-telefoner har begrænset RAM |
| Overskridelse | **eksplicit fejl** — stop læsning, log, signalér transport-fejl, luk agenten ned (procesgenstart ved næste session) | ikke stille afkortning (ville korrumpere JSON), ikke ubegrænset vækst |
| Måling | akkumuleret bufferstørrelse pr. linje, ikke pr. read | en linje kan komme i mange små reads |

**Kvota-tjek placeres før hver append** til linje-bufferen — ikke efter.

## Den komplette algoritme ● (som skal kodes)

```
BUFFER = QByteArray (akkumulerer rå bytes fra QProcess::readyRead)
MAX_LINE = 4 MiB

ved hver read:
  BUFFER += nye bytes
  loop:
    pos = BUFFER.indexOf('\n')
    hvis pos == -1:
      hvis BUFFER.size() > MAX_LINE: → A6-fejl (eksplicit, stop, luk agent)
      break (vent på flere bytes)
    linje = BUFFER.left(pos)
    BUFFER = BUFFER.mid(pos + 1)
    hvis linje.size() > MAX_LINE: → A6-fejl
    parseLinje(linje)   // se nedenfor

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
| syntetisk | linje > 4 MiB | ○ A6-fejl: eksplicit fejl, agent lukkes | A6 |

**Samlede kriterier ●:** 16 succes-udleveringer (13 + 2 + 1), 4 forventede forkastninger/EOF (ikke crashes), 0 uendelige løkker, 0 crashes, A6-fejl ved >4 MiB.

## Status ●

Parserens kerne er nu **fuldt specificeret**: byte-akkumulering, `
`-split, ExtraData-offset-loop med tre sikkerhedsregler, EOF-håndtering, A6 buffergrænse. Ingen arkitekturvalg genåbnes. Trin E kan kodes direkte mod denne spec + de tre fixtures.
