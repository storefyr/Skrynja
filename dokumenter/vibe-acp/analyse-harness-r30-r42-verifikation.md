# Verifikation af R30–R42 i `mistralai-vibe-local-harness` + `mistral-vibe` 3.0.2

**Dato:** 26.10.2026 · **Kilde:** officiel PyPI sdist `mistral_vibe-3.0.2.tar.gz`
**To kodetræer undersøgt:**
- **H** = `harness/runtimes/python/python/mistralai_vibe_local_harness/` (telefonens pakke — Unified Runtime)
- **V** = `vibe/` (mistral-vibe kernepakken)

Relæ-AI's rettelse er ● bekræftet: telefonen kører **to pakker**, og `adapters/mistral.py` findes **kun** i H, ikke i V. Samme fejl, to uafhængige forekomster.

---

## R30 — `low → none` i ReasoningEffort-tabellen

| Forekomst | Sti | Linje | Kode |
|---|---|---|---|
| **H (telefonen)** | `…/mistralai_vibe_local_harness/vibe/adapters/mistral.py` | 78-84 | `type ReasoningEffort = Literal["none", "high"]` (PEP 695-alias) + tabel med `"low": "none"` |
| **V (kerne)** | `vibe/core/llm/backend/mistral.py` | 255-262 | `ReasoningEffortValue = Literal["none", "high"]` + samme tabel |

**● Bekræftet i begge pakker.** To uafhængige forekomster, identisk indhold, forskellig syntaks (PEP 695 `type` vs. `TypeAlias`).

**NY detalje i H** (`adapters/mistral.py:135-137`):
```python
if reasoning_effort := _THINKING_TO_REASONING_EFFORT.get(route.thinking):
    kwargs["reasoning_effort"] = (
        "none" if disable_reasoning else reasoning_effort
    )
```
- H's adapter har **ikke** den betingede `off`-undtagelse som V's `_reasoning_effort()` (V: `mistral.py:265-280`). I H sendes `off` via `_THINKING_TO_REASONING_EFFORT.get("off")` → `None` → **feltet udelades** (walrus + `if`). Altså: i H er `off` altid "udeladt", i V er `off` `"none"` på curated sæt og udeladt på ikke-curated.
- **`disable_reasoning=True` tvinger `"none"`** — dette er præcis R31's mekanisme, og den er eksplicit i H's kode.

---

## R31 — `disable_reasoning=standalone` tvinger `none` på kompaktionsvejen

**● Bekræftet i H:** `…/vibe/_completion.py`

- Linje 221: `standalone = compaction and action.request == "standalone"`
- Linje 305: `disable_reasoning=standalone`
- Linje 303: `config.compaction_model if standalone else route` — kompaktion bruger **compaction_model** (se R32)
- Kommentar linje 218-220: *"A standalone compaction request carries the history as a transcript and runs without tools and without reasoning"*

Kæden: standalone-kompaktning → `disable_reasoning=True` → i adapteren: `kwargs["reasoning_effort"] = "none"` uanset route.thinking. Hvis modellen afviser `reasoning_effort="none"` (som zai-glm gør ifølge måling), fejler kompaktningen med `3051`.

---

## R32 — kompaktion arver aktiv model; tærskel 800 000

**Delvist ● bekræftet — med en vigtig korrektion:**

**H's del (telefonen):**
- `…/vibe/_runtime_config.py:452-456`:
  ```python
  compaction_model = self.compaction_model
  if compaction_model == _DEFAULT_LOCAL_MODEL_ROUTE:
      compaction_model = active_model
  ```
  → Hvis `compaction_model` ikke er sat, **arver kompaktion den aktive model** ●
- `…/vibe/_completion.py:303` bekræfter: standalone-kompaktning kører på `config.compaction_model`.

**V's del (kerne):**
- `vibe/core/config/vibe_schema.py:1016-1020` (`get_compaction_model`):
  ```python
  def get_compaction_model(self) -> ModelConfig:
      if self.compaction_model is not None and self._shares_active_provider(...):
          return self.compaction_model
      return self.get_active_model()
  ```
  → Samme princip: uden eksplicit compaction_model → aktiv model ●

**Tærsklen 800 000 — ○ KORRIGERET:**
- `800_000` findes **ingen steder** i koden (hverken H eller V).
- `DEFAULT_AUTO_COMPACT_THRESHOLD = 200_000` (`vibe/core/config/_defaults.py:25`)
- `AUTO_COMPACT_WINDOW_RATIO = 0.8` (`_defaults.py:39`)
- Beregningen i `_apply_auto_compact_threshold` (`vibe_schema.py:1248-1276`):
  ```python
  if cap != UNSET and (window is None or cap < window):  # model-specifik cap vinder
      ...
  if global_set and (window is None or global_value < window):
      threshold = global_value        # global cap (200 000 default)
  elif window:
      threshold = int(window * 0.8)   # 80 % af vinduet
  else:
      threshold = global_value        # window ukendt → global default
  ```

**Hvad betyder det for glm-5-3 (window = None)?**
- `window is None` → hverken `elif window` (falsk) nor betingelsen `cap < window` kan evalueres.
- Hvis global cap er sat (default 200 000): `global_set=True`, `window is None` → **threshold = 200 000**.
- Hvis global cap er UNSET: `global_value = DEFAULT = 200 000`, samme resultat.

**Konklusion:** For en model uden `max_context_length` bliver tærsklen **200 000**, ikke 800 000 — medmindre brugerens `config.toml` sætter `auto_compact_threshold = 800000` eksplicit. Den anden AI's påstand om 800 000 er ○ **ikke understøttet af koden**; den kommer sandsynligvis fra en målt `config.toml` på telefonen eller en forveksling med `zai-glm-5-3-flash`'s `max_context_length: 1048576` (hvis 80 %: `1048576 * 0.8 = 838860 ≈ 800 000`? nej — det passer ikke præcist; 838 860 ≠ 800 000).

**Åbent:** Hvor kommer 800 000 fra? Muligvis telefonens `~/.vibe/config.toml` har `auto_compact_threshold = 800000`. Bed brugeren køre: `grep auto_compact_threshold ~/.vibe/config.toml`.

---

## R36 — `zai-glm-5-3-flash` har `max_context_length: 1048576`; `glm-5-3` har ikke

**○ Ikke verificerbart i koden** — bekræftet at glm-modellerne **ikke** ligger i statisk katalog (kun i docstring). Værdierne kommer fra serverens dynamiske katalog (`ModelCatalogResponse`). Den anden AI's måling af eksperimentlasten (`experiment_eval_cache.json`) er den gyldige kilde her ● (som de selv skrev).

**Kodestøtte for mekanismen:** `ModelConfig.max_context_length: int | None = Field(default=None, ge=1)` (`vibe/core/config/models.py:527`) — `None` = "window ukendt". Kommentar: *"None when the window is unknown; 0 or a negative would silently disable compaction"*.

---

## R40 — katalogcache-TTL 30 døgn

**● Bekræftet:** `vibe/core/model_catalog/_constants.py:18`:
```python
MODEL_CATALOG_CACHE_TTL_SECONDS: Final = 30 * 24 * 60 * 60
```
Kommentar linje 17: *"experiment-eval TTL because a model list moves far less than a variant."*

**Bemærk:** Dette er i **V** (kerne), ikke H. Men H har ingen egen model_catalog-mappe (verificeret: `find` fandt ingen `model_catalog` i H). Katalog-cachen håndteres altså af **V**, selv når H kører som runtime. Telefonens `experiment_eval_cache.json` ligger under `~/.vibe/` — konsistent med V's cache-sti.

---

## R42 — tom katalog er «authoritative»

**● Bekræftet:** `vibe/core/model_catalog/cache.py:39-52`:
```python
def store_cached_model_catalog(config, response) -> None:
    """Overwrite the cached catalog for this user's credential.

    Called on every successful fetch, including an empty catalog: a successful
    response is authoritative.
    """
```
Docstring siger det eksplicit: *"including an empty catalog"*. En succesfuld fetch med tom liste **overskriver** cachen med tom liste.

**Konsekvens for Skrynja (R37/R42 arbejdsliste):** Hvis serveren svarer tomt (f.eks. ved netværksfejl der returnerer 200 med tom liste, eller ved rate-limiting), bliver `configOptions.model.options` tom — og agenten anser det som autoritativt. Skrynja skal vise **fejl**, ikke tom menu (som vejkortets Fase 5 siger).

---

## R34 — to `Skrynja.conf`-stier (åbent fra før)

Ikke relateret til agentkoden — det er Skrynjas egen Qt-konfiguration. Udestående.

---

## `_is_primary_mode()` — svar 1

**○ Ikke fundet i sdist'en.** Funktionen findes ikke i hverken H eller V's Python-kode — den ligger sandsynligvis i **Rust-delen** (`vibe/cli-rust/` eller `vibe-rs`) eller i app-server-protokollen, som sdist'en ikke indeholder fuldt ud. 

**Men som relæ-AI korrekt påpeger:** spørgsmålet er protokolmålbart. `set_config_option mode=auto-approve` returnerede `result` uden fejl → værdien **accepteres**. Om den **undertrykker dialogen** kræver repro-scriptet.

**Fra koden vi kan se:** H's `_runtime_config.py:31` kommentar nævner eksplicit *"the smart-approve risk"*:
```python
# call actions (those stay ``agent`` or ``compaction``): the smart-approve risk
```
(dette er en kommentar om CompletionPurpose-typen). Det antyder at `smart-approve` er en **kendt risikofaktor** i designet — men ikke om det undertrykker permission-dialogen.

---

## Samlet status for R-listen

| ID | Status i denne verifikation | Kommentar |
|---|---|---|
| R30 | ● bekræftet ×2 | H: `adapters/mistral.py:80-84`; V: `core/llm/backend/mistral.py:257-262` |
| R31 | ● bekræftet | H: `_completion.py:221,305` + `adapters/mistral.py:135-137` |
| R32 | ◐ delvist | "Arver aktiv model" ●; "tærskel 800 000" ○ — koden siger 200 000 default |
| R36 | ○ ikke i kode | kommer fra dynamisk katalog; måling er gyldig kilde |
| R40 | ● bekræftet | V: `model_catalog/_constants.py:18` = 30 døgn |
| R42 | ● bekræftet | V: `model_catalog/cache.py:39-52` — docstring eksplicit |

## Nye fund tilføjet

| ID | Indhold |
|---|---|
| **N1** | H's adapter har **ikke** V's betingede `off`-undtagelse — i H udelades `off` altid (walrus), i V sendes `off→"none"` på curated sæt |
| **N2** | H's adapter tvinger `reasoning_effort="none"` når `disable_reasoning=True` — eksplicit i koden (`adapters/mistral.py:135-137`) |
| **N3** | 800 000 findes ingen steder i koden; default er 200 000 (`_defaults.py:25`), 80 % af vinduet ellers (`_defaults.py:39`) |
| **N4** | H har ingen egen `model_catalog` — katalog-cache håndteres af V selv i harness-mode |
| **N5** | `_is_primary_mode()` findes ikke i Python-koden — ligger sandsynligvis i Rust/app-server |

## Anbefaling til Skrynja-klienten

1. **Modelmenu fra `configOptions`** — aldrig hardcoded (dobbelt bekræftet).
2. **`thinking`-menu fra `configOptions`** — skifter ved modelskift; klienten skal **ikke** antage at alle 5 niveauer altid er tilgængelige.
3. **`low` er en løgn på zai-glm** — agenten accepterer det, men sender `none`. Klienten kan ikke detektere det. Overvej at filtrere `low` fra menuen for modeller der afviser `low` (kræver måling pr. model — A7).
4. **Tom `model.options` = fejl, ikke tom menu** (R42).
5. **Kompaktionstærskel:** Hvis Skrynja viser token-forbrug, skal den bruge agenten's faktiske tærskel — ikke en antaget 800 000.
