# Architecture de Skrynja

Ce document décrit l'architecture technique de Skrynja, une application de chat IA pour Sailfish OS,
branchée sur l'**API Conversations de Mistral** (`/v1/conversations`). Skrynja est un fork de
SailCat réécrit autour de cette API, qui est la seule à offrir l'outil `web_search`.

## Vue d'ensemble

Skrynja est une application native Sailfish OS construite avec Qt 5.6 et QML. Elle utilise l'API
Conversations de Mistral AI (`https://api.mistral.ai/v1/conversations`) avec support du streaming
(Server-Sent Events) pour offrir des conversations en temps réel, y compris la recherche web.

```
┌─────────────────────────────┐
│           Interface QML (Silica)             │
│  ┌─────────────┬─────────────┬─────────────┐ │
│  │ChatPage     │Settings     │  AboutPage  │ │
│  │             │Page         │             │ │
│  └──────┬──────┴──────┬──────┴──────┬──────┘ │
└─────────┼─────────────┼─────────────┼────────┘
          │ Qt/QML Context Properties
┌─────────┴─────────────┬─────────────┴────────┐
│         Backend Qt C++ (QtCore/QtNetwork)    │
│  ┌──────────┬─────────────────┬─────────────┐ │
│  │MistralAPI│Conversation     │  Settings   │ │
│  │          │Model            │  Manager    │ │
│  └────┬─────┴────────┬────────┴──────┬──────┘ │
└───────┼──────────────┼───────────────┼────────┘
          │ HTTPS/SSE
┌─────────┴──────────────┴───────────────┴────────┐
│      Mistral AI — API Conversations             │
│  /v1/conversations, /v1/models                  │
└─────────────────────────────────────────────────┘
```

## Différences fondamentales avec SailCat

| Sujet | SailCat (Chat Completions) | Skrynja (Conversations) |
|---|---|---|
| Endpoint | `POST /v1/chat/completions` | `POST /v1/conversations#stream`, `POST /v1/conversations/{id}#stream` |
| Historique | Construit et envoyé à chaque requête | Gardé côté serveur ; le client n'envoie que le nouveau message |
| Fournisseurs | Mistral, Scaleway, OVHcloud, Groq, custom | Mistral AI uniquement |
| Recherche web | Impossible | `tools: [{"type": "web_search"}]` à la création |
| Modèle | Modifiable à chaque message | Fixé à la création de la conversation |
| System prompt | Envoyé à chaque requête | `instructions`, fixé à la création |
| Limite de contexte | Coupée localement (`contextMessageLimit`) | Aucune (le serveur garde tout) |
| Édition / régénération | Troncature locale + renvoi | Supprimées (nécessiteraient un restart par entry) |
| Persistance distante | Aucune | Chaque conversation stocke son `conversation_id` |

## Composants principaux

### 1. Backend C++ (`src/`)

#### MistralAPI (`mistralapi.h/cpp`)

**Responsabilité**: Gestion de la communication avec l'API Conversations de Mistral.

**Endpoints utilisés**:
- `POST /v1/conversations#stream` — crée une conversation (modèle, `instructions`, `tools` fixés ici)
- `POST /v1/conversations/{id}#stream` — ajoute un message (`inputs` + `completion_args`)
- `POST /v1/conversations/{id}/restart#stream` — après une réponse annulée (retourne un NOUVEL id)
- `GET /v1/conversations/{id}/messages` — réconciliation avant un nouvel essai
- `DELETE /v1/conversations/{id}` — suppression distante
- `GET /v1/models` — catalogue des modèles
- `POST /v1/files` — upload multipart d'une image (vision) ; retourne un `file_id`

**Piège important**: `#stream` est un fragment d'URL. `QUrl` le traiterait comme tel et
`QNetworkAccessManager` ne l'envoie pas dans la requête. Il faut l'encoder (`%23stream`).

**Fonctionnalités**:
- Envoi de requêtes POST vers l'API Conversations
- Support du streaming via Server-Sent Events (SSE) avec événements typés (`event: <nom>`)
- Parsing des événements SSE en temps réel (`message.output.delta`, `tool.execution.*`,
  `conversation.response.done`)
- Gestion des erreurs réseau et API
- Annulation des requêtes en cours
- Réconciliation (`GET .../messages`) avant de renvoyer une requête ambiguë
- Upload d'images (F14) : `uploadImage()` envoie l'image à `POST /v1/files`
  (multipart, JPEG redimensionné à 1024px) et émet `imageUploaded(fileId)`.
  Le tour référence ensuite le fichier par `{"type":"file","file_id":"..."}`
  au lieu d'un data URL base64. En cas d'échec, repli sur un chunk
  `image_url` base64.

**Signaux Qt**:
- `streamingResponse(QString)` - Émis pour chaque chunk de texte reçu
- `responseCompleted()` - Émis quand la réponse est complète
- `messageSent()` - Émis quand la requête est acceptée
- `conversationCreated(QString id)` - Émis avec le `conversation_id` retourné à la création
- `toolActivity(QString name)` - Émis quand un outil (ex. `web_search`) démarre
- `sourcesReceived(QStringList)` - Émis avec les sources de la recherche web
- `imageUploaded(QString fileId)` - Émis quand l'upload d'une image réussit (F14)
- `imageUploadFailed()` - Émis quand l'upload d'une image échoue
- `errorChanged()` - Émis en cas d'erreur

**Propriétés Q_PROPERTY**:
- `isBusy` - Indique si une requête est en cours
- `error` - Message d'erreur actuel

#### ConversationModel (`conversationmodel.h/cpp`)

**Responsabilité**: Modèle de données pour l'historique des conversations.

**Type**: `QAbstractListModel` - Compatible avec QML ListView

**Structure de données**:
```cpp
struct Message {
    QString role;       // "user" ou "assistant"
    QString content;    // Contenu du message
    QString imagePath;  // Chemin local de l'image jointe (vision)
    QString imageFileId;// file_id distant (upload /v1/files, F14)
    qint64 timestamp;   // Horodatage Unix
};
```

**Rôles QML**:
- `role` - Rôle du message (user/assistant)
- `content` - Contenu textuel
- `timestamp` - Horodatage

**Méthodes Q_INVOKABLE**:
- `addUserMessage(QString)` - Ajouter un message utilisateur
- `addAssistantMessage(QString)` - Ajouter une réponse de l'assistant
- `updateLastAssistantMessage(QString)` - Mettre à jour pendant le streaming
- `clearConversation()` - Effacer l'historique

**Comportement de streaming**:
La méthode `updateLastAssistantMessage()` permet d'afficher progressivement la réponse de l'IA
en mettant à jour le dernier message au fur et à mesure que les chunks arrivent.

#### ConversationManager (`conversationmanager.h/cpp`)

**Responsabilité**: Gestion persistante des conversations, locales et distantes.

**Stockage**: Un fichier JSON par conversation sous `~/.local/share/harbour-skrynja/conversations/`

**Champs ajoutés par rapport à SailCat**:
- `remoteConversationId` — l'identifiant retourné par Mistral (vide tant que la conversation
  n'a pas été créée côté serveur)
- `dirty` — positionné quand une réponse a été annulée en cours de streaming ; le prochain
  message repart de zéro via `/restart#stream`
- `imageFileId` (par message) — le `file_id` retourné par `POST /v1/files` après l'upload
  d'une image (F14) ; persisté pour qu'un nouvel essai ou un tour suivant référence le
  même fichier côté serveur

**Méthodes Q_INVOKABLE** (inchangées pour la plupart):
- `createNewConversation(model)` - Crée une conversation avec un modèle fixé
  (verrouillé côté serveur par l'API Conversations); `conversationModel(id)` le relit
- `loadConversation()`, `deleteConversation()` (supprime aussi à distance)
- `saveCurrentConversation()`, `getConversationsList()`, `getConversationDetails()`
- `setRemoteConversationId()`, `remoteConversationId()` — suivi de l'id distant
- `markConversationDirty()` / `isConversationDirty()` — gestion des réponses annulées

#### Providers (`providers.h/cpp`)
**Responsabilité**: Constantes Mistral et filtres du catalogue de modèles.

Mistral AI est le seul fournisseur : l'API Conversations (`/v1/conversations`)
n'existe nulle part ailleurs. Ce qui reste de l'ancien registre multi-fournisseurs
est réduit à :

- `ID`, `NAME`, `BASE_URL`, `KEY_URL`, `REGION` — constantes de l'API Mistral
- `fallbackModels()` / `defaultModel()` — catalogue de secours utilisé avant
  le premier appel à `/v1/models`
- `isChatModelId()` — filtre les entrées du catalogue qui ne sont pas des
  modèles de chat (embeddings, speech, image, moderation)
- `looksLikeVisionModel()` — devine le support des images d'après le nom du
  modèle ; faux en cas de doute (le bouton pièce jointe reste caché)

#### SettingsManager (`settingsmanager.h/cpp`)

**Responsabilité**: Gestion persistante des paramètres utilisateur.

**Stockage**: `QSettings` (fichiers de configuration locaux)

**Propriétés Q_PROPERTY** (allégées par rapport à SailCat):
- `apiKey` - Clé API Mistral (unique, stockée brouillée)
- `modelName` - Modèle par défaut, choisi à la création de chaque conversation
  et verrouillé côté serveur pour toute sa durée de vie
- `webSearch` - Active la recherche web (outil `web_search`)
- `temperature`, `maxTokens` - Paramètres de génération (par requête, via `completion_args`)
- `systemPrompt` - Prompt système (envoyé comme `instructions` à la création)
- `language`, `chatStyle`, `autoTitleMode`, `showTimestamps`, `savedPrompts`

**Persistance**:
Les paramètres sont sauvegardés automatiquement à chaque modification dans:
```
~/.config/harbour-skrynja/Skrynja.conf
```

### 2. Frontend QML (`qml/`)

#### ChatPage.qml

**Responsabilité**: Interface principale de conversation.

**Composants Silica**:
- `SilicaFlickable` - Conteneur scrollable
- `PullDownMenu` - Menu pour paramètres/nouvelle conversation
- `SilicaListView` - Affichage des messages
- `TextField` + `IconButton` - Saisie de messages

**Gestion du streaming**:
```qml
property string streamingContent: ""

Connections {
    target: mistralApi
    onStreamingResponse: {
        streamingContent += content
        conversationModel.updateLastAssistantMessage(streamingContent)
    }
    onToolActivity: {
        // Affiche « Recherche sur le web… » pendant l'exécution de web_search
    }
}
```

**Comportement**:
1. L'utilisateur tape un message et appuie sur Envoyer
2. Le message est ajouté au `conversationModel`
3. Un message vide de l'assistant est créé
4. L'API est appelée: création si c'est le premier message, append sinon
5. Les chunks de réponse sont accumulés dans `streamingContent`
6. Le dernier message est mis à jour en temps réel
7. Quand la réponse est complète, `streamingContent` est réinitialisé

#### SettingsPage.qml

**Responsabilité**: Configuration de l'API et des préférences.

**Type**: `Dialog` - Permet d'accepter ou annuler les modifications

**Composants**:
- `TextSwitch` - Activer la recherche web
- `TextField` - Saisie de la clé API
- `ComboBox` - Sélection du modèle
- `Button` - Effacer la clé API

### 3. Point d'entrée (`src/harbour-skrynja.cpp`)

**Responsabilité**: Initialisation de l'application Qt/Sailfish.

**Context Properties**:
```cpp
context->setContextProperty("mistralApi", &mistralApi);
context->setContextProperty("conversationModel", &conversationModel);
context->setContextProperty("settingsManager", &settingsManager);
```

## Flux de données

### Envoi d'un message

```
User Input (QML TextField)
    ↓
ChatPage.sendMessage()
    ↓
conversationModel.addUserMessage()
    ↓
conversationModel.addAssistantMessage("") // Message vide
    ↓
mistralApi.sendMessage(apiKey, model, systemPrompt, message, webSearch, remoteId)
    ↓
Si remoteId est vide : POST /v1/conversations#stream   (création)
Sinon                : POST /v1/conversations/{id}#stream  (append)
    ↓
[Réseau] → Mistral Conversations API
```

### Réception de la réponse (Streaming)

```
[Réseau] ← Mistral API (SSE)
    ↓
QNetworkReply.readyRead()
    ↓
MistralAPI.onReadyRead()
    ↓
MistralAPI.processStreamData()
    ↓
MistralAPI.parseStreamLine()  — dispatch sur le nom de l'événement
    ↓
event: conversation.created    → emit conversationCreated(id)
event: tool.execution.*        → emit toolActivity(name)
event: message.output.delta    → emit streamingResponse(content)
event: conversation.response.done → emit usageReceived(...)
    ↓
[Signal Qt] → QML Connections
    ↓
streamingContent += content
    ↓
conversationModel.updateLastAssistantMessage()
    ↓
[Binding Qt] → ListView update
    ↓
UI mis à jour en temps réel
```

### Réponse annulée par l'utilisateur

```
mistralApi.cancelRequest()
    ↓
conversationManager.markConversationDirty(id)
    ↓
Prochain message :
POST /v1/conversations/{id}/restart#stream  {"from_entry_id": "<dernier bon entry>"}
    ↓
Nouveau conversation_id retourné → stocké, ancien supprimé (DELETE)
```

## Format de l'API (Conversations)

### Requête (création)

```http
POST https://api.mistral.ai/v1/conversations%23stream
Content-Type: application/json
Authorization: Bearer <API_KEY>
Accept: text/event-stream
```

```json
{
  "model": "mistral-small-latest",
  "instructions": "Tu es un assistant utile.",
  "tools": [{"type": "web_search"}],
  "inputs": [
    {"object": "entry", "type": "message.input", "role": "user", "content": "Bonjour"}
  ],
  "completion_args": {"temperature": 0.7}
}
```

### Requête (append)

```http
POST https://api.mistral.ai/v1/conversations/{id}%23stream
```

```json
{
  "inputs": [
    {"object": "entry", "type": "message.input", "role": "user", "content": "Quel temps fait-il ?"}
  ],
  "completion_args": {"temperature": 0.7}
}
```

### Réponse (Streaming SSE, événements typés)

```
event: conversation.created
data: {"conversation_id": "019b2bd7-...", ...}

event: tool.execution.started
data: {"name": "web_search", "arguments": "{\"query\":\"météo\"}"}

event: message.output.delta
data: {"content": "Il"}

event: message.output.delta
data: {"content": " fait"}

event: conversation.response.done
data: {"usage": {"prompt_tokens": 42, "completion_tokens": 128}}
```

Il n'y a **pas** de marqueur `[DONE]`. Pour les modèles de raisonnement, `content` est une
liste de chunks (pensée + texte), pas une chaîne simple.

## Gestion des erreurs

### Erreurs réseau
- Timeout de connexion
- Perte de connexion
- Serveur inaccessible

→ Affichées via `mistralApi.error` dans un `Label` rouge sur `ChatPage`

### Erreurs API
- Clé API invalide (401)
- Rate limit dépassé (429)
- Modèle invalide (400)
- Conversation inconnue (404) → recréer la conversation côté serveur

→ Parsing du JSON d'erreur et extraction du message détaillé

### Cas particulier : requête ambiguë
Si une requête a pu atteindre le serveur mais que la réponse est perdue (timeout, coupure
réseau), le client interroge `GET /v1/conversations/{id}/messages` et compare avant de
renvoyer, pour ne jamais ajouter le message deux fois.

### Erreurs utilisateur
- Clé API manquante
- Champ de message vide

→ Validation côté QML (boutons désactivés, redirections)

## Sécurité

### Stockage de la clé API

```cpp
QSettings m_settings("harbour-skrynja", "Skrynja");
m_settings.setValue("apiKey", m_apiKey);
```

- Stockage local uniquement (pas de cloud)
- Lecture/écriture protégées par permissions Linux
- Pas de transmission à des tiers
- Communication HTTPS uniquement avec Mistral

### Vie privée — différence importante avec SailCat

Avec l'API Conversations, **l'historique des conversations est stocké sur les serveurs de
Mistral**. C'est le prix de la mémoire côté serveur et de la recherche web. Cela est
communiqué clairement à l'utilisateur dans l'interface et la documentation.

### Bonnes pratiques

- ✅ Validation des entrées utilisateur
- ✅ Gestion des erreurs réseau
- ✅ Pas de hardcoding de secrets
- ✅ Utilisation de HTTPS
- ✅ Respect du rate limiting (1 requête/seconde sur le free tier)

## Build & Packaging

### Compilation

```bash
qmake5 harbour-skrynja.pro
make
```

### Packaging RPM

Le fichier `harbour-skrynja.spec` définit:
- Dépendances (`sailfishsilica-qt5`, Qt5 modules)
- Processus de build (`%qmake5`, `make`)
- Installation (`%qmake5_install`)
- Fichiers inclus dans le RPM

### Structure du RPM

```
/usr/bin/harbour-skrynja                      # Exécutable
/usr/share/harbour-skrynja/qml/               # Fichiers QML
/usr/share/applications/harbour-skrynja.desktop
/usr/share/icons/hicolor/*/apps/harbour-skrynja.png
```

## Performance

### Optimisations

- **Lazy loading**: ListView charge les items à la demande
- **Streaming**: Affichage progressif sans attendre la réponse complète
- **Modèle C++**: QAbstractListModel performant pour grandes listes
- **Bindings efficaces**: Mise à jour uniquement des items modifiés

### Considérations

- Le streaming réduit la latence perçue
- Le buffer SSE évite les pertes de données
- Les signaux Qt sont thread-safe
- QML est optimisé pour l'affichage en liste

## Extensions futures

### Édition et régénération de messages

Nécessiterait:
- Suivi des `entry_id` pour chaque message (stockés localement)
- `POST /v1/conversations/{id}/restart#stream` avec `from_entry_id`
- Gestion du fait que restart retourne un **nouvel** `conversation_id`

### Agents Mistral

L'API Agents offre:
- Outils configurables (web_search, code_interpreter, etc.)
- Instructions persistantes par agent
- Endpoint `/v1/agents` + conversations avec `agent_id`

## Références

- [Mistral Conversations API Docs](https://docs.mistral.ai/api/endpoint/beta/conversations)
- [Mistral Websearch Docs](https://docs.mistral.ai/studio/agents/agent-tools/websearch)
- [Sailfish SDK Docs](https://docs.sailfishos.org/)
- [Qt 5.6 Documentation](https://doc.qt.io/qt-5.6/)
- [Silica Components](https://sailfishos.org/develop/docs/silica/)
