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

# Icon files
SAILFISHAPP_ICONS = \
    icon/86x86/harbour-skrynja.png \
    icon/108x108/harbour-skrynja.png \
    icon/128x128/harbour-skrynja.png \
    icon/172x172/harbour-skrynja.png \
    icon/256x256/harbour-skrynja.png

SAILFISHAPP_ICONS_PATH = icon

# Application name for QSettings
QMAKE_TARGET.DESTDIR = harbour-skrynja
