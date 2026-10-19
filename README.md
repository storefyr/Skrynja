# Skrynja - Mistral AI Chat for Sailfish OS

En Sailfish OS applikation der gør det muligt at stille spørgsmål til Mistral AI og modtage svar direkte på din enhed.

## Funktioner

- Send spørgsmål til Mistral AI
- Modtag svar i realtid
- Chat historik
- Indstillinger for API nøgle og model
- Understøttelse af flere Mistral modeller
- **Mørkt tema (Dark Mode)** - Automatisk tilpasning til Sailfish OS 5's tema-indstillinger
- Optimeret til Jollaphone 2026's skærmstørrelse

## Forudsætninger

- Sailfish OS 5.x (testet på Jollaphone 2026)
- Qt 5
- Qt Network modul
- Internetforbindelse

## Installation

### 1. Klon dette repository

```bash
git clone https://github.com/dit-brugernavn/skrynja.git
cd skrynja
```

### 2. Opret bygningsmiljø

```bash
mkdir build
cd build
qmake ..
make
```

### 3. Installer applikationen

```bash
sudo make install
```

### 4. Kør applikationen

Applikationen vil være tilgængelig i din app menu som "Skrynja".

## Konfiguration

1. Åbn applikationen
2. Tryk på "Indstillinger"
3. Indtast din Mistral AI API nøgle (kan fås fra https://mistral.ai)
4. Vælg ønsket model (mistral-tiny, mistral-small, mistral-medium)
5. Gem indstillingerne

## Brug

1. Skriv dit spørgsmål i inputfeltet
2. Tryk på "Send" eller tryk Enter
3. Vent på svar fra Mistral AI
4. Chat historikken vises automatisk

## API Nøgle

Du kan få en API nøgle ved at:
1. Gå til https://mistral.ai
2. Opret en konto (hvis du ikke allerede har en)
3. Gå til API sektion
4. Opret en ny API nøgle
5. Kopier nøglen og indsæt den i applikationens indstillinger

## Understøttede Modeller

- `mistral-tiny` - Hurtig og effektiv (standard)
- `mistral-small` - God balance mellem hastighed og kvalitet
- `mistral-medium` - Højeste kvalitet

## Sailfish OS 5 Funktioner

- **Automatisk tema-detektion**: Applikationen tilpasser sig automatisk til systemets tema (lys/mørk)
- **Forbedret visuel design**: Optimeret farveskema for bedst mulig læsbarhed
- **Cover Action**: Hurtig adgang via Jollaphone 2026's cover flow
- **Responsive layout**: Tilpasser sig til Jollaphone 2026's skærmstørrelse (6,5")

## Fejlfinding

### "API nøgle er påkrævet"
- Sørg for at du har indtastet din API nøgle i indstillingerne
- Nøglen skal starte med "mistral-"

### "Ugyldigt svar fra server"
- Tjek din internetforbindelse
- Sørg for at din API nøgle er korrekt
- Prøv igen senere

### "Network error"
- Tjek din internetforbindelse
- Sørg for at din enhed har adgang til internettet

## Projektstruktur

```
skrynja/
├── skrynja.pro    # Qt projektfil
├── skrynja.desktop # Desktop entry
├── skrynja.yaml   # RPM spec fil
├── qml/
│   ├── main.qml               # Hoved QML fil
│   ├── CoverPage.qml          # Cover side
│   ├── pages/
│   │   └── MainPage.qml       # Hovedside med UI
│   └── qml.qrc                # Resource fil
├── src/
│   ├── main.cpp               # Hoved C++ fil
│   ├── mistralclient.h        # Header for Mistral client
│   └── mistralclient.cpp      # Implementation af Mistral client
├── icon/                      # App ikoner
│   ├── 86x86/
│   ├── 108x108/
│   ├── 128x128/
│   ├── 172x172/
│   └── 256x256/
└── README.md                  # Denne fil
```

## Bidrag

Bidrag er velkomne! Åbn venligst en issue eller pull request for eventuelle ændringer.

## Licens

Denne applikation er udgivet under **GNU General Public License version 2 (GPL-2.0)**. Se LICENSE filen for detaljer.

## Kontakt

For spørgsmål eller feedback, kontakt venligst:
- Email: support@mistral.ai
- Web: https://mistral.ai
