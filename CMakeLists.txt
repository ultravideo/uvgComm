cmake_minimum_required(VERSION 3.16)

include(cmake/ProjectDetails.cmake)

# Note: We do not set the default build type, since this is typically handled by Qt Creator

project(kvazzup
    VERSION ${kvazzup_VER}
    DESCRIPTION ${kvazzup_DESCR}
    HOMEPAGE_URL ${kvazzup_URL}
    LANGUAGES CXX)

set(CMAKE_POLICY_DEFAULT_CMP0100 NEW)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Concurrent Gui Multimedia MultimediaWidgets Network OpenGL Svg Widgets)

if(${QT_VERSION_MAJOR} LESS 6)
    message(FATAL_ERROR "Qt versions smaller than 6.2 are no longer supported")
elseif(${QT_VERSION_MAJOR} EQUAL 6 AND ${QT_VERSION_MINOR} LESS 2)
    message(FATAL_ERROR "Qt versions smaller than 6.2 are no longer supported")
endif()

message(STATUS "Building Kvazzup with Qt version " ${QT_VERSION})

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(${QT_VERSION_MAJOR} EQUAL 6 AND ${QT_VERSION_MINOR} GREATER 2)
    message(STATUS "Qt Standard project setup")
    qt_standard_project_setup()
else()
    include(GNUInstallDirs)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
endif()

#option(KVAZZUP_ENABLE_SRTP    "Enables media encryption" ON)
#option(KVAZZUP_ENABLE_PRINTS  "Print Debug prints to command line" ON)
#option(KVAZZUP_ENABLE_LOGGING "Save log to file" ON)
#option(KVAZZUP_ENABLE_WERROR  "Fail with compiler warnings" OFF)
option(KVAZZUP_ENABLE_FACE_DETECTION "Enable face detection in Kvazzup" OFF)

include(dependencies/FindDependencies.cmake)

include(cmake/Versioning.cmake)

set(KVAZZUP_SOURCES
    src/common.cpp src/common.h
    src/global.h
    src/icetypes.h
    src/initiation/connectionpolicy.cpp                 src/initiation/connectionpolicy.h
    src/initiation/negotiation/networkcandidates.cpp    src/initiation/negotiation/networkcandidates.h
    src/initiation/negotiation/sdpdefault.cpp           src/initiation/negotiation/sdpdefault.h
    src/initiation/negotiation/sdpice.cpp               src/initiation/negotiation/sdpice.h
    src/initiation/negotiation/sdpnegotiation.cpp       src/initiation/negotiation/sdpnegotiation.h
    src/initiation/negotiation/sdptypes.h
    src/initiation/negotiation/sipcontent.cpp           src/initiation/negotiation/sipcontent.h
    src/initiation/negotiation/sdpmeshconference.cpp    src/initiation/negotiation/sdpmeshconference.h
    src/initiation/siphelper.cpp                        src/initiation/siphelper.h
    src/initiation/sipmanager.cpp                       src/initiation/sipmanager.h
    src/initiation/sipmessageflow.cpp                   src/initiation/sipmessageflow.h
    src/initiation/sipmessageprocessor.cpp              src/initiation/sipmessageprocessor.h
    src/initiation/siptypes.h
    src/initiation/transaction/sipallow.cpp             src/initiation/transaction/sipallow.h
    src/initiation/transaction/sipcallbacks.cpp         src/initiation/transaction/sipcallbacks.h
    src/initiation/transaction/sipclient.cpp            src/initiation/transaction/sipclient.h
    src/initiation/transaction/sipdialogstate.cpp       src/initiation/transaction/sipdialogstate.h
    src/initiation/transaction/sipserver.cpp            src/initiation/transaction/sipserver.h
    src/initiation/transport/connectionserver.cpp       src/initiation/transport/connectionserver.h
    src/initiation/transport/sipauthentication.cpp      src/initiation/transport/sipauthentication.h
    src/initiation/transport/sipconversions.cpp         src/initiation/transport/sipconversions.h
    src/initiation/transport/sipfieldcomposing.cpp      src/initiation/transport/sipfieldcomposing.h
    src/initiation/transport/sipfieldcomposinghelper.cpp src/initiation/transport/sipfieldcomposinghelper.h
    src/initiation/transport/sipfieldparsing.cpp        src/initiation/transport/sipfieldparsing.h
    src/initiation/transport/sipfieldparsinghelper.cpp  src/initiation/transport/sipfieldparsinghelper.h
    src/initiation/transport/sipmessagesanity.cpp       src/initiation/transport/sipmessagesanity.h
    src/initiation/transport/siprouting.cpp             src/initiation/transport/siprouting.h
    src/initiation/transport/siptransport.cpp           src/initiation/transport/siptransport.h
    src/initiation/transport/siptransporthelper.cpp     src/initiation/transport/siptransporthelper.h
    src/initiation/transport/tcpconnection.cpp          src/initiation/transport/tcpconnection.h
    src/kvazzupcontroller.cpp src/kvazzupcontroller.h
    src/logger.cpp src/logger.h
    src/media/delivery/delivery.cpp                 src/media/delivery/delivery.h
    src/media/delivery/ice.cpp                      src/media/delivery/ice.h
    src/media/delivery/icecandidatetester.cpp       src/media/delivery/icecandidatetester.h
    src/media/delivery/icepairtester.cpp            src/media/delivery/icepairtester.h
    src/media/delivery/icesessiontester.cpp         src/media/delivery/icesessiontester.h
    src/media/delivery/uvgrtpreceiver.cpp           src/media/delivery/uvgrtpreceiver.h
    src/media/delivery/uvgrtpsender.cpp             src/media/delivery/uvgrtpsender.h
    src/media/mediamanager.cpp                      src/media/mediamanager.h
    src/media/processing/audiocapturefilter.cpp     src/media/processing/audiocapturefilter.h
    src/media/processing/audioframebuffer.cpp       src/media/processing/audioframebuffer.h
    src/media/processing/audiomixer.cpp             src/media/processing/audiomixer.h
    src/media/processing/audiomixerfilter.cpp       src/media/processing/audiomixerfilter.h
    src/media/processing/audiooutputdevice.cpp      src/media/processing/audiooutputdevice.h
    src/media/processing/audiooutputfilter.cpp      src/media/processing/audiooutputfilter.h
    src/media/processing/camerafilter.cpp           src/media/processing/camerafilter.h
    src/media/processing/displayfilter.cpp          src/media/processing/displayfilter.h
    src/media/processing/dspfilter.cpp              src/media/processing/dspfilter.h
    src/media/processing/filter.cpp                 src/media/processing/filter.h
    src/media/processing/filtergraph.cpp            src/media/processing/filtergraph.h
    src/media/processing/halfrgbfilter.cpp          src/media/processing/halfrgbfilter.h
    src/media/processing/kvazaarfilter.cpp          src/media/processing/kvazaarfilter.h
    src/media/processing/openhevcfilter.cpp         src/media/processing/openhevcfilter.h
    src/media/processing/opusdecoderfilter.cpp      src/media/processing/opusdecoderfilter.h
    src/media/processing/opusencoderfilter.cpp      src/media/processing/opusencoderfilter.h
    src/media/processing/roimanualfilter.cpp        src/media/processing/roimanualfilter.h
    src/media/processing/scalefilter.cpp            src/media/processing/scalefilter.h
    src/media/processing/screensharefilter.cpp      src/media/processing/screensharefilter.h
    src/media/processing/speexaec.cpp               src/media/processing/speexaec.h
    src/media/processing/speexdsp.cpp               src/media/processing/speexdsp.h
    src/media/processing/yuvconversions.cpp         src/media/processing/yuvconversions.h
    src/media/processing/yuvtorgb32.cpp             src/media/processing/yuvtorgb32.h
    src/media/processing/libyuvconverter.cpp        src/media/processing/libyuvconverter.h
    src/media/resourceallocator.cpp                 src/media/resourceallocator.h
    src/participantinterface.h
    src/settingskeys.h
    src/statisticsinterface.h
    src/stunmessage.cpp src/stunmessage.h
    src/stunmessagefactory.cpp src/stunmessagefactory.h
    src/udpserver.cpp src/udpserver.h
    src/ui/about.ui
    src/ui/gui/avatarholder.ui
    src/ui/gui/avatarimage.cpp                      src/ui/gui/avatarimage.h
    src/ui/gui/callwindow.cpp                       src/ui/gui/callwindow.h             src/ui/gui/callwindow.ui
    src/ui/gui/chartpainter.cpp                     src/ui/gui/chartpainter.h
    src/ui/gui/conferenceview.cpp                   src/ui/gui/conferenceview.h
    src/ui/gui/contactlist.cpp                      src/ui/gui/contactlist.h
    src/ui/gui/contactlistitem.cpp                  src/ui/gui/contactlistitem.h
    src/ui/gui/guimessage.cpp                       src/ui/gui/guimessage.h             src/ui/gui/guimessage.ui
    src/ui/gui/incomingcallwidget.ui
    src/ui/gui/messagewidget.ui
    src/ui/gui/outgoingcallwidget.ui
    src/ui/gui/statisticswindow.cpp                 src/ui/gui/statisticswindow.h       src/ui/gui/statisticswindow.ui
    src/ui/gui/videodrawhelper.cpp                  src/ui/gui/videodrawhelper.h
    src/ui/gui/videointerface.h
    src/ui/gui/videowidget.cpp                      src/ui/gui/videowidget.h
    src/ui/settings/audiosettings.cpp               src/ui/settings/audiosettings.h     src/ui/settings/audiosettings.ui
    src/ui/settings/automaticsettings.cpp           src/ui/settings/automaticsettings.h src/ui/settings/automaticsettings.ui
    src/ui/settings/camerainfo.cpp                  src/ui/settings/camerainfo.h
    src/ui/settings/defaultsettings.cpp             src/ui/settings/defaultsettings.h
    src/ui/settings/deviceinfointerface.h
    src/ui/settings/microphoneinfo.cpp              src/ui/settings/microphoneinfo.h
    src/ui/settings/screeninfo.cpp                  src/ui/settings/screeninfo.h
    src/ui/settings/settings.cpp                    src/ui/settings/settings.h          src/ui/settings/settings.ui
    src/ui/settings/settingshelper.cpp              src/ui/settings/settingshelper.h
    src/ui/settings/sipsettings.cpp                 src/ui/settings/sipsettings.h       src/ui/settings/sipsettings.ui
    src/ui/settings/stunmessage.ui
    src/ui/settings/videosettings.cpp               src/ui/settings/videosettings.h     src/ui/settings/videosettings.ui
    src/ui/uimanager.cpp                            src/ui/uimanager.h
    src/videoviewfactory.cpp                        src/videoviewfactory.h
    src/mediaid.h                                   src/mediaid.cpp
)


if (KVAZZUP_ENABLE_FACE_DETECTION)
    list(APPEND KVAZZUP_SOURCES
      src/media/processing/roifilter.cpp  src/media/processing/roifilter.h
      src/media/processing/detection_types.h
    )
endif()

qt_add_executable(kvazzup WIN32 MACOSX_BUNDLE
    ${KVAZZUP_SOURCES}
    src/main.cpp
    version.cpp
)

set(KVAZZUP_RESOURCES
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
    stylesheet.qss
    fonts/OpenSans-Regular.ttf
    favicon.ico
)

qt_add_resources(kvazzup "Resources"
    PREFIX "/"
    FILES
        ${KVAZZUP_RESOURCES}
)

target_include_directories(kvazzup PRIVATE
    src
)

set(KVAZZUP_LIBS LibOpenHevcWrapper
    Qt::Concurrent
    Qt::Core
    Qt::Gui
    Qt::Multimedia
    Qt::MultimediaWidgets
    Qt::Network
    Qt::OpenGL
    Qt::Svg
    Qt::Widgets
    uvgrtp
    opus
    kvazaar
    yuv
)

if (KVAZZUP_ENABLE_FACE_DETECTION)
    message("Enabling face detection")
    find_package(OpenCV)

    if (OpenCV_FOUND)
        target_compile_definitions(kvazzup PRIVATE KVAZZUP_HAVE_OPENCV)
        list(APPEND KVAZZUP_LIBS ${OpenCV_LIBS})
        include_directories( ${OpenCV_INCLUDE_DIRS} )
    endif()

    target_compile_definitions(kvazzup PRIVATE KVAZZUP_HAVE_ONNX_RUNTIME)
    include_directories(${CMAKE_SOURCE_DIR}/include)
    target_link_directories(kvazzup PRIVATE ${CMAKE_SOURCE_DIR}/libs)
    message("Linking to ${CMAKE_SOURCE_DIR}/libs")

    list(APPEND KVAZZUP_LIBS onnxruntime)
endif()

target_compile_definitions(kvazzup PRIVATE KVAZZUP_RTP_MULTIPLEXING)

if (CRYPTOPP_FOUND AND NOT MSVC)
    list(APPEND KVAZZUP_LIBS cryptopp.a) # this makes sure we link up the static version
else()
    list(APPEND KVAZZUP_LIBS cryptopp)
endif()

find_package(OpenMP) # needed for multithreading conversions
if(OpenMP_CXX_FOUND)
    list(APPEND KVAZZUP_LIBS OpenMP::OpenMP_CXX)
endif()

find_package(JPEG QUIET) # needed for motion jpeg inside libyuv, optional
if (JPEG_FOUND)
    list(APPEND KVAZZUP_LIBS  ${JPEG_LIBRARY})
else()
    target_compile_definitions(kvazzup PRIVATE KVAZZUP_NO_JPEG)
endif()

if(WIN32)
    list(APPEND KVAZZUP_LIBS ole32 oleaut32 ws2_32)
    
    if(MSVC)
        target_compile_definitions(kvazzup PRIVATE PIC)
        list(APPEND KVAZZUP_LIBS advapi32 kernel32 libspeexdsp)
    elseif(MINGW)
        list(APPEND KVAZZUP_LIBS speexdsp ssp strmiids)
    endif()
elseif(UNIX)
    list(APPEND KVAZZUP_LIBS speexdsp)
    target_compile_options(kvazzup PUBLIC "-march=native")
endif()

target_link_libraries(kvazzup PRIVATE ${KVAZZUP_LIBS})


# Unit tests
#add_subdirectory(test EXCLUDE_FROM_ALL)

if((CONFIG(OFF)) AND ((CMAKE_BUILD_TYPE STREQUAL Debug)))
    set_target_properties(kvazzup PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()

install(TARGETS kvazzup
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(${QT_VERSION_MAJOR} EQUAL 6 AND ${QT_VERSION_MINOR} LESS_EQUAL 2)
    message(WARNING "Kvazzup does not support deployment with Qt 6.2 version")
elseif(${QT_VERSION_MAJOR} EQUAL 6 AND ${QT_VERSION_MINOR} LESS_EQUAL 4)
    qt_generate_deploy_app_script(
        TARGET kvazzup
        FILENAME_VARIABLE deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
else()
    qt_generate_deploy_app_script(
        TARGET kvazzup
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()


