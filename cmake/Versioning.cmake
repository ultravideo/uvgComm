if(GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE kvazzup_GIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(result)
        message(STATUS "Failed to get git hash")
    else()
        message(STATUS "Got git hash: ${kvazzup_GIT_HASH}")
    endif()
endif()

if(kvazzup_GIT_HASH)
    SET(kvazzup_GIT_HASH "${kvazzup_GIT_HASH}")
endif()

option(RELEASE_COMMIT "Create a release version" OFF)
if(RELEASE_COMMIT)
    set (KVAZZUP_VERSION "${PROJECT_VERSION}")
elseif(kvazzup_GIT_HASH)
    set (KVAZZUP_VERSION "${PROJECT_VERSION}-${kvazzup_GIT_HASH}")
else()
    set (KVAZZUP_VERSION "${PROJECT_VERSION}-source")
    set(kvazzup_GIT_HASH "source")
endif()

message(STATUS "Kvazzup version: ${KVAZZUP_VERSION}")

configure_file(cmake/version.cpp.in version.cpp @ONLY)

if (RELEASE_COMMIT)
    target_compile_definitions(${PROJECT_NAME}_version PRIVATE KVAZZUP_RELEASE_COMMIT)
endif()

