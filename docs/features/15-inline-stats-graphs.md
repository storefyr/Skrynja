# 15 — Animated Stats & Graphs in Conversation Pages

**Status:** done (v1.9.20)
**Depends on:** 14 (animation patterns)
**Touches:** `src/conversationmanager.{h,cpp}`, `qml/components/CountUpLabel.qml` (new), `qml/components/RatioDonut.qml` (new), `qml/pages/ConversationDetailPage.qml`, `qml/pages/ConversationHistoryPage.qml`, `qml/pages/StatsPage.qml`, `harbour-skrynja.pro`

## Backend

- `getConversationStatistics(conversationId)`: per-conversation `messageCount`, `userCount`, `assistantCount`, `totalChars`, `avgChars`, `longestChars`, `estimatedTokens`, `durationMs`, and `rhythm` — a list of `{chars, role}` for the last 40 messages.
- `getConversationsList()` and `searchConversations()` now include `userMessageCount` for per-item ratio bars.

## New components

- **CountUpLabel** — odometer label; counts 0 → value (1s OutCubic) when its `go` property flips. Animation is driven declaratively (`displayValue: go ? value : 0` + Behavior), so it works inside ListView headers without imperative calls.
- **RatioDonut** — Canvas donut whose arc sweeps from 12 o'clock (1.1s OutCubic), rounded caps, animated percentage in the center. Colors from Theme.

## Per page

- **ConversationDetailPage** ("View details"): stats panel with donut (share of messages sent by you), count-up messages/~tokens, human duration; plus a **conversation rhythm** chart — one bar per message, height ∝ √(length), user vs assistant colors, 30ms staggered OutBack cascade — with a small legend.
- **ConversationHistoryPage**: 14-day activity histogram in the header (staggered grow-in, re-animates on each visit) and a thin animated user/assistant ratio bar under each conversation row.
- **StatsPage**: existing charts (daily, hourly, sent/received) now grow in over 800ms on open via a shared `chartProgress` driver.

## Acceptance criteria

- [ ] Opening "View details" animates donut sweep, counter roll-up and rhythm cascade together.
- [ ] History header histogram cascades on every visit; ratio bars fill on load.
- [ ] StatsPage charts grow on open.
- [ ] Search results still render (ratio bar included) and empty conversations don't break the panel.
- [ ] Nothing animates after load completes (battery).
