include(FetchContent)

if (NOT UVGRTP_FOUND)
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building uvgRTP")

    # uvgRTP
    FetchContent_Declare(
            uvgrtp
            GIT_REPOSITORY https://github.com/jrsnen/uvgRTP.git
            GIT_TAG        465fdffdaf6ae49c2b17171ad5377a5d4c99eb97
    )

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    set(UVGRTP_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(UVGRTP_DISABLE_EXAMPLES  ON CACHE BOOL "" FORCE)
    set(UVGRTP_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(UVGRTP_DOWNLOAD_CRYPTO ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(uvgrtp)

    unset(BUILD_SHARED_LIBS)
endif()
