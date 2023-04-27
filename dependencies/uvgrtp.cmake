include(FetchContent)

find_package(uvgRTP QUIET)

if (NOT UVGRTP_FOUND)
    # try pkgconfig just to be sure
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_search_module(UVGRTP uvgrtp uvgRTP)
    endif()
endif()

if (UVGRTP_FOUND)
    message(STATUS "Using system version of uvgRTP")
else()
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
