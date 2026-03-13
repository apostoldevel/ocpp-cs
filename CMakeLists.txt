cmake_minimum_required(VERSION 3.25)
project(ocpp-cs VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Project identity ─────────────────────────────────────────────────────────

set(APP_NAME        "cs"                      CACHE STRING "Application name")
set(APP_DESCRIPTION "OCPP Central System"     CACHE STRING "Application description")

message(STATUS "App name: ${APP_NAME}")
message(STATUS "App description: ${APP_DESCRIPTION}")

# ─── Settings ─────────────────────────────────────────────────────────────────

set(INSTALL_AS_ROOT  ON  CACHE BOOL "Install to /usr/sbin and /etc (requires root)")

set(WITH_POSTGRESQL  ON  CACHE BOOL "Build with PostgreSQL (libpq)")
set(WITH_SSL         ON  CACHE BOOL "Build with OpenSSL")
set(WITH_CURL        OFF CACHE BOOL "Build with libcurl")

if(INSTALL_AS_ROOT)
    set(INSTALL_BIN_PATH "/usr/sbin")
    set(APP_PREFIX "/etc/${APP_NAME}")
else()
    set(INSTALL_BIN_PATH "/usr/local/sbin")
    set(APP_PREFIX "$ENV{HOME}/.config/${APP_NAME}")
endif()

# ─── Global compile options ──────────────────────────────────────────────────

add_compile_options("$<$<CONFIG:Debug>:-D_DEBUG>")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ─── libapostol (core framework) ─────────────────────────────────────────────

add_subdirectory(src/lib/libapostol)

target_compile_definitions(apostol PUBLIC
    APP_NAME="${APP_NAME}"
    APP_DESCRIPTION="${APP_DESCRIPTION}"
    APP_DEFAULT_LOCALE="en_US.UTF-8"
    APP_VAR="${APP_NAME}"
    APP_OLDPID_EXT=".oldbin"
    APP_DEFAULT_USER="nobody"
    APP_DEFAULT_GROUP="nogroup"
    APP_DEFAULT_LISTEN="0.0.0.0"
    APP_PREFIX="${APP_PREFIX}/"
    APP_CONF_PREFIX="conf/"
    APP_CACHE_PREFIX="cache/"
    APP_SBIN_PATH="sbin/${APP_NAME}"
    APP_CONF_FILE="${APP_NAME}.json"
    APP_PID_FILE="logs/${APP_NAME}.pid"
    APP_LOCK_FILE="logs/${APP_NAME}.lock"
    APP_ERROR_LOG_FILE="logs/error.log"
    APP_ACCESS_LOG_FILE="logs/access.log"
    APP_POSTGRES_LOG_FILE="logs/postgres.log"
    APP_STREAM_LOG_FILE="logs/stream.log"
    APP_DOC_ROOT="www"
    APP_VERSION="${PROJECT_VERSION}"
    APP_DEFAULT_LOG_LEVEL="notice"
    APP_DEFAULT_LOG_MAX_SIZE=104857600
    APP_DEFAULT_LISTEN_BACKLOG=128
    APP_DEFAULT_WORKERS=1
    APP_DEFAULT_PORT=9220
    APP_DEFAULT_SERVER_TIMEOUT=5000
    APP_DEFAULT_PG_CONNECT_TIMEOUT=5
)

# ─── Build-time version header ───────────────────────────────────────────────

set(APOSTOL_VERSION_OUT "${CMAKE_BINARY_DIR}/include/apostol/version.hpp")

add_custom_target(apostol_version
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DPROJECT_VERSION=${PROJECT_VERSION}
        -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        -DPROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        -DVERSION_IN=${CMAKE_SOURCE_DIR}/cmake/version.hpp.in
        -DVERSION_OUT=${APOSTOL_VERSION_OUT}
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
    BYPRODUCTS ${APOSTOL_VERSION_OUT}
    COMMENT "Generating version header"
)

# ─── Module & process auto-discovery ─────────────────────────────────────────

file(GLOB_RECURSE module_files  CONFIGURE_DEPENDS src/modules/*.cpp)
file(GLOB_RECURSE process_files CONFIGURE_DEPENDS src/processes/*.cpp)
file(GLOB_RECURSE ocpp_files    CONFIGURE_DEPENDS src/ocpp/*.cpp)

message(STATUS "Module sources : ${module_files}")
message(STATUS "Process sources: ${process_files}")
message(STATUS "OCPP sources   : ${ocpp_files}")

# ─── Modules & processes library ─────────────────────────────────────────────

set(all_module_sources ${module_files} ${process_files} ${ocpp_files})

if(all_module_sources)
    add_library(apostol_modules STATIC ${all_module_sources})
    set(MOD_SCOPE PUBLIC)
else()
    add_library(apostol_modules INTERFACE)
    set(MOD_SCOPE INTERFACE)
endif()

target_include_directories(apostol_modules
    ${MOD_SCOPE}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/Workers
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/Helpers
        ${CMAKE_CURRENT_SOURCE_DIR}/src/processes
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ocpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/rapidxml
        ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(apostol_modules ${MOD_SCOPE} apostol)

if(all_module_sources)
    add_dependencies(apostol_modules apostol_version)
    target_compile_options(apostol_modules PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
    )
endif()

# ─── Main executable ─────────────────────────────────────────────────────────

file(GLOB_RECURSE app_src_files CONFIGURE_DEPENDS src/app/*.cpp)

message(STATUS "App sources    : ${app_src_files}")

add_executable(${APP_NAME} ${app_src_files})

target_include_directories(${APP_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
)

target_link_libraries(${APP_NAME} PRIVATE apostol_modules)

# ─── Tests ────────────────────────────────────────────────────────────────────

option(BUILD_TESTING "Build unit tests" OFF)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)

    include(CTest)
    include(Catch)

    file(GLOB_RECURSE test_files CONFIGURE_DEPENDS tests/unit/*.cpp)

    if(test_files)
        add_executable(ocpp_tests ${test_files})
        target_link_libraries(ocpp_tests PRIVATE Catch2::Catch2WithMain apostol_modules)
        target_include_directories(ocpp_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/rapidxml
        )
        catch_discover_tests(ocpp_tests)
    endif()
endif()

# ─── Install ──────────────────────────────────────────────────────────────────

set(INSTALL_PATH "${APP_PREFIX}")

install(TARGETS ${APP_NAME} DESTINATION ${INSTALL_BIN_PATH})

install(DIRECTORY DESTINATION ${INSTALL_PATH})
install(DIRECTORY DESTINATION ${INSTALL_PATH}/conf)
install(DIRECTORY DESTINATION ${INSTALL_PATH}/logs)
install(DIRECTORY DESTINATION ${INSTALL_PATH}/cache)

install(DIRECTORY conf/sites/ DESTINATION ${INSTALL_PATH}/sites)
install(DIRECTORY conf/cp/    DESTINATION ${INSTALL_PATH}/cp)
install(DIRECTORY www/        DESTINATION ${INSTALL_PATH}/www)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/conf/default.json")
    install(FILES conf/default.json DESTINATION ${INSTALL_PATH} RENAME ${APP_NAME}.json)
endif()

if(INSTALL_AS_ROOT)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/auto/unit.service")
        install(FILES auto/unit.service
            DESTINATION /lib/systemd/system
            RENAME ${APP_NAME}.service
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
        install(CODE "execute_process(COMMAND systemctl daemon-reload)")
        install(CODE "execute_process(COMMAND systemctl enable ${APP_NAME}.service)")
    endif()
endif()
