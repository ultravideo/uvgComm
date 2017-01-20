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
    src/audiooutputdevice.cpp \
    src/opusencoderfilter.cpp \
    src/opusdecoderfilter.cpp \
    src/speexaecfilter.cpp \
    src/callmanager.cpp \
    src/callnegotiation.cpp \
    src/sipstringcomposer.cpp \
    src/networksender.cpp \
    src/networkreceiver.cpp

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
    src/audiooutputdevice.h \
    src/opusencoderfilter.h \
    src/opusdecoderfilter.h \
    src/speexaecfilter.h \
    src/callmanager.h \
    src/callnegotiation.h \
    src/sipstringcomposer.h \
    src/networksender.h \
    src/networkreceiver.h

FORMS    += \
    ui/callwindow.ui \
    ui/mainwindow.ui \
    ui/statisticswindow.ui


QT+=multimedia
QT+=multimediawidgets
QT+=network

QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$PWD/../include/kvazaar/src
INCLUDEPATH += $$PWD/../include/openHEVC/gpac/modules/openhevc_dec
INCLUDEPATH += $$PWD/../include/opus/include
INCLUDEPATH += $$PWD/../include/libosip2/include
INCLUDEPATH += $$PWD/../include/live/liveMedia/include
INCLUDEPATH += $$PWD/../include/live/groupsock/include
INCLUDEPATH += $$PWD/../include/live/UsageEnvironment/include
INCLUDEPATH += $$PWD/../include/live/BasicUsageEnvironment/include

win32: LIBS += -L$$PWD/../libs
win32: LIBS += -llibkvazaar.dll
win32: LIBS += -llibopus.dll
win32: LIBS += -llibLibOpenHevcWrapper.dll
win32: LIBS += -llivemedia.dll
win32: LIBS += -llibspeexdsp.dll
win32: LIBS += -lws2_32
win32: LIBS += -llibosip2.dll

INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../

DISTFILES += \
    .gitignore
