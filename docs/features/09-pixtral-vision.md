# 09 — Image Support for Vision Models (Pixtral)

**Status:** done (v2.0.0)
**Depends on:** 04 (uses `settingsManager.isVisionModel()`; without 04, hardcode `modelName.indexOf("pixtral") !== -1`)
**Touches:** `src/conversationmodel.{h,cpp}`, `src/conversationmanager.cpp`, `src/mistralapi.cpp`, `qml/pages/ChatPage.qml`, `qml/components/MessageBubble.qml`, `harbour-skrynja.pro`

This is the largest feature. Implement in the order below; each step compiles independently.

## Goal

Attach one image to a user message when a vision-capable model is selected. The image is displayed in the chat, persisted with the conversation (as a local file path), and sent to the API as base64.

## API reference

For vision models, `content` becomes an array:

```json
{"role": "user", "content": [
  {"type": "text", "text": "What is in this image?"},
  {"type": "image_url", "image_url": "data:image/jpeg;base64,/9j/4AAQ..."}
]}
```

Non-vision messages keep plain string content. Limits: max ~10MB per image; downscale before encoding regardless.

## Step 1 — Message model carries an image path

`src/conversationmodel.h`:
- `struct Message`: add `QString imagePath;` (empty = no image).
- Add role `ImagePathRole` to the enum + `roleNames()` (`"imagePath"`) + `data()`.
- Change signature: `Q_INVOKABLE void addUserMessage(const QString &content, const QString &imagePath = QString());`
- `addMessage()` gains the same optional parameter (used by ConversationManager when loading).

`src/conversationmanager.cpp`:
- Find where messages are serialized/deserialized (JSON objects with `role`/`content`/`timestamp`) and add optional `imagePath`. Missing key on load → empty string. This keeps old saved conversations loadable — verify by loading a pre-existing conversation.

## Step 2 — Base64 encoding helper (C++)

QML cannot read files; do it in C++. Add to `ConversationManager` (or a small new `ImageHelper` class registered as context property — if new class, update `harbour-skrynja.pro` and `src/harbour-skrynja.cpp`):

```cpp
Q_INVOKABLE QString imageToDataUrl(const QString &filePath) const;
```

Implementation:
- Strip a `file://` prefix if present (`QUrl(filePath).toLocalFile()` when it starts with "file:").
- Load with `QImage`; fail → return `""`.
- If `width > 1024 || height > 1024`: `img = img.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);`
- Save as JPEG quality 85 into a `QBuffer`/`QByteArray`, then return `"data:image/jpeg;base64," + QString::fromLatin1(bytes.toBase64())`.
- All of this is Qt 5.6-safe. Requires `QT += gui` (already the case for a QML app).

## Step 3 — API request building

`MistralAPI::sendMessage()` currently flattens each message to string content. Change the message conversion loop:

```cpp
QVariantMap msgMap = msgVariant.toMap();
QJsonObject msgObj;
msgObj["role"] = msgMap["role"].toString();
QVariant contentVar = msgMap["content"];
if (contentVar.type() == QVariant::List) {
    msgObj["content"] = QJsonArray::fromVariantList(contentVar.toList());
} else {
    msgObj["content"] = contentVar.toString();
}
```

The array is built QML-side (step 5), so MistralAPI stays dumb. `ConversationModel::getMessagesForApi()` keeps returning plain string contents; ChatPage swaps in the array form for messages that have an image.

## Step 4 — Attach button + picker (ChatPage.qml)

- Add `import Sailfish.Pickers 1.0` (Harbour-allowed).
- `property string attachedImagePath: ""`
- In the input Row, before the TextArea, an `IconButton`:
  - `icon.source: attachedImagePath === "" ? "image://theme/icon-m-attach" : "image://theme/icon-m-attach?" + Theme.highlightColor`
  - `visible: settingsManager.isVisionModel(settingsManager.modelName)` (see Depends on)
  - `onClicked:` if an image is attached, clear it (`attachedImagePath = ""`); else `pageStack.push(imagePickerPage)`
- Adjust the TextArea width binding to subtract the new button when visible.

```qml
Component {
    id: imagePickerPage
    ImagePickerPage {
        onSelectedContentPropertiesChanged: {
            chatPage.attachedImagePath = selectedContentProperties.filePath
        }
    }
}
```

- Show a small thumbnail strip above the input when `attachedImagePath !== ""`: `Image { source: attachedImagePath; height: Theme.itemSizeMedium; fillMode: Image.PreserveAspectFit; asynchronous: true }` with a clear (`icon-m-clear`) button.

## Step 5 — sendMessage() wiring (ChatPage.qml)

In `sendMessage()`:

```js
var imagePath = attachedImagePath
attachedImagePath = ""
conversationModel.addUserMessage(message, imagePath)
var messages = conversationModel.getMessagesForApi()
if (imagePath !== "") {
    var dataUrl = conversationManager.imageToDataUrl(imagePath)
    if (dataUrl !== "") {
        messages[messages.length - 1] = {
            "role": "user",
            "content": [
                { "type": "text", "text": message },
                { "type": "image_url", "image_url": dataUrl }
            ]
        }
    }
}
```

History messages with images are re-sent as text-only (their `content` from `getMessagesForApi()` is the plain text) — acceptable simplification; the model sees the image only in the turn where it was attached. Document this in the About/README if asked.

## Step 6 — Display (MessageBubble.qml)

- `property string imagePath: ""` set from `model.imagePath` in the ChatPage delegate.
- Above the text Label, `Image { visible: imagePath !== ""; source: imagePath; width: Math.min(parent.width * 0.6, sourceSize.width); fillMode: Image.PreserveAspectFit; asynchronous: true }`.
- `contentHeight` must account for the image: restructure the bubble as a `Column` (image + label) and bind `contentHeight` to the column height + padding.
- Missing file (deleted from gallery) → `Image` fails silently; keep a `status === Image.Error` fallback hiding the element.

## Acceptance criteria

- [ ] Attach button appears only for vision-capable models.
- [ ] Pick image → thumbnail preview → send → image shows in the user bubble and Pixtral describes it correctly.
- [ ] Conversation with image reloads after app restart (path persisted); old conversations still load.
- [ ] Non-vision request bodies are unchanged (string content).
- [ ] A 4000×3000 photo is downscaled (request body < 1MB, no API 413 error).
- [ ] Docker build passes.
