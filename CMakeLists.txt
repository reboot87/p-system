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
#   platform/skia     -> optional backend headers. The app only needs
#                        the platform-free C ABI header at runtime;
#                        Skia sources/libs are compiled solely into
#                        airui-skia(.dll) when AIRUI_WITH_SKIA=ON.
include_directories(
    ${CMAKE_SOURCE_DIR}/src/airui/core/include
    ${CMAKE_SOURCE_DIR}/src/airui/platform/win32
    ${CMAKE_SOURCE_DIR}/src/airui/platform/skia
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
    src/airui/core/src/ControlsData.cpp
    src/airui/core/src/OverlayCore.cpp
    src/airui/core/src/FrameworkCore.cpp
    src/airui/core/src/ControlsMigrated.cpp
    src/airui/core/src/ControlsInput.cpp
    src/airui/core/src/ControlsChoice.cpp
    src/airui/core/src/ControlsDisplay.cpp
    src/airui/core/src/ControlsFeedback.cpp
    src/airui/core/src/ControlsLayout.cpp
    src/airui/core/src/ControlsAdvancedInput.cpp
    src/airui/core/src/ControlsAdvancedData.cpp
    src/airui/core/src/ControlsAdvancedOverlay.cpp
    src/airui/core/src/ControlsEnterpriseEdit.cpp
    src/airui/core/src/ControlsEnterpriseData.cpp
    src/airui/core/src/ControlsEnterpriseShell.cpp
    src/airui/core/src/ControlsStudioCharts.cpp
    src/airui/core/src/ControlsStudioEditors.cpp
    src/airui/core/src/ControlsStudioShell.cpp
    src/airui/core/src/ControlsClasses.cpp
    src/airui/core/src/ShellCore.cpp
    src/airui/core/src/PaintRecorder.cpp
    src/airui/core/src/PaintBackend.cpp
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
    src/airui/platform/win32/PaintProviders.cpp
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
        user32 gdi32 msimg32 shell32 comctl32 shlwapi advapi32 ole32 uxtheme imm32)
else()
    # Phase 0: the Win32 backend is simply not built on non-Windows
    # targets; airui-core must still configure and compile.
    message(STATUS "AirUI: non-Windows host - building airui-core only")
    add_library(airui-win32 INTERFACE)
    target_link_libraries(airui-win32 INTERFACE airui-core)
endif()

# ------------------------------------------------------------
# Skia is a RUNTIME backend choice, NOT a compile-time one.
#
# The application always contains the runtime registry (core) and the
# dynamic loader (win32). Dropping airui-skia.dll next to the exe is
# enough to make "skia" selectable at runtime, with no SDK and no
# rebuild of the official CMake + VS2017 + GDI product.
#
# AIRUI_WITH_SKIA therefore only controls whether THIS build produces
# Skia code: when ON it builds airui-skia.dll (plus an embedded skia
# provider and the real-SDK tests). It never decides which painter a
# frame uses. Selection is --paint-backend / AIRUI_PAINT_BACKEND /
# ShellOptions.paintBackend / Shell::SetPaintBackend, resolved by
# PainterRegistry with an automatic gdi fallback.
#
# Turn it on only when a Skia SDK is available:
#   -DAIRUI_WITH_SKIA=ON -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
# (vcpkg install skia:x64-windows), or
#   -DAIRUI_WITH_SKIA=ON -DAIRUI_SKIA_ROOT=<prebuilt-prefix>
# See docs/skia-backend.md.
# ------------------------------------------------------------
option(AIRUI_WITH_SKIA
       "Build the Skia backend (plugin DLL + embedded provider); does not select the runtime backend"
       OFF)

set(AIRUI_SKIA_FOUND OFF)
set(AIRUI_SKIA_INCLUDE_DIRS "")
set(AIRUI_SKIA_LIBRARIES "")

if(AIRUI_WITH_SKIA)
    find_package(unofficial-skia CONFIG QUIET)
    if(TARGET unofficial::skia::skia)
        set(AIRUI_SKIA_FOUND ON)
        set(AIRUI_SKIA_LIBRARIES unofficial::skia::skia)
    elseif(AIRUI_SKIA_ROOT)
        find_path(AIRUI_SKIA_INC include/core/SkCanvas.h
            PATHS ${AIRUI_SKIA_ROOT}/include/skia
                  ${AIRUI_SKIA_ROOT}/include
                  ${AIRUI_SKIA_ROOT}
            NO_DEFAULT_PATH)
        find_library(AIRUI_SKIA_LIB NAMES skia skia_lib
            PATHS ${AIRUI_SKIA_ROOT}/lib ${AIRUI_SKIA_ROOT}/lib/Release
                  ${AIRUI_SKIA_ROOT}/out/Release
            NO_DEFAULT_PATH)
        if(AIRUI_SKIA_INC AND AIRUI_SKIA_LIB)
            set(AIRUI_SKIA_FOUND ON)
            set(AIRUI_SKIA_INCLUDE_DIRS ${AIRUI_SKIA_INC})
            set(AIRUI_SKIA_LIBRARIES ${AIRUI_SKIA_LIB})
        endif()
    endif()

    if(NOT AIRUI_SKIA_FOUND)
        message(FATAL_ERROR
            "AIRUI_WITH_SKIA=ON but no Skia SDK was found.\n"
            "  vcpkg : vcpkg install skia:x64-windows, then configure with "
            "-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake\n"
            "  manual: pass -DAIRUI_SKIA_ROOT=<prefix> with headers at "
            "include/skia/include/core and a skia library under lib/\n"
            "See docs/skia-backend.md.")
    endif()

    add_library(airui-skia STATIC
        src/airui/platform/skia/airui/SkiaTypes.cpp
        src/airui/platform/skia/airui/SkiaPainter.cpp
        src/airui/platform/skia/airui/SkiaSurface.cpp
    )
    target_include_directories(airui-skia PUBLIC
        ${CMAKE_SOURCE_DIR}/src/airui/platform/skia
        ${AIRUI_SKIA_INCLUDE_DIRS})
    target_link_libraries(airui-skia
        PUBLIC airui-core
        PRIVATE ${AIRUI_SKIA_LIBRARIES})
    if(WIN32)
        # SkiaSurface presents through a 32bpp DIB + AlphaBlend.
        target_link_libraries(airui-skia PUBLIC msimg32 gdi32 user32)
    endif()
    target_compile_definitions(airui-skia PUBLIC AIRUI_HAVE_SKIA=1)

    if(WIN32)
        # A linked-in provider makes "skia" work even without the DLL;
        # it registers under the same runtime id as the plugin provider.
        # AIRUI_HAVE_SKIA only controls this registration - the choice
        # of painter is still made by PainterRegistry at runtime.
        target_link_libraries(airui-win32 PUBLIC airui-skia)

        # The dynamically loadable backend (stable C ABI, see
        # SkiaPluginAbi.h). Its import library gets a distinct name so
        # it cannot collide with the static airui-skia.lib.
        add_library(airui-skia-plugin SHARED
            src/airui/platform/skia/airui/SkiaPlugin.cpp)
        target_link_libraries(airui-skia-plugin PRIVATE airui-skia)
        set_target_properties(airui-skia-plugin PROPERTIES
            OUTPUT_NAME "airui-skia"
            ARCHIVE_OUTPUT_NAME "airui-skia-import"
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug)
    endif()

    message(STATUS "AirUI: Skia backend built (${AIRUI_SKIA_LIBRARIES}); runtime selection stays dynamic")
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
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
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
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
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
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
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
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
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
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
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
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
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
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Data & navigation controls test (Table / List / Tree / Breadcrumb /
# Pagination / Toast) - core-only, no Win32 backend
add_executable(test_data_controls
    tests/test_data_controls.cpp
)
target_link_libraries(test_data_controls airui-core)
set_target_properties(test_data_controls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Overlay + focus test (layers, modal blocking, tab order, focus trap)
add_executable(test_overlay_core
    tests/test_overlay_core.cpp
)
target_link_libraries(test_overlay_core airui-core)
set_target_properties(test_overlay_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Framework / migrated / extended controls regression (core only)
add_executable(test_framework_extra
    tests/test_framework_extra.cpp
)
target_link_libraries(test_framework_extra airui-core)
set_target_properties(test_framework_extra PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Third wave controls regression (core only)
add_executable(test_advanced_controls
    tests/test_advanced_controls.cpp
)
target_link_libraries(test_advanced_controls airui-core)
set_target_properties(test_advanced_controls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Fourth wave controls regression (core only)
add_executable(test_enterprise_controls
    tests/test_enterprise_controls.cpp
)
target_link_libraries(test_enterprise_controls airui-core)
set_target_properties(test_enterprise_controls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Fifth wave controls regression (core only)
add_executable(test_studio_controls
    tests/test_studio_controls.cpp
)
target_link_libraries(test_studio_controls airui-core)
set_target_properties(test_studio_controls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Object-oriented control layer regression (core only)
add_executable(test_control_classes
    tests/test_control_classes.cpp
)
target_link_libraries(test_control_classes airui-core)
set_target_properties(test_control_classes PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Core layout test - links airui-core only (no Win32 backend)
add_executable(test_core_layout
    tests/test_core_layout.cpp
)
target_link_libraries(test_core_layout airui-core)
set_target_properties(test_core_layout PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Core controls test (PaintButton / PaintCheckbox / PaintRadio)
add_executable(test_controls_core
    tests/test_controls_core.cpp
)
target_link_libraries(test_controls_core airui-core)
set_target_properties(test_controls_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# ShellCore HAL test - platform-free state / layout / hit test / IPainter
add_executable(test_shell_hal
    tests/test_shell_hal.cpp
)
target_link_libraries(test_shell_hal airui-core)
set_target_properties(test_shell_hal PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
# /SUBSYSTEM is MSVC-only; keep this target linkable on Linux/macOS CI.
if(MSVC)
    set_target_properties(test_shell_hal PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

# PaintRecorder golden test - platform-free DrawCall consistency base (G5)
add_executable(test_paint_recorder
    tests/test_paint_recorder.cpp
)
target_link_libraries(test_paint_recorder airui-core)
set_target_properties(test_paint_recorder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
if(MSVC)
    set_target_properties(test_paint_recorder PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

# Runtime painter backend registry: explicit / auto / hot switch /
# graceful gdi fallback. Platform-free, runs on all three OSes.
add_executable(test_paint_backend
    tests/test_paint_backend.cpp
)
target_link_libraries(test_paint_backend airui-core)
set_target_properties(test_paint_backend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
if(MSVC)
    set_target_properties(test_paint_backend PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

# Windows runtime wiring: the default build (no SDK, no DLL) must still
# resolve a "skia" request and fall back to gdi instead of failing.
if(WIN32)
    add_executable(test_paint_runtime tests/test_paint_runtime.cpp)
    target_link_libraries(test_paint_runtime PRIVATE airui-win32 airui-core)
    set_target_properties(test_paint_runtime PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    )
    if(MSVC)
        set_target_properties(test_paint_runtime PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endif()
endif()

# Skia backend value mapping. NO Skia SDK needed: this is a pure-C++
# test of the conversion layer, so it runs in ctest on all three OSes.
add_executable(test_skia_types
    tests/test_skia_types.cpp
    src/airui/platform/skia/airui/SkiaTypes.cpp
)
target_include_directories(test_skia_types PRIVATE
    ${CMAKE_SOURCE_DIR}/src/airui/platform/skia)
target_link_libraries(test_skia_types PRIVATE airui-core)
set_target_properties(test_skia_types PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
if(MSVC)
    set_target_properties(test_skia_types PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

# Real dual-backend check: needs a Skia SDK (AIRUI_WITH_SKIA=ON).
# Compares the platform-free PaintRecorder golden against the exact
# IPainter call sequence received by the Skia backend, and verifies
# raster pixels (G5, docs/cross-platform.md).
if(AIRUI_WITH_SKIA)
    add_executable(test_skia_backend tests/test_skia_backend.cpp)
    target_link_libraries(test_skia_backend PRIVATE airui-skia airui-core)
    set_target_properties(test_skia_backend PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    )
    if(MSVC)
        set_target_properties(test_skia_backend PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endif()

    # Proves Skia is selectable at runtime from airui-skia.dll, not by
    # a compile-time switch. Loads the plugin through the C ABI, draws
    # across the module boundary and checks the raster pixels.
    if(WIN32)
        add_executable(test_skia_plugin tests/test_skia_plugin.cpp)
        target_link_libraries(test_skia_plugin PRIVATE airui-win32 airui-core)
        set_target_properties(test_skia_plugin PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
        )
        if(MSVC)
            set_target_properties(test_skia_plugin PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
        endif()
    endif()
endif()

# AirUI reuse demo (examples/DemoWindow.cpp) — minimal Shell+Page example
add_executable(airui_demo
    examples/DemoWindow.cpp
)
target_link_libraries(airui_demo
    airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(airui_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
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
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
)
# IPC pipe naming test (per-session isolation: name must carry _s<sessionId>)
add_executable(test_ipc_pipe
    tests/test_ipc_pipe.cpp
    src/ipc/Ipc.cpp
    src/core/MachineConfig.cpp
)
target_link_libraries(test_ipc_pipe advapi32)
set_target_properties(test_ipc_pipe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
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
foreach(_t test_core_layout test_controls_core test_shell_hal test_paint_recorder test_paint_backend test_paint_runtime test_skia_types test_skia_backend test_skia_plugin test_data_controls test_overlay_core test_framework_extra test_advanced_controls test_enterprise_controls test_studio_controls test_control_classes test_theme_manager test_draw_calls test_locale test_component_gallery test_ipc_pipe)
    if(TARGET ${_t})
        add_test(NAME ${_t} COMMAND ${_t})
    endif()
endforeach()
