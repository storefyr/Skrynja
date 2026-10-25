# 17 — Meaningful Stats (categories removed)

**Status:** categories removed; stats remain (v0.9.9+)
**Depends on:** 03 (token tracking), 15 (stats UI)

## Rationale

The user/assistant message ratio was meaningless: an AI always answers, so it is 1:1 by construction. Replaced everywhere with metrics that actually vary.

## Categories (removed)

The category feature was removed entirely: `src/categories.{h,cpp}`, `qml/components/Categories.js`, `qml/components/CategoryChip.qml`, the category field in the `Conversation` struct, the category prompt tokens in `generateTitle()`, the "Question categories" stats section, and the category ComboBox in conversation settings. Titles are still generated, but without a category.

## Token time series & per-conversation tokens

- `addTokenUsage` also writes a daily map (`stats/dailyTokens` JSON, date → tokens, pruned at 62 days) and increments the current conversation's persisted `totalTokens`.
- `getStatistics()` exposes `tokensPerDay` (aligned 14-day list), `tokensThisMonth`, `totalUserChars`, `totalAssistantChars`.
- `getConversationStatistics()` exposes real `totalTokens`, `userChars`, `assistantChars`.

## Stat replacements

- **Detail donut**: was "% sent by you" (~50%), now "% written by AI" (characters — actually varies).
- **Detail tokens**: real count when tracked, `~estimate` for pre-tracking conversations.
- **History row bar**: was user ratio, now conversation size relative to the largest conversation.
- **StatsPage "Sent vs received"** → **"Who writes more?"** (characters split you/AI).
- **StatsPage sections**: "Tokens - last 14 days" (bar chart) + "Tokens this month" detail row.

## Acceptance criteria

- [x] Tokens chart fills day by day; "Tokens this month" accumulates; restarting the app keeps the series.
- [x] Old saved conversations load fine (estimated tokens).
