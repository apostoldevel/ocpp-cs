cmake_minimum_required(VERSION 3.13)
project(apostol-ocpp)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -rdynamic")

set(PROJECT_NAME "ocpp")
set(PROJECT_DESCRIPTION "OCPP Central System Service")

message(STATUS "Project name: ${PROJECT_NAME}")
message(STATUS "Project description: ${PROJECT_DESCRIPTION}")

# Settings
# ----------------------------------------------------------------------------------------------------------------------
set(INSTALL_AS_ROOT  ON  CACHE BOOL "Install as root")
set(WITH_POSTGRESQL  ON  CACHE BOOL "Build with PostgreSQL")
set(WITH_SQLITE3     OFF CACHE BOOL "Build with Sqlite3")
# ----------------------------------------------------------------------------------------------------------------------

if (INSTALL_AS_ROOT)
    set(INSTALL_BIN_PATH "/usr/sbin")
    set(PROJECT_PREFIX "/etc/${PROJECT_NAME}")
else()
    set(INSTALL_BIN_PATH "/usr/bin")
    set(PROJECT_PREFIX "$ENV{HOME}/.${PROJECT_NAME}")
endif()

add_compile_options("$<$<CONFIG:DEBUG>:-D_DEBUG>")

set(CORE_LIB_NAME apostol-core)
set(MODULES_LIB_NAME apostol-modules)

add_custom_target(
        auto_increment_version
        ${CMAKE_COMMAND}
        -D VERSION_FILE=${CMAKE_SOURCE_DIR}/version.h
        -P ${CMAKE_SOURCE_DIR}/AutoVersion.cmake
)

# Delphi classes for C++
# ----------------------------------------------------------------------------------------------------------------------
add_compile_options("-DDELPHI_LIB_EXPORTS")
add_compile_options(-DWWWServerName="${PROJECT_DESCRIPTION}")

if (WITH_POSTGRESQL)
    set(PQ_LIB_NAME "pq")
    add_compile_options("-DWITH_POSTGRESQL")
endif()

if (WITH_SQLITE3)
    set(SQLITE3_LIB_NAME "sqlite3")
    add_compile_options("-DWITH_SQLITE3")
endif()

set(DELPHI_LIB_DIR "src/lib/delphi")

add_subdirectory(${DELPHI_LIB_DIR})

# -Iinclude
include_directories(${DELPHI_LIB_DIR}/include)
include_directories(${DELPHI_LIB_DIR}/src)

# Apostol
# ----------------------------------------------------------------------------------------------------------------------
include_directories(src/app src/core src/modules src/lib/rapidxml)

file(GLOB app_files version.h src/app/*.hpp src/app/*.cpp)
file(GLOB core_files src/core/*.hpp src/core/*.cpp)
file(GLOB modules_files src/modules/Modules.hpp src/modules/*/*.hpp src/modules/*/*.cpp)
file(GLOB lib_files src/lib/rapidxml/*.hpp)

# Apostol Core
# ----------------------------------------------------------------------------------------------------------------------
add_library(${CORE_LIB_NAME} STATIC
        $<TARGET_OBJECTS:delphi>
        ${core_files}
        )

target_compile_definitions(${CORE_LIB_NAME} PUBLIC
        APP_NAME="${PROJECT_NAME}"
        APP_DESCRIPTION="${PROJECT_DESCRIPTION}"
        APP_DEFAULT_LOCALE="en_US.UTF-8"
        APP_VAR="${PROJECT_NAME}"
        APP_OLDPID_EXT=".oldbin"
        APP_DEFAULT_USER="nobody"
        APP_DEFAULT_GROUP="nogroup"
        APP_DEFAULT_LISTEN="0.0.0.0"
        APP_PREFIX="${PROJECT_PREFIX}/"
        APP_CONF_PREFIX="conf/"
        APP_CACHE_PREFIX="cache/"
        APP_SBIN_PATH="sbin/${PROJECT_NAME}"
        APP_CONF_FILE="${PROJECT_NAME}.conf"
        APP_PID_FILE="logs/${PROJECT_NAME}.pid"
        APP_LOCK_FILE="logs/${PROJECT_NAME}.lock"
        APP_ERROR_LOG_FILE="logs/error.log"
        APP_ACCESS_LOG_FILE="logs/access.log"
        APP_POSTGRES_LOG_FILE="logs/postgres.log"
        APP_DOC_ROOT="www/"
        )

target_link_libraries(${CORE_LIB_NAME} dl pthread ${PQ_LIB_NAME} ${SQLITE3_LIB_NAME})

# Apostol modules
# ----------------------------------------------------------------------------------------------------------------------
add_library(${MODULES_LIB_NAME} STATIC ${modules_files})
target_link_libraries(${MODULES_LIB_NAME} ${CORE_LIB_NAME} crypto)

# Apostol
# ----------------------------------------------------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${lib_files} ${app_files})
target_link_libraries(${PROJECT_NAME} ${MODULES_LIB_NAME})

add_dependencies(${PROJECT_NAME} auto_increment_version)

# Install
# ----------------------------------------------------------------------------------------------------------------------
set(INSTALL_PATH "${PROJECT_PREFIX}")

install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_BIN_PATH})

install(DIRECTORY DESTINATION ${INSTALL_PATH})
install(DIRECTORY DESTINATION ${INSTALL_PATH}/conf)
install(DIRECTORY DESTINATION ${INSTALL_PATH}/logs)
install(DIRECTORY www/ DESTINATION ${INSTALL_PATH}/www)
install(FILES conf/default.conf DESTINATION ${INSTALL_PATH}/conf)
install(FILES conf/default.conf DESTINATION ${INSTALL_PATH} RENAME ${PROJECT_NAME}.conf)

if (INSTALL_AS_ROOT)
    install(FILES auto/daemon DESTINATION /etc/init.d RENAME ${PROJECT_NAME} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    install(CODE "execute_process(COMMAND update-rc.d ${PROJECT_NAME} defaults)")
endif()
