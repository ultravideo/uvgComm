include(FetchContent)


if (NOT KVAZAAR_FOUND)
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building Kvazaar")

    # Kvazaar
    FetchContent_Declare(
            kvazaar
            GIT_REPOSITORY https://github.com/ultravideo/kvazaar.git
            GIT_TAG        f49af6386c9c7cddaa9d00e85cfe30cfe1d6a60f # 2.3.1 release
    )

    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

    if (TRUE) # use this if kvazaar gets a CMake in the main repo
        FetchContent_MakeAvailable(kvazaar)
    else()
        FetchContent_Populate(kvazaar)

        configure_file(dependencies/kvazaar_CMakeLists.txt ${kvazaar_SOURCE_DIR}/CMakeLists.txt COPYONLY)
        add_subdirectory(${kvazaar_SOURCE_DIR} ${kvazaar_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()

    unset(BUILD_SHARED_LIBS)
    unset(BUILD_TESTS)
endif()
