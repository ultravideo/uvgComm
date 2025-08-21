include(FetchContent)

if (NOT UVGRTP_FOUND)
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building uvgRTP")

    # uvgRTP
    FetchContent_Declare(
            uvgrtp
            GIT_REPOSITORY https://github.com/jrsnen/uvgRTP.git
            GIT_TAG        a5d24fe202242edcfa15449a8172ea237f67bdc6
    )

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    set(UVGRTP_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(UVGRTP_DISABLE_EXAMPLES  ON CACHE BOOL "" FORCE)
    set(UVGRTP_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(UVGRTP_DOWNLOAD_CRYPTO ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(uvgrtp)

    unset(BUILD_SHARED_LIBS)
endif()
