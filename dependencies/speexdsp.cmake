include(ExternalProject)

find_package(SpeexDSP QUIET)

if (NOT SPEEXDSP_FOUND)
    # try pkgconfig just to be sure
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_search_module(SPEEXDSP libspeexdsp speexdsp)
    endif()
endif()

if (SPEEXDSP_FOUND)
    message(STATUS "Using found SpeexDSP library")
elseif(MSVC)
    message(STATUS "Downloading ready-built SpeexDSP binary library")

    ExternalProject_Add(SpeexDSPDownload
      URL https://github.com/ultravideo/kvazzup/releases/download/v0.12.0/speexdsp.zip
      URL_HASH SHA256=7522fb434a1929b34e62b291ec5cfdaff29d4a48e6e1ed7208fee294b4f78a63
      PREFIX ${CMAKE_CURRENT_BINARY_DIR}/external
      DOWNLOAD_NAME "speexdsp.zip"
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ${CMAKE_COMMAND} -E echo "No build step"
      INSTALL_COMMAND ""
      DOWNLOAD_EXTRACT_TIMESTAMP ON
    )

    include_directories(${CMAKE_CURRENT_BINARY_DIR}/external/src/SpeexDSPDownload/include)
    link_directories(${CMAKE_CURRENT_BINARY_DIR}/external/src/SpeexDSPDownload/)
else()
    message(FATAL_ERROR "Please install SpeexDSP package to compile Kvazzup")
endif()
