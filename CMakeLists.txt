cmake_minimum_required(VERSION 3.10)
project(AireRecord)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set source file encoding for MSVC
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8 /execution-charset:utf-8")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /source-charset:utf-8 /execution-charset:utf-8")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src/airui/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/sqlite
)

# Source files
file(GLOB_RECURSE AIRUI_SOURCES 
    src/airui/src/*.cpp
)
file(GLOB_RECURSE CORE_SOURCES 
    src/core/*.cpp
)
file(GLOB_RECURSE IPC_SOURCES 
    src/ipc/*.cpp
)
file(GLOB_RECURSE PAGES_SOURCES 
    src/pages/*.cpp
)
file(GLOB_RECURSE PLUGINS_SOURCES 
    src/plugins/*.cpp
)
file(GLOB_RECURSE SQLITE_SOURCES 
    third_party/sqlite/sqlite3.c
)

# Resource file
set(RESOURCE_FILE ${CMAKE_SOURCE_DIR}/res/app.rc)

# Common sources for both executables
set(COMMON_SOURCES
    ${AIRUI_SOURCES}
    ${CORE_SOURCES}
    ${IPC_SOURCES}
    ${PAGES_SOURCES}
    ${PLUGINS_SOURCES}
    ${SQLITE_SOURCES}
)

# Service executable (airecordd.exe)
add_executable(airecordd
    src/service/main.cpp
    ${COMMON_SOURCES}
    ${RESOURCE_FILE}
)
target_link_libraries(airecordd 
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(airecordd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
)

# Client executable (airecord.exe)
add_executable(airecord
    src/main.cpp
    ${COMMON_SOURCES}
    ${RESOURCE_FILE}
)
target_link_libraries(airecord 
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(airecord PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
)

# Test executable
add_executable(test_ui 
    tests/test_ui.cpp
    ${COMMON_SOURCES}
    ${RESOURCE_FILE}
)
target_link_libraries(test_ui 
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_ui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)