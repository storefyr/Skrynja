# AGENTS.md

Regler for AI-agenter, der arbejder i dette repositorium. Læs LÆSMIG.md
først; den beskriver, hvad programmet er.

Bindende dokumenter i dokumenter/: opsaetning.adoc (parter, enhed, orakel),
orakel.adoc (rangordning, prøve før kode), ikke-maal.adoc (forbud med
betingelser), vejkort.adoc (skiver S0-S7), specifikation.adoc (krav),
journal.adoc (kørsler).

## Rangordning

Når to kilder er uenige, gælder den højere i denne liste:

1. Wire-optagelsen fra enheden.
2. Specifikationsparagraffen, der henviser til optagelsen.
3. Prøven, der afprøver paragraffen.
4. Kilden.
5. Agentens egne prøver.

Koden er ikke orakel. Er koden og specifikationen uenige, er koden forkert,
og afvigelsen rettes i koden — ikke i specifikationen.

Prøven committes før den kildefil, den afprøver. Rækkefølgen måles med git
log, ikke med en udtalelse om, hvad der blev skrevet først.

## Hårde krav

1. **Kun Qt 5.6.** Sailfish OS leverer Qt 5.6. Ingen API indført i 5.7+
(`qAsConst`, `QRandomGenerator`, C++17-afhængige Qt-APIer). Er en API
usikker, slås den op i Qt 5.6-dokumentationen før brug.
2. **QML: QtQuick 2.0 + Sailfish.Silica 1.0.** Ingen QtQuick.Controls, ingen
ToolTip, ingen `additionalContent`, ingen Qt Labs-moduler.
3. **Harbour-regler.** Appnavnet beholder `harbour-`-præfikset. Kun tilladte
importer.
4. **JavaScript i QML er ES5.** Ingen pilefunktioner, intet `let`/`const` i
ældre kontekster, ingen skabelonstrenge.
5. **Kun aarch64.** Målenheden er Jolla Phone (2026). Der bygges og testes
ikke for armv7hl eller i486.
6. **Indespærring er ikke i kraft.** Programmet kører i dag uden
SailJail-profil. Omfanget skal måles, før det skrives som krav — det må
ikke antages, hverken til eller fra.

## Byg og prøve

Enhedsprøver i `tests/` (QtTest, ingen Silica-afhængighed). Ændres adfærd i
`src/`, udvides `tests/tst_main.cpp` tilsvarende.

Jolla Phone 2026 er den eneste mål- og verifikationsenhed. Et acceptkriterium,
der ikke kan afgøres på den, er ugyldigt og skal omskrives. Der findes ingen
udgivelseskanal, og et versionsnummer blokerer intet.

Nye kildefiler skal i `harbour-skrynja.pro` (SOURCES/HEADERS), nye QML-filer i
`OTHER_FILES`/install.

## Sprog

Kode, kommentarer, commit-beskeder og tags er engelske. Projektdokumentation
er dansk, herunder `LÆSMIG.md`.

Der er ingen sprogfiler. Brugerfladen er dansk og har én bruger, jf.
`ikke-maal.adoc` N4. Nye synlige tekster oversættes ikke og skal derfor ikke
gennem `qsTr()`.

## Konventioner

- **Commits:** én linje, kort, konventionelt præfiks (`feat:`, `fix:`,
`chore:`). Ingen emoji, ingen AI-tilskrivning.
- **Version:** internt. Hæves i `rpm/harbour-skrynja.spec` og
`harbour-skrynja.yaml` efter behov. Der er ingen udgivelseskanal, og et tag
udløser intet.
- **Stil:** 4 mellemrum i C++ og QML, Qt-navngivning (`m_`-præfiks,
camelCase).
- **Kommentarer** kun, hvor koden ikke kan forklare sig selv.
- **Agentversionen** styres med `uv tool`, ikke af dette repositorium.
`vibe-acp` er et værktøjssymlink i `~/.local/bin/`.

## Arbejdsform

- Én opgave, ét formål.
- Ingen påstand om et udfald uden en udskrift, der kan gengives.
- Ingen tomme commits.
- Læs de relevante filer før redigering; gæt aldrig på et metodenavn —
kontrollér det i headeren.
- Hold ændringen inden for opgaven. Omkringliggende kode refaktoreres ikke
uden at det er bedt om.
- Fejler et byg eller en prøve, gengives den faktiske fejludskrift.
- Kontrakt- og måleplanen står i `dokumenter/vejkort.adoc`. Kørsler føres i
`dokumenter/journal.adoc`.

## Agentens rolle

En grøn testsuite beviser ikke, at koden er rigtig. Prøver skrevet af samme
instans som kilden, og enige med den, er en selvbekræftelse.

Rækkefølgen håndhæves derfor mekanisk: prøvens commit er ældre end kildens,
målt med `git log`, og prøvens aftryk står i en logfil uden for repoet.
