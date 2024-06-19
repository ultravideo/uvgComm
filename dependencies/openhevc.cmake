include(FetchContent)

if (NOT OPENHEVC_FOUND)
    if(NOT MSVC)
        find_package(Git)

        message(STATUS "Fetching and building OpenHEVC")

        # OpenHEVC
        FetchContent_Declare(
                        LibOpenHevcWrapper
                        GIT_REPOSITORY https://github.com/jrsnen/openHEVC.git
                        GIT_TAG        cbe04ded9da58ea0994e122eeb550cf8aaae109e
        )

        # static version compiles by default
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

        FetchContent_MakeAvailable(LibOpenHevcWrapper)

        unset(BUILD_SHARED_LIBS)

        # this gets us the openHevcWrapper.h include file
        file(COPY ${libopenhevcwrapper_SOURCE_DIR}/gpac/modules/openhevc_dec/openHevcWrapper.h
            DESTINATION ${CMAKE_BINARY_DIR}/include/)
    else()
        include(ExternalProject)

        message(STATUS "Downloading ready-built OpenHEVC binary library")

        # Download OpenHEVC binaries from previous uvgComm release
        FetchContent_Declare(
                LibOpenHevcWrapper
                URL https://github.com/ultravideo/uvgComm/releases/download/v0.12.0/openhevc.zip
                URL_HASH SHA256=345097b7ce272e7b958fdc2ba43f1f41061f63e7fab738b09516334b5e96f90b
                DOWNLOAD_EXTRACT_TIMESTAMP ON
        )

        FetchContent_MakeAvailable(LibOpenHevcWrapper)

        include_directories(${libopenhevcwrapper_SOURCE_DIR}/include)
        link_directories(${libopenhevcwrapper_SOURCE_DIR})

        file(GLOB OPENHEVC_LIB "${libopenhevcwrapper_SOURCE_DIR}/LibOpenHevcWrapper.*")
        list(APPEND OPENHEVC_LIB ${libopenhevcwrapper_SOURCE_DIR}/pthreadVC2.dll)
        file(COPY ${OPENHEVC_LIB} DESTINATION ${CMAKE_BINARY_DIR})

        # Install openhevc dll with uvgComm install
        install(FILES ${OPENHEVC_LIB} DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()
