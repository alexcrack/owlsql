#-------------------------------------------------
#
# Project created by QtCreator 2018-02-19T14:52:45
#
#-------------------------------------------------

CONFIG   += c++14
QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = owlsql
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        mainwindow/mainwindow.cpp \
    session_manager/sessionmanager.cpp \
    session_manager/dbcombobox.cpp \
    session_manager/sessiondelegate.cpp \
    session_manager/sessionstreemodel.cpp \
    session_manager/treeitem.cpp \
    session_manager/foldertreeitem.cpp \
    session_manager/sessiontreeitem.cpp \
    session_manager/isdirtydatawidgetmapper.cpp

HEADERS += \
        mainwindow/mainwindow.h \
    consts.h \
    session_manager/sessionmanager.h \
    session_manager/dbcombobox.h \
    session_manager/sessiondelegate.h \
    session_manager/sessionstreemodel.h \
    session_manager/treeitem.h \
    session_manager/foldertreeitem.h \
    session_manager/sessiontreeitem.h \
    session_manager/isdirtydatawidgetmapper.h

FORMS += \
        mainwindow/mainwindow.ui \
    session_manager/sessionmanager.ui

RESOURCES += \
    res/icons.qrc \
    res/simpletreemodel.qrc
