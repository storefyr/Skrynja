# 19 — Unit Test Suite

**Status:** done (v2.0.4)
**Depends on:** nothing
**Touches:** `tests/` (new), `src/mistralapi.h` (friend declaration), `.github/workflows/*.yml`, `AGENTS.md`

## Approach

The four backend classes are pure Qt (no Silica), so they compile and run against stock Ubuntu Qt 5. A single QtTest binary (`tests/tst_skrynja`) runs three suites; a dedicated `unit-tests` CI job runs on `ubuntu-22.04` for every manual build, PR, push to main and tag — and `create-release` now requires it, so a red test blocks the release.

## Coverage

- **ConversationModel**: add/count, streaming update creating the assistant row, empty-bubble cleanup, regenerate removal (and its user-last no-op), truncation bounds, pin toggling, image path round trip, API payload shape (role+content only), last-assistant lookup skipping empty content.
- **MistralAPI** (via `friend class TestMistralAPI` on the private `processStreamData`): SSE delta parsing, UTF-8 sequence split across network chunks, `[DONE]`, usage extraction, malformed lines, multiple lines per chunk.
- **ConversationManager**: full save/load round trip (pins, image paths, title, category), markdown export format, pinned list across conversations, fun stats (ghost count, longest message, top words), token accumulation (lifetime, per conversation, daily series), category counting. `XDG_CONFIG_HOME` is redirected to a QTemporaryDir so tests never touch real user settings.

## Out of scope

- QML/UI tests: Silica components need the Sailfish runtime; still manual on device.
- Network-level tests: MistralAPI request building is exercised only up to JSON body construction.

## Local run

```bash
cd tests && qmake tests.pro && make && ./tst_skrynja
```
