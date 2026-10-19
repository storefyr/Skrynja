# Skrynja — Mistral AI chat for Sailfish OS

En simpel chat-klient til Mistral AI for Sailfish OS. Skriv et spørgsmål, få et svar.

## Funktioner

- Chat med Mistral AI (`api.mistral.ai`)
- Vælg mellem Mistrals chat-modeller
- API-nøgle gemmes lokalt på enheden (QSettings, organization `Harbour`, application `skrynja`)
- Følger systemets lyst/mørke tema automatisk via Sailfish Silica

## Forudsætninger

- Sailfish OS (Qt 5.6, Sailfish Silica)
- Mistral AI API-nøgle fra [console.mistral.ai](https://console.mistral.ai)

## Bygning

Byg med Sailfish OS SDK:

```bash
mkdir build && cd build
qmake ..
make
```

Installér på enhed eller emulator:

```bash
make install  # eller deploy via SDK/Qt Creator
```

## Brug

1. Åbn appen
2. Træk ned og vælg «Indstillinger», eller åbn indstillinger fra app-menuen
3. Indsæt din Mistral API-nøgle (starter med `mistral-`) og vælg en model
4. Skriv dit spørgsmål og tryk «Send»

## Projektstruktur

```
harbour-skrynja/
├── harbour-skrynja.pro        # qmake projektfil (sailfishapp)
├── harbour-skrynja.desktop    # Desktop entry
├── harbour-skrynja.yaml       # RPM spec (yaml)
├── LICENSE                    # GPL-2.0
├── README.md
├── icon/                      # App-ikoner (86–256 px + SVG)
├── qml/
│   ├── main.qml               # ApplicationWindow
│   ├── CoverPage.qml          # Cover
│   └── pages/
│       ├── MainPage.qml       # Chat
│       └── SettingsDialog.qml # API-nøgle og model
└── src/
    ├── main.cpp               # Entry point (libsailfishapp)
    ├── mistralclient.h
    └── mistralclient.cpp      # Mistral API-klient (QtNetwork)
```

## Sikkerhed

API-nøglen gemmes i **klartekst** i QSettings (`~/.config/Harbour/skrynja`). Det er standardpraksis for Sailfish OS-apps, men nøglen ligger lokalt på din enhed. Brug en dedikeret nøgle, og slet den ved at fjerne filen eller nulstille indstillingerne i appen.

Al trafik går direkte til `https://api.mistral.ai` over HTTPS. Appen logger eller sender ikke data til andre.

## Licens

GPL-2.0 — se LICENSE.

## Kontakt

- [GitHub Issues](https://github.com/storefyr/Skrynja/issues)
