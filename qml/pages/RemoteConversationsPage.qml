import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: remotePage

    property bool loading: false
    property bool loadingMessages: false
    property string pendingRemoteId: ""
    property string errorMessage: ""
    property var attachQueue: []
    property int attachTotal: 0
    property int attachDone: 0

    SilicaListView {
        id: remoteList
        anchors.fill: parent

        PullDownMenu {
            MenuItem {
                text: remotePage.attachQueue.length > 0
                      ? qsTr("Attaching %1 of %2...").arg(remotePage.attachDone).arg(remotePage.attachTotal)
                      : qsTr("Attach all")
                enabled: !remotePage.loading && !remotePage.loadingMessages
                         && remotePage.attachQueue.length === 0
                onClicked: remotePage.startAttachAll()
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: {
                    remotePage.loading = true
                    remotePage.errorMessage = ""
                    mistralApi.fetchRemoteConversations(settingsManager.apiKey)
                }
            }
        }

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
                        id: modelLabel
                        text: remoteModel || ""
                        visible: text !== ""
                        color: remoteItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }

                    Label {
                        id: dateLabel
                        text: remotePage.formatDate(remoteUpdatedAt || remoteCreatedAt)
                        color: remoteItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                    }

                    Label {
                        text: remoteId ? "· " + remoteId : ""
                        visible: remoteName === ""
                        color: remoteItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                        truncationMode: TruncationMode.Fade
                        width: parent.width - parent.spacing * 2
                               - (modelLabel.visible ? modelLabel.width : 0)
                               - (dateLabel.visible ? dateLabel.width : 0)
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
                if (remotePage.attachQueue.length > 0) {
                    // Bulk attach: keep going with the next one.
                    remotePage.attachDone++
                    remotePage.processNextInQueue()
                } else {
                    // Single attach: pop back to history and open the chat.
                    pageStack.pop()
                    var historyPage = pageStack.find(function(page) {
                        return page.openChat !== undefined
                    })
                    if (historyPage) {
                        historyPage.openChat()
                    }
                }
            } else {
                if (remotePage.attachQueue.length > 0) {
                    remotePage.attachDone++
                    remotePage.processNextInQueue()
                } else {
                    remotePage.errorMessage = qsTr("Could not attach conversation.")
                }
            }
        }

        onRemoteMessagesFetchFailed: {
            remotePage.loadingMessages = false
            if (remoteConversationId === remotePage.pendingRemoteId) {
                remotePage.pendingRemoteId = ""
            }
            if (remotePage.attachQueue.length > 0) {
                remotePage.attachDone++
                remotePage.processNextInQueue()
            } else {
                remotePage.errorMessage = qsTr("Could not fetch messages for this conversation.")
            }
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

    function startAttachAll() {
        attachQueue = []
        for (var i = 0; i < remoteModel.count; ++i) {
            var item = remoteModel.get(i)
            if (!conversationManager.hasRemoteConversation(item.remoteId)) {
                attachQueue.push(item.remoteId)
            }
        }
        attachTotal = attachQueue.length
        attachDone = 0
        if (attachQueue.length === 0) {
            return
        }
        processNextInQueue()
    }

    function processNextInQueue() {
        if (attachQueue.length === 0) {
            // All done: refresh the list so attached items show as such,
            // then pop back to history.
            attachTotal = 0
            attachDone = 0
            pageStack.pop()
            var historyPage = pageStack.find(function(page) {
                return page.openChat !== undefined
            })
            if (historyPage) {
                historyPage.refreshList()
            }
            return
        }
        var nextId = attachQueue.shift()
        pendingRemoteId = nextId
        loadingMessages = true
        mistralApi.fetchRemoteMessages(settingsManager.apiKey, nextId)
    }
}
