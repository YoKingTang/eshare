#-------------------------------------------------
#
# Project created by QtCreator 2016-07-18T17:26:59
#
#-------------------------------------------------

QT       += core gui widgets network

TARGET = eshare
TEMPLATE = app


SOURCES +=  main.cpp\
            mainwindow.cpp \
            pickreceiver.cpp \
    peerfiletransfer.cpp \
    filechunker.cpp \
    dynamictreewidgetitem.cpp

HEADERS  += mainwindow.h \
            pickreceiver.h \
    peerfiletransfer.h \
    filechunker.h \
    dynamictreewidgetitem.h

FORMS    += mainwindow.ui \
            pickreceiver.ui

DISTFILES +=

RESOURCES += resources.qrc
