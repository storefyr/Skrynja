# Afsluttende status — måleserien færdig ●

Denne journal samler relæ-AI'ens afsluttende status med mine kodemæssige verifikationer i sdist 3.0.2. Alle punkter er nu ● medmindre andet er angivet.

## N5 lukket med filbevis ●

`~/.vibe/experiment_eval_cache.json` er skrevet af den generiske **keyed JSON-cache** (`vibe/core/utils/keyed_json_cache.py:5,32,44`): hver post har `stored_at_timestamp` + `payload`, keyed pr. credential-fingerprint. To hash-nøgler på telefonen.

Payloadets features (bekræftet i `vibe/core/experiments/active.py:15-25`):

| Feature | Konstant |
|---|---|
| `vibe_cli_system_prompt` | `SYSTEM_PROMPT` |
| `vibe_cli_managed_shell_tools` | `MANAGED_SHELL_TOOLS` |
| `vibe_cli_default_routing_model` | `CLI_MODEL_ROUTING` |
| `vibe_cli_smart_approve` | `SMART_APPROVE` |
| `vibe_cli_smart_approve_default` | `SMART_APPROVE_DEFAULT` |
| `vibe_cli_extra_models` | `CLI_EXTRA_MODELS` |

Mapping (`growthbook.py:88-100`): `CLI_EXTRA_MODELS` → `routed_extra_models` → `_routed_models()` (`growthbook.py:170-190`) → `ModelConfig.model_validate(entry)` → `data["models"]` — **uden om `_CATALOG_FIELDS`**. Tærsklen bæres altså i eksperimentlaget, ikke som katalogattribut. N5 formulering fra relæ-AI'en accepteret: *tærsklen kommer fra GrowthBook-payloadet, ikke fra modelkataloglaget* ●.

**Korrektion ●:** `extra_models` rummer **2** modeller (`glm-5-3`, `zai-glm-5-3-flash`). De 18 tærskel-forekomster i filen er samme payload gentaget på tværs af hash-nøgler og features — ikke 18 modelposter.

`vibe_cli_default_routing_model.defaultValue`: `active_model: glm-5-3`, `fallbacks.multimodal: mistral-medium-3.5`.

## Modeltal ●

| Model (alias) | `max_context_length` | `auto_compact_threshold` | Tærskel/vindue |
|---|---|---|---|
| `glm-5-3` | **None (ukendt)** | 800 000 | — |
| `zai-glm-5-3-flash` | 1 048 576 | 800 000 | **76,3 %** (800000/1048576 = 76,29) |

## N8 står ● — tre uafhængige kilder

1. Wire: `usage_update.size = 800000` for `zai-glm-5-3-flash`
2. Wire: `token_threshold: 800000` i kompaktionspolitikken
3. Kodekæde verificeret linje for linje på installeret pakke (relæ-AI) og i sdist (mig):
   `_runtime.py:1352` `derive_config` → `:1439` `build_unified_runtime_snapshot` → `:2147` def → `:2202` `context_window=active_model.auto_compact_threshold` → `acp/agent.py:1808` `size=runtime.context_window`

Vinduet ligger i stedet på `_runtime.py:1584` (`max_context_length` → `LocalModelRoute` → Rust-kernen) og når ikke ACP-laget.

## Nyt risiko: R12 ●

**R12:** *Feltnavn må ikke antages at have samme semantik på tværs af lag.*
`context_window` betyder `max_context_length` i Rust-kernen (linje 1584) og `auto_compact_threshold` i ACP-laget (linje 2202). Afbødning: følg altid tildelingsstedet, aldrig navnet.

## Klientregel: vindue vs tærskel ● (ny nuance)

For `zai-glm-5-3-flash` udgør tærsklen 76,3 % af vinduet — kompaktion nås sandsynligvis før vinduesgrænsen.

For `glm-5-3` er vinduet **ukendt** — ingen målte kilder erklærer det. Hvis det faktiske vindue er mindre end 800 000, nås kompaktionen **aldrig**, og fejlen kommer som kontekstoverskridelse i stedet for `3051`.

**Klientregel ●:** vis kun advarslen «nærmer sig kompaktionsgrænsen», når vinduet er kendt og ligger over tærsklen. Ellers vis **«ukendt»** — aldrig udled vinduet af `size`.

## Klientregler — klar til implementering ●

| Vises | Kilde | Når kilden svigter |
|---|---|---|
| «Kompaktion ved …» | `usage_update.size`, læst ved hver hændelse | `size <= 0` ⇒ «kompaktion slået fra» |
| «Kontekstvindue …» | `extra_models[].max_context_length` (fra `~/.vibe/experiment_eval_cache.json`) | felt mangler ⇒ **ukendt**, aldrig udledt af `size` |
| Advarsel ved `used → size` | kun når vindue kendt og > tærskel | ellers «ukendt» |
| Modelmenu | `configOptions.model.options` | `Skrynja.conf` er forældet (R33) |
| `low` for zai-glm | undertrykkes (A7) | fører til `3051` |
| `tool_call`/`tool_call_update` | vises altid (A5/H2) | `permission = "always"` + allowlist giver ingen dialog |
| Modelskift | én samlet nulstilling af tærskel, `low`-regel og katalog | `size` kan ændre sig midt i sessionen (`session.py:966`) |

## Implementationsrækkefølge ●

1. Transport + tolerant NDJSON-parser over `stdio` (Trin E)
2. `initialize`/`session/new`/`session/prompt` + `cancel`
3. `session/update`-håndtering (agent_message, tool_call, tool_call_update, usage_update)
4. `usage_update`-visning (L5, N8, R12, vindue-ukendt-regel)
5. `configOptions`-drevne menuer + A7 (`low`-undertrykkelse)
6. `session/request_permission`-håndtering (defensiv — betinget kald)
7. A5/H2: læs og vis `permission`/`allowlist` fra `~/.vibe/config.toml`

## Åbne ○ — ingen blokerende

| ID | Spørgsmål | Klientrelevans |
|---|---|---|
| — | `glm-5-3`'s reelle vindue | lav — vises som «ukendt» |
| R34 | hvilken `Skrynja.conf`-sti er aktiv | lav — modelmenu kommer fra `configOptions` |
| R41 | `model_catalog_cache.json` skrives aldrig, mens eval-cachen skrives | lav — mulig flaggate eller credential-fingerprint |

## Risikoregister — endelig tilstand

| ID | Risiko | Status |
|---|---|---|
| R11 | Template/fallback-konfiguration læst som aktiv | ● lukket (følg `configure_runtime`-kæden) |
| R12 | Feltnavn med forskellig semantik på tværs af lag | ● lukket (følg tildelingsstedet) |
| R30 | `low→"none"` i to uafhængige filer | ● agent-intern; undgås via A7 |
| R31 | `disable_reasoning=standalone` tvinger `"none"` på kompaktionsvejen | ● betinget; kun ved standalone-fallback |
| R32 | Kompaktionsrisiko ved 800 000 på `glm-5-3` | ● reel; advarsel ved `used → size` når vindue kendt |
| R37 | `configOptions` fornyelig midt i session | ● håndter `config_option_update` |
| R40 | Katalogcache-TTL 30 døgn | ○ information |
| R42 | Tom katalog er authoritative | ● vis fejl, ikke tom menu |
