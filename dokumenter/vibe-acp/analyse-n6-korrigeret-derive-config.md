# N6 — KORRIGERET: kompaktion er IKKE disabled på telefonen ●

**Dato:** 26.10.2026 · **Trigger:** Relæ-besked — wire-måling viser `mode: automatic, token_threshold: 800000`
**Konsekvens:** Min tidligere kodelæsning (N6 = disabled) var **falsk for telefonens runtime**. Relæ-AI's måling vinder over min statiske læsning.

---

## Hvad jeg læste forkert

Jeg fandt `_core_config()` i `mistralai_vibe_local_harness/vibe/_host.py:5123-5160`:

```python
context=RustContextSettings(
    compaction=RustDisabledCompactionPolicy(),   # ← jeg læste dette som "altid disabled"
    compaction_fallback="standalone",
)
```

Og konkluderede: «I harness-mode er Core-kompaktning deaktiveret.» **Det var en slutning fra en enkelt funktion, ikke en måling.** Jeg overså at `_core_config` er **fallback/template** — ikke den konfiguration, telefonen faktisk kører.

---

## Den rigtige vej: `derive_config` i app-serverlaget ●

Telefonen kører `vibe-acp` → app-server → `UnifiedHarnessSessionBackendAdapter` → `context.derive(settings)` → `derive_config()` i `vibe/app_server/_runtime.py:1352+`.

Og der — linje ~1385-1389 (i `derive_config`):

```python
compaction_policy = (
    RustAutomaticCompactionPolicy(
        token_threshold=active_model.auto_compact_threshold
    )
    if active_model.auto_compact_threshold > 0
    else RustDisabledCompactionPolicy()
)
```

**Dette er den afgørende forskel:**

| | `_core_config` (harness-pakken) | `derive_config` (app-server) |
|---|---|---|
| **Hvem kalder den** | Fallback/template i `UnifiedHarnessSessionBackendHost.__init__` | App-serveren ved hver session-derivation |
| **Compaction** | `RustDisabledCompactionPolicy()` — altid | `RustAutomaticCompactionPolicy(token_threshold=active_model.auto_compact_threshold)` hvis > 0 |
| **Hvad gælder på telefonen** | Nej — overskrives | **Ja — dette er telefonens konfiguration** |
| **Fallback** | `compaction_fallback="standalone"` | Sættes via `core_context_settings(compaction=compaction_policy, ...)` |

`configure_runtime(derivation.core_config, ...)` (`_unified_harness_backend_adapter.py:2611` og 5 andre steder) **overskriver** `_runtime_config_template = _core_config("runtime-template")` — så `_core_config` er kun template før første derivation.

---

## Wire-målingen bekræfter ●

Relæ-AI målte på telefonen: `mode: automatic, token_threshold: 800000`.

Det stemmer præcis overens med koden:
- `active_model.auto_compact_threshold = 800000` (fra GrowthBook-override, jf. N5)
- `800000 > 0` → `RustAutomaticCompactionPolicy(token_threshold=800000)`
- Core vil compacte automatisk ved 800 000 tokens

**Tre kilder, samme tal (R32 bekræftet tre steder):**
1. Journalens `token_threshold` (wire-måling)
2. Wire'ens `usage_update.size` = 800000 (N8)
3. Eksperimentlastens `auto_compact_threshold` = 800000 (GrowthBook-payload)

---

## N6 — korrigeret status

| Påstand | Tidligere (min kodelæsning) | Nu (wire + korrekt kodesti) |
|---|---|---|
| Core-kompaktning | Disabled (`_core_config`) | **Automatic ved 800 000** (`derive_config`) |
| N6 | ◐ «standardkonfig: disabled» | ● **FALSK** — telefonen kører `automatic` |
| Standalone-fallback | Primær vej ved overflow | **Fallback** — kun hvis in-context kompaktion fejler |
| R31's `3051`-risiko | Rammer ved enhver kontekstfyldning | Rammer **kun hvis** in-context kompaktion fejler og standalone kører med `disable_reasoning=True` → `"none"` |

---

## Konsekvens for Skrynja — opdateret ●

1. **`usage_update.size` = 800 000 er en REEL kompaktionsgrænse** — Core vil compacte ved denne tærskel. Det er ikke meningsløst; det er præcis hvad brugeren skal advares mod.
2. **Vis `used / size` som «forbrug vs. kompaktionsgrænse»** — når `used` nærmer sig `size`, vil agenten compacte. Det er en legitim advarsel.
3. **`3051`-risikoen er betinget:** Den opstår kun hvis:
   - In-context kompaktion fejler (f.eks. model afviser, eller summary invalid), OG
   - Core falder tilbage til standalone, OG
   - Standalone kører med `disable_reasoning=True` → `reasoning_effort="none"`, OG
   - Modellen er zai-glm (afviser `"none"`)
   Så: risikoen er **reel men betinget** — ikke ved enhver kompaktion, kun ved fallback på zai-glm.
4. **A7 (undertryk `low` for zai-glm)** bliver vigtigere: Hvis brugeren sætter `thinking=low` på zai-glm, sendes `"none"` — og hvis det sammenfalder med en standalone-kompaktning, fejler det med `3051`.
5. **R32 lukket:** Kompaktionskæden er agent-intern, men nu fuldt forstået: tærskel 800 000 (GrowthBook) → Core compacter automatisk → ved fejl, standalone med `disable_reasoning` → `3051` på zai-glm.

---

## Lærd lektion (til vejkortet)

> **En fallback/template-funktion er ikke den aktive konfiguration.** `_core_config` i harness-pakken er en template; den aktive `RustHarnessConfig` kommer fra `derive_config` i app-serverlaget via `configure_runtime`. Statisk kodelæsning uden at følge override-kæden fører til forkerte konklusioner.

Dette bør ind i vejkortets §9 (risikoregister) som en ny risiko:
**R11:** «Template/fallback-konfiguration læses som aktiv» — afbødning: følg altid `configure_runtime`-kæden, eller mål på wire.

---

## Samlet status (opdateret)

| Punkt | Status |
|---|---|
| N5 | ● lukket — GrowthBook-override |
| **N6** | ● **KORRIGERET — FALSK**: kompaktion er `automatic` ved 800 000, ikke disabled |
| N7 | ● lukket — `usage_update`, ikke configOptions |
| N8 | ● lukket — `size` = kompaktionsgrænse (nu: en REEL grænse) |
| L5 | ● lukket — filtrér `used == 0`; guard `size <= 0` |
| R32 | ● **lukket** — 800 000 bekræftet tre steder; kæden forstået |
| R30/R31 | ● agent-intern; A7 afhjælper klientside |
| A5/H2 | ● klar til implementation |
| Versionsspørgsmål | ● lukket — 3.0.2 = PyPI 3.0.2, ACP SDK 0.11.0 |
| R34, R41 | ○ åbne, ikke-blokerende |
