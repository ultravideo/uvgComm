# CMakeLists for compiling uvgComm unit tests

include(cmake/FindDependencies.cmake)

enable_testing()
include(GoogleTest)

set(uvgComm_TEST_SOURCES ${uvgComm_SOURCES})
list(TRANSFORM uvgComm_TEST_SOURCES PREPEND "../")

qt_add_executable(uvgComm_test WIN32 MACOSX_BUNDLE
            main.cpp
            test_1_common.cpp
            test_2_stun.cpp
            test_3_logger.cpp
            initiation/test_initiation.cpp
            media/test_media.cpp
            ui/test_ui.cpp

            ${uvgComm_TEST_SOURCES}
        )


target_include_directories(uvgComm_test PRIVATE
    ../../include
    ../
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src>
)

target_link_directories(uvgComm_test PRIVATE
    ../../msvc_libs
    ../lib
)

if(MSVC)
    target_compile_definitions(uvgComm_test PRIVATE PIC)
endif()

target_link_libraries(uvgComm_test PRIVATE GTest::GTestMain ${uvgComm_LIBS})

gtest_add_tests(TARGET uvgComm_test)
