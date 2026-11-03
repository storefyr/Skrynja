import QtQuick 2.0
import Sailfish.Silica 1.0

// ACP clean slate chat page (storefyr, 2026-11-02). Udvidet 2026-11-03:
// permission-dialog (session/request_permission), aktivitetslinje
// (tool_call/thought), fejl-banner og config-menu (model/thinking/mode via
// session/set_config_option — aldrig session/set_mode).
Page {
    id: acpPage
    objectName: "acpChatPage"
    allowedOrientations: Orientation.All

    property bool autoScroll: true

    function configById(cid) {
        var l = acpClient.configOptions
        for (var i = 0; i < l.length; i++)
            if (l[i].id === cid) return l[i]
        return null
    }
    function configLabel(cid, fallback) {
        var c = configById(cid)
        if (!c) return fallback
        var o = c.options || []
        for (var i = 0; i < o.length; i++)
            if (o[i].value === c.currentValue) return o[i].name || c.currentValue
        return c.currentValue
    }
    // Skift til næste værdi i option-listen (wrap rundt).
    function cycleConfig(cid) {
        var c = configById(cid)
        if (!c || !c.options || c.options.length === 0) return
        var vals = []
        for (var i = 0; i < c.options.length; i++) vals.push(c.options[i].value)
        var j = vals.indexOf(c.currentValue)
        acpClient.sendSetConfigOption(cid, vals[(j + 1) % vals.length])
    }

    SilicaListView {
        id: messageList
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: statusArea.top
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

        PullDownMenu {
            MenuItem {
                text: "Model: " + acpPage.configLabel("model", "?")
                onClicked: acpPage.cycleConfig("model")
            }
            MenuItem {
                text: "Thinking: " + acpPage.configLabel("thinking", "?")
                onClicked: acpPage.cycleConfig("thinking")
            }
            MenuItem {
                text: "Mode: " + acpPage.configLabel("mode", "?")
                onClicked: acpPage.cycleConfig("mode")
            }
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

    // Aktivitetslinje (tool-kald / tænkning) + fejl-banner over input
    Column {
        id: statusArea
        anchors {
            left: parent.left
            right: parent.right
            bottom: inputArea.top
        }
        width: parent.width

        Label {
            width: parent.width
            visible: acpClient.activity !== ""
            text: acpClient.activity
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
        Rectangle {
            width: parent.width
            height: errLabel.height + 2 * Theme.paddingSmall
            visible: acpClient.lastError !== ""
            color: Theme.rgba(Theme.errorColor, 0.2)
            Label {
                id: errLabel
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: acpClient.lastError
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.errorColor
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
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

    // Permission-dialog: viser ventende session/request_permission med de
    // målte options (allow_once, allow_session_exact, ...). Svar går tilbage
    // på samme id (målt id 0/1) via answerPermission/dismissPermission.
    Dialog {
        id: permDialog
        property var perm: acpClient.pendingPermission
        canAccept: false
        onOpened: {}
        Connections {
            target: acpClient
            onPendingPermissionChanged: {
                if (acpClient.pendingPermission.id !== undefined && acpClient.pendingPermission.id !== null)
                    permDialog.open()
            }
        }
        Column {
            width: parent.width
            spacing: Theme.paddingMedium
            DialogHeader {
                title: qsTr("Tilladelse anmodet")
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
        acpClient.lastError = ""
        acpClient.sendPrompt(message)
        messageList.positionViewAtEnd()
    }
}
