# A7 — korrigeret begrundelse ● kodelæst i sdist 3.0.2

**Relæets indvending var korrekt ●.** Min tidligere formulering — «agentens `thinking`-validering er hardcoded `{"off","low","medium","high","max"}` (`agent.py:1069-1075`) — ikke mod modellens `thinking_levels`» — holder **delvist**, men jeg overfortolkede den. Her er den præcise kæde.

## Svar på relæets spørgsmål ●

**Er `low` et niveau, som agenten afviser, eller et niveau modellen erklærer og API'et afviser?**

Begge dele — på to forskellige lag:

1. **ACP-laget (`agent.py:1069-1075`) accepterer `low`** — men kun mod den **generiske** mængde `THINKING_LEVELS = ("off","low","medium","high","max")` (`vibe/config_values.py:11-12`), **ikke** mod modellens `thinking_levels`. ACP-laget er et type-tjek, ikke et model-tjek.
2. **Model-laget accepterer også `low`** — `ModelConfig.thinking: ThinkingLevel` (`models.py:518`) og `_reset_thinking_outside_levels` (`models.py:563-575`) accepterer `low` så længe `"low" in self.thinking_levels`. For zai-glm erklærer GrowthBook-payloadet selv `thinking_levels: ["low","high","max"]` — **modellen lyver om sine egne niveauer** (R30-familien).
3. **API'et afviser** — harness-adapteren oversætter `low → "none"` (`adapters/mistral.py:78-84`), og zai-glm afviser `reasoning_effort:"none"` med HTTP 400 → ACP `-32603`, `code:"3051"`.

Så: **`low` er et niveau, modellen erklærer, agenten accepterer to gange (generisk + model), og API'et afviser.** Relæets verden-2 er den korrekte: *modellen lyver om sine egne niveauer*.

## Den fulde kæde for `set_config_option thinking=low` ●

```
ACP-klient
  └─ session/set_config_option {configId:"thinking", value:"low"}
       └─ vibe/acp/agent.py:1069-1075  case "thinking", str(level) if level in {off,low,medium,high,max}
            ├─ ACCEPTERER — mod THINKING_LEVELS (config_values.py:12), IKKE mod model.thinking_levels
            └─ config.set_thinking(level)
                 └─ _runtime_resources.py:182  set_thinking → write_model(reasoning_effort=level)
                      └─ "config/model/write" (protocol.py:1258, reasoning_effort: str | None — INGEN validering her)
                           └─ server.py:1417 → _unified_harness_backend_adapter.py:6485 write_model_config
                                └─ model_config_write_ops (_config_write.py:36)
                                     └─ op: set /models/{alias}/thinking = "low"  (ingen niveau-validering)
                                └─ config/_write_config → lag-merge → ModelConfig.model_validate
                                     └─ models.py:563-575 _reset_thinking_outside_levels
                                          └─ "low" in thinking_levels=["low","high","max"] → ACCEPTERES, ingen reset
       └─ svar: hela configOptions — thinking.currentValue = "low" ● (ingen fejl!)

Først ved næste prompt:
  └─ harness adapter adapters/mistral.py:135  _THINKING_TO_REASONING_EFFORT.get("low") → "none"
       └─ kwargs["reasoning_effort"] = "none"
            └─ Mistral API → HTTP 400 → ACP -32603, code "3051"  (målt på wire ●)
```

**Ingen lag mellem ACP og API afviser `low` for zai-glm.** Fejlen kommer først ved prompt-tid, som `-32603`/`3051`.

## Hvad betyder det for A7 ●

Relæets konsekvens er korrekt:

- A7's fravalgsliste må være **model-niveau, ikke kun to aliaser**. Hvis en fremtidig model i zai-glm-familien erklærer `thinking_levels` med `low` og rammes af samme `low→"none"`-tabel, rammer fejlen den næste model.
- **Bedre regel ●:** undertryk `low` i `thinking`-menuen for **enhver model, hvor `low` fører til `3051`**. Da klienten ikke kan kende wire-mappingen på forhånd, er den praktiske regel: fravalgsliste på model-niveau (alias eller mønster), udvidet når en ny model viser samme adfærd. Start med `glm-5-3` og `zai-glm-5-3-flash`; tilføj aliaset når en ny rammes.
- Alternativt ○ (ikke anbefalet): klienten kunne sende `low` og håndtere `3051` ved at nulstille til `off` og informere brugeren — men det giver en dårlig oplevelse (fejl midt i en turn) og er sværere at teste.

## Korrektion af tidligere påstande ●

| Tidligere påstand | Korrektion |
|---|---|
| «agentens validering er hardcoded `{off,low,medium,high,max}` — ikke mod modellens `thinking_levels`» | **Delvist korrekt** — ACP-laget tjekker kun mod den generiske `THINKING_LEVELS`, ikke mod model. Men det er ikke hele historien: model-laget (`_reset_thinking_outside_levels`) accepterer også `low`, fordi zai-glm selv erklærer det. Ingen lag afviser før API'et |
| «A7 er nødvendig fordi agenten accepterer `low`» | **Upræcis** — agenten accepterer `low` fordi modellen erklærer det. A7 er nødvendig fordi **modellen lyver** om sine niveauer, og fordi ingen agent-lag afviser før API-kaldet |
| Linje 743 for `config_option_update` | Korrekt i **min sdist** (`agent.py:743-745`), men relæet måler **1835** på telefonen — to forskellige builds/installationer. Begge er `_send_config_options`-kald; semantikken er ens. Noteret som versions-drift, ikke en fejl i kæden |

## Klientregel (A7, endelig) ●

| Regel | Kilde |
|---|---|
| Byg `thinking`-menuen fra `configOptions.thinking.options` | `acp/utils.py:176-186` (`make_thinking_response`) |
| Fjern `low` for modeller på fravalgslisten (start: `glm-5-3`, `zai-glm-5-3-flash`) | A7 — udvides ved nye ramte modeller |
| Fravalgslisten er **model-niveau**, ikke kun to aliaser — enhver model med samme wire-adfærd rammes | denne analyse |
| `off` er altid sikker (H-adapter: feltet udelades; V-adapter: `"none"` kun på curated sets — mistral-medium-3.5 har `["off","high"]`, curated) | N1 ● |
| Ved `3051` på prompt: vis fejl, behold lokal thinking-tilstand, foreslå `off` eller `high` | wire-målt ● |
