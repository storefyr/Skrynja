import QtQuick 2.0
import Sailfish.Silica 1.0

// ACP clean slate chat page (storefyr, 2026-11-02). Minimal by design: the
// message list is the local conversation model, sending goes through the
// acpClient context property (AcpClient) to the agent's session/prompt.
Page {
    id: acpPage
    objectName: "acpChatPage"
    allowedOrientations: Orientation.All

    property bool autoScroll: true

    SilicaListView {
        id: messageList
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: inputArea.top
        }
        clip: true
        model: conversationModel
        spacing: Theme.paddingSmall

        onMovementStarted: acpPage.autoScroll = false
        onMovementEnded: acpPage.autoScroll = messageList.atYEnd
        onCountChanged: if (acpPage.autoScroll) positionViewAtEnd()

        header: PageHeader {
            title: "Skrynja"
            description: acpClient.sessionId !== "" ? "ACP session aktiv" : "ACP: venter paa session..."
        }

        delegate: Column {
            width: messageList.width - 2 * Theme.horizontalPageMargin
            x: Theme.horizontalPageMargin
            spacing: Theme.paddingSmall / 2

            Label {
                width: parent.width
                text: model.role === "user" ? qsTr("Dig") : qsTr("Vibe")
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }
            Label {
                width: parent.width
                text: model.content
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: model.role === "user" ? Theme.primaryColor : Theme.highlightColor
            }
        }
    }

    Column {
        id: inputArea
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        width: parent.width

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.rgba(Theme.highlightColor, 0.4)
        }

        Row {
            width: parent.width
            spacing: Theme.paddingMedium
            anchors.horizontalCenter: parent.horizontalCenter

            TextArea {
                id: messageInput
                width: parent.width - sendButton.width - parent.spacing
                placeholderText: acpClient.sessionId !== "" ? qsTr("Skriv til Vibe...") : qsTr("Venter paa agent...")
                enabled: acpClient.sessionId !== ""
                EnterKey.onClicked: sendMessage()
            }

            Button {
                id: sendButton
                text: qsTr("Send")
                enabled: messageInput.text.trim().length > 0 && acpClient.sessionId !== ""
                onClicked: sendMessage()
            }
        }
    }

    function sendMessage() {
        var message = messageInput.text.trim()
        if (message.length === 0 || acpClient.sessionId === "") return
        conversationModel.addUserMessage(message)
        conversationManager.saveCurrentConversation()
        messageInput.text = ""
        acpClient.sendPrompt(message)
        messageList.positionViewAtEnd()
    }
}
