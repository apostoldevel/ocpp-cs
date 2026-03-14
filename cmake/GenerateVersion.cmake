cmake_minimum_required(VERSION 3.25)

set(GIT_HASH "unknown")

find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

configure_file("${VERSION_IN}" "${VERSION_OUT}" @ONLY)

message(STATUS "${APP_NAME} version: ${PROJECT_VERSION}+${GIT_HASH}")
