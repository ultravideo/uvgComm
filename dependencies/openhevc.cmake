if(NOT MSVC)

    include(FetchContent)
    find_package(Git)

    message(STATUS "Fetching and building OpenHEVC")

    # CMake for Crypto++
    FetchContent_Declare(
                    LibOpenHevcWrapper
                    GIT_REPOSITORY https://github.com/OpenHEVC/openHEVC.git
                    GIT_TAG        20dde1a748ad0b890067814c7f24cbcef6568cae # ffmpeg_update branch
    )

    FetchContent_MakeAvailable(LibOpenHevcWrapper)
else()
    # TODO: Either download an MSVC building OpenHEVC version or download built binaries
endif()



