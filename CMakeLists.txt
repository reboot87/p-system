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

# Source files - core + new features
set(AIRUI_SOURCES
    src/airui/src/Canvas.cpp
    src/airui/src/Controls.cpp
    src/airui/src/Fonts.cpp
    src/airui/src/Gfx.cpp
    src/airui/src/Page.cpp
    src/airui/src/Shell.cpp
    src/airui/src/Theme.cpp
    src/airui/src/Widgets.cpp
    # New features - Layout & Components
    src/airui/src/Layout.cpp
    src/airui/src/Components.cpp
    # Animation
    src/airui/src/Animation.cpp
    # Responsive
    src/airui/src/Responsive.cpp
    # Accessibility / i18n
    src/airui/src/Accessibility.cpp
    # Theme JSON
    src/airui/src/ThemeJson.cpp
    # Theme Manager encapsulation
    src/airui/src/ThemeManager.cpp
    # TestShell
    src/airui/src/TestShell.cpp
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

# Theme manager test
add_executable(test_theme_manager 
    tests/test_theme_manager.cpp
    ${COMMON_SOURCES}
    ${RESOURCE_FILE}
)
target_link_libraries(test_theme_manager 
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_theme_manager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Draw calls test
add_executable(test_draw_calls 
    tests/test_draw_calls.cpp
    ${COMMON_SOURCES}
    ${RESOURCE_FILE}
)
target_link_libraries(test_draw_calls 
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_draw_calls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Locale test
add_executable(test_locale 
    tests/test_locale.cpp
    ${COMMON_SOURCES}
    ${RESOURCE_FILE}
)
target_link_libraries(test_locale 
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_locale PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)