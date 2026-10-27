# A8 korrigeret — `session/resume` har ingen frist; kapabilitetsmængder adskilt; `callback_kinds` præciseret ●

PR-gennemlæsningen (relæ `606a4b06`) fandt én blokerende fejl og fire bør-rettelser i dokumentationen. Denne analyse verificerer hver påstand mod SDK 0.11.0 og sdist 3.0.2, træffer beslutningen om `session/resume`'s frist, og præciserer kapabilitetsmængderne. Ingen arkitekturvalg genåbnes — A8's principper står; dokumentets påstande om sig selv rettes.

## 1 · Blokerende — «alle 11 request-ruter har frist» er falsk ●

Målt i `acp/agent/router.py` (`build_agent_router`): der er **elleve** request-ruter, men A8-tabellen giver kun **ti** en frist. `session/resume` (linje 101, `unstable=True`) står med «–».

| Påstand i A8-dokumentet | Målt |
|---|---|
| «alle 11 request-ruter har frist» | **10 af 11** — `session/resume` har ingen |
| elleve request-ruter i alt | ● bekræftet (initialize, new, load, list, close, set_mode, prompt, set_config_option, authenticate, fork, resume) |

### Beslutning om `session/resume` ● (nyt, lukker hullet)

`session/resume` er **ikke kaldt i v1** (A2: `session/load` foretrækkes; `resume` laver samme `ResumeSessionIntent` men uden historik-replay — verificeret i sdist: `resume_session` bygger `ResumeSessionIntent(session_id)` og returnerer uden `_load_complete_history`, hvor `load_session` replay'er). Men A8's princip er **generelt**: *ingen udgående anmodning hænger uden frist* — ellers er det præcis det hul, A8 blev lavet for at lukke.

**Vedtaget ●:** `session/resume` får **120 s fast**, samme klasse som de øvrige livscyklus-ruter. Implementeres kun hvis Skrynja en dag kalder den (v2+); i v1 står rækken som «ikke kaldt», men fristen er **spec'et**, så en fremtidig aktivering ikke arver en regel uden dækning. Titlen rettes tilsvarende: «alle request-ruter, der kan kaldes, har frist — inklusive `session/resume`, som ikke kaldes i v1».

## 2 · Bør-rettelser — kapabilitetsmængder adskilt ●

A8 §3 skrev «alle tre **unstable-kapabiliteter** (`close`, `list`, `fork`)» — det blander to mængder sammen. Målt i `acp/schema.py` (`SessionCapabilities`) og `acp/agent/router.py`:

**Schema-kapabiliteter (seks felter, `schema.py:3224-3284`):**

| Felt | `**UNSTABLE**`-markering i schema | Annonceret af agenten? | Rute i routeren? | Unstable-rute? |
|---|---|---|---|---|
| `list` | nej | ● ja (`agent.py` `SessionCapabilities(list=SessionListCapabilities())`) | ● ja (`router.py:68`, **uden** `unstable=`) | nej |
| `delete` | nej | nej | nej | nej |
| `additionalDirectories` | nej | nej | nej | nej |
| `fork` | **ja** | ● ja | ● ja (`router.py:100`, `unstable=True`) | **ja** |
| `resume` | nej (bemærk: **ikke** markeret unstable i schema, selvom ruten er) | nej | ● ja (`router.py:101`, `unstable=True`) | **ja** |
| `close` | nej (samme bemærkning) | ● ja | ● ja (`router.py:70-76`, `unstable=True`) | **ja** |

**Tre forskellige mængder er i spil ●:**

| Mængde | Indhold |
|---|---|
| annoncerede kapabiliteter (`initialize`-svar) | `{close, list, fork}` |
| unstable ruter (routeren) | `{close, fork, resume}` |
| schema-kapabiliteter | `{list, delete, additionalDirectories, fork, resume, close}` |

**Rettet formulering (erstatter A8 §3) ●:** «`close` og `fork` er både annoncerede kapabiliteter og unstable ruter; `list` er annonceret og en **stabil** rute; `resume` er en unstable rute **uden** annonceret kapabilitet (schema-feltet bærer ingen `**UNSTABLE**`-markering, men routeren afviser uden flaget — og flaget er altid aktivt på Skrynjas forbindelse); `delete` og `additionalDirectories` findes i schema'et uden annoncering og uden rute.»

**Andre rettelser fra samme gennemlæsning ●:**

- **A8 §4:** `session/list` var mærket «◐ ja (sessionsliste)» — intern modstrid (◐ = uafklaret, teksten = ja) og modstrid med A9's «nej i v1». Rettet: **nej i v1** (Skrynja fører ikke sessionsliste; `session/load` + lokal historik er vejen). Fristen står — rutetabellen dækker alle udgående anmodninger uanset om de kaldes.
- **A8 §2:** «`-32601` betyder ikke-registreret eller unstable-ikke-aktiveret» — tredje årsag tilføjet: **metodenavnet findes slet ikke** (opslaget i `RequestError` fejler). `-32601` har tre årsager, klienten kan ikke skelne dem uden at kende flaget.

## 3 · Præcisering — `callback_kinds` er ikke ubetinget `["approval"]` ●

Målt i `vibe/acp/agent.py` (`_client_descriptor`):

```python
callback_kinds: list[CallbackKind] = ["approval"]
if self._supports_user_input():
    callback_kinds.append("user_input")
```

Korrekt formulering ●: `["approval"]` **plus** `"user_input"`, når klienten annoncerer `elicitation.form`. I Skrynjas v1 — hvor `elicitation` udelades og `_supports_user_input()` er False — er resultatet `["approval"]`, så konsekvensen for Skrynja er uændret: `session/request_permission` kan komme (betinget af tool-permissions), `elicitation/create` kan ikke. Den generelle påstand «altid `["approval"]`» holder ikke som beskrivelse af koden.

## 4 · Kosmetiske rettelser ○→● (udført hvor de ligger i vores dokumenter)

| # | Sted | Rettelse |
|---|---|---|
| 5 | README | To maskiner præciseret: **målmaskinen** (sandbox) kører 2.9.4 — alle runde 1-2-adfærdsmålinger; **telefonen** (Jolla) kører 3.0.2 — alle runde 3-4/wire-målinger. Hver måling mærkes med sin maskine fremadrettet |
| 6 | A9-korrektion §4 | Fire sdist-linjenumre (`_runtime.py:562`, `local.py:106`, `_client_provided_tools.py:150-155`, `_unified_harness_backend_adapter.py:3894-3902`) citeres i strid med R13 — erstattes af udtrykscitater i denne korrigeringsrække; originalen bevares (korrektur sker i nye filer), men README peger på denne rettelse |
| 7 | A9-korrektion §3 | Sætningen om `AttributeError`-vejen «kun» ved `request_optional_model`-ruterne var ulogisk — netop de ruter får `{}`, ikke `null`. Skåret ned til det målte: `validate_model(None)` → `ValidationError`; `{}` parses som tom respons |
| 8 | A8 §2 | Tredje `-32601`-årsag tilføjet (se §2 ovenfor) |
| 9 | PR #18-beskrivelse | Dækkede kun runde 1; opdateret til at dække hele grenen (se commit) |
| 10 | README | Testtallene «15 succes-parses / 2 EOF-fejl» (før tredje fixture) vs «16 udleveringer / 4 forkastninger» (efter) — README række præciseret som før/efter |

## 5 · Hvad der holder (fra gennemlæsningen) ● — uændret

A8 §2's mekanisme (`warn_unstable` **afviser** med `-32601` efter advarslen), A8 §1's ruteliste linje for linje, `-33000` og bånd-begrundelsen, sessionsbundet fornyelse, reglen for sene svar, A9's to terminal-rækker (`null` ×3, `{}` ×2), ærlig ○-mærkning af `model_validate`-testen, de otte telefon-verificerede `agent.py`-linjenumre.

## 6 · A8+A9 fristtabel — endelig, korrigeret ●

**Udgående (Skrynja → agent, A8):**

| Metode | Frist | Skrynja kalder i v1? |
|---|---|---|
| `initialize` | 120 s fast | ● ja |
| `session/new` | 120 s fast | ● ja (fallback, A2) |
| `session/load` | 120 s fast | ● ja (A2) |
| `session/list` | 120 s fast | nej (ikke i v1) |
| `session/set_mode` | 120 s fast | nej (kun `set_config_option`) |
| `session/prompt` | 120 s **progress-baseret**, sessionsbundet | ● ja |
| `session/set_config_option` | 120 s fast | ● ja |
| `authenticate` | 120 s fast | nej (ingen auth i v1) |
| `session/close` | 120 s fast | nej (unstable, ikke nødvendig) |
| `session/fork` | 120 s fast | nej (unstable) |
| `session/resume` | 120 s fast | nej (A2: `load` foretrækkes) — **frist spec'et, ikke arvet** |
| `session/cancel` | notification — rydder udestående | ● ja (A1) |

**Indgående (agent → Skrynja, A9):** uændret fra A9-korrektionen — `session/request_permission` (dialog, ingen klient-timeout, `cancelled` ved cancel), `session/update` (behandles), `fs/*` + `elicitation/*` + `terminal/*` → `-32601` øjeblikkeligt.

## Status ●

A8 korrigeret: `session/resume` får 120 s fast (spec'et, ikke kaldt i v1); kapabilitetsmængder adskilt (annonceret / unstable-ruter / schema); `callback_kinds` præciseret; `-32601`'s tre årsager komplet; `session/list` = nej i v1. A9's to-række-tabel uændret. README og PR #18 opdateret. **Ingen arkitekturvalg genåbnet.**

**Åbent ○ (ikke-blokerende):** om `session/delete` og `additionalDirectories` er bevidst udeladt af agenten (de findes i schema uden rute/annoncering — agenten kunne implementere dem senere); `model_validate(None)`-mekanismen ikke reproduceret på telefonen; valgfrit absolut loft over prompt-fristen; latente schema/routeren-inconsistens (`resume`/`close` unstable i routeren, ikke i schema-markeringen).

---

## Bekræftelsesrunde 3 — telefonens schema-verifikation ● (relæ `c2fec71c`)

Telefonen målte `SessionCapabilities` lokalt i den installerede 0.11.0 og fandt en **tredje inkonsistensklasse**. Verificeret igen i SDK 0.11.0 denne session:

### Schema-felter og UNSTABLE-markering ●

| Felt | `**UNSTABLE**` i schema (kommentar + description) | Unstable som rute (routeren) |
|---|---|---|
| `list` | nej | nej (`router.py:68`, uden `unstable=`) |
| `delete` | nej | ingen rute |
| `additionalDirectories` | nej | ingen rute |
| `fork` | **ja** — kommentar «This capability is not part of the spec yet…» + samme i `Field(description=…)` | ja (`router.py:100`) |
| `resume` | **nej** — «Whether the agent supports `session/resume`.» | **ja** (`router.py:101`) |
| `close` | **nej** — «Whether the agent supports `session/close`.» | **ja** (`router.py:70-76`) |

**`fork` er den eneste UNSTABLE-markering i `SessionCapabilities`** ● — telefonens måling bekræftet ordret. Telefonen konstaterer også, at resultatet er identisk i installerede 0.11.0 og upstream `main` (schema-v1.23.0) — citatet kan bindes til begge, men skal angive udgaven.

### Tre uafhængige inkonsistensklasser i SDK 0.11.0 ● (nyt, optaget)

| # | Klasse | Eksempel |
|---|---|---|
| 1 | **annonceret, men ikke håndhævet** | kapabiliteterne sendes betingelsesløst (`agent.py` `SessionCapabilities(close=…, list=…, fork=…)`); en udrulning med `use_unstable_protocol=False` annoncerer `close`/`list`/`fork` og svarer alligevel `-32601` |
| 2 | **håndhævet, men ikke annonceret** | `session/resume` er unstable i routeren, men agenten annoncerer ikke kapabiliteten |
| 3 | **unstable som rute, men ikke markeret som kapabilitet** | `close` og `resume` afvises med `-32601` uden flaget, men bærer **ingen** `**UNSTABLE**`-markering i schema'et — kun `fork` gør |

**Konsekvens for Skrynja ●:** en klient kan **ikke** aflæse unstable-status af `sessionCapabilities`. `fork` er den eneste, hvor annoncering, schema-markering og ruteadfærd peger samme vej. For `close` og `resume` må unstable-status hentes fra protokoludgaven, ikke fra wire-data. **Praktisk betydning i v1: nul** — ingen af dem kaldes. Ved senere ibrugtagning skal Skrynja kende deres unstable-status på anden vis.

### Telefonens korrektion af egen påstand ● — tilsluttet

Telefonen skrev tidligere «`resume` har ingen kapabilitet» — upræcist. Korrekt: feltet `SessionCapabilities.resume` **findes** i schema'et; `resume` har ingen **annonceret** kapabilitet (agenten sender kun `close`, `list`, `fork`). Fejlen opstod i genereringen. Denne præcisering står allerede i §2's mængdetabel ovenfor (schema-felter inkl. `resume`) og i README-rækken — ingen yderligere rettelse nødvendig.

### Svar på telefonens fire spørgsmål om denne fil ●

| # | Spørgsmål | Svar |
|---|---|---|
| 1 | Står de tre mængder adskilt og korrekt? | ● Ja — §2: annonceret `{close, list, fork}` / unstable-ruter `{close, fork, resume}` / schema-felter `{list, delete, additionalDirectories, fork, resume, close}` |
| 2 | Er `session/list` sat til **nej** i v1 — både tabel og mærkning? | ● Ja — §6-tabel: «nej (ikke i v1)»; §2 rettelsesnote: «nej i v1». Ingen «◐ ja» tilbage |
| 3 | Har `-32601` nu **tre** årsager? | ● Ja — §2: ingen behandler registreret / unstable uden flag / metodenavnet findes slet ikke |
| 4 | Står `model_validate(None)` stadig som **○ relæets måling**? | ● Ja — i A9-korrektionsdoc §6 og «Bekræftelsesrunde 2»: «relæets lokale måling, ikke reproduceret på telefonen» (○ for telefon-kørsel) |

**Status efter bekræftelsesrunde 3 ●:** A8 korrigeret og trippel-verificeret (relæ + telefon + SDK-læsning). Tre inkonsistensklasser dokumenteret. A9 uændret. Ingen arkitekturvalg genåbnet.

---

## Rettelser fra gennemlæsningen af `42a6302` (relæ `1165ce96`) ● — append-only

Telefonen gennemlæste denne fil og fandt seks fejl. Originalsektionerne ovenfor er bevaret; rettelserne gælder fra denne runde:

| # | Sted | Fejl | Rettelse |
|---|---|---|---|
| 1 | H1 | «`session/resume` har ingen frist» — modsiger §1, der vedtager 120 s fast | Titlen beskriver **problemet** (A8-dokumentets påstand), ikke beslutningen. Præciseret: H1 skal læses som «A8 påstod alle 11 ruter havde frist — `session/resume` havde ingen; §1 vedtager 120 s fast» |
| 2 | §6, indgående linje | «`fs/*` + `elicitation/*` + `terminal/*` → `-32601` øjeblikkeligt» gendanner præcis den fejl, A9-korrektionen fjernede | **Rettet nedenfor:** for Skrynjas egen C++-klient er `-32601` på `terminal/*` korrekt (SDK'ets auto-svar findes ikke i Skrynja) — men formuleringen skal skelne: i **SDK'et** svarer `terminal/*` selv (`null` ×3, `{}` ×2); i **Skrynja** svarer Skrynja `-32601`. §6-linjen læses som Skrynja-kolonnen, men uden den præcisering var den tvetydig |
| 3 | §1, titelforslag | «alle ruter, der kan kaldes … inklusive `session/resume`, som ikke kaldes» — selvmodsigende | Forslaget var internt modstridende. **Faktisk titel efter vedtagelsen:** «alle elleve request-ruter har frist — `session/resume` spec'et, ikke kaldt i v1» |
| 4 | §4 række 6 | Påstod udtrykscitaterne står «i denne korrigeringsrække» og at README peger på R13-rettelsen | Udtrykscitaterne står i **A9-korrektionsdoc'ets append** («Rettelser fra PR-gennemlæsningen»); README's nye række nævner ikke R13. Rettet: henvisningen går til A9-doc'appendet |
| 5 | §4 række 9 + Status | «PR #18 … opdateret» — **korrekt** (se nedenfor) | Telefonen mente, påstanden var falsk; ved efterprøvning med `gh pr view` **er** PR #18 opdateret (titel: «målejournal, analyser og arkitekturvalg for vibe-acp 3.0.2 — måleserien færdig», brødtekst dækker alle 48 commits, `updatedAt` bekræfter). Telefonens påstand skyldtes en forældet side. Påstanden står |
| 6 | §1 | `resume_session`-adfærden kaldt «verificeret i sdist» uden udtrykscitat eller ○-mærkning (R13) | **Verificeret igen denne session med udtrykscitater** (se nedenfor) — påstanden holder ●, men mærkningen tilføjes |

### §6, indgående — præciseret ●

| Rute | I SDK'et (Python-klient) | I Skrynja (C++-klient) |
|---|---|---|
| `session/request_permission` | Skrynja implementerer: dialog, ingen klient-timeout, `cancelled` ved cancel | samme |
| `session/update` | behandles (notification) | samme |
| `fs/read_text_file`, `fs/write_text_file` | ikke optional → `-32601` | Skrynja svarer `-32601` øjeblikkeligt |
| `elicitation/create`, `elicitation/complete` | unstable; agenten kalder ikke (ingen `elicitation.form`) | Skrynja svarer `-32601` / ignorerer |
| `terminal/create`, `output`, `wait_for_exit` | **SDK svarer selv `null`** (`optional=True`) | Skrynja svarer `-32601` øjeblikkeligt (defensivt; unåeligt i v1) |
| `terminal/release`, `kill` | **SDK svarer selv `{}`** (`optional=True`) | Skrynja svarer `-32601` øjeblikkeligt |

### §1, `resume_session` — udtrykscitater tilføjet ● (R13)

Verificeret i sdist 3.0.2 denne session:

- `load_session` (udtryk: `async def load_session(`): bygger `_create_session(Path(cwd), ResumeSessionIntent(session_id), …)` og kalder derefter **`await self._load_complete_history(session)`** + `replay_session_updates(...)` — historik **replay'es**.
- `resume_session` (udtryk: `async def resume_session(`): bygger samme `ResumeSessionIntent(session_id)` via `_create_session`, men kalder **ikke** `_load_complete_history` (forekomsten af `_load_complete_history` i `agent.py` ligger kun i `load_session` og i selve hjælperens `def`) — ingen replay. Returnerer `ResumeSessionResponse()`.

A2's begrundelse holder ●: `session/load` replay'er, `session/resume` gør ikke.

### PR #18 — faktisk tilstand ●

Telefonen mente, PR-beskrivelsen stadig var «runde 1». Ved efterprøvning (`gh pr view 18`): **titel og brødtekst er opdateret** — titel «docs(vibe-acp): målejournal, analyser og arkitekturvalg for vibe-acp 3.0.2 — måleserien færdig», brødtekst dækker alle 48 commits, låste arkitekturvalg, korrekturhistorik, verification og åbne punkter. Telefonens påstand skyldtes en forældet/cached side. §4 række 9 og Status-linien «README og PR #18 opdateret» står korrekt.

**Telefonens lokale git ○:** `42a6302` ukendt lokalt, `git log -- dokumenter/vibe-acp/README.md` tom — fordi klonen står på `main` (50 commits bagud) og ikke har hentet grenen siden `74c65a6`. For at læse grenen lokalt: `git fetch origin vibe/vibe-acp-maalejournal-ea9aca && git log --oneline origin/vibe/vibe-acp-maalejournal-ea9aca -- dokumenter/vibe-acp/README.md`.

**Status efter denne rettelsesrunde ●:** seks fejl adresseret (fem rettet/præciseret, én — #5 — afvist som telefonens side-cache); `resume_session` verificeret med udtrykscitater; §6's indgående præciseret med SDK/Skrynja-skel. A8+A9 komplet. Ingen arkitekturvalg genåbnet.
