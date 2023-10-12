include(FetchContent)

if (NOT CRYPTOPP_FOUND)
    find_package(Git REQUIRED)

    message(STATUS "Fetching and building Crypto++")

    # CMake for Crypto++
    FetchContent_Declare(
            cryptopp-cmake
            GIT_REPOSITORY https://github.com/abdes/cryptopp-cmake.git
            GIT_TAG        43367a9cef6576b34179427a31a619802205406e
    )

    set(CRYPTOPP_INSTALL OFF CACHE BOOL "" FORCE) # we don't want to install Crypto++
    set(CRYPTOPP_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(cryptopp-cmake)

    unset(BUILD_SHARED_LIBS) # unset so it does not affect other projects

    # copy lib binary so it is found later
    file(GLOB CRYPTOPP_BIN "${cryptopp-cmake_BINARY_DIR}/cryptopp/cryptopp.*")
    file(COPY ${CRYPTOPP_BIN} DESTINATION ${CMAKE_BINARY_DIR}/lib/)
    file(GLOB CRYPTOPP_BIN "${cryptopp-cmake_BINARY_DIR}/cryptopp/libcryptopp.*")
    file(COPY ${CRYPTOPP_BIN} DESTINATION ${CMAKE_BINARY_DIR}/lib/)
endif()
