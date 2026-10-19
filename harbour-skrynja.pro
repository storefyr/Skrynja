TARGET = harbour-skrynja

CONFIG += sailfishapp

QT += network

SOURCES += \
    src/main.cpp \
    src/mistralclient.cpp

HEADERS += \
    src/mistralclient.h

OTHER_FILES += \
    qml/main.qml \
    qml/CoverPage.qml \
    qml/pages/MainPage.qml \
    qml/pages/SettingsDialog.qml \
    harbour-skrynja.desktop \
    harbour-skrynja.yaml

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256
SAILFISHAPP_ICONS_PATH = icon
