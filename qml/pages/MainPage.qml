import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: mainPage

    property string pendingSources: ""

    ListModel {
        id: chatModel
    }

    function scrollToBottom() {
        flickable.contentY = Math.max(0, flickable.contentHeight - flickable.height)
    }

    function trimHistory() {
        while (chatModel.count > 50) {
            chatModel.remove(0)
        }
    }

    function sendMessage() {
        var text = inputField.text.trim()
        if (text.length === 0 || mistralClient.isLoading) {
            return
        }
        chatModel.append({ "role": "user", "content": text, "sources": "" })
        inputField.text = ""
        trimHistory()
        scrollToBottom()
        mistralClient.sendMessage(text)
    }

    SilicaFlickable {
        id: flickable
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: "Indstillinger"
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsDialog.qml"))
            }
        }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: "Skrynja"
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: chatModel.count === 0
                text: "Skriv en besked til Skrynja"
                color: Theme.secondaryColor
            }

            Repeater {
                model: chatModel

                Column {
                    width: column.width - Theme.paddingLarge * 2
                    x: Theme.paddingLarge
                    spacing: Theme.paddingSmall

                    Label {
                        width: parent.width
                        horizontalAlignment: role === "user" ? Text.AlignRight : Text.AlignLeft
                        text: role === "user" ? "Dig" : "Skrynja"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryHighlightColor
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.Wrap
                        horizontalAlignment: role === "user" ? Text.AlignRight : Text.AlignLeft
                        text: content
                    }
                    Label {
                        width: parent.width
                        visible: sources.length > 0
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryHighlightColor
                        text: "Kilder: " + sources
                    }
                }
            }

            Label {
                width: parent.width - Theme.paddingLarge * 2
                x: Theme.paddingLarge
                visible: mistralClient.isLoading || mistralClient.lastError.length > 0
                text: mistralClient.lastError.length > 0 ? mistralClient.lastError : "Skrynja skriver…"
                color: mistralClient.lastError.length > 0 ? Theme.errorColor : Theme.secondaryColor
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
            }

            Row {
                width: parent.width - Theme.paddingLarge * 2
                x: Theme.paddingLarge
                spacing: Theme.paddingMedium

                TextField {
                    id: inputField
                    width: parent.width - sendButton.width - parent.spacing
                    placeholderText: "Skriv dit spørgsmål…"
                }
                TextButton {
                    id: sendButton
                    text: "Send"
                    enabled: inputField.text.trim().length > 0 && !mistralClient.isLoading
                    onClicked: mainPage.sendMessage()
                }
            }
        }

        VerticalScrollDecorator {}
    }

    Connections {
        target: mistralClient
        onSourcesReceived: {
            pendingSources = sources.join(", ")
        }
        onMessageReceived: {
            chatModel.append({ "role": "assistant", "content": response, "sources": pendingSources })
            pendingSources = ""
            trimHistory()
            scrollToBottom()
        }
    }
}
