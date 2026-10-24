# AGENTS.md

Instructions for AI coding agents (Mistral Vibe, Claude Code, etc.) working on this repository.

## Project

Skrynja is a native Sailfish OS chat client for Mistral AI, built on the **Conversations API**
(`/v1/conversations`) — not Chat Completions. That API is stateful (the server remembers the
conversation) and is the only one that offers the `web_search` tool. Qt/C++ backend, QML/Silica
frontend, SSE streaming for real-time responses. Forked from SailCat (harbour-sailcat).

## Migration status

The codebase is inherited from SailCat and is being rewritten step by step.
As of v0.1.0 the rename (app name, packaging, paths, docs) is done, but
`src/mistralapi.cpp` still speaks Chat Completions (`/v1/chat/completions`) and
`src/providers.cpp` still lists the old providers. The sections below describe
the **target** architecture and the decisions already locked in; when a doc and
the code disagree, trust the code and note the gap. The API rewrite is tracked
in `docs/ROADMAP.md`.

Locked decisions (do not relitigate in code reviews):

- Mistral AI only; no other provider, no custom endpoint.
- Context-limit trimming removed (server keeps the full history).
- Model fixed per conversation (create-time only).
- Edit and regenerate removed for now (needs entry-level restart).
- Retry after an ambiguous failure reconciles via `GET .../messages` first.
- A cancelled answer marks the conversation dirty; the next message restarts it.
- Old local conversations start fresh server-side (no replay migration).
- Conversation history lives on Mistral's servers; this is documented to users.

## Hard Constraints

These are non-negotiable. Violating them produces code that does not compile or gets rejected by the Jolla Store:

1. **Qt 5.6 only.** Sailfish OS ships Qt 5.6. Do not use any API introduced in Qt 5.7+ (no `qAsConst`, no `QRandomGenerator`, no connection syntax requiring newer Qt, no C++17-dependent Qt APIs). When unsure whether an API exists in 5.6, check the Qt 5.6 docs before using it.
2. **QML: QtQuick 2.0 + Sailfish.Silica 1.0.** No QtQuick.Controls, no ToolTip, no `additionalContent`, no Qt Labs modules. Only Silica components. Past crashes were caused by exactly this (see git log).
3. **Harbour rules.** App name keeps the `harbour-` prefix. Only Harbour-allowed imports (Sailfish.Silica, Sailfish.Pickers, QtQuick, QtQuick.Layouts, Nemo.Configuration, Nemo.Notifications). `Sailfish.TransferEngine` and `Sailfish.Share` are NOT allowed.
4. **JavaScript in QML is ES5.** No arrow functions, no `let`/`const` in older QML contexts, no template literals.
5. **Conversations API streaming is a body parameter, not a URL fragment.** Mistral's docs write
   `/v1/conversations#stream` as endpoint notation, but the wire path is plain `/v1/conversations`
   with `"stream": true` in the JSON body (plus `Accept: text/event-stream`). Do NOT put `#stream`
   or `%23stream` in the URL — the server answers 404. The unit tests cover this.
6. **SailJail sandboxing is mandatory** (since Sailfish OS 4.4.0). The desktop file must have an
   `[X-Sailjail]` section with `Permissions=`, `OrganizationName=org.skrynja` and
   `ApplicationName=Skrynja`. `QStandardPaths::AppDataLocation` / `AppConfigLocation` always
   return the sandboxed paths — never fall back to `QDir::homePath()`. QSettings must be
   constructed with an explicit path inside `AppConfigLocation` (see `skrynjaSettingsPath()`),
   not the two-argument constructor.

## Architecture

- `src/mistralapi.{h,cpp}` — HTTP + SSE streaming to the Conversations API. Endpoints:
  `POST /v1/conversations` with `"stream": true` (create),
  `POST /v1/conversations/{id}` with `"stream": true` (append),
  `POST /v1/conversations/{id}/restart` with `"stream": true` (after a cancelled answer),
  `GET /v1/conversations/{id}/messages` (reconcile before retry),
  `DELETE /v1/conversations/{id}` (delete), `GET /v1/models` (catalogue).
  Signals: `streamingResponse(content)`, `responseCompleted()`, `messageSent()`,
  `titleGenerated(title)`, `conversationCreated(id)`, `toolActivity(name)`, `sourcesReceived(list)`.
- `src/conversationmodel.{h,cpp}` — QAbstractListModel of messages (`role`, `content`, `timestamp`, `pinned`, `imagePath`).
- `src/conversationmanager.{h,cpp}` — persistence (one JSON file per conversation under `AppDataLocation`),
  current conversation lifecycle, remote `conversation_id` tracking, dirty flag after a cancelled
  answer, statistics, search.
- `src/settingsmanager.{h,cpp}` — QSettings wrapper: `apiKey` (stored obfuscated), `modelName`,
  `webSearch` toggle, `language`, `chatStyle`, `savedPrompts`.
- `src/securestore.{h,cpp}` — owner-only file permissions and salted obfuscation for the API key.
- `src/categories.{h,cpp}` — conversation categories and the local keyword classifier. Mirrored for display in `qml/components/Categories.js`.
- `src/providers.{h,cpp}` — kept from SailCat for the model-catalogue parsing only; there is a single provider (Mistral AI) and no provider picker.
- `qml/pages/ChatPage.qml` — main chat UI. `qml/components/MessageBubble.qml` — message rendering with regex-based markdown and source chips. `qml/pages/SettingsPage.qml` — settings dialog.

All C++ classes are exposed to QML as context properties: `mistralApi`, `conversationModel`, `conversationManager`, `settingsManager` (registered in `src/harbour-skrynja.cpp`).

## Conversations API semantics (important)

- **Create-time only:** `model`, `instructions` (system prompt) and `tools` (web_search) are set when
  the conversation is created. Per-turn parameters go in `completion_args` (`temperature`, `max_tokens`).
- **Append is not idempotent.** If a request may have reached the server, reconcile with
  `GET /v1/conversations/{id}/messages` before sending again.
- **Restart returns a new `conversation_id`.** The old one must be deleted and the new one persisted.
- **SSE events are typed**, sent as `event: <name>` + `data: {...}`: `conversation.response.started`
  (carries `conversation_id`), `tool.execution.started/delta/done`, `message.output.delta`
  (text chunks), `conversation.response.done` (usage), `conversation.response.error`.
  There is no `[DONE]` marker.
- **Reasoning models** return `content` as a list of chunks (thinking + text), not a plain string.

## Translations

Only **English (en)** and **Danish (da)** are shipped. `translations/harbour-skrynja-<code>.ts`,
one per entry in `SettingsManager::availableLanguages()`. Adding a language means adding the
`.ts` file, the code in `availableLanguages()`, the `TRANSLATIONS` list in the `.pro`, and the
entry in the SettingsPage language ComboBox — the ComboBox order must match `availableLanguages()`.

## Build & Test

**Target device: Jolla Phone (2026) — aarch64 only.** CI builds a single RPM for aarch64. Do not build or test for armv7hl or i486.

```bash
# Docker build (no SDK needed) — this is the reliable way to verify compilation
docker run --rm -it -v $(pwd):/home/sailfish/src -w /home/sailfish/src \
  coderus/sailfishos-platform-sdk:5.0.0.43 \
  mb2 -t SailfishOS-5.0.0.43-aarch64 build
```

Unit tests live in `tests/` (QtTest, no Silica dependency — the C++ classes are pure Qt). They run in CI on plain Ubuntu Qt and block releases. Locally:

```bash
cd tests && qmake tests.pro && make && ./tst_skrynja
```

When changing `src/` behavior, extend `tests/tst_main.cpp` accordingly. QML is tested on device via PR artifacts: copy `test-pr.sh` to the phone and run `sh test-pr.sh <pr-number>` — it downloads and installs the latest aarch64 RPM from the PR's CI build. Quick QML iteration still works via hot-deploy: `scp qml/... nemo@<device-ip>:/usr/share/harbour-skrynja/qml/...` then restart the app.

New source files must be added to `harbour-skrynja.pro` (SOURCES/HEADERS) and new QML files to the `OTHER_FILES`/install sections. New user-visible strings must use `qsTr()`.

### Device scripts (run on the Jolla Phone)

| Script | Purpose |
|--------|---------|
| `test-pr.sh <N>` | Download and install the aarch64 RPM for PR #N from the `pr-<N>-rpm` branch |
| `update.sh [version]` | Download and install the latest (or given) release RPM |

Both scripts use only `curl` (preinstalled on Sailfish OS). They must NOT call the GitHub API — see the rule below.

### External API calls from device scripts — avoid

**GitHub API calls are rate-limited** (60 req/hour unauthenticated) and **Actions artifacts require authentication** to download. Do not use `api.github.com` or Actions artifact URLs in device scripts. Instead:

- CI pushes PR RPMs to a `pr-<N>-rpm` git branch with a **fixed filename** (`skrynja-pr<N>.rpm`).
- Device scripts download directly from `raw.githubusercontent.com/<repo>/<branch>/<fixed-name>` — no API, no auth, no rate limit.
- Prefer a known/fixed path over an API lookup when the filename can be controlled on the producer side.

## Conventions

- **Everything in English**: code, comments, commit messages, tags, docs.
- **Commits**: one-liner, very concise, conventional prefix (`feat:`, `fix:`, `chore:`). No emoji, no "Co-authored-by" or AI attribution lines.
- **Tags/releases**: semver `vX.Y.Z`, starting at `v0.1.0` for Skrynja (the SailCat 2.x history stays with SailCat). Pushing a tag triggers the release CI (`.github/workflows/build-docker.yml`), which builds aarch64 only and creates the GitHub release. Bump the version in `rpm/harbour-skrynja.spec` and `harbour-skrynja.yaml` when releasing.
- Match existing code style: 4-space indent in C++ and QML, Qt naming (`m_` member prefix, camelCase).

## Working Style

- Be concise. Answer the question first, then supporting detail. No filler.
- Read the relevant files before editing; never guess a method name or signature — verify it in the header.
- Keep changes minimal and scoped to the request. Do not refactor adjacent code unless asked.
- Comments only where the code cannot explain itself (protocol quirks, Qt 5.6 workarounds). No narrative comments.
- If a build or test fails, report the actual error output — do not claim success without verifying.
- Feature work is specified in `docs/features/`. When asked to implement a feature, read its spec file first and follow it; update `docs/ROADMAP.md` status when done.

## Mistral Conversations API Quick Reference

```
POST https://api.mistral.ai/v1/conversations
Authorization: Bearer <key>
Accept: text/event-stream
{"model": "...", "instructions": "...", "tools": [{"type": "web_search"}],
 "stream": true,
 "inputs": [{"object": "entry", "type": "message.input", "role": "user", "content": "..."}],
 "completion_args": {"temperature": 0.7}}

POST https://api.mistral.ai/v1/conversations/{id}          (append; "stream": true, inputs + completion_args only)
POST https://api.mistral.ai/v1/conversations/{id}/restart  ("stream": true; from_entry_id; returns a NEW id)
GET  https://api.mistral.ai/v1/conversations/{id}/messages   (reconcile)
DELETE https://api.mistral.ai/v1/conversations/{id}
```

SSE response: typed events, `event: <name>` followed by `data: {...}`. Text chunks arrive as
`event: message.output.delta` with `data.content`; usage arrives in `conversation.response.done`.
Parsing lives in `MistralAPI::parseStreamLine()`; UTF-8-safe buffering in `processStreamData()`.
