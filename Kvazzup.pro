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


# Copies the given files to the destination directory
defineTest(copyToDestination) {
    files = $$1

    for(FILE, files) {
        DDIR = $$2

        # Replace slashes in paths with backslashes for Windows
        win32:FILE ~= s,/,\\,g
        win32:DDIR ~= s,/,\\,g
        win32:mkpath($${DDIR}) # done immediately
        QMAKE_POST_LINK += $(COPY_DIR) $$shell_quote($$FILE) $$shell_quote($$DDIR) $$escape_expand(\\n\\t)
    }

    export(QMAKE_POST_LINK)
}


INCLUDEPATH += src

SOURCES +=\
    src/initiation/connectionpolicy.cpp \
    src/initiation/negotiation/ice.cpp \
    src/initiation/negotiation/icecandidatetester.cpp \
    src/initiation/negotiation/icepairtester.cpp \
    src/initiation/negotiation/icesessiontester.cpp \
    src/initiation/negotiation/negotiation.cpp \
    src/initiation/negotiation/networkcandidates.cpp \
    src/initiation/negotiation/sdpnegotiator.cpp \
    src/initiation/negotiation/sipcontent.cpp \
    src/initiation/negotiation/stunmessage.cpp \
    src/initiation/negotiation/stunmessagefactory.cpp \
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
    src/media/delivery/delivery.cpp \
    src/media/delivery/kvzrtpreceiver.cpp \
    src/media/delivery/kvzrtpsender.cpp \
    src/media/mediamanager.cpp \
    src/media/processing/aecinputfilter.cpp \
    src/media/processing/aecplaybackfilter.cpp \
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
    src/media/processing/screensharefilter.cpp \
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
    src/ui/settings/screeninfo.cpp \
    src/ui/settings/settings.cpp \
    src/ui/settings/settingshelper.cpp \
    src/ui/settings/sipsettings.cpp

HEADERS  += \
    src/initiation/connectionpolicy.h \
    src/initiation/negotiation/ice.h \
    src/initiation/negotiation/icecandidatetester.h \
    src/initiation/negotiation/icepairtester.h \
    src/initiation/negotiation/icesessiontester.h \
    src/initiation/negotiation/icetypes.h \
    src/initiation/negotiation/mediacapabilities.h \
    src/initiation/negotiation/negotiation.h \
    src/initiation/negotiation/networkcandidates.h \
    src/initiation/negotiation/sdpnegotiator.h \
    src/initiation/negotiation/sdptypes.h \
    src/initiation/negotiation/sipcontent.h \
    src/initiation/negotiation/stunmessage.h \
    src/initiation/negotiation/stunmessagefactory.h \
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
    src/media/delivery/delivery.h \
    src/media/delivery/kvzrtpreceiver.h \
    src/media/delivery/kvzrtpsender.h \
    src/media/mediamanager.h \
    src/media/processing/aecinputfilter.h \
    src/media/processing/aecplaybackfilter.h \
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
    src/media/processing/screensharefilter.h \
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
    src/ui/settings/screeninfo.h \
    src/ui/settings/settings.h \
    src/ui/settings/settingshelper.h \
    src/ui/settings/sipsettings.h

FORMS    += \
    ui/callwindow.ui \
    ui/mediasettings.ui \
    ui/messagewidget.ui \
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

# Uses console window instead of IDE. Activates after linking.
#debug {
#    CONFIG += console
#}


INCLUDEPATH += $$PWD/../include/openhevc_dec
INCLUDEPATH += $$PWD/../include/opus
INCLUDEPATH += $$PWD/../include/

win32-msvc{
INCLUDEPATH += $$PWD/../include/kvzrtp
INCLUDEPATH += $$PWD/../include/kvzrtp/formats

LIBS += -L$$PWD/../msvc_libs
LIBS += -lLibOpenHevcWrapper
LIBS += -llibspeexdsp
LIBS += -lopus
LIBS += -lkvazaar_lib
LIBS += -lkvzrtp
message("Using Visual Studio libraries in ../msvc_libs")
}

win32-g++{
INCLUDEPATH += $$PWD/../include/kvzrtp

LIBS += -L$$PWD/../lib64
LIBS += -llibkvazaar.dll
LIBS += -llibopus.dll
LIBS += -llibLibOpenHevcWrapper.dll
LIBS += -llibspeexdsp.dll
LIBS += -lkvzrtp
LIBS += -fopenmp # TODO: Does msvc also need this?
message("Using MinGW libraries in ../libs")
}

unix {
QMAKE_CXXFLAGS += -msse4.1 -mavx2 -fopenmp

INCLUDEPATH += /usr/include/opus/
INCLUDEPATH += /usr/local/include/kvzrtp/
INCLUDEPATH += /usr/local/include/kvzrtp/formats

LIBS += -lopus
LIBS += -lkvazaar
LIBS += -lspeex
LIBS += -lspeexdsp
LIBS += -lLibOpenHevcWrapper
LIBS += -lgomp
LIBS += -lkvzrtp

message("Using Unix libraries")
}

win32: LIBS += -lws2_32
win32: LIBS += -lstrmiids
win32: LIBS += -lole32
win32: LIBS += -loleaut32

INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../


# copy assets to build folder so we have them when running from QtCreator
copyToDestination($$PWD/stylesheet.qss, $$OUT_PWD)
copyToDestination($$PWD/fonts, $$OUT_PWD/fonts)
copyToDestination($$PWD/icons, $$OUT_PWD/icons)


# deploying portable version
# Copies only Qt libararies. OpenMP is not copied.
# OpenMP is located in Tools folder of Qt
CONFIG(false){
  isEmpty(TARGET_EXT) {
      win32 {
          TARGET_CUSTOM_EXT = .exe
      }
      macx {
          TARGET_CUSTOM_EXT = .app
      }
  } else {
      TARGET_CUSTOM_EXT = $${TARGET_EXT}
  }

  win32 {
      DEPLOY_COMMAND = windeployqt
  }
  macx {
      DEPLOY_COMMAND = macdeployqt
  }

  DEPLOY_TARGET = $$shell_quote($$shell_path($${OUT_PWD}/$${TARGET}$${TARGET_CUSTOM_EXT}))
  OUTPUT_DIR =    $$shell_quote($$shell_path($${PWD}/portable))
  message("Enabled deployment to" $${OUTPUT_DIR})

  # copy Kvazzup.exe
  copyToDestination($$DEPLOY_TARGET, $$OUTPUT_DIR)

  # Copy assets
  # uses OUT_PWD to avoid cyclic copy error
  copyToDestination($$OUT_PWD/stylesheet.qss, $$OUTPUT_DIR)
  copyToDestination($$PWD/fonts, $$OUTPUT_DIR/fonts)
  copyToDestination($$PWD/icons, $$OUTPUT_DIR/icons)
  # Add deploy command to after linking
  QMAKE_POST_LINK += $${DEPLOY_COMMAND} $${OUTPUT_DIR} $$escape_expand(\\n\\t)
}


DISTFILES += \
    .gitignore
