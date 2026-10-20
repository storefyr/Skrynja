# 13 — Pinned Messages

**Status:** done (v1.9.17)
**Depends on:** nothing
**Touches:** `src/conversationmodel.{h,cpp}`, `src/conversationmanager.{h,cpp}`, `qml/components/MessageBubble.qml`, `qml/pages/ChatPage.qml`, `qml/pages/PinnedMessagesPage.qml` (new), `harbour-skrynja.pro`

## Goal

Pin any message from its context menu, see pinned messages marked visually in the chat, and browse all pinned messages (across all conversations) in a dedicated page. Tapping an entry opens its conversation and scrolls to the message.

## Data model

- `Message` struct gains `bool pinned = false`, persisted in the conversation JSON as `"pinned"` (absent in old saves → `false`, so existing history remains loadable).
- `ConversationModel`: `PinnedRole` (QML name `pinned`), `togglePinned(int index)` emitting `dataChanged` on `PinnedRole` only; `addMessage()` gained a `pinned` default parameter used when loading.
- `ConversationManager`: pinned round-trips through save/load; `getPinnedMessages()` returns a QVariantList of `{conversationId, conversationTitle, messageIndex, role, content, timestamp}` across all conversations.

## UI

- **MessageBubble**: context menu Pin/Unpin (emits `pinToggled`); pinned messages show a thin highlight bar on the left edge plus a small `icon-s-favorite` star in the top-right corner.
- **ChatPage**: toggling a pin saves the conversation immediately; pulley menu entry "Pinned messages" pushes the new page (saving first so current-conversation pins are visible).
- **PinnedMessagesPage**: list of star + conversation title, 3-line content preview, date. Tap → `loadConversation()` + sets `chatPage.pendingScrollIndex` + `pop()`; ChatPage applies the scroll (`positionViewAtIndex(..., ListView.Center)`) when it becomes active again.

## Edge cases

- Message index shifts after edit/truncate: the pinned list is rebuilt from saved data each time the page opens, and pins live on the messages themselves, so indices are always current at click time.
- Deleting a conversation removes its pins implicitly (they live inside the conversation record).

## Acceptance criteria

- [ ] Pin a message → star + highlight bar appear; unpin removes them.
- [ ] Pins survive app restart; old conversations (pre-1.9.17) still load.
- [ ] Pinned page lists pins from several conversations; tapping jumps to the right message in the right conversation.
- [ ] Empty state shows the hint text.
