# A6 — begrundet grænseværdi og forkast-ikke-dræb ● beslutning

Relæet havde to indvendinger: 4 MiB var gættet uden måling, og reaktionen (luk agenten) var for drastisk. Begge adresseres her med kodelæsning som grundlag.

## (a) Begrundelse af grænseværdien — hvad kan én wire-linje faktisk fylde? ●

Den største linje på ACP-wire kommer fra `tool_call`/`tool_call_update` med `rawOutput` (`session_updates.py:345,383`). Jeg sporede, hvad der kan stå i det felt:

**`rawOutput` bærer `CompletedEffectState.output`** (`session_updates.py:658-667`) — det rå output fra et tool-kald. Størrelsen afhænger af tool'ets egen cap:

| Tool | Cap-kilde | Værdi |
|---|---|---|
| `bash` | `BashToolConfig.max_output_bytes` (`bash.py:396-398`) | **default 16 000 bytes** (stdout + stderr hver) |
| `grep` | `GrepToolConfig.max_output_bytes` (`grep.py:23-25`) | **default 64 000 bytes** |
| `read_file` | `read_file.py:39` `was_truncated` + `vibe/utils/io.py:236` | linje/byte-begrænset (`max_bytes` param) |
| hooks | `_foreign_hooks.py:364` `_MAX_HOOK_OUTPUT_BYTES` | **1 000 000 bytes** |
| sandbox-helper | `_sandbox_helper.py:64` `MAX_OUTPUT_BYTES` | **512 KiB** (head+tail) |

**Største kendte enkelt-output: ~1 MB** (hook output). Bemærk: ACP `rawOutput` er `Optional[Any]` (`schema.py:4999-5002`) — **ingen protokol-garanti** for størrelse, og harness-pakken (`mistralai_vibe_local_harness`) har **ingen** `max_output_bytes` i sin Python-kode (søgt, ikke fundet) — caps ligger i Rust-kernen der.

**Andre store felter på samme linje:**

- `content[]` — output-deltaer (`_effect_progress_updates`, `session_updates.py:355-383`), akkumuleret indtil terminal state. Kan vokse til samme størrelse som `rawOutput`.
- `_meta` — små (stats, tool_name).
- Kompaktér-turn: `compact_start_update` (`utils.py:190`) sender kun en lille «Compacting…»-tekst — **ikke** råt output. Kompaktions-**svaret** kommer som normale `agent_message_chunk`-updates, hver lille.

**JSON-escaping:** Python `json.dumps` escaper kontroltegn som `\u00XX` (6 bytes pr. tegn) og `\n`→`\\n` (2 bytes). Worst case ~6x for binær-agtig tekst.

**Beregning ●:**

```
Største realistiske linje = rawOutput (1 MB hook, worst case) + content-deltaer (samme output igen, 1 MB)
                          + JSON-escaping 2x på den rå tekst (worst case 6x, realistisk 2x)
                          ≈ 1 MB + 1 MB, escaped ≈ 4-12 MB
Helt sikker øvre grænse for LEGITIME linjer: ~2 MB råt → ~4 MB escaped
```

**Valgt grænse: `ACP_MAX_LINE_BYTES = 8 MiB`** (ikke 4, ikke 32):

- 2x margin over den største kendte legitime linje (~4 MB escaped)
- Stadig langt under hvad der kan tømme en telefon (Jolla Phone 2026 har flere GB RAM; en enkelt 8 MB `QByteArray` er ingenting)
- Værn mod **protokolkorruption** (uendelig løkke uden `\n`, eller en enkelt besked der fylder hundrede MB) — ikke mod legitime beskeder

**Status:** ○ delvist begrundet — caps for bash/grep/hooks er kodelæste ●, men harness' Rust-kernede caps er ikke læsbare fra Python-sdist'en. 8 MiB giver 2x margin over det største målte/kodelæste output. Hvis en måling på telefonen viser en større legitim linje, hæves grænsen — den er én konstant.

## (b) Reaktion ved overskridelse — forkast linjen, behold forbindelsen ●

**Beslutning: log + forkast linjen, behold forbindelsen.**

Begrundelse:

1. **En over-grænse-linje er ikke nødvendigvis protokolkorruption.** En enkelt besked kan være enorm uden at resten af strømmen fejler. At dræbe agenten koster hele sessionen; at forkaste én linje koster én meddelelse.
2. **Strømmen er selv-synkroniserende på `\n`.** Efter en forkastet linje ved parseren præcis, hvor næste linje starter — ingen desynkronisering.
3. **Den farlige korruption er en linje uden `\n` der vokser uendeligt.** Den håndteres anderledes: hvis bufferen vokser uden at finde `\n`, er det samme A6-fejl, men her **er** dræb berettiget — for strømmen kan ikke synkroniseres igen uden at finde næste `\n`.

**To forskellige A6-tilstande ●:**

| Tilstand | Udgangspunkt | Reaktion |
|---|---|---|
| **A6a — komplet linje over grænsen** | Linje med `\n` fundet, men længde > 8 MiB | Log «linje forkastet, over 8 MiB», kassér linjen, fortsæt med næste. Forbindelsen består |
| **A6b — akkumuleret buffer over grænsen uden `\n`** | Ingen `\n` i buffer, størrelse > 8 MiB | Log «buffer over 8 MiB uden linjeskift — protokolkorruption», eksplicit transport-fejl, luk agenten (procesgenstart ved næste session) |

A6a = tab af én meddelelse. A6b = tab af sessionen, men kun når synkronisering er umulig.

## Præciseret testforventning for `ugyldig` linje 1 ●

Relæets nuance accepteret: linje 1 giver `[10, None, 'FEJL']` — **to** parsebare objekter før skraldet (id=10 og et id-løst objekt `{"ikke":"gyldig json"`), ikke «id=10 + skrald». Korrekt forventning:

> Linje 1: offset-loop udtrækker id=10 (gyldigt), så `{"ikke":"gyldig json"` (id-løst, parsebart indtil `}}`), tredje udtræk fejler → **regel 2: hele linjen forkastes, ingen af de to objekter udleveres**.

Det id-løse mellemobjekt er **ikke** en parser-fejl — det er fixture'ens pointe: selv gyldige objekter før skrald udleveres ikke, når linjen som helhed forkastes.

## Opdateret algoritme (A6-delen) ●

```
ved hver read:
  BUFFER += nye bytes
  hvis BUFFER.size() > MAX_LINE og BUFFER ikke indeholder '\n':
    → A6b: eksplicit transport-fejl, luk agent, stop
  loop:
    pos = BUFFER.indexOf('\n')
    hvis pos == -1: break
    linje = BUFFER.left(pos)
    BUFFER = BUFFER.mid(pos + 1)
    hvis linje.size() > MAX_LINE:
      → A6a: log «linje forkastet (>8 MiB)», fortsæt loop (næste linje)
    ellers: parseLinje(linje)
```

## Opdateret testplan — A6-tilføjelser ●

| Test | Udgangspunkt | Forventet |
|---|---|---|
| A6a | Linje på 9 MiB med `\n` | Linje forkastes, logges; næste linje parses normalt; forbindelse består |
| A6b | 9 MiB uden `\n` i buffer | Transport-fejl, agent lukkes; ingen crash, ingen uendelig vækst |
| Præciseret `ugyldig` linje 1 | `[10, None, 'FEJL']` | 0 objekter udleveret; hele linjen forkastet (regel 2) |

## Status ●

A6 er nu begrundet (8 MiB, 2x margin over største kendte legitime linje) og reaktionen differentieret (A6a forkast-én-linje / A6b dræb-ved-desynk). Parser-spec'en er komplet. Ingen arkitekturvalg genåbnes.
