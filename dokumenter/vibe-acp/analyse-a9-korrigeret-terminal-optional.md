# A9 korrigeret — `terminal/*` er `optional=True` i SDK'et; to rækker, ikke én; agent-null håndteres som værktøjsfejl ●

Relæet (meddelelse `226b12f4`) påviste, at den vedtagne A9-tabel var forkert for `terminal/*`: de fem ruter er registreret `optional=True` i klient-routeren, så **SDK'et svarer selv** med `default_result, før Skrynjas kode når ind — Skrynja kan ikke afvise dem med `-32601`. Denne analyse verificerer mekanismen endeligt i SDK 0.11.0 og sdist 3.0.2, korrigerer tabellen til **to** terminal-rækker, og sporer konsekvensen af `null` på agent-siden. To åbne punkter fra relæet lukkes ●.

## 1 · `optional`-mekanismen — verificeret endeligt ●

Relæets forbehold var: «`_resolve_handler` selv er ikke læst». Den er nu læst. Kæden i `acp/router.py`:

```
_resolve_handler(obj, attr)          # router.py:29-50
  └─ func = getattr(obj, attr, None)          ← fanger manglende attribut, ingen AttributeError
     fallback: camelCase-attribut via to_camel_case(attr), getattr igen
     └─ func is None eller ikke callable → return (None, attr, legacy_api)

_make_func(model, obj, attr)         # router.py:93-96
  └─ func, attr, legacy_api = _resolve_handler(obj, attr)
     func is None → return None                  ← ingen undtagelse kastes

Route.handle(params)                  # router.py:63-78
  ├─ func is None:
  │    ├─ optional=True  → return default_result   ← INGEN fejl tilbage til agenten
  │    └─ ellers        → raise RequestError.method_not_found  (−32601)
  ├─ warn_unstable → UserWarning lokalt + raise −32601
  └─ ellers → await func(params) (+ adapt_result)
```

**Åbent punkt 1 lukket ●:** `_resolve_handler` fanger manglende attribut via `getattr(obj, attr, None)` (med camelCase-fallback), og `_make_func` returnerer `None` i stedet for at kaste. `func is None`-grenen i `handle()` er derfor **nåelig** for en klient, der ikke implementerer ruten — og ved `optional=True` returneres `default_result` uden fejl.

**Sidefund om `legacy_api` ◐ (ingen betydning for Skrynja):** `_resolve_handler` returnerer også en `legacy_api`-markør — en ældre handler-form, der udsender `DeprecationWarning` og kaldes med ét model-objekt i stedet for nøgleords-argumenter. Det forklarer, hvorfor `params`-bygningen i `_make_func`'s wrapper ser dobbelt ud. Skrynja bygger sin egen C++-klient og rører ikke denne vej.

**`warn_unstable` bekræftet igen ●:** ligger **efter** `func is None`-grenen — en unstable-rute afvises med `-32601` efter en lokal `UserWarning`, hvis `use_unstable_protocol=False`. På agentens forbindelse til Skrynja er flaget `True` (`vibe/acp/agent.py`, `_serve_acp_agent`), så `warn_unstable` altid er False der.

## 2 · Korrektion — A9 skal have to terminal-rækker ●

`build_client_router` (`acp/client/router.py:94-165`) registrerer præcis fem `terminal/*`-ruter med `optional=True`, og `default_result` falder i **to grupper**:

| Rute | `default_result` | Wire-svar til agenten | `adapt_result` |
|---|---|---|---|
| `terminal/create` | `None` | `result: null` | – |
| `terminal/output` | `None` | `result: null` | – |
| `terminal/wait_for_exit` | `None` | `result: null` | – |
| `terminal/release` | `{}` | `result: {}` | `normalize_result` |
| `terminal/kill` | `{}` | `result: {}` | `normalize_result` |

`fs/*` og `session/request_permission` er **ikke** optional — ingen behandler betyder hårdt `-32601`. `elicitation/create` bærer `warn_unstable=not use_unstable_protocol` direkte i `Route(...)` (ikke `unstable=`), og `elicitation/complete` er en unstable-notification.

**Rettet A9-tabel (erstatter §4/§5 i `analyse-a9-indgaaende-anmodninger-kapabiliteter.md`):**

| Indgående | Skrynjas svar | Grund |
|---|---|---|
| `session/request_permission` | Dialog → brugerbeslutning → svar. **Ingen klient-timeout** (A1: svar `cancelled` ved `session/cancel`) | ikke optional; `callback_kinds=["approval"]` er ubetinget (`agent.py:679`) |
| `session/update` (notification) | Behandles, intet svar | – |
| `fs/read_text_file`, `fs/write_text_file` | `-32601` øjeblikkeligt | ikke optional → `func is None`-grenen; desuden ikke annoncere (`clientCapabilities: {}`) |
| `elicitation/create`, `elicitation/complete` | `-32601` / ignoreres | ikke annoncere (`elicitation.form` udeladt → `_supports_user_input()` False → kaldsstedet nås aldrig); unstable |
| **`terminal/create`, `terminal/output`, `terminal/wait_for_exit`** | **Ikke Skrynjas beslutning — SDK svarer `null`** | `optional=True`, `default_result=None` |
| **`terminal/release`, `terminal/kill`** | **Ikke Skrynjas beslutning — SDK svarer `{}`** | `optional=True`, `default_result={}` |

**Vigtig præcisering for Skrynjas egen C++-klient ●:** Skrynja implementerer **ikke** Python-SDK'et — den taler NDJSON selv. SDK'ets auto-svar findes ikke i Skrynja; Skrynja skal selv vælge, hvad den svarer på `terminal/*`. Den defensive og korrekte adfærd for Skrynja er **`-32601` øjeblikkeligt** (ruten er ikke implementeret og ikke annoncere). I v1 er ruten desuden unåelig (se §4), så svaret er forsvar i dybden, aldrig nået i praksis.

## 3 · Agent-side konsekvens af `null` — sporet til ende ●

Relæets åbne punkt 2: «håndterer agenten `null` fra `terminal/create`/`terminal/output`?» Svaret er **nej — og nej betyder en værktøjsfejl, ikke et hæng**. Kæden, verificeret i sdist 3.0.2 + SDK 0.11.0:

```
agent-side: AgentSideConnection.create_terminal  (acp/agent/connection.py)
  └─ request_model(conn, "terminal/create", …, CreateTerminalResponse)
       └─ response = await conn.send_request(...)   ← wire-resultatet: null
            └─ validate_model(None, CreateTerminalResponse)   (acp/utils.py:73-75)
                 └─ CreateTerminalResponse.model_validate(None)
                      └─ pydantic ValidationError  ● målt lokalt:
                           «1 validation error for CreateTerminalResponse»
                           (terminal_id: Input should be a valid string)

app-server: AppServerSession._handle_request  (app_server/session.py:1021-1055)
  └─ _dispatch_client_tool(handler, TERMINAL_CREATE, params)
       └─ handler = AcpClientToolHandler  (alltid til stede på ACP-stien)
            └─ await self._client.create_terminal(...)   ← ValidationError kastes HER
  └─ except ValidationError → respond(id, INVALID_PARAMS, str(exc))
  └─ except Exception      → respond(id, INTERNAL_ERROR, str(exc))
```

**Åbent punkt 2 lukket ● — med en nuancering:** der er to mulige fejlveje, og begge ender som **værktøjsfejl til modellen, ikke et hæng**:

| Rute | Wire-resultat | Første fejlssted | Fejl til modellen |
|---|---|---|---|
| `terminal/create` | `null` | `model_validate(None)` → `ValidationError` i `AgentSideConnection.create_terminal` | `INVALID_PARAMS` (pydantic-`ValidationError` fanget først, `session.py:1039-1046`) |
| `terminal/output` | `null` | samme — `TerminalOutputResponse.model_validate(None)` | `INVALID_PARAMS` |
| `terminal/wait_for_exit` | `null` | samme — `WaitForTerminalExitResponse.model_validate(None)` | `INVALID_PARAMS` |
| `terminal/release` | `{}` | `request_optional_model` → `validate_optional_model({}, …)` → tom `ReleaseTerminalResponse` | **intet** — `AcpClientToolHandler.release_terminal` kalder videre og returnerer `EmptyResponse()` — **tavst succes** |
| `terminal/kill` | `{}` | samme — tom `KillTerminalResponse` | **intet** — tavst succes |

Min tidligere formulering («`None` → `AttributeError` på `response.terminal_id` → INTERNAL_ERROR») var **forkert om mekanismen** ●: `validate_model(None, …)` kaster `ValidationError`, før `AcpClientToolHandler.create_terminal` når `response.terminal_id` (`vibe/acp/tool_io.py`). `AttributeError`-vejen ville først være nåelig, hvis en `null`-resultat blev coerceret til `None` — det sker kun på `request_optional_model`-ruterne (`release`/`kill`), hvor `{}`-svaret parses fint og **ikke** når `None`-håndteringen. Konsekvensen er den samme i praksis: modellen ser en værktøjsfejl (ikke et hæng) for de tre `null`-ruter, og tavst «succes» for de to `{}`-ruter.

**Nyt åbent punkt ○ (agent-internt, ikke Skrynjas):** agenten kan ikke skelne «klienten understøtter ikke terminaler» (`null`/`{}` fra `optional`) fra «terminalen blev oprettet, men uden id». Den stille tolerance er en fejlkilde i SDK'et, ikke en beskyttelse — men den ligger uden for Skrynjas rækkevidde.

## 4 · `terminal/*` og `fs/*` er unåelige på ACP-stien i v1 ●

Selv hvis Skrynja en dag annoncerede `terminal: true` eller `fs.*: true`, er der **to uafhængige stop**, verificeret i sdist 3.0.2:

1. **Kapabilitetslaget** (`agent.py:668-690`, `_client_descriptor`): `client_tools` bygges kun fra `capabilities.fs.*` / `capabilities.terminal`. Med `clientCapabilities: {}` (Skrynjas v1) bliver listen **tom** — og `callback_kinds` er `["approval"]` uden betingelse.
2. **Værktøjslaget** (uafhængigt af kapabiliteter): ACP-stien konstruerer `LocalHarnessOptions(client=…, session_options=…, session=…, client_tool_handler=…)` (`agent.py:601-611`) — **`client_tools` sættes aldrig** (feltet default-er til `None`, `_runtime.py:562`). `LocalHarnessHost._process_for` overfører `options.client_tools` til `HarnessProcess` (`local.py:106`) → `ClientProvidedTools(None)` → `self._groups = ()` (`_client_provided_tools.py:150-155`) → adapteren kalder kun `configure_provided_tool_executor` **hvis `context.client_tools.group_names` er non-tom** (`_unified_harness_backend_adapter.py:3894-3902`). Core får derfor **ingen klient-værktøjsgrupper** — uanset hvad klienten annoncerer.

**Følge ●:** Core kan ikke kalde `fs/*`/`terminal/*` på wire. Den eneste levende klient-kaldsvej er `callback/call` → `CallbackRequested` → `_answer_callback` → `session/request_permission` (betinget af `callback_kinds`, altid `["approval"]`, og af tool-permissions). App-serverens `_handle_request` (`session.py:1021-1055`) dispatcher `clientTool/*` til `AcpClientToolHandler` — men Core når aldrig dertil uden grupper.

## 5 · A9 endelig for Skrynja (C++-klient) ●

| Indgående | Skrynjas svar | Frist |
|---|---|---|
| `session/request_permission` | Dialog → brugerbeslutning (`allow_once`/`allow_always`/`reject_once`/`reject_always`) → svar med samme id | **Ingen klient-timeout** — «hængende» = «brugeren tænker», legitimt. Ved `session/cancel`: svar `cancelled` (A1, spec-krav) |
| `session/update` (notification) | Behandles (ingen svar) | – |
| `fs/read_text_file`, `fs/write_text_file` | `-32601` øjeblikkeligt | – |
| `terminal/create`, `terminal/output`, `terminal/wait_for_exit`, `terminal/release`, `terminal/kill` | `-32601` øjeblikkeligt (defensivt — SDK auto-svar findes ikke i Skrynjas egen klient; unåeligt i v1 alligevel) | – |
| `elicitation/create`, `elicitation/complete` | `-32601` / ignoreres | – |

`clientCapabilities: {}` i `initialize` er **bekræftet korrekt v1** (schema-defaults: `fs.readTextFile=false`, `fs.writeTextFile=false`, `terminal=false`, `elicitation=None`; målt accepteret på wire). A9 har ingen timeout-mekanisme: implementeringsbegrænsning (to ruter) + øjeblikkelig afvisning af resten + brugerbeslutning uden frist på `request_permission`. Agentens egen timeout håndterer agent-siden (`CALLBACK_CLOSED` swallowes, `agent.py:729-732`).

## 6 · R13-noter (sdist ≠ installeret træ) ○

- **Thinking-guard:** relæet målte **ingen** hardcoded thinking-level-liste i den installerede `vibe/acp/agent.py` (`eb5fc4f4`); sdist'en **har** en — `case "thinking", str(level) if level in {"off","low","medium","high","max"}` (`agent.py:1069-1075`, læst igen denne session). Guarden validerer mod den **globale** `THINKING_LEVELS` (`vibe/config_values.py:11-12`), ikke mod modellens `thinking_levels`, så `low` passer alligevel for zai-glm og dør ved API'et med `3051`. **A7's konklusion er uændret**; forskellen er versions-drift mellem sdist og installeret træ. Per R13 citeres udtrykket, ikke linjenummeret, når dette relæes videre.
- **Null-mekanismen (§3)** er verificeret ved lokal kørsel af SDK 0.11.0's egen `validate_model` mod schema-modellerne — samme kode, samme Pydantic-major, som telefonen kører (`agent_client_protocol 0.11.0` ● målt). Ikke desto: den er kørt i målemaskinen, ikke på telefonen — markeret ● for kodelæsning + lokal kørsel, ○ for telefon-kørsel.

## Testplan — A9 korrigeret ●

| Test | Forventet |
|---|---|
| Agent sender `session/request_permission` | Dialog vises; svar med korrekt id (heltal) og `outcome` |
| Agent sender `session/cancel` mens permission-dialog åben | Dialog lukkes; `cancelled` svares på den udestående anmodning (A1) |
| Agent sender `fs/read_text_file` (skulle ikke ske) | Skrynja svarer `-32601` øjeblikkeligt |
| Agent sender `terminal/create` (skulle ikke ske) | Skrynja svarer `-32601` øjeblikkeligt (ikke `null` — det er SDK'ets auto-svar, ikke Skrynjas) |
| Agent sender `terminal/release` (skulle ikke ske) | `-32601` |
| Agent sender `elicitation/create` (skulle ikke ske) | `-32601` |
| `session/update` notification | Behandles, intet svar sendt |
| `initialize` med `clientCapabilities: {}` | Agent accepterer; ingen fs/terminal/elicitation kald senere (kun `request_permission` ved behov) |

## Status ●

A9 korrigeret og lukket: to terminal-rækker (`null` ×3, `{}` ×2 i SDK'et; `-32601` ×5 i Skrynjas egen klient), `optional`-mekanismen verificeret endeligt (`_resolve_handler` → `_make_func` → `handle`), agent-null sporet til `ValidationError` → værktøjsfejl (ikke hæng), `terminal/*`/`fs/*` unåelige i v1 (kapabiliteter **og** manglende `client_tools`-grupper i Core). `clientCapabilities: {}` bekræftet. A8 uændret. Ingen arkitekturvalg genåbnet — A9's kernevalg (kun `request_permission` + `session/update` implementeret; resten afvist) står.
