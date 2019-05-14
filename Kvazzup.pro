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
    src/media/delivery/framedsourcefilter.cpp \
    src/media/delivery/rtpsinkfilter.cpp \
    src/media/delivery/rtpstreamer.cpp \
    src/media/mediamanager.cpp \
    src/media/processing/audiocapturedevice.cpp \
    src/media/processing/audiocapturefilter.cpp \
    src/media/processing/audiooutput.cpp \
    src/media/processing/audiooutputdevice.cpp \
    src/media/processing/camerafilter.cpp \
    src/media/processing/cameraframegrabber.cpp \
    src/media/processing/camerainfo.cpp \
    src/media/processing/displayfilter.cpp \
    src/media/processing/filter.cpp \
    src/media/processing/filtergraph.cpp \
    src/media/processing/kvazaarfilter.cpp \
    src/media/processing/openhevcfilter.cpp \
    src/media/processing/opusdecoderfilter.cpp \
    src/media/processing/opusencoderfilter.cpp \
    src/media/processing/rgb32toyuv.cpp \
    src/media/processing/scalefilter.cpp \
    src/media/processing/speexaecfilter.cpp \
    src/common.cpp \
    src/media/processing/yuvtorgb32.cpp \
    src/sip/connectionserver.cpp \
    src/sip/connectiontester.cpp \
    src/sip/globalsdpstate.cpp \
    src/sip/ice.cpp \
    src/sip/iceflowcontrol.cpp \
    src/sip/sdpparametermanager.cpp \
    src/sip/stun.cpp \
    src/sip/stunmsg.cpp \
    src/sip/stunmsgfact.cpp \
    src/sip/tcpconnection.cpp \
    src/sip/udpserver.cpp \
    src/gui/callwindow.cpp \
    src/gui/conferenceview.cpp \
    src/gui/contactlist.cpp \
    src/gui/contactlistitem.cpp \
    src/gui/settings.cpp \
    src/gui/statisticswindow.cpp \
    src/gui/videowidget.cpp \
    src/sip/sipclienttransaction.cpp \
    src/sip/sipcontent.cpp \
    src/sip/sipconversions.cpp \
    src/sip/sipfieldcomposing.cpp \
    src/sip/sipfieldparsing.cpp \
    src/sip/sipservertransaction.cpp \
    src/sip/siptransport.cpp \
    src/sip/siptransactions.cpp \
    src/sip/sipdialogstate.cpp \
    src/gui/customsettings.cpp \
    src/gui/videoviewfactory.cpp \
    src/gui/videoglwidget.cpp \
    src/sip/connectionpolicy.cpp \
    src/gui/videoyuvwidget.cpp \
    src/gui/videodrawhelper.cpp \
    src/kvazzupcore.cpp \
    src/sip/sipdialogclient.cpp \
    src/sip/sipnondialogclient.cpp \
    src/gui/advancedsettings.cpp \
    src/gui/settingshelper.cpp

HEADERS  += \
    src/media/delivery/framedsourcefilter.h \
    src/media/delivery/rtpsinkfilter.h \
    src/media/delivery/rtpstreamer.h \
    src/media/mediamanager.h \
    src/media/processing/audiocapturedevice.h \
    src/media/processing/audiocapturefilter.h \
    src/media/processing/audiooutput.h \
    src/media/processing/audiooutputdevice.h \
    src/media/processing/camerafilter.h \
    src/media/processing/cameraframegrabber.h \
    src/media/processing/camerainfo.h \
    src/media/processing/displayfilter.h \
    src/media/processing/filter.h \
    src/media/processing/filtergraph.h \
    src/media/processing/kvazaarfilter.h \
    src/media/processing/openhevcfilter.h \
    src/media/processing/optimized/rgb2yuv.h \
    src/media/processing/optimized/yuv2rgb.h \
    src/media/processing/opusdecoderfilter.h \
    src/media/processing/opusencoderfilter.h \
    src/media/processing/rgb32toyuv.h \
    src/media/processing/scalefilter.h \
    src/media/processing/speexaecfilter.h \
    src/media/processing/yuvtorgb32.h \
    src/sip/connectionserver.h \
    src/sip/connectiontester.h \
    src/sip/globalsdpstate.h \
    src/sip/ice.h \
    src/sip/iceflowcontrol.h \
    src/sip/sdpparametermanager.h \
    src/sip/stun.h \
    src/sip/stunmsg.h \
    src/sip/stunmsgfact.h \
    src/sip/tcpconnection.h \
    src/sip/udpserver.h \
    src/statisticsinterface.h \
    src/common.h \
    src/participantinterface.h \
    src/gui/callwindow.h \
    src/gui/conferenceview.h \
    src/gui/contactlist.h \
    src/gui/contactlistitem.h \
    src/gui/settings.h \
    src/gui/statisticswindow.h \
    src/gui/videowidget.h \
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
    src/global.h \
    src/gui/customsettings.h \
    src/gui/videointerface.h \
    src/gui/videoviewfactory.h \
    src/gui/videoglwidget.h \
    src/sip/connectionpolicy.h \
    src/gui/videoyuvwidget.h \
    src/gui/videodrawhelper.h \
    src/kvazzupcore.h \
    src/sip/sipdialogclient.h \
    src/sip/sipnondialogclient.h \
    src/gui/advancedsettings.h \
    src/gui/settingshelper.h

FORMS    += \
    ui/advancedsettings.ui \
    ui/callwindow.ui \
    ui/customsettings.ui \
    ui/statisticswindow.ui \
    ui/about.ui \
    ui/settings.ui \
    ui/incomingcallwidget.ui \
    ui/outgoingcallwidget.ui

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

qtHaveModule(charts)
{
  QT += charts
}

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
