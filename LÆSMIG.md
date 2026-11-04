# Skrynja — læsmig

> README for dette repositorium. Dansk navn, samme funktion.

Skrynja er en ACP-klient til Sailfish OS. Klienten starter en lokal agent som
underordnet proces og lægger agentens arbejde ind i en grænseflade: værktøjskald,
filændringer, tilladelser og tilstande vises, mens de sker, og brugeren svarer
med et tryk i stedet for en kommando i en tekststrøm.

ACP er Agent Client Protocol: et sprog mellem klient og agent over standard-ind
og -ud. Skrynja er klienten. Agenten kører på telefonen, i telefonens filsystem.

## Hvem dokumentet er skrevet til

| Part | Rolle |
|---|---|
| **Projektejer** | Træffer beslutningerne. Måler på telefonen. Underskriver kontrakter. Implementerer ikke. |
| **Projektejerens maskinelle stokastiker** | Læser kilder, skriver sufflier og specifikation, gransker. Implementerer ikke; ingen skriverettighed. |
| **Mistral Vibe** | Implementerer efter suffli, bygger og kører CI på GitHub. Måler ikke på telefonen. |

## Succeskriteriet

```
prompt sendt → tilladelse spurgt → tilladelse besvaret
→ fil skrevet → end_turn → appen afslutter med kode 0
```

Kæden hænger ikke sammen endnu. Intet andet trin regnes som færdigt, før den gør.

## Målt tilstand

| Område | Tilstand |
|---|---|
| Aktiv bagende | ACP over standard-ind og -ud, i `src/acpclient.*` |
| **Uafklaret** | `src/mistralapi.*` og de Mistral-relaterede QML-sider findes stadig i træet. Om de kompileres med, er ikke målt |
| `initialize` | `protocolVersion: 1`, `clientInfo`, **tom** `clientCapabilities` |
| Kapabiliteter | ingen erklæret — agenten må derfor ikke kalde `fs/*` eller `terminal/*` |
| Session | `session/load` forsøges, falder tilbage til `session/new` ved fejl |
| Session-id | gemmes i indstillingerne under `acp/sessionId` |
| Indgående metoder med behandler | `session/update` og `session/request_permission` |
| `configOptions` | læses fra svar og sendes til grænsefladen |
| `modes` | læses ikke |
| Tilladelsesdialog | vises med `id`, `title` og `options` |
| `kind` i options | tabes — kun `optionId` og `name` bæres videre |
| Flere samtidige tilladelser | ikke muligt — én anmodning gemmes ad gangen |
| Indespærring | ingen; appen kører ikke i SailJail |
| Agentproces | startes med hardkodet sti `/home/defaultuser/.local/bin/vibe-acp`, et uv-værktøjssymlink |
| Log | alle `[acp]`-linjer spejles til `/tmp/skrynja-app.log` |

Den tomme kapabilitetserklæring betyder, at filadgang er en død vej i dag. Det
er en erklæring fra klienten, ikke en mangel i agenten.

## Næste skridt

| # | Trin | Målepunkt |
|---|---|---|
| 0 | Luk PR #23 | `state: closed` |
| 1 | »Afvis« og »Stop« som to handlinger | Stop sender `session/cancel` **før** `cancelled` |
| 2 | `clientCapabilities.fs` + behandler for læsning | Agenten kalder metoden, klienten svarer med `content` |
| 3 | Behandler for skrivning + kontrol af rodmængde | Fil oprettet; sti uden for roden afvist |
| 4 | `modes` læst og vist | tilstandene synlige i grænsefladen |

Trin 1 må ikke springes over: en agent, der må skrive filer, skal kunne standses.

`terminal/*` er overvejet og ikke besluttet. Det ville give agenten ret til at
køre kommandoer på telefonen og kræver egen sikkerhedspolitik.

## Dokumenter

| Fil | Indhold |
|---|---|
| `AGENTS.md` | regler for Vibe: sufflier, måling, PR-form |
| `RELEASE.md` | udgivelse: version, tag, CI |
| `dokumenter/vejkort.md` | kontrakt- og måleplan |
| `dokumenter/vibe-acp/` | målelab: journaler, analyser, målescripts |
| `LICENSE` | GPL-3.0 |

Én kontrakt ad gangen er normativ for sit lag. En specifikation må ikke gengive
ACP; den henviser til ACP og beskriver kun de valg, ACP overlader til klienten.

## Licens

GNU General Public License v3.0 — se [LICENSE](LICENSE).
