# Målejournal — runde 3: wire-bekræftelser fra telefonen (Jolla Phone 2026)

**Dato:** 26.10.2026 · **Kilde:** Målinger kørt på telefonen af brugeren, rapporteret via relæ-AI
**Mærkning:** ● målt på wire · ○ ikke målt · ◐ slutning

---

## N8 — ● BEKRÆFTET PÅ WIRE

**Måling:** For `zai-glm-5-3-flash` (`max_context_length = 1048576`, `auto_compact_threshold = 800000`) rapporterer `usage_update.size = 800000`.

| Felt | Værdi målt | Forventet hvis «rigtigt vindue» | Konklusion |
|---|---|---|---|
| `size` | **800000** | 1048576 | **N8 holder:** `size` = `auto_compact_threshold`, ikke `max_context_length` |

Kodelæsningen (`_runtime.py:2202`: `context_window=active_model.auto_compact_threshold`) og wire-målingen stemmer overens. `size` er **kompaktionsgrænsen**, ikke vinduet.

**Klientsidekonsekvens (låst):** Skrynja må mærke `size` som «kompaktionsgrænse», aldrig som «context window». En procentberegning `used / size` viser «hvor tæt på kompaktion», ikke «hvor fyldt er konteksten».

---

## L5 — ● KORRIGERET (filter på `used == 0` alene)

**Måling:** `used = 0` optræder sammen med **positiv** `size` både før og efter gyldigt forbrug.

Det betyder:
- `used = 0` er **ikke** et cancel-signatur alene — det optræder også ved session-start og efter gyldigt forbrug (f.eks. efter reset eller når agenten ikke har målt endnu).
- Filteret `used == 0 && size == 0` (fra forrige revision) er **for smalt** — det ville lade `used = 0, size = 800000` passere og overskrive en gyldig visning med 0.

**Korrigeret regel:**

| Regel | Begrundelse |
|---|---|
| **Filtrér alle `usage_update` hvor `used == 0`** — uanset `size` | Nul-usage må aldrig overskrive registreret forbrug (Fase 4 + denne måling) |
| Guard mod `size <= 0` før procentberegning | `size = 0` = «kompaktion slået fra» (dokumenteret); negativ ≠ -1 kan teoretisk lække |
| Vis `used` fra første `used > 0` | Før det er der intet meningsfuldt at vise |
| Vis `size` kun hvis `size > 0`, mærket «kompaktionsgrænse» | N8 bekræftet på wire |
| Vis «kompaktion slået fra» når `size == 0` | Dokumenteret betydning af 0 — information, ikke fejl |

**Korrigeret pseudokode:**

```cpp
void onUsageUpdate(int used, int size) {
    if (used == 0) return;                     // L5 korrigeret: filtrér used==0 alene
    showUsed(used);                            // fra første used > 0
    if (size > 0) {
        showLimit(size, tr("kompaktionsgrænse"));  // N8 wire-bekræftet
        showPercent(used, size);               // kun når size > 0
    } else if (size == 0) {
        showCompactionDisabled();              // «kompaktion slået fra»
    }
    // size < 0: vis ingenting (guard alligevel)
}
```

---

## N6 — ○ fortsat uafgjort

Ingen kontekstfyldning opnået på telefonen. `RustDisabledCompactionPolicy` gælder derfor stadig kun for standardkonfigurationen (kodelæst), ikke bekræftet på wire.

**Hvad der stadig mangler:** En session lang nok til at fylde konteksten, med observation af om der kommer `auto_compact_triggered` (Core-kompaktning aktiv → N6 falsk) eller direkte `3051` (standalone-fallback → N6 bekræftet).

**Alternativ måling (mindre præcis, men hurtigere):** Søg i agentens logfiler efter kompaktionsrelaterede events:
```sh
grep -i "compact" $HOME/.vibe/logs/*.log 2>/dev/null | tail -20
```

---

## Samlet status efter runde 3

| Punkt | Status |
|---|---|
| N5 | ● lukket — GrowthBook-payload, præciseret formulering |
| N6 | ○ uafgjort — ingen kontekstfyldning opnået; kodelæst siger standardkonfig = disabled |
| N7 | ● lukket — omformuleret til `usage_update` (ikke configOptions) |
| N8 | ● **lukket — bekræftet på wire**: `size` = `auto_compact_threshold` |
| L5 | ● **lukket — korrigeret**: filtrér `used == 0` alene + guard `size <= 0` |
| R32 | ◐ delvist afgjort — 800 000 bekræftet som tærskel på wire; kilden (GrowthBook vs config.toml) uafgjort |

## Anbefaling til Skrynja-klienten (låst)

1. **Vis `used`** fra første `usage_update` med `used > 0`; ignorér alle med `used == 0`.
2. **Vis `size` som «kompaktionsgrænse»** kun når `size > 0`; mærk den tydeligt — det er ikke vinduet.
3. **Vis «kompaktion slået fra»** når `size == 0`.
4. **Procentberegning kun når `size > 0`** — ellers division mod nul.
5. **Ingen advarsel mod hårdkodet tærskel** — brug wire-værdierne.
