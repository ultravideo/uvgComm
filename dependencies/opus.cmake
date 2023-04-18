include(FetchContent)

# Git
find_package(Git)

message(STATUS "Fetching and building opus")

# opus
FetchContent_Declare(
        opus
        GIT_REPOSITORY https://github.com/xiph/opus
        GIT_TAG        v1.3.1
)

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(opus)
unset(BUILD_SHARED_LIBS)
