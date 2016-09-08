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
    UI/PickReceiver.cpp \
    UI/WaitPacking.cpp \
    RunGuard/RunGuard.cpp

HEADERS  += \
    Data/TransferRequest.h \
    Listener/TransferListener.h \
    UI/MainWindow.h \
    UI/DynamicTreeWidgetItem.h \
    UI/DynamicTreeWidgetItemDelegate.h \
    UI/TransferTreeView.h \
    Receiver/TransferStarter.h \
    Chunker/Chunker.h \
    UI/PickReceiver.h \
    UI/WaitPacking.h \
    RunGuard/RunGuard.h

FORMS    += \
            UI/PickReceiver.ui \
            UI/MainWindow.ui \
            UI/WaitPacking.ui

# Windows-only. Needed for Win32 API
win32:LIBS += -luser32

DISTFILES +=

RESOURCES += Resources.qrc
RC_FILE = Res/eshare.rc

# zlib and quazip
INCLUDEPATH += $$PWD/ThirdParty/zlib \
               $$PWD/ThirdParty/zlib/build
INCLUDEPATH += ThirdParty/quazip/quazip
debug {
    LIBS += -L$$PWD/ThirdParty/zlib/build/Debug -lzlibd
    LIBS += -L$$PWD/ThirdParty/quazip/build/Debug -lquazip
}
release {
    LIBS += -L$$PWD/ThirdParty/zlib/build/Release -lzlib
    LIBS += -L$$PWD/ThirdParty/quazip/build/Release -lquazip
}
