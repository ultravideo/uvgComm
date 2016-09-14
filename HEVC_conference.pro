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
    src/framedsourcefilter.cpp \
    src/rtpsinkfilter.cpp \
    src/audiocapturefilter.cpp \
    src/audiocapturedevice.cpp \
    src/statisticswindow.cpp \
    src/audiooutput.cpp \
    src/audiooutputdevice.cpp

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
    src/framedsourcefilter.h \
    src/rtpsinkfilter.h \
    src/audiocapturefilter.h \
    src/audiocapturedevice.h \
    src/statisticswindow.h \
    src/statisticsinterface.h \
    src/audiooutput.h \
    src/audiooutputdevice.h

FORMS    += \
    ui/callwindow.ui \
    ui/mainwindow.ui \
    ui/statisticswindow.ui


QT+=multimedia
QT+=multimediawidgets

QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$PWD/../kvazaar/src
INCLUDEPATH += $$PWD/../openHEVC/gpac/modules/openhevc_dec

INCLUDEPATH += $$PWD/../live/liveMedia/include
INCLUDEPATH += $$PWD/../live/groupsock/include
INCLUDEPATH += $$PWD/../live/UsageEnvironment/include
INCLUDEPATH += $$PWD/../live/BasicUsageEnvironment/include

win32: LIBS += -L$$PWD/../
win32: LIBS += -llibkvazaar.dll
win32: LIBS += -llibLibOpenHevcWrapper.dll
win32: LIBS += -llivemedia.dll
win32: LIBS += -lws2_32

INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../
