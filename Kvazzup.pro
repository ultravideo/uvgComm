#-------------------------------------------------
#
# Project created by QtCreator 2016-04-07T09:05:00
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Kvazzup
TEMPLATE = app

INCLUDEPATH += src

SOURCES +=\
    src/main.cpp \
    src/rtpstreamer.cpp \
    src/framedsourcefilter.cpp \
    src/rtpsinkfilter.cpp \
    src/common.cpp \
    src/mediamanager.cpp \
    src/callmanager.cpp \
    src/filter.cpp \
    src/filtergraph.cpp \
    src/audio/audiocapturedevice.cpp \
    src/audio/audiocapturefilter.cpp \
    src/audio/audiooutput.cpp \
    src/audio/audiooutputdevice.cpp \
    src/video/camerafilter.cpp \
    src/video/cameraframegrabber.cpp \
    src/video/displayfilter.cpp \
    src/video/dshowcamerafilter.cpp \
    src/video/kvazaarfilter.cpp \
    src/video/openhevcfilter.cpp \
    src/audio/opusdecoderfilter.cpp \
    src/audio/opusencoderfilter.cpp \
    src/video/rgb32toyuv.cpp \
    src/audio/speexaecfilter.cpp \
    src/video/yuvtorgb32.cpp \
    src/video/dshow/capture.cpp \
    src/connection.cpp \
    src/connectionserver.cpp \
    src/sipstringcomposer.cpp \
    src/gui/callwindow.cpp \
    src/gui/conferenceview.cpp \
    src/gui/contactlist.cpp \
    src/gui/contactlistitem.cpp \
    src/gui/settings.cpp \
    src/gui/statisticswindow.cpp \
    src/gui/videowidget.cpp \
    src/udpserver.cpp \
    src/stun.cpp \
    src/sipmanager.cpp \
    src/siprouting.cpp \
    src/globalsdpstate.cpp \
    src/sipsession.cpp \
    src/sipconnection.cpp

HEADERS  += \
    src/filter.h \
    src/filtergraph.h \
    src/rtpstreamer.h \
    src/framedsourcefilter.h \
    src/rtpsinkfilter.h \
    src/statisticsinterface.h \
    src/common.h \
    src/mediamanager.h \
    src/participantinterface.h \
    src/callmanager.h \
    src/video/dshow/capture_interface.h \
    src/video/dshow/SampleGrabber.h \
    src/audio/audiocapturedevice.h \
    src/video/dshow/capture.h \
    src/video/cameraframegrabber.h \
    src/video/openhevcfilter.h \
    src/audio/opusdecoderfilter.h \
    src/audio/opusencoderfilter.h \
    src/video/rgb32toyuv.h \
    src/video/yuvtorgb32.h \
    src/audio/speexaecfilter.h \
    src/audio/audiocapturefilter.h \
    src/audio/audiooutput.h \
    src/audio/audiooutputdevice.h \
    src/video/dshowcamerafilter.h \
    src/video/camerafilter.h \
    src/video/kvazaarfilter.h \
    src/video/optimized/rgb2yuv.h \
    src/video/optimized/yuv2rgb.h \
    src/video/displayfilter.h \
    src/connection.h \
    src/connectionserver.h \
    src/sipstringcomposer.h \
    src/gui/callwindow.h \
    src/gui/conferenceview.h \
    src/gui/contactlist.h \
    src/gui/contactlistitem.h \
    src/gui/settings.h \
    src/gui/statisticswindow.h \
    src/gui/videowidget.h \
    src/udpserver.h \
    src/stun.h \
    src/sipmanager.h \
    src/siprouting.h \
    src/globalsdpstate.h \
    src/siptypes.h \
    src/sipsession.h \
    src/sipconnection.h

FORMS    += \
    ui/callwindow.ui \
    ui/statisticswindow.ui \
    ui/callingwidget.ui \
    ui/about.ui \
    ui/advancedSettings.ui \
    ui/basicSettings.ui


QT+=multimedia
QT+=multimediawidgets
QT+=network
QT += svg

QMAKE_CXXFLAGS += -std=c++11
QMAKE_CXXFLAGS += -msse4.1 -mavx2

#CONFIG += console

INCLUDEPATH += $$PWD/../include/openhevc_dec
INCLUDEPATH += $$PWD/../include/opus
INCLUDEPATH += $$PWD/../include/live/liveMedia/include
INCLUDEPATH += $$PWD/../include/live/groupsock/include
INCLUDEPATH += $$PWD/../include/live/UsageEnvironment/include
INCLUDEPATH += $$PWD/../include/live/BasicUsageEnvironment/include
INCLUDEPATH += $$PWD/../include/

win32: LIBS += -L$$PWD/../libs
win32: LIBS += -llibkvazaar.dll
win32: LIBS += -llibopus.dll
win32: LIBS += -llibLibOpenHevcWrapper.dll
win32: LIBS += -llivemedia.dll
win32: LIBS += -llibspeexdsp.dll
win32: LIBS += -lws2_32

win32: LIBS += -lstrmiids
win32: LIBS += -lole32
win32: LIBS += -loleaut32


INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../

DISTFILES += \
    .gitignore
