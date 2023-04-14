cmake_minimum_required(VERSION 3.16)

include(cmake/ProjectDetails.cmake)
project(kvazzup
    VERSION ${kvazzup_VER}
    DESCRIPTION ${kvazzup_DESCR}
    HOMEPAGE_URL ${kvazzup_URL}
    LANGUAGES CXX)


include(GNUInstallDirs)

include(dependencies/cryptopp.cmake)
include(dependencies/uvgrtp.cmake)
include(dependencies/openhevc.cmake)
include(dependencies/kvazaar.cmake)
include(dependencies/opus.cmake)
include(dependencies/speexdsp.cmake)

include(cmake/Versioning.cmake)

#option(ENABLE_SRTP    "Enables media encryption" ON)
#option(ENABLE_PRINTS  "Print Debug prints to command line" ON)
#option(ENABLE_LOGGING "Save log to file" ON)
#option(ENABLE_WERROR  "Fail with compiler warnings" OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Concurrent Gui Multimedia MultimediaWidgets Network OpenGL Svg Widgets)

qt_standard_project_setup()

qt_add_executable(Kvazzup WIN32 MACOSX_BUNDLE
    src/common.cpp src/common.h
    src/global.h
    src/icetypes.h
    src/initiation/connectionpolicy.cpp src/initiation/connectionpolicy.h
    src/initiation/negotiation/networkcandidates.cpp src/initiation/negotiation/networkcandidates.h
    src/initiation/negotiation/sdpdefault.cpp src/initiation/negotiation/sdpdefault.h
    src/initiation/negotiation/sdpice.cpp src/initiation/negotiation/sdpice.h
    src/initiation/negotiation/sdpnegotiation.cpp src/initiation/negotiation/sdpnegotiation.h
    src/initiation/negotiation/sdptypes.h
    src/initiation/negotiation/sipcontent.cpp src/initiation/negotiation/sipcontent.h
    src/initiation/siphelper.cpp src/initiation/siphelper.h
    src/initiation/sipmanager.cpp src/initiation/sipmanager.h
    src/initiation/sipmessageflow.cpp src/initiation/sipmessageflow.h
    src/initiation/sipmessageprocessor.cpp src/initiation/sipmessageprocessor.h
    src/initiation/siptypes.h
    src/initiation/transaction/sipallow.cpp src/initiation/transaction/sipallow.h
    src/initiation/transaction/sipcallbacks.cpp src/initiation/transaction/sipcallbacks.h
    src/initiation/transaction/sipclient.cpp src/initiation/transaction/sipclient.h
    src/initiation/transaction/sipdialogstate.cpp src/initiation/transaction/sipdialogstate.h
    src/initiation/transaction/sipserver.cpp src/initiation/transaction/sipserver.h
    src/initiation/transport/connectionserver.cpp src/initiation/transport/connectionserver.h
    src/initiation/transport/sipauthentication.cpp src/initiation/transport/sipauthentication.h
    src/initiation/transport/sipconversions.cpp src/initiation/transport/sipconversions.h
    src/initiation/transport/sipfieldcomposing.cpp src/initiation/transport/sipfieldcomposing.h
    src/initiation/transport/sipfieldcomposinghelper.cpp src/initiation/transport/sipfieldcomposinghelper.h
    src/initiation/transport/sipfieldparsing.cpp src/initiation/transport/sipfieldparsing.h
    src/initiation/transport/sipfieldparsinghelper.cpp src/initiation/transport/sipfieldparsinghelper.h
    src/initiation/transport/sipmessagesanity.cpp src/initiation/transport/sipmessagesanity.h
    src/initiation/transport/siprouting.cpp src/initiation/transport/siprouting.h
    src/initiation/transport/siptransport.cpp src/initiation/transport/siptransport.h
    src/initiation/transport/siptransporthelper.cpp src/initiation/transport/siptransporthelper.h
    src/initiation/transport/tcpconnection.cpp src/initiation/transport/tcpconnection.h
    src/kvazzupcontroller.cpp src/kvazzupcontroller.h
    src/logger.cpp src/logger.h
    src/main.cpp
    src/media/delivery/delivery.cpp src/media/delivery/delivery.h
    src/media/delivery/ice.cpp src/media/delivery/ice.h
    src/media/delivery/icecandidatetester.cpp src/media/delivery/icecandidatetester.h
    src/media/delivery/icepairtester.cpp src/media/delivery/icepairtester.h
    src/media/delivery/icesessiontester.cpp src/media/delivery/icesessiontester.h
    src/media/delivery/uvgrtpreceiver.cpp src/media/delivery/uvgrtpreceiver.h
    src/media/delivery/uvgrtpsender.cpp src/media/delivery/uvgrtpsender.h
    src/media/mediamanager.cpp src/media/mediamanager.h
    src/media/processing/audiocapturefilter.cpp src/media/processing/audiocapturefilter.h
    src/media/processing/audioframebuffer.cpp src/media/processing/audioframebuffer.h
    src/media/processing/audiomixer.cpp src/media/processing/audiomixer.h
    src/media/processing/audiomixerfilter.cpp src/media/processing/audiomixerfilter.h
    src/media/processing/audiooutputdevice.cpp src/media/processing/audiooutputdevice.h
    src/media/processing/audiooutputfilter.cpp src/media/processing/audiooutputfilter.h
    src/media/processing/camerafilter.cpp src/media/processing/camerafilter.h
    src/media/processing/displayfilter.cpp src/media/processing/displayfilter.h
    src/media/processing/dspfilter.cpp src/media/processing/dspfilter.h
    src/media/processing/filter.cpp src/media/processing/filter.h
    src/media/processing/filtergraph.cpp src/media/processing/filtergraph.h
    src/media/processing/halfrgbfilter.cpp src/media/processing/halfrgbfilter.h
    src/media/processing/kvazaarfilter.cpp src/media/processing/kvazaarfilter.h
    src/media/processing/openhevcfilter.cpp src/media/processing/openhevcfilter.h
    src/media/processing/opusdecoderfilter.cpp src/media/processing/opusdecoderfilter.h
    src/media/processing/opusencoderfilter.cpp src/media/processing/opusencoderfilter.h
    src/media/processing/rgb32toyuv.cpp src/media/processing/rgb32toyuv.h
    src/media/processing/roimanualfilter.cpp src/media/processing/roimanualfilter.h
    src/media/processing/scalefilter.cpp src/media/processing/scalefilter.h
    src/media/processing/screensharefilter.cpp src/media/processing/screensharefilter.h
    src/media/processing/speexaec.cpp src/media/processing/speexaec.h
    src/media/processing/speexdsp.cpp src/media/processing/speexdsp.h
    src/media/processing/yuvconversions.cpp src/media/processing/yuvconversions.h
    src/media/processing/yuvtorgb32.cpp src/media/processing/yuvtorgb32.h
    src/media/processing/yuyvtorgb32.cpp src/media/processing/yuyvtorgb32.h
    src/media/processing/yuyvtoyuv420.cpp src/media/processing/yuyvtoyuv420.h
    src/media/resourceallocator.cpp src/media/resourceallocator.h
    src/participantinterface.h
    src/settingskeys.h
    src/statisticsinterface.h
    src/stunmessage.cpp src/stunmessage.h
    src/stunmessagefactory.cpp src/stunmessagefactory.h
    src/udpserver.cpp src/udpserver.h
    src/ui/about.ui
    src/ui/gui/avatarholder.ui
    src/ui/gui/avatarimage.cpp src/ui/gui/avatarimage.h
    src/ui/gui/callwindow.cpp src/ui/gui/callwindow.h src/ui/gui/callwindow.ui
    src/ui/gui/chartpainter.cpp src/ui/gui/chartpainter.h
    src/ui/gui/conferenceview.cpp src/ui/gui/conferenceview.h
    src/ui/gui/contactlist.cpp src/ui/gui/contactlist.h
    src/ui/gui/contactlistitem.cpp src/ui/gui/contactlistitem.h
    src/ui/gui/guimessage.cpp src/ui/gui/guimessage.h src/ui/gui/guimessage.ui
    src/ui/gui/incomingcallwidget.ui
    src/ui/gui/messagewidget.ui
    src/ui/gui/outgoingcallwidget.ui
    src/ui/gui/statisticswindow.cpp src/ui/gui/statisticswindow.h src/ui/gui/statisticswindow.ui
    src/ui/gui/videodrawhelper.cpp src/ui/gui/videodrawhelper.h
    src/ui/gui/videointerface.h
    src/ui/gui/videowidget.cpp src/ui/gui/videowidget.h
    src/ui/settings/audiosettings.cpp src/ui/settings/audiosettings.h src/ui/settings/audiosettings.ui
    src/ui/settings/automaticsettings.cpp src/ui/settings/automaticsettings.h src/ui/settings/automaticsettings.ui
    src/ui/settings/camerainfo.cpp src/ui/settings/camerainfo.h
    src/ui/settings/defaultsettings.cpp src/ui/settings/defaultsettings.h
    src/ui/settings/deviceinfointerface.h
    src/ui/settings/microphoneinfo.cpp src/ui/settings/microphoneinfo.h
    src/ui/settings/screeninfo.cpp src/ui/settings/screeninfo.h
    src/ui/settings/settings.cpp src/ui/settings/settings.h src/ui/settings/settings.ui
    src/ui/settings/settingshelper.cpp src/ui/settings/settingshelper.h
    src/ui/settings/sipsettings.cpp src/ui/settings/sipsettings.h src/ui/settings/sipsettings.ui
    src/ui/settings/stunmessage.ui
    src/ui/settings/videosettings.cpp src/ui/settings/videosettings.h src/ui/settings/videosettings.ui
    src/ui/uimanager.cpp src/ui/uimanager.h
    src/videoviewfactory.cpp src/videoviewfactory.h
    version.cpp
)

qt_add_resources(Kvazzup "Icons"
    PREFIX "/"
    FILES
        icons/add_contact.svg
        icons/add_to_call.svg
        icons/avatar.svg
        icons/start_call.svg
        icons/end_call.svg
        icons/settings.svg
        icons/screen_share.svg
        icons/video_on.svg
        icons/video_off.svg
        icons/mic_on.svg
        icons/mic_off.svg
)

qt_add_resources(Kvazzup "Stylesheet"
    PREFIX "/"
    FILES
        stylesheet.qss
)

qt_add_resources(Kvazzup "Fonts"
    PREFIX "/"
    FILES
        fonts/OpenSans-Regular.ttf
)

qt_add_resources(Kvazzup "Ico"
    PREFIX "/"
    FILES
        favicon.ico
)

target_include_directories(Kvazzup PRIVATE
    ..
    ../include
    src
)

target_link_directories(Kvazzup PRIVATE
    ../msvc_libs
    lib
)

if(CMAKE_BUILD_TYPE)
    message(STATUS "Selecting build type: ${CMAKE_BUILD_TYPE}")
elseif (NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected. Selecting Release. You may use CMAKE_BUILD_TYPE to select a different build type.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


target_link_libraries(Kvazzup PRIVATE
    LibOpenHevcWrapper
    Qt::Concurrent
    Qt::Core
    Qt::Gui
    Qt::Multimedia
    Qt::MultimediaWidgets
    Qt::Network
    Qt::OpenGL
    Qt::Svg
    Qt::Widgets
    cryptopp
    uvgrtp
    opus
)

if(WIN32)
    target_link_libraries(Kvazzup PRIVATE
        ole32
        oleaut32
        ws2_32
    )

    if(MSVC)
        target_compile_definitions(Kvazzup PRIVATE PIC)

        target_link_libraries(Kvazzup PRIVATE
            advapi32
            kernel32
            kvazaar_lib
            libspeexdsp
        )
    elseif(MINGW)
        target_link_libraries(Kvazzup PRIVATE
            # Remove: fopenmp
            kvazaar
            speexdsp
            ssp
            strmiids
        )

    target_compile_options(Kvazzup
        -fopenmp
        -mavx2
        -msse4.1
    )
    endif()
elseif(UNIX)

    target_link_libraries(Kvazzup PRIVATE
        kvazaar
        speexdsp
    )

    target_compile_options(Kvazzup
        -fopenmp
        -mavx2
        -msse4.1
    )
endif()

add_subdirectory(test EXCLUDE_FROM_ALL)

if((CONFIG(OFF)) AND ((CMAKE_BUILD_TYPE STREQUAL Debug)))
    set_target_properties(Kvazzup PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()

install(TARGETS Kvazzup
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET Kvazzup
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
