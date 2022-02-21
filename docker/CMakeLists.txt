cmake_minimum_required(VERSION 3.13)
project(apostol-cs)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -rdynamic")

set(PROJECT_NAME "cs")
set(PROJECT_DESCRIPTION "OCPP Central System")

message(STATUS "Project name: ${PROJECT_NAME}")
message(STATUS "Project description: ${PROJECT_DESCRIPTION}")

# Settings
# ----------------------------------------------------------------------------------------------------------------------
set(INSTALL_AS_ROOT    ON  CACHE BOOL "Install as root")
set(USE_SYSTEMD        ON  CACHE BOOL "Use systemd")
set(WITH_AUTHORIZATION OFF CACHE BOOL "Build with authorization")
set(WITH_POSTGRESQL    OFF CACHE BOOL "Build with PostgreSQL")
set(WITH_SSL           ON  CACHE BOOL "Build with SSL")
set(WITH_CURL          OFF CACHE BOOL "Build with cURL")
set(WITH_SQLITE3       OFF CACHE BOOL "Build with Sqlite3")
# ----------------------------------------------------------------------------------------------------------------------

if (INSTALL_AS_ROOT)
    set(INSTALL_BIN_PATH "/usr/sbin")
    set(PROJECT_PREFIX "/etc/${PROJECT_NAME}")
else()
    set(INSTALL_BIN_PATH "/usr/local/sbin")
    set(PROJECT_PREFIX "$ENV{HOME}/.config/${PROJECT_NAME}")
endif()

list( APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake-modules" )

add_compile_options("$<$<CONFIG:DEBUG>:-D_DEBUG>")

set(CORE_LIB_NAME apostol-core)
#set(MODULES_LIB_NAME apostol-modules)

add_custom_target(
        auto_increment_version
        ${CMAKE_COMMAND}
        -D VERSION_FILE=${CMAKE_SOURCE_DIR}/version.h
        -P ${CMAKE_SOURCE_DIR}/AutoVersion.cmake
)

set(PROJECT_LIB_DIR "src/lib")

# Delphi classes for C++
# ----------------------------------------------------------------------------------------------------------------------
set(DELPHI_LIB_NAME delphi)
set(DELPHI_LIB_DIR "${PROJECT_LIB_DIR}/${DELPHI_LIB_NAME}")

# -Iinclude
include_directories(${DELPHI_LIB_DIR}/include)
include_directories(${DELPHI_LIB_DIR}/src)

add_compile_options("-DDELPHI_LIB_EXPORTS")
add_compile_options(-DWWWServerName="${PROJECT_DESCRIPTION}")

if (WITH_AUTHORIZATION)
    add_compile_options("-DWITH_AUTHORIZATION")
endif()

if (WITH_POSTGRESQL)
    set(PQ_LIB_NAME "pq")
    add_compile_options("-DWITH_POSTGRESQL")
endif()

if (WITH_SSL)
    set(SSL_LIB_NAME "ssl")
    add_compile_options("-DWITH_SSL")
endif()

if (WITH_SQLITE3)
    set(SQLITE3_LIB_NAME "sqlite3")
    add_compile_options("-DWITH_SQLITE3")
endif()

if (WITH_CURL)
    set(CURL_LIB_NAME "curl")
    add_compile_options("-DWITH_CURL")
endif()

add_subdirectory(${DELPHI_LIB_DIR})

# Find PkgConfig
#------------------------------------------------------------------------------
find_package( PkgConfig REQUIRED )

set( prefix "${CMAKE_PREFIX_PATH}" )
set( exec_prefix "\${prefix}" )
set( libdir "\${exec_prefix}/lib" )
set( includedir "\${exec_prefix}/include" )

set( pkgconfigdir "${libdir}/pkgconfig" CACHE PATH "Path to pkgconfig directory." )

# Find dl
#------------------------------------------------------------------------------
if ((${CMAKE_SYSTEM_NAME} MATCHES "Linux"))
    find_package( Dl REQUIRED )

    message(STATUS "Dl libs: ${dl_LIBS}")
    message(STATUS "Dl libraries: ${dl_LIBRARIES}")
endif()

# rapidxml
# ----------------------------------------------------------------------------------------------------------------------
include_directories(${PROJECT_LIB_DIR}/rapidxml)
file(GLOB lib_files ${lib_files} ${PROJECT_LIB_DIR}/rapidxml/*.hpp)

# picojson
# ----------------------------------------------------------------------------------------------------------------------
include_directories(${PROJECT_LIB_DIR}/picojson)
file(GLOB lib_files ${lib_files} ${PROJECT_LIB_DIR}/picojson/*.h)

# jwt-cpp
# ----------------------------------------------------------------------------------------------------------------------
include_directories(${PROJECT_LIB_DIR}/jwt-cpp)
file(GLOB lib_files ${lib_files} ${PROJECT_LIB_DIR}/jwt-cpp/*.h)

# Apostol
# ----------------------------------------------------------------------------------------------------------------------
include_directories(src/app src/core src/modules src/modules/Workers src/modules/Helpers src/processes)

file(GLOB app_files version.h src/app/*.hpp src/app/*.cpp)
file(GLOB core_files src/core/*.hpp src/core/*.cpp)
file(GLOB modules_files src/modules/Modules.hpp src/modules/*/*.hpp src/modules/*/*/*.hpp src/modules/*/*/*.cpp)
file(GLOB processes_files src/processes/Processes.hpp src/processes/*/*.hpp src/processes/*/*.cpp)

# Apostol Core
# ----------------------------------------------------------------------------------------------------------------------
add_library(${CORE_LIB_NAME} STATIC
        $<TARGET_OBJECTS:delphi>
        ${lib_files}
        ${core_files}
        ${modules_files}
        ${processes_files}
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
        APP_STREAM_LOG_FILE="logs/stream.log"
        APP_DOC_ROOT="www"
        )

target_link_libraries(${CORE_LIB_NAME} pthread ${PQ_LIB_NAME} ${SQLITE3_LIB_NAME} ${CURL_LIB_NAME} ssl crypto)

# Apostol modules
# ----------------------------------------------------------------------------------------------------------------------
#add_library(${MODULES_LIB_NAME} STATIC ${modules_files})
#target_link_libraries(${MODULES_LIB_NAME} ${CORE_LIB_NAME})

# Apostol
# ----------------------------------------------------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${app_files})
target_link_libraries(${PROJECT_NAME} ${CORE_LIB_NAME})

add_dependencies(${PROJECT_NAME} auto_increment_version)

# Install
# ----------------------------------------------------------------------------------------------------------------------
file(GLOB conf_files conf/*.conf)

set(INSTALL_PATH "${PROJECT_PREFIX}")

install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_BIN_PATH})

install(DIRECTORY DESTINATION ${INSTALL_PATH})
install(DIRECTORY DESTINATION ${INSTALL_PATH}/oauth2)
install(DIRECTORY DESTINATION ${INSTALL_PATH}/conf)
install(DIRECTORY DESTINATION ${INSTALL_PATH}/logs)
install(DIRECTORY DESTINATION ${INSTALL_PATH}/sites)
install(DIRECTORY DESTINATION ${INSTALL_PATH}/cp)
install(DIRECTORY conf/oauth2/ DESTINATION ${INSTALL_PATH}/oauth2)
install(DIRECTORY conf/sites/ DESTINATION ${INSTALL_PATH}/sites)
install(DIRECTORY conf/cp/ DESTINATION ${INSTALL_PATH}/cp)
install(DIRECTORY www/ DESTINATION ${INSTALL_PATH}/www)
install(FILES ${conf_files} DESTINATION ${INSTALL_PATH}/conf)
install(FILES conf/default.conf DESTINATION ${INSTALL_PATH} RENAME ${PROJECT_NAME}.conf)

if (INSTALL_AS_ROOT)
    if (USE_SYSTEMD)
        install(FILES auto/unit.service DESTINATION /etc/systemd/system RENAME ${PROJECT_NAME}.service PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
        install(CODE "execute_process(COMMAND systemctl enable ${PROJECT_NAME}.service)")
    else()
        install(FILES auto/daemon DESTINATION /etc/init.d RENAME ${PROJECT_NAME} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
        install(CODE "execute_process(COMMAND update-rc.d ${PROJECT_NAME} defaults)")
    endif()
endif()
