# R34 ● lukket på telefonen — én aktiv konfigurationsmappe

**Målt på telefonen (relæ-AI) ●:**

- `~/.config/org.skrynja/Skrynja/` — aktiv, senest rørt 25. okt.
- `~/.config/harbour-skrynja/` — rørt 21. okt., indeholdt en redigerbar `Skrynja.conf` (1360 bytes) **uden effekt**
- `diff` på nøglenavne før sletning: én nøgle kun i den gamle fil — `contextMessageLimit=0` («ingen grænse»); nøglen findes ikke i den aktive fil. Intet at overføre
- Skrynja kørte ikke (`pgrep` tom); kopi af begge mapper i `/tmp/skrynja-konf-backup-2026-10-27`; `~/.config/harbour-skrynja/` slettet

**○ Forbehold:** `/tmp` ryddes typisk ved genstart på Sailfish. Skal kopien bevares, flyttes den under `~/Documents` eller `~/.local/share`.

## Nummereringsafklaring ● (vigtig)

Relæ-beskeden kalder denne lukning «R42». I **vores** nummerering er R42 allerede optaget:

| ID | Vores betydning (vejkort/måleserie) | Relæets betydning i denne besked |
|---|---|---|
| R34 | hvilken `Skrynja.conf`-sti er aktiv | — |
| R42 | tom katalog er «authoritative» (R37/R42 arbejdsliste) | «den ekstra `Skrynja.conf` er fjernet» |

For at undgå to betydninger af samme ID kalder vi denne lukning **R34** (som den hører under — spørgsmålet var altid «hvilken sti er aktiv»). **R34 ● lukket endeligt.** R42 (tom katalog = fejl, ikke tom menu) står uændret.

## Konsekvens ●

Én aktiv konfigurationsmappe, ingen konkurrerende fil. Lag 2 og 3 har én utvetydig sti:

```
~/.config/org.skrynja/Skrynja/Skrynja.conf
```

- Lag 2: `acp/thinkingSuppressLow` (brugeroverride)
- Lag 3: `acp/thinkingSuppressLowLearned` (bekræftet læring)

## Åben ○ (UI, ikke-blokerende)

Om «Nulstil»-vejen for lag 3 skal ligge i indstillingerne eller som knap ved thinking-menuen — afgøres når UI-skitsen foreligger. **Blokerer ikke Trin E.**

## Status — alle arkitekturvalg låst ●

A1, A2, kun `session/set_config_option`, A7 som 3-lags tilstand med bekræftet læring, thinking-UI med «samme som High», N8/R12, L5, R37/R42, Trin E. R34 ● (én aktiv sti), R41 ◐ (flaggate). Ingen åbne ○ blokerer implementationen.
