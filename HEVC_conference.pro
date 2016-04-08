#-------------------------------------------------
#
# Project created by QtCreator 2016-04-07T09:05:00
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = HEVC_conference
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    callwindow.cpp \
    cameraframegrabber.cpp \
    filter.cpp \
    filtergraph.cpp

HEADERS  += mainwindow.h \
    callwindow.h \
    cameraframegrabber.h \
    filter.h \
    filtergraph.h

FORMS    += mainwindow.ui \
    callwindow.ui


QT+=multimedia
QT+=multimediawidgets
