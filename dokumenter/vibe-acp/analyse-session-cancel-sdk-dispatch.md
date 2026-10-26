# `session/cancel` — SDK-dispatch vs agent-registrering ● kodelæst

**Spørgsmål fra relæ-AI:** er fraværet af strengen `"session/cancel"` i `vibe/acp/` meningsfuldt, eller et artefakt af SDK-dispatch?

**Svar ●:** Fraværet er et **artefakt af SDK-dispatch** — tekstsøgningen var værdiløs som bevis. `session/cancel` er **fuldt understøttet** og implementeret i agenten.

## 1 · SDK'et dispatcher selv ●

`agent_client_protocol` 0.11.0 (Python SDK, wheel `acp/`):

```
acp/meta.py:14                "session_cancel": "session/cancel",
acp/agent/router.py:103       router.route_notification(
                                  AGENT_METHODS["session_cancel"], CancelNotification, agent, "cancel")
```

SDK'et oversætter metodenavnet `session/cancel` til Python-metoden `agent.cancel(...)` — agenten behøver aldrig at nævne strengen selv. Relæ-AI'ens præmis er korrekt: **tekstsøgning efter metodenavn-strengen er kun gyldig evidens, hvis agenten selv registrerer metoden.** Det gør den ikke her.

## 2 · Agenten implementerer `cancel` ●

`vibe/acp/agent.py:981-987`:

```python
@override
async def cancel(self, session_id: str, **kwargs: Any) -> None:
    del kwargs
    session = self._get_session(session_id)
    session.app_server.resources.telemetry.record(
        "vibe.user_cancelled_action", {"action": "interrupt_agent"}
    )
    await session.cancel_prompt()
```

Kæden videre:

```
agent.py:981  cancel(session_id)
  └─ session.py:40  AcpSession.cancel_prompt()
       ├─ hvis ingen aktiv turn: return (no-op, ingen fejl)
       └─ app_server.interrupt()
            └─ session.py:769  Session.interrupt()
                 └─ "turn/interrupt" request mod app-server (med expected_turn_id)
```

Svaret på prompt-requestet kommer derefter fra `agent.py:851-853`:

```python
except asyncio.CancelledError:
    if self._client_connection_closed():
        raise  # forbindelsen er lukket — intet svar sendes
    self._send_usage_update(session)
    return PromptResponse(stop_reason="cancelled", usage=self._usage(session))
```

**Wire-adfærd ● (allerede målt på telefonen tidligere):** `cancel` sendes som notifikation **uden `id`**; prompt-svaret kommer som `stopReason: "cancelled"` med `usage` sat til nul (L5: filtrér `used == 0`).

### Udestående tilladelsesanmodninger ved cancel

Spec'en siger klienten SKAL svare `cancelled` på udestående `request_permission`. Agentens implementering:

- `_forward_event` (`agent.py:725-732`): ved `CallbackRequested` kaldes `_answer_callback`, som **awaiter** `self.client.request_permission(...)` (linje 896) — altså blokerer på klientens svar.
- Ved cancel: `session.cancel_prompt()` → `interrupt()` → app-server lukker den aktive turn. Dette lukker også den åbne callback (jf. kommentaren ved `agent.py:729`: «A cancel that lands while the client is still answering closes the callback first; the turn then reports its own stop reason»).
- Hvis klienten derefter alligevel svarer på den gamle `request_permission` (id: 0), modtages svaret på en lukket callback → `AppServerResponseError` med `CALLBACK_CLOSED`, som bevidst **swallowes** (linje 730-732: `if exc.error.code is not ProtocolErrorCode.CALLBACK_CLOSED: raise`).

**Konsekvens for Skrynja ●:** ACP-spec'en **kræver**, at klienten svarer `cancelled` på udestående `request_permission`, når den sender `session/cancel`. Agentens nuværende adfærd (lukker callback ved interrupt, swallower `CALLBACK_CLOSED`) gør svaret *overflødigt i praksis* — men spec-kravet vejer tungere, og et ignoreret svar koster intet. Skrynja bør derfor ved cancel:

1. Sende svar på evt. udestående `request_permission` med `{"id": <heltal>, "result": {"outcome": {"outcome": "cancelled"}}}` (`DeniedOutcome`, `schema.py:2414-2415`) — **før eller samtidig med** cancel-notifikationen
2. Sende `session/cancel` (notifikation, uden `id`)
3. Lukke permission-dialogen lokalt, så brugeren ikke kan klikke «allow» på en død anmodning

Rækkefølgen (svar først, så cancel) er at foretrække: hvis interrupt når at lukke callback'en først, swallowes svaret — men hvis svaret når igennem først, afvises anmodningen pænt. Begge rækkefølger er sikre; svar-først er mest spec-korrekt.

### Forud-markering af værktøjskald

Spec'en siger klienten BØR markere igangværende tool calls som annullerede. Agentens adfærd: ved `stopReason: "cancelled"` kommer der ingen `tool_call_update` med status `cancelled` fra agenten (ingen sådan kode i `agent.py`). Skrynja må derfor selv sætte status `cancelled` på alle åbne `tool_call`s lokalt, når cancel sendes.

## 3 · `session/resume` — ikke ACP-stabil, men implementeret ● (vigtig korrektion)

Relæ-AI antog `session/resume` var app-server-niveau. Det er **forkert**:

```
acp/meta.py:19                "session_resume": "session/resume",
acp/agent/router.py:101       router.route_request(..., unstable=True)
vibe/acp/agent.py:1133        async def resume_session(...) — @override
```

- `session/resume` **findes i ACP SDK 0.11.0** (`meta.py:19`), registreret i routeren (linje 101) — men markeret **`unstable=True`**.
- Agenten bygger routeren med `use_unstable_protocol=True` (`agent.py:1859`):
  ```python
  agent=agent, use_unstable_protocol=True, observers=[acp_message_observer]
  ```
- Agenten **implementerer** `resume_session` (`agent.py:1133-1153`): den kalder `_create_session(..., ResumeSessionIntent(session_id), ...)` — samme intent som `load_session`.

**Forskel på `session/load` og `session/resume` i denne agent ●:**

| Metode | Linje | Historik | Replay |
|---|---|---|---|
| `session/load` | `agent.py:555` | `_create_session` + `ResumeSessionIntent` | `_load_complete_history(session)` + `replay_session_updates(...)` sendes som `session_update` **før** svaret |
| `session/resume` | `agent.py:1133` | `_create_session` + `ResumeSessionIntent` | **ingen** `_load_complete_history`, ingen replay — kun `_send_usage_update` |

Begge bruger `ResumeSessionIntent`, som genåbner den samme app-server-session (samme `session_id`). Forskellen er alene, om historikken læses og replays til klienten.

**Konsekvens for Skrynja ●:**
- `session/resume` er **tilgængelig** på denne agent (unstable, men aktivt fordi agenten sætter `use_unstable_protocol=True`), men den giver **ingen historik-replay**.
- `session/load` er den korrekte metode til at genåbne en session **med** historik — og den er ACP-stabil (ikke unstable).
- Skrynja skal altså bygge sessionslag **med** historikindlæsning via `session/load`, ikke `session/resume`. `initialize`-svaret viser `agentCapabilities.loadSession: true` ● (allerede målt).
- Hvis `session/load` fejler med `SessionNotFoundError` (session findes ikke længere på agentens side — fx efter agent-genstart), må Skrynja starte en **ny** session via `session/new`.

## Arkitekturvalg for Skrynja ● (svar på «hvilket valg er det»)

| Spørgsmål | Svar |
|---|---|
| Er `session/cancel` understøttet? | **Ja ●** — fuldt, via SDK-dispatch + `agent.cancel` (`agent.py:981`) |
| Skal Skrynja sende `cancel` alligevel? | **Ja** — det er den eneste måde at afbryde et aktivt turn på |
| Får Skrynja `stopReason: "cancelled"`? | **Ja ●** — målt på wire tidligere |
| Skal Skrynja svare `cancelled` på udestående permission? | **Ja ●** — spec'en kræver det; svar format `{"outcome":{"outcome":"cancelled"}}` (`DeniedOutcome`). Agentens swallow af `CALLBACK_CLOSED` gør det overflødigt i praksis, men kravet står. Svar før eller samtidig med cancel |
| Skal Skrynja markere tool calls som cancelled? | **Ja, lokalt** — agenten sender ingen `tool_call_update(status=cancelled)` |
| Genåbning med historik? | **`session/load`** (stabil, med replay) — ikke `session/resume` (unstable, ingen replay) |
| Fallback ved ukendt session? | `session/new` — ny session uden historik |
| Behøves procesgenstart som cancel-fallback? | **Nej** — cancel er fuldt understøttet. Procesgenstart kun ved transport-død |

## Lærd lektion ● (tilføjes som R13)

**R13:** *Tekstsøgning efter en protokol-metodes navnstreng i agent-koden er ikke gyldig evidens for fravær* — når dispatch ligger i SDK'et. Afbødning: læs SDK'ets router/meta (`acp/meta.py`, `acp/agent/router.py`) først, følg metodenavnet til agent-metoden, verificér implementeringen der.
