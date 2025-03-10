#-------------------------------------------------
#
# Project created by QtCreator 2018-05-20T03:20:24
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = TinyScreenshoter
TEMPLATE = app

DESTDIR += $$PWD/../bin

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += static
RC_FILE = res/res.rc

LIBS += -static-libgcc -static-libstdc++ -static -pthread

INCLUDEPATH += src/ ../lib/

SOURCES += \
        src/main.cpp \
        src/tiny_screenshoter.cpp

HEADERS += \
        src/tiny_screenshoter.h

win32:{
    DEFINES += SPNG_STATIC SPNG_SSE=0 SPNG_USE_MINIZ
    SOURCES += \
            ../lib/spng.c \
            ../lib/miniz.c

    HEADERS += \
            ../lib/spng.h \
            ../lib/miniz.h
}

FORMS += \
        src/tiny_screenshoter.ui

RESOURCES += \
    res/res.qrc
