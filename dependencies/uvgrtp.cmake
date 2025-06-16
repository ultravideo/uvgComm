include(FetchContent)

if (NOT UVGRTP_FOUND)
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building uvgRTP")

    # uvgRTP
    FetchContent_Declare(
            uvgrtp
            GIT_REPOSITORY https://gitlab.tuni.fi/cs/ultravideo/uvgrtp.git
            GIT_TAG        3bb0442a9510e3bfbaa2af331a77bf04ef0ec365 # uvgRTP private version
    )

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    set(UVGRTP_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(UVGRTP_DISABLE_EXAMPLES  ON CACHE BOOL "" FORCE)
    set(UVGRTP_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(UVGRTP_DOWNLOAD_CRYPTO ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(uvgrtp)

    unset(BUILD_SHARED_LIBS)
endif()
