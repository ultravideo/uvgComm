#-------------------------------------------------
#
# Project created by QtCreator 2016-04-07T09:05:00
#
#-------------------------------------------------

QT       += core gui

message("Parsing project file. Qt version:" "$$QT_MAJOR_VERSION"."$$QT_MINOR_VERSION" ------------------------------)
message("Minimum supported Qt version:" "5.4")

greaterThan(QT_MAJOR_VERSION, 5)
{
  greaterThan(QT_MINOR_VERSION, 4)
  {
    message("Qt version is supported")
    # this is because of qopenglwidget.
  }
}

TARGET = Kvazzup

win32-g++:  TEMPLATE = app
win32-msvc: TEMPLATE = app # vcapp does not currently generate makefile

INCLUDEPATH += src

SOURCES +=\
    src/main.cpp \
    src/rtpstreamer.cpp \
    src/framedsourcefilter.cpp \
    src/rtpsinkfilter.cpp \
    src/common.cpp \
    src/mediamanager.cpp \
    src/filter.cpp \
    src/filtergraph.cpp \
    src/audio/audiocapturedevice.cpp \
    src/audio/audiocapturefilter.cpp \
    src/audio/audiooutput.cpp \
    src/audio/audiooutputdevice.cpp \
    src/video/camerafilter.cpp \
    src/video/cameraframegrabber.cpp \
    src/video/displayfilter.cpp \
    src/video/kvazaarfilter.cpp \
    src/video/openhevcfilter.cpp \
    src/audio/opusdecoderfilter.cpp \
    src/audio/opusencoderfilter.cpp \
    src/video/rgb32toyuv.cpp \
    src/audio/speexaecfilter.cpp \
    src/video/yuvtorgb32.cpp \
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
    src/sip/sipdialogstate.cpp \
    src/scalefilter.cpp \
    src/video/camerainfo.cpp \
    src/gui/customsettings.cpp \
    src/gui/videoviewfactory.cpp \
    src/gui/videoglwidget.cpp \
    src/sdpparametermanager.cpp \
    src/sip/connectionpolicy.cpp \
    src/gui/videoyuvwidget.cpp \
    src/gui/videodrawhelper.cpp \
    src/kvazzupcore.cpp \
    src/ice.cpp \
    src/iceflowcontrol.cpp \
    src/stunmsg.cpp \
    src/stunmsgfact.cpp \
    src/connectiontester.cpp \
    src/sip/sipdialogclient.cpp \
    src/sip/sipnondialogclient.cpp \
    src/gui/advancedsettings.cpp \
    src/gui/settingshelper.cpp

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
    src/audio/audiocapturedevice.h \
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
    src/sip/sipdialogstate.h \
    src/sip/sdptypes.h \
    src/scalefilter.h \
    src/global.h \
    src/video/camerainfo.h \
    src/gui/customsettings.h \
    src/gui/videointerface.h \
    src/gui/videoviewfactory.h \
    src/gui/videoglwidget.h \
    src/sdpparametermanager.h \
    src/sip/connectionpolicy.h \
    src/gui/videoyuvwidget.h \
    src/gui/videodrawhelper.h \
    src/kvazzupcore.h \
    src/ice.h \
    src/iceflowcontrol.h \
    src/stunmsg.h \
    src/stunmsgfact.h \
    src/connectiontester.h \
    src/sip/sipdialogclient.h \
    src/sip/sipnondialogclient.h \
    src/gui/advancedsettings.h \
    src/gui/settingshelper.h

FORMS    += \
    ui/callwindow.ui \
    ui/statisticswindow.ui \
    ui/about.ui \
    ui/advancedSettings.ui \
    ui/settings.ui \
    ui/incomingcallwidget.ui \
    ui/outgoingcallwidget.ui \
    ui/customSettings.ui

# just in case we sometimes like to support smaller qt versions.
greaterThan(4, QT_MAJOR_VERSION)
{
  QT += widgets
  QT += multimedia
}

QT+=multimediawidgets
QT+=network
QT+=svg # for icons

QT += opengl

#win32-g++: QMAKE_CXXFLAGS += -std=c++11 -fopenmp
win32-g++: QMAKE_CXXFLAGS += -msse4.1 -mavx2 -fopenmp

# may need a rebuild

#debug {
#    CONFIG += console
#}

INCLUDEPATH += $$PWD/../include/openhevc_dec
INCLUDEPATH += $$PWD/../include/opus
INCLUDEPATH += $$PWD/../include/


win32-msvc{
INCLUDEPATH += $$PWD/../include/live666/liveMedia/include
INCLUDEPATH += $$PWD/../include/live666/groupsock/include
INCLUDEPATH += $$PWD/../include/live666/UsageEnvironment/include
INCLUDEPATH += $$PWD/../include/live666/BasicUsageEnvironment/include

LIBS += -L$$PWD/../msvc_libs
LIBS += -llive666
LIBS += -lLibOpenHevcWrapper
LIBS += -llibspeexdsp
LIBS += -lopus
LIBS += -lkvazaar_lib
message("Using Visual Studio libraries in ../msvc_libs")
}

win32-g++{
INCLUDEPATH += $$PWD/../include/live/liveMedia/include
INCLUDEPATH += $$PWD/../include/live/groupsock/include
INCLUDEPATH += $$PWD/../include/live/UsageEnvironment/include
INCLUDEPATH += $$PWD/../include/live/BasicUsageEnvironment/include
#LIBS += -L$$PWD/../lib32
LIBS += -L$$PWD/../lib64
LIBS += -llibkvazaar.dll
LIBS += -llibopus.dll
LIBS += -llibLibOpenHevcWrapper.dll
LIBS += -llivemedia.dll
LIBS += -llibspeexdsp.dll
LIBS += -fopenmp # TODO: Does msvc also need this?
message("Using MinGW libraries in ../libs")
}

unix {
QMAKE_CXXFLAGS += -msse4.1 -mavx2 -fopenmp

INCLUDEPATH += /usr/local/include/BasicUsageEnvironment/
INCLUDEPATH += /usr/local/include/UsageEnvironment/
INCLUDEPATH += /usr/local/include/groupsock/
INCLUDEPATH += /usr/local/include/liveMedia/
INCLUDEPATH += /usr/include/opus/

LIBS += -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment
LIBS += -lopus -lkvazaar -lspeex -lspeexdsp -lLibOpenHevcWrapper -lgomp
}


win32: LIBS += -lws2_32
win32: LIBS += -lstrmiids
win32: LIBS += -lole32
win32: LIBS += -loleaut32


INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../

DISTFILES += \
    .gitignore
