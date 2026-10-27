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

**Sidefund om `legacy_api` ● (ingen betydning for Skrynja):** `_resolve_handler` returnerer også en `legacy_api`-markør — en ældre handler-form, der udsender `DeprecationWarning` og kaldes med ét model-objekt i stedet for nøgleords-argumenter. Det forklarer, hvorfor `params`-bygningen i `_make_func`'s wrapper ser dobbelt ud. Skrynja bygger sin egen C++-klient og rører ikke denne vej.

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
| `session/request_permission` | Dialog → brugerbeslutning → svar. **Ingen klient-timeout** (A1: svar `cancelled` ved `session/cancel`) | ikke optional; `callback_kinds` starter som `["approval"]` — `approval` tilføjes **ubetinget**, listen får `"user_input"` ved `elicitation.form` (`agent.py:679-681`) |
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

Min tidligere formulering («`None` → `AttributeError` på `response.terminal_id` → INTERNAL_ERROR») var **forkert om mekanismen** ●: `validate_model(None, …)` kaster `ValidationError`, før `AcpClientToolHandler.create_terminal` når `response.terminal_id` (`vibe/acp/tool_io.py`). Konsekvensen: modellen ser en værktøjsfejl (ikke et hæng) for de tre `null`-ruter, og tavst «succes» for de to `{}`-ruter. *(Kropsrettet i runde 9: mellem «(`vibe/acp/tool_io.py`).» og «Konsekvensen …» stod sætningen «`AttributeError`-vejen ville først være nåelig, hvis en `null`-resultat blev coerceret til `None` — det sker kun på `request_optional_model`-ruterne (`release`/`kill`), hvor `{}`-svaret parses fint og **ikke** når `None`-håndteringen» — en spekulation, som appendet «Rettelser fra PR-gennemlæsningen» punkt 3 allerede erklærede skåret væk. Det målte står nu alene, som appendet påstod.)*

**Nyt åbent punkt ○ (agent-internt, ikke Skrynjas):** agenten kan ikke skelne «klienten understøtter ikke terminaler» (`null`/`{}` fra `optional`) fra «terminalen blev oprettet, men uden id». Den stille tolerance er en fejlkilde i SDK'et, ikke en beskyttelse — men den ligger uden for Skrynjas rækkevidde.

## 4 · `terminal/*` og `fs/*` er unåelige på ACP-stien i v1 ●

Selv hvis Skrynja en dag annoncerede `terminal: true` eller `fs.*: true`, er der **to uafhængige stop**, verificeret i sdist 3.0.2:

1. **Kapabilitetslaget** (`agent.py:668-690`, `_client_descriptor`): `client_tools` bygges kun fra `capabilities.fs.*` / `capabilities.terminal`. Med `clientCapabilities: {}` (Skrynjas v1) bliver listen **tom** — og `callback_kinds` er `["approval"]` uden betingelse **i v1** (`approval` ubetinget; `"user_input"` tilføjes ved `elicitation.form`).
2. **Værktøjslaget** (uafhængigt af kapabiliteter): ACP-stien konstruerer `LocalHarnessOptions(client=…, session_options=…, session=…, client_tool_handler=…)` (`agent.py:601-611`) — **`client_tools` sættes aldrig** (feltet default-er til `None`, `_runtime.py:562`). `LocalHarnessHost._process_for` overfører `options.client_tools` til `HarnessProcess` (`local.py:106`) → `ClientProvidedTools(None)` → `self._groups = ()` (`_client_provided_tools.py:150-155`) → adapteren kalder kun `configure_provided_tool_executor` **hvis `context.client_tools.group_names` er non-tom** (`_unified_harness_backend_adapter.py:3894-3902`). Core får derfor **ingen klient-værktøjsgrupper** — uanset hvad klienten annoncerer.

**Følge ●:** Core kan ikke kalde `fs/*`/`terminal/*` på wire. Den eneste levende klient-kaldsvej er `callback/call` → `CallbackRequested` → `_answer_callback` → `session/request_permission` (betinget af `callback_kinds`, der altid indeholder `"approval"`, og af tool-permissions). App-serverens `_handle_request` (`session.py:1021-1055`) dispatcher `clientTool/*` til `AcpClientToolHandler` — men Core når aldrig dertil uden grupper.

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
- **Null-mekanismen (§3)** er verificeret ved lokal kørsel af SDK 0.11.0's egen `validate_model` mod schema-modellerne — samme kode, samme Pydantic-major, som telefonen kører (`agent_client_protocol 0.11.0` ● målt). Ikke desto mindre: den er kørt i målemaskinen, ikke på telefonen — markeret ● for kodelæsning + lokal kørsel, ○ for telefon-kørsel.

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

---

## Bekræftelsesrunde 2 — telefonens målinger mod sdist ● (relæ `5f898ad8`)

Telefonen målte bekræftelserne lokalt i den installerede udgave og tilføjede to præciseringer. Begge verificeret igen i sdist/SDK denne session:

| Telefonens måling | Verificeret her | Status |
|---|---|---|
| `acp/router.py:31-37`: `getattr(obj, attr, None)` i begge forgreninger; ingen `AttributeError` | identisk i SDK 0.11.0 (`_resolve_handler`) | ● |
| **Omvendt fallback** (`router.py:38-43`): callable attribut **uden** underscore → signatur-inspektion for `params` → `legacy_api=True` | bekræftet — `elif callable(func) and "_" not in attr:` + `inspect.signature` + `len(parameters) == 2 and "params" in parameters` | ● |
| `agent.py:601-611`: `LocalHarnessOptions(client=…, session_options=…, session=…, client_tool_handler=…)` — intet `client_tools` | bekræftet (`_start_session`-vejen) | ● |
| `agent.py:1499-1502`: passiv vært (`_host_resources`) sætter kun `client` + `session_options` — heller intet `client_tools` | bekræftet (`LocalHarnessOptions(client=self._client_descriptor(), session_options=SessionOptions(cwd=…))`) | ● |
| `terminal/kill` er femte `optional=True` (linje 143) med `default_result={}` | bekræftet (`client/router.py:139-146`: `optional=True, default_result={}, adapt_result=normalize_result`) | ● |

**Nye præciseringer optaget i dokumentet:**

1. **Omvendt fallback i `_resolve_handler`** (§1, sidefund): når attributten findes og navnet har **ingen** underscore, inspiceres signaturen — to parametre med ét ved navn `params` markerer legacy-formen (ét model-objekt i stedet for nøgleords-argumenter). Uden betydning for A9, men forklarer `legacy_api`-flaget og de to kaldeformer i `handle()`.
2. **Passiv vært uden `client_tools`** (§4): også `_host_resources` (`agent.py:1499-1502`) konstruerer uden `client_tools` — unåelighedskæden gælder **begge** værtsveje, ikke kun `_start_session`.

**Telefonens forbehold ○ (stående):** `model_validate(None)` → `ValidationError` er relæets lokale måling, ikke reproduceret på telefonen. Mekanismen hviler på SDK 0.11.0-kode + lokal kørsel mod samme schema-modeller; telefon-kørsel udestår (ikke-blokerende — ruten er unåelig i v1 alligevel).

**Status efter bekræftelsesrunde 2 ●:** A9 korrigeret og dobbelt-verificeret (relæ + telefon). A8 uændret. A8+A9, parser-spec og transport-spec komplet. Ingen arkitekturvalg genåbnet.

---

## Rettelser fra PR-gennemlæsningen (relæ `606a4b06`) ● — optaget uden at ændre ovenstående

PR-gennemlæsningen fandt tre præciseringer i dette dokument. Originalsektionerne ovenfor er bevaret (korrektur sker i nye afsnit); rettelserne gælder fra denne runde:

1. **`callback_kinds` er ikke ubetinget `["approval"]` ●** (§2-tabel og §4): koden bygger listen som `["approval"]` og tilføjer `"user_input"`, når klienten annoncerer `elicitation.form` (`agent.py:679-681`, målt). Korrekt formulering: `approval` er **ubetinget** — så `session/request_permission` kan altid komme — men listen som helhed er betinget. I Skrynjas v1 (elicitation udeladt) er resultatet `["approval"]`, så konsekvensen for Skrynja er uændret.

2. **R13 — sdist-linjenumre i §4 punkt 2 ●:** henvisningerne `_runtime.py:562`, `local.py:106`, `_client_provided_tools.py:150-155`, `_unified_harness_backend_adapter.py:3894-3902` citerer sdist-linjenumre i strid med dette dokumentets egen R13-regel. Fremadrettet citeres **udtrykkene**: `LocalHarnessOptions(client=…, session_options=…, session=…, client_tool_handler=…)` (intet `client_tools`), `client_tools=options.client_tools`, `self._groups = () if tools is None else …`, `if context.client_tools.group_names:`. Kædens semantik er uændret ●.

3. **§3 — `AttributeError`-spekulationen skåret væk ●:** den oprindelige sætning hævdede, at en `AttributeError`-vej «kun» ville være nåelig via `request_optional_model`-ruterne — ulogisk, for netop de ruter (`release`/`kill`) får `{}` fra SDK'et, ikke `null`. Det målte står alene: `validate_model(None, …)` → `ValidationError` → værktøjsfejl (`INVALID_PARAMS`) for de tre `null`-ruter; `{}` parses som tom respons → tavst succes for de to `{}`-ruter.

---

## Bekræftelsesrunde 4 — `handle` og `serialize_params` målt ● (relæ `7d44b866`)

Telefonen målte `Route.handle` og `serialize_params` direkte og korrigerede én af mine slutninger. Verificeret igen i SDK 0.11.0 denne session — inklusive en lokal kørsel af `Route.handle` mod rigtige `Route`-objekter:

### `adapt_result` rammer ikke `default_result` ● — min «dobbelt sikret» var forkert

Telefonen er rigtig, og jeg bekræfter med en kørt test: `Route.handle` returnerer `default_result` med **tidlig retur** i `func is None`-grenen — `adapt_result` anvendes kun på et reelt behandlersvar (`if self.adapt_result is not None and self.kind == "request"` efter `await self.func(params)`). Kørsel bekræfter:

```
Route(func=None, optional=True, default_result={}, adapt_result=normalize_result).handle({})
  → {}        # adapt_result IKKE kørt på default_result
Route(func=<handler returning None>, optional=True, default_result={}, adapt_result=normalize_result).handle({})
  → {}        # adapt_result kørt: None → {} via normalize_result
```

**Korrektion ●:** `terminal/release` og `terminal/kill` er beskyttet **én** gang — af `{}` som `default_result`. `normalize_result` beskytter kun behandlersvar (relevant hvis en klient faktisk implementerede ruten og returnerede `None`). Min formulering «dobbelt sikret» var en slutning skrevet som en målt kæde. §2-tabellen ovenfor viser `adapt_result=normalize_result` korrekt som registreret — men kolonnen må ikke læses som «køres også på default_result». A9's to-række-tabel står i øvrigt uændret.

| Rute | `default_result` | `adapt_result` kørt på default? | Wire | Agent-side |
|---|---|---|---|---|
| `create` / `output` / `wait_for_exit` | `None` | – | `null` | `ValidationError` → værktøjsfejl |
| `release` / `kill` | `{}` | **nej** (kun på behandlersvar) | `{}` | tavst succes |

### `serialize_params` lukket ●

`serialize_params` (udtryk: `params.model_dump(mode="json", by_alias=True, exclude_none=True, exclude_defaults=True)`). Kørsel bekræfter: `KillTerminalResponse()` → `{}`, `ReleaseTerminalResponse()` → `{}` — ikke `{"_meta": null}`. Uden praktisk betydning for A9 (ingen terminal-handler registreres på ACP-stien), men lukket som ○-punkt.

### To nye fund i `handle` ●

1. **`func is None`-grenen ligger før `warn_unstable`** — en rute med både `optional=True` og `warn_unstable=True` ville svare `default_result` i stedet for `-32601`. **`optional` er stærkere end unstable-flaget.** Ingen nuværende rute har begge (○ ikke målt), men rækkefølgen bør stå som regel i spec'en — den er kontraintuitiv. **Tilføjet til A9 som rækkefølgeregel ●:** i SDK'ets `handle` er rækkefølgen (a) ingen behandler → `default_result` eller `-32601`; (b) `warn_unstable` → advarsel + `-32601`; (c) behandlerkald + `adapt_result`. Skrynjas egen klient bør følge samme rækkefølge, hvis den implementerer `optional`-lignende auto-svar (den gør ikke i v1 — Skrynja svarer altid selv).
2. **`-32601`'s tredje variant er nu målt, ikke udledt ●:** `routes.get(method)` → `None` → `RequestError.method_not_found(method)` i routerens dispatch (før `route.handle` kaldes). *(Kropsrettet i runde 6: tidligere stod «Alle tre årsager» — efter runde 5 er der **fire varianter**: (a) ingen behandler uden `optional`, (b) `warn_unstable`, (c) ukendt metodenavn, (d) `_`-præfiks uden extension-handler. Se runde 5 nedenfor.)*

### Kolonneforveksling i A8-doc §6 — bekræftet og allerede præciseret ●

Telefonen peger på kolonneforvekslingen i `analyse-a8-korrigeret-resume-frist-kapabiliteter.md` §6-tabellen: kolonnen «I SDK'et (Python-klient)» beskriver **Skrynjas** adfærd for `session/request_permission` («Skrynja implementerer: dialog…») — kolonnen beskriver faktisk «A9's vedtagne adfærd». *(Kropsrettet i runde 9: tidligere stod «SDK'et er agenten, ikke klienten; der er ingen «SDK-klient» at beskrive for denne rute» og «Rettet her frem for in-place». «Ingen SDK-klient»-påstanden er **forkert ●** — verificeret i SDK 0.11.0: `acp/client/router.py` **er** SDK'ets klientside og registrerer `session/request_permission` og `session/update`, som videresender til `client.request_permission` / `client.session_update`; A8-doc's runde 8 erklærer samme påstand «Forkert ●». Korrekt begrundelse: for de to session-ruter beskriver kolonnen A9-vedtaget for Skrynja, fordi en SDK-klient ville videresende til sin egen `Client`-implementering. «Rettet her frem for in-place» var desuden forkert om mekanismen — rettelsen skete faktisk in-place i A8-doc i runde 7.)* §6-tabellens venstre kolonne skal læses som «**A9-vedtaget (gælder Skrynja)**», ikke «I SDK'et». For `terminal/*`-rækkerne er kolonnen korrekt — der beskriver den *Python-SDK-klient*, Skrynja ville erstatte.

**Status efter bekræftelsesrunde 4 ●:** `adapt_result`-spørgsmålet lukket (én beskyttelse, ikke to); `serialize_params` lukket; `-32601` lukket med tre målte varianter (senere fire — se runde 5); rækkefølgereglen `optional` før `unstable` tilføjet A9; §6-kolonneforvekslingen præciseret. A9's to-række-tabel uændret. A8 uændret. Ingen arkitekturvalg genåbnet.

---

## Bekræftelsesrunde 5 — punkt 6b lukket; fjerde `-32601`-variant; fem tekstrettelser ● (relæ `d544375b`)

### Punkt 6b lukket ● — nul ruter har både `optional=True` og `unstable`

Målt i SDK 0.11.0 denne session: `optional=True` forekommer kun i `acp/client/router.py` (de fem `terminal/*`-ruter); ingen af dem bærer `unstable`. Agent-routeren har **ingen** `optional=True` overhovedet. Kombinationen `optional` + `unstable` er **konstruerbar** (`route_request` tager begge som uafhængige parametre; `warn_unstable=unstable and not self._use_unstable_protocol`), men forekommer ikke i 0.11.0. Rækkefølgereglen (`func is None` før `warn_unstable`, så `optional` vinder) er dermed en **latent fælde for fremtidige ruter**, ikke en aktuel fejl — og står nu som regel i A9 (runde 4).

**Præcisering af `warn_unstable` ●:** sættes som `unstable and not self._use_unstable_protocol`. På Skrynjas forbindelse (`use_unstable_protocol=True` på agentens side) er `warn_unstable` **False** på alle ruter — unstable-grenen i `handle` er død der. Samme konklusion som tidligere, nu med udtrykket bag.

### Fjerde variant af `-32601` ● — `-32601` har nu **fire** årsager

Telefonen fandt en fjerde variant i `_resolve_handler` og dispatchen. Verificeret i SDK 0.11.0 denne session:

| # | Årsag | Sted | Kilde |
|---|---|---|---|
| a | ingen behandler — attribut **mangler eller er ikke-kaldbar** (`func is None or not callable(func)` → `None`) | `_resolve_handler` → `_make_func` → `handle` første gren | målt i 0.11.0 denne session (telefonen læste samme gren i upstream) |
| b | `warn_unstable` (unstable-rute uden aktiveret flag) | `handle` anden gren | målt |
| c | ukendt metodenavn — `routes.get(method)` → `None` | router-dispatch | målt |
| d | metodenavn med `_`-præfiks **uden** extension-handler (`method.startswith("_")` + `ext_handler is None`) | router-dispatch | målt i 0.11.0 denne session (`router.py`: `if isinstance(method, str) and method.startswith("_"): if ext_handler is None: raise RequestError.method_not_found(method)`) |

**Korrektion:** A8-doc §2 og denne fil's runde 4 siger «tre årsager» — rettet til **fire varianter** (a-d). (a) og (d) var telefonens læsning af upstream `main`; jeg har målt begge i den lokale 0.11.0 — de findes dér også. Klienten kan ikke skelne nogen af dem uden at kende flaget og extension-konfigurationen.

### Fem tekstrettelser i `41bf277` ● — alle adresseret her (append-only)

| # | Fejl | Rettelse |
|---|---|---|
| 1 | **Ikke-diskriminerende test** (runde 4): min kørsel brugte `normalize_result` som `adapt_result`, og `normalize_result({})` returnerer `{}` uændret — testen kunne ikke skelne «adapt kørt på `{}`» fra «adapt ikke kørt». Telefonen har ret | **Diskriminerende test kørt denne session** med en spy-adapter: `Route(func=None, optional=True, default_result={'raw': True}, adapt_result=spy).handle({})` → `{'raw': True}`, **adapt kørt 0 gange**; samme rute med handler → adapt kørt 1 gang. Konklusion uændret: `adapt_result` kører **ikke** på `default_result` ● |
| 2 | **Selvmodsigende kolonneparagraf** (runde 4, «Kolonneforveksling»): jeg skrev at kolonnen skal læses som «A9-vedtaget», men samtidig at den for `terminal/*` er korrekt som «Python-SDK-klient» — én kolonne, to betydninger | Præciseret: §6-tabellen i A8-doc har **to forskellige venstre-kolonne-betydninger** på forskellige rækker. `session/request_permission` og `session/update`: kolonnen beskriver **A9-vedtaget for Skrynja**. `fs/*`, `elicitation/*`, `terminal/*`: kolonnen beskriver **Python-SDK-klientens** adfærd (hvad SDK'et ville gøre, hvis Skrynja brugte det). Læseren skal kende forskellen pr. række — en bedre tabel ville have haft to kolonner; append-only forhindrer ombygning, så forskellen står her eksplicit |
| 3 | **Manglende runde 3-pegepind**: A9-doc nævner «Bekræftelsesrunde 2» i README-rækken og i filen, men runde 3 (schema-verifikation, tredje inkonsistensklasse) blev appenderet til **A8-doc**, ikke A9-doc — A9-doc's læser ser ikke pege. | Pege tilføjet her: A9-relevante dele af runde 3 (schema-felter, `resume` uden annonceret kapabilitet, tredje inkonsistensklasse) står i `analyse-a8-korrigeret-resume-frist-kapabiliteter.md` → «Bekræftelsesrunde 3». README-rækken for A8-doc nævner allerede «tre inkonsistensklasser» — pege tilføjes nedenfor i README |
| 4 | **A9's «ubetinget»** (§2-tabel, oprindelig tekst): «`callback_kinds=["approval"]` er ubetinget» — telefonen påpegede i runde 3, at listen er betinget (`user_input` ved `elicitation.form`); rettelsen blev appenderet (punkt 1 i «Rettelser fra PR-gennemlæsningen»), men §2-tabellens oprindelige celle står uændret og siger stadig «ubetinget» | Præciseret her: §2-tabellens «ubetinget» gælder **kun `approval`-elementet** (det tilføjes altid); listen som helhed er betinget. Den appendede rettelse i «Rettelser fra PR-gennemlæsningen» punkt 1 står og er den autoritative formulering |
| 5 | **A8's urørte §6** (oprindelig A8-doc §6, «Indgående» linje 96): «`fs/*` + `elicitation/*` + `terminal/*` → `-32601` øjeblikkeligt» gendanner A9-korrektionens fejl — den præcisering, der blev appenderet til **A8-korrektionsdoc** (`1cfa12d`), nåede ikke den **oprindelige** A8-doc (`analyse-a8-komplet-rutekort-kode.md`), hvis §5-tabel stadig viser én fælles række | Peget tilføjet her: den oprindelige A8-doc's §5 indgående-række er **erstattet af** A9-korrektionsdoc + A8-korrektionsdoc's §6-præcisering. Læses den oprindelige A8-doc alene, er `terminal/*`-rækken forældet — det står nu i README-rækken for den oprindelige A8-doc (som allerede er markeret «titel overdrevet»; pege udbygges nedenfor) |

**Status efter bekræftelsesrunde 5 ●:** punkt 6b lukket (0 ruter med `optional`+`unstable`; kombinationen konstruerbar = latent fælde, regel står i A9); `-32601` har **fire** målelige varianter; fem tekstrettelser i `41bf277` adresseret; diskriminerende `adapt_result`-test kørt og bekræfter runde 4. A9's to-række-tabel uændret. A8 uændret. Ingen arkitekturvalg genåbnet.

---

## Kropsrettelser i runde 6 (in-place, noteret efter konventionen) ●

Følgende korte faktafejl er rettet **i kroppen** af denne fil i samme commit som A8-doc's runde 6 — hver rettelse angives her, så revisionssporet bevares:

1. **Runde 4, punkt 2:** «Alle tre årsager er dermed belagte» → note om at der efter runde 5 er **fire varianter** (a-d), med pege til runde 5.
2. **Runde 4, status:** «`-32601` lukket med tre målte årsager» → «tre målte varianter (senere fire — se runde 5)».
3. **Runde 5, korrektion:** «rettet til **fire** (a-d)» → «rettet til **fire varianter** (a-d)» (telefonens formulering: (d) er en undermængde af «ukendt metode» med anden mekanisme — «varianter» er præcisere end «årsager»).
4. **Runde 5, status:** «fire målelige årsager» → «fire målelige varianter».

Konventionen (præciseret i A8-korrektionsdoc's runde 6): analysedokumenters krop er append-only **undtagen korte, utvetydige faktafejl**, der ellers ville leve i modstrid med README og de andre analysedokumenter; README redigeres in-place som levende indeks.

---

## Kropsrettelser i runde 7 (in-place, noteret efter konventionen) ● — gennemlæsning af `5a2d8db` (relæ `5ea9ae1f`)

Telefonen gennemlæste `5a2d8db` og fandt A9's fire terminologibyte udført ● — men «ubetinget»-fejlen stod stadig **tre steder i kroppen**, mens appendet sagde det modsatte. Samme selektivitet som runde 5 blev kritiseret for: konventionen blev anvendt på terminologi («årsager» → «varianter»), ikke på fakta («ubetinget»). Rettet i kroppen i denne commit:

1. **§2-tabel** (tidligere linje 51): «`callback_kinds=["approval"]` er ubetinget» → «`callback_kinds` starter som `["approval"]` — `approval` tilføjes **ubetinget**, listen får `"user_input"` ved `elicitation.form`».
2. **§4 punkt 1** (tidligere linje 100): «er `["approval"]` uden betingelse» → «er `["approval"]` uden betingelse **i v1** (`approval` ubetinget; `"user_input"` tilføjes ved `elicitation.form`)».
3. **§4 følge** (tidligere linje 103): «altid `["approval"]`» → «der altid indeholder `"approval"`».
4. **§6** (tidligere linje 120): «Ikke desto:» → «Ikke desto mindre:» (afskåret udtryk).

README række 44 rettet in-place: runde 6 tilføjet til rundenummereringen, «tredje/fjerde årsag» → «tredje/fjerde variant» (konsistent med række 42), runde 6's kropsrettelser nævnt.

**R14 ●:** append-only-konventionen gælder **indhold**, ikke kun **ordvalg** — en terminologibyte uden den tilsvarende indholdsrettelse efterlader kroppen i modstrid med appendet. *(Formuleret som regel i runde 8; tidligere stod den som «Lærd lektion (R14 ◐)» med en fejlagtig datering — bisætningen «modsiger resten af samme dokument» blev først indført i A8-doc's runde 7, og ◐ er ikke defineret i forhold til ●/○. R14 er nu en regel, ikke en observation.)*

**Status efter runde 7 ●:** A9's tre «ubetinget»-steder og «Ikke desto» rettet. *(Statuslinjen kropsrettet i runde 8: «README række 44 konsistent» og «Alle tre dokumenter konsistente» var skrevet før README blev læst efter — telefonen påviste, at række 44 manglede runde 7 og tillagde runde 7's arbejde til runde 6. Efter runde 8's README-rettelser gælder påstanden.)* A8-doc's H1, konvention, runde 6-note og §6-kolonne ligeledes rettet (se A8-doc's runde 7). A9's to-række-tabel uændret. Ingen arkitekturvalg genåbnet.

---

## Kropsrettelser i runde 8 (in-place, noteret efter konventionen) ● — gennemlæsning af `080ae2c` (relæ `f14eefcc`)

Telefonens gennemlæsning fandt to forhold i denne fil. Rettet i kroppen i denne commit:

1. **R14 formuleret som regel ●** (tidligere i runde 7's note): «Lærd lektion (tilføjet som R14 ◐)» antydede læring på tværs af sessioner, og ◐ er ikke defineret i forhold til ●/○. Desuden fejlagtig datering: bisætningen «modsiger resten af samme dokument» blev først indført i A8-doc's **runde 7** (samme commit), ikke runde 6. Nu: «**R14 ●:** append-only-konventionen gælder **indhold**, ikke kun **ordvalg**» — med den gamle ordlyd citeret i noten.
2. **Statuslinje** (runde 7's afslutning): «README række 44 konsistent» og «Alle tre dokumenter konsistente» var skrevet før README blev læst efter — telefonen påviste, at række 44 manglede runde 7 og tillagde runde 7's «tre ubetinget-steder» til runde 6. Kropsrettet med note; gælder efter denne rundes README-rettelser.

README række 44 rettet in-place: rundenummereringen udvidet til «2, 4, 5, 6, 7 og 8»; «runde 6: kropsrettelser — tre «ubetinget»-steder…» flyttet fra runde 6 til runde 7; runde 6's faktiske arbejde (terminologibyte «årsager» → «varianter») angivet; runde 8's R14-formulering tilføjet. Række 42 (A8-doc) ligeledes udvidet med runde 7 og 8.

**Status efter runde 8 ●:** A8-doc, A9-doc og README konsistente i krop og append — inklusive rundenummerering. *(Statuslinjen kropsrettet i runde 9: påstanden var skrevet før telefonens gennemlæsning af `feba661` — denne fil bar selv «ingen SDK-klient»-påstanden og den uskårne §3-spekulation, A8-doc bar fire fejl, og README manglede runde 8's to sidste punkter. Efter runde 9's rettelser gælder den.)* A9's to-række-tabel uændret. Ingen arkitekturvalg genåbnet.

---

## Kropsrettelser i runde 9 (in-place, noteret efter konventionen) ● — gennemlæsning af `feba661` (relæ `1751d71a`)

Telefonen gennemlæste `feba661` med friske API-hentninger (ingen cache) og fandt seks fejl og fire nye punkter på tværs af A8-doc, denne fil og README. De A8- og README-rettede punkter står i A8-doc's runde 9 og README. Denne fil fik tre kropsrettelser i denne runde, hver noteret in-line med gammel ordlyd:

1. **§3 — `AttributeError`-spekulationen faktisk skåret væk** (telefonens fund 4): appendet «Rettelser fra PR-gennemlæsningen» punkt 3 erklærede sætningen skåret væk, men den stod stadig i kroppen («`AttributeError`-vejen ville først være nåelig, hvis … det sker kun på `request_optional_model`-ruterne …»). Nu fjernet; det målte står alene. A8-doc's §4 række 7 rettet tilsvarende («Skåret ned til det målte» → præcis tidsangivelse, runde 9).
2. **Runde 4, «Kolonneforveksling»-paragraf** (telefonens fund 5): «SDK'et er agenten, ikke klienten; der er ingen «SDK-klient»» + «Rettet her frem for in-place» stod i kroppen, mens A8-doc's runde 8 erklærede samme påstand «Forkert ●» og viste, at `acp/client/router.py` **er** SDK'ets klientside (registrerer `session/request_permission` og `session/update`, videresender til `client.*`). Paragrafen rettet: korrekt begrundelse (videresendelse til klientens egen `Client`-implementering), og «Rettet her frem for in-place» korrigeret — rettelsen skete in-place i A8-doc i runde 7. **Bemærk:** runde 8's egen note i denne fil havde kun to punkter (R14-formulering, statuslinje) og nævnte ikke denne kropsmodstrid — telefonens fund 5 dækkede den ikke eksplicit, men den fulgte af fund 5's påstand om, at A8's note erklærede fejlen «Forkert ●» uden tilsvarende rettelse her.
3. **◐-mærket defineret bort** (telefonens punkt a): §1 brugte «Sidefund om `legacy_api` **◐**» i samme fil, der (runde 8) erklærer «◐ er ikke defineret i forhold til ●/○». Sidefundet er verificeret (omvendt signatur-fallback målt i runde 2, `elif callable(func) and "_" not in attr:` + `inspect.signature`) → mærket **●**. Samme mærke rettet i README's R41-linje (se nedenfor) — R41-doc'en bruger selv «◐ delvist afgjort», så projektets faktiske betydning af ◐ er «delvist»; README's to R41-linjer er nu ens.
4. **Statuslinje** (telefonens §5): runde 8's afslutning påstod konsistens i samme commit, hvor denne fils to kropsmodstrider (punkt 1-2) stod. Kropsrettet med note; gælder efter denne rundes rettelser.

**Status efter runde 9 ●:** A8-doc, A9-doc og README læst igen efter alle rettelser — konsistente i krop og append, inklusive rundenummerering (runde 9 tilføjet i README's begge rækker). A9's to-række-tabel uændret. Ingen arkitekturvalg genåbnet.
