# R32 — afgjort: hvor kommer 800 000 fra, og gælder det på telefonen?

**Dato:** 26.10.2026 · **Kilde:** PyPI sdist `mistral_vibe-3.0.2.tar.gz`, begge kodetræer
**Svarer på:** Relæ-AI's præcisering om at 800 000 står i katalogdata, ikke i kode eller config.toml

---

## Del 1: Præcedens i `_apply_auto_compact_threshold` (V, `vibe_schema.py:1248-1276`)

Rækken er **klar** — modelfeltet læses FØR ratio og default:

```python
for alias, model in self.models.items():
    window = model.max_context_length
    cap = model.auto_compact_threshold          # ← læses FØRST
    if cap != UNSET_AUTO_COMPACT_THRESHOLD and (window is None or cap < window):
        models[alias] = model                   # ← katalog-værdien vinder, uændret
        continue
    if global_set and (window is None or global_value < window):
        threshold = global_value                # ← ellers global config/default (200 000)
    elif window:
        threshold = int(window * AUTO_COMPACT_WINDOW_RATIO)  # ← ellers 80 % af vinduet
    else:
        threshold = global_value
```

**● Konklusion:** Hvis `model.auto_compact_threshold = 800000` er sat (fra katalog eller config), og `window is None` (som for `glm-5-3`), så er betingelsen `cap != UNSET and (window is None or ...)` → `True` → **modellen beholder 800 000 uændret**. Relæ-AI's oprindelige slutning var korrekt: `window is None` rammer første led, og katalogværdien bevares.

**Men** — og det er afgørende — **kommer katalogfeltet overhovedet med?**

---

## Del 2: Kataloglaget kasserer `auto_compact_threshold` — ● NYT, KRITISK FUND

`vibe/core/config/layers/model_catalog.py:28-38`:

```python
_CATALOG_FIELDS = frozenset({
    "name",
    "provider",
    "alias",
    "display_name",
    "supports_images",
    "max_context_length",
    "temperature",
    "thinking_levels",
    "thinking",
})
```

**`auto_compact_threshold` er IKKE i `_CATALOG_FIELDS`.**

Og i `_catalog_model_payload` (samme fil, linje 96-133) bygges payloadet eksplicit:

```python
values: dict[str, Any] = {
    "name": entry.id,
    "provider": _CATALOG_PROVIDER,
    "alias": entry.id,
    "display_name": entry.label,
    "supports_images": entry.capabilities.vision,
    "max_context_length": entry.max_context_length,
}
if entry.parameters.temperature is not None:
    values["temperature"] = entry.parameters.temperature
if entry.parameters.reasoning is not None:
    values["thinking_levels"] = _thinking_levels(entry.id, entry.parameters.reasoning)
...
return model.model_dump(include=set(_CATALOG_FIELDS))
```

Tre barrierer:
1. `ModelCatalogEntry` (`model_catalog/models.py:32-49`) har **ikke et `auto_compact_threshold`-felt** — serverens felt parses ikke engang.
2. `ModelCatalogParameters` (samme fil, linje 25-29) har kun `temperature` og `reasoning`.
3. `_catalog_model_payload` sætter kun de felter, der eksplicit skrives ind — og `auto_compact_threshold` er hverken i `values` eller i `_CATALOG_FIELDS`.

**● Konklusion:** Selv hvis serverens katalog svarer med `"auto_compact_threshold": 800000` i en modelpost, **kasserer klienten feltet**. Kataloglaget kan kun bidrage: name, provider, alias, display_name, supports_images, max_context_length, temperature, thinking_levels, thinking.

---

## Del 3: Så hvor kommer telefonens 800 000 fra?

Hvis feltet ikke kan komme fra kataloglaget, er mulighederne:

1. **Telefonens `~/.vibe/config.toml`** — en `[[models]]`-sektion med `auto_compact_threshold = 800000`. **Ikke målt endnu** — kommandoen er givet (`grep -n auto_compact_threshold $HOME/.vibe/config.toml`).
2. **Telefonens `experiment_eval_cache.json`** — relæ-AI målte feltet *der*. Men hvad er `experiment_eval_cache.json`? Det er **GrowthBook eval-cachen** (`vibe/core/experiments/_constants.py:14`: `EVAL_CACHE_FILE_NAME = "experiment_eval_cache.json"`, TTL 7 døgn), IKKE modelkatalog-cachen. Modelkatalog-cachen ligger et andet sted (`MODEL_CATALOG_CACHE_FILE` i `vibe/core/paths.py`).

   **Hypotese ◐:** `experiment_eval_cache.json` er GrowthBook feature-flags/experimenter — og 800 000 kan være en **experiment-variant** (f.eks. `auto_compact_threshold` som GrowthBook-flag), ikke katalogdata. Det ville forklare hvorfor feltet ligger i eval-cachen og hvorfor det ikke er i koden.

3. **En anden config-layer** — GrowthBook-laget kan sætte `auto_compact_threshold` som override (laget sidder over kataloglaget i lag-rækkefølgen).

---

## Del 4: Gælder 800 000 i harness-mode (telefonen)?

To spor:

**Spor A — V's config-system (`_apply_auto_compact_threshold`):** Gælder hvis telefonen kører V's config-orchestrator. Så gælder præcedensen: model-felt (fra config.toml eller GrowthBook) → global → 80 % af vindue → default 200 000.

**Spor B — H's runtime (`RustDisabledCompactionPolicy`):** I harness-mode sættes:
```python
# vibe/_host.py:5128-5132
context=RustContextSettings(
    compaction=RustDisabledCompactionPolicy(),   # mode="disabled"
    compaction_fallback="standalone",
)
```

**● NYT, KRITISK FUND:** I harness-mode er **Core's automatiske kompaktion DEAKTIVERET** (`RustDisabledCompactionPolicy`, `protocol.py:341-342`). Der er ingen `RustAutomaticCompactionPolicy` med `token_threshold` noget sted i H's Python-kode — `_core_config` er den eneste konstruktion, og den bruger altid `disabled`.

**Konsekvens ◐:** Hvis telefonen kører i denne harness-konfiguration:
- Core compacter **ikke automatisk** ved en token-tærskel.
- Når konteksten er fyldt, fejler in-context-requestet, og Core falder tilbage til **standalone kompaktion** (`compaction_fallback="standalone"`).
- Standalone-kompaktning kører med `disable_reasoning=True` → `reasoning_effort="none"` (R31/N2) → på zai-glm fejler det med `3051`.

Så på telefonen er **ikke tærsklen 800 000, der er problemet — det er at automatisk kompaktion er slået fra i Core, og fallback-vejen fejler på zai-glm**. R32's formulering («kompaktionsrisiko ved 800 000 tokens») beskriver måske V's adfærd, ikke H's.

**Åbent ○:** Er telefonens `vibe-acp` faktisk denne `_core_config` (disabled + standalone fallback)? Det kan måles: kør en lang session på telefonen og observer om der kommer `auto_compact_triggered` events, eller om den går direkte til standalone efter context-overflow.

---

## Del 5: Hvad betyder det for Skrynja?

| Spørgsmål | Svar |
|---|---|
| Viser Skrynja en kompaktions-advarsel ved 800 000 tokens? | **Nej — ikke baseret på denne tærskel.** Tærsklen er ikke klient-synlig via ACP (ingen configOption for den). Hvis Skrynja vil advare, skal den baseres på `usage_update` (faktisk forbrug) vs. modellens `max_context_length` (som **er** i katalog-payload og dermed i config). |
| Kan Skrynja afværge `3051` på kompaktionsvejen? | **Nej** — det er agent-internt (R31/N2). Skrynja kan kun undgå zai-glm-familien eller advare. |
| Er `max_context_length` tilgængelig for Skrynja? | **Ja, potentielt** — feltet er i `_CATALOG_FIELDS`, så det når med i config. Men det udstilles ikke nødvendigvis som ACP `configOption`. ○ Skal måles: kommer `max_context_length` med i `configOptions` på wire? |
| Hvad med `thinking_levels`? | **Ja** — i `_CATALOG_FIELDS`, og bekræftet på wire af relæ-AI's måling (`["low","high","max"]` for zai-glm). |

---

## Samlet status for R32

| Påstand | Status | Kommentar |
|---|---|---|
| 800 000 står i katalogdata | ◐ delvist | Målt i `experiment_eval_cache.json`, men det er **GrowthBook eval-cachen**, ikke modelkatalog-cachen. Kan være et experiment-flag, ikke katalogdata. |
| Kataloglaget overfører `auto_compact_threshold` | ● **NEJ** | Feltet er ikke i `ModelCatalogEntry`, ikke i `_CATALOG_FIELDS`. Kasseret. |
| `_apply_auto_compact_threshold` læser modelfeltet først | ● ja | `window is None` + `cap=800000` → bevares. Men kun hvis feltet når modellen — fra config.toml eller GrowthBook, ikke fra katalog. |
| Kompaktion er nåelig på standardmodellen | ○ uafgjort | I V: ja ved tærsklen. I H: Core-kompaktning er **disabled**, fallback er standalone → `3051` på zai-glm. |
| `standalone`-vejen sender `none` → `3051` | ● bekræftet | H: `_completion.py:221,305` + `adapters/mistral.py:135-137` |

## Kommandoer der afgør resten (på telefonen)

```sh
# 1. Er 800000 i config.toml?
grep -n auto_compact_threshold $HOME/.vibe/config.toml

# 2. Hvad er experiment_eval_cache.json faktisk? (GrowthBook?)
head -c 600 $HOME/.vibe/experiment_eval_cache.json

# 3. Hvor ligger modelkatalog-cachen?
ls -la $HOME/.vibe/ | grep -i catalog

# 4. Viser ACP configOptions max_context_length? (køres i en session)
#    — se repro-script, søg efter "max_context_length" i session/new-svaret

# 5. To pakke-versioner
$HOME/.local/share/skrynja/agent/bin/pip show mistral-vibe mistralai-vibe-local-harness agent-client-protocol 2>&1 | grep -E '^(Name|Version|Location)'
```

## Nye fund tilføjet til journalen

| ID | Indhold |
|---|---|
| **N4** | `_CATALOG_FIELDS` (`model_catalog.py:28-38`) kasserer `auto_compact_threshold` — katalog kan ikke sætte tærsklen |
| **N5** | `experiment_eval_cache.json` er GrowthBook eval-cachen (TTL 7 døgn, `_constants.py:14,16`), IKKE modelkatalog-cachen — 800 000 derfra er sandsynligvis et experiment-flag |
| **N6** | I harness-mode er Core-kompaktning **disabled** (`_host.py:5129`), fallback er standalone → R31/N2 rammer direkte |
| **N7** | `max_context_length` ER i `_CATALOG_FIELDS` — potentielt klient-synligt, skal måles på wire |
