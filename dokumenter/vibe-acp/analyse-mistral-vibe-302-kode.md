# Kodeanalyse: mistral-vibe 3.0.2 (PyPI sdist) — svar til relæ-AI's §6

**Kilde:** `mistral_vibe-3.0.2.tar.gz` fra PyPI (officielt sdist, hentet 26.10.2026)
**Scope:** Besvarer specifikt spørgsmålet om `ReasoningEffort`-tabellen og `low`→`none`-mappingen i 3.0.2, og om der findes en nyere build.

---

## Svar på §6: Findes rettelsen i en nyere build end 3.0.2?

**● NEJ.** To uafhængige fakta:

1. **3.0.2 er nyeste udgivelse på PyPI** — versionslisten ender ved `3.0.2` (efterfulgt af 3.0.1, 3.0.0, 2.9.6 … 2.9.4). Ingen udgivelse nyere end 3.0.2 findes pr. 26.10.2026.
2. **I 3.0.2 selv er tabellen UÆNDRET:** `low` afbildes stadig til `"none"`.

### Bevis — tabellen i 3.0.2

`vibe/core/llm/backend/mistral.py:255-262`:
```python
ReasoningEffortValue = Literal["none", "high"]

_THINKING_TO_REASONING_EFFORT: dict[str, ReasoningEffortValue] = {
    "low": "none",
    "medium": "high",
    "high": "high",
    "max": "high",
}
```

Sammenlignet med den anden AI's fund i telefonens build (`adapters/mistral.py:80`, samme indhold) — **identisk tabel, blot flyttet**: i 3.0.2 ligger koden i `vibe/core/llm/backend/mistral.py`, ikke `vibe/adapters/mistral.py`. Telefonen kører samme kode, ny sti.

**Konklusion:** R30 er **ikke rettet** i 3.0.2. `low` → `"none"` på wire. Hvis API'et accepterer `low` (som den anden AI målte), er det fordi *serveren* accepterer begge, mens klienten aldrig sender `low` — den sender `none` for `low`.

---

## NYT FUND (ikke i den anden AI's liste): `_reasoning_effort()` har en betinget undtagelse

`vibe/core/llm/backend/mistral.py:265-280` — funktionen `_reasoning_effort(model)`:

```python
def _reasoning_effort(model: ModelConfig) -> ReasoningEffortValue | None:
    if (
        model.thinking == "off"
        and "off" in model.thinking_levels
        and model.thinking_levels_curated
    ):
        return "none"
    return _THINKING_TO_REASONING_EFFORT.get(model.thinking)
```

**Betydning:** For `thinking == "off"` sendes `"none"` **kun** når modelsættet er "curated" (se nedenfor). For **ikke-curated** sæt (default 5 niveauer) sendes feltet **helt udeladt** — `None` — og serverens default gælder. Docstring forklarer hvorfor: *"some hosted models (the zai-glm family) reject the explicit value"*.

Dette er **ny information for Skrynja**: forskellen på `off` og `low` er ikke kun tabellen — `off` kan ende som "felt udeladt" eller `"none"` afhængigt af om modelsættet er curated.

---

## `thinking_levels_curated` — hvad betyder det?

`vibe/core/config/models.py:553-560`:
```python
@property
def thinking_levels_curated(self) -> bool:
    return set(self.thinking_levels) != set(THINKING_LEVELS)
```

`THINKING_LEVELS` (default) = `("off", "low", "medium", "high", "max")` (`vibe/config_values.py:12`).

- **Curated** = modelspecifik liste afviger fra default 5 — f.eks. `mistral-medium-3.5` har `["off", "high"]` (`vibe/core/config/vibe_schema.py:202`) → curated → `off` sendes som `"none"`.
- **Ikke-curated** = default 5 → `off` sendes **ikke** (felt udeladt).

---

## `ReasoningEffortValue` — er typen udvidet ud over `Literal["none", "high"]`?

**● NEJ.** `vibe/core/llm/backend/mistral.py:255`:
```python
ReasoningEffortValue = Literal["none", "high"]
```

Den eneste nye type er `ThinkingLevel` i `vibe/config_values.py:11`:
```python
type ThinkingLevel = Literal["off", "low", "medium", "high", "max"]
```

Bemærk: `config_values.py` bruger `type X = ...` — det er **Python 3.12 `type`-statement**, ikke en runtime-type. Den er konfigurationsniveauets type, ikke wire-typen.

---

## Verifikation af den anden AI's øvrige kildefund i 3.0.2

| ID | Påstand | I 3.0.2? | Sti/linje |
|---|---|---|---|
| R30 | `low→none` tabel | ● bekræftet | `vibe/core/llm/backend/mistral.py:257-262` |
| R31 | `disable_reasoning=standalone` tvinger `none` på kompaktionsvejen | ○ ikke verificeret her (søges ikke i denne runde) | — |
| R32 | kompaktion arver aktiv model; tærskel 800 000 for glm-5-3 | ○ delvist — `max_context_length` findes i `ModelConfig` (`models.py:527`), men glm-5-3's katalogpost findes **ikke** i 3.0.2-sdist (se nedenfor) | — |
| R36 | zai-glm-5-3-flash `max_context_length: 1048576` | ○ ikke verificeret — modellen findes ikke i koden | — |
| R40 | katalogcache-TTL 30 døgn | ○ ikke verificeret i denne runde | — |
| R42 | tom katalog er «authoritative» | ○ ikke verificeret i denne runde | — |

**Vigtigt:** `glm-5-3` og `zai-glm-5-3-flash` forekommer **kun én gang** i hele 3.0.2-koden — i docstring i `mistral.py` (`"the zai-glm family"`). De er **ikke** i den statiske modelkatalog. De kommer fra **serverens dynamiske katalog** (som vejkortet også antog: `configOptions.model.options` er kilden). Dette bekræfter:
- Skrynja må **ikke** hardcode modeller — menuen skal bygges fra `configOptions`.
- `thinking_levels` for glm-modeller kommer også dynamisk (fra server-katalog), ikke fra lokal kode.

---

## ACP-relevant: hvordan `thinking` og `model` eksponeres over ACP i 3.0.2

`vibe/acp/utils.py:160-186`:
- `build_model_config()` → `configOptions[]` entry `id="model"`, `current_value=config.active_model.alias`, options fra `config.models` (alias, display_name, name)
- `make_thinking_response()` → `id="thinking"`, `current_value=config.active_model.thinking`, options = `config.active_model.thinking_levels` (display: `level.capitalize()`)

`vibe/acp/agent.py:1055-1095` (`session/set_config_option` handler):
- `"mode"` → `agents.switch(mode)` — kun hvis `_is_primary_mode(session, mode)`
- `"model"` → valideres mod `config.current.models` (alias-match) → `config.update({"active_model": model}, reload_runtime=True)`
- `"thinking"` → accepterer **kun** `{"off","low","medium","high","max"}` (hårdkodet sæt i koden!) → `set_thinking(level)`
- `"max_turns"`, `"max_tokens"` → sessionsettings
- alt andet → `InvalidRequestError("Unsupported config option ...")`

**Bemærkelsesværdigt for Skrynja:**
1. `thinking`-valideringen i ACP-laget accepterer alle 5 niveauer — **men** `_reasoning_effort()` i backend mapper `low→none`. Så Skrynja kan sætte `low` uden fejl, men agenten sender `none` på wire. Det er agent-internt (R30), Skrynja kan ikke se det.
2. `model` valideres mod den aktuelle modelliste — et ugyldigt alias giver `InvalidRequestError`.
3. `mode` valideres mod `_is_primary_mode` — `smart-approve`/`auto-approve` afhænger af denne funktion (ikke læst i denne runde — relevant for det åbne spørgsmål).

---

## Konsekvens for Skrynja-klienten (C++/Qt)

1. **Modelmenu:** bygges fra `configOptions[id="model"].options` — aldrig hardcodet. `currentValue` = aktivt alias.
2. **Thinking-menu:** bygges fra `configOptions[id="thinking"].options` — skifter ved modelskift (agenten gør det via `make_thinking_response` på hver config-read).
3. **`low` på zai-glm:** Skrynja vil tilbyde `low` (det står i options), agenten accepterer det, men sender `none` til API'et. **Klienten kan ikke detektere eller forhindre dette** — det er agent-internt (R30). Hvis brugeren vælger `low` og forventer lav reasoning, får de `none`.
4. **`off` på mistral-medium-3.5:** curated sæt `["off","high"]` → `off` sendes som `"none"` på wire.
5. **`off` på ikke-curated model:** feltet udelades helt — server default.

---

## Åbne spørgsmål tilbage til relæ-AI

1. `_is_primary_mode()` — hvilke modes accepterer agenten som `mode` i `set_config_option`? (bestemmer om `smart-approve`/`auto-approve` er gyldige configId-værdier)
2. R31/R32/R40/R42 — kan verificeres i samme sdist; sig til hvis I vil have dem tjekket.
3. Er telefonens `vibe-acp` faktisk 3.0.2-koden, eller en Mistral-intern build? Stien `adapters/mistral.py` (relæ-AI) vs `core/llm/backend/mistral.py` (PyPI 3.0.2) antyder **forskellige builds** — samme tabel, forskellig placering. Det er værd at verificere, før vi konkluderer at telefonen kører præcis PyPI 3.0.2.
