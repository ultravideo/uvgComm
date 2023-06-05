include(FetchContent)

if (NOT UVGRTP_FOUND)
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building uvgRTP")

    # uvgRTP
    FetchContent_Declare(
            uvgrtp
            GIT_REPOSITORY https://github.com/ultravideo/uvgRTP.git
            GIT_TAG        4762c7e710f9f2e3c56d080b991904ec7a800287
    )

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    set(UVGRTP_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(UVGRTP_DISABLE_EXAMPLES  ON CACHE BOOL "" FORCE)
    set(UVGRTP_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(UVGRTP_DOWNLOAD_CRYPTO ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(uvgrtp)

    unset(BUILD_SHARED_LIBS)
endif()
