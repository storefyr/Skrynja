# 05 — Regenerate Last Response — REMOVED IN SKRYNJA

> **Removed in the fork.** Regenerate requires entry-level restart
> (`POST /v1/conversations/{id}/restart#stream`), which is deferred. See
> `docs/ROADMAP.md` ("Removed in the fork").

# 05 — Regenerate Last Response (historical SailCat spec)

**Status:** todo
**Depends on:** 02 (reuse the system-prompt prepend helper if implemented)
**Touches:** `src/conversationmodel.{h,cpp}`, `qml/components/MessageBubble.qml`, `qml/pages/ChatPage.qml`

## Goal

Context-menu action "Regenerate" on the last assistant message: remove it and re-request a completion for the same history.

## Implementation

### ConversationModel

```cpp
Q_INVOKABLE void removeLastAssistantMessage();
```

If the last message exists and has role `"assistant"`: `beginRemoveRows`, remove, `endRemoveRows`, emit `countChanged()`. Otherwise no-op.

### MessageBubble.qml

Add to the existing `ContextMenu` (which currently only has Copy):

```qml
MenuItem {
    text: qsTr("Regenerate")
    visible: role === "assistant" && index === ListView.view.count - 1 && !mistralApi.isBusy
    onClicked: chatPage.regenerateLastResponse()
}
```

Notes:
- `index` is available in the delegate context; `ListView.view` gives the attached view. Verify both resolve inside `MessageBubble` (it's a `ListItem` delegate); if not, pass `isLast` as a property from the delegate binding in ChatPage instead.
- Referencing `chatPage` by id from a component file creates a coupling — prefer a signal: declare `signal regenerateRequested()` in MessageBubble, emit it, and connect in the ChatPage delegate binding: `onRegenerateRequested: chatPage.regenerateLastResponse()`.

### ChatPage.qml

```js
function regenerateLastResponse() {
    if (mistralApi.isBusy) return
    conversationModel.removeLastAssistantMessage()
    var messages = conversationModel.getMessagesForApi()
    if (settingsManager.systemPrompt !== "") {
        messages.unshift({ "role": "system", "content": settingsManager.systemPrompt })
    }
    autoScroll = true
    conversationModel.addAssistantMessage("")
    mistralApi.sendMessage(settingsManager.apiKey, settingsManager.modelName, messages,
                           settingsManager.temperature, settingsManager.maxTokens)
}
```

(Drop the temperature/maxTokens/systemPrompt parts if features 01/02 are not implemented yet — adapt to whatever `sendMessage` signature exists at that point.)

The existing `onResponseCompleted` handler already saves the conversation, so persistence of the replaced answer is automatic. The title-generation trigger (`count === 2`) may re-fire after regenerating the first answer — that is acceptable (title just regenerates too).

## Edge cases

- Regenerate while a request is streaming: guarded by `!mistralApi.isBusy` (menu hidden + function guard).
- Conversation whose last message is a user message (previous request failed): menu not shown; the user can just resend.

## Acceptance criteria

- [ ] Long-press on the last assistant message shows "Regenerate"; on any other message it does not.
- [ ] Regenerating produces a new streamed answer replacing the old one, and the saved conversation on disk contains only the new answer.
- [ ] No "Regenerate" entry while busy.
- [ ] Docker build passes.
