include(FetchContent)

if (NOT UVGRTP_FOUND)
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building uvgRTP")

    # uvgRTP
    FetchContent_Declare(
            uvgrtp
            GIT_REPOSITORY https://github.com/ultravideo/uvgRTP.git
            GIT_TAG        4423d6942627fd8458e488333b1f059ebe2d243c
    )

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(uvgrtp)

    unset(BUILD_SHARED_LIBS)
endif()
