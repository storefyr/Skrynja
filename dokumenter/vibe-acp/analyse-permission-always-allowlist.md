# Permission `always` + allowlist: hvorfor dialogen udeblev — og hvad det betyder for Skrynja

**Dato:** 26.10.2026 · **Trigger:** Relæ-besked ○ — årsagen til udeblevne dialoger er `permission = "always"` (to `[tools.*]`-sektioner) plus `id` i allowlist i `~/.vibe/config.toml`, skrevet midt i måleserien. **Forklaringen er filkonfigureret, ikke mode- eller `_is_primary_mode`-relateret.**
**Kilde:** PyPI sdist `mistral_vibe-3.0.2.tar.gz`

---

## Verifikation i koden ●

### 1. `permission = "always"` i `[tools.*]` — config-niveau

`vibe/core/config/vibe_schema.py:573-574`:
```python
tools: Annotated[dict[str, dict[str, Any]], ...]
```

Hver tool-sektion (`[tools.bash]`, `[tools.file_system.read]`, …) kan sætte `permission`. Enum (`vibe/core/tools/models.py:14-17`):

```python
class ToolPermission(StrEnum):
    ALWAYS = auto()
    NEVER = auto()
    ASK = auto()
```

### 2. Hvor `ALWAYS` kortslutter prompten — tre lag

**Lag A — bash-toolen selv** (`vibe/core/tools/builtins/bash.py:574-575`):
```python
if self.config.permission == ToolPermission.ALWAYS:
    return True   # _is_unconditionally_allowed → PermissionContext(ALWAYS)
```

**Lag B — allowlist-match** (`vibe/core/tools/builtins/bash.py:506-509` + `utils.py:95-97`):
```python
def _is_allowlisted(self, command: str) -> bool:
    return any(_matches_pattern(command, pattern) for pattern in self.config.allowlist)
```
Allowlist-kommandoer (som `id`) får `PermissionContext(permission=ToolPermission.ALWAYS)` — uanset `permission`-feltet.

**Lag C — unified permission resolver** (`vibe/app_server/_unified_permissions.py:183-186, 481-487`):
```python
_OUTCOME_BY_PERMISSION: dict[ToolPermission, Literal["allow", "deny"]] = {
    ToolPermission.ALWAYS: "allow",
    ToolPermission.NEVER: "deny",
}
...
fixed = _OUTCOME_BY_PERMISSION.get(context.permission)
if fixed == "allow":
    allowed = True
    continue   # ← springer videre uden required_permissions
```

Hvis **alle** tools i en builtin ender som `allow` (eller abstain), og der ingen `uncovered` permissions er:

```python
if uncovered:
    return PermissionOutcome(decision="ask", ...)
return PermissionOutcome(decision="allow", authorized_path=authorized_path)
```

**`decision="allow"` betyder: ingen `request_permission` til klienten.** ACP-prompten i `vibe/acp/agent.py:896` (`self.client.request_permission(...)`) kaldes **kun** når der er en `ApprovalCallbackDetail` — altså når beslutningen er `ask`.

### 3. Konklusion ●

| Konfiguration | Effekt | Prompt? |
|---|---|---|
| `[tools.bash] permission = "always"` | Alle bash-kald auto-godkendes | **Nej** |
| `id` i `[tools.bash] allowlist` | `id` matcher allowlist → ALWAYS | **Nej** (for `id`) |
| `[tools.file_system.*] permission = "always"` | Fil-læs/skriv auto-godkendes | **Nej** |
| Ingen af delene | `ASK` → `request_permission` til klient | **Ja** |

Relæ-AI's forklaring er **● bekræftet i koden**: `permission = "always"` + allowlist-match giver `decision="allow"` i `_unified_permissions.py`, og ACP-klienten får aldrig en `request_permission`. Det har **intet med `mode` eller `_is_primary_mode()` at gøre** — mode (`ask`/`accept-edits`/`smart-approve`/`auto-approve`) styrer agent-profilen, mens `permission = "always"` er en **tool-konfiguration** der ligger under og uafhængigt af mode.

---

## Det åbne spørgsmål — lukket ●

| Spørgsmål | Tidligere status | Ny status |
|---|---|---|
| Undertrykker `smart-approve`/`auto-approve` dialogen? | ○ (måling ugyldig) | **Lukket uden måling** — årsagen var filkonfiguration, ikke mode. Mode-spørgsmålet er stadig ○ teoretisk, men **irrelevant for Skrynja**: Skrynja skal forholde sig til `request_permission` når den kommer, og respektere at den ikke kommer når agenten er konfigureret til auto-allow |

---

## Konsekvens for Skrynja-klienten ●

1. **Permission-dialogen er betinget — ikke garanteret.** Skrynja skal implementere `session/request_permission` (med `id: 0`, svarformat `{"outcome":{"outcome":"selected","optionId":"…"}}`), men må **ikke** antage at den nogensinde kaldes. Hvis brugeren har `permission = "always"` i config, kommer der ingen prompts.

2. **Skrynja skal vise hvad agenten faktisk kører.** Når der ingen prompt kommer, kører agenten værktøjet direkte — Skrynja ser det via `tool_call`/`tool_call_update` i `session/update`. Det er klientens **eneste** indikator på at noget kører.

3. **`allow_always` / `allow_always_permanent` svar fra klienten** — når de **er** tilgængelige — svarer til `APPROVE_FOR_SESSION` / `APPROVE_PERMANENTLY` (`agent.py:912-915`), som agenten persisterer (session-scope eller permanent i config). Skrynja skal kende forskellen: `allow_always` gælder sessionen, `allow_always_permanent` skriver til config.

4. **A5/H2 (fra vejkortet):** `allow_always_permanent` viser `session_pattern` og kræver eksplicit bekræftelse — dette er klientens ansvar, ikke agentens.

5. **`bypass_tool_permissions`** (`vibe_schema.py:816`) — en global config der også slår prompts fra. Samme konsekvens som `permission = "always"`.

---

## Opdateret status

| Punkt | Status |
|---|---|
| Åbent spørgsmål: «undertrykker smart-approve/auto-approve dialogen?» | ● **Lukket** — årsagen var `permission="always"` + allowlist i config.toml, ikke mode |
| `_is_primary_mode()` | ○ irrelevant for Skrynja — ligger i Rust-laget, og mode påvirker ikke om `request_permission` sendes (det gør `permission`-config) |
| A5/H2 | ● uændret — klienten skal vise `session_pattern` og kræve bekræftelse for `allow_always_permanent` |
| Fase 3 (værktøjskald og tilladelse) | ● kan implementeres — kontrakten er klar: `request_permission` kommer **når** agenten beslutter `ask`, aldrig ved `allow` |

## Anbefaling til Skrynja (ny)

- Implementer `request_permission`-håndtering som **defensiv** kode — den kaldes betinget.
- Vis altid `tool_call`/`tool_call_update` i UI, så brugeren kan se hvad agenten kører selv uden prompts.
- Overvej en indikator når agenten kører uden at spørge (f.eks. «kører uden godkendelse» ved tool_calls når ingen permission-dialog blev vist).
