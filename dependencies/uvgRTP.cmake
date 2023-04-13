include(FetchContent)

# Git
find_package(Git)


message(STATUS "Fetching and building uvgRTP")
# uvgRTP
FetchContent_Declare(
        uvgRTP
        GIT_REPOSITORY https://github.com/ultravideo/uvgRTP.git
        GIT_TAG        4423d6942627fd8458e488333b1f059ebe2d243c
)

FetchContent_MakeAvailable(uvgRTP)