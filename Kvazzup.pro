#-------------------------------------------------
#
# Project created by QtCreator 2016-04-07T09:05:00
#
#-------------------------------------------------

QT       += core gui concurrent

TARGET = Kvazzup

TEMPLATE = app

# Copies the given files to the destination directory
defineTest(copyToDestination) {
    files = $$1
    win32:COPY_DIR = xcopy /s /q /y /i

    for(FILE, files) {
        DDIR = $$2

        # Replace slashes in paths with backslashes for Windows
        win32:FILE ~= s,/,\\,g
        win32:DDIR ~= s,/,\\,g
        win32:mkpath($${DDIR}) # done immediately
        QMAKE_POST_LINK +=  $${COPY_DIR} $$shell_quote($$FILE) $$shell_quote($$DDIR) $$escape_expand(\\n\\t)
    }

    export(QMAKE_POST_LINK)
}

# Qt parses this file three times. Once on a general level, once for debug and once for release
message("Parsing Kvazzup project file, Qt version" "$$QT_MAJOR_VERSION"."$$QT_MINOR_VERSION".)


INCLUDEPATH += src

SOURCES +=\
    src/initiation/connectionpolicy.cpp \
    src/initiation/negotiation/ice.cpp \
    src/initiation/negotiation/icecandidatetester.cpp \
    src/initiation/negotiation/icepairtester.cpp \
    src/initiation/negotiation/icesessiontester.cpp \
    src/initiation/negotiation/networkcandidates.cpp \
    src/initiation/negotiation/sdpnegotiation.cpp \
    src/initiation/negotiation/sdpnegotiationhelper.cpp \
    src/initiation/negotiation/sipcontent.cpp \
    src/initiation/negotiation/stunmessage.cpp \
    src/initiation/negotiation/stunmessagefactory.cpp \
    src/initiation/negotiation/udpserver.cpp \
    src/initiation/sipmanager.cpp \
    src/initiation/sipmessageflow.cpp \
    src/initiation/sipmessageprocessor.cpp \
    src/initiation/transaction/sipcallbacks.cpp \
    src/initiation/transaction/sipclient.cpp \
    src/initiation/transaction/sipdialogstate.cpp \
    src/initiation/transaction/sipserver.cpp \
    src/initiation/transport/connectionserver.cpp \
    src/initiation/transport/sipauthentication.cpp \
    src/initiation/transport/sipconversions.cpp \
    src/initiation/transport/sipfieldcomposing.cpp \
    src/initiation/transport/sipfieldcomposinghelper.cpp \
    src/initiation/transport/sipfieldparsing.cpp \
    src/initiation/transport/sipfieldparsinghelper.cpp \
    src/initiation/transport/sipmessagesanity.cpp \
    src/initiation/transport/siprouting.cpp \
    src/initiation/transport/siptransport.cpp \
    src/initiation/transport/siptransporthelper.cpp \
    src/initiation/transport/tcpconnection.cpp \
    src/kvazzupcontroller.cpp \
    src/logger.cpp \
    src/main.cpp \
    src/media/delivery/delivery.cpp \
    src/media/delivery/uvgrtpreceiver.cpp \
    src/media/delivery/uvgrtpsender.cpp \
    src/media/mediamanager.cpp \
    src/media/processing/audiocapturefilter.cpp \
    src/media/processing/audioframebuffer.cpp \
    src/media/processing/audiomixer.cpp \
    src/media/processing/audiomixerfilter.cpp \
    src/media/processing/audiooutputdevice.cpp \
    src/media/processing/audiooutputfilter.cpp \
    src/media/processing/camerafilter.cpp \
    src/media/processing/cameraframegrabber.cpp \
    src/media/processing/displayfilter.cpp \
    src/media/processing/dspfilter.cpp \
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
    src/media/processing/speexaec.cpp \
    src/media/processing/speexdsp.cpp \
    src/media/processing/yuvconversions.cpp \
    src/media/processing/yuvtorgb32.cpp \
    src/media/processing/yuyvtorgb32.cpp \
    src/media/processing/yuyvtoyuv420.cpp \
    src/media/resourceallocator.cpp \
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
    src/ui/settings/automaticsettings.cpp \
    src/ui/settings/camerainfo.cpp \
    src/ui/settings/defaultsettings.cpp \
    src/ui/settings/microphoneinfo.cpp \
    src/ui/settings/screeninfo.cpp \
    src/ui/settings/settings.cpp \
    src/ui/settings/settingshelper.cpp \
    src/ui/settings/sipsettings.cpp \
    src/ui/settings/videosettings.cpp \
    src/ui/uimanager.cpp

HEADERS  += \
    src/initiation/connectionpolicy.h \
    src/initiation/negotiation/ice.h \
    src/initiation/negotiation/icecandidatetester.h \
    src/initiation/negotiation/icepairtester.h \
    src/initiation/negotiation/icesessiontester.h \
    src/initiation/negotiation/icetypes.h \
    src/initiation/negotiation/mediacapabilities.h \
    src/initiation/negotiation/networkcandidates.h \
    src/initiation/negotiation/sdpnegotiation.h \
    src/initiation/negotiation/sdpnegotiationhelper.h \
    src/initiation/negotiation/sdptypes.h \
    src/initiation/negotiation/sipcontent.h \
    src/initiation/negotiation/stunmessage.h \
    src/initiation/negotiation/stunmessagefactory.h \
    src/initiation/negotiation/udpserver.h \
    src/initiation/sipmanager.h \
    src/initiation/sipmessageflow.h \
    src/initiation/sipmessageprocessor.h \
    src/initiation/siptypes.h \
    src/initiation/transaction/sipcallbacks.h \
    src/initiation/transaction/sipclient.h \
    src/initiation/transaction/sipdialogstate.h \
    src/initiation/transaction/sipserver.h \
    src/initiation/transport/connectionserver.h \
    src/initiation/transport/sipauthentication.h \
    src/initiation/transport/sipconversions.h \
    src/initiation/transport/sipfieldcomposing.h \
    src/initiation/transport/sipfieldcomposinghelper.h \
    src/initiation/transport/sipfieldparsing.h \
    src/initiation/transport/sipfieldparsinghelper.h \
    src/initiation/transport/sipmessagesanity.h \
    src/initiation/transport/siprouting.h \
    src/initiation/transport/siptransport.h \
    src/initiation/transport/siptransporthelper.h \
    src/initiation/transport/tcpconnection.h \
    src/kvazzupcontroller.h \
    src/logger.h \
    src/media/delivery/delivery.h \
    src/media/delivery/uvgrtpreceiver.h \
    src/media/delivery/uvgrtpsender.h \
    src/media/mediamanager.h \
    src/media/processing/audiocapturefilter.h \
    src/media/processing/audioframebuffer.h \
    src/media/processing/audiomixer.h \
    src/media/processing/audiomixerfilter.h \
    src/media/processing/audiooutputdevice.h \
    src/media/processing/audiooutputfilter.h \
    src/media/processing/camerafilter.h \
    src/media/processing/cameraframegrabber.h \
    src/media/processing/displayfilter.h \
    src/media/processing/dspfilter.h \
    src/media/processing/filter.h \
    src/media/processing/filtergraph.h \
    src/media/processing/kvazaarfilter.h \
    src/media/processing/openhevcfilter.h \
    src/media/processing/opusdecoderfilter.h \
    src/media/processing/opusencoderfilter.h \
    src/media/processing/rgb32toyuv.h \
    src/media/processing/scalefilter.h \
    src/media/processing/screensharefilter.h \
    src/media/processing/speexaec.h \
    src/media/processing/speexdsp.h \
    src/media/processing/yuvconversions.h \
    src/media/processing/yuvtorgb32.h \
    src/media/processing/yuyvtorgb32.h \
    src/media/processing/yuyvtoyuv420.h \
    src/media/resourceallocator.h \
    src/settingskeys.h \
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
    src/ui/settings/automaticsettings.h \
    src/ui/settings/camerainfo.h \
    src/ui/settings/defaultsettings.h \
    src/ui/settings/deviceinfointerface.h \
    src/ui/settings/microphoneinfo.h \
    src/ui/settings/screeninfo.h \
    src/ui/settings/settings.h \
    src/ui/settings/settingshelper.h \
    src/ui/settings/sipsettings.h \
    src/ui/settings/videosettings.h \
    src/ui/uimanager.h

FORMS    += \
    ui/audiosettings.ui \
    ui/automaticsettings.ui \
    ui/callwindow.ui \
    ui/conference/incomingcallwidget.ui \
    ui/conference/outgoingcallwidget.ui \
    ui/message/guimessage.ui \
    ui/message/messagewidget.ui \
    ui/message/stunmessage.ui \
    ui/sipsettings.ui \
    ui/statisticswindow.ui \
    ui/about.ui \
    ui/settings.ui \
    ui/videosettings.ui


QT += multimedia
QT += multimediawidgets

QT += network
QT += svg # for icons
QT += opengl

win32-g++: QMAKE_CXXFLAGS += -msse4.1 -mavx2 -fopenmp

RC_ICONS = favicon.ico

# common includes
INCLUDEPATH += $$PWD/../include/openhevc_dec
INCLUDEPATH += $$PWD/../include/

# These you need to install or build yourself. Here are the libraries that
# have the same name on each platform.
LIBS += -lopus
LIBS += -lLibOpenHevcWrapper

LIBS += -luvgrtp


# Windows configurations have optional library folders defined for easier inclusion
# The folders are different for easier building of multiple configurations. The debug
# and release folders are different because uvgRTP and Crypto++ are usually compiled as
# static libraries which means they don't work when mixing build types.

# Visual Studio
win32-msvc{
  # Note: I had problems with msvc version 10.0.18362.0
  # and had to use another version

  # you can put your libaries here
  CONFIG(debug, debug|release) {
    LIBRARY_FOLDER = -L$$PWD/../msvc_libs_dbg
  } else:CONFIG(release, debug|release) {
    LIBRARY_FOLDER = -L$$PWD/../msvc_libs
  }

  #DEFINES += KVZ_STATIC_LIB # static kvazaar. Untested
  DEFINES += PIC             # shared kvazaar

  LIBS += -lkvazaar_lib
  LIBS += -ladvapi32
  LIBS += -lkernel32

  LIBS += -llibspeexdsp

  # Needed for encryption. Can be removed if uvgRTP was compiled without
  # Crypto++ support.
  LIBS += -lcryptlib
}

# MinGW
win32-g++{
  # you can put your libaries here
  CONFIG(debug, debug|release) {
    LIBRARY_FOLDER = -L$$PWD/../libs_dbg
  } else:CONFIG(release, debug|release) {
    LIBRARY_FOLDER = -L$$PWD/../libs
  }

  LIBS += -lkvazaar
  LIBS += -fopenmp # make sure openMP is installed in your build environment

  LIBS += -lstrmiids
  LIBS += -lssp

  LIBS += -lspeexdsp

  # Needed for encryption. Can be removed if uvgRTP was compiled without
  # Crypto++ support.
  LIBS += -lcryptopp
}

# These apply to both Windows configurations
win32{
  INCLUDEPATH += $$PWD/../include/uvgrtp
  INCLUDEPATH += $$PWD/../include/opus

  # These seem to be needed
  LIBS += -lws2_32
  LIBS += -lole32
  LIBS += -loleaut32

  # Here we finally include the library folder set earlier
  LIBS += $${LIBRARY_FOLDER}
  message("Using library folder:" $${LIBRARY_FOLDER})
}


# Linux
unix {
  LIBS += -lkvazaar
  QMAKE_CXXFLAGS += -msse4.1 -mavx2 -fopenmp
  QMAKE_LFLAGS += -fopenmp
  INCLUDEPATH += /usr/include/opus/
  INCLUDEPATH += /usr/local/include/uvgrtp/

  LIBS += -lspeexdsp

  # Needed for encryption. Can be removed if uvgRTP was compiled without
  # Crypto++ support.
  LIBS += -lcryptopp
}

INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../


# copy assets to build folder so we have them when running from QtCreator
copyToDestination($$PWD/stylesheet.qss, $$OUT_PWD)
copyToDestination($$PWD/fonts, $$OUT_PWD/fonts)
copyToDestination($$PWD/icons, $$OUT_PWD/icons)

# Deploying a portable version of Kvazzup with Qt deployment script.
# Copies only Qt libraries. OpenMP is not copied.
# On windows OpenMP is located in Tools folder of Qt.

# TODO: There is a bug in this that creates a growing recursive structure and
# it eventually stops working. When that is solved, this should be enabled in release mode.
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
      DEPLOY_COMMAND = $(QTDIR)\bin\windeployqt
  }
  macx {
      DEPLOY_COMMAND = macdeployqt
  }

  # if we are in debug mode, add a console window to executable so we see debug prints.
  CONFIG(debug, debug|release) {
      CONFIG += console
  }

  DEPLOY_TARGET = $$shell_quote($$shell_path($${OUT_PWD}/$${TARGET}$${TARGET_CUSTOM_EXT}))
  OUTPUT_DIR =    $$shell_quote($$shell_path($${PWD}/portable))
  message("Enabled deployment from" $${DEPLOY_TARGET} "to" $${OUTPUT_DIR})

  # copy Kvazzup.exe
  copyToDestination($$DEPLOY_TARGET, $$OUTPUT_DIR)

  # Copy assets
  # uses OUT_PWD to avoid cyclic copy error
  copyToDestination($$OUT_PWD/stylesheet.qss, $$OUTPUT_DIR)
  copyToDestination($$PWD/fonts, $$OUTPUT_DIR/fonts)
  copyToDestination($$PWD/icons, $$OUTPUT_DIR/icons)

  # Add deploy command to after linking
  QMAKE_POST_LINK += $${DEPLOY_COMMAND} $${OUTPUT_DIR} $$escape_expand(\\n\\t)

  message("Qt dir:" $(QTDIR))

  message("QMAKE_POST_LINK after deployment:" $${QMAKE_POST_LINK})
}


DISTFILES += \
    .gitignore
