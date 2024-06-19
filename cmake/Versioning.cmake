if(GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE uvgComm_GIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(result)
        message(STATUS "Failed to get git hash")
    else()
        message(STATUS "Got git hash: ${uvgComm_GIT_HASH}")
    endif()
endif()

if(uvgComm_GIT_HASH)
    SET(uvgComm_GIT_HASH "${uvgComm_GIT_HASH}")
endif()

option(RELEASE_COMMIT "Create a release version" OFF)
if(RELEASE_COMMIT)
    set (uvgComm_VERSION "${PROJECT_VERSION}")
elseif(uvgComm_GIT_HASH)
    set (uvgComm_VERSION "${PROJECT_VERSION}-${uvgComm_GIT_HASH}")
else()
    set (uvgComm_VERSION "${PROJECT_VERSION}-source")
    set(uvgComm_GIT_HASH "source")
endif()

message(STATUS "uvgComm version: ${uvgComm_VERSION}")

configure_file(cmake/version.cpp.in version.cpp @ONLY)

if (RELEASE_COMMIT)
    target_compile_definitions(${PROJECT_NAME}_version PRIVATE uvgComm_RELEASE_COMMIT)
endif()

