# A8 — progress-baseret frist for `session/prompt` ● + `session/fork` verificeret (unstable) + sti-rettelse

Relæets gennemgang bekræftede caps, citat og margin — og fandt en alvorlig fejl i A8's blanket-frist. Denne analyse retter A8 og lukker formalia.

## 1 · Replay-citat bekræftet — sti rettet ●

Relæet har ret: filen er **`vibe/acp/session_updates.py`**, ikke `vibe/app_server/`. Linjenumre passer præcist:

- `_effect_progress_updates` (350–356): `output_delta = _text_delta(previous_output, _output_text(entry))` — normal drift sender **deltaer** ●
- `_replayed_effect_content` (393–405): returnerer `content` for `IN_PROGRESS`-poster og tilføjer output-tekst — replay bærer **både** `content[]` og output i samme `tool_call` ●

**Præcisering accepteret ●:** replay-linjen (`session/load`) er den **største kendte legitime linje** — det er den, 8 MiB faktisk skal tåle, ikke gennemsnitslinjen.

## 2 · Margin — begge formuleringer korrekte ● (tabel fra relæet accepteret)

| Kontekst | Største linje | Margin ved 8 MiB |
|---|---|---|
| Normal drift (deltaer) | ~1 MB (hook) | **8×** |
| Replay (`session/load`) | ~4 MB escaped | **2×** |

8 MiB står ●.

## 3 · A8 rettet — progress-baseret frist for `session/prompt` ●

**Relæets indvending korrekt ●:** `session/prompt` kan legitimt tage minutter. `bash` har `default_timeout: None` (målt i `bash.py:396-404`), og en kompaktér-turn kan køre længe. En fast 120 s-frist på `session/prompt` ville syntetisere `-32000` **midt i legitimt arbejde**, mens agenten fortsætter — en værre fejl end den, A8 skulle lukke.

**Rettet fristmodel ●:**

| Anmodning | Frist | Begrundelse |
|---|---|---|
| `initialize`, `session/new`, `session/load`, `set_config_option`, `close` | **120 s fast** ● | Hurtige operationer; agenten svarer øjeblikkeligt eller aldrig |
| `session/prompt` | **Progress-baseret** ● | Forny fristen ved hver indkommende `session/update` for sessionen; kun reel tavshed i 120 s udløser timeout |
| `session/fork` | **120 s fast** ● | Se punkt 4 — verificeret, men se bemærkning |
| Svar på `request_permission` (klient→agent) | **Agentens egen frist** ● | Ikke klientens — agenten har sin egen timeout; klienten svarer når brugeren beslutter |

**Progress-baseret frist — mekanisme ●:**

```
ved afsendelse af session/prompt:
  sæt promptDeadline = nu + 120 s
  sæt promptLastActivity = nu

ved hver indkommende besked for sessionen (session/update, ethvert svar):
  hvis sessionen har en udestående prompt:
    promptLastActivity = nu

timer-tjek (hvert sekund):
  hvis udestående prompt og nu - promptLastActivity > 120 s:
    → syntetisk timeout -32000 «No activity for 120 s"
```

**Hvad progress-baseret fanger — og ikke fanger ●:**

| Scenario | Resultat |
|---|---|
| A6a: svar-linje forkastet, derefter tavshed | Timeout efter 120 s reel tavshed ● (A6a lukket) |
| Lang bash-kommando med løbende `tool_call_update` | Frist fornyes ved hver update — ingen falsk timeout ● |
| Lang bash-kommando **uden** output i 120 s | Timeout — men agenten kører videre; ved genstart af prompt afbrydes den gamle turn (A1) |
| Kompaktér-turn med løbende updates | Frist fornyes — ingen falsk timeout ● |

Bemærk ○: et langt arbejde uden **noget** output i 120 s udløser stadig timeout. Det er korrekt adfærd (brugeren ser «ingen aktivitet»), og agenten kan fortsætte i baggrunden — ved næste prompt afbrydes den gamle turn.

## 4 · `session/fork` verificeret ● — unstable, men tilgængelig

Verificeret mod ACP 0.11.0 (wheel) + agent 3.0.2:

| Punkt | Resultat |
|---|---|
| SDK meta | `acp/meta.py:18` `"session_fork": "session/fork"` ● |
| Router | `acp/agent/router.py:100` — registreret med **`unstable=True`** ○ |
| Schema | `ForkSessionRequest` (`schema.py:3883`), `ForkSessionResponse` (`schema.py:4839`) ● |
| Capability | `sessionCapabilities.fork` — mærket **UNSTABLE** i schema (`schema.py:3256-3260`) |
| Agent-implementering | `agent.py:1100` `fork_session` — implementeret ● |
| Agenten kører unstable | `use_unstable_protocol=True` (`agent.py:1859`) — så fork er **aktiv** på denne agent ● |

**Konsekvens ●:** `session/fork` får **120 s fast frist** (som A8 oprindeligt). Men Skrynja bør **ikke kalde** `session/fork` i første version — den er unstable (kan fjernes/ændres), og ingen måling på telefonen har bekræftet den end-to-end. Fristen er dækkende, hvis en fremtidig version tilføjer fork.

## 5 · Formalia — `-32000` markeret som syntetisk ●

`-32000` er **ikke** en ACP-protokolkode fra agenten. ACP's server-error range er `-32000` til `-32099` (JSON-RPC 2.0 reserved), men agenten bruger målt `-32601`/`-32602`/`-32603`. Klientens timeout-fejl skal markeres som **syntetisk, klientgenereret** — aldrig forveksles med en agent-fejl:

```
ACPError{code: -32000, message: "Request timed out after 120 s (client-side timeout)", synthetic: true}
```

UI'et må vise «klient-timeout» forskelligt fra agent-fejl.

## Opdateret A8-spec ● (endelig)

| Anmodning | Frist | Type |
|---|---|---|
| `initialize` | 120 s | fast |
| `session/new` | 120 s | fast |
| `session/load` | 120 s | fast |
| `session/set_config_option` | 120 s | fast |
| `session/close` | 120 s | fast |
| `session/fork` | 120 s | fast (ikke kaldt i v1) |
| `session/prompt` | 120 s **progress-baseret** (forny ved hver besked for sessionen) | activity |
| `request_permission`-svar | agentens egen frist | — |

Syntetisk timeout: `-32000`, `synthetic: true`, message «… (client-side timeout)».

## Testplan — A8 endelig ●

| Test | Forventet |
|---|---|
| `initialize` uden svar i 120 s | Syntetisk timeout, `synthetic: true` |
| `session/prompt` med updates hvert 10 s i 10 min | Ingen timeout (frist fornyes) |
| `session/prompt` med 130 s tavshed | Timeout efter 120 s tavshed |
| `session/prompt` hvor A6a forkaster svar-linje, derefter tavshed | Timeout efter 120 s — A6a lukket ● |
| `session/cancel` med udestående prompt | Ryddet med `cancelled`, ikke timeout |
| Agent dør med udestående anmodninger | Alle ryddet ved `finished` med transport-fejl |
| `session/fork` (hvis kaldt) | 120 s fast frist |

## Status ●

A8 endelig: progress-baseret frist for `session/prompt`, fast 120 s for resten, agentens egen frist for permission-svar. `session/fork` verificeret (unstable, aktiv, men ikke kaldt i v1). `-32000` markeret syntetisk. Replay-sti rettet. Margin-tabellen accepteret. Parser-spec + transport-spec komplet.
