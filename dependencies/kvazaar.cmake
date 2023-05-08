include(FetchContent)

if (NOT KVAZAAR_FOUND)
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building Kvazaar")

    # Kvazaar
    FetchContent_Declare(
            kvazaar
            GIT_REPOSITORY https://github.com/jrsnen/kvazaar.git
            GIT_TAG        0bd045db34c9896f37ce27bc6177c4f3bb1af097
    )

    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(kvazaar)

    unset(BUILD_SHARED_LIBS)
    unset(BUILD_TESTS)
endif()
