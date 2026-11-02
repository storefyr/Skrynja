import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow {
    id: appWindow

    // ACP clean slate (storefyr, 2026-11-02): the app opens on the ACP chat
    // page. The legacy ChatPage (Conversations API) is not part of this build.
    initialPage: Component { ConversationHistoryPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    Component.onCompleted: {
        pageStack.push(Qt.resolvedUrl("pages/AcpChatPage.qml"), {},
                       PageStackAction.Immediate)
    }

    // The app can be killed while in the background without further warning,
    // so flush the conversation as soon as it stops being the active window.
    Connections {
        target: Qt.application
        onStateChanged: {
            if (Qt.application.state !== Qt.ApplicationActive) {
                conversationManager.saveCurrentConversation()
            }
        }
    }
}
