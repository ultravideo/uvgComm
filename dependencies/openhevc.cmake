include(FetchContent)

if (NOT OPENHEVC_FOUND)
    if(NOT MSVC)
        find_package(Git)

        message(STATUS "Fetching and building OpenHEVC")

        # OpenHEVC
        FetchContent_Declare(
                        LibOpenHevcWrapper
                        GIT_REPOSITORY https://github.com/OpenHEVC/openHEVC.git
                        GIT_TAG        20dde1a748ad0b890067814c7f24cbcef6568cae # ffmpeg_update branch
        )

        # static version compiles by default
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

        FetchContent_MakeAvailable(LibOpenHevcWrapper)

        unset(BUILD_SHARED_LIBS)

        # this gets us the openHevcWrapper.h include file
        include_directories(${libopenhevcwrapper_SOURCE_DIR}/gpac/modules/openhevc_dec)
    else()
        include(ExternalProject)

        message(STATUS "Downloading ready-built OpenHEVC binary library")

        # Download OpenHEVC binaries from previous Kvazzup release
        ExternalProject_Add(LibOpenHevcWrapperDownload
          URL https://github.com/ultravideo/kvazzup/releases/download/v0.12.0/openhevc.zip
          URL_HASH SHA256=345097b7ce272e7b958fdc2ba43f1f41061f63e7fab738b09516334b5e96f90b
          PREFIX ${CMAKE_CURRENT_BINARY_DIR}/external
          DOWNLOAD_NAME "openhevc.zip"
          CONFIGURE_COMMAND ""
          BUILD_COMMAND ${CMAKE_COMMAND} -E echo "No build step"
          INSTALL_COMMAND ""
          DOWNLOAD_EXTRACT_TIMESTAMP ON
        )

        include_directories(${CMAKE_CURRENT_BINARY_DIR}/external/src/LibOpenHevcWrapperDownload/include)
        link_directories(${CMAKE_CURRENT_BINARY_DIR}/external/src/LibOpenHevcWrapperDownload/)
    endif()
endif()



