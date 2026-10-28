# Fixture rettet ● + beslutning: objektgrænser inden for en linje ●

**Relæets måling var korrekt ●:** den oprindelige fixture havde linje 14 med **to JSON-objekter på én linje** (157 bytes, `Extra data` ved char 128) — en sammenklæbning af `end_turn`-linjen og den delvise linje, fordi jeg havde kørt tilføj-kommandoen to gange uden at nulstille. Linje 15 (28 bytes, uafsluttet) var korrekt.

## Rettelse ●

| Fil | Indhold | Status |
|---|---|---|
| `fixture-trin-e.ndjson` | 13 komplette linjer + 1 delvis linje (28 bytes, ingen `\n`) i slutningen | ● genskabt og valideret |
| `fixture-trin-e-sammenklaebning.ndjson` | **navngivet testtilfælde:** linje 1 = to komplette objekter uden `\n` mellem dem (id=7, id=8); linje 2 = delvis linje (id=9, uafsluttet) | ● ny, valideret |

Hovedfixturen valideret: 13/13 komplette linjer parses som gyldig JSON; sidste emne er delvist uden newline.

## Beslutning: parser-strategi ved sammenklæbning ●

Relæet stillede det afgørende spørgsmål: en parser der kun deler på `\n` taber **begge** objekter på en sammenklæblet linje. Tre strategier:

| Strategi | Vurdering |
|---|---|
| Del kun på `\n`, forkast linjen ved parsefejl | ● enkel; taber to gyldige meddelelser — og agenten **kan** sende dem sammen (ingen flush-garanti mellem to svar) |
| Find objektgrænser (strengbevidst klamretælling) | ● robust; men egen tokenizer = egen fejlkilde; duplicerer hvad JSON-parseren allerede ved |
| **Del på `\n`, og ved «Extra data» forsøg iterativ `raw_decode`** | ● **vedtaget** — genbruger JSON-parseren, ingen egen tokenizer |

**Vedtaget: tredje vej ●.**

Python's `json.JSONDecoder().raw_decode(text)` returnerer `(objekt, slut-offset)` og stopper ved første komplette objekt — verificeret på sammenklæbnings-fixturen: `raw_decode` genfinder både id=7 og id=8 på den sammenklæbte linje.

**Qt 5.6-ækvivalent ○→●:** `QJsonDocument::fromJson` svarer ikke direkte til `raw_decode` — den fejler ved «Extra data» uden at give offset. Men Qt 5.6 har `QJsonParseError::offset` (tilføjet i Qt 5.0), som giver præcis det: **positionen hvor parse fejlede = slutningen af første objekt**. Så algoritmen i Qt 5.6:

```
1. Del buffer på \n → linjer
2. For hver komplet linje:
   a. QJsonDocument::fromJson(linje)
   b. Hvis OK → færdig
   c. Hvis fejl OG QJsonParseError::offset > 0:
        → første objekt = linje[0:offset] (valider ved fromJson — skal lykkes)
        → resten = linje[offset:] → gentag fra (a) med resten
   d. Hvis fejl OG offset == 0 (eller ugyldig) → linjen er skrald:
        log + forkast (tolerant parser: aldrig crash)
3. Ved EOF (procesdød): sidste delvise linje uden \n:
   → forsøg parse; hvis den fejler, er det forventet — log som «afbrudt ved EOF», ikke som fejl
```

**Bemærk ○:** `QJsonParseError::offset` er byte-offset i `fromJson`'s input. For ASCII-sikker wire (Python `ensure_ascii=True`) er byte-offset = char-offset. For rå UTF-8 i `_meta` kan de afvige — løsningen: kør objekt-split på **bytes** (`QByteArray`), konverter hvert fundet objekt til `QString::fromUtf8` før `fromJson`. Så er offset altid byte-korrekt.

## Testplan — opdateret med forventede udfald ●

| # | Fixture-linje | Forventet udfald |
|---|---|---|
| 1 | `initialize`-svar id=1 | ● parses, `protocolVersion == 1` |
| 2 | `session/new`-svar id=2 | ● parses, har `sessionId` + `configOptions` |
| 3 | `agent_message_chunk` | ● parses, notifikation (ingen id) |
| 4 | `tool_call` | ● parses, `toolCallId` bevaret |
| 5 | `usage_update` `used:0` | ● parses, **filtreres af L5** |
| 6 | `tool_call_update` | ● parses, `rawOutput.stdout` bevaret |
| 7 | `usage_update` `used:12345, size:800000` | ● parses, `size` mærkes «kompaktionsgrænse» (N8) |
| 8 | `request_permission` id=0 | ● parses, **heltals-id bevaret** (ikke streng) |
| 9 | `config_option_update` | ● parses, erstatter lokale configOptions (R37) |
| 10 | fejl `-32603` + `data.code:"3051"` | ● parses, `error.code` og `data.code` tilgængelige |
| 11 | `stopReason:"cancelled"` nul-usage | ● parses, usage filtreres af L5 |
| 12 | ukendt `sessionUpdate` | ● parses uden crash, logges, **ignoreres** (tolerance) |
| 13 | `stopReason:"end_turn"` | ● parses, reel usage bevares |
| 14 | delvis linje ved EOF | ○ **forventes at fejle** — logges som «afbrudt ved EOF», ikke crash |
| S1 | sammenklæbning linje 1 (id=7 + id=8) | ● **begge** objekter udtrækkes via offset-loop |
| S2 | sammenklæbning linje 2 (delvis, id=9) | ○ **forventes at fejle** ved EOF — samme som #14 |

**Testkriterie ●:** 13 komplette + 2 sammenklæbte objekter = **15 succesfulde parses**; 2 delvise linjer = **2 forventede EOF-fejl** (log, ikke crash); 0 crashes; 0 ufangne objekter.

## Praktiske forhold fra relæet — noteret, ikke blokerende for dokumentation

1. Fixturen ligger på sidegrenen, ikke `main` — relevant når B6-testen skal køre i bygget. **PR #18 skal merges** eller `dokumenter/vibe-acp/` kopieres til `main` først.
2. Arbejdstræet på `main` har 5 ucommittede ændringer — skal stash'es/committes før grenskift.

Begge er driftsforhold omkring merge, ikke omkring denne rettelse.

## Uændret låst ●

Alle arkitekturvalg. Denne beslutning tilføjer kun: parser-strategi (offset-loop) + fixture-rettelse + navngivet sammenklæbnings-tilfælde.

---

## Statusnote — historik ● (runde 12, relæ `83268bd1`)

Fixture-rettelsen og offset-beslutningen her er uændret og gældende; indholdet er konsolideret i `spec-trin-e-ndjson-parser.md` (§4, §7), som fra runde 12 er den gældende specifikation for Trin E. Testplanen i denne fil (15 succes-parses + 2 EOF-fejl) var før tredje fixture og er **afløst** af den endelige testplan i den nye spec §8 (16 udleveringer + 4 forkastninger/EOF). Denne fil bevares som historik og revisionsspor.
