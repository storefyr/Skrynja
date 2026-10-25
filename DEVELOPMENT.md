# Skrynja Development Guide

Guide complet pour développer et builder Skrynja localement.

## Prérequis

### Option 1 : Sailfish SDK (Recommandé pour le développement)

Téléchargez et installez le [Sailfish SDK](https://sailfishos.org/develop/):

```bash
# Linux
sudo zypper install SailfishOS-latest-Installer.run
./SailfishOS-latest-Installer.run

# Windows/Mac
# Télécharger l'installateur depuis sailfishos.org
```

### Option 2 : Docker (Rapide pour builder)

Utilisez le container Platform SDK :

```bash
docker pull coderus/sailfishos-platform-sdk:5.0.0.43
```

### Option 3 : GitHub Actions (Sans setup local)

Poussez sur GitHub et laissez les Actions builder pour vous !

## Configuration du projet

### 1. Cloner le repo

```bash
git clone https://github.com/storefyr/Skrynja.git
cd harbour-skrynja
```

### 2. Structure du projet

```
harbour-skrynja/
├── src/                    # C++ backend
│   ├── mistralapi.*        # API Mistral (HTTP + SSE)
│   ├── conversationmodel.* # Modèle de données
│   └── settingsmanager.*   # Gestion des settings
├── qml/                    # QML frontend
│   ├── pages/              # Pages Silica
│   │   ├── ChatPage.qml
│   │   ├── SettingsPage.qml
│   │   └── AboutPage.qml
│   └── cover/              # Active cover
│       └── CoverPage.qml
├── rpm/                    # Packaging
│   └── harbour-skrynja.spec
├── harbour-skrynja.pro     # QMake project
├── harbour-skrynja.yaml    # Build config
└── harbour-skrynja.desktop # Desktop entry
```

## Building

### Avec Sailfish SDK

#### 1. Ouvrir dans Qt Creator

```bash
# Lancer Qt Creator du SDK
~/SailfishOS/bin/qtcreator

# File > Open File or Project
# Sélectionner harbour-skrynja.pro
```

#### 2. Configurer les kits

Dans Qt Creator :
- Build > Configure Project
- Sélectionner le kit Sailfish (aarch64 — Jolla Phone 2026)

#### 3. Builder

```
Build > Build All
```

Ou en ligne de commande :

```bash
# Utiliser sfdk (Sailfish SDK)
sfdk config target=SailfishOS-5.0.0.43-aarch64
sfdk build
```

#### 4. Déployer sur l'émulateur

```bash
sfdk emulator start
sfdk deploy --manual
```

#### 5. Déployer sur un appareil

```bash
# Via USB avec SSH
sfdk device set <device-ip>
sfdk deploy
```

### Avec Docker/mb2

```bash
# Entrer dans le container
docker run --rm -it \
  -v $(pwd):/home/sailfish/src \
  -w /home/sailfish/src \
  coderus/sailfishos-platform-sdk:5.0.0.43 \
  bash

# Builder
mb2 -t SailfishOS-5.0.0.43-aarch64 build

# Le RPM est dans RPMS/
```

### Avec GitHub Actions

Le plus simple ! Poussez un tag et laissez CI/CD faire le travail :

```bash
# Créer un tag avec versioning sémantique
git tag v1.0.0
git push origin v1.0.0

# Les workflows vont :
# 1. Builder pour aarch64 (Jolla Phone 2026)
# 2. Générer un changelog depuis les commits
# 3. Créer une release GitHub avec le RPM aarch64
# 4. Publier automatiquement
```

Les RPM seront disponibles dans [Releases](https://github.com/storefyr/Skrynja/releases) quelques minutes après.

**Note:** Le tag DOIT être au format `vX.Y.Z` (avec `v` en préfixe) pour déclencher le build.

Voir [RELEASE.md](RELEASE.md) pour le guide complet de release.

## Développement

### Workflow typique

1. **Modifier le code** (C++ ou QML)

2. **Builder** avec `sfdk build`

3. **Déployer** sur l'émulateur
   ```bash
   sfdk emulator start
   sfdk deploy --manual
   ```

4. **Tester** l'application

5. **Itérer** !

### Debugging

#### Logs Qt

```bash
# Sur l'appareil/émulateur
devel-su
journalctl -f | grep skrynja
```

#### Qt Creator Debugger

- Run > Start Debugging (F5)
- Breakpoints fonctionnent dans C++
- Console QML pour les erreurs QML

#### Console QML

Ajouter dans le code QML :
```qml
Component.onCompleted: console.log("Debug:", variable)
```

### Hot Reload QML

QML peut être modifié sans recompiler le C++ :

```bash
# Transférer seulement les QML
scp qml/pages/ChatPage.qml nemo@<device-ip>:/usr/share/harbour-skrynja/qml/pages/

# Redémarrer l'app
```

## Tests

### Tests manuels

1. **Première utilisation**
   - Lancer l'app
   - Vérifier le message de bienvenue
   - Aller dans Settings
   - Entrer une clé API valide

2. **Conversation basique**
   - Envoyer un message simple
   - Vérifier le streaming
   - Vérifier l'affichage

3. **Features avancées**
   - Tester la nouvelle conversation (pulley menu)
   - Tester le changement de modèle
   - Tester la cover active
   - Tester la gestion d'erreurs (clé invalide)

### Tests API

Tester les requêtes avec curl (noter le `%23` à la place de `#`, sinon le fragment n'est pas envoyé) :

```bash
curl -X POST 'https://api.mistral.ai/v1/conversations%23stream' \
  -H "Authorization: Bearer YOUR_KEY" \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -d '{
    "model": "mistral-small-latest",
    "tools": [{"type": "web_search"}],
    "inputs": "Test"
  }'
```

## Architecture

### Backend C++

**MistralAPI** - Communication avec l'API
- `sendMessage()` : Envoie une requête POST
- `onReadyRead()` : Traite les chunks SSE
- `parseStreamLine()` : Parse le JSON des events
- Signaux : `streamingResponse()`, `responseCompleted()`

**ConversationModel** - QAbstractListModel
- `addUserMessage()` : Ajouter un message utilisateur
- `addAssistantMessage()` : Ajouter une réponse
- `updateLastAssistantMessage()` : Update pendant streaming
- `toJsonArray()` : Convertir pour l'API

**SettingsManager** - QSettings wrapper
- `apiKey` : Stockage de la clé
- `modelName` : Modèle sélectionné
- `useCustomKey` : Mode free tier vs custom

### Frontend QML

**ChatPage** - Interface principale
- `SilicaListView` avec `conversationModel`
- `TextField` + `IconButton` pour l'input
- Gestion du streaming avec `Connections`

**SettingsPage** - Dialog de configuration
- `TextField` pour API key
- `ComboBox` pour le modèle
- Validation avec `canAccept`

### Flux de données

```
User Input → ChatPage.sendMessage()
  → conversationModel.addUserMessage()
  → mistralApi.sendMessage()
  → [HTTP POST] → Mistral API
  → [SSE Stream] → mistralApi.onReadyRead()
  → emit streamingResponse(content)
  → ChatPage.onStreamingResponse
  → conversationModel.updateLastAssistantMessage()
  → ListView.update()
```

## Problèmes courants

### Erreur de build "sailfishapp.h not found"

```bash
# Vérifier que le kit Sailfish est bien configuré
sfdk tools list
sfdk config target=SailfishOS-5.0.0.43-aarch64
```

### RPM non généré

```bash
# Vérifier le .spec et .yaml
cat rpm/harbour-skrynja.spec
mb2 -t SailfishOS-5.0.0.43-aarch64 build --verbose
```

### Émulateur ne démarre pas

```bash
# Recréer l'émulateur
sfdk emulator stop
sfdk emulator start
```

### Erreurs QML au runtime

```bash
# Checker la syntaxe
qmlscene qml/harbour-skrynja.qml

# Voir les logs
journalctl -f | grep qml
```

## Ressources

### Documentation

- [Sailfish SDK Docs](https://docs.sailfishos.org/)
- [Qt 5.6 Reference](https://doc.qt.io/qt-5.6/)
- [Silica Components](https://sailfishos.org/develop/docs/silica/)
- [Mistral API Docs](https://docs.mistral.ai/api)

### Exemples

- [Sailfish App Examples](https://github.com/sailfishos/sailfish-components-webview)
- [Qt Network Examples](https://doc.qt.io/qt-5.6/qtnetwork-examples.html)

### Communauté

- [Sailfish Forum](https://forum.sailfishos.org/)
- [Jolla Together](https://together.jolla.com/)
- [Reddit r/SailfishOS](https://reddit.com/r/SailfishOS)

## Tips & Tricks

### Compilation rapide

```bash
# Builder seulement pour une archi
sfdk config target=SailfishOS-5.0.0.43-aarch64
sfdk build

# Pas de clean
sfdk build --no-fix-version
```

### Debugging distant

```bash
# Sur l'appareil
gdbserver :2345 /usr/bin/harbour-skrynja

# Sur le PC
sfdk gdb
target remote <device-ip>:2345
```

### Profiling

```bash
# Avec perf (sur l'appareil)
devel-su
perf record -g harbour-skrynja
perf report
```

## Contributing

Voir [CONTRIBUTING.md](CONTRIBUTING.md) pour les guidelines de contribution.

## License

GPL-3.0-only - Voir [LICENSE](LICENSE)

---

Happy coding! 🐱⛵
