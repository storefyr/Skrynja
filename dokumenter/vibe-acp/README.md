# vibe-acp målelab

Måleresultater og scripts fra arbejdet med vejkortet for Skrynja mod `vibe-acp`
(ACP-klient, jf. `../vejkort.md` Rev. 13).

## Indhold

| Fil | Beskrivelse |
|---|---|
| `maalejournal-runde1.md` | Første målerunde: M0.0, M0.1, M0.15, M0.14 (delvist) |
| `maalejournal-runde1-tilfoejelse.md` | Afslutning af M0.15/M0.1 + forberedelse af M0.16 |
| `maalejournal-runde2.md` | Anden målerunde: M0.0b (integritet), M0.1b (fuld frame), M0.16 (bro, halvt), versionsaudit |
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
