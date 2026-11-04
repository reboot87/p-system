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
#   core/include      -> airui-core public headers (platform-free)
#   platform/win32    -> airui-win32 public headers (airui/*.h)
include_directories(
    ${CMAKE_SOURCE_DIR}/src/airui/core/include
    ${CMAKE_SOURCE_DIR}/src/airui/platform/win32
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/sqlite
)

# ------------------------------------------------------------
# airui-core: platform-free. Must never link user32/gdi32 and
# must never include a platform header (enforced by
# scripts/check-core-clean.ps1).
# ------------------------------------------------------------
set(AIRUI_CORE_SOURCES
    src/airui/core/src/Theme.cpp
    src/airui/core/src/Layout.cpp
    src/airui/core/src/Animation.cpp
    src/airui/core/src/ControlsCore.cpp
    src/airui/core/src/ShellCore.cpp
)
add_library(airui-core STATIC ${AIRUI_CORE_SOURCES})
target_include_directories(airui-core PUBLIC
    ${CMAKE_SOURCE_DIR}/src/airui/core/include)
if(NOT WIN32)
    target_link_libraries(airui-core PUBLIC pthread)
endif()

# ------------------------------------------------------------
# airui-win32: the Win32 backend (GDI / DWM / native widgets).
# ------------------------------------------------------------
set(AIRUI_SOURCES
    src/airui/platform/win32/Canvas.cpp
    src/airui/platform/win32/Controls.cpp
    src/airui/platform/win32/Fonts.cpp
    src/airui/platform/win32/Gfx.cpp
    src/airui/platform/win32/Page.cpp
    src/airui/platform/win32/Shell.cpp
    src/airui/platform/win32/Widgets.cpp
    src/airui/platform/win32/Components.cpp
    src/airui/platform/win32/Responsive.cpp
    src/airui/platform/win32/Accessibility.cpp
    src/airui/platform/win32/ThemeJson.cpp
    src/airui/platform/win32/ThemeManager.cpp
    src/airui/platform/win32/TestShell.cpp
)
if(WIN32)
    add_library(airui-win32 STATIC ${AIRUI_SOURCES})
    target_include_directories(airui-win32 PUBLIC
        ${CMAKE_SOURCE_DIR}/src/airui/platform/win32)
    target_link_libraries(airui-win32 PUBLIC airui-core
        user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32 uxtheme imm32)
else()
    # Phase 0: the Win32 backend is simply not built on non-Windows
    # targets; airui-core must still configure and compile.
    message(STATUS "AirUI: non-Windows host - building airui-core only")
    add_library(airui-win32 INTERFACE)
    target_link_libraries(airui-win32 INTERFACE airui-core)
endif()
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

# Common sources for both executables.
# AirUI itself is linked as a library (airui-win32), not compiled here.
set(COMMON_SOURCES
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
    airui-win32
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
    airui-win32
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
    airui-win32
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
    airui-win32
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
    airui-win32
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
    airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_locale PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Component gallery renderer
add_executable(test_component_gallery 
    tests/test_component_gallery.cpp
    src/gallery/GalleryDraw.cpp
    ${COMMON_SOURCES}
    ${RESOURCE_FILE}
)
target_link_libraries(test_component_gallery 
    airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_component_gallery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Core layout test - links airui-core only (no Win32 backend)
add_executable(test_core_layout
    tests/test_core_layout.cpp
)
target_link_libraries(test_core_layout airui-core)
set_target_properties(test_core_layout PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Core controls test (PaintButton / PaintCheckbox / PaintRadio)
add_executable(test_controls_core
    tests/test_controls_core.cpp
)
target_link_libraries(test_controls_core airui-core)
set_target_properties(test_controls_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# AirUI interactive component gallery (GUI)
add_executable(airui_gallery
    src/gallery/main.cpp
    src/gallery/GalleryDraw.cpp
    ${RESOURCE_FILE}
)
target_link_libraries(airui_gallery
    airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(airui_gallery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
)
# ------------------------------------------------------------
# Automated tests
#   cd build; ctest -C Release --output-on-failure
# ------------------------------------------------------------
# core cleanliness gate: fails if core/ references any platform API
if(WIN32)
    add_custom_target(check-core-clean
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_SOURCE_DIR}/scripts/check-core-clean.ps1
        VERBATIM)
endif()

# test_ui is an end-to-end script that drives a *running* airecord.exe
# (window lookup + IPC), so it is not registered as a ctest case.
enable_testing()
foreach(_t test_core_layout test_controls_core test_theme_manager test_draw_calls test_locale test_component_gallery)
    if(TARGET ${_t})
        add_test(NAME ${_t} COMMAND ${_t})
    endif()
endforeach()
