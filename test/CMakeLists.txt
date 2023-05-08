# CMakeLists for compiling Kvazzup unit tests

include(cmake/FindDependencies.cmake)

enable_testing()
include(GoogleTest)

set(KVAZZUP_TEST_SOURCES ${KVAZZUP_SOURCES})
list(TRANSFORM KVAZZUP_TEST_SOURCES PREPEND "../")

qt_add_executable(kvazzup_test WIN32 MACOSX_BUNDLE
            main.cpp
            test_1_common.cpp
            test_2_stun.cpp
            test_3_logger.cpp
            initiation/test_initiation.cpp
            media/test_media.cpp
            ui/test_ui.cpp

            ${KVAZZUP_TEST_SOURCES}
        )


target_include_directories(kvazzup_test PRIVATE
    ../../include
    ../
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src>
)

target_link_directories(kvazzup_test PRIVATE
    ../../msvc_libs
    ../lib
)

if(MSVC)
    target_compile_definitions(kvazzup_test PRIVATE PIC)
endif()

target_link_libraries(kvazzup_test PRIVATE GTest::GTestMain ${KVAZZUP_LIBS})

gtest_add_tests(TARGET kvazzup_test)
