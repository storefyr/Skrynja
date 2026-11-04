# AGENTS.md

Regler for AI-agenter, der arbejder i dette repositorium. Læs `LÆSMIG.md`
først; den beskriver, hvad programmet er.

## Projekt

Skrynja er en ACP-klient til Sailfish OS. Klienten starter en lokal agent som
underordnet proces og taler ACP med den over standard-ind og -ud.

Når et dokument og koden er uenige, gælder koden. Notér afvigelsen i PR'en.

## Hårde krav

1. **Kun Qt 5.6.** Sailfish OS leverer Qt 5.6. Ingen API indført i 5.7+
   (`qAsConst`, `QRandomGenerator`, C++17-afhængige Qt-API'er). Er en API
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

## Byg og test

Enhedsprøver i `tests/` (QtTest, ingen Silica-afhængighed). De kører i CI på
almindelig Ubuntu-Qt og blokerer udgivelser. Ændres adfærd i `src/`, udvides
`tests/tst_main.cpp` tilsvarende.

```bash
cd tests && qmake tests.pro && make && ./tst_skrynja
```

Nye kildefiler skal i `harbour-skrynja.pro` (SOURCES/HEADERS), nye QML-filer i
`OTHER_FILES`/install. Nye synlige tekster skal gennem `qsTr()`.

Byg og CI køres på GitHub. Agenter, der arbejder i repoet, bygger ikke på
enheden og måler ikke på den.

## Sprog

Kode, kommentarer, commit-beskeder og tags er engelske. Projektdokumentation
er dansk, herunder `LÆSMIG.md`.

## Konventioner

- **Commits:** én linje, kort, konventionelt præfiks (`feat:`, `fix:`,
  `chore:`). Ingen emoji, ingen AI-tilskrivning.
- **Tags:** semver `vX.Y.Z`. Version hæves i `rpm/harbour-skrynja.spec` og
  `harbour-skrynja.yaml` ved udgivelse. Push af et tag udløser
  `.github/workflows/build-docker.yml`.
- **Stil:** 4 mellemrum i C++ og QML, Qt-navngivning (`m_`-præfiks, camelCase).
- **Kommentarer** kun, hvor koden ikke kan forklare sig selv.
- **Agentversionen** styres med `uv tool`, ikke af dette repositorium.
  `vibe-acp` er et værktøjssymlink i `~/.local/bin/`.
- **Enhedsscripts** (`test-pr.sh`, `update.sh`) henter fra
  `raw.githubusercontent.com` med fast filnavn. De må ikke kalde GitHub-API'et
  eller hente Actions-artefakter — begge er begrænsede eller kræver
  autentifikation.

## Arbejdsform

- Én PR, ét formål. En PR forgrenet fra en forældet `main` lukkes; den
  rebases ikke.
- Ingen påstand om et udfald uden en udskrift, der kan gengives.
- Ingen tomme commits for at udløse CI.
- Læs de relevante filer før redigering; gæt aldrig på et metodenavn —
  kontrollér det i headeren.
- Hold ændringen inden for opgaven. Omkringliggende kode refaktoreres ikke
  uden at det er bedt om.
- Fejler et byg eller en prøve, gengives den faktiske fejludskrift.
- Kontrakt- og måleplanen står i `dokumenter/vejkort.md`. Målinger føres i
  `dokumenter/vibe-acp/`.
