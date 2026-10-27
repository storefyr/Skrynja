# Lag 3 — erstattet med bekræftet læring ● + R41 introduceret og afgjort ◐

## Indvending mod tekstmatch ● accepteret — samme fejlklasse som R13

Relæet har ret: at matche fri tekst fra providerens response body — formuleret af en tredjepart, på et sprog og i et format Skrynja ikke kontrollerer — er **samme fejlklasse som R13** (tekstsøgning som bevis). Providerens formulering kan ændre sig, lokaliseres, eller formuleres på en måde mønsteret ikke fanger. En falsk positiv gør lag 3 til en stille, permanent, forkert tilstandsændring.

## Lag 3 — beslutning: bekræftet læring, ikke automatisk ●

| Alternativ | Vurdering |
|---|---|
| Automatisk læring på tekstmatch | ○ afvist — falsk positiv sandsynlig; samme fejlklasse som R13 |
| **Læring efter brugerbekræftelse** | ● **vedtaget** — falsk positiv umulig; koster ét klik, kun ved reel fejl |
| Ingen læring, kun manuel override | ● acceptabel fallback, men dårligere UX |

**Vedtaget mekanisme ●:**

1. Ved `-32603` + `data.code == "3051"` på første `session/prompt` efter et `thinking`-skift til `low` på en model, der **ikke** allerede er på fravalgslisten:
2. Skrynja viser en dialog: **«Low blev afvist for `<model>`. Skjul Low for denne model?»** med knapperne «Ja, skjul» / «Nej»
3. Ved «Ja, skjul»: aliaset skrives til `acp/thinkingSuppressLowLearned` i `Skrynja.conf`, `thinking`-menuen genbygges uden `low`, og brugeren informeres
4. Ved «Nej»: intet skrives; fejlen vises som normalt

**Egenskaber ●:**

- **Falsk positiv umulig** — listen ændres kun ved eksplicit brugerhandling
- **Synligt i samme øjeblik** — lag 3 skrives kun sammen med en dialog, brugeren ser årsagen
- **Fortrydeligt** — «Nulstil»-vejen (indstillingsside) tømmer `acp/thinkingSuppressLowLearned` uden at røre lag 2; plus brugeren kan redigere filen direkte på `~/.config/org.skrynja/Skrynja/Skrynja.conf` (R34 ●)
- **Kontekstbetingelsen beholdes** — dialogen vises kun ved første prompt efter skift til `low`; en `3051` midt i en lang samtale (hvor årsagen kan være noget andet) udløser ikke dialogen, kun fejlvisning

**Lag 3 er nu en tilstand, der kun ændres ved brugerens egen handling ●.**

## «Den anden sti er et levn» — præciseret ○

Påstanden kan ikke efterprøves i repoet alene (begge stier afhænger af runtime-miljøet). Hvad repoet viser: `harbour-skrynja.cpp:26-27` sætter `org.skrynja`/`Skrynja`, og **ingen kode** i Skrynja refererer `harbour-skrynja` som config-mappe (kun som QML-sti og translations-navn). Så `~/.config/harbour-skrynja/Skrynja.conf` kan kun stamme fra en ældre version, hvor organizationName ikke var sat, eller fra en ekstern komponent. Formuleringen «levn» står som ○ antagelse, indtil `ls` på telefonen viser hvad der faktisk ligger i mapperne. **Ikke-blokerende** — QSettings bruger kun den aktive sti.

## R41 — introduceret og afgjort ◐ ●

R41 er **ikke** et vejkort-punkt fra Rev. 13 — det opstod i måleserien (relæ-AI'en observerede det på telefonen). Definition:

> **`~/.vibe/model_catalog_cache.json` skrives aldrig, mens `~/.vibe/experiment_eval_cache.json` skrives.**

**Kodelæst i sdist 3.0.2 ● — årsagen fundet:**

`vibe/core/model_catalog/session.py:26`:

```python
if not config.experimental_enable_model_catalog:
    logger.debug("Model catalog fetch skipped: experimental flag off")
    return None
```

Flagget `experimental_enable_model_catalog` (`vibe_schema.py:732`) har **`default=False`**. Uden flagget hentes kataloget **aldrig**, og dermed skrives `model_catalog_cache.json` aldrig (`store_cached_model_catalog` kaldes kun efter en succesfuld fetch, `session.py:73`).

Derudover: selv med flagget on kræves en Mistral-credential (`_cache_key` → `mistral_credential_cache_key` → `None` uden API-nøgle → hverken læsning eller skrivning, `cache.py:30-36,44-52`).

**Så R41 er ○ delvist afgjort:** årsagen er **flaggated** (`experimental_enable_model_catalog=False` er default), ikke en fejl. På telefonen er flagget ikke sat i `~/.vibe/config.toml` (målt ○ — antaget ud fra at filen ikke skrives; en eksplicit `experimental_enable_model_catalog = true` ville ændre det).

**Konsekvens for Skrynja ○:** ingen direkte — Skrynja får model-listen fra ACP `configOptions` (R37/R42), ikke fra katalog-cachen. R41 er kun relevant hvis Skrynja senere vil læse `max_context_length` fra `experiment_eval_cache.json` (N8-udvidelsen: «kontekstvindue» fra GrowthBook-payloadet) — og den fil **skrives** (N5 ●), så den vej er åben.

## Opdaterede klientregler ●

| Regel | Kilde |
|---|---|
| Lag 3 ændres kun ved brugerbekræftelse («Ja, skjul») | denne analyse |
| Dialog vises kun ved `-32603` + `3051` + første prompt efter skift til `low` + model ikke allerede på listen | denne analyse |
| Ingen tekstmatch mod provider-beskeder | denne analyse (R13-lærd) |
| «Nulstil» tømmer `acp/thinkingSuppressLowLearned`, rører ikke lag 2 | tidligere beslutning |
| R41: katalog-cache er flaggated (`experimental_enable_model_catalog=False` default); ikke-blokerende | denne analyse |

## Alle arkitekturvalg låst ● (endelig)

A1, A2, kun `session/set_config_option`, A7 som 3-lags tilstand med **bekræftet** læring, thinking-UI med «samme som High», N8/R12, L5, R37/R42, Trin E, R34 afgjort, R41 afgjort (flaggate, ikke-blokerende). **Ingen åbne ○ blokader implementationen.**
