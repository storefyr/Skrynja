# Arkitekturvalg A1 + A2 + sidefund: `session/set_mode` vs `session/set_config_option` ●

## A1 — afbrydelse ● vedtaget

Enig med relæets forslag, med én præcisering:

1. Svar `cancelled` på udestående `request_permission` (`{"outcome":{"outcome":"cancelled"}}`, `DeniedOutcome` `schema.py:2414`) — **spec-krav**, før eller samtidig med cancel
2. Send `session/cancel` (notifikation, uden `id`) → `agent.cancel` (`agent.py:981`) → `interrupt()`
3. Forvent `stopReason: "cancelled"` på prompt-svaret (`agent.py:851-853`); filtrér nul-`usage` (L5)
4. Markér åbne `tool_call`s som `cancelled` **lokalt** — agenten sender ingen sådan update
5. **Procesgenstart som fallback** efter en kort frist (f.eks. 5 s uden `cancelled`-svar) — enig; det er korrekt defensiv adfærd ved transport-død, ikke ved almindelig cancel

## A2 — genoptagelse ● vedtaget

Sessionslaget bygges til at **indlæse historik**:

- Primær: `session/load` (`agent.py:555`) — `_load_complete_history` + `replay_session_updates` sendes som `session_update` **før** svaret
- Fallback: `session/new` ved `SessionNotFoundError` (session findes ikke længere — fx efter agent-genstart)
- `session/resume` (`agent.py:1133`) bruges **ikke** — samme `ResumeSessionIntent` men uden historik-replay

## Sidefund — to veje til mode ● besluttet: kun `set_config_option`

**Kodekæde (begge veje ender samme sted agent-internt):**

| | `session/set_mode` | `session/set_config_option` (`configId:"mode"`) |
|---|---|---|
| Router | `router.py:77-83` → `agent.set_session_mode` | `router.py:87-93` → `agent.set_config_option` |
| Agent | `agent.py:1042-1050` | `agent.py:1053-1095` (case `"mode"`, linje 1060) |
| Intern handling | `agents.switch(mode_id)` | `agents.switch(mode)` |
| Gate | `_is_primary_mode` (`agent.py:1754-1756`) — ellers **tavst `None`** | samme gate — ellers falder igennem til `InvalidRequestError` |
| Svar | tom `SetSessionModeResponse` | **hele `configOptions`** tilbage |

`_is_primary_mode` tjekker at mode-id er i `available_modes` = agenter med `agent_type == AgentType.AGENT` (`utils.py:130`) plus altid den aktive (`utils.py:133-134`). På telefonen er alle fire (`ask`, `accept-edits`, `smart-approve`, `auto-approve`) `AgentType.AGENT` (builtin order `manager.py:262-267`); `explore`/`lean` er `SUBAGENT` og afvises begge veje.

**Hvorfor kun `set_config_option`:**

1. **Én vej er simplere.** To kald-muligheder for samme tilstand er dobbelt så meget kode at teste og vedligeholde.
2. **Svaret bærer hele `configOptions`** — Skrynja får mode + model + thinking tilbage i ét kald og kan opdatere hele UI på én gang uden ekstra rundture.
3. **Bedre fejlrapportering.** `set_mode` returnerer tavst `None` ved ikke-primary mode — klienten kan ikke skelne «ukendt mode» fra «ikke tilladt mode». `set_config_option` giver `InvalidRequestError` med besked.
4. **`thinking` findes kun her.** Der er ingen ACP-metode til at sætte `thinking` direkte — kun `set_config_option` med `configId:"thinking"`. A7's `low`-undertrykkelse hører hjemme her.
5. Agenten pusher selv `config_option_update` ved agent-skift (`agent.py:743-745`, kommentar: «re-pushing the full config options here reflects the new profile (mode + model + thinking)») — så selv hvis mode ændres agent-internt (slash-kommando), får Skrynja besked via den samme kanal.

**A7 og vejen hænger sammen ●:** agentens `thinking`-validering er hardcoded `{"off","low","medium","high","max"}` (`agent.py:1069-1075`) — **ikke** mod modellens `thinking_levels`. Agenten accepterer `low` for zai-glm og fejler først ved API-kaldet med `3051`. A7 er derfor nødvendig klient-side uanset metodevalg: Skrynja bygger `thinking`-menuen fra `configOptions.thinking.options` **minus** forbudte (model, niveau)-par, og sender aldrig `low` for `glm-5-3`/`zai-glm-5-3-flash`. Fordi vi kun kalder `set_config_option`, er der kun én udgang at sikre.

## Opdaterede klientregler ●

| Regel | Kilde |
|---|---|
| Mode/model/thinking sættes **kun** via `session/set_config_option` | dette dokument |
| `session/set_mode` bruges **ikke** | dette dokument |
| Svaret fra `set_config_option` erstatter hele lokale `configOptions` | `agent.py:1093-1095` |
| `thinking`-menu = `configOptions.thinking.options` minus A7-forbudte par | A7 + `agent.py:1069-1075` |
| Ugyldig `configId` eller værdi → `InvalidRequestError` (vis fejl, behold lokal tilstand) | `agent.py:1089-1091` |
| Agent-internt modeskift (slash-kommando) → `config_option_update` notifikation | `agent.py:743-745` |
