import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: remotePage

    property bool loading: false
    property bool loadingMessages: false
    property string pendingRemoteId: ""
    property string errorMessage: ""

    SilicaListView {
        id: remoteList
        anchors.fill: parent

        header: Column {
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Remote Conversations")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Conversations stored on Mistral's servers. Attach one to continue it here.")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !remotePage.loading && remoteModel.count > 0
                text: qsTr("%n conversation(s) on server", "", remoteModel.count)
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }

            Separator {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                color: Theme.rgba(Theme.highlightColor, 0.4)
            }
        }

        model: ListModel {
            id: remoteModel
        }

        delegate: ListItem {
            id: remoteItem
            contentHeight: contentColumn.height + 2 * Theme.paddingMedium

            property bool alreadyAttached: conversationManager.hasRemoteConversation(remoteId)

            onClicked: {
                if (alreadyAttached || remoteId === "") {
                    return
                }
                remotePage.pendingRemoteId = remoteId
                remotePage.loadingMessages = true
                mistralApi.fetchRemoteMessages(settingsManager.apiKey, remoteId)
            }

            Column {
                id: contentColumn
                anchors {
                    left: parent.left
                    right: parent.right
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                spacing: Theme.paddingSmall

                Label {
                    width: parent.width
                    text: remoteName !== "" ? remoteName : qsTr("Untitled conversation")
                    color: remoteItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    font.pixelSize: Theme.fontSizeMedium
                    truncationMode: TruncationMode.Fade
                }

                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium

                    Label {
                        text: remoteModel || ""
                        visible: text !== ""
                        color: remoteItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }

                    Label {
                        text: remotePage.formatDate(remoteUpdatedAt || remoteCreatedAt)
                        color: remoteItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }

                    Label {
                        text: remoteId ? "· " + remoteId.substring(0, 13) : ""
                        visible: remoteName === ""
                        color: remoteItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }
                }

                Label {
                    width: parent.width
                    text: alreadyAttached ? qsTr("Already attached") : qsTr("Tap to attach")
                    color: alreadyAttached ? Theme.secondaryColor
                                           : (remoteItem.highlighted ? Theme.secondaryHighlightColor : Theme.highlightColor)
                    font.pixelSize: Theme.fontSizeTiny
                    opacity: alreadyAttached ? 0.7 : 1.0
                }
            }
        }

        ViewPlaceholder {
            enabled: !remotePage.loading && remoteModel.count === 0 && remotePage.errorMessage === ""
            text: qsTr("No remote conversations")
            hintText: qsTr("Conversations you create on other devices appear here")
        }

        VerticalScrollDecorator {}
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: remotePage.loading || remotePage.loadingMessages
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.horizontalPageMargin
        horizontalAlignment: Text.AlignHCenter
        visible: remotePage.errorMessage !== ""
        text: remotePage.errorMessage
        color: Theme.errorColor
        wrapMode: Text.Wrap
    }

    Connections {
        target: mistralApi

        onRemoteConversationsFetched: {
            remotePage.loading = false
            remotePage.errorMessage = ""
            remoteModel.clear()
            for (var i = 0; i < conversations.length; ++i) {
                var c = conversations[i]
                remoteModel.append({
                    remoteId: c.id,
                    remoteName: c.name || "",
                    remoteModel: c.model || "",
                    remoteCreatedAt: c.created_at || "",
                    remoteUpdatedAt: c.updated_at || ""
                })
            }
        }

        onRemoteConversationsFetchFailed: {
            remotePage.loading = false
            remotePage.errorMessage = qsTr("Could not fetch remote conversations. Check your API key.")
        }

        onRemoteMessagesFetched: {
            remotePage.loadingMessages = false
            if (remoteConversationId !== remotePage.pendingRemoteId) {
                return
            }
            remotePage.pendingRemoteId = ""

            var title = ""
            var model = ""
            for (var i = 0; i < remoteModel.count; ++i) {
                if (remoteModel.get(i).remoteId === remoteConversationId) {
                    title = remoteModel.get(i).remoteName
                    model = remoteModel.get(i).remoteModel
                    break
                }
            }

            var localId = conversationManager.attachRemoteConversation(
                        remoteConversationId, title, model, messages)
            if (localId !== "") {
                // Pop back to the history page, which will show the new
                // conversation, then open the chat on top of it.
                pageStack.pop()
                var historyPage = pageStack.find(function(page) {
                    return page.openChat !== undefined
                })
                if (historyPage) {
                    historyPage.openChat()
                }
            } else {
                remotePage.errorMessage = qsTr("Could not attach conversation.")
            }
        }

        onRemoteMessagesFetchFailed: {
            remotePage.loadingMessages = false
            if (remoteConversationId === remotePage.pendingRemoteId) {
                remotePage.pendingRemoteId = ""
            }
            remotePage.errorMessage = qsTr("Could not fetch messages for this conversation.")
        }
    }

    Component.onCompleted: {
        remotePage.loading = true
        remotePage.errorMessage = ""
        mistralApi.fetchRemoteConversations(settingsManager.apiKey)
    }

    function formatDate(isoString) {
        if (!isoString) return ""
        var dt = new Date(isoString)
        if (isNaN(dt.getTime())) return isoString
        return Qt.formatDateTime(dt, "yyyy-MM-dd hh:mm")
    }
}
