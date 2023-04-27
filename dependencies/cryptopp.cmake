include(FetchContent)

find_package(Cryptopp QUIET)

if (CRYPTOPP_FOUND)
    message(STATUS "Using system version of Crypto++")
else()
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building Crypto++")

    # CMake for Crypto++
    FetchContent_Declare(
            cryptopp-cmake
            GIT_REPOSITORY https://github.com/abdes/cryptopp-cmake.git
            GIT_TAG        CRYPTOPP_8_7_0_1
    )

    set(CRYPTOPP_INSTALL OFF CACHE BOOL "" FORCE) # we don't want to install Crypto++
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(cryptopp-cmake)

    unset(BUILD_SHARED_LIBS)
endif()
