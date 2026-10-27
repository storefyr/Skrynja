# Lag 3 korrigeret — læring kun ved positiv evidens ● + R34 afgjort ●

## Indvending mod lag 3 ● accepteret

Relæet har ret: `3051` er **ikke** et særkende for `low`. Koden kommer fra harness' `model_stream_failed` → `TurnErrorCode.BACKEND_ERROR` → ACP `-32603` med `data.code = "3051"`, `data.raw_status_code = 400`. Samme kode kan komme fra enhver HTTP 400 fra provideren — kontekstoverløb, ugyldigt billede, kvote, ugyldig parameter af hvilken som helst art. Hvis lag 3 lærte ved enhver `3051`, ville `low` blive undertrykket for modeller, hvor det virker fint, uden synlig årsag.

## Hvad fejlen faktisk indeholder på wire ● (kodelæst)

Kæden for en provider-fejl (`reasoning_effort: "none"` afvist af zai-glm):

```
harness _completion.py: _stream_failed_event
  └─ RustProtocolError(code="model_stream_failed",
       message=read_provider_failure(exc).message,   ← provider's egen forklaring
       details=_failure_details(...))
       └─ details = {provider, model, purpose, exceptionType, httpStatus, correlationId?}

adapter _unified_harness_backend_adapter.py:
  _harness_turn_error_code: "model_stream_failed" → TurnErrorCode.BACKEND_ERROR
  (kun 429→RATE_LIMIT og credential-status→INVALID_API_KEY omdannes; 400 bliver BACKEND_ERROR)

app_server session.py:400: AppServerTurnError(event.turn.error)
  └─ PublicTurn.error = PublicError(message, code=BACKEND_ERROR, details={...})

acp agent.py:854-860: AppServerTurnError → from_public_error(exc.error)
  └─ BACKEND_ERROR falder i `case _:` → InternalError(error.message)
  └─ InternalError → ACP JSON-RPC error -32603, message = error.message
```

**Det afgørende ●:** `error.message` bærer **providerens egen forklaring** fra response body (`_provider_explanation`, `_provider_failure.py:249-269` — læser `error.message`/`message`/`detail` fra JSON-body'en). For zai-glm's afvisning af `reasoning_effort:"none"` måles beskeden på wire som:

```
"API error occurred: Status 400 …" med code:"3051", raw_status_code:400
```

**Men ○ kritisk:** `3051` og `raw_status_code` findes **ikke i sdist-koden** — de kommer fra providerens response body (Mistral API's egen fejlkode), ikke fra harness. Beskeden «API error occurred: Status 400 …» er ligeledes provider-genereret. Så:

- `data.code` (`"3051"`) = providerens fejlkode — **ikke entydig for reasoning_effort**
- `error.message` = inneholder providerens forklaring — **kan** nævne `reasoning_effort` eller parameter-navnet, men det afhænger af providerens formulering
- `details` fra `_failure_details` (provider, model, purpose, httpStatus) når **ikke** ACP-klienten — `from_public_error` kaster alt andet end `message` væk for `BACKEND_ERROR`

**Konsekvens for Skrynja ○:** klienten ser kun `-32603` + `message` + `data.code`. Der er **ingen struktureret markør** for «årsag = reasoning_effort». Eneste evidens er tekst-mønster i `message`.

## Lag 3 — korrigeret regel: læring kun ved positiv evidens ●

| Betingelse | Handling |
|---|---|
| `-32603` OG `data.code == "3051"` OG `message` matcher mønster for reasoning/parameter-afvisning (f.eks. indeholder `reasoning_effort`, `reasoning`, eller `none` i parameter-kontekst) OG første prompt efter thinking-skift til `low` på samme model | ● lær — tilføj alias til `acp/thinkingSuppressLowLearned` |
| `-32603` + `3051` uden entydigt mønster i `message` | ○ vis fejl, rør ikke listen |
| Enhver anden fejlkode | ○ vis fejl, rør ikke listen |

**To betingelser skal begge være opfyldt ●:**

1. **Tekstuel evidens** — `message` matcher et mønster, der entydigt peger på reasoning-parameteren. Mønsteret vedligeholdes i klienten (kan udvides uden genbygning? nej — det er kode; men det er smalt og sjældent ramt).
2. **Kontekstuel evidens** — fejlen opstår på **første `session/prompt` efter et `thinking`-skift til `low`** på den model. Denne betingelse alene er stærk: hvis `low` lige er valgt, og prompt fejler med det samme, er årsagen næsten sikker `low→"none"`.

**Ekstra krav — synlighed og fortrydelse ● (accepteret):**

- Når lag 3 skriver, vises det i UI'et: «Low er ikke tilgængelig for `<model>` — skjult automatisk».
- Der skal være en **synlig vej til at tømme** lag 3 — ikke kun nøglen. Forslag: en indstillingsside (eller en «Nulstil»-knap ved thinking-menuen) der tømmer `acp/thinkingSuppressLowLearned`. Lag 2 (`acp/thinkingSuppressLow`) røres ikke.

**Forsigtighed ved tekst-mønster ○:** providerens formulering kan ændre sig. Hvis mønsteret ikke matcher, lærer klienten ikke — det er den sikre retning (falsk negativ frem for falsk positiv). Betingelse 2 (kontekst) er den primære; tekst-mønsteret er sekundær bekræftelse. Overvej: lær kun ved betingelse 2 alene, hvis tekst-mønsteret viser sig ustabilt — men så risiko for at lære ved en tilfældig 400 lige efter et skift. **Begge betingelser sammen er vedtaget.**

## R34 afgjort ●

`harbour-skrynja.cpp:26-27`:

```cpp
app->setOrganizationName("org.skrynja");
app->setApplicationName("Skrynja");
```

Med `QStandardPaths::AppConfigLocation` på Sailfish/Linux bliver stien:

```
~/.config/org.skrynja/Skrynja/Skrynja.conf
```

Det er én af de to målte stier ● — den **aktive** er `~/.config/org.skrynja/Skrynja/Skrynja.conf`. Den anden (`~/.config/harbour-skrynja/Skrynja.conf`) er et levn fra før organizationName blev sat (eller en ældre packaging). **R34 lukket ●.**

Lag 2 og 3 bliver dermed læsbare og redigerbare INI-filer (QSettings NativeFormat) på en kendt sti. Brugeren kan tømme lag 3 ved at slette nøglen `acp/thinkingSuppressLowLearned` — og med den nye synlige «Nulstil»-vej behøver de ikke kende nøglen.

## Opdaterede klientregler ●

| Regel | Kilde |
|---|---|
| Lag 3 lærer kun ved `-32603` + `3051` + entydigt tekstmønster + første prompt efter skift til `low` | denne analyse |
| Lag 3 skriver til `acp/thinkingSuppressLowLearned` i `~/.config/org.skrynja/Skrynja/Skrynja.conf` | R34 ● |
| Synlig «Nulstil»-vej tømmer lag 3, rører ikke lag 2 | denne analyse |
| Ved læring: bruger informeres «Low ikke tilgængelig for `<model>` — skjult» | denne analyse |
| Falsk negativ (ikke lært) er acceptabel; falsk positiv (lært forkert) er ikke | denne analyse |

## Alle arkitekturvalg låst ●

A1, A2, kun `session/set_config_option`, A7 som 3-lags tilstand med positiv-evidens-læring, thinking-UI med «samme som High», N8/R12, L5, R37/R42, Trin E. R34 afgjort.
