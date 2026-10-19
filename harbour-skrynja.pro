TARGET = harbour-skrynja
TARGETPATH = /usr/bin

# Project configuration
QT += qml quick network
CONFIG += c++17 sailfishapp

# Source files
SOURCES += \
    src/main.cpp \
    src/mistralclient.cpp

# Headers
HEADERS += \
    src/mistralclient.h

# Resource file
RESOURCES += \
    qml.qrc

# QML files
DISTFILES += \
    qml/main.qml \
    qml/CoverPage.qml \
    qml/pages/MainPage.qml \
    harbour-skrynja.desktop \
    harbour-skrynja.yaml

# Icon files - just size directories, sailfishapp.prf will handle the rest
SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256
SAILFISHAPP_ICONS_PATH = icon
