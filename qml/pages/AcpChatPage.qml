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

    // Punkt 1: permission-dialog — viser ventende session/request_permission med
    // de målte options (allow_once, allow_session_exact, ...). Svar går tilbage på
    // samme id (målt id 0/1) via answerPermission/dismissPermission. Kø håndteres i
    // acpClient (ældste først): dialogen lukkes kun når køen er tom, ellers vises
    // næste anmodning med det samme.
    Dialog {
        id: permDialog
        property var perm: acpClient.pendingPermission
        canAccept: false
        Connections {
            target: acpClient
            onPendingPermissionChanged: {
                if (acpClient.pendingPermissionCount > 0)
                    permDialog.open()
            }
        }
        Column {
            width: parent.width
            spacing: Theme.paddingMedium
            DialogHeader {
                title: qsTr("Tilladelse anmodet") + (acpClient.pendingPermissionCount > 1 ? " (" + acpClient.pendingPermissionCount + ")" : "")
                acceptText: ""
            }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                text: permDialog.perm.title || ""
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
            }
            Repeater {
                model: permDialog.perm.options || []
                Button {
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    x: Theme.horizontalPageMargin
                    text: modelData.name || modelData.optionId
                    onClicked: {
                        acpClient.answerPermission(modelData.optionId)
                        if (acpClient.pendingPermissionCount === 0)
                            permDialog.close()
                    }
                }
            }
            Button {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                text: qsTr("Afvis")
                onClicked: {
                    acpClient.dismissPermission()
                    if (acpClient.pendingPermissionCount === 0)
                        permDialog.close()
                }
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
