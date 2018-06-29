#-------------------------------------------------
#
# Project created by QtCreator 2016-04-07T09:05:00
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Kvazzup

win32-g++:   TEMPLATE = app
win32-msvc: TEMPLATE = vcapp


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
    src/connectionserver.cpp \
    src/gui/callwindow.cpp \
    src/gui/conferenceview.cpp \
    src/gui/contactlist.cpp \
    src/gui/contactlistitem.cpp \
    src/gui/settings.cpp \
    src/gui/statisticswindow.cpp \
    src/gui/videowidget.cpp \
    src/udpserver.cpp \
    src/stun.cpp \
    src/globalsdpstate.cpp \
    src/tcpconnection.cpp \
    src/sip/sipclienttransaction.cpp \
    src/sip/sipcontent.cpp \
    src/sip/sipconversions.cpp \
    src/sip/sipfieldcomposing.cpp \
    src/sip/sipfieldparsing.cpp \
    src/sip/sipservertransaction.cpp \
    src/sip/siptransport.cpp \
    src/sip/siptransactions.cpp \
    src/sip/sipregistration.cpp \
    src/sip/sipdialogstate.cpp \
    src/scalefilter.cpp

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
    src/connectionserver.h \
    src/gui/callwindow.h \
    src/gui/conferenceview.h \
    src/gui/contactlist.h \
    src/gui/contactlistitem.h \
    src/gui/settings.h \
    src/gui/statisticswindow.h \
    src/gui/videowidget.h \
    src/udpserver.h \
    src/stun.h \
    src/globalsdpstate.h \
    src/tcpconnection.h \
    src/sip/sipclienttransaction.h \
    src/sip/sipfieldcomposing.h \
    src/sip/sipfieldparsing.h \
    src/sip/sipcontent.h \
    src/sip/sipconversions.h \
    src/sip/sipservertransaction.h \
    src/sip/siptransactionuser.h \
    src/sip/siptransport.h \
    src/sip/siptypes.h \
    src/sip/siptransactions.h \
    src/sip/sipregistration.h \
    src/sip/sipdialogstate.h \
    src/sip/sdptypes.h \
    src/scalefilter.h

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
QT+=svg

#win32-g++: QMAKE_CXXFLAGS += -std=c++11 -fopenmp
win32: QMAKE_CXXFLAGS += -msse4.1 -mavx2

# may need a rebuild
#CONFIG += console

INCLUDEPATH += $$PWD/../include/openhevc_dec
INCLUDEPATH += $$PWD/../include/opus
INCLUDEPATH += $$PWD/../include/live/liveMedia/include
INCLUDEPATH += $$PWD/../include/live/groupsock/include
INCLUDEPATH += $$PWD/../include/live/UsageEnvironment/include
INCLUDEPATH += $$PWD/../include/live/BasicUsageEnvironment/include
INCLUDEPATH += $$PWD/../include/


win32-msvc{
LIBS += -L$$PWD/../msvc_libs
message("Using Visual Studio libraries")
}

win32-g++{
LIBS += -L$$PWD/../libs
LIBS += -llibkvazaar.dll
LIBS += -llibopus.dll
LIBS += -llibLibOpenHevcWrapper.dll
LIBS += -llivemedia.dll
LIBS += -llibspeexdsp.dll
LIBS += -fopenmp # TODO: Does msvc need this?
message("Using MinGW libraries")
}


win32: LIBS += -lws2_32

win32: LIBS += -lstrmiids
win32: LIBS += -lole32
win32: LIBS += -loleaut32


INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../

DISTFILES += \
    .gitignore

message("Qt project read")
