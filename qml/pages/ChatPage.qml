import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import Nemo.Notifications 1.0
import "../components"

Page {
    id: chatPage
    objectName: "chatPage"
    allowedOrientations: Orientation.All

    property bool firstUse: false
    property bool autoScroll: true
    property int lastPromptTokens: 0
    property int lastCompletionTokens: 0
    property int conversationTokens: 0
    property int pendingScrollIndex: -1
    property string attachedImagePath: ""
    // Set while the attached image is being uploaded to Mistral's Files API.
    // The send waits for the upload: the turn references the file id.
    property bool uploadingImage: false
    // F18: voice input state
    property bool recording: false
    property bool transcribing: false
    property string recordingPath: ""
    property bool titleRequested: false
    // Server-side tool currently running (e.g. web_search), empty when idle.
    property string toolName: ""

    // Set while a rate-limited request waits its turn again. The answer is
    // still coming, so this is a notice, not an error.
    property int retrySeconds: 0
    property int retryAttempt: 0
    property int retryAttempts: 0

    // Per-conversation system prompt override. Kept as a property rather
    // than read on demand so bindings actually re-evaluate when it changes.
    property string conversationPromptOverride: ""

    // The model is fixed when the conversation is created (Conversations API
    // locks it server-side). Empty before creation: the global default is
    // used for the first turn and then stored on the conversation.
    readonly property string activeModel: {
        var m = conversationManager.conversationModel(
                    conversationManager.currentConversationId())
        return m !== "" ? m : settingsManager.resolveModel(settingsManager.modelName)
    }

    readonly property bool providerReady: settingsManager.hasApiKey

    onStatusChanged: {
        if (status === PageStatus.Active) {
            // Settings of this conversation reachable by swiping forward.
            // The history is one swipe back: it is the page underneath.
            if (pageStack.nextPage(chatPage) === null) {
                pageStack.pushAttached(Qt.resolvedUrl("ConversationSettingsPage.qml"))
            }
            // Jump requested from the pinned messages page
            if (pendingScrollIndex >= 0) {
                autoScroll = false
                messageListView.positionViewAtIndex(pendingScrollIndex, ListView.Center)
                pendingScrollIndex = -1
            }
            chatPage.refreshOverrides()
            // Looking at it counts as reading it
            conversationManager.markConversationRead(
                        conversationManager.currentConversationId())
        }
    }

    SilicaListView {
        id: messageListView
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: inputArea.top
        }
        clip: true

        model: conversationModel
        spacing: settingsManager.chatStyle === "compact" ? Theme.paddingSmall
                                                         : Theme.paddingMedium

        add: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 300; easing.type: Easing.OutQuad }
                NumberAnimation { property: "scale"; from: 0.92; to: 1; duration: 300; easing.type: Easing.OutBack }
            }
        }

        // Stop following the stream when the user scrolls away,
        // resume when they come back to the bottom
        onMovementStarted: chatPage.autoScroll = false
        onMovementEnded: chatPage.autoScroll = messageListView.atYEnd

        header: PageHeader {
            title: "Skrynja"
            // Which model is answering this conversation.
            description: chatPage.activeModel
        }

        // Banner for archived conversations: the remote copy is gone (deleted
        // on another device), but the local history is still here.
        Rectangle {
            id: archivedBanner
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
            height: archivedLabel.height + 2 * Theme.paddingSmall
            visible: conversationManager.isArchived(
                         conversationManager.currentConversationId())
            color: Theme.rgba(Theme.secondaryHighlightColor, 0.15)
            z: 1

            Label {
                id: archivedLabel
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Archived — deleted on another device. Sending a message starts a new conversation.")
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // The two pulleys used to carry the same seven entries, which made both
        // of them a wall of text. They are split by what they act on: the top
        // one leaves this conversation, the bottom one acts on it.
        PullDownMenu {
            MenuItem {
                text: qsTr("Pinned messages")
                onClicked: chatPage.openPinned()
            }
            MenuItem {
                text: qsTr("Prompt library")
                onClicked: chatPage.openPromptLibrary()
            }
            MenuItem {
                text: qsTr("Settings & About")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
        }

        PushUpMenu {
            MenuItem {
                text: qsTr("Conversation settings")
                onClicked: chatPage.openConversationSettings()
            }
            MenuItem {
                text: qsTr("Export conversation")
                enabled: conversationModel.count > 0
                onClicked: chatPage.exportCurrentConversation()
            }
            MenuItem {
                text: qsTr("New conversation")
                enabled: conversationModel.count > 0
                onClicked: chatPage.startNewConversation()
            }
        }

        ViewPlaceholder {
            enabled: conversationModel.count === 0
            text: firstUse ? qsTr("Welcome to Skrynja") : qsTr("Start a conversation")
            hintText: firstUse ? qsTr("Enter your Mistral API key to get started") : qsTr("Type a message below")
        }

        delegate: MessageBubble {
            width: messageListView.width
            role: model.role
            content: model.content
            isLast: index === messageListView.count - 1
            pinned: model.pinned
            timestamp: model.timestamp
            imagePath: model.imagePath
            messageModel: model.messageModel
            messageSources: model.sources || []
            messageThinking: model.thinking || ""
            messageInterrupted: model.interrupted || false

            onPinToggled: {
                conversationModel.togglePinned(index)
                conversationManager.saveCurrentConversation()
            }
        }

        VerticalScrollDecorator {}
    }

    // Footer with input area
    Column {
        id: inputArea
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        spacing: 0

        // Attached image preview
        Item {
            width: parent.width
            height: chatPage.attachedImagePath !== "" ? Theme.itemSizeMedium + Theme.paddingMedium : 0
            visible: height > 0
            clip: true

            Behavior on height { NumberAnimation { duration: 200 } }

            Image {
                id: attachPreview
                source: chatPage.attachedImagePath
                height: Theme.itemSizeMedium
                width: Theme.itemSizeMedium * 1.5
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize.width: 256
            }

            IconButton {
                anchors {
                    left: attachPreview.right
                    leftMargin: Theme.paddingMedium
                    verticalCenter: parent.verticalCenter
                }
                icon.source: "image://theme/icon-m-clear"
                onClicked: chatPage.attachedImagePath = ""
            }
        }

        // Token usage banner
        Item {
            width: parent.width
            height: visible ? tokenLabel.height + Theme.paddingSmall : 0
            visible: chatPage.conversationTokens > 0

            Label {
                id: tokenLabel
                anchors.centerIn: parent
                text: chatPage.lastCompletionTokens > 0
                      ? qsTr("Tokens: %1 total - last: %2 in / %3 out")
                            .arg(chatPage.conversationTokens)
                            .arg(chatPage.lastPromptTokens)
                            .arg(chatPage.lastCompletionTokens)
                      : qsTr("Tokens: %1 total").arg(chatPage.conversationTokens)
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.secondaryColor
            }
        }

        // Rate limit notice. Not an error: the request is on its way back out,
        // so it gets the highlight colour and no dismiss button.
        Rectangle {
            width: parent.width
            height: chatPage.retryAttempt > 0
                    ? retryLabel.height + Theme.paddingMedium * 2 : 0
            color: Theme.rgba(Theme.highlightColor, 0.15)
            visible: height > 0

            Behavior on height { NumberAnimation { duration: 200 } }

            Label {
                id: retryLabel
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.horizontalPageMargin
                }
                text: chatPage.retrySeconds > 0
                      ? qsTr("Provider busy - retrying in %1 s (%2/%3)")
                            .arg(chatPage.retrySeconds)
                            .arg(chatPage.retryAttempt)
                            .arg(chatPage.retryAttempts)
                      : qsTr("Provider busy - retrying (%1/%2)")
                            .arg(chatPage.retryAttempt)
                            .arg(chatPage.retryAttempts)
                color: Theme.highlightColor
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
            }
        }

        // Error banner
        Rectangle {
            width: parent.width
            height: mistralApi.error !== "" ? errorLabel.height + Theme.paddingMedium * 2 : 0
            color: Theme.rgba(Theme.errorColor, 0.2)
            visible: height > 0

            Behavior on height { NumberAnimation { duration: 200 } }

            Label {
                id: errorLabel
                anchors {
                    left: parent.left
                    right: retryErrorButton.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.paddingMedium
                }
                text: mistralApi.error
                color: Theme.errorColor
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
            }

            IconButton {
                id: retryErrorButton
                anchors {
                    right: closeErrorButton.left
                    verticalCenter: parent.verticalCenter
                }
                icon.source: "image://theme/icon-m-refresh"
                enabled: !mistralApi.isBusy && conversationModel.count > 0
                onClicked: chatPage.retryLastRequest()
            }

            IconButton {
                id: closeErrorButton
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    rightMargin: Theme.paddingMedium
                }
                icon.source: "image://theme/icon-m-clear"
                onClicked: mistralApi.clearError()
            }
        }

        // Separator with a scanning light while a response is streaming
        Item {
            id: separatorArea
            width: parent.width
            height: Theme.paddingSmall

            Separator {
                width: parent.width
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.highlightColor
                opacity: 0.3
            }

            Rectangle {
                id: scanGlow
                visible: mistralApi.isBusy || chatPage.uploadingImage
                width: separatorArea.width / 3
                height: Theme.paddingSmall / 2
                radius: height / 2
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.highlightColor
                opacity: 0.25
                x: scanLine.x - (width - scanLine.width) / 2
            }

            Label {
                visible: chatPage.uploadingImage
                anchors.centerIn: parent
                text: qsTr("Uploading image…")
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.secondaryColor
            }

            Label {
                visible: !chatPage.uploadingImage && chatPage.toolName !== ""
                anchors.centerIn: parent
                text: chatPage.toolName === "web_search"
                      ? qsTr("Searching the web…")
                      : qsTr("Working (%1)…").arg(chatPage.toolName)
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.secondaryColor
            }
            Rectangle {
                id: scanLine
                visible: (mistralApi.isBusy && chatPage.toolName === "")
                         || chatPage.uploadingImage
                width: separatorArea.width / 5
                height: 2
                radius: 1
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.highlightColor

                SequentialAnimation on x {
                    running: mistralApi.isBusy || chatPage.uploadingImage
                    loops: Animation.Infinite
                    NumberAnimation {
                        from: -scanLine.width
                        to: separatorArea.width
                        duration: 1100
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }

        // Input row
        Item {
            width: parent.width
            height: Math.max(messageInput.height, Theme.itemSizeSmall) + Theme.paddingMedium * 2

            Row {
                anchors {
                    fill: parent
                    margins: Theme.paddingMedium
                }
                spacing: Theme.paddingMedium

                // Image attachment (vision models only)
                IconButton {
                    id: attachButton
                    anchors.verticalCenter: parent.verticalCenter
                    visible: settingsManager.isVisionModel(
                                                              chatPage.activeModel)
                    icon.source: "image://theme/icon-m-attach"
                    icon.highlighted: chatPage.attachedImagePath !== ""
                    enabled: !chatPage.uploadingImage
                    onClicked: {
                        if (chatPage.attachedImagePath !== "") {
                            chatPage.attachedImagePath = ""
                        } else {
                            pageStack.push(imagePickerComponent)
                        }
                    }
                }

                // F18: Voice input - record, transcribe, fill the input.
                Item {
                    id: micButton
                    anchors.verticalCenter: parent.verticalCenter
                    visible: chatPage.providerReady
                    width: Theme.iconSizeMedium
                    height: Theme.iconSizeMedium

                    property bool recording: chatPage.recording
                    property bool transcribing: chatPage.transcribing

                    IconButton {
                        anchors.fill: parent
                        icon.source: micButton.recording
                            ? "image://theme/icon-m-mic-mute"
                            : "image://theme/icon-m-mic"
                        icon.highlighted: micButton.recording || micButton.transcribing
                        enabled: !mistralApi.isBusy && !chatPage.uploadingImage
                                 && !micButton.transcribing
                        opacity: micButton.recording ? 1.0 : (micButton.transcribing ? 0.5 : 1.0)

                        // Pulsing ring while recording
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width * 1.6
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.color: Theme.errorColor
                            border.width: 2
                            visible: micButton.recording
                            z: -1
                            SequentialAnimation on opacity {
                                running: micButton.recording
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.2; duration: 600 }
                                NumberAnimation { from: 0.2; to: 1.0; duration: 600 }
                            }
                        }

                        onClicked: {
                            if (chatPage.recording) {
                                // Stop recording. Transcription starts when
                                // the recorder emits 'stopped' (file is flushed).
                                // IMPORTANT: set transcribing BEFORE stop() —
                                // stop() may emit 'stopped' synchronously (when
                                // the recorder is not in RecordingState), and
                                // onStopped only transcribes when transcribing
                                // is already true. Setting it after stop() made
                                // the transcription never start, so the
                                // watchdog always fired with "took too long".
                                chatPage.recording = false
                                chatPage.transcribing = true
                                transcribingWatchdog.restart()
                                globalAudioRecorder.stop()
                            } else if (!chatPage.transcribing) {
                                // Start recording
                                chatPage.recordingPath = mistralApi.tempAudioPath()
                                globalAudioRecorder.outputLocation = chatPage.recordingPath
                                globalAudioRecorder.record()
                                chatPage.recording = true
                            }
                        }
                    }

                    // Recording duration label
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.bottom
                        anchors.topMargin: Theme.paddingSmall
                        visible: micButton.recording
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.errorColor
                        text: {
                            if (!micButton.recording) return ""
                            var ms = globalAudioRecorder.duration
                            var totalSec = Math.floor(ms / 1000)
                            var min = Math.floor(totalSec / 60)
                            var sec = totalSec % 60
                            return min + ":" + (sec < 10 ? "0" + sec : sec)
                        }
                    }
                }

                TextArea {
                    id: messageInput
                    width: parent.width - sendButton.width - parent.spacing * 2
                           - (attachButton.visible ? attachButton.width + parent.spacing : 0)
                           - (micButton.visible ? micButton.width + parent.spacing : 0)
                    height: Math.min(implicitHeight, Theme.itemSizeSmall * 2.5)
                    placeholderText: chatPage.recording
                                     ? qsTr("Recording... tap mic to stop")
                                     : (chatPage.transcribing
                                        ? qsTr("Transcribing...")
                                        : (chatPage.uploadingImage
                                           ? qsTr("Uploading image...")
                                           : qsTr("Type a message...")))
                    labelVisible: false
                    enabled: !mistralApi.isBusy && !chatPage.uploadingImage
                             && !chatPage.recording && !chatPage.transcribing
                             && chatPage.providerReady
                    font.pixelSize: Theme.fontSizeSmall

                    // A disabled Enter key is greyed out and does nothing, it does
                    // not fall back to a new line. In new-line mode the key stays
                    // enabled with its default icon and the handler ignores it.
                    EnterKey.enabled: !settingsManager.enterKeySends
                                      || (text.trim().length > 0 && !mistralApi.isBusy)
                    EnterKey.iconSource: settingsManager.enterKeySends
                                         ? "image://theme/icon-m-enter-accept"
                                         : "image://theme/icon-m-enter"
                    EnterKey.onClicked: {
                        if (settingsManager.enterKeySends)
                            sendMessage()
                    }
                }

                IconButton {
                    id: sendButton
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: mistralApi.isBusy
                        ? "image://theme/icon-m-pause"
                        : "image://theme/icon-m-message"
                    enabled: (!mistralApi.isBusy && !chatPage.uploadingImage && !chatPage.recording && !chatPage.transcribing && messageInput.text.trim().length > 0 && chatPage.providerReady) || mistralApi.isBusy

                    // Pulsing halo while a request is in flight
                    Rectangle {
                        id: busyHalo
                        anchors.centerIn: parent
                        width: parent.width
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: Theme.highlightColor
                        border.width: 2
                        visible: mistralApi.isBusy
                        z: -1

                        ParallelAnimation {
                            running: mistralApi.isBusy
                            loops: Animation.Infinite

                            NumberAnimation {
                                target: busyHalo
                                property: "scale"
                                from: 0.6
                                to: 1.25
                                duration: 900
                                easing.type: Easing.OutQuad
                            }
                            NumberAnimation {
                                target: busyHalo
                                property: "opacity"
                                from: 0.7
                                to: 0
                                duration: 900
                            }
                        }
                    }

                    onClicked: {
                        if (mistralApi.isBusy) {
                            mistralApi.cancelRequest()
                        } else {
                            sendMessage()
                        }
                    }
                }
            }
        }
    }

    // First launch dialog
    Dialog {
        id: firstLaunchDialog
        allowedOrientations: Orientation.All
        canAccept: true
        onAccepted: settingsManager.setFirstLaunchComplete()

        SilicaFlickable {
            anchors.fill: parent
            contentHeight: column.height

            Column {
                id: column
                width: parent.width
                spacing: Theme.paddingLarge

                DialogHeader {
                    title: qsTr("Welcome to Skrynja")
                    acceptText: qsTr("Get Started")
                }

                Icon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "image://theme/icon-l-message"
                    width: Theme.iconSizeExtraLarge
                    height: Theme.iconSizeExtraLarge
                    color: Theme.highlightColor
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Skrynja"
                    font.pixelSize: Theme.fontSizeHuge
                    color: Theme.highlightColor
                }

                SectionHeader {
                    text: qsTr("Which AI answers you?")
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: qsTr("Mistral AI. Skrynja uses Mistral's Conversations API, which is what makes web search possible. Several models are available, including free-tier ones.")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.primaryColor
                }

                SectionHeader {
                    text: qsTr("Privacy & Storage")
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: qsTr("• A copy of your conversations is stored locally on your device\n• The conversation history also lives on Mistral's servers (required by the Conversations API)\n• Your API key is stored scrambled\n• No sync with any web interface")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.primaryColor
                }

                SectionHeader {
                    text: qsTr("Getting Started")
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: qsTr("1. Paste your Mistral API key in Settings\n2. Pick a model\n3. Start chatting!")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.primaryColor
                }

                Item {
                    width: parent.width
                    height: Theme.paddingLarge
                }
            }
        }
    }

    Notification {
        id: exportNotification
        appName: "Skrynja"
    }

    Notification {
        id: errorNotification
        appName: "Skrynja"
    }

    Component {
        id: imagePickerComponent

        ImagePickerPage {
            onSelectedContentPropertiesChanged: {
                // Keep our own downscaled copy: the gallery original can be
                // deleted, and the conversation still needs the picture.
                chatPage.attachedImagePath =
                    conversationManager.retainImage(selectedContentProperties.filePath)
            }
        }
    }

    // Connections to API
    Connections {
        target: mistralApi

        // Streaming itself is handled by ConversationManager, so a response
        // keeps landing in the model even when this page has been popped.
        onMessageSent: {
            chatPage.clearRetryNotice()
            messageListView.positionViewAtEnd()
        }

        // A server-side tool (web_search) started: say so instead of a
        // generic "typing" while the model is busy elsewhere.
        onToolActivity: {
            chatPage.toolName = name
        }
        onResponseCompleted: {
            chatPage.toolName = ""
        }
        // The provider said "not now": the request goes out again on its own.
        onRateLimited: {
            chatPage.retrySeconds = seconds
            chatPage.retryAttempt = attempt
            chatPage.retryAttempts = maxAttempts
            retryCountdown.restart()
        }

        onTitleGenerated: {
            // Another page may have asked for a title on a different
            // conversation: only take the ones meant for this one.
            if (targetId !== "" && targetId !== conversationManager.currentConversationId()) {
                return
            }
            conversationManager.updateCurrentConversationTitle(title)
            conversationManager.updateCurrentConversationCategory(category)
        }
    }

    Connections {
        target: settingsManager

        onApiKeyChanged: {
            firstUse = !providerReady
        }

    }

    Connections {
        target: conversationManager

        onStreamingUpdated: {
            chatPage.clearRetryNotice()
            // Only emitted for the conversation on screen, but the page may
            // have scrolled away from the bottom
            if (chatPage.autoScroll) {
                messageListView.positionViewAtEnd()
            }
        }

        onTokenUsageChanged: {
            // A response can finish for a conversation the user has already
            // left: the banner only reports on the one being shown.
            if (conversationId !== conversationManager.currentConversationId()) {
                return
            }
            chatPage.lastPromptTokens = promptTokens
            chatPage.lastCompletionTokens = completionTokens
            var stats = conversationManager.getConversationStatistics(conversationId)
            chatPage.conversationTokens = stats.totalTokens || 0
        }

        onResponseFinished: {
            chatPage.clearRetryNotice()
            // Answers that landed in another conversation are none of this
            // page's business
            if (conversationId !== conversationManager.currentConversationId()) {
                return
            }

            if (chatPage.autoScroll) {
                messageListView.positionViewAtEnd()
            }

            // The answer landed under the user's eyes: no unread badge
            if (chatPage.status === PageStatus.Active) {
                conversationManager.markConversationRead(
                            conversationManager.currentConversationId())
            }

            // Generate title after the first exchange, once per conversation.
            // Only happens while this page is alive: a conversation left before
            // the first answer landed keeps its fallback title, and "Suggest a
            // title" is there to fix it.
            if (!chatPage.titleRequested && conversationModel.count === 2) {
                chatPage.titleRequested = true
                chatPage.labelConversation(conversationId)
            }
        }

        onCurrentConversationChanged: {
            chatPage.lastPromptTokens = 0
            chatPage.lastCompletionTokens = 0
            chatPage.titleRequested = false
            // Restore the running total instead of showing 0 for a
            // conversation that already cost something.
            var stats = conversationManager.getConversationStatistics(
                            conversationManager.currentConversationId())
            chatPage.conversationTokens = stats.totalTokens || 0
            chatPage.refreshOverrides()
            chatPage.fetchModelsIfNeeded()
            if (chatPage.status === PageStatus.Active) {
                conversationManager.markConversationRead(
                            conversationManager.currentConversationId())
            }
        }
    }

    Connections {
        target: mistralApi

        onModelsFetched: settingsManager.updateModelCache(models)

        // The image upload finished: store the file id on the user message
        // and send the turn that references it.
        onImageUploaded: {
            chatPage.uploadingImage = false
            conversationManager.setLastUserImageFileId(fileId)
            chatPage.sendTurn()
        }
        // The upload failed: fall back to an inline base64 image so the user
        // still gets an answer.
        onImageUploadFailed: {
            chatPage.uploadingImage = false
            chatPage.sendTurn()
        }
    }

    Component.onCompleted: {
        firstUse = !providerReady

        var stats = conversationManager.getConversationStatistics(
                        conversationManager.currentConversationId())
        conversationTokens = stats.totalTokens || 0
        refreshOverrides()
        fetchModelsIfNeeded()

        // Show first launch dialog after a short delay to let PageStack settle
        if (settingsManager.isFirstLaunch()) {
            firstLaunchTimer.start()
        }
    }

    // Ticks the notice down so the wait is visibly finite.
    Timer {
        id: retryCountdown
        interval: 1000
        repeat: true
        onTriggered: {
            if (chatPage.retrySeconds > 0) {
                chatPage.retrySeconds--
            } else {
                stop()
            }
        }
    }

    Timer {
        id: firstLaunchTimer
        interval: 500
        repeat: false
        onTriggered: firstLaunchDialog.open()
    }

    function fetchModelsIfNeeded() {
        if (mistralApi.isBusy) {
            return
        }
        if (settingsManager.hasApiKey && settingsManager.modelCacheStale()) {
            mistralApi.fetchModels(settingsManager.apiKey)
        }
    }

    // Naming a conversation is the only request the app makes on its own.
    // Which is why it is a setting: on a free tier, every call counts.
    function labelConversation(conversationId) {
        var mode = settingsManager.autoTitleMode
        if (mode === "off") {
            return
        }
        if (mode === "local") {
            conversationManager.autoLabelConversation(conversationId)
            return
        }

        var digest = conversationManager.conversationDigest(conversationId)
        if (digest) {
            mistralApi.generateTitle(
                        settingsManager.apiKey,
                        chatPage.activeModel, digest, conversationId)
        }
    }

    function clearRetryNotice() {
        retryCountdown.stop()
        chatPage.retrySeconds = 0
        chatPage.retryAttempt = 0
    }

    function refreshOverrides() {
        var overrides = conversationManager.getConversationOverrides(
                            conversationManager.currentConversationId())
        conversationPromptOverride = overrides.systemPrompt || ""
    }

    function effectiveSystemPrompt() {
        return conversationPromptOverride !== "" ? conversationPromptOverride
                                                 : settingsManager.systemPrompt
    }

    function dispatchRequest() {
        var content = conversationManager.pendingUserContent()
        var imagePath = conversationManager.pendingUserImagePath()
        if (content.trim().length === 0 && imagePath === "") {
            return
        }

        // F14: when an image is attached, upload it to Mistral's Files API
        // first, then send the turn with the returned file id. The image is
        // stored server-side once instead of being resent as base64 on every
        // turn of the conversation.
        if (imagePath !== ""
                && conversationManager.pendingUserImageFileId() === "") {
            uploadingImage = true
            mistralApi.uploadImage(settingsManager.apiKey, imagePath)
            return
        }

        sendTurn()
    }

    function sendTurn() {
        var content = conversationManager.pendingUserContent()
        var imagePath = conversationManager.pendingUserImagePath()
        var imageFileId = conversationManager.pendingUserImageFileId()

        autoScroll = true
        // Recorded before the call: the token statistics are attributed in C++
        conversationManager.setActiveModel(activeModel)

        // The Conversations API is stateful: the first turn creates the remote
        // conversation (model, system prompt and web search are locked there),
        // later turns only append the new message.
        mistralApi.sendMessage(settingsManager.apiKey,
                               activeModel,
                               effectiveSystemPrompt(),
                               content,
                               imagePath,
                               settingsManager.webSearch,
                               conversationManager.remoteConversationId(),
                               settingsManager.temperature,
                               settingsManager.maxTokens,
                               imageFileId)
    }

    function sendMessage() {
        var message = messageInput.text.trim()
        if (message.length === 0) return

        if (!providerReady) {
            pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            return
        }

        // An archived conversation has no remote counterpart anymore: a new
        // message starts a fresh server-side conversation.
        if (conversationManager.isArchived(
                    conversationManager.currentConversationId())) {
            conversationManager.unarchiveConversation(
                        conversationManager.currentConversationId())
        }

        var imagePath = attachedImagePath
        attachedImagePath = ""

        messageInput.text = ""
        messageInput.focus = false
        conversationModel.addUserMessage(message, imagePath)

        // Persist before the round trip: losing the question because the app
        // was closed mid-answer is worse than losing the answer.
        conversationManager.saveCurrentConversation()

        dispatchRequest()
    }

    function retryLastRequest() {
        if (mistralApi.isBusy) return

        mistralApi.clearError()
        conversationModel.removeLastMessageIfEmpty()
        dispatchRequest()
    }


    function insertPrompt(text) {
        if (!text) return
        messageInput.text = messageInput.text.length > 0
                ? messageInput.text + "\n" + text
                : text
        messageInput.focus = true
    }

    function startNewConversation() {
        // The model is fixed at creation: pick the global default now.
        conversationManager.createNewConversation(
                    settingsManager.resolveModel(settingsManager.modelName))
    }

    function openPinned() {
        conversationManager.saveCurrentConversation()
        pageStack.push(Qt.resolvedUrl("PinnedMessagesPage.qml"), { chatPage: chatPage })
    }

    function openPromptLibrary() {
        pageStack.push(Qt.resolvedUrl("PromptLibraryPage.qml"), { chatPage: chatPage })
    }

    // Same destination as swiping forward, kept in the pulley for discovery
    function openConversationSettings() {
        conversationManager.saveCurrentConversation()
        if (pageStack.nextPage(chatPage) !== null) {
            pageStack.navigateForward()
        } else {
            pageStack.push(Qt.resolvedUrl("ConversationSettingsPage.qml"))
        }
    }

    function exportCurrentConversation() {
        var path = conversationManager.exportConversation(conversationManager.currentConversationId())
        if (path !== "") {
            exportNotification.previewSummary = qsTr("Conversation exported")
            exportNotification.previewBody = path
        } else {
            exportNotification.previewSummary = qsTr("Export failed")
            exportNotification.previewBody = ""
        }
        exportNotification.publish()
    }

    // F18: Transcription result handler.
    Connections {
        target: mistralApi

        onTranscriptionReady: {
            transcribingWatchdog.stop()
            chatPage.transcribing = false
            if (text.length > 0) {
                // Fill the input field with the transcribed text. The user
                // can edit before sending.
                if (messageInput.text.length > 0) {
                    messageInput.text = messageInput.text + " " + text
                } else {
                    messageInput.text = text
                }
                messageInput.focus = true
                messageInput.cursorPosition = messageInput.text.length
            }
            // Clean up the temporary recording.
            if (chatPage.recordingPath !== "") {
                // The file is in the cache directory; it will be overwritten
                // or cleaned by the system. No explicit delete needed.
            }
        }

        onTranscriptionFailed: {
            transcribingWatchdog.stop()
            chatPage.transcribing = false
            console.warn("Transcription failed: " + message)
            errorNotification.previewSummary = qsTr("Transcription failed")
            errorNotification.previewBody = message
            errorNotification.publish()
        }
    }

    // F18: Audio recorder error handler.
    Connections {
        target: globalAudioRecorder

        onErrorOccurred: {
            chatPage.recording = false
            chatPage.transcribing = false
            console.warn("AudioRecorder error: " + globalAudioRecorder.errorString)
            errorNotification.previewSummary = qsTr("Recording failed")
            errorNotification.previewBody = globalAudioRecorder.errorString
            errorNotification.publish()
        }

        onStopped: {
            // Recording stopped and file is flushed to disk.
            // Now it is safe to read the file and transcribe.
            transcribingWatchdog.stop()
            if (chatPage.transcribing && chatPage.recordingPath !== "") {
                // The interface language is the best guess for the spoken
                // language: it tells the model which language to expect,
                // which skips detection and improves accuracy.
                var lang = settingsManager.language
                if (lang === "nb_NO") lang = "nb"

                // F18 anonymization (experimental): if enabled, process
                // the recording locally (pitch shift, optional ring
                // modulation, noise — depending on the strength preset)
                // before it leaves the device, so the speaker is harder to
                // identify from their voice. On any failure we fall back to
                // the original recording — privacy must never block
                // transcription.
                // NOTE: must call anonymizeFile() (returns error as QString),
                // NOT anonymize() — QML cannot pass a QString* parameter, so
                // anonymize() throws "Unknown method parameter type" and the
                // call never reaches C++.
                var uploadPath = chatPage.recordingPath
                if (settingsManager.voiceAnonymization) {
                    var anonPath = chatPage.recordingPath.replace(/\.wav$/i, "-anon.wav")
                    var err = voiceAnonymizer.anonymizeFile(
                                chatPage.recordingPath,
                                anonPath,
                                settingsManager.voiceAnonymizationStrength)
                    if (err === "") {
                        uploadPath = anonPath
                        console.warn("Voice anonymization applied: " + anonPath)
                    } else {
                        console.warn("Voice anonymization failed (" + err
                                     + "), sending original recording")
                        errorNotification.previewSummary = qsTr("Voice anonymization failed")
                        errorNotification.previewBody = err
                        errorNotification.publish()
                    }
                }

                mistralApi.transcribeAudio(settingsManager.apiKey,
                                            uploadPath, lang)
            }
        }
    }

    // F18: Watchdog — if "Transcribing..." lasts too long, reset the UI.
    // Covers cases where both the recorder 'stopped' signal and the HTTP
    // timeout fail to fire (e.g. QAudioRecorder hangs before stop()).
    // 120 s: transcription is a batch job; the C++ HTTP timeout (90 s)
    // fires first with a proper error, this is the last-resort backstop.
    Timer {
        id: transcribingWatchdog
        interval: 120000
        repeat: false
        onTriggered: {
            if (chatPage.transcribing) {
                console.warn("Transcribing watchdog triggered — resetting UI")
                chatPage.transcribing = false
                chatPage.recording = false
                errorNotification.previewSummary = qsTr("Transcription failed")
                errorNotification.previewBody = qsTr("The transcription took too long. Please try again.")
                errorNotification.publish()
            }
        }
    }

}
