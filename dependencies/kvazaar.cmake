include(FetchContent)

find_package(Git)

message(STATUS "Fetching and building Kvazaar")

# CMake for Crypto++
FetchContent_Declare(
        kvazaar
        GIT_REPOSITORY https://github.com/jrsnen/kvazaar.git
        GIT_TAG        b04e868cf7a0388556154c39e9c04be728f32ab0
)

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(kvazaar)

unset(BUILD_SHARED_LIBS)
