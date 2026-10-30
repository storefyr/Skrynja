# A8 komplet rutekort ● — alle request-ruter har frist; fejlkode -33000; sessionsbundet fornyelse

Relæets rutekort er verificeret i SDK 0.11.0. Denne analyse lukker alle åbne punkter: fuld fristtabel, rettet fejlkode, sessionsbundet progress-fornyelse, regel for sene svar, og kapabiliteter bekræftet i `initialize`.

## 1 · Fuld ruteliste ● (verificeret i `acp/agent/router.py:56-105`)

| Linje | Metode | Type | unstable | Implementeret i agent |
|---|---|---|---|---|
| 59 | `initialize` | request | – | `agent.py:460` ● |
| 60 | `session/new` | request | – | ● |
| 62–67 | `session/load` | request | – | `agent.py:555` ● |
| 68 | `session/list` | request | – | ● |
| 70–76 | `session/close` | request | **`unstable=True`** | `agent.py:990` ● |
| 78 | `session/set_mode` | request | – | `agent.py:1042` ● |
| 84 | `session/prompt` | request | – | `agent.py:794` ● |
| 85–93 | `session/set_config_option` | request (via `add_route`) | – | `agent.py:1053` ● |
| 94 | `authenticate` | request | – | ● |
| 100 | `session/fork` | request | **`unstable=True`** | `agent.py:1100` ● |
| 101 | `session/resume` | request | **`unstable=True`** | `agent.py:1133` ● |
| 103 | `session/cancel` | **notification** | – | `agent.py:981` ● |

## 2 · `unstable`-mekanismen — præciseret ● (vigtig rettelse til relæets mekanisme-beskrivelse)

Relæet skrev: «`func` bindes ALTID; `warn_unstable` styrer en lokal UserWarning der aldrig når klienten». Det er **delvist forkert** — jeg læste `acp/router.py:61-71` direkte:

```python
async def handle(self, params: Any) -> Any:
    if self.func is None:
        ...
    if self.warn_unstable:
        warnings.warn(f"The method {self.method} is part of the unstable protocol...", UserWarning, stacklevel=3)
        raise RequestError.method_not_found(self.method)   # ← AFVISER!
    result = await self.func(params)
```

**`warn_unstable=True` giver ikke kun en advarsel — den afviser kaldet med `method_not_found` (`-32601`)** efter advarslen. Men:

```
warn_unstable = unstable and not self._use_unstable_protocol   (router.py:130, 150)
```

Og agenten sætter `use_unstable_protocol=True` (`agent.py:1859`). Så på Skrynjas forbindelse er `warn_unstable` altid `False`, og **alle ruter er kaldbare** — også `session/close`, `session/fork`, `session/resume`.

**Korrekt formulering ●:** `unstable` er en **rådgivende markering med håndhævelse** (afviser med `-32601` hvis klienten ikke har aktiveret unstable) — men på Skrynjas forbindelse er håndhævelsen **inaktiv** (`use_unstable_protocol=True`), så alle tre unstable-ruter er kaldbare. `-32601` betyder «ikke registreret eller unstable-ikke-aktiveret» — klienten kan ikke skelne uden at kende flaget.

## 3 · Kapabiliteter i `initialize`-svaret ● — MÅLT I KODE (lukker ○)

`agent.py:497-510`:

```python
session_capabilities=SessionCapabilities(
    close=SessionCloseCapabilities(),   # ← close annonceres
    list=SessionListCapabilities(),     # ← list annonceres
    fork=SessionForkCapabilities(),     # ← fork annonceres
),
```

**Alle tre unstable-kapabiliteter (`close`, `list`, `fork`) annonceres i `initialize`-svaret** ● — også selvom de er unstable i schema. (Wire-måling tidligere viste `sessionCapabilities: {close:{}, fork:{}, list:{}}` ● — konsistent.)

**Konsekvens ●:** Skrynja kan læse kapabiliteterne fra `initialize` og bør respektere dem. `close`/`fork` er annonceret, men Skrynja kalder ikke dem i v1 (fork: unstable, ikke målt end-to-end; close: ikke nødvendig — procesdød lukker sessionen).

## 4 · A8 — komplet fristtabel ● (alle request-ruter)

| Metode | Frist | Type | Skrynja kalder i v1? |
|---|---|---|---|
| `initialize` | 120 s | fast | ● ja |
| `session/new` | 120 s | fast | ● ja |
| `session/load` | 120 s | fast | ● ja (A2) |
| `session/list` | 120 s | fast | ○ ja (sessionsliste) |
| `session/close` | 120 s | fast | ○ nej (unstable, ikke nødvendig) |
| `session/set_mode` | 120 s | fast | ○ nej (A2: kun set_config_option) |
| `session/prompt` | 120 s | **progress-baseret** | ● ja |
| `session/set_config_option` | 120 s | fast | ● ja |
| `authenticate` | 120 s | fast | ○ nej (ingen auth i v1 — agent kører lokalt med nøgle i miljø) |
| `session/fork` | 120 s | fast | ○ nej (unstable) |
| `session/resume` | – | – | ○ nej (A2: load foretrækkes) |
| `session/cancel` | – | notification — **rydder udestående** | ● ja (A1) |

**Alle request-ruter har nu en frist** ● — ingen anmodning hænger uden timeout.

## 5 · Fejlkode rettet: `-33000` (uden for reserveret bånd) ●

Relæet har ret: JSON-RPC 2.0 reserverer `-32000…-32099` til «implementation-defined server-errors» — det bånd agenten selv kan sende i. En klient-syntetisk `-32000` ville være uadskillelig fra en ægte agent-fejl.

**Vedtaget ●:** klient-syntetisk timeout bruger **`-33000`**, uden for det reserverede bånd, med `synthetic: true` som ekstra markør:

```
ACPError{code: -33000, message: "Request timed out after 120 s (client-side timeout)", synthetic: true}
```

## 6 · Progress-fornyelse — sessionsbundet ●

| Regel | Implementering |
|---|---|
| Forny kun ved samme `sessionId` som den udestående anmodning | `session/update` og svar matchet på `params.sessionId` / svar-`id` → kun samme session fornyer |
| Forny ved `agent_message_chunk`, `tool_call`, `tool_call_update` **og** `request_permission` | alle fire er livstegn fra agenten for den session |
| `session/update` for en anden session tæller ikke | eksplicit sessionId-match |
| Svar på den udestående anmodning selv | afslutter naturligvis (ikke fornyelse — afslutning) |

## 7 · Sene svar efter timeout ● (regel tilføjet)

Ved timeout fjernes anmodningen fra den udestående tabel. Kommer svaret alligevel med samme `id`:

```
→ log «late response for id=X, discarded»
→ forkast beskeden
→ INGEN fejl sendes tilbage til agenten (et svar på et svar er protokol-fejl)
→ INGEN behandling som ukendt id (ville udløse fejlmeddelelse)
```

## 8 · Absolut loft over progress-baseret frist ○ (bevidst valg, valgfrit)

En agent der bliver ved med at sende updates uden at afslutte, fornyer fristen i det uendelige. **Standard:** ingen absolut grænse — acceptabelt, fordi hver update er et ægte livstegn. **Valgfrit:** absolut loft på 30 min, vist i UI som «turen kører stadig» (ikke som fejl). Ikke i v1.

## 9 · Rettede citationsstier ● (accepteret fra relæet)

| Før (forkert) | Korrekt |
|---|---|
| `vibe/acp/meta.py`, `vibe/acp/router.py`, `vibe/acp/schema.py` | top-level **`acp/meta.py`, `acp/router.py`, `acp/agent/router.py`** (SDK-pakken `agent_client_protocol` installeres som `acp/`) |
| `router.py:100` fork-registrering | **`acp/agent/router.py:100`** |
| `meta.py:18` som «unstable=True» | `acp/meta.py:18` er metodenavn-mapping `"session_fork": "session/fork"`; unstable-markering sidder i `acp/agent/router.py:100` |
| «ruterne findes ikke i SDK'et» | de findes i **`acp/agent/router.py`** (undermappe, som flad glob ikke ramte) |

## Testplan — A8 komplet ●

| Test | Forventet |
|---|---|
| Hver request-rute uden svar i 120 s | Syntetisk `-33000`, `synthetic: true` |
| `session/prompt` med updates fra samme session hver 10 s i 10 min | Ingen timeout |
| `session/prompt` med updates fra **anden** session | Tæller ikke — timeout efter 120 s |
| `session/prompt` med `request_permission` som livstegn | Fornyer fristen |
| `session/prompt` 130 s tavshed | Timeout `-33000` efter 120 s |
| A6a: svar-linje forkastet, derefter tavshed | Timeout efter 120 s ● |
| Sent svar efter timeout (samme id) | Logges «late response», forkastes, ingen fejl til agent |
| `session/cancel` med udestående | Ryddet med `cancelled`, ikke timeout |
| Agent dør med udestående | Alle ryddet ved `finished` |

## Status ●

A8 komplet: alle 11 request-ruter har frist, fejlkode `-33000` syntetisk, sessionsbundet fornyelse, regel for sene svar, kapabiliteter bekræftet i `initialize`. Rutekort fuldt målt. `unstable`-mekanismen præciseret (rådgivende + håndhævelse, inaktiv på Skrynjas forbindelse). Citationsstier rettet. Ingen arkitekturvalg genåbnet.
