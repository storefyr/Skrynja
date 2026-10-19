TARGET = skrynja

QT += qml quick network

SOURCES += src/main.cpp \
           src/mistralclient.cpp

HEADERS += src/mistralclient.h

RESOURCES += \
    qml.qrc

DISTFILES += \
    qml/*.qml \
    qml/pages/*.qml \
    skrynja.desktop \
    translations/*.ts

SAILFISHAPP_ICONS = \
    icon/86x86/skrynja.png \
    icon/108x108/skrynja.png \
    icon/128x128/skrynja.png \
    icon/172x172/skrynja.png \
    icon/256x256/skrynja.png

SAILFISHAPP_ICONS_PATH = images

# Required for QSettings
CONFIG += c++17

# Sailfish OS specific
SAILFISHAPP_ICONS = \
    icon/86x86/skrynja.png \
    icon/108x108/skrynja.png \
    icon/128x128/skrynja.png \
    icon/172x172/skrynja.png \
    icon/256x256/skrynja.png

SAILFISHAPP_ICONS_PATH = images
