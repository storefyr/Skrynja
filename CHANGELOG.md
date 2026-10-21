# Changelog

All notable changes to Skrynja will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.5.3] - 2026-10-21
Skrynja 0.5.3 — Hotfix: rename ListModel roles to avoid QML name conflicts.

### Fixed
- **Remote Conversations page**: ListModel roles `id`, `name`, `model` collided
  with QML reserved names/context (`id` attribute, `model` context property).
  Roles are now named `remoteId`, `remoteName`, `remoteModel`,
  `remoteCreatedAt`, `remoteUpdatedAt` and accessed directly (no `modelData.`
  or `model.` prefix). In a QML `ListModel` delegate, roles are exposed as
  direct properties of the delegate context.

## [0.5.2] - 2026-10-21
Skrynja 0.5.2 — Hotfix: fix delegate model role access in RemoteConversationsPage.

### Fixed
- **Remote Conversations page**: delegate used `model.id` / `model.name` which
  are not defined for a `ListModel` delegate (only `modelData` or named roles
  work). Rows showed empty text and `onClicked` received an empty id, so
  tapping did nothing. Now uses explicit `property` bindings to `modelData`.

## [0.5.1] - 2026-10-21
Skrynja 0.5.1 — Hotfix: match the live Conversations API response shapes.

### Fixed
- **Remote conversations list**: the API returns a bare JSON array, not
  `{"data": [...]}`. Both shapes are now accepted.
- **Remote messages**: entries use the typed shape (`type: "message.input"` /
  `"message.output"`) with content directly on the entry, not nested under
  `outputs`. The parser now reads the correct fields and captures the model
  name on assistant messages.
- Added HTTP status and response body logging on fetch failures for diagnosis.

## [0.5.0] - 2026-10-21
Skrynja 0.5.0 — New feature: attach remote conversations from Mistral's servers.

### Added
- **Attach remote conversation (F17).** A new page lists the conversations
  stored on Mistral's servers for the configured API key
  (`GET /v1/conversations`). Tapping one fetches its full message history
  (`GET /v1/conversations/{id}/messages`) and creates a local conversation
  that keeps the remote id, so the next message appends to the existing
  remote conversation. Sources from web_search tool executions are preserved.
- Accessible from the conversation history pull-down menu:
  "Attach remote conversation".
- Already-attached remote conversations are marked and cannot be attached
  twice.

## [0.4.6] - 2026-10-21
Skrynja 0.4.6 — Cleanup: removed debug logging now that streaming works.

### Removed
- **Debug logging** from `MistralAPI` (POST URL, request body, raw SSE chunks,
  SSE lines, parsed events, HTTP status, error bodies, network errors).
  The SSE parsing fixes in v0.4.5 resolved the missing-answers issue.

## [0.4.5] - 2026-10-21
Skrynja 0.4.5 — Hotfix: robust SSE parsing (no-space colons, data-only fallback, ThinkChunk list).

### Fixed
- **SSE field parsing** now follows the spec: the space after the colon is
  optional, so `data:{...}` and `event:foo` parse correctly (was: only
  `data: {...}` with a space).
- **Data-only SSE fallback**: when the server sends `data:` lines without an
  `event:` line (like Chat Completions), the payload self-describes via its
  `type` field and is dispatched accordingly.
- **ThinkChunk.thinking** is a list of text/reference chunks, not a plain
  string. The reasoning text is now extracted from the nested structure
  (was: `.toString()` on an array returned an empty string, losing all
  thinking output).

### Added
- Debug logging from v0.4.4 retained for diagnosis.

## [0.4.4] - 2026-10-21
Skrynja 0.4.4 — Debug build: added SSE/network logging to diagnose missing answers.

### Added
- **Debug logging** in `MistralAPI`: logs the POST URL and request body, raw SSE
  chunks, each SSE line, parsed event names, HTTP status on finish, and network
  errors. View with `journalctl -f` or run from terminal. To be removed or
  reduced once the streaming issue is resolved.

## [0.4.3] - 2026-10-21
Skrynja 0.4.3 — Hotfix: SSE event names did not match the current API (no answers arrived).

### Fixed
- **SSE event names updated to match the live API.** The Conversations API renamed
  its streaming events since the original implementation:
  - `conversation.created` → `conversation.response.started` (carries `conversation_id`)
  - `tool.execution.finished` → `tool.execution.done`
  - Added handler for `conversation.response.error`
  The old names were silently ignored, so the app never received the conversation
  id (no append path) and never received text deltas (no answer shown). The token
  counter worked because `conversation.response.done` was already correct.

## [0.4.2] - 2026-10-21
Skrynja 0.4.2 — Hotfix: web search toggle was never saved to disk.

### Fixed
- **Web search toggle persistence.** `SettingsManager::saveSettings()` was missing
  the `generation/webSearch` key, so the toggle reset to off on every launch.
  The Settings UI wrote to the in-memory property but the value was never
  persisted. Conversations created without `tools: [{"type": "web_search"}]`
  have no web search — the model correctly reports it has no internet access.

## [0.4.1] - 2026-10-21
Skrynja 0.4.1 — Hotfix: Conversations API streaming URL was wrong (404 on every send).

### Fixed
- **Streaming endpoint path.** Mistral's docs write `/v1/conversations#stream` as endpoint
  notation, but `#stream` is not a URL fragment — it is their way of naming the streaming
  variant of the same path. The wire path is plain `POST /v1/conversations` with
  `"stream": true` in the JSON body (plus `Accept: text/event-stream`). The old code sent
  requests to `/v1/conversations%23stream`, which the server answered with 404 Not Found.
  `turnPath()` now returns plain paths and `buildTurnBody()` always sets `"stream": true`.

## [0.4.0] - 2026-10-21
Skrynja 0.4.0 — SailJail sandboxing (mandatory since Sailfish OS 4.4.0).

### Changed (breaking)
- **SailJail application profile.** The desktop file now uses the `[X-Sailjail]`
  section format (`Permissions=Internet;Pictures;Documents`,
  `OrganizationName=org.skrynja`, `ApplicationName=Skrynja`) instead of the
  old flat `X-Sailjail-Permissions=` key. Sandboxing is mandatory for Harbour
  apps since Sailfish OS 4.4.0.
- **Organization name changed** from `harbour-skrynja` to `org.skrynja` to
  match the desktop file. This moves the data directory to
  `~/.local/share/org.skrynja/Skrynja/` and the config directory to
  `~/.config/org.skrynja/Skrynja/`. Existing local conversations and settings
  under the old paths are not migrated — start fresh or copy the files
  manually.
- **QSettings uses explicit sandboxed paths.** Both `SettingsManager` and
  `ConversationManager` now construct `QSettings` with a path inside
  `QStandardPaths::AppConfigLocation` (the sandboxed config dir) instead of
  the two-argument constructor, which would write to an inaccessible path
  outside the sandbox.
- **`QDir::homePath()` fallbacks removed.** `conversationsDir()`,
  `retainImage()` and `SecureStore::saltFilePath()` no longer fall back to
  `~/.local/share/harbour-skrynja` — under SailJail that path is outside the
  sandbox and unwritable. `QStandardPaths::AppDataLocation` always returns
  the sandboxed path.

## [0.3.0] - 2026-10-21
Skrynja 0.3.0 — Vision: images uploaded once to Mistral's Files API.

### Added
- **Vision images are uploaded once, server-side (F14).** Attached images are
  uploaded to Mistral's Files API (`POST /v1/files`, multipart) before the turn
  is sent. The turn references the returned file id with a
  `{"type":"file","file_id":"..."}` chunk instead of carrying a base64 data URL,
  so the request body stays small no matter how large the image was. The file id
  is persisted on the user message (`imageFileId` field) and survives reloads;
  a retry or a later turn references the same server-side file. If the upload
  fails the turn falls back to an inline base64 `image_url` chunk so the user
  still gets an answer. The chat shows an "Uploading image…" indicator while the
  upload runs.

## [0.2.0] - 2026-10-21
Skrynja 0.2.0 — Phase 0 feature batch: web search sources, Mistral-only,
model fixed per conversation, reasoning model support.

### Added
- **Web search source chips.** `web_search` results are collected from
  `tool.execution.finished` events, deduplicated, and shown as tappable
  chips under the answer in `MessageBubble`. Persisted in the conversation
  JSON (`sources` field on each message).
- **Reasoning model support.** `type: "thinking"` chunks are accumulated
  and shown collapsed under the answer ("Show reasoning" to expand).
  Persisted in the conversation JSON (`thinking` field).

### Changed (breaking)
- **Mistral AI only.** The multi-provider registry (`Providers::all()`,
  `byId()`, `isValid()`, `defaultId()`, `Provider` struct) is gone. Only
  Mistral serves `/v1/conversations`. Per-provider keys, base URLs and
  model caches removed from `SettingsManager`.
- **Model fixed per conversation.** `nextMessageModel` and the per-message
  model override are removed. The model is chosen at creation
  (`createNewConversation(model)`) and locked server-side. The ChatPage
  quick model switcher and `ModelSelector.qml` are gone.

### Removed
- `ProviderBadge.qml`, `ProvidersUi.js`, `ModelSelector.qml` deleted.
- `SettingsManager::nextMessageModel` and its QSettings key removed.
- `SettingsManager::contextMessageLimit` and its Settings UI (server-side
  history; nothing to trim locally).

### Fixed
- `SettingsManager::migrateToSingleProvider()` no longer deletes the
  migrated `apiKeyEnc`/`modelName`/`models` keys it just wrote.
- `StatsPage.qml`: restored the `Column` wrapper around the per-model
  answers section.

## [Unreleased]

### Changed- **Reasoning models show their thinking.** `type: "thinking"` chunks  are accumulated during streaming and attached to the assistant message  as a `thinking` field, rendered collapsed under the answer in  `MessageBubble` (tap "Show reasoning" to expand). Persisted in the  conversation JSON alongside `sources`.- **Web search sources are shown and persisted.** `tool.execution.finished`  events are parsed for `web_search` result URLs, deduplicated, attached to  the assistant message and rendered as tappable source chips in  `MessageBubble`. Sources survive save/reload via a new `sources` field on  each message.- **Settings: "Web search" toggle replaces "Limit conversation context".** The  context limit is gone (server-side history); web search is a create-time  option, so the description says so.
- **System prompt description** now says "when a conversation is created" instead  of "before every conversation".

### Removed- `SettingsManager::contextMessageLimit` and its Settings UI. The Conversations  API keeps the history server-side; there is nothing to trim locally.- **Per-message and per-conversation model override.** `nextMessageModel`,  the ChatPage quick model switcher, `ModelSelector.qml` and the model  ComboBox in `ConversationSettingsPage` are gone. The model is chosen  when a conversation is created (`createNewConversation(model)`) and  fixed for its life — the Conversations API locks it server-side.  `SettingsManager::nextMessageModel` and its QSettings key are removed.- **Multi-provider registry.** `Providers::all()/byId()/isValid()/defaultId()`  and the `Provider` struct are gone; `providers.cpp` now only holds the  Mistral constants and the catalogue filters (`isChatModelId`,  `looksLikeVisionModel`). Per-provider keys, base URLs and model caches are  removed from `SettingsManager` and the Settings UI — the Conversations API  exists only on Mistral.- `ProviderBadge.qml` and `ProvidersUi.js` deleted; `MessageBubble` and  `StatsPage` show the model name only.

### Fixed- `ChatPage.qml`: repaired brace balance broken during the Conversations API  rewrite (extra `}` after the attached-image preview closed the input Column  early, leaving the rest of the page outside the root item).- `SettingsManager::migrateToSingleProvider()` no longer deletes the migrated  `apiKeyEnc`/`modelName`/`models` keys it just wrote, so a SailCat upgrade  keeps its configured key and model.- `StatsPage.qml`: restored the `Column` wrapper around the per-model answers  section; removing the provider column had left the page brace-imbalanced.

### Tests- Unit tests rewritten for the Conversations API: typed SSE events  (`message.output.delta`, `conversation.response.done`, `conversation.created`,  `tool.execution.started`, `error`), create-vs-append request bodies, image chunk  encoding, reconcile check (`remoteHasUserMessage`), title output parsing, and  `%23stream` URL encoding.
- `remoteConversationId` round-trip test: the id reported by  `conversation.created` is persisted on the conversation and survives a reload.
- `pendingUserContent` / `pendingUserImagePath` accessor tests.
- `buildTurnBodyWithImage` skips gracefully on hosts without a JPEG image plugin.


## [0.1.0] - 2026-10-20

Skrynja is a fork of [SailCat](https://github.com/nicosouv/harbour-sailcat),
rewritten around Mistral's Conversations API. This is the first release.

### Changed (breaking)

- **Rewritten to use the Conversations API** (`/v1/conversations`) instead of
  Chat Completions (`/v1/chat/completions`). The API is stateful: the server
  remembers the conversation, so the client only appends new messages.
- **Web search is now possible.** Mistral's `web_search` tool only exists on the
  Conversations API. A toggle in Settings (and per conversation) enables it; the
  chat shows a "Searching the web…" indicator while the model searches and lists
  sources with the answer.
- **Mistral AI only.** Scaleway, OVHcloud, Groq and custom endpoints speak the
  OpenAI dialect and do not support `/v1/conversations`, so the provider picker
  is gone.
- **Context limit trimming removed.** With server-side history there is nothing
  to trim locally; the server keeps the full conversation.
- **The model is fixed per conversation.** It is chosen when the conversation is
  created; start a new conversation to switch models.
- **Edit and regenerate removed.** They require entry-level restart, which is
  deferred to a later release.
- **Migration: existing conversations start fresh server-side.** Old messages
  stay visible locally, but the remote conversation only begins with the next
  message sent.

### Added

- Remote conversation tracking: each local conversation stores the
  `conversation_id` returned by Mistral.
- Reconciliation before retry: after an ambiguous failure (timeout, network
  drop) the client fetches `GET /v1/conversations/{id}/messages` and compares
  before sending again, so a message is never appended twice.
- Cancelled answers are marked dirty; the next message restarts the
  conversation server-side from before the cancelled turn
  (`POST /v1/conversations/{id}/restart#stream`).
- Deleting a conversation also deletes it server-side
  (`DELETE /v1/conversations/{id}`).
- Privacy notice: conversation history lives on Mistral's servers with this API.

## Previous history

Skrynja inherits the history of SailCat. The entries below are from the SailCat
changelog and describe versions released under the SailCat name.

## [2.3.0] - 2026-09-13

### Added

- **Skrynja is no longer tied to Mistral.** Every backend it can talk to speaks the
  same OpenAI chat-completions dialect, so the API layer now takes its endpoint from
  the selected provider instead of hardcoding `api.mistral.ai`. Presets ship for
  Mistral AI, Scaleway, OVHcloud AI Endpoints and Groq, plus a custom entry that takes
  any base URL - including a llama.cpp or Ollama server on the local network.
- Three of the four presets have a free tier, and the two French ones (Scaleway,
  OVHcloud) keep conversations in the EU. OVHcloud answers without an API key at a
  lower rate limit, so the app is usable before signing up anywhere.
- Each provider keeps its own API key, its own default model and its own model cache.
  Switching provider swaps the whole set; a key is never sent to another host.
- **A conversation is pinned to the provider it started with.** Changing the provider
  in the settings decides what new conversations use; it does not silently redirect
  an ongoing one. Each conversation can be moved to another provider from its own
  settings page, which spells out that the rest of it goes elsewhere and that the
  answers already received do not change.
- The chat header names who is answering (`Groq - llama-3.3-70b-versatile`), the model
  picker lists that provider's catalogue, and the history flags a conversation whose
  provider is not the selected one.
- **Every answer records the provider and model that produced it** and shows them
  under the message, so a conversation that changed provider halfway stays readable.
  Answers written before 2.3 are stamped with Mistral - the only provider that
  existed then - and show no model, which is genuinely not recoverable.

- **Statistics know who answers.** A "Providers and models" section reports the most
  used provider and the most used model, with a breakdown of answers per provider and
  per model. Counted from the answers themselves rather than from the token ledger,
  which carries no provider - so answers received before 2.3 count towards Mistral,
  under their provider only since their model was never recorded.
- The conversation history shows the provider and the model that answered last on each
  conversation.
- **Automatic titles are a setting now** (`Settings > Generation > Automatic titles`).
  Naming a conversation used to cost one request to the model after the first answer -
  the only call the app ever made on its own, and on a free tier it competes with the
  next question. The default works it out on the phone instead: the first question
  becomes the title, and the local classifier that already backs "Auto-label
  conversations" picks the category. "Ask the model" restores the old behaviour, "Off"
  does neither. "Suggest a title" in a conversation's settings still asks the model.
- Each conversation in the history carries a coloured provider badge, which also shows
  the streaming pulse and the unread light.

### Changed

- **The history rows had run out of room.** They grow with their content now, the date
  is relative ("14:32", "Yesterday", "12/08"), the relative-size bar is gone - the
  message count already said it - and the provider is a badge rather than a fourth
  line of text.
- The cost estimate reports "free" for a model served by a free-tier provider instead
  of pricing it from the Mistral list, and the model picker hides the image
  attachment button unless the catalogue says the model can read images.
- A per-conversation model override is checked against the active provider before
  every request: a Mistral model id is not sent to Groq.

### Fixed

- **`429 Too Many Requests` no longer loses the answer.** Mistral's free tier allows
  about one request per second; the app sent whatever it had whenever it had it and
  reported the rejection as a raw transfer error. Requests are now spaced out by
  1.2 s - whether they carry a question, a title or a model catalogue - and a
  rejection (429, 502, 503, 529) is retried up to three times, waiting as long as the
  provider asks or 2 s, 4 s, 8 s otherwise. While it waits, the chat says so with a
  countdown instead of showing an error, and the question is not lost. A custom
  endpoint is exempt: nobody rate-limits their own llama.cpp.
- When the attempts do run out, the error says what happened instead of quoting the
  transfer layer.

## [2.2.1] - 2026-08-19

### Fixed

- **An answer could land in the wrong conversation.** `ConversationModel` is a single
  reused object, so opening another conversation while a response was streaming made
  the rest of the answer appear in - and be saved into - the conversation that was
  opened next. The stream is now tied to the conversation that asked: it keeps
  buffering in the background and is written back where it belongs, and coming back
  to it mid-response shows everything received so far.
- Token usage and the unread flag are charged to the conversation that asked, not to
  whatever happens to be on screen when the response ends.

### Changed

- "Conversation History" removed from the chat pulley: swiping back does it.
- "Settings & About" added to the history pulley, which is the root page and had no
  other way in.

## [2.2.0] - 2026-08-19

### Changed

- **Two-sided navigation from a conversation.** The history is now the root page
  with the chat on top of it: swipe back for the history, swipe forward for the
  settings of the current conversation. The app still opens directly on the chat.
- **Pulley menus split by what they act on** instead of carrying the same seven
  entries twice. Top: history, pinned messages, prompt library, settings. Bottom:
  conversation settings, export, new conversation. The history pulley drops the two
  entries that duplicated the chat's.
- **Streaming moved into C++.** `ConversationManager` now owns the accumulation and
  the throttling, so an answer keeps landing in the conversation when the chat page
  is popped - which is exactly what happens when you swipe back to the history
  mid-response. Token accounting moved with it.
- Conversation settings is a page rather than a dialog: it follows the current
  conversation and writes changes when you leave it, only for fields you touched.

### Added

- **Unread indicator.** A conversation receiving an answer shows a pulsing dot in the
  history; once the answer has landed it stays lit until you open the conversation.
  The flag is persisted, so it survives a restart.

## [2.1.1] - 2026-08-19

### Fixed

- **Conversation history was unusable in 2.1.0.** `getConversationsList()` returned a
  key named `model`, and a ListModel role called `model` shadows the delegate's own
  model object: every `model.title`, `model.id` and `model.category` lookup silently
  became `undefined`. Titles all read "Empty conversation", category chips vanished,
  and opening, renaming, exporting or sharing a conversation from the history did
  nothing. No data was lost - titles and categories were intact on disk the whole
  time, only the list could not read them.

### Added

- **Suggest a title** in the conversation settings: asks the model for a title and a
  category based on the whole conversation, not just the first message. The proposal
  fills the form and is only stored when you confirm.
- Automatic titling now reads a digest of the exchange instead of the first message
  alone, which gives noticeably better titles.

### Removed

- "Export" and "Share" from the conversation context menu in the history. Exporting
  stays available from the chat pulley menu.

## [2.1.0] - 2026-08-19

### Security

- API key is no longer written in clear text. It is scrambled with a per-install
  random salt and both the settings file and the conversation files are set to
  owner-only permissions. Existing keys are migrated automatically on first launch.
- Exported conversations are written with owner-only permissions.
- All API and update requests now require TLS 1.2 or later, verify the peer and
  refuse redirects, so a bearer token can never follow a redirect elsewhere.
- Markdown links are restricted to `http`, `https` and `mailto`; a `javascript:`
  URL in a model reply can no longer reach `Qt.openUrlExternally`.
- Quotes are escaped when rendering messages, closing an attribute injection in
  generated `<a href>` tags.
- The update checker only opens URLs on `github.com`.
- Conversation file names are validated, so an id can never escape the storage
  directory.
- The API key field is masked by default, with an explicit reveal switch.

### Added

- **Conversation context limit**: send only the most recent N messages, with an
  on-screen notice of how many were left out. Keeps long conversations affordable
  and avoids hitting the model context window.
- **Prompt library**: save, edit and reuse prompts, insertable into the input field.
- **Per-conversation model and system prompt**, plus title and category, from a new
  conversation settings page.
- **Retry button** on the error banner: resend without retyping.
- **Chat styles**: flat, bubbles, compact and cards, with an option to hide timestamps.
- **28 conversation categories** instead of 7 (code, debugging, devops, data, design,
  writing, translation, learning, research, maths, science, business, finance, career,
  legal, health, cooking, travel, home, gaming, music, books & movies, sports,
  relationships, productivity, ideas, practical, other), a local keyword classifier that
  takes over when the model answers "other", and an "Auto-label conversations" action
  that relabels existing ones.
- **Estimated cost** per model in the statistics page, from a hand-maintained price list.
- **Rename a conversation** from the history page.
- **Share a conversation** through the Sailfish share menu, when available.
- Live answer preview on the cover page.
- **Norwegian bokmål** translation (thanks @fsilye), selectable from the settings.

### Changed

- Conversations are stored as one JSON file per conversation under the app data
  directory instead of a single blob in QSettings. Writes now touch only the
  conversation that changed. Existing data is migrated automatically.
- Streaming updates are coalesced at ~11 fps instead of repainting the whole message
  on every token, which removes the stutter on long answers.
- Attached images are copied into the app data directory and stay in the request
  payload on later turns, so follow-up questions about a picture still see it.
- Images and the per-conversation model are now honoured when regenerating a response.
- The "Use my own API key" switch is gone: there is no bundled key, so it only ever
  got in the way.
- Title generation is billed and now counted in the token statistics.
- All seven translations now carry the same set of strings, with the obsolete ones
  removed and the gaps where de/es/fi/it had been left in English filled in.

### Removed

- In-app update check. It was already unreachable from the UI in 2.0.4, and the store
  handles updates; the whole `UpdateChecker` class is gone. The settings page still
  shows the running version.

### Fixed

- A message sent just before the app was closed is no longer lost: the conversation
  is saved before the request and whenever the app leaves the foreground.
- The token banner keeps the running total when reopening an existing conversation
  instead of showing zero.
- A rejected request no longer leaves an empty assistant bubble behind, which used to
  be persisted and then sent back to the API.
- The conversation title is no longer regenerated after editing a message back down
  to two messages.
- Removed the dead `ConversationListPage.qml` and stale entries in the translation files.

## [2.0.4] - 2026-07-04

### Added

- QtTest unit suite covering the backend classes, wired as a CI gate on pull
  requests and on release builds.

### Changed

- README updated for the 2.x feature set.

## [1.0.0] - 2025-11-10

### Added

#### Core Features
- Complete Mistral AI integration with streaming support
- Native Sailfish OS UI using Silica components
- Real-time conversation with Server-Sent Events (SSE)
- Support for Mistral free tier and custom API keys
- Multiple model selection (Small, Large, Pixtral)

#### Backend (Qt C++)
- **MistralAPI** class for HTTP communication
  - POST requests to `/v1/chat/completions`
  - SSE streaming parser for progressive responses
  - Error handling and network management
  - Request cancellation support
- **ConversationModel** (QAbstractListModel)
  - Message history management
  - Real-time update during streaming
  - JSON conversion for API requests
- **SettingsManager**
  - Persistent storage with QSettings
  - API key management
  - Model selection persistence

#### Frontend (QML/Silica)
- **ChatPage**
  - Message list with SilicaListView
  - Text input with send button
  - Streaming response display
  - Loading indicators
  - Error messages
  - Pulley menu (Settings, About, New Conversation)
- **SettingsPage**
  - API key configuration dialog
  - Model selection ComboBox
  - Custom key toggle switch
  - Key deletion with remorse
- **AboutPage**
  - Application information
  - Feature list
  - Credits and license
  - GitHub link
- **CoverPage**
  - Active cover with message count
  - Quick action to clear conversation

#### Infrastructure
- GitHub Actions CI/CD
  - Multi-architecture builds (armv7hl, aarch64, i486)
  - Automated RPM releases on tags
  - PR validation builds
- Comprehensive documentation
  - README.md with usage guide
  - ARCHITECTURE.md with technical details
  - DEVELOPMENT.md for contributors
  - CONTRIBUTING.md with guidelines
- RPM packaging configuration
  - Spec file for all architectures
  - Desktop file for launcher
  - YAML build configuration

### Technical Details

#### API Integration
- Endpoint: `https://api.mistral.ai/v1/chat/completions`
- Supported models:
  - mistral-small-latest (default)
  - mistral-large-latest
  - pixtral-12b-latest
- Authentication: Bearer token
- Streaming: SSE with real-time parsing

#### Platform Support
- Sailfish OS 3.0+
- Qt 5.6
- Architectures: armv7hl, aarch64, i486

#### Security
- Local API key storage only
- HTTPS-only communication
- No telemetry or tracking
- No third-party services

### Known Limitations

- No offline mode (requires internet)
- No conversation persistence (lost on app close)
- No image upload support (Pixtral model text-only for now)
- Free tier rate limits may be restrictive

## Future Plans

### [1.1.0] - Planned

- Persistent conversation storage
- Multiple conversation threads
- Export conversations (text, markdown)
- Dark theme option

### [1.2.0] - Planned

- Image upload support for Pixtral
- Voice input integration
- Advanced API parameters (temperature, max_tokens)

### [2.0.0] - Ideas

- Mistral Agents API support
- Local conversation search
- Conversation sharing
- Multiple language support
- Custom system prompts

---

[Unreleased]: https://github.com/nicosouv/harbour-skrynja/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/nicosouv/harbour-skrynja/releases/tag/v1.0.0
