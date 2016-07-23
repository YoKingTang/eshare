#-------------------------------------------------
#
# Project created by QtCreator 2016-07-18T17:26:59
#
#-------------------------------------------------

QT       += core gui widgets network

TARGET = eshare
TEMPLATE = app


SOURCES +=  main.cpp \
    Listener/TransferListener.cpp \
    UI/MainWindow.cpp \
    UI/DynamicTreeWidgetItem.cpp \
    UI/DynamicTreeWidgetItemDelegate.cpp \
    UI/TransferTreeView.cpp \
    Receiver/TransferStarter.cpp \
    Chunker/Chunker.cpp \
    UI/PickReceiver.cpp

HEADERS  += \
    Data/TransferRequest.h \
    Listener/TransferListener.h \
    UI/MainWindow.h \
    UI/DynamicTreeWidgetItem.h \
    UI/DynamicTreeWidgetItemDelegate.h \
    UI/TransferTreeView.h \
    Receiver/TransferStarter.h \
    Chunker/Chunker.h \
    UI/PickReceiver.h

FORMS    += \
            UI/PickReceiver.ui \
    UI/MainWindow.ui

DISTFILES +=

RESOURCES += Resources.qrc
