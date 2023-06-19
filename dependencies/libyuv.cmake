include(FetchContent)

if (NOT LIBYUV_FOUND)
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building libyuv")

    # libyuv
    FetchContent_Declare(
            yuv
            GIT_REPOSITORY https://chromium.googlesource.com/libyuv/libyuv
            GIT_TAG        eb6e7bb63738e29efd82ea3cf2a115238a89fa51 # stable
    )

    set(TEST OFF CACHE BOOL "" FORCE)
    set(LIBYUV_INSTALL OFF CACHE BOOL "" FORCE)

    if (FALSE) # use this if libyuv CMakeLists.txt becomes workable
        FetchContent_MakeAvailable(yuv)
    else()
        FetchContent_Populate(yuv)

        configure_file(dependencies/libyuv_CMakeLists.txt ${yuv_SOURCE_DIR}/CMakeLists.txt COPYONLY)
        add_subdirectory(${yuv_SOURCE_DIR} ${yuv_BINARY_DIR} EXCLUDE_FROM_ALL)

    endif()

    unset(TEST)
    unset(LIBYUV_INSTALL)

    include_directories(${yuv_SOURCE_DIR}/include)
endif()
