project(kvazzup_test)

include(cmake/FindDependencies.cmake)

enable_testing()
include(GoogleTest)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE
            main.cpp
            test_1_common.cpp
            test_2_stun.cpp
            test_3_logger.cpp
            initiation/test_initiation.cpp
            media/test_media.cpp
            ui/test_ui.cpp

            ../src/common.cpp ../src/common.h
            ../src/global.h
            ../src/icetypes.h
            ../src/initiation/connectionpolicy.cpp ../src/initiation/connectionpolicy.h
            ../src/initiation/negotiation/networkcandidates.cpp ../src/initiation/negotiation/networkcandidates.h
            ../src/initiation/negotiation/sdpdefault.cpp ../src/initiation/negotiation/sdpdefault.h
            ../src/initiation/negotiation/sdpice.cpp ../src/initiation/negotiation/sdpice.h
            ../src/initiation/negotiation/sdpnegotiation.cpp ../src/initiation/negotiation/sdpnegotiation.h
            ../src/initiation/negotiation/sdptypes.h
            ../src/initiation/negotiation/sipcontent.cpp ../src/initiation/negotiation/sipcontent.h
            ../src/initiation/siphelper.cpp ../src/initiation/siphelper.h
            ../src/initiation/sipmanager.cpp ../src/initiation/sipmanager.h
            ../src/initiation/sipmessageflow.cpp ../src/initiation/sipmessageflow.h
            ../src/initiation/sipmessageprocessor.cpp ../src/initiation/sipmessageprocessor.h
            ../src/initiation/siptypes.h
            ../src/initiation/transaction/sipallow.cpp ../src/initiation/transaction/sipallow.h
            ../src/initiation/transaction/sipcallbacks.cpp ../src/initiation/transaction/sipcallbacks.h
            ../src/initiation/transaction/sipclient.cpp ../src/initiation/transaction/sipclient.h
            ../src/initiation/transaction/sipdialogstate.cpp ../src/initiation/transaction/sipdialogstate.h
            ../src/initiation/transaction/sipserver.cpp ../src/initiation/transaction/sipserver.h
            ../src/initiation/transport/connectionserver.cpp ../src/initiation/transport/connectionserver.h
            ../src/initiation/transport/sipauthentication.cpp ../src/initiation/transport/sipauthentication.h
            ../src/initiation/transport/sipconversions.cpp ../src/initiation/transport/sipconversions.h
            ../src/initiation/transport/sipfieldcomposing.cpp ../src/initiation/transport/sipfieldcomposing.h
            ../src/initiation/transport/sipfieldcomposinghelper.cpp ../src/initiation/transport/sipfieldcomposinghelper.h
            ../src/initiation/transport/sipfieldparsing.cpp ../src/initiation/transport/sipfieldparsing.h
            ../src/initiation/transport/sipfieldparsinghelper.cpp ../src/initiation/transport/sipfieldparsinghelper.h
            ../src/initiation/transport/sipmessagesanity.cpp ../src/initiation/transport/sipmessagesanity.h
            ../src/initiation/transport/siprouting.cpp ../src/initiation/transport/siprouting.h
            ../src/initiation/transport/siptransport.cpp ../src/initiation/transport/siptransport.h
            ../src/initiation/transport/siptransporthelper.cpp ../src/initiation/transport/siptransporthelper.h
            ../src/initiation/transport/tcpconnection.cpp ../src/initiation/transport/tcpconnection.h
            ../src/kvazzupcontroller.cpp ../src/kvazzupcontroller.h
            ../src/logger.cpp ../src/logger.h
            ../src/media/delivery/delivery.cpp ../src/media/delivery/delivery.h
            ../src/media/delivery/ice.cpp ../src/media/delivery/ice.h
            ../src/media/delivery/icecandidatetester.cpp ../src/media/delivery/icecandidatetester.h
            ../src/media/delivery/icepairtester.cpp ../src/media/delivery/icepairtester.h
            ../src/media/delivery/icesessiontester.cpp ../src/media/delivery/icesessiontester.h
            ../src/media/delivery/uvgrtpreceiver.cpp ../src/media/delivery/uvgrtpreceiver.h
            ../src/media/delivery/uvgrtpsender.cpp ../src/media/delivery/uvgrtpsender.h
            ../src/media/mediamanager.cpp ../src/media/mediamanager.h
            ../src/media/processing/audiocapturefilter.cpp ../src/media/processing/audiocapturefilter.h
            ../src/media/processing/audioframebuffer.cpp ../src/media/processing/audioframebuffer.h
            ../src/media/processing/audiomixer.cpp ../src/media/processing/audiomixer.h
            ../src/media/processing/audiomixerfilter.cpp ../src/media/processing/audiomixerfilter.h
            ../src/media/processing/audiooutputdevice.cpp ../src/media/processing/audiooutputdevice.h
            ../src/media/processing/audiooutputfilter.cpp ../src/media/processing/audiooutputfilter.h
            ../src/media/processing/camerafilter.cpp ../src/media/processing/camerafilter.h
            ../src/media/processing/displayfilter.cpp ../src/media/processing/displayfilter.h
            ../src/media/processing/dspfilter.cpp ../src/media/processing/dspfilter.h
            ../src/media/processing/filter.cpp ../src/media/processing/filter.h
            ../src/media/processing/filtergraph.cpp ../src/media/processing/filtergraph.h
            ../src/media/processing/halfrgbfilter.cpp ../src/media/processing/halfrgbfilter.h
            ../src/media/processing/kvazaarfilter.cpp ../src/media/processing/kvazaarfilter.h
            ../src/media/processing/openhevcfilter.cpp ../src/media/processing/openhevcfilter.h
            ../src/media/processing/opusdecoderfilter.cpp ../src/media/processing/opusdecoderfilter.h
            ../src/media/processing/opusencoderfilter.cpp ../src/media/processing/opusencoderfilter.h
            ../src/media/processing/rgb32toyuv.cpp ../src/media/processing/rgb32toyuv.h
            ../src/media/processing/roimanualfilter.cpp ../src/media/processing/roimanualfilter.h
            ../src/media/processing/scalefilter.cpp ../src/media/processing/scalefilter.h
            ../src/media/processing/screensharefilter.cpp ../src/media/processing/screensharefilter.h
            ../src/media/processing/speexaec.cpp ../src/media/processing/speexaec.h
            ../src/media/processing/speexdsp.cpp ../src/media/processing/speexdsp.h
            ../src/media/processing/yuvconversions.cpp ../src/media/processing/yuvconversions.h
            ../src/media/processing/yuvtorgb32.cpp ../src/media/processing/yuvtorgb32.h
            ../src/media/processing/yuyvtorgb32.cpp ../src/media/processing/yuyvtorgb32.h
            ../src/media/processing/yuyvtoyuv420.cpp ../src/media/processing/yuyvtoyuv420.h
            ../src/media/resourceallocator.cpp ../src/media/resourceallocator.h
            ../src/participantinterface.h
            ../src/settingskeys.h
            ../src/statisticsinterface.h
            ../src/stunmessage.cpp ../src/stunmessage.h
            ../src/stunmessagefactory.cpp ../src/stunmessagefactory.h
            ../src/udpserver.cpp ../src/udpserver.h
            ../src/ui/about.ui
            ../src/ui/gui/avatarholder.ui
            ../src/ui/gui/avatarimage.cpp ../src/ui/gui/avatarimage.h
            ../src/ui/gui/callwindow.cpp ../src/ui/gui/callwindow.h ../src/ui/gui/callwindow.ui
            ../src/ui/gui/chartpainter.cpp ../src/ui/gui/chartpainter.h
            ../src/ui/gui/conferenceview.cpp ../src/ui/gui/conferenceview.h
            ../src/ui/gui/contactlist.cpp ../src/ui/gui/contactlist.h
            ../src/ui/gui/contactlistitem.cpp ../src/ui/gui/contactlistitem.h
            ../src/ui/gui/guimessage.cpp ../src/ui/gui/guimessage.h ../src/ui/gui/guimessage.ui
            ../src/ui/gui/incomingcallwidget.ui
            ../src/ui/gui/messagewidget.ui
            ../src/ui/gui/outgoingcallwidget.ui
            ../src/ui/gui/statisticswindow.cpp ../src/ui/gui/statisticswindow.h ../src/ui/gui/statisticswindow.ui
            ../src/ui/gui/videodrawhelper.cpp ../src/ui/gui/videodrawhelper.h
            ../src/ui/gui/videointerface.h
            ../src/ui/gui/videowidget.cpp ../src/ui/gui/videowidget.h
            ../src/ui/settings/audiosettings.cpp ../src/ui/settings/audiosettings.h ../src/ui/settings/audiosettings.ui
            ../src/ui/settings/automaticsettings.cpp ../src/ui/settings/automaticsettings.h ../src/ui/settings/automaticsettings.ui
            ../src/ui/settings/camerainfo.cpp ../src/ui/settings/camerainfo.h
            ../src/ui/settings/defaultsettings.cpp ../src/ui/settings/defaultsettings.h
            ../src/ui/settings/deviceinfointerface.h
            ../src/ui/settings/microphoneinfo.cpp ../src/ui/settings/microphoneinfo.h
            ../src/ui/settings/screeninfo.cpp ../src/ui/settings/screeninfo.h
            ../src/ui/settings/settings.cpp ../src/ui/settings/settings.h ../src/ui/settings/settings.ui
            ../src/ui/settings/settingshelper.cpp ../src/ui/settings/settingshelper.h
            ../src/ui/settings/sipsettings.cpp ../src/ui/settings/sipsettings.h ../src/ui/settings/sipsettings.ui
            ../src/ui/settings/stunmessage.ui
            ../src/ui/settings/videosettings.cpp ../src/ui/settings/videosettings.h ../src/ui/settings/videosettings.ui
            ../src/ui/uimanager.cpp ../src/ui/uimanager.h
            ../src/videoviewfactory.cpp ../src/videoviewfactory.h
        )


target_include_directories(kvazzup_test PRIVATE
    ../../include
    ../
)

target_link_directories(kvazzup_test PRIVATE
    ../../msvc_libs
    ../lib
)

target_link_libraries(kvazzup_test PRIVATE
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
    opus
    uvgrtp
)

if(WIN32)
    target_link_libraries(kvazzup_test PRIVATE
        ole32
        oleaut32
        ws2_32
    )

    if(MSVC)
        target_compile_definitions(kvazzup_test PRIVATE PIC)

        target_link_libraries(kvazzup_test PRIVATE
            advapi32
            cryptlib
            kernel32
            kvazaar_lib
            libspeexdsp
        )
    elseif(MINGW)
        target_link_libraries(kvazzup_test PRIVATE
            # Remove: fopenmp
            cryptopp
            kvazaar
            speexdsp
            ssp
            strmiids
        )

    target_compile_options(kvazzup_test
        -fopenmp
        -mavx2
        -msse4.1
    )
    endif()
elseif(UNIX)

    target_link_libraries(kvazzup_test PRIVATE
        cryptopp
        kvazaar
        speexdsp
    )

    target_compile_options(kvazzup_test
        -fopenmp
        -mavx2
        -msse4.1
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src>)

# set crypto++ to be linked in tests if available
if (NOT DISABLE_CRYPTO AND CRYPTOPP_FOUND)
    if(MSVC)
        set(CRYPTOPP_LIB_NAME "cryptlib")
    else()
        set(CRYPTOPP_LIB_NAME "cryptopp")
    endif()
else()
    set(CRYPTOPP_LIB_NAME "")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE GTest::GTestMain ${CRYPTOPP_LIB_NAME})

gtest_add_tests(TARGET ${PROJECT_NAME})
