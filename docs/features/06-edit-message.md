# 06 — Edit User Message & Resend — REMOVED IN SKRYNJA

> **Removed in the fork.** Editing requires entry-level restart
> (`POST /v1/conversations/{id}/restart#stream`), which is deferred. See
> `docs/ROADMAP.md` ("Removed in the fork").

# 06 — Edit User Message & Resend (historical SailCat spec)

**Status:** todo
**Depends on:** 05 (same MessageBubble signal pattern)
**Touches:** `src/conversationmodel.{h,cpp}`, `qml/components/MessageBubble.qml`, `qml/pages/ChatPage.qml`

## Goal

Context-menu action "Edit" on any user message: truncate the conversation from that message onward, put its text back into the input field for editing, and let the user send normally.

## Implementation

### ConversationModel

```cpp
Q_INVOKABLE void truncateFrom(int index);
```

Removes messages `[index, count)`. Use `beginRemoveRows(QModelIndex(), index, m_messages.count() - 1)` once for the whole range; guard `index < 0 || index >= m_messages.count()`. Emit `countChanged()`.

### MessageBubble.qml

```qml
signal editRequested()

MenuItem {
    text: qsTr("Edit")
    visible: role === "user" && !mistralApi.isBusy
    onClicked: editRequested()
}
```

### ChatPage.qml

In the delegate binding:

```qml
delegate: MessageBubble {
    ...
    onEditRequested: chatPage.editMessage(index, model.content)
}
```

```js
function editMessage(index, content) {
    if (mistralApi.isBusy) return
    conversationModel.truncateFrom(index)
    conversationManager.saveCurrentConversation()
    messageInput.text = content
    messageInput.focus = true
}
```

Save immediately after truncation so an app close before resending doesn't resurrect the removed tail.

## Edge cases

- Editing the first message of a titled conversation: keep the existing title (regenerating it is not worth the complexity).
- Editing while streaming: guarded by `!mistralApi.isBusy`.
- Truncating everything (edit of message 0) leaves an empty conversation — `ViewPlaceholder` shows again; verify no crash in `saveCurrentConversation()` with zero messages.

## Acceptance criteria

- [ ] Long-press a user message → "Edit" → messages from it onward disappear, its text is in the input, keyboard open.
- [ ] Sending the edited text produces a normal new exchange.
- [ ] Killing the app between edit and resend does not restore removed messages.
- [ ] Docker build passes.
