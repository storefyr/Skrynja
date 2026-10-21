# Feature Roadmap

Each feature has a detailed spec in `docs/features/`. Implement them one at a time, in order
within a phase (later specs sometimes build on earlier ones — dependencies are listed in each
spec). Bump a patch version and tag a release after each feature or small batch.

Status legend: `todo` / `in progress` / `done`

## Phase 0 — Fork & Conversations API rewrite (Skrynja)

Skrynja is a fork of SailCat rewritten around Mistral's Conversations API
(`/v1/conversations`). The features below are the fork's own roadmap. The SailCat feature
specs in `docs/features/` remain as historical reference; the ones marked **removed** in the
"Removed in the fork" section are no longer implemented.

| # | Feature | Spec | Status |
|---|---------|------|--------|
| F1 | Rename to Skrynja (app name, harbour id, packaging, paths, docs, translations) | — | done (v0.1.0) |
| F2 | `mistralapi.cpp` rewrite: request builder for `/v1/conversations#stream` + `/v1/conversations/{id}#stream`, typed SSE parser (`message.output.delta`, `conversation.response.done`, `tool.execution.*`), `conversation_id` handling | — | done (unreleased) |
| F3 | Web search: `tools: [{"type": "web_search"}]` at creation, toggle in Settings, "Searching the web…" indicator, source chips in `MessageBubble` | — | todo |
| F4 | `conversationmanager.cpp`: persistent `remoteConversationId` + `dirty` fields, migration of the JSON format | — | done (unreleased) |
| F5 | Reconcile-before-retry: `GET /v1/conversations/{id}/messages` after an ambiguous failure | — | done (unreleased) |
| F6 | Cancel = dirty; next message via `POST /v1/conversations/{id}/restart#stream` (new id, delete old) | — | todo |
| F7 | Remote deletion: `DELETE /v1/conversations/{id}` on conversation delete / purge | — | todo |
| F8 | Remove multi-provider: `providers.cpp` trimmed to Mistral, provider picker and per-provider keys removed from Settings/UI | — | done |
| F9 | Remove context-limit trimming (`buildApiMessages`, `trimmedMessageCount`, trim indicator) | — | done (unreleased) |
| F10 | Model fixed per conversation; remove `nextMessageModel` and per-conversation model override | — | done |
| F11 | Remove edit & regenerate (entry-level restart deferred) | — | done (unreleased) |
| F12 | Privacy notice in UI + docs (history lives on Mistral's servers) | — | todo |
| F13 | Unit tests: `%23stream` URL building, SSE event parser, reconcile, dirty/restart | — | done (unreleased) |
| F14 | Vision via `MessageInputEntry` chunks (text + image_url), image stored once server-side | — | todo |
| F15 | Reasoning models: `content` arrives as a chunk list (thinking + text) — parse and render both | — | todo |

## Removed in the fork

| Feature | Reason |
|---|---|
| Multi-provider (Scaleway, OVHcloud, Groq, custom endpoint) | Only Mistral serves `/v1/conversations` |
| Context-limit trimming | Server-side history; nothing to trim locally |
| Per-message model override | Model is create-time only |
| Edit message & resend, regenerate | Needs entry-level restart; deferred |
| Replay migration of old conversations | Old conversations start fresh server-side |

## Historical roadmap (SailCat, reference only)

The phases below are the SailCat roadmap, kept for reference. Features already shipped in
SailCat are inherited by the fork; their specs live in `docs/features/`.

### Phase 1 — API quick wins (backend-focused, low risk)

| # | Feature | Spec | Status |
|---|---------|------|--------|
| 01 | Generation parameters (temperature, max_tokens) | [01-generation-parameters.md](features/01-generation-parameters.md) | done (v1.9.7) |
| 02 | System prompt / personas | [02-system-prompt.md](features/02-system-prompt.md) | done (v1.9.8) |
| 03 | Token usage tracking | [03-token-usage.md](features/03-token-usage.md) | done (v1.9.9) |
| 04 | Dynamic model list from API | [04-dynamic-model-list.md](features/04-dynamic-model-list.md) | done (v1.9.10) |

### Phase 2 — Chat UX

| # | Feature | Spec | Status |
|---|---------|------|--------|
| 05 | Regenerate last response | [05-regenerate-response.md](features/05-regenerate-response.md) | removed in fork |
| 06 | Edit user message & resend | [06-edit-message.md](features/06-edit-message.md) | removed in fork |
| 07 | Export conversation | [07-export-conversation.md](features/07-export-conversation.md) | done (v1.9.13) |
| 08 | Copy code block | [08-copy-code-block.md](features/08-copy-code-block.md) | done (v1.9.14) |

### Phase 3 — Vision

| # | Feature | Spec | Status |
|---|---------|------|--------|
| 09 | Image support for Pixtral | [09-pixtral-vision.md](features/09-pixtral-vision.md) | done (v2.0.0); payload moves to `MessageInputEntry` chunks in F14 |

### Later phases

See the full SailCat history in the git log and `CHANGELOG.md` ("Previous history"). The
multi-provider feature (`21-multi-provider.md`) and per-message model override are removed
in the fork.
