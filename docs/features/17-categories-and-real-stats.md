# 17 — Question Categories & Meaningful Stats

**Status:** done (v1.9.22)
**Depends on:** 03 (token tracking), 15 (stats UI)
**Touches:** `src/mistralapi.{h,cpp}`, `src/conversationmanager.{h,cpp}`, `qml/components/CategoryChip.qml` (new), `qml/pages/ChatPage.qml`, `qml/pages/ConversationDetailPage.qml`, `qml/pages/ConversationHistoryPage.qml`, `qml/pages/StatsPage.qml`, `harbour-skrynja.pro`

## Rationale

The user/assistant message ratio was meaningless: an AI always answers, so it is 1:1 by construction. Replaced everywhere with metrics that actually vary, plus automatic conversation categorization.

## Categorization (no extra API call)

`generateTitle` now asks for a compact JSON `{"title": "...", "category": "..."}` in a single request. Fixed category enum: `code`, `writing`, `translation`, `learning`, `ideas`, `practical`, `other`. Parsing is defensive (extracts the `{...}` substring, falls back to raw-content-as-title, validates the category against the enum). Signal became `titleGenerated(title, category)`; `Conversation` gained a persisted `category` field. Old conversations show as uncategorized.

`CategoryChip` component: colored pill (fixed palette readable on both ambiences), shown in the history rows and the detail stats panel.

## Token time series & per-conversation tokens

- `addTokenUsage` also writes a daily map (`stats/dailyTokens` JSON, date → tokens, pruned at 62 days) and increments the current conversation's persisted `totalTokens`.
- `getStatistics()` exposes `tokensPerDay` (aligned 14-day list), `tokensThisMonth`, `totalUserChars`, `totalAssistantChars`, `categoryCounts`.
- `getConversationStatistics()` exposes real `totalTokens`, `userChars`, `assistantChars`, `category`.

## Stat replacements

- **Detail donut**: was "% sent by you" (~50%), now "% written by AI" (characters — actually varies).
- **Detail tokens**: real count when tracked, `~estimate` for pre-tracking conversations.
- **History row bar**: was user ratio, now conversation size relative to the largest conversation.
- **StatsPage "Sent vs received"** → **"Who writes more?"** (characters split you/AI).
- **StatsPage new sections**: "Question categories" (sorted colored bars) and "Tokens - last 14 days" (bar chart) + "Tokens this month" detail row.

## Notes

- Category colors/labels are duplicated between CategoryChip and StatsPage (qsTr needs QML context; a JS `.pragma library` cannot translate). Keep both switches in sync.

## Acceptance criteria

- [ ] A new conversation gets a title AND a colored category chip after the first exchange.
- [ ] Model returning malformed JSON still yields a usable title, category "other".
- [ ] Tokens chart fills day by day; "Tokens this month" accumulates; restarting the app keeps the series.
- [ ] Old saved conversations load fine (no category, estimated tokens).
