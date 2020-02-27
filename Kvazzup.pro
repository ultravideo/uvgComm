#-------------------------------------------------
#
# Project created by QtCreator 2016-04-07T09:05:00
#
#-------------------------------------------------

QT       += core gui

message("Parsing project file.")
message("Qt version:" "$$QT_MAJOR_VERSION"."$$QT_MINOR_VERSION" "Min: 5.4")

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
    src/initiation/connectionpolicy.cpp \
    src/initiation/negotiation/connectiontester.cpp \
    src/initiation/negotiation/ice.cpp \
    src/initiation/negotiation/iceflowcontrol.cpp \
    src/initiation/negotiation/negotiation.cpp \
    src/initiation/negotiation/sdpparametermanager.cpp \
    src/initiation/negotiation/sipcontent.cpp \
    src/initiation/negotiation/stun.cpp \
    src/initiation/negotiation/stunmsg.cpp \
    src/initiation/negotiation/stunmsgfact.cpp \
    src/initiation/negotiation/udpserver.cpp \
    src/initiation/sipmanager.cpp \
    src/initiation/transaction/sipclient.cpp \
    src/initiation/transaction/sipdialog.cpp \
    src/initiation/transaction/sipdialogclient.cpp \
    src/initiation/transaction/sipdialogmanager.cpp \
    src/initiation/transaction/sipdialogstate.cpp \
    src/initiation/transaction/sipnondialogclient.cpp \
    src/initiation/transaction/sipregistrations.cpp \
    src/initiation/transaction/sipserver.cpp \
    src/initiation/transport/connectionserver.cpp \
    src/initiation/transport/sipconversions.cpp \
    src/initiation/transport/sipfieldcomposing.cpp \
    src/initiation/transport/sipfieldparsing.cpp \
    src/initiation/transport/siprouting.cpp \
    src/initiation/transport/siptransport.cpp \
    src/initiation/transport/tcpconnection.cpp \
    src/kvazzupcontroller.cpp \
    src/main.cpp \
    src/media/delivery/kvzrtp/kvzrtp.cpp \
    src/media/delivery/kvzrtp/kvzrtpreceiver.cpp \
    src/media/delivery/kvzrtp/kvzrtpsender.cpp \
    src/media/delivery/live555/framedsourcefilter.cpp \
    src/media/delivery/live555/rtpsinkfilter.cpp \
    src/media/delivery/live555/rtpstreamer.cpp \
    src/media/mediamanager.cpp \
    src/media/processing/audiocapturedevice.cpp \
    src/media/processing/audiocapturefilter.cpp \
    src/media/processing/audiooutput.cpp \
    src/media/processing/audiooutputdevice.cpp \
    src/media/processing/camerafilter.cpp \
    src/media/processing/cameraframegrabber.cpp \
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
    src/ui/gui/callwindow.cpp \
    src/ui/gui/conferenceview.cpp \
    src/ui/gui/contactlist.cpp \
    src/ui/gui/contactlistitem.cpp \
    src/ui/gui/statisticswindow.cpp \
    src/ui/gui/videodrawhelper.cpp \
    src/ui/gui/videoglwidget.cpp \
    src/ui/gui/videoviewfactory.cpp \
    src/ui/gui/videowidget.cpp \
    src/ui/gui/videoyuvwidget.cpp \
    src/ui/settings/camerainfo.cpp \
    src/ui/settings/mediasettings.cpp \
    src/ui/settings/microphoneinfo.cpp \
    src/ui/settings/settings.cpp \
    src/ui/settings/settingshelper.cpp \
    src/ui/settings/sipsettings.cpp

HEADERS  += \
    src/initiation/connectionpolicy.h \
    src/initiation/negotiation/connectiontester.h \
    src/initiation/negotiation/ice.h \
    src/initiation/negotiation/iceflowcontrol.h \
    src/initiation/negotiation/icetypes.h \
    src/initiation/negotiation/negotiation.h \
    src/initiation/negotiation/sdpparametermanager.h \
    src/initiation/negotiation/sdptypes.h \
    src/initiation/negotiation/sipcontent.h \
    src/initiation/negotiation/stun.h \
    src/initiation/negotiation/stunmsg.h \
    src/initiation/negotiation/stunmsgfact.h \
    src/initiation/negotiation/udpserver.h \
    src/initiation/sipmanager.h \
    src/initiation/siptransactionuser.h \
    src/initiation/siptypes.h \
    src/initiation/transaction/sipclient.h \
    src/initiation/transaction/sipdialog.h \
    src/initiation/transaction/sipdialogclient.h \
    src/initiation/transaction/sipdialogmanager.h \
    src/initiation/transaction/sipdialogstate.h \
    src/initiation/transaction/sipnondialogclient.h \
    src/initiation/transaction/sipregistrations.h \
    src/initiation/transaction/sipserver.h \
    src/initiation/transport/connectionserver.h \
    src/initiation/transport/sipconversions.h \
    src/initiation/transport/sipfieldcomposing.h \
    src/initiation/transport/sipfieldparsing.h \
    src/initiation/transport/siprouting.h \
    src/initiation/transport/siptransport.h \
    src/initiation/transport/tcpconnection.h \
    src/kvazzupcontroller.h \
    src/media/delivery/kvzrtp/kvzrtp.h \
    src/media/delivery/kvzrtp/kvzrtpreceiver.h \
    src/media/delivery/kvzrtp/kvzrtpsender.h \
    src/media/delivery/live555/framedsourcefilter.h \
    src/media/delivery/live555/rtpsinkfilter.h \
    src/media/delivery/live555/rtpstreamer.h \
    src/media/delivery/irtpstreamer.h \
    src/media/mediamanager.h \
    src/media/processing/audiocapturedevice.h \
    src/media/processing/audiocapturefilter.h \
    src/media/processing/audiooutput.h \
    src/media/processing/audiooutputdevice.h \
    src/media/processing/camerafilter.h \
    src/media/processing/cameraframegrabber.h \
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
    src/serverstatusview.h \
    src/statisticsinterface.h \
    src/common.h \
    src/participantinterface.h \
    src/global.h \
    src/ui/gui/callwindow.h \
    src/ui/gui/conferenceview.h \
    src/ui/gui/contactlist.h \
    src/ui/gui/contactlistitem.h \
    src/ui/gui/statisticswindow.h \
    src/ui/gui/videodrawhelper.h \
    src/ui/gui/videoglwidget.h \
    src/ui/gui/videointerface.h \
    src/ui/gui/videoviewfactory.h \
    src/ui/gui/videowidget.h \
    src/ui/gui/videoyuvwidget.h \
    src/ui/settings/camerainfo.h \
    src/ui/settings/deviceinfointerface.h \
    src/ui/settings/mediasettings.h \
    src/ui/settings/microphoneinfo.h \
    src/ui/settings/settings.h \
    src/ui/settings/settingshelper.h \
    src/ui/settings/sipsettings.h

FORMS    += \
    ui/callwindow.ui \
    ui/mediasettings.ui \
    ui/sipsettings.ui \
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
INCLUDEPATH += $$PWD/../include/kvzrtp

#LIBS += -L$$PWD/../lib32
LIBS += -L$$PWD/../lib64
LIBS += -llibkvazaar.dll
LIBS += -llibopus.dll
LIBS += -llibLibOpenHevcWrapper.dll
LIBS += -llivemedia.dll
LIBS += -llibspeexdsp.dll
LIBS += -lkvzrtp
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
INCLUDEPATH += /usr/local/include/kvzrtp/
INCLUDEPATH += /usr/local/include/kvzrtp/formats

LIBS += -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment
LIBS += -lopus -lkvazaar -lspeex -lspeexdsp -lLibOpenHevcWrapper -lgomp -lkvzrtp
}


win32: LIBS += -lws2_32
win32: LIBS += -lstrmiids
win32: LIBS += -lole32
win32: LIBS += -loleaut32


INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../


# copy assets to build folder
copydata.commands = $(COPY_DIR) $$shell_path($$PWD/stylesheet.qss) $$shell_path($$OUT_PWD) &&
copydata.commands += $(COPY_DIR) $$shell_path($$PWD/fonts) $$shell_path($$OUT_PWD/fonts) &&
copydata.commands += $(COPY_DIR) $$shell_path($$PWD/icons) $$shell_path($$OUT_PWD/icons)

first.depends = $(first) copydata
export(first.depends)
export(copydata.commands)
QMAKE_EXTRA_TARGETS += first copydata


DISTFILES += \
    .gitignore
