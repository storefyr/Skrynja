# Changelog

All notable changes to Skrynja will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.9.7] - 2026-10-22
Skrynja 0.9.7 — Voice anonymization is now ON BY DEFAULT, with a single fixed level.

### Changed
- **Anonymization is on by default.** The toggle in Settings → Voice input
  now starts enabled. Disable it only if you experience transcription
  problems. A stored value (including an explicit "off") is respected.
- **The strength selector is gone.** After on-device testing converged on
  one level, the subtle/medium/strong presets and the ComboBox were
  removed. Every anonymized recording uses the same fixed pipeline:
  pitch shift (~1.24) + spectral tilt (−0.55) + telephone-band limiting
  (300–3400 Hz) + noise (1.2%), jittered per recording.
- The UI description now says plainly what happens and that the feature
  is on by default.

### Removed
- `VoiceAnonymizer::Strength` enum, `strengthFromString()`,
  `anonymizeWithStrength()` and the `ringModulate()` DSP primitive (ring
  modulation broke transcription too often to ship; see v0.9.2–v0.9.6).
- `SettingsManager::voiceAnonymizationStrength` and
  `availableVoiceAnonymizationStrengths()`.
- Legacy settings keys `voice/pitchFactor` (0.9.1) and
  `voice/anonymizationStrength` (0.9.2–0.9.6) are removed on load.

### Tests
- Updated: default-on persistence, legacy-key cleanup, fixed-pipeline
  round-trip (length range), per-recording jitter, wrapper signature.

## [0.9.2 – 0.9.6] - 2026-10-22
Skrynja 0.9.2–0.9.6 — Experimental voice anonymization iteration (PR #8, test branch `vibe/f18b-strong-anonymization-b6c41e`).

These were test releases of the voice anonymization feature, superseded by 0.9.7
(single fixed level, on by default). Consolidated history:

- **0.9.2**: strength presets (subtle/medium/strong) + ring modulation added.
- **0.9.3**: fixed “strong” breaking speech-to-text — ring carrier at 180 Hz sat
  inside the voice fundamental range (85–255 Hz); carrier moved above it (320/350 Hz).
- **0.9.4**: retuned “strong” to max safe effort after a second failing test round.
- **0.9.5**: anti-voiceprint hardening — seeded RNG (was deterministic noise!),
  per-recording jitter (±3–5%), telephone-band limiting in “strong”.
- **0.9.6**: ring modulation removed from presets (kept breaking STT even above
  the voice range) — replaced by spectral tilt, which keeps harmonics intact.

## [0.9.1] - 2026-10-22
Skrynja 0.9.1 — Voice anonymization (F18b): anonymize your voice before it leaves the device.

**Was experimental — opt-in via Settings → Voice input → "Anonymize voice". On by default since 0.9.7.**

### Added
- **Voice anonymization (F18b)**: a new "Voice input" section
  in Settings with an "Anonymize voice" toggle and a "Voice pitch" slider
  (0.80–1.25, default 1.15). When enabled, the recording is processed
  **locally on the device** before it is sent to Mistral:
  - **Pitch shift** via resampling with linear interpolation — changes the
    fundamental frequency of the voice (higher or lower).
  - **Subtle background noise** (0.5% of full scale) — masks fine spectral
    details without hurting transcription accuracy.
  - On any processing failure the app falls back to the original recording:
    privacy must never block transcription.
- New `VoiceAnonymizer` C++ class (`src/voiceanonymizer.{h,cpp}`): minimal
  16-bit PCM WAV reader/writer + DSP helpers, exposed to QML as
  `voiceAnonymizer`.
- New settings `voiceAnonymization` (bool, default off) and
  `voicePitchFactor` (double, default 1.15, clamped to [0.8, 1.25]).
- 10 new unit tests (pitch no-op/shorten/lengthen, noise no-op/change,
  WAV round-trip, garbage/missing-file rejection, settings persistence,
  pitch clamping).

### Fixed (after first on-device test)
- **Anonymization never actually ran**: `ChatPage.qml` called
  `VoiceAnonymizer::anonymize(...)` with a `QString*` out-parameter.
  QML cannot pass `QString*` to C++ ("Unknown method parameter type:
  QString*"), so the call threw, `ok` was falsy, and the app silently
  fell back to the original recording — the user heard their normal
  voice. Added a QML-friendly wrapper `anonymizeFile()` that returns
  the error as a `QString` (empty on success). ChatPage now calls the
  wrapper and shows a notification if anonymization fails.

## [0.8.13] - 2026-10-22
Skrynja 0.8.13 — Fix: recorder never recorded — wrong codec + broken signal connects.

### Fixed
- **Wrong audio codec (F18, root cause of "Could not read the recording
  file")**: the recorder was configured with `setCodec("audio/x-wav")` —
  but `audio/x-wav` is a *container* mimetype, not a codec. On Sailfish's
  GStreamer backend this fails with "Not compatible codecs and container
  format", `record()` never enters `RecordingState`, and no file is ever
  written. Configuration is now codec `audio/PCM` + container `wav` +
  audio input `pulseaudio:`, matching the working pattern from
  mentaljam/sailfish-recorder. Supported codecs/containers/audio inputs
  are logged at startup for on-device diagnostics.
- **Broken signal connects on Qt 5.6 (F18)**: the string-based
  `SIGNAL(stateChanged(QAudioRecorder::State))` connects failed on
  Sailfish's Qt 5.6 with "No such signal" (moc registers the inherited
  `QMediaRecorder::State` signature), so `onStateChanged`/`onError` never
  ran on device. Replaced with compile-time-checked pointer-based
  connects (`error` disambiguated with a static_cast, as it is overloaded).
- `record()` log line now also prints the active codec and container.

## [0.8.12] - 2026-10-22
Skrynja 0.8.12 — Fix: transcription race condition — transcription never started.

### Fixed
- **Transcription race condition (F18)**: `chatPage.transcribing` was set to
  `true` *after* `globalAudioRecorder.stop()` was called. On the Sailfish
  GStreamer backend, `stateChanged(StoppedState)` can be emitted
  **synchronously** from inside `stop()`, so the `onStopped` handler ran
  while `transcribing` was still `false` and skipped the `transcribeAudio()`
  call entirely. The watchdog then fired after 120 s with "The transcription
  took too long" — even though no transcription request was ever sent. This
  explains why v0.8.11 (Voxtral 3 model) showed the exact same symptom on
  device. `transcribing` and the watchdog are now set **before** `stop()` is
  called.
- **Duplicate `stopped` signal (F18)**: `stopped` could be emitted twice (once
  from `onStateChanged(StoppedState)` and once from the stop-timeout guard),
  which could start the transcription twice. A `emitStoppedOnce()` guard now
  guarantees exactly one emission per recording session.
- **Invalid WAV rejection (F18)**: `transcribeAudio` now validates the WAV
  file (minimum 44-byte RIFF/WAVE header) before uploading. A truncated file
  — a known Sailfish `QAudioRecorder` failure mode — now fails fast with a
  clear message ("The recording could not be saved") instead of being sent
  to the API and failing obscurely.

### Added
- **Recorder diagnostics (F18)**: `AudioRecorder` now logs state, status,
  error string and output file size at record/stop/state-change/timeout.
  These appear in the app log (`journalctl` / `devel-su` console) and make
  on-device debugging of transcription issues possible without guessing.

## [0.8.11] - 2026-10-22
Skrynja 0.8.11 — Fix: voice transcription works for Danish speakers.

### Fixed
- **Transcription model**: switched from `voxtral-mini-latest` (which
  resolves to Voxtral Mini Transcribe 2, supporting only 13 languages —
  Danish is not one of them) to `voxtral-mini-transcribe-3-0` (Voxtral 3,
  launched 2026-10-20, 50+ languages including Danish). Danish speech
  previously failed or hung until the timeout.
- **HTTP timeout**: raised the transcription request timeout from 30 s to
  90 s. Transcription is a batch job; shorter clips finish in a few
  seconds, but longer recordings on a less common language can take longer.
- **QML watchdog**: raised `transcribingWatchdog` from 35 s to 120 s so it
  stays above the C++ timeout and acts only as a last-resort backstop.

### Added
- **Language hint**: the spoken language is now sent to the transcription
  API (`language` form field), derived from the app's interface language.
  This skips language detection and improves accuracy, especially for
  short utterances. `nb_NO` maps to `nb`.
- **Danish interface language**: `da` added to the language list, with a
  new `translations/harbour-skrynja-da.ts` (currently untranslated strings
  fall back to English).

## [0.8.10] - 2026-10-21
Skrynja 0.8.10 — Fix: QML watchdog guarantees "Transcribing..." never hangs.

### Fixed
- Added a 35-second QML `Timer` watchdog (`transcribingWatchdog`) that
  starts when the user taps stop. If neither the recorder's `stopped`
  signal nor the HTTP timeout fires within 35 seconds, the watchdog
  resets the UI and shows a "Transcription took too long" notification.
- `AudioRecorder::stop()` now also handles `PausedState` and emits
  `stopped` immediately if the recorder is not in `RecordingState`.
- The watchdog is stopped when transcription succeeds or fails normally.

## [0.8.9] - 2026-10-21
Skrynja 0.8.9 — Fix: timeouts prevent "Transcribing..." from hanging forever.

### Fixed
- Added a 30-second timeout (`m_transcriptionTimer`) for the transcription
  HTTP request. If the server does not respond in time, the request is
  aborted and `transcriptionFailed("Transcription timed out")` is emitted.
- Added a 5-second safety timer (`m_stopTimer`) in `AudioRecorder::stop()`.
  If `QAudioRecorder` never reaches `StoppedState` (e.g. PulseAudio backend
  hangs), the `stopped` signal is force-emitted so the UI does not stay
  in "Transcribing..." forever.

## [0.8.8] - 2026-10-21
Skrynja 0.8.8 — Fix: wait for file flush before transcribing.

### Fixed
- `QAudioRecorder::stop()` is asynchronous — the WAV file is not fully
  written to disk the instant `stop()` returns. Previously, transcription
  was triggered immediately after `stop()`, causing "Could not read the
  recording file" because the file was empty or incomplete.
- Now transcription is triggered from the `stopped` signal handler, which
  fires after the recorder has flushed and closed the file.

## [0.8.7] - 2026-10-21
Skrynja 0.8.7 — Improvement: descriptive transcription error messages.

### Changed
- `transcriptionFailed` signal now carries a `QString message` parameter
  with a human-readable error description.
- `onTranscriptionFinished()` parses the server's JSON error body
  (`message` or `detail` field) and maps common HTTP status codes to
  friendly messages: 401/403 → "Invalid API key", 429 → "Rate limit
  reached", 402 → "Payment required".
- The QML notification now displays the actual error message instead of
  a generic "Check your API key" text.
- Pre-flight errors (no API key, empty file, unreadable file) also emit
  descriptive messages.

## [0.8.6] - 2026-10-21
Skrynja 0.8.6 — Improvement: recording timer, error notifications, robustness.

### Added
- **Recording timer**: a live `M:SS` counter appears below the mic button
  while recording, showing elapsed time via `QAudioRecorder::duration()`.
- **Error notifications**: recording errors and transcription failures now
  show a Sailfish notification with a descriptive message, instead of only
  a silent `console.warn`.
- **Cache directory**: `tempAudioPath()` now ensures the cache directory
  exists (`QDir::mkpath`) before returning a path — important under SailJail
  where the sandbox directory may not have been created yet.
- `available` property on `AudioRecorder` is now `CONSTANT` (it does not
  change at runtime).

## [0.8.5] - 2026-10-21
Skrynja 0.8.5 — Fix: microphone button could not be tapped to stop recording.

### Fixed
- Removed `!chatPage.recording` from the mic button's `enabled` condition.
  Previously, when recording started the button became disabled, making it
  impossible to tap it again to stop. Now the button stays enabled while
  recording (only disabled during transcription or when the API is busy).

## [0.8.4] - 2026-10-21
Skrynja 0.8.4 — Fix: use correct Sailfish theme icon names for microphone.

### Fixed
- Changed microphone icon from `icon-m-microphone` (does not exist in the
  Sailfish theme) to `icon-m-mic` (idle) and `icon-m-mic-mute` (recording).
  These are the correct icon names per the official Sailfish Icon Reference.

## [0.8.3] - 2026-10-21
Skrynja 0.8.3 — Fix: replaced QML AudioRecorder with C++ QAudioRecorder.

### Fixed
- Removed `import QtMultimedia 5.0` from `ChatPage.qml` entirely. The QML
  plugin (`qt5-qtdeclarative-import-multimedia`) was the root cause of
  "Kunne ikke indlæse siden" — even when installed, it failed to load on
  the user's device.
- Created a new C++ class `AudioRecorder` (src/audiorecorder.h/.cpp) that
  wraps `QAudioRecorder` and is exposed to QML as a context property
  `globalAudioRecorder`. This only links against `libQt5Multimedia.so.5`
  (Harbour-allowed) and does not require the QML plugin package.
- Removed `qt5-qtdeclarative-import-multimedia` from Requires (no longer
  needed). Kept `qt5-qtmultimedia` and `qt5-qtmultimedia-plugin-audio-pulseaudio`.

## [0.8.2] - 2026-10-21
Skrynja 0.8.2 — Fix: added missing Qt Multimedia runtime dependencies.

### Fixed
- Added `qt5-qtmultimedia`, `qt5-qtdeclarative-import-multimedia`, and
  `qt5-qtmultimedia-plugin-audio-pulseaudio` to `Requires` in both the spec
  file and the yaml file. Without `qt5-qtdeclarative-import-multimedia`, the
  `import QtMultimedia 5.0` statement in `ChatPage.qml` fails silently on
  device, causing the entire page to fail loading with "Kunne ikke indlæse
  siden". Also added `pkgconfig(Qt5Multimedia)` to PkgBR in yaml (it was
  only in the spec file).

## [0.8.1] - 2026-10-21
Skrynja 0.8.1 — Fix: ChatPage failed to load ("Kunne ikke indlæse siden").

### Fixed
- Removed invalid `mediaObject {}` grouped property from `AudioRecorder` in
  `ChatPage.qml`. The empty block caused a QML parse error on device, making
  the entire chat page fail to load. `AudioRecorder` picks up the default
  microphone automatically when the app has the Microphone permission.

## [0.8.0] - 2026-10-21
Skrynja 0.8.0 — New feature (F18): voice input with Mistral transcription.

### Added
- **Voice input**: a microphone button in the chat input area records audio
  using `AudioRecorder` (QtMultimedia). Tap to start, tap again to stop.
  While recording, the button shows a pulsing red ring and the input field
  shows "Recording...".
- **Transcription**: the recording is sent to Mistral's Audio Transcriptions
  API (`POST /v1/audio/transcriptions`, model `voxtral-mini-latest`). The
  transcribed text fills the input field so the user can edit before sending.
- **Microphone permission** added to the SailJail `[X-Sailjail]` section.
- **Qt Multimedia** added as a build dependency.

## [0.7.2] - 2026-10-21
Skrynja 0.7.2 — Feature: archive conversations deleted on other devices.

### Added
- **Two-way sync with archiving**: when auto-sync fetches the remote
  conversation list, local conversations whose remote counterpart no
  longer exists on the server are marked as *archived* instead of being
  deleted. They appear dimmed in the history with an "Archived" label.
- **Chat banner**: opening an archived conversation shows a banner
  explaining that the remote copy is gone and that sending a message
  starts a fresh server-side conversation.
- **Unarchive on send**: when the user sends a message in an archived
  conversation, it is un-archived and the remote id is cleared, so the
  next turn creates a new server-side conversation.
- **Delete all archived**: pull-down menu and context menu entry to
  permanently delete all archived conversations at once.

## [0.7.1] - 2026-10-21
Skrynja 0.7.1 — Feature: mark interrupted answers in the UI.

### Added
- **Interrupted marker**: when an answer is cut off (cancelled, timeout,
  network drop), it is now flagged with a subtle italic "Interrupted"
  label under the text. The flag persists across app restarts.
- This is the simple version of F6: the partial answer stays visible and
  is clearly marked, without restarting the server-side conversation.

## [0.7.0] - 2026-10-21
Skrynja 0.7.0 — Feature (F7): delete conversations on the server too.

### Added
- **Remote deletion**: when a conversation is deleted locally, it is also
  deleted on Mistral's servers (`DELETE /v1/conversations/{id}`) when the
  conversation has a remote id and an API key is available. This prevents
  auto-sync from bringing the conversation back on next startup.
- **Purge all** now also deletes all remote conversations on the server.
- The remote deletion is fire-and-forget: the local copy is removed
  immediately regardless of the server outcome. A 404 (already gone) counts
  as success.

## [0.6.1] - 2026-10-21
Skrynja 0.6.1 — Feature: automatic remote conversation sync.

### Added
- **Auto-sync**: when Skrynja starts and an API key is configured, it
  automatically fetches all remote conversations from Mistral's servers
  and attaches any that are not yet attached locally. Also triggers when
  a new API key is entered in Settings. Runs silently in the background;
  the history list refreshes when new conversations arrive.

## [0.6.0] - 2026-10-21
Skrynja 0.6.0 — New feature: bulk-attach all remote conversations.

### Added
- **Attach all** pull-down menu on the Remote Conversations page: attaches
  every not-yet-attached remote conversation in sequence, showing progress
  ("Attaching N of M..."). When done, returns to the history page with all
  conversations visible.
- **Refresh** pull-down menu entry to re-fetch the remote list.

## [0.5.10] - 2026-10-21
Skrynja 0.5.10 — Hotfix: show full conversation id for unnamed remotes.

### Fixed
- **Remote conversations list**: show the full conversation id instead of
  a 13-character prefix. The prefix truncation made different conversations
  with similar ids appear identical.

## [0.5.9] - 2026-10-21
Skrynja 0.5.9 — Hotfix: dedupe remote conversations in the C++ parser.

### Fixed
- **Remote conversations list**: duplicate conversations are now removed
  in `parseRemoteConversations()` (C++ layer) using a `QSet`, not just in
  the QML layer. The previous QML-only dedup did not work because the
  QVariantMap keys from C++ arrive as JavaScript objects where `seen[c.id]`
  does not behave as expected.

## [0.5.8] - 2026-10-21
Skrynja 0.5.8 — Hotfix: dedupe remote conversations and show count.

### Fixed
- **Remote conversations list**: duplicate conversations returned by the
  API are now deduplicated by id, so the same conversation no longer
  appears multiple times.
- Added a count label showing how many conversations were found on the
  server.

## [0.5.7] - 2026-10-21
Skrynja 0.5.7 — Hotfix: show full conversation id for unnamed remotes.

### Fixed
- **Remote conversations list**: show 13 characters of the conversation id
  (e.g. `conv_01a2b3c4`) instead of 8, so unnamed conversations with
  similar prefixes can be told apart.

## [0.5.6] - 2026-10-21
Skrynja 0.5.6 — Hotfix: distinguish unnamed remote conversations.

### Fixed
- **Remote conversations list**: when a conversation has no name, the
  first 8 characters of its id are shown next to the date so unnamed
  conversations can be told apart.

## [0.5.5] - 2026-10-21
Skrynja 0.5.5 — Hotfix: attached conversations appear at the top of history.

### Fixed
- **Attach remote conversations**: an attached conversation now gets
  `updatedAt = now` instead of the timestamp of the last original message,
  so it appears at the top of the history list where the user expects it.
- **Remote list fallback**: when a conversation has no name, the list now
  shows "Untitled conversation" instead of the model name (which was
  confusing — every row showed the same model name).

## [0.5.4] - 2026-10-21
Skrynja 0.5.4 — Hotfix: derive titles for attached conversations without names.

### Fixed
- **Attach remote conversations**: when Mistral has no name for a conversation
  (common for API-created ones), the attached conversation now gets its title
  from the first user message instead of "Imported conversation" or the model
  name. The remote list also shows the model name as a fallback when the
  conversation name is empty.

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
