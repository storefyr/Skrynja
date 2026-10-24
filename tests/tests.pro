TEMPLATE = app
TARGET = tst_skrynja

QT += core network gui multimedia testlib
CONFIG += c++11 console
CONFIG -= app_bundle

INCLUDEPATH += ../src

SOURCES += tst_main.cpp \
    ../src/audiorecorder.cpp \
    ../src/conversationmodel.cpp \
    ../src/conversationmanager.cpp \
    ../src/mistralapi.cpp \
    ../src/settingsmanager.cpp \
    ../src/securestore.cpp \
    ../src/categories.cpp \
    ../src/providers.cpp \
    ../src/voiceanonymizer.cpp

HEADERS += ../src/audiorecorder.h \
    ../src/conversationmodel.h \
    ../src/conversationmanager.h \
    ../src/mistralapi.h \
    ../src/settingsmanager.h \
    ../src/securestore.h \
    ../src/categories.h \
    ../src/providers.h \
    ../src/voiceanonymizer.h
