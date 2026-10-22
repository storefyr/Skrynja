TARGET = harbour-skrynja

CONFIG += sailfishapp

QT += network multimedia

# Version is passed by the spec file (%qmake5 VERSION=%{version})
isEmpty(VERSION) {
    VERSION = 0.8.13
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
    src/audiorecorder.cpp

HEADERS += src/mistralapi.h \
    src/conversationmodel.h \
    src/conversationmanager.h \
    src/settingsmanager.h \
    src/securestore.h \
    src/categories.h \
    src/providers.h \
    src/audiorecorder.h

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
                translations/harbour-skrynja-da.ts \
                translations/harbour-skrynja-fr.ts \
                translations/harbour-skrynja-de.ts \
                translations/harbour-skrynja-es.ts \
                translations/harbour-skrynja-fi.ts \
                translations/harbour-skrynja-it.ts \
                translations/harbour-skrynja-nb_NO.ts \
                translations/harbour-skrynja-sv.ts
