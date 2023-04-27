include(FetchContent)

find_package(Opus QUIET)

if (OPUS_FOUND)
    message(STATUS "Using system version of Opus")
else()
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
endif()
