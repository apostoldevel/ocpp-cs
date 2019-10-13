find_package(Git)

if (GIT_FOUND)
    message(STATUS "Found Git")
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            OUTPUT_VARIABLE GIT_REVISION
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Current Git commit hash: " ${GIT_REVISION})
endif()

file(READ ${VERSION_FILE} VERSION_TEXT)

message(STATUS "FILE: ${VERSION_TEXT}")

string(REGEX MATCH "\"([0-9]+)\\.([0-9]+)\\.([0-9a-zA-Z]+)\\-b([0-9]+)\"" MY_MATCHES "${VERSION_TEXT}")

set(VAR_MAJOR ${CMAKE_MATCH_1})
set(VAR_MINOR ${CMAKE_MATCH_2})
set(VAR_PATCH ${CMAKE_MATCH_3})
set(VAR_TWEAK ${CMAKE_MATCH_4})

message(STATUS "MATCH: ${VAR_MAJOR}.${VAR_MINOR}.${VAR_PATCH}-b${VAR_TWEAK}")

if (${GIT_REVISION} STREQUAL ${VAR_PATCH})
    math(EXPR VAR_TWEAK "${VAR_TWEAK}+1")
else()
    math(EXPR VAR_TWEAK "1")
endif()

set(VAR_PATCH ${GIT_REVISION})

message(STATUS "AUTO_VERSION: ${VAR_MAJOR}.${VAR_MINOR}.${VAR_PATCH}-b${VAR_TWEAK}")

string(REGEX REPLACE
        "\"([0-9]+)\\.([0-9]+)\\.([0-9a-zA-Z]+)\\-b([0-9]+)\""
        "\"${VAR_MAJOR}.${VAR_MINOR}.${VAR_PATCH}-b${VAR_TWEAK}\""
        VERSION_TEXT "${VERSION_TEXT}")

file(WRITE ${VERSION_FILE} "${VERSION_TEXT}")
