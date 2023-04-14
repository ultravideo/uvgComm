include(FetchContent)

find_package(Git)

message(STATUS "Fetching and building Crypto++")

# CMake for Crypto++
FetchContent_Declare(
        cryptopp-cmake
        GIT_REPOSITORY https://github.com/abdes/cryptopp-cmake.git
        GIT_TAG        CRYPTOPP_8_7_0_1
)

FetchContent_MakeAvailable(cryptopp-cmake)