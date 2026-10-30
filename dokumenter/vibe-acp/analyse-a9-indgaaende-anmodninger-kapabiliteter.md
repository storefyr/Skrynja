# A9 — indgående anmodninger: Skrynjas egen svartid ● + klient-kapabiliteter i `initialize` ●

Relæets sidste åbne punkt fra A8-komplettering: for **indgående** anmodninger (agent → klient) venter **agenten** på Skrynja. Uden en beslutning om Skrynjas egen svartid kan en langsom Skrynja efterlade agenten hængende — samme fejlklasse som A8, spejlet. Denne analyse verificerer klient-rutekortet og kapabilitets-gating, og fastlægger A9.

## 1 · Klient-rutekort verificeret ● (`acp/client/router.py:94-165`)

| Linje | Metode | Type | `optional` | unstable |
|---|---|---|---|---|
| 97 | `fs/write_text_file` | request | nej | – |
| 98 | `fs/read_text_file` | request | nej | – |
| 100 | `session/request_permission` | request | nej | – |
| 106 | `terminal/create` | request | **ja** (`default_result=None`) | – |
| 114 | `terminal/output` | request | **ja** | – |
| 122 | `terminal/release` | request | **ja** | – |
| 131 | `terminal/wait_for_exit` | request | **ja** | – |
| 139 | `terminal/kill` | request | **ja** | – |
| 150–156 | `elicitation/create` | request | nej | **ja** (`warn_unstable=not use_unstable_protocol` direkte i `Route`) |
| 158–163 | `elicitation/complete` | notification | – | **ja** |
| 165 | `session/update` | notification | – | – |

**To vigtige detaljer ●:**

1. **`terminal/*` er `optional=True`** — hvis klienten ikke implementerer dem, svarer SDK'et automatisk med `default_result` (None/{}), **ikke** med fejl. Så selv hvis agenten kalder `terminal/*` uden kapabilitet, hænger den ikke — den får et tomt svar.
2. **`fs/*` og `session/request_permission` er IKKE optional** — ingen behandler = `-32601` tilbage til agenten. Men de er kapabilitetsgated (se punkt 2), så agenten kalder dem kun hvis Skrynja har annonanceret støtte.

## 2 · Kapabilitets-gating — hvad agenten faktisk kan kalde ●

Verificeret i `vibe/acp/agent.py:662-690` (`_client_descriptor`):

```python
if capabilities is not None and capabilities.fs is not None:
    if capabilities.fs.read_text_file:
        client_tools.append("filesystem/read")
    if capabilities.fs.write_text_file:
        client_tools.append("filesystem/write")
if capabilities is not None and capabilities.terminal:
    client_tools.append("terminal")
callback_kinds: list[CallbackKind] = ["approval"]
if self._supports_user_input():
    callback_kinds.append("user_input")
```

**`client_tools` og `callback_kinds` går ind i `ClientDescriptor`** — som app-server fortæller Core, hvilke klient-værktøjer der findes. Så:

| Skrynja annoncere i `initialize` | Agenten må kalde | Konsekvens hvis agenten alligevel kalder uden kapabilitet |
|---|---|---|
| `fs.readTextFile: false` (default) | ingenting fs | agenten bruger egne værktøjer |
| `fs.writeTextFile: false` (default) | ingenting fs | samme |
| `terminal: false` (default) | ingen `terminal/*` | SDK svarer automatisk tomt (`optional=True`) |
| `elicitation` udeladt (default `None`) | ingen `elicitation/create` | ruten er unstable-gated; med `use_unstable_protocol=True` på agentens klient-forbindelse… |
| `elicitation.form: true` | `elicitation/create` | — |

**Elicitation-gating er reelt ●** (`agent.py:662-666` `_supports_user_input`): uden `capabilities.elicitation.form` kaldes `create_elicitation` **aldrig** (`agent.py:956-957` er den eneste kaldsvej). **Anbefaling fra relæet accepteret ●: Skrynja udelader `elicitation` i v1** — det fjerner hele unstable-eksponeringen på klientsiden.

**Vigtig præcisering ○→● om elicitation og `use_unstable_protocol`:** relæet spekulerede i, om Skrynjas egen klientforbindelse skal bygges med `use_unstable_protocol=True`. Svaret er **nej, og det er ligegyldigt**: elicitation-gating sker via **kapabiliteten** (`_supports_user_input`), ikke via tilstandsvariablen. Uden `elicitation.form` når agenten aldrig kaldsstedet. Tilstandsvariablen på Skrynjas side styrer kun, om `elicitation/create` ville afvises med `-32601` hvis agenten alligevel kaldte — men agenten kalder ikke. Så: **udelad `elicitation`, byg klient-routeren med `use_unstable_protocol=False` (default)**. Ingen eksponering.

## 3 · Skrynjas `initialize` — endelig `clientCapabilities` ●

```json
{
  "protocolVersion": 1,
  "clientCapabilities": {},
  "clientInfo": {"name": "harbour-skrynja", "title": "Skrynja", "version": "<app-version>"}
}
```

`clientCapabilities: {}` — **tom** ●. Konsekvenser (alle verificeret):

- `fs` default = `FileSystemCapabilities()` med `read_text_file=False`, `write_text_file=False` → agenten kalder **ingen** `fs/*` ●
- `terminal` default = `False` → agenten kalder **ingen** `terminal/*` ●
- `elicitation` default = `None` → `_supports_user_input()` = False → ingen `elicitation/create` ●
- `approval` callback er **altid** i `callback_kinds` (`["approval"]` uden betingelse) → `session/request_permission` **kan** komme (betinget af tool-permission, ikke klient-kapabilitet) ● — Skrynja skal implementere den (defensiv, A5/H2)

**`clientCapabilities: {}` er allerede målt på wire ●** (telefonen accepterede det). Det er den korrekte v1-konfiguration.

## 4 · A9 — Skrynjas egen svartid på indgående anmodninger ● (vedtaget)

| Indgående anmodning | Skrynjas adfærd | Frist/timeout |
|---|---|---|
| `session/request_permission` | Vis dialog; svar når brugeren beslutter | **Ingen klient-side timeout** — brugeren beslutter. Agenten har sin egen timeout (agent-side). Ved `session/cancel`: svar `cancelled` (A1, spec-krav) |
| `fs/read_text_file` | **Afvises** (`-32601` method not found) — ikke annoncere i v1 | — |
| `fs/write_text_file` | **Afvises** — samme | — |
| `terminal/*` (5) | **Afvises** — ikke annoncere; selv hvis kaldt, er de `optional=True` i SDK, men Skrynja implementerer dem ikke, så de fejler — det er OK, agenten kalder dem ikke uden kapabilitet | — |
| `elicitation/create` | **Afvises** — ikke annoncere; agenten kalder ikke | — |
| `session/update` (notification) | Behandles — ingen svar | — |

**Beslutning ●:** Skrynja implementerer **kun** `session/request_permission` og `session/update` i v1. Alle andre indgående ruter afvises med `-32601` (metode ikke understøttet) — hvilket er korrekt, fordi de ikke er annoncere.

**Om Skrynjas svartid på `request_permission` ○:** agenten venter på brugerens beslutning — det kan tage minutter, og det er **korrekt** (brugeren læser og beslutter). Der er ingen klient-side timeout, fordi der ikke er nogen rimelig grænse for menneskelig beslutningstid. Agentens egen timeout håndterer agentens side. **Hvis agenten timeout'er først**, lukker den callback'en (målt: `CALLBACK_CLOSED` swallowes, `agent.py:729-732`).

**Spejlet A8 ○ (bevidst valg):** A8 beskytter Skrynja mod en hængende agent. A9 beskytter agenten mod en hængende Skrynja — men for `request_permission` er «hængende» = «brugeren tænker», hvilket er legitimt. For de ruter Skrynja **afviser**, svarer den øjeblikkeligt med `-32601` — ingen hængning mulig. Så A9 har ingen timeout-mekanisme; den har en **implementerings-begrænsning** (kun to ruter) plus øjeblikkelig afvisning af resten.

## 5 · Samlet A8+A9 fristtabel ● (endelig)

**Udgående (Skrynja → agent, A8):**

| Metode | Frist |
|---|---|
| `initialize`, `session/new`, `session/load`, `session/list`, `set_config_option`, `set_mode`, `authenticate`, `close`, `fork` | 120 s fast |
| `session/prompt` | 120 s progress-baseret (sessionsbundet) |
| `session/cancel` | notification — rydder udestående |

**Indgående (agent → Skrynja, A9):**

| Metode | Skrynjas svar |
|---|---|
| `session/request_permission` | Dialog → brugerbeslutning → svar (ingen klient-timeout) |
| `session/update` | Behandles (notification, intet svar) |
| `fs/*`, `terminal/*`, `elicitation/*` | `-32601` øjeblikkeligt (ikke annoncere, ikke implementeret i v1) |

## Testplan — A9 ●

| Test | Forventet |
|---|---|
| Agent sender `session/request_permission` | Dialog vises; svar sendes med korrekt id (heltal) og outcome |
| Agent sender `fs/read_text_file` (skulle ikke ske) | Skrynja svarer `-32601` øjeblikkeligt |
| Agent sender `terminal/create` (skulle ikke ske) | `-32601` |
| Agent sender `elicitation/create` (skulle ikke ske) | `-32601` |
| `session/update` notification | Behandles, intet svar sendt |
| `initialize` med `clientCapabilities: {}` | Agent accepterer; ingen fs/terminal/elicitation kald senere |

## Status ●

A9 vedtaget: Skrynja implementerer kun `request_permission` + `session/update`; afviser resten øjeblikkeligt med `-32601`. `clientCapabilities: {}` er den korrekte v1-konfiguration (allerede målt på wire). Elicitation udeladt — fjerner unstable-eksponering. Ingen klient-side timeout på permission (brugeren beslutter); agentens egen timeout håndterer agent-siden.

**Rutekortet er nu fuldt målt på begge sider ●.** A8+A9 komplet. Parser-spec + transport-spec + frist-spec komplet. Ingen arkitekturvalg genåbnet.
