# N8 — hele kæden for `usage_update.size` ● kodelæst i sdist 3.0.2

**Spørgsmål fra relæ-AI ○:** ligger `_runtime.py:2202` i samme `derive_config`-funktion?
**Svar ●:** Nej — men kæden er direkte, uden mellemliggende lag der kan ændre værdien.

## Kæden (alle linjer verificeret i sdist 3.0.2)

```
acp/agent.py:1808          size=runtime.context_window
  └─ _runtime_resources.py:380-381   property context_window → self._state.context_window
       └─ client_state.py:80         self.context_window = snapshot.context_window
            └─ _runtime.py:2202       RuntimeSnapshot(context_window=active_model.auto_compact_threshold)
                 i build_unified_runtime_snapshot (def ved 2147)
                 └─ kaldt fra _runtime.py:1439 inde i derive_config (def ved 1352)
```

`derive_config` er en **indre funktion** (linje 1352, indlejret i klassen fra ~696). Den kalder `build_unified_runtime_snapshot` ved linje 1439, og returværdien bærer `context_window=active_model.auto_compact_threshold` (linje 2202).

## To forskellige `context_window` — samme navn, to betydninger ●

| Linje | Funktion | Værdi | Går til |
|---|---|---|---|
| `_runtime.py:1584` | `derive_config` (indre) | `active_model.max_context_length` | `LocalModelRoute.context_window` → Rust-kernen (modellens reelle vindue) |
| `_runtime.py:2202` | `build_unified_runtime_snapshot` | `active_model.auto_compact_threshold` | `RuntimeSnapshot.context_window` → ACP-laget (`usage_update.size`) |

Det er en **navnekollision i agentens egen kode**: `RuntimeSnapshot.context_window` bærer tærsklen, ikke vinduet. Det er ikke en fejl vi har læst forkert — det er hvad koden faktisk gør.

## Hvorfor samme tal for begge glm-modeller ●

- `glm-5-3`: intet `max_context_length` erklæret, `auto_compact_threshold=800000` (GrowthBook) → `size=800000`
- `zai-glm-5-3-flash`: `max_context_length=1048576`, `auto_compact_threshold=800000` (GrowthBook) → `size=800000`

Begge får 800000 fordi værdien kommer fra **samme payload-felt** (`auto_compact_threshold`), ikke fra vinduet. Wire-målingen (`size=800000` for flash med vindue 1048576) er dermed forklaret præcist.

## Sentinel-status ● (fra tidligere analyse, nu lukket helt)

`RuntimeSnapshot.context_window` er erklæret `int` (ikke Optional) i `protocol.py:1102`. `_apply_auto_compact_threshold` (`vibe_schema.py:1247-1276`) garanterer en konkret værdi i alle fire grene — `-1` kan ikke nå linje 2202. **`0` kan** (modellen kan erklære "never compact"), og L5-guarden mod `size <= 0` står derfor.

Der er også en event-baseret opdatering (`session.py:966`: `self._state.context_window = event.params.context_window`), så værdien kan ændre sig midt i sessionen ved modelskift — svarende til R37 (configOptions er fornyelig).

## Konsekvens for Skrynja ● (N8 lukket endeligt)

- `usage_update.size` = **kompaktionsgrænse** (`auto_compact_threshold`), aldrig kontekstvinduet. Navngivningen «kompaktionsgrænse» er ikke blot en regel — det er en korrekt beskrivelse af data.
- En procent `used / size` måler **afstand til kompaktion**, ikke afstand til vinduesgrænsen. Skrynja kan vise den som sådan.
- `max_context_length` (det reelle vindue) når klienten **ikke** via ACP — hverken i `configOptions` (N7) eller i `usage_update` (N8). Hvis Skrynja vil vise vinduet, skal det læses fra `~/.vibe/experiment_eval_cache.json` eller accepteres som ukendt.
- Ved modelskift kan `size` ændre sig midt i sessionen — klienten skal opdatere grænsen ved hver `usage_update`, ikke cache den.

## Åbne spørgsmål

Ingen ○ fra denne analyse. R34 og R41 står (klientside, ikke-blokerende).
