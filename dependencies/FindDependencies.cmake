# dependencies
find_package(Cryptopp 8.0 QUIET)
find_package(Kvazaar  2.2 QUIET)
find_package(libyuv       QUIET) # libyuv does not have versions
find_package(OpenHEVC 2.0 QUIET)
find_package(Opus     1.1 QUIET)
find_package(SpeexDSP 1.2 QUIET)
find_package(uvgRTP   2.3 QUIET)

# do another check with pkgConfig in case the first one failed
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    if (NOT CRYPTOPP_FOUND)
        pkg_search_module(CRYPTOPP libcrypto++>=8.0 cryptopp>=8.0)
    endif()
    if (NOT KVAZAAR_FOUND)
        pkg_search_module(KVAZAAR libkvazaar>=2.2 kvazaar>=2.2)
    endif()
    if (NOT LIBYUV_FOUND)
        pkg_search_module(LIBYUV yuv libyuv)
    endif()
    if (NOT OPENHEVC_FOUND)
        pkg_search_module(OPENHEVC OpenHEVC LibOpenHevcWrapper>=2.0 OpenHevcWrapper>=2.0)
    endif()
    if (NOT OPUS_FOUND)
        pkg_search_module(OPUS opus>=1.1 libopus>=1.1)
    endif()
    if (NOT SPEEXDSP_FOUND)
        pkg_search_module(SPEEXDSP libspeexdsp>=1.2 speexdsp>=1.2)
    endif()
    if (NOT UVGRTP_FOUND)
        pkg_search_module(UVGRTP uvgrtp>=2.3 uvgRTP>=2.3)
    endif()
endif()

# Here we prints status of all dependencies so it is easier to figure out what
# is happening during build process
if (CRYPTOPP_FOUND)
    message(STATUS "Found Crypto++")
else()
    message(STATUS "Did not find Crypto++")
endif()

if (KVAZAAR_FOUND)
    message(STATUS "Found Kvazaar")
else()
    message(STATUS "Did not find Kvazaar")
endif()

if (LIBYUV_FOUND)
    message(STATUS "Found libyuv")
else()
    message(STATUS "Did not find libyuv")
endif()

if (OPENHEVC_FOUND)
    message(STATUS "Found OpenHEVC")
else()
    message(STATUS "Did not find OpenHEVC")
endif()

if (OPUS_FOUND)
    message(STATUS "Found Opus")
else()
    message(STATUS "Did not find Opus")
endif()

if (SPEEXDSP_FOUND)
    message(STATUS "Found SpeexDSP")
else()
    message(STATUS "Did not find SpeexDSP")
endif()

if (UVGRTP_FOUND)
    message(STATUS "Found uvgRTP")
else()
    message(STATUS "Did not find uvgRTP")
endif()


include(dependencies/cryptopp.cmake)
include(dependencies/uvgrtp.cmake)
include(dependencies/openhevc.cmake)
include(dependencies/kvazaar.cmake)
include(dependencies/libyuv.cmake)
include(dependencies/opus.cmake)
include(dependencies/speexdsp.cmake)

# include and link directories for dependencies. 
# These are needed when the compilation happens a second time and the library 
# is found so no compilation happens. Not needed in every case, but a nice backup
include_directories(${CMAKE_BINARY_DIR}/include)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/lib)
