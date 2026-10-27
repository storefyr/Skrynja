# Generel svarfrist i transportlaget ● — A8

Relæets indvending mod A6a var korrekt: en forkastet komplet linje over 8 MiB kan være svaret på en udestående anmodning — og så hænger klienten uden hverken svar eller fejl. Løsningen er ikke specifik for A6a; den bør være en **generel mekanisme i transportlaget**.

## A8 — svarfrist på udestående anmodninger ● (nyt krav, vedtaget)

**Mekanisme:** hver klient→agent-anmodning med `id` får en frist. Når fristen udløber uden svar, afvises anmodningen lokalt med en syntetisk fejl, og UI'et frigøres.

| Aspekt | Specifikation |
|---|---|
| Frist | **120 sekunder** fra afsendelse (relæets forslag; dækker långsomme netværkskald og store kompaktér-turns) |
| Gælder for | `initialize`, `session/new`, `session/load`, `session/prompt`, `session/set_config_option`, `session/close`, `session/fork`, og svar på `session/request_permission` (klient→agent-anmodninger med id) |
| Gælder **ikke** for | Notifikationer uden id (`session/cancel` — ingen svar forventes) og indkommende agent-anmodninger (`session/request_permission` fra agenten — den har sin egen timeout agent-side; klienten svarer når brugeren beslutter) |
| Ved udløb | Anmodningen fjernes fra den udestående tabel; kalderen får `ACPError{code: -32000, message: "Request timed out after 120 s"}` (klient-side syntetisk, ikke fra agenten) |
| UI | «Anmodningen fik ikke svar — prøv igen» med mulighed for at annullere/retry |
| Interaktion med cancel | Ved `session/cancel` på en session fjernes alle den sessions udestående anmodninger med `stopReason: "cancelled"` (ikke timeout) |

## Hvilke tabte-svar-scenarier fristen dækker ●

| Scenario | Uden frist | Med A8 |
|---|---|---|
| A6a: svar-linje over 8 MiB forkastes | Hængende UI, ingen fejl | Timeout efter 120 s, syntetisk fejl, UI frigjort |
| A6b: transport-fejl lukker agenten | Hængende UI hvis luk ikke detekteres | QProcess::finished rydder alle udestående med transport-fejl (umiddelbart) |
| Agent-process dør uden at lukke stdout rent | Hængende UI | Samme — `finished`-signal rydder |
| Agent hænger i et långsomt kald uden at svare | Hængende UI | Timeout efter 120 s |
| Netværks-langsomt svar (legitimt, >120 s) | — | Falsk timeout — brugeren kan retry; acceptabelt på en telefon |

## Margin-regning — rettet ●

Relæet har ret i regnestykket: største **kendt enkelt-output** er 1 MB (hook). 8 MiB / 1 MB = **8× margin**, ikke 2×.

Min «2×»-regning forudsatte, at `content[]` og `rawOutput` bærer samme output samtidig på samme linje (~2 MB råt, ~4 MB escaped). Jeg verificerede i koden: det er kun tilfældet ved **replay** (`_replayed_effect_content`, `session_updates.py:393-405` — bruges af `session/load`, hvor både `content` med fuld output OG `rawOutput` sendes i samme `tool_call`). Ved normal progress (`_effect_progress_updates`, linje 350+) sendes kun **deltaer** i `content` og `rawOutput` kun ved terminal state — så en enkelt progress-linje bærer typisk ikke begge fulde kopier.

**Korrekt formulering ●:** marginen er **8× over største kendt enkelt-output** (1 MB hook), og **2× over worst-case replay-linje** (~4 MB escaped, kun ved `session/load`). Begge er rigelige; 8 MiB står.

## Opdateret A6-spec — refererer til A8 ●

A6a-reaktionen tilføjes: «ved forkastet linje tjekkes den udestående anmodningstabel — hvis linjen kunne have båret et svar (kan ikke afgøres uden at parse), afventer A8-fristen; anmodningen times ud efter 120 s med syntetisk fejl». A6a dræber ikke forbindelsen; A8 sikrer at ingen anmodning hænger for evigt.

## Testplan — A8-tilføjelser ●

| Test | Forventet |
|---|---|
| Anmodning uden svar i 120 s (mock-agent) | Syntetisk timeout-fejl `-32000` efter præcis 120 s; anmodning fjernet fra tabel |
| Svar ankommer efter 119 s | Svar behandles normalt; ingen timeout |
| `session/cancel` mens anmodning udestående | Anmodning fjernes med `cancelled`, ikke timeout |
| Agent-process dør med anmodning udestående | Alle udestående ryddes med transport-fejl ved `finished` (ikke først efter 120 s) |
| A6a + udestående svar | Linje forkastes; anmodning times ud ved 120 s; UI viser fejl |

## Status ●

A8 tilføjet som generel transportlagsmekanisme. A6a/A6b refererer nu til den. Margin rettet til 8×/2× (begge formuleringer korrekte i deres kontekst). Parser-spec + transport-spec komplet. Ingen arkitekturvalg genåbnes.
