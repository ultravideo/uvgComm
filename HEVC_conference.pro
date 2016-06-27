#-------------------------------------------------
#
# Project created by QtCreator 2016-04-07T09:05:00
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = HEVC_conference
TEMPLATE = app

INCLUDEPATH += src

SOURCES +=\
    src/callwindow.cpp \
    src/camerafilter.cpp \
    src/cameraframegrabber.cpp \
    src/displayfilter.cpp \
    src/filter.cpp \
    src/filtergraph.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/videowidget.cpp \
    src/kvazaarfilter.cpp \
    src/rgb32toyuv.cpp \
    src/openhevcfilter.cpp \
    src/yuvtorgb32.cpp \
    src/rtpstreamer.cpp \
    src/framedsourcepile.cpp

HEADERS  += \
    src/callwindow.h \
    src/camerafilter.h \
    src/cameraframegrabber.h \
    src/displayfilter.h \
    src/filter.h \
    src/filtergraph.h \
    src/mainwindow.h \
    src/videowidget.h \
    src/kvazaarfilter.h \
    src/rgb32toyuv.h \
    src/openhevcfilter.h \
    src/yuvtorgb32.h \
    src/rtpstreamer.h \
    src/framedsourcepile.h

FORMS    += \
    ui/callwindow.ui \
    ui/mainwindow.ui


QT+=multimedia
QT+=multimediawidgets

QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$PWD/../kvazaar/src
INCLUDEPATH += $$PWD/../openHEVC/gpac/modules/openhevc_dec

INCLUDEPATH += $$PWD/../live/liveMedia/include
INCLUDEPATH += $$PWD/../live/groupsock/include
INCLUDEPATH += $$PWD/../live/UsageEnvironment/include
INCLUDEPATH += $$PWD/../live/BasicUsageEnvironment/include

win32: LIBS += -L$$PWD/../ -llibkvazaar.dll
win32: LIBS += -llibLibOpenHevcWrapper.dll
win32: LIBS += -llivemedia.dll

INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../
