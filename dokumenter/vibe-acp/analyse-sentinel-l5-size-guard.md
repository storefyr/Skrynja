# Sentinel-analyse: kan `UNSET_AUTO_COMPACT_THRESHOLD` lække ind i `usage_update.size`?

**Dato:** 26.10.2026 · **Kilde:** PyPI sdist `mistral_vibe-3.0.2.tar.gz`
**Svarer på:** Relæ-AI's skærpning — hvad er `auto_compact_threshold`, når feltet ikke er sat, og kan sentinellen (`-1`) nå wire?

---

## Sentinellen

`vibe/core/config/_defaults.py:42-45`:
```python
# A -1 written by hand is therefore indistinguishable from unset and resolves
# the same way; 0 stays the documented way to disable compaction.
UNSET_AUTO_COMPACT_THRESHOLD = -1
```

Kommentaren er eksplicit: **-1 kan aldrig nå en modelpost som «sat» værdi** — den er merge-intern. Og `models.py:614-616`:
```python
# The unset sentinel is merge-internal; it must never reach a TOML file.
if entry.get("auto_compact_threshold") == UNSET_AUTO_COMPACT_THRESHOLD:
    del entry["auto_compact_threshold"]
```

---

## Kan -1 lække ind i `size`? — Nej, og her er hvorfor ●

`size` kommer fra `runtime.context_window`, som sættes i `RuntimeSnapshot` (`_runtime.py:2202`):

```python
context_window=active_model.auto_compact_threshold,
```

Nøglen er: **`active_model` er den løste `ModelConfig` efter ALLE validators har kørt** — inklusive `_apply_auto_compact_threshold` (`vibe_schema.py:1247-1276`), som er en `@model_validator(mode="after")`.

Den funktion garanterer, at hver model i `self.models` **altid** ender med en konkret, positiv tærskel:

```python
for alias, model in self.models.items():
    window = model.max_context_length
    cap = model.auto_compact_threshold
    if cap != UNSET and (window is None or cap < window):
        models[alias] = model          # cap er en ægte værdi (eller 0 = "aldrig compact")
        continue
    if global_set and (window is None or global_value < window):
        threshold = global_value       # global config-værdi eller DEFAULT (200 000)
    elif window:
        threshold = int(window * 0.8)  # 80 % af vinduet
    else:
        threshold = global_value       # DEFAULT_AUTO_COMPACT_THRESHOLD = 200_000
    models[alias] = model.model_copy(update={"auto_compact_threshold": threshold})
```

**I alle fire grene ender `auto_compact_threshold` med en konkret værdi:**

| Gren | Værdi | Kan den være -1? |
|---|---|---|
| Model-felt sat (`cap != -1`) | `cap` (brugerens/katalogets/GrowthBooks værdi, eller 0 = "aldrig") | Nej — betingelsen kræver `cap != -1` |
| Global sat, window None/lav | `global_value` | Nej — `global_value = global_cap if global_set else 200_000`, og `global_set` kræver `global_cap != -1` |
| Window sat | `int(window * 0.8)` | Nej — window er `int | None` med `ge=1` (models.py:527), så ≥ 1 → threshold ≥ 0 |
| Ellers (window None, global unset) | `global_value` = `DEFAULT_AUTO_COMPACT_THRESHOLD` = 200_000 | Nej — konstant |

**● Konklusion:** Efter `_apply_auto_compact_threshold` kan `auto_compact_threshold` ikke være -1. Sentinellen kan ikke lække ind i `size` via denne vej.

---

## Men: hvad med `0`? — Den CAN nå wire ●

Kommentaren i `_defaults.py:43-44` siger: **«0 stays the documented way to disable compaction»**.

Hvis en modelpost (eller global config) sætter `auto_compact_threshold = 0`:
- `cap = 0`, `cap != UNSET` → True
- `window is None or cap < window` → True (0 < window for ethvert window)
- → modellen beholder `0` uændret

Og så bliver `context_window = 0` i `RuntimeSnapshot`, og `size = 0` på wire.

**Relæ-AI's guard mod `size = 0` er derfor ikke paranoia — den er nødvendig ●**

Scenarier der giver `size = 0`:
- Brugeren har sat `auto_compact_threshold = 0` i `config.toml` (dokumenteret måde at slå kompaktion fra)
- En GrowthBook-payload sætter `0` for en model

---

## Hvad med før første `StatsUpdated`-event? ◐

`session.py:966`:
```python
if isinstance(event, StatsUpdated):
    self._state.stats = event.params.stats
    self._state.context_window = event.params.context_window
```

`context_window` opdateres **kun** ved `StatsUpdated`-events. Hvad er værdien før første event? Lad mig tjekke initial state...

`RuntimeSnapshot.context_window: int` (protocol.py:1102) — **påkrævet `int`, ikke `Optional`**. Så initial snapshot fra `project_runtime_snapshot` (`_runtime.py:2202`) sætter den til `active_model.auto_compact_threshold` — som efter validator altid er konkret.

Men `_state.context_window` i session — hvad er default? Det afhænger af `SessionState`-initialisering, som jeg ikke har fundet eksplicit. Hvis den initialiseres til 0 før første `StatsUpdated`, så kan en `usage_update` sendt tidligt have `size = 0` selv uden nogen bruger-konfiguration. **Dette er ○ — ikke verificeret i koden, bør måles.**

---

## `used` ved cancel — relæ-AI's påstand ● bekræftet

`AgentStatsSnapshot.context_tokens: int = 0` (`models.py:456`) — default 0. Ved cancel nulstilles usage (relæ-AI målte det: «ved cancel er used = 0»). Kombineret med `size = 0`-scenariet ovenfor giver `used / size` **0/0**.

---

## L5 — udvidet, præciseret ●

| Regel | Begrundelse |
|---|---|
| Filtrér `usage_update` hvor `used == 0` **og** `size == 0` | Nul-usage må ikke overskrive registreret forbrug (vejkort Fase 4) |
| **Guard mod `size <= 0`** før procentberegning | `size = 0` er dokumenteret («never compact»); sentinel `-1` kan ikke forekomme efter validator, men `0` kan |
| Vis `used` ubetinget | Altid korrekt, også ved cancel (vis «0 tokens» eller skjul — beslutning for UI) |
| Vis `size` kun hvis `size > 0` | ellers er grænsen ukendt/udefineret |
| Mærk `size` som «kompaktionsgrænse» | N8: det er `auto_compact_threshold`, ikke `max_context_length` |
| Håndter `size = 0` eksplicit som «kompaktjon slået fra» | Det er den dokumenterede betydning af 0 — vis det, ikke som fejl |

---

## Sentinel-kæden, opsummeret

```
UNSET (-1) ──merge──▶ _apply_auto_compact_threshold ──▶ altid konkret værdi (≥ 0)
                                                          │
                                    ┌─────────────────────┼─────────────────────┐
                                    ▼                     ▼                     ▼
                              model-felt sat        global/window         DEFAULT 200_000
                              (inkl. 0 = "off")     (80% af window)
                                    │
                                    ▼
                         RuntimeSnapshot.context_window
                                    │
                                    ▼
                         usage_update.size (wire)
```

**-1 kan ikke nå wire** via denne vej ●. **0 kan** ●. **Negativ andet end -1?** `ModelConfig.auto_compact_threshold: int = UNSET` — ingen `ge=0` validator på model-niveau (kun `max_context_length` har `ge=1`). Så en bruger *kan* skrive `-5` i config.toml... men kommentaren siger at håndskrevet -1 er «indistinguishable from unset». Hvad med -5? `cap = -5`, `cap != -1` → True, `window is None or -5 < window` → True → modellen beholder -5! **Så et negativt tal ≠ -1 kan faktisk lække igennem** ◐.

**Yderligere guard anbefaling:** Behandl `size <= 0` som «ukendt/ikke vist» — ikke kun `size == 0`. Det dækker både 0 («off») og eventuelle negative værdier.

---

## Opdateret status

| Punkt | Status |
|---|---|
| N8 kodelæst, ikke målt | ◐ — wire-måling udestår (script givet af relæ-AI) |
| Sentinel-læk (`-1` i `size`) | ● **kan ikke ske** — validator garanterer konkret værdi |
| `size = 0` | ● **kan ske** — dokumenteret «never compact»; guard nødvendig |
| `size < 0` (håndskrevet ≠ -1) | ◐ teoretisk muligt; guard `size <= 0` dækker det |
| L5 udvidet | ● filtrér nul-usage **og** guard `size <= 0`; `size = 0` vises som «kompaktion slået fra» |
| `used` ved cancel | ● 0 — vis eller skjul, beslutning for UI |

## Anbefaling til Skrynja (præciseret)

```cpp
// Pseudokode for usage-håndtering
void onUsageUpdate(int used, int size) {
    if (used == 0 && size == 0) return;      // nul-usage: behold tidligere
    showUsed(used);                            // altid
    if (size > 0) {
        showLimit(size, tr("kompaktionsgrænse"));  // N8: ikke «vindue»
        showPercent(used, size);               // kun når size > 0
    } else if (size == 0) {
        showLimitDisabled();                   // «kompaktion slået fra»
    }
    // size < 0: vis ingenting (skulle ikke forekomme; guard alligevel)
}
```
