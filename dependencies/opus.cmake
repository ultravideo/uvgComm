include(FetchContent)

if (NOT OPUS_FOUND)
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building opus")

    # opus
    FetchContent_Declare(
            opus
            GIT_REPOSITORY https://github.com/xiph/opus
            GIT_TAG        v1.3.1
    )

    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(opus)

    unset(BUILD_SHARED_LIBS)

    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/opus)

    file(GLOB OPUS_HEADERS "${opus_SOURCE_DIR}/include/*")
    file(COPY ${OPUS_HEADERS} DESTINATION ${CMAKE_BINARY_DIR}/include/opus)

    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    file(REMOVE ${opus_BINARY_DIR}/opus.pc)
    file(GLOB OPUS_LIB "${opus_BINARY_DIR}/opus.*")
    file(COPY ${OPUS_LIB} DESTINATION ${CMAKE_BINARY_DIR}/lib)

endif()
