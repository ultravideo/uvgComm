include(FetchContent)

# Git
find_package(Git)

message(STATUS "Fetching and building libyuv")


# uvgRTP
FetchContent_Declare(
        yuv
        GIT_REPOSITORY https://github.com/jrsnen/libyuv.git
        GIT_TAG        3e10a91ba61d175109a19f282cb9bca756c057e8
)

set(TEST OFF CACHE BOOL "" FORCE)
set(LIBYUV_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(yuv)

unset(TEST)
unset(LIBYUV_INSTALL)

include_directories(${yuv_SOURCE_DIR}/include)
