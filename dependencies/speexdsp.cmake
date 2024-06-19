include(ExternalProject)

if (NOT SPEEXDSP_FOUND)
    if(MSVC)
        message(STATUS "Downloading ready-built SpeexDSP binary library")

        # libspeexdsp
        FetchContent_Declare(
                libspeexdsp
                URL https://github.com/ultravideo/uvgComm/releases/download/v0.12.0/speexdsp.zip
                URL_HASH SHA256=7522fb434a1929b34e62b291ec5cfdaff29d4a48e6e1ed7208fee294b4f78a63
                DOWNLOAD_EXTRACT_TIMESTAMP ON
        )

        FetchContent_MakeAvailable(libspeexdsp)

        include_directories(${libspeexdsp_SOURCE_DIR}/include)
        link_directories(${libspeexdsp_SOURCE_DIR})

        file(GLOB SPEEXDSP_LIB "${libspeexdsp_SOURCE_DIR}/libspeexdsp.*")

        file(COPY ${SPEEXDSP_LIB} DESTINATION ${CMAKE_BINARY_DIR})

        # install libspeexdsp dll with uvgComm install
        install(FILES ${SPEEXDSP_LIB} DESTINATION ${CMAKE_INSTALL_BINDIR})
    else()
        message(FATAL_ERROR "Please install SpeexDSP package to compile uvgComm")
    endif()
endif()
