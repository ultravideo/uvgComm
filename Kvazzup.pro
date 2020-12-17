#-------------------------------------------------
#
# Project created by QtCreator 2016-04-07T09:05:00
#
#-------------------------------------------------

QT       += core gui concurrent

message("Parsing project file.")
message("Qt version:" "$$QT_MAJOR_VERSION"."$$QT_MINOR_VERSION")

TARGET = Kvazzup

TEMPLATE = app

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
    src/media/delivery/uvgrtpreceiver.cpp \
    src/media/delivery/uvgrtpsender.cpp \
    src/media/mediamanager.cpp \
    src/media/processing/aecinputfilter.cpp \
    src/media/processing/aecprocessor.cpp \
    src/media/processing/audiocapturefilter.cpp \
    src/media/processing/audiomixerfilter.cpp \
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
    src/ui/gui/chartpainter.cpp \
    src/ui/gui/conferenceview.cpp \
    src/ui/gui/contactlist.cpp \
    src/ui/gui/contactlistitem.cpp \
    src/ui/gui/guimessage.cpp \
    src/ui/gui/statisticswindow.cpp \
    src/ui/gui/videodrawhelper.cpp \
    src/ui/gui/videoglwidget.cpp \
    src/ui/gui/videoviewfactory.cpp \
    src/ui/gui/videowidget.cpp \
    src/ui/gui/videoyuvwidget.cpp \
    src/ui/settings/audiosettings.cpp \
    src/ui/settings/camerainfo.cpp \
    src/ui/settings/microphoneinfo.cpp \
    src/ui/settings/screeninfo.cpp \
    src/ui/settings/settings.cpp \
    src/ui/settings/settingshelper.cpp \
    src/ui/settings/sipsettings.cpp \
    src/ui/settings/videosettings.cpp

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
    src/media/delivery/uvgrtpreceiver.h \
    src/media/delivery/uvgrtpsender.h \
    src/media/mediamanager.h \
    src/media/processing/aecinputfilter.h \
    src/media/processing/aecprocessor.h \
    src/media/processing/audiocapturefilter.h \
    src/media/processing/audiomixerfilter.h \
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
    src/ui/gui/chartpainter.h \
    src/ui/gui/conferenceview.h \
    src/ui/gui/contactlist.h \
    src/ui/gui/contactlistitem.h \
    src/ui/gui/guimessage.h \
    src/ui/gui/statisticswindow.h \
    src/ui/gui/videodrawhelper.h \
    src/ui/gui/videoglwidget.h \
    src/ui/gui/videointerface.h \
    src/ui/gui/videoviewfactory.h \
    src/ui/gui/videowidget.h \
    src/ui/gui/videoyuvwidget.h \
    src/ui/settings/audiosettings.h \
    src/ui/settings/camerainfo.h \
    src/ui/settings/deviceinfointerface.h \
    src/ui/settings/microphoneinfo.h \
    src/ui/settings/screeninfo.h \
    src/ui/settings/settings.h \
    src/ui/settings/settingshelper.h \
    src/ui/settings/sipsettings.h \
    src/ui/settings/videosettings.h

FORMS    += \
    ui/audiosettings.ui \
    ui/callwindow.ui \
    ui/guimessage.ui \
    ui/messagewidget.ui \
    ui/sipsettings.ui \
    ui/statisticswindow.ui \
    ui/about.ui \
    ui/settings.ui \
    ui/incomingcallwidget.ui \
    ui/outgoingcallwidget.ui \
    ui/videosettings.ui


QT += multimediawidgets
QT += network
QT += svg # for icons
QT += opengl

win32-g++: QMAKE_CXXFLAGS += -msse4.1 -mavx2 -fopenmp

# common includes
INCLUDEPATH += $$PWD/../include/openhevc_dec
INCLUDEPATH += $$PWD/../include/

# These you need to install or build yourself
LIBS += -lopus
LIBS += -lLibOpenHevcWrapper
LIBS += -lspeexdsp
LIBS += -luvgrtp

# windows build settings
win32{
  INCLUDEPATH += $$PWD/../include/uvgrtp
  INCLUDEPATH += $$PWD/../include/opus

  LIBS += -lws2_32
  LIBS += -lole32
  LIBS += -loleaut32
}


win32-msvc{
  # Note: I had problems with msvc version 10.0.18362.0
  # and had to use another version

  # static kvazaar. Untested
  #DEFINES += KVZ_STATIC_LIB

  # shared kvazaar
  DEFINES += PIC

  LIBS += -lkvazaar_lib

  # you can put your libaries here
  LIBS += -L$$PWD/../msvc_libs
  LIBS += -ladvapi32
  LIBS += -lkernel32
  message("Using MSVC libraries in ../msvc_libs")
}


win32-g++{

  LIBS += -lkvazaar
  LIBS += -fopenmp # make sure openMP is installed in your build environment
  # you can put your libaries here
  LIBS += -L$$PWD/../libs
  LIBS += -lstrmiids
  LIBS += -lssp
  message("Using MinGW libraries in ../libs")
}

unix {
  LIBS += -lkvazaar
  QMAKE_CXXFLAGS += -msse4.1 -mavx2 -fopenmp
  QMAKE_LFLAGS += -fopenmp
  INCLUDEPATH += /usr/include/opus/
  INCLUDEPATH += /usr/local/include/uvgrtp/
}

INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../


# copy assets to build folder so we have them when running from QtCreator
copyToDestination($$PWD/stylesheet.qss, $$OUT_PWD)
copyToDestination($$PWD/fonts, $$OUT_PWD/fonts)
copyToDestination($$PWD/icons, $$OUT_PWD/icons)

# deploying portable version
# Copies only Qt libraries. OpenMP is not copied.
# On windows OpenMP is located in Tools folder of Qt
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

  # Add a console window to executable so we see debug prints.
  debug {
      CONFIG += console
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
