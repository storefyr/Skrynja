# Verifikation af N5, N6, N7 — GrowthBook-laget, Core-konfiguration, usage_update

**Dato:** 26.10.2026 · **Kilde:** PyPI sdist `mistral_vibe-3.0.2.tar.gz` + `agent_client_protocol-0.11.0.whl`

---

## N5 — formulering accepteret og verificeret ●

Relæ-AI's formulering er korrekt: *tærsklen kommer fra GrowthBook-payloadet, ikke fra modelkataloglaget.*

**Verifikation i koden** (`vibe/core/config/layers/growthbook.py`):

1. **GrowthBook-laget kan injicere modeller uden om `_CATALOG_FIELDS`** ●
   - `GROWTHBOOK_CONFIG_MAPPINGS` (linje 88-100) mapper eksperimenter til config-felter:
     - `CLI_MODEL_ROUTING` → `routed_default_model`, `routed_model_config`
     - `CLI_EXTRA_MODELS` → `routed_extra_models` (mapper `_map_routed_extra_models`, linje 71-85: accepterer "a bare list of model definitions or an object with a `models` array")
   - `_routed_models()` (linje 170-190) validerer hver entry med `ModelConfig.model_validate(entry)` — **ingen `_CATALOG_FIELDS`-begrænsning** — og lægger dem i `data["models"]`, det kanoniske deep-merged felt.
   - `_build_config_snapshot()` (linje 137-164) returnerer dette som et config-layer — højere prioritet end kataloglaget.

2. **Konsekvens:** En GrowthBook-experiment-payload med en `models`-liste kan indeholde **vilkårlige `ModelConfig`-felter** — inklusive `auto_compact_threshold`. De bliver til `models` i config-laget og når `_apply_auto_compact_threshold` via den normale merge. `_CATALOG_FIELDS` begrænser **kun** `ModelCatalogLayer`, ikke `GrowthbookLayer`.

3. **`experiment_eval_cache.json`** (`vibe/core/experiments/_constants.py:14-16`): `EVAL_CACHE_FILE_NAME`, TTL **7 døgn** (`EVAL_CACHE_TTL_SECONDS`). Dette er cachen for eval-varianter — altså GrowthBook-varianter. Katalogformede modelposter der (display_name, prices, thinking_levels) passer med `CLI_EXTRA_MODELS`/`CLI_MODEL_ROUTING`-mekanismen: eksperimentlasten bærer selv modellisten.

**● N5 omformuleret (accepteret):** Tærsklen (800 000) kommer fra GrowthBook-payloadet injiceret som override-lag — hverken «katalogdata» i `_CATALOG_FIELDS`-forstand eller «flag» i snæver forstand. Det er et **eksperiment-routet model-override**.

---

## N6 — Core-konfiguration: er `RustDisabledCompactionPolicy` garanteret? ◐ delvist

**Hvad jeg fandt i koden:**

1. `_core_config()` (`_host.py:5123-5160`) er den **eneste** konstruktion af `RustHarnessConfig` i harness-pakken, og den sætter altid `RustDisabledCompactionPolicy()` + `compaction_fallback="standalone"`.

2. **MEN** — `UnifiedHarnessSessionBackendHost` har en `configure_runtime()`-metode (`_host.py:500-519`) der accepterer en **vilkårlig `RustHarnessConfig`** og gemmer den i `_configured_runtime_config`. Denne bruges:
   - Linje 654: `_refresh_subagent_configuration()` — læser `_configured_runtime_config`
   - Linje 2219: subagent-konfiguration
   - Linje 433: `self._runtime_config_template = _core_config("runtime-template")` — template, før `configure_runtime` kaldes

3. **Hvem kalder `configure_runtime`?** ○ **Ingen kaldere fundet i sdist'en** — hverken i harness-pakken eller i `vibe/acp/`. Det betyder: enten kaldes den fra **Rust-siden** (vibe-rs/app-server, ikke i sdist), eller også er den en ekstensionsflade der ikke bruges i standardkonfigurationen.

**Konklusion ◐:** I standardkonfigurationen (ingen `configure_runtime`-kald) gælder `_core_config` → `RustDisabledCompactionPolicy` → N6 holder. Men fordi `configure_runtime` eksisterer som offentlig metode og kan sætte en **anden** `RustHarnessConfig` (potentielt med `RustAutomaticCompactionPolicy(token_threshold=...)`), er N6 **ikke absolut garanteret** uden wire-måling.

**Wire-måling der afgør det:** Kør en session der fylder konteksten og observer:
- Kommer der `auto_compact_triggered` før en fejl? → Core-kompaktning er aktiv (N6 falsk)
- Fejler in-context-requestet direkte og går til standalone (som målt: `3051`)? → N6 bekræftet på wire

---

## N7 — `max_context_length` på wire: usage_update bærer faktisk vinduet ● (men ikke som configOption)

**To forskellige spørgsmål:**

### N7a: Kommer `max_context_length` med i ACP `configOptions`?
**○ Usikkert — sandsynligvis nej.** `configOptions` bygges i `vibe/acp/utils.py:160-186` (`build_model_config`, `make_thinking_response`) — kun `id`, `name`, `current_value`, `category`, `type`, `options` (value/name/description). Ingen `max_context_length`. `SessionConfigSelectOption` har ikke plads til det. Så **N7 som configOption: sandsynligvis ikke**.

### N7b: Får klienten vinduet på anden wire-vej? ● JA — via `usage_update`

`vibe/acp/agent.py:1794-1822` (`_send_usage_update`):

```python
await self.client.session_update(
    session_id=session.id,
    update=UsageUpdate(
        session_update="usage_update",
        used=stats.context_tokens,       # faktisk forbrug
        size=runtime.context_window,     # ← vinduet!
        cost=cost,
        **{"_meta": { ... token-detaljer ... }},
    ),
)
```

Og ACP 0.11.0-skemaet (`acp/schema.py:1606-1612`):
```python
class _UsageUpdate(BaseModel):
    used: int   # "Tokens currently in context."
    size: int   # "Total context window size in tokens."
    cost: Optional[Cost] = None
```

**Kritisk detalje — hvad er `runtime.context_window`?**

`vibe/app_server/_runtime.py:2202`:
```python
context_window=active_model.auto_compact_threshold,
```

**`context_window` i `usage_update.size` er IKKE `max_context_length` — det er `auto_compact_threshold`!**

Så på wire får Skrynja:
- `used` = faktisk token-forbrug (fra `stats.context_tokens`)
- `size` = **kompaktions-tærsklen** (auto_compact_threshold), ikke det reelle context window

For `glm-5-3` (hvis 800 000 gælder via GrowthBook): `size = 800000`, `used` = faktisk forbrug. For `mistral-medium-3.5`: `size = ?` — `auto_compact_threshold` for den model beregnes i `_apply_auto_compact_threshold`: window = 262144 (fra `vibe_schema.py:203`), 80 % = 209715, eller global default 200 000 hvis lavere... præcis: `min(global_value, window*0.8)` hvis global_set, ellers `int(262144 * 0.8) = 209715`.

**Konsekvens for Skrynja ◐:** `usage_update.size` er **ikke** «hvor meget plads er der tilbage» — det er «hvornår compacter agenten» (i V's arkitektur). I H's arkitektur (kompaktning disabled) er størrelsen måske meningsløs eller sat anderledes. Dette skal måles på wire: hvad står der faktisk i `size` for hver model på telefonen?

---

## Samlet status

| ID | Relæ-AI's formulering | Min verifikation | Status |
|---|---|---|---|
| N5 | Tærsklen kommer fra GrowthBook-payloadet | ● GrowthBook-laget kan injicere modeller med alle ModelConfig-felter uden om `_CATALOG_FIELDS` (`growthbook.py:170-190`, `data["models"]`). `experiment_eval_cache.json` er eval-cachen (TTL 7 døgn) | **Accepteret og bekræftet** |
| N6 | Disabled kompaktion gør tærskelspørgsmålet betydningsløst | ◐ `_core_config` er eneste konstruktion og sætter disabled — **men** `configure_runtime()` (`_host.py:500-519`) kan sætte en anden config, og ingen kaldere findes i sdist (sandsynligvis Rust-side). Standardkonfig: N6 holder; absolut garanti: nej | **Delvist — wire-måling nødvendig** |
| N7 | `max_context_length` i `configOptions`? | ◐ **Nej** i `configOptions` (utils.py bygger kun value/name/description). **Men** `usage_update.size` bærer et vindue — som faktisk er `auto_compact_threshold`, ikke `max_context_length` (`_runtime.py:2202`) | **Omformuleret — se N8** |

## N8 — nyt fund: `usage_update.size` = `auto_compact_threshold`, ikke context window

| Felt på wire | Kilde i koden | Betydning |
|---|---|---|
| `usage_update.used` | `stats.context_tokens` | Faktisk token-forbrug — **det eneste brugbare advarselssignal** ● |
| `usage_update.size` | `active_model.auto_compact_threshold` (`_runtime.py:2202`) | Kompaktions-tærskel, **ikke** reelt vindue ◐ |
| `usage_update.cost` | `stats.session_cost` | Kumuleret pris i USD |
| `usage_update._meta.*` | diverse stats | steps, promptTokens, completionTokens, cachedTokens, totalTokens, tokensPerSecond, lastTurn* |

**Konsekvens for Skrynja:** Hvis Skrynja vil vise «forbrug vs. grænse», kan den vise `used / size` — men `size` er tærsklen, ikke vinduet. Visningen bør formuleres som «forbrug vs. kompaktionsgrænse» (hvis N6 holder) eller «forbrug» uden grænse (hvis `size` er meningsløs i harness-mode). **Måles på wire:** hvad står i `size` for glm-5-3 vs mistral-medium-3.5 på telefonen?

---

## Wire-målinger der afgør resten

1. **N6 på wire:** Lang session til kontekstfyldning — kommer `auto_compact_triggered` (Core-kompaktning aktiv) eller direkte `3051` (standalone-fallback)?
2. **N7/N8 på wire:** I en normal session, log alle `usage_update` — hvad står i `used`, `size`, `cost` for hver model?
3. **configOptions på wire:** Kommer der overhovedet et felt med `max_context_length`? (Forventet: nej)
4. **Fire målinger fra relæ-AI (uændret):** `grep auto_compact_threshold config.toml`, `head experiment_eval_cache.json`, `ls ~/.vibe | grep catalog`, `pip show` af de tre pakker.

## Anbefaling til Skrynja-klienten (opdateret)

1. **Vis forbrug fra `usage_update.used`** — altid tilgængeligt, altid korrekt.
2. **Vis `size` som «kompaktionsgrænse»** — ikke som «context window». Formuleringen afhænger af N6's wire-måling.
3. **Advar ikke ved en hårdkodet tærskel** — brug `used / size` fra wire.
4. **Ingen `max_context_length` i configOptions** — forvent ikke feltet; hent vinduet fra `usage_update.size` hvis overhovedet.
