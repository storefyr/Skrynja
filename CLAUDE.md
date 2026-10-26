# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Skrynja is a native Sailfish OS chat client for **Mistral AI only**, built on the
**Conversations API** (`/v1/conversations`) — not Chat Completions. The Conversations API is
stateful (the server remembers the history) and is the only Mistral API that offers the
`web_search` tool, which is the point of the fork. Qt/C++ backend, QML/Silica frontend,
streaming SSE (Server-Sent Events) to display real-time AI responses.

Forked from SailCat and fully rewritten to the Conversations API. See
`ROADMAP.md` at the repository root for the current plan.

## Build Commands

### Local Development (with Sailfish SDK)

```bash
# Configure target architecture (aarch64 — Jolla Phone 2026)
sfdk config target=SailfishOS-5.0.0.43-aarch64

# Build the project
sfdk build

# Deploy to emulator
sfdk emulator start
sfdk deploy --manual

# Deploy to device
sfdk device set <device-ip>
sfdk deploy
```

### Docker Build (without SDK)

```bash
# Build using Docker container
docker run --rm -it \
  -v $(pwd):/home/sailfish/src \
  -w /home/sailfish/src \
  coderus/sailfishos-platform-sdk:5.0.0.43 \
  mb2 -t SailfishOS-5.0.0.43-aarch64 build

# RPMs are generated in RPMS/ directory
```

### Release Process

```bash
# Create and push a semver tag to trigger automated builds
git tag v1.0.0
git push origin v1.0.0

# GitHub Actions builds for aarch64 (Jolla Phone 2026) and creates release
# For manual/test builds, use workflow_dispatch (builds with version 0.1.0-dev)
```

## Architecture

### Backend (Qt/C++)

Four QObject classes are exposed to QML as context properties, plus two helper
namespaces:

1. **MistralAPI** (`src/mistralapi.*`)
   - Manages HTTP communication with Mistral's Conversations API (`https://api.mistral.ai/v1/conversations`).
     Mistral AI is the only provider; there is no endpoint switching
   - Endpoints: `POST /v1/conversations#stream` (create: model + instructions + tools are set here),
     `POST /v1/conversations/{id}#stream` (append: inputs + completion_args only),
     `POST /v1/conversations/{id}/restart#stream` (after a cancelled answer; returns a NEW id),
     `GET /v1/conversations/{id}/messages` (reconcile before retrying an ambiguous failure),
     `DELETE /v1/conversations/{id}`, `GET /v1/models`
   - Parses the typed SSE events (`event: message.output.delta`, `tool.execution.*`,
     `conversation.response.done` with usage, `conversation.created` with the id) — there is no `[DONE]` marker
   - Enforces TLS 1.2+, peer verification and no redirects on every request
   - Spaces requests out (1.2 s, every kind of call) and resends a `429`/`502`/`503`/`529`
     up to three times, honouring `Retry-After`. Before resending an append after an
     ambiguous failure, reconcile with `GET .../messages` so a message is never appended twice
   - Signals: `streamingResponse()`, `responseCompleted()`, `messageSent()`,
     `usageReceived()`, `sideRequestUsage()`, `titleGenerated()`, `modelsFetched()`,
     `rateLimited()`, `conversationCreated()`, `toolActivity()`, `sourcesReceived()`
   - Properties: `isBusy`, `error`

2. **ConversationModel** (`src/conversationmodel.*`)
   - QAbstractListModel for displaying messages in ListView
   - Stores messages with role ("user"/"assistant"), content, timestamp, pinned,
     imagePath, and - on answers - the model and provider that produced them
   - Key methods:
     - `addUserMessage()` - Add new user message
     - `addAssistantMessage()` - Start new assistant response
     - `updateLastAssistantMessage()` - Update during streaming
     - `messages()` - Read-only view used by ConversationManager
   - Roles: `RoleRole`, `ContentRole`, `TimestampRole`, `PinnedRole`, `ImagePathRole`,
     `ModelRole` (exposed as `messageModel`, never `model`), `ProviderRole`

3. **ConversationManager** (`src/conversationmanager.*`)
   - Owns the conversation list and the current ConversationModel
   - Storage: one JSON file per conversation under `AppDataLocation/conversations/`,
     written atomically, owner-only, with dirty tracking so a save touches one file.
     Each conversation also stores its remote `conversation_id` (empty until the first
     message has been answered server-side) and a `dirty` flag set when an answer was
     cancelled mid-stream. Migrates the pre-2.1 single QSettings blob on first launch
   - The request payload is no longer rebuilt per turn: only the new user message is
     sent as `inputs`; history lives server-side. The system prompt travels as
     `instructions` at conversation creation
   - `autoLabelConversation()` gives a conversation a title without a request,
     which is what `autoTitleMode` defaults to
   - Per-conversation overrides: `model` (fixed at creation), `systemPrompt`, title,
     web-search toggle. There is no provider override: Mistral AI only
   - Statistics, fun stats, search, markdown export, cost input (`modelUsage`)

4. **SettingsManager** (`src/settingsmanager.*`)
   - Wraps QSettings for persistent configuration, file kept owner-only
   - API key stored scrambled via SecureStore, never in clear text
   - Single Mistral key, default model and model cache (no `providers/<id>/` split
     anymore — one provider)
   - Properties: `apiKey`, `modelName`, `webSearch`, `language`, `temperature`,
     `maxTokens`, `systemPrompt`, `chatStyle`, `autoTitleMode`, `showTimestamps`,
     `savedPrompts`
   - `autoTitleMode` (`local` by default, `ai`, `off`) decides whether naming a
     conversation is worth a request. On a free tier it is the only call the app
     makes on its own
   - `modelPricing()` / `estimatedCost()` back the cost estimate in the stats page
   - `appVersion` exposes `APP_VERSION`, injected by qmake from the spec version

There is deliberately no in-app update check: the store handles updates, and the
GitHub Releases poll was removed in 2.1.0.

Helpers (plain namespaces, not QObjects):

- **SecureStore** (`src/securestore.*`) - owner-only file permissions and salted
  obfuscation for the API key.
- **Providers** (`src/providers.*`) - inherited from SailCat; currently still lists
  Mistral AI, Scaleway, OVHcloud, Groq and custom, but only the Mistral catalogue
  parsing is used. The other entries are removed as part of the API rewrite.
### Frontend (QML/Silica)

- **qml/pages/ChatPage.qml** - Main conversation interface. Owns the streaming
  throttle (deltas flushed on a 90 ms timer, never per token)
- **qml/pages/ConversationHistoryPage.qml** - List, search, share, auto-label
- **qml/pages/ConversationDetailPage.qml** - Read-only view with per-conversation charts
- **qml/pages/ConversationSettingsPage.qml** - Per-conversation title, model, prompt
- **qml/pages/PromptLibraryPage.qml** - Saved prompts
- **qml/pages/PinnedMessagesPage.qml** - Pinned messages across conversations
- **qml/pages/SettingsPage.qml** - API key, appearance, generation, update checker
- **qml/pages/StatsPage.qml** - Charts, badges, estimated cost
- **qml/cover/CoverPage.qml** - Active cover, follows the answer while streaming
- **qml/components/MessageBubble.qml** - Message delegate, implements the four chat
  styles (`flat`, `bubbles`, `compact`, `cards`) and the markdown renderer
- **qml/components/ProviderBadge.qml** - Coloured initial standing in for a provider
  logo, doubling as the streaming pulse and the unread light. Colors and glyphs live
  in `qml/components/ProvidersUi.js`, keyed by the ids in `src/providers.cpp`. No
  third-party logo ships in the package

### Data Flow

```
User sends message → ChatPage.sendMessage()
  → conversationModel.addUserMessage()
  → conversationManager.saveCurrentConversation()   // before the round trip
  → ChatPage.dispatchRequest()
  → mistralApi.sendMessage(apiKey, model, systemPrompt, userMessage, temperature,
                           maxTokens, webSearch, remoteConversationId)
  → emit messageSent() → conversationModel.addAssistantMessage("")
  → POST /v1/conversations%23stream        (first message: creates the conversation)
  → POST /v1/conversations/{id}%23stream   (later messages: appends)
  → SSE events → mistralApi.onReadyRead()
  → parseStreamLine() dispatches on the event name
  → conversationCreated(id) → persisted in the conversation JSON
  → toolActivity("web_search") → "Searching the web…" indicator
  → message.output.delta → emit streamingResponse(content)
  → conversation.response.done → emit usageReceived(...)
  → ConversationManager accumulates, marks streamPending
  → streamTimer (90 ms) → conversationModel.updateLastAssistantMessage()
  → ListView automatically updates
```

Invariants worth keeping:

- The empty assistant bubble is added on `messageSent()`, not before the call. A
  request the API layer rejects must not leave a placeholder behind.
- **Only the new user message goes on the wire.** History lives server-side; the
  client sends `inputs` (plus `instructions`/`tools`/`model` at creation). Retry and
  send all go through `dispatchRequest()`; edit/regenerate are removed for now.
- **Append is not idempotent.** After an ambiguous failure, `MistralAPI` reconciles
  with `GET /v1/conversations/{id}/messages` before resending. Never resend an append
  blindly.
- **A cancelled answer marks the conversation dirty.** The next message goes through
  `POST /v1/conversations/{id}/restart#stream` with `from_entry_id`; restart returns a
  NEW `conversation_id`, which replaces the stored one, and the old one is deleted.
- **No QML owns streaming state.** `ConversationManager::bindApi()` wires the API
  signals and accumulates the answer, because the chat page is destroyed whenever
  the user swipes back to the history. Anything that must survive that (accumulation,
  throttling, token accounting, the unread flag) belongs in C++.

### Page stack

```
ConversationHistoryPage   (root, initialPage)
  └─ ChatPage             (pushed immediately at startup)
       └─ ConversationSettingsPage   (attached, swipe forward)
```

Swiping back from the chat pops it; the history pushes a fresh one when a
conversation is opened. Never assume a `ChatPage` instance exists — look it up by
`objectName: "chatPage"` and handle null.

## Mistral Conversations API Integration

### Endpoints

```
POST   https://api.mistral.ai/v1/conversations%23stream        (create; %23 = literal '#')
POST   https://api.mistral.ai/v1/conversations/{id}%23stream   (append)
POST   https://api.mistral.ai/v1/conversations/{id}/restart%23stream
GET    https://api.mistral.ai/v1/conversations/{id}/messages  (reconcile)
DELETE https://api.mistral.ai/v1/conversations/{id}
GET    https://api.mistral.ai/v1/models
```

### Request Format (create)
```json
{
  "model": "mistral-small-latest",
  "instructions": "You are a helpful assistant.",
  "tools": [{"type": "web_search"}],
  "inputs": [{"object": "entry", "type": "message.input", "role": "user", "content": "Hello"}],
  "completion_args": {"temperature": 0.7, "max_tokens": 1024}
}
```

Append requests carry only `inputs` and `completion_args`.

### SSE Stream Format
```
event: conversation.created
data: {"conversation_id": "019b...", ...}

event: tool.execution.started
data: {"name": "web_search", "arguments": "{\"query\":\"...\"}"}

event: message.output.delta
data: {"content": "Hello"}

event: conversation.response.done
data: {"usage": {"prompt_tokens": 12, "completion_tokens": 34, ...}}
```

`MistralAPI::parseStreamLine()` dispatches on the `event:` name. For reasoning models
`content` in `message.output.delta` is a list of chunks (thinking + text), not a plain string.

### `#stream` is a URL fragment
`QUrl` treats `#stream` as a fragment and `QNetworkAccessManager` does not send
fragments in the request line. Percent-encode it (`%23stream`) — see the unit tests.

## Key Development Notes

### QML Hot Reload
QML files can be updated without rebuilding C++:
```bash
scp qml/pages/ChatPage.qml nemo@<device-ip>:/usr/share/harbour-skrynja/qml/pages/
# Then restart the app
```

### Debugging
```bash
# View app logs on device/emulator
devel-su
journalctl -f | grep skrynja
```

### CI/CD Workflows
- **build-docker.yml** - Triggered on `v*.*.*` tags or manual dispatch
  - Extracts version from `GITHUB_REF` (e.g., `refs/tags/v1.0.0` → `1.0.0`)
  - Uses `0.1.0` for manual builds without tags (RPM forbids `-` in Version)
  - Updates `rpm/harbour-skrynja.spec` with extracted version
  - Builds for aarch64 only (Jolla Phone 2026)
  - Creates GitHub release with changelog (tag builds only)
- **pr-build.yml** - Validates PRs with aarch64 build

### Important Constraints

1. **Qt 5.6** - Sailfish OS uses Qt 5.6, avoid newer Qt features
2. **Harbour Rules** - Must use `harbour-` prefix, only allowed dependencies
3. **Conversation storage format** - One JSON file per conversation under
   `AppDataLocation/conversations/`. Never reintroduce a single-blob write: it made
   every token-usage update rewrite the whole history.
4. **Streaming Only** - The app relies on SSE streaming; non-streaming mode not implemented
   (title generation is the one exception and uses a plain request)
5. **API key handling** - Never log it, never write it in clear text, never widen the
   permissions of the settings file. One Mistral key; it is only ever sent to
   `api.mistral.ai`. See `src/securestore.*`.
6. **Never expose a QML role or list key named `model`** - it shadows the delegate's own
   model object and turns every other lookup into `undefined`. The message model exposes
   `messageModel`; the conversation list has no such key at all.

### File Structure
```
src/                     # C++ backend classes and helpers
qml/pages/              # Main UI pages
qml/components/         # Reusable delegates
qml/cover/              # Active cover
tests/                  # QtTest suite, gates CI
rpm/harbour-skrynja.spec # RPM packaging spec
harbour-skrynja.pro     # QMake project file
harbour-skrynja.yaml    # Sailfish build config
harbour-skrynja.desktop # Desktop launcher entry
```

## Testing

Manual testing checklist from DEVELOPMENT.md:
1. First launch - verify welcome, enter API key in Settings
2. Send message - verify streaming display works
3. New conversation via pulley menu
4. Model switching in Settings
5. Cover page actions
6. Error handling with invalid API key

Test API directly:
```bash
curl -X POST 'https://api.mistral.ai/v1/conversations%23stream' \
  -H "Authorization: Bearer YOUR_KEY" \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -d '{"model":"mistral-small-latest","tools":[{"type":"web_search"}],"inputs":"Test"}'
```
- memory message de commit, tag, commentaaire, code, tout doit etre en anglais
- memory Sailfish utilise Qt 5.6
- memory quand on fait des modifications, fait le plus possible de version patch