# 07 — Export Conversation

**Status:** todo
**Depends on:** nothing
**Touches:** `src/conversationmanager.{h,cpp}`, `qml/pages/ChatPage.qml`, `qml/pages/ConversationHistoryPage.qml`

## Goal

Export a conversation as a Markdown file in the user's Documents folder, plus a "copy all to clipboard" shortcut. No share sheet: `Sailfish.TransferEngine` / `Sailfish.Share` are not Harbour-allowed, so file + clipboard is the compliant approach.

## Implementation

### ConversationManager

```cpp
Q_INVOKABLE QString exportConversation(const QString &conversationId) const;   // returns file path, or "" on failure
Q_INVOKABLE QString conversationToMarkdown(const QString &conversationId) const;
```

`conversationToMarkdown` format:

```markdown
# <title>

_Exported from Skrynja — 2026-07-03 14:30_

## User

<content>

## Assistant

<content>
```

`exportConversation`:
- Target dir: `QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)` (exists in Qt 5.6).
- Filename: `skrynja-<sanitized-title>-<yyyyMMdd-HHmmss>.md`. Sanitize: keep `[a-zA-Z0-9-_]`, replace everything else with `-`, collapse repeats, lowercase, truncate to 40 chars.
- Write with `QFile` + `QTextStream`, `stream.setCodec("UTF-8")` (required on Qt 5.6 — default codec is locale-dependent).
- Return the absolute path, or empty string on any failure (log with `qWarning()`).

### UI

Two entry points, same actions:

1. `ChatPage.qml` PullDownMenu — add above "New conversation":

```qml
MenuItem {
    text: qsTr("Export conversation")
    enabled: conversationModel.count > 0
    onClicked: {
        var path = conversationManager.exportConversation(/* current id — add a currentConversationId getter if none exists; check conversationmanager.h first */)
        if (path !== "") {
            exportNotice.show(qsTr("Exported to %1").arg(path))
        } else {
            exportNotice.show(qsTr("Export failed"))
        }
    }
}
```

Use a `Notification` from `Nemo.Notifications 1.0` (Harbour-allowed) or reuse the existing `RemorsePopup`-style feedback — a simple `Label`-based banner is also fine; match what the app already does for transient feedback.

**Important:** `ConversationManager` currently keeps `m_currentConversationId` private with no getter. Add `Q_INVOKABLE QString currentConversationId() const;`.

2. `ConversationHistoryPage.qml` — add to each list item's ContextMenu: `qsTr("Export")` → same call with `model.id`, plus `qsTr("Copy as text")` → `Clipboard.text = conversationManager.conversationToMarkdown(model.id)`.

## Acceptance criteria

- [ ] Export creates a readable UTF-8 `.md` file under `~/Documents` with all messages in order (verify accents/emoji survive).
- [ ] Filename is filesystem-safe for titles containing `/`, `:`, spaces, quotes.
- [ ] Empty conversation → menu disabled.
- [ ] "Copy as text" puts the full markdown in the clipboard.
- [ ] Docker build passes; no non-Harbour imports introduced.
