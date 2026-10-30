# Beslutning: thinking-UI (sidefund 1) + fravalgslistens placering (A7 som tilstand) ●

## Verifikation af relæets sidefund i sdist 3.0.2 ●

**Sidefund 1 — `medium`, `high`, `max` er identiske over for API'et ● bekræftet:**

`vibe/core/llm/backend/mistral.py:257-262` (core) og `harness/.../adapters/mistral.py:78-84` (harness):

```python
_THINKING_TO_REASONING_EFFORT = {
    "low": "none",
    "medium": "high",
    "high": "high",
    "max": "high",
}
```

Alle tre bliver `"high"` på wire. Kun `off` (→ felt udeladt eller `"none"` afhængig af curated), `low` (→ `"none"`) og `high` (→ `"high"`) har reel effekt. `medium` og `max` er **aliases for `high`**.

**Sidefund 2 — `off` findes ikke i tabellen ● bekræftet:** `_THINKING_TO_REASONING_EFFORT.get("off")` giver `None` → feltet udelades (harness) eller `"none"` kun på curated sets (core, `_reasoning_effort()` betinget af `thinking_levels_curated`). `off` og «intet `reasoning_effort`» er samme tilstand på wire for uncurated modeller. Klienten kan ikke skelne dem — og behøver ikke.

**Koordinater ○:** relæet har ret — tabellen ligger i `core/llm/backend/mistral.py:257` på den kørende installation, ikke `adapters/mistral.py:135`. Sdist ≠ installeret træ. **R13 gælder:** citer udtryk, ikke linjenumre, når kilderne divergerer. Begge tabeller findes i sdist'en (to uafhængige forekomster, R30) — på telefonen ser relæet kun core-stien. Konsekvensen (A7) er identisk begge steder.

## (a) Beslutning: skal `medium`/`max` skjules eller markeres? ●

**Beslutning: vis dem, men markér dem som ligestillede med `high` — skjul ikke.**

Begrundelse:

1. **Skjulning er et fravalg af agentens erklærede muligheder.** `configOptions.thinking.options` kommer fra modellens `thinking_levels` — agenten har besluttet, at denne model tilbyder `medium` og `max`. Hvis Skrynja skjuler dem, fjerner klienten valg, som agenten mener er gyldige, uden at kunne begrunde det over for brugeren.
2. **Markering bevarer information uden at vildlede.** En note som «samme som High» (eller «High») ved siden af `Medium` og `Max` gør det klart for brugeren, at der ingen forskel er på wire — samtidig med at valget forbliver tilgængeligt, hvis en fremtidig agent-version igen begynder at skelne.
3. **Det er fremadkompatibelt.** Hvis Mistral en dag giver `max` en reel, stærkere betydning end `high`, skal Skrynja ikke bygges om — kun mærkningen fjernes.
4. **Det følger mønsteret fra A7.** A7 fjerner `low` fordi det **fejler** (`3051`). `medium`/`max` fejler ikke — de er bare ækvivalente. Forskellen mellem «kan ikke vælges» og «vælges uden forskel» bør afspejles i UI'et.

**UI-regel ●:** for hver option i `configOptions.thinking.options`, vis værdien. Hvis værdien er `medium` eller `max`, og den aktuelle model ikke er kendt for at skelne dem (ingen målte model gør), vis «(samme som High)» som sekundær tekst. Hvis en fremtidig måling viser en model, der skelner, falder mærkningen væk for den model.

## (b) Beslutning: fravalgslistens placering ●

**Beslutning: vedtaget som foreslået — kombination, A7 bliver en tilstand.**

Tre lag, i prioriteret rækkefølge (senere lag vinder ved konflikt):

| Lag | Mekanisme | Indhold | Vedligeholdelse |
|---|---|---|---|
| 1. Hardcoded standard | C++-konstant i Skrynja | `glm-5-3`, `zai-glm-5-3-flash` | kræver ny klientversion |
| 2. Brugeroverride | `Skrynja.conf` (QSettings, eksisterende `SettingsManager`) | `acp/thinkingSuppressLow` = kommasepareret liste | kan rettes uden genbygning |
| 3. Indlært | samme `Skrynja.conf`-nøgle, skrevet ved `3051` | alias tilføjes automatisk når fejlen måles | selvrettende |

**Hvorfor kombinationen:**

- Lag 1 sikrer, at de to kendte modeller er dækket fra dag ét, selv på en telefon uden config-ændringer.
- Lag 2 giver brugeren (og os) en måde at udvide eller indsnævre listen uden at bygge en ny klient — kritisk på Sailfish, hvor hver udgivelse går gennem Harbour-review.
- Lag 3 gør klienten selvrettende: når en ny model rammes af `3051`, huskes det, og `low` undertrykkes næste gang samme model vælges.

**Hvordan lag 3 lærer ●:** ved en `session/prompt`-fejl med `code == "3051"` (eller tilsvarende API-400-mønster i `data`), og hvor den aktive model er kendt fra `configOptions.model.currentValue`, tilføjes aliaset til `acp/thinkingSuppressLow` i `Skrynja.conf`. Ved næste `thinking`-menu-bygning for den model undertrykkes `low`. Brugeren informeres: «Low er ikke tilgængelig for denne model og er skjult».

**Konfliktregel ●:** hvis brugeren eksplicit har fjernet et alias fra `Skrynja.conf` (lag 2), må lag 3 ikke skrive det tilbage automatisk uden en markering. Enkel løsning: lag 3 skriver kun til en separat nøgle `acp/thinkingSuppressLowLearned`, og den effektive liste er foreningen af lag 1 + lag 2 + lag 3. Brugeren kan så tømme lag 3 uden at røre lag 2.

**Effektiv fravalgsliste ●:** `hardcoded ∪ Skrynja.conf(acp/thinkingSuppressLow) ∪ Skrynja.conf(acp/thinkingSuppressLowLearned)`

**QSettings-sti ●:** Skrynja bruger allerede `skrynjaSettingsPath("Skrynja")` = `QStandardPaths::AppConfigLocation + "/Skrynja.conf"` (`conversationmanager.cpp:26-30`, `settingsmanager.cpp:55`). Det svarer til én af de to målte stier på telefonen (`~/.config/harbour-skrynja/Skrynja.conf` eller `~/.config/org.skrynja/Skrynja/Skrynja.conf` — R34 ○ stadig åben, men irrelevant for denne beslutning, da QSettings selv vælger den aktive).

## Opdaterede klientregler ●

| Regel | Kilde |
|---|---|
| `thinking`-menu fra `configOptions.thinking.options` | `acp/utils.py:176-186` |
| Fjern `low` hvis aktiv model ∈ fravalgsliste (3 lag) | denne beslutning |
| Vis `medium`/`max` med «(samme som High)» | sidefund 1, denne beslutning |
| `off` vises altid (sikker, N1) | N1 ● |
| Ved `3051`: tilføj aktiv model til `acp/thinkingSuppressLowLearned`, informer bruger, foreslå `off`/`high` | denne beslutning |
| Fravalgslisten genberegnes ved modelskift (én samlet nulstilling) | afsluttende status ● |

## Uændret ●

A1, A2, «kun `session/set_config_option`», N8/R12, L5, R37/R42, Trin E — alle låst.
