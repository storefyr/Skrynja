TARGET = harbour-skrynja

CONFIG += sailfishapp

QT += network multimedia

# Version is passed by the spec file (%qmake5 VERSION=%{version})
isEmpty(VERSION) {
    VERSION = 0.9.8
}
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += src/harbour-skrynja.cpp \
    src/mistralapi.cpp \
    src/conversationmodel.cpp \
    src/conversationmanager.cpp \
    src/settingsmanager.cpp \
    src/securestore.cpp \
    src/categories.cpp \
    src/providers.cpp \
    src/audiorecorder.cpp \
    src/voiceanonymizer.cpp

HEADERS += src/mistralapi.h \
    src/conversationmodel.h \
    src/conversationmanager.h \
    src/settingsmanager.h \
    src/securestore.h \
    src/categories.h \
    src/providers.h \
    src/audiorecorder.h \
    src/voiceanonymizer.h

DISTFILES += qml/harbour-skrynja.qml \
    qml/cover/CoverPage.qml \
    qml/pages/ChatPage.qml \
    qml/pages/ConversationHistoryPage.qml \
    qml/pages/ConversationDetailPage.qml \
    qml/pages/ConversationSettingsPage.qml \
    qml/pages/PinnedMessagesPage.qml \
    qml/pages/PromptLibraryPage.qml \
    qml/pages/RemoteConversationsPage.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/StatsPage.qml \
    qml/components/MessageBubble.qml \
    qml/components/TypingIndicator.qml \
    qml/components/CountUpLabel.qml \
    qml/components/RatioDonut.qml \
    qml/components/CategoryChip.qml \
    qml/components/Categories.js \
    rpm/harbour-skrynja.spec \
    harbour-skrynja.desktop

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

CONFIG += sailfishapp_i18n

TRANSLATIONS += translations/harbour-skrynja-en.ts \
                translations/harbour-skrynja-da.ts
