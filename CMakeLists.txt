cmake_minimum_required(VERSION 3.15)
project(AireRecord VERSION 0.9.0)

# ------------------------------------------------------------
# Single version source: project(AireRecord VERSION ...).
# configure_file() turns cmake/Version.h.in into a generated
# header exposing AIRUI_VERSION (narrow) / AIRUI_VERSION_W
# (wide). Every target that surfaces the version includes it
# via ${CMAKE_BINARY_DIR}/generated. No hardcoded "vX.Y" in src/.
# ------------------------------------------------------------
configure_file(cmake/Version.h.in
               ${CMAKE_BINARY_DIR}/generated/airui/Version.h @ONLY)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# P1-5 phase 2: emit compile_commands.json so clang-tidy (and IDEs) can do
# accurate cross-translation-unit analysis instead of single-file heuristics.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Console test targets must use /SUBSYSTEM:CONSOLE on MSVC. That link flag
# is MSVC-only, so it is guarded here so Linux/macOS can link the same
# platform-free targets (see docs/remediation-plan.md P2-2).
function(airui_console_target name)
    if(MSVC)
        set_target_properties(${name} PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endif()
endfunction()

# Set source file encoding for MSVC
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8 /execution-charset:utf-8")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /source-charset:utf-8 /execution-charset:utf-8")
endif()

# ------------------------------------------------------------
# airui-core: platform-free. Must never link user32/gdi32 and
# must never include a platform header (enforced by
# scripts/check-core-clean.ps1).
# ------------------------------------------------------------
set(AIRUI_CORE_SOURCES
    src/airui/core/src/Theme.cpp
    src/airui/core/src/Layout.cpp
    src/airui/core/src/DeclarativeLayout.cpp
    src/airui/core/src/DeclarativeJson.cpp
    src/airui/core/src/DeclarativeRender.cpp
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
    ${CMAKE_SOURCE_DIR}/src/airui/core/include
    ${CMAKE_BINARY_DIR}/generated)
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
    src/airui/platform/win32/OverlayWindow.cpp
    src/airui/platform/win32/PaintProviders.cpp
    src/airui/platform/win32/PaintFrame.cpp
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
        ${CMAKE_SOURCE_DIR}/src/airui/platform/win32
        ${CMAKE_SOURCE_DIR}/src/airui/core/include)
    target_include_directories(airui-win32 PRIVATE
        ${CMAKE_SOURCE_DIR}/src/airui/platform/skia
        ${CMAKE_SOURCE_DIR}/src)
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
        ${CMAKE_SOURCE_DIR}/src
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

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    src/core/*.cpp
)
file(GLOB_RECURSE IPC_SOURCES CONFIGURE_DEPENDS
    src/ipc/*.cpp
)
file(GLOB_RECURSE PAGES_SOURCES CONFIGURE_DEPENDS
    src/pages/*.cpp
)
file(GLOB_RECURSE PLUGINS_SOURCES CONFIGURE_DEPENDS
    src/plugins/*.cpp
)
file(GLOB_RECURSE AGENT_SOURCES CONFIGURE_DEPENDS
    src/agent/*.cpp
)

# Resource file
set(RESOURCE_FILE ${CMAKE_SOURCE_DIR}/res/app.rc)

# Business-layer sources are compiled once into a shared static library
# (app-common) instead of being recompiled into every executable / test
# target. sqlite3.c (~9 MB) likewise lives in its own static lib so it is
# compiled exactly once. See docs/remediation-plan.md P2-2.
add_library(app-common STATIC
    ${CORE_SOURCES}
    ${IPC_SOURCES}
    ${PAGES_SOURCES}
    ${PLUGINS_SOURCES}
    ${AGENT_SOURCES}
)
target_include_directories(app-common PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)
target_include_directories(app-common PRIVATE
    ${CMAKE_SOURCE_DIR}/src/airui/core/include
    ${CMAKE_SOURCE_DIR}/src/airui/platform/win32
    ${CMAKE_SOURCE_DIR}/third_party/sqlite)

add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_SOURCE_DIR}/third_party/sqlite)

# Service executable (airecordd.exe)
add_executable(airecordd
    src/service/main.cpp
    ${RESOURCE_FILE}
)
target_include_directories(airecordd PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(airecordd 
    PRIVATE app-common sqlite3 airui-win32
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
    ${RESOURCE_FILE}
)
target_include_directories(airecord PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(airecord 
    PRIVATE app-common sqlite3 airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(airecord PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
)

# ------------------------------------------------------------
# Skia plugin auto-deploy (no manual DLL copy, no separate command).
#
# The Skia backend is a runtime DLL (airui-skia.dll + its transitive
# vcpkg runtime DLLs: skia, icu, harfbuzz...). When the app is built
# WITHOUT a local Skia SDK (the normal Windows path), the plugin comes
# from a prebuilt location. We resolve it and copy ONLY the *.dll files
# next to the exe on every build, so double-clicking the exe "just
# works" with Skia and a developer's own freshly-built exe is never
# overwritten.
#
# Resolution order (best-effort; the build never fails if nothing is
# found -- the app silently falls back to GDI):
#   1. -DAIRUI_SKIA_PLUGIN_DIR=<dir>                 (explicit)
#   2. <binary-dir>/ci-all/airui-skia-windows-x64    (manual drop)
#   3. <binary-dir>/ci-skia                          (fetched from CI)
# Step 3 is populated automatically by `gh run download` inside
# scripts/deploy-skia-plugin.cmake when `gh` + network + repo access
# are available, so a plain `cmake --build` is enough -- no separate
# fetch command, no manual copy.
# ------------------------------------------------------------
set(AIRUI_SKIA_PLUGIN_DIR "" CACHE PATH
    "Directory containing airui-skia.dll and its dependencies")

function(airui_deploy_skia_plugin tgt)
    if(NOT WIN32)
        return()
    endif()
    if(AIRUI_WITH_SKIA)
        # Built locally: the airui-skia-plugin target emits airui-skia.dll
        # into the same output dir, nothing to copy.
        return()
    endif()
    # The CI-published plugin is x64; a 32-bit exe cannot LoadLibrary it,
    # so don't fetch a useless bundle next to a Win32 build.
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        return()
    endif()

    # POST_BUILD runs after the target links; deploy-skia-plugin.cmake
    # copies only *.dll files so the exe is left untouched. It is best-
    # effort: offline / no-gh builds simply fall back to GDI.
    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DTARGET_DIR="$<TARGET_FILE_DIR:${tgt}>"
                -DBINARY_DIR="${CMAKE_BINARY_DIR}"
                -DPLUGIN_DIR="${AIRUI_SKIA_PLUGIN_DIR}"
                -P "${CMAKE_SOURCE_DIR}/scripts/deploy-skia-plugin.cmake"
        COMMENT "Deploying Skia plugin next to ${tgt} (best-effort)")
endfunction()

# One-time helper: download the latest Skia plugin artifact from CI into
# <binary-dir>/ci-skia. Requires `gh` + network + repo access. After it
# runs once, plain `cmake --build` keeps the plugin deployed via the
# POST_BUILD step above.
if(WIN32 AND NOT AIRUI_WITH_SKIA)
    add_custom_target(fetch-skia-plugin
        COMMAND ${CMAKE_COMMAND}
                -DCMAKE_BINARY_DIR="${CMAKE_BINARY_DIR}"
                -P "${CMAKE_SOURCE_DIR}/scripts/fetch-skia-plugin.cmake"
        COMMENT "Download latest Skia plugin artifact from CI")
endif()

airui_deploy_skia_plugin(airecord)

# Test executable
add_executable(test_ui 
    tests/test_ui.cpp
    ${RESOURCE_FILE}
)
target_include_directories(test_ui PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(test_ui 
    PRIVATE app-common sqlite3 airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_ui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_ui)

# Theme manager test
add_executable(test_theme_manager 
    tests/test_theme_manager.cpp
    ${RESOURCE_FILE}
)
target_include_directories(test_theme_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(test_theme_manager 
    PRIVATE app-common sqlite3 airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_theme_manager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_theme_manager)

# Draw calls test
add_executable(test_draw_calls 
    tests/test_draw_calls.cpp
    ${RESOURCE_FILE}
)
target_include_directories(test_draw_calls PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(test_draw_calls 
    PRIVATE app-common sqlite3 airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_draw_calls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_draw_calls)

# Locale test
add_executable(test_locale 
    tests/test_locale.cpp
    ${RESOURCE_FILE}
)
target_include_directories(test_locale PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(test_locale 
    PRIVATE app-common sqlite3 airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_locale PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_locale)

# Accessibility test: WM_GETOBJECT -> ObjectFromLresult -> IAccessible
# round-trip through the real a11y property table (see a11y::HandleGetObject).
add_executable(test_accessibility 
    tests/test_accessibility.cpp
)
target_include_directories(test_accessibility PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(test_accessibility 
    PRIVATE airui-win32
    user32 gdi32 ole32 oleaut32 oleacc uuid
)
set_target_properties(test_accessibility PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_accessibility)

# Component gallery renderer
add_executable(test_component_gallery 
    tests/test_component_gallery.cpp
    src/gallery/GalleryDraw.cpp
    ${RESOURCE_FILE}
)
target_include_directories(test_component_gallery PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(test_component_gallery 
    PRIVATE app-common sqlite3 airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(test_component_gallery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_component_gallery)

# Data & navigation controls test (Table / List / Tree / Breadcrumb /
# Pagination / Toast) - core-only, no Win32 backend
add_executable(test_data_controls
    tests/test_data_controls.cpp
)
target_link_libraries(test_data_controls airui-core)
set_target_properties(test_data_controls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_data_controls)

# Overlay + focus test (layers, modal blocking, tab order, focus trap)
add_executable(test_overlay_core
    tests/test_overlay_core.cpp
)
target_link_libraries(test_overlay_core airui-core)
set_target_properties(test_overlay_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_overlay_core)

# Framework / migrated / extended controls regression (core only)
add_executable(test_framework_extra
    tests/test_framework_extra.cpp
)
target_link_libraries(test_framework_extra airui-core)
set_target_properties(test_framework_extra PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_framework_extra)

# Third wave controls regression (core only)
add_executable(test_advanced_controls
    tests/test_advanced_controls.cpp
)
target_link_libraries(test_advanced_controls airui-core)
set_target_properties(test_advanced_controls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_advanced_controls)

# Fourth wave controls regression (core only)
add_executable(test_enterprise_controls
    tests/test_enterprise_controls.cpp
)
target_link_libraries(test_enterprise_controls airui-core)
set_target_properties(test_enterprise_controls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_enterprise_controls)

# Fifth wave controls regression (core only)
add_executable(test_studio_controls
    tests/test_studio_controls.cpp
)
target_link_libraries(test_studio_controls airui-core)
set_target_properties(test_studio_controls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_studio_controls)

# Object-oriented control layer regression (core only)
add_executable(test_control_classes
    tests/test_control_classes.cpp
)
target_link_libraries(test_control_classes airui-core)
set_target_properties(test_control_classes PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_control_classes)

# Core layout test - links airui-core only (no Win32 backend)
add_executable(test_core_layout
    tests/test_core_layout.cpp
)
target_link_libraries(test_core_layout airui-core)
set_target_properties(test_core_layout PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_core_layout)

# Declarative layout model + measure/layout engine (core only)
add_executable(test_declarative_layout
    tests/test_declarative_layout.cpp
)
target_link_libraries(test_declarative_layout airui-core)
set_target_properties(test_declarative_layout PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_declarative_layout)

# Declarative JSON inflate (core only)
add_executable(test_declarative_json
    tests/test_declarative_json.cpp
)
target_link_libraries(test_declarative_json airui-core)
set_target_properties(test_declarative_json PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_declarative_json)

# Declarative interaction: binding, hit-testing, click dispatch (core only)
add_executable(test_declarative_interact
    tests/test_declarative_interact.cpp
)
target_link_libraries(test_declarative_interact airui-core)
set_target_properties(test_declarative_interact PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_declarative_interact)

# Declarative render bridge (core only, PaintRecorder assertions)
add_executable(test_declarative_render
    tests/test_declarative_render.cpp
)
target_link_libraries(test_declarative_render airui-core)
set_target_properties(test_declarative_render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_declarative_render)

# Declarative gallery demo end-to-end (core-only; drives the extra demo)
add_executable(test_declarative_gallery
    tests/test_declarative_gallery.cpp
)
target_link_libraries(test_declarative_gallery airui-core)
target_include_directories(test_declarative_gallery PRIVATE
    ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_declarative_gallery PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}")
set_target_properties(test_declarative_gallery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_declarative_gallery)

# Core controls test (PaintButton / PaintCheckbox / PaintRadio)
add_executable(test_controls_core
    tests/test_controls_core.cpp
)
target_link_libraries(test_controls_core airui-core)
set_target_properties(test_controls_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_controls_core)

# HotState test - 自绘热区的悬停/按下/拖出取消语义（平台无关）
add_executable(test_hot_state
    tests/test_hot_state.cpp
)
target_link_libraries(test_hot_state airui-core)
set_target_properties(test_hot_state PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_hot_state)

# ShellCore HAL test - platform-free state / layout / hit test / IPainter
add_executable(test_shell_hal
    tests/test_shell_hal.cpp
)
target_link_libraries(test_shell_hal airui-core)
set_target_properties(test_shell_hal PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_shell_hal)

# PaintRecorder golden test - platform-free DrawCall consistency base (G5)
add_executable(test_paint_recorder
    tests/test_paint_recorder.cpp
)
target_link_libraries(test_paint_recorder airui-core)
set_target_properties(test_paint_recorder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_paint_recorder)

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
airui_console_target(test_paint_backend)

# Windows runtime wiring: the default build (no SDK, no DLL) must still
# resolve a "skia" request and fall back to gdi instead of failing.
if(WIN32)
    add_executable(test_paint_runtime tests/test_paint_runtime.cpp)
    target_link_libraries(test_paint_runtime PRIVATE airui-win32 airui-core)
    set_target_properties(test_paint_runtime PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    )
    airui_console_target(test_paint_runtime)

    # GDI vs Skia raster diff on an identical IPainter call sequence.
    # Runs without a Skia SDK (GDI baseline); with airui-skia.dll deployed
    # it reports the per-primitive pixel difference. See the command
    # `test_paint_compare` in docs/skia-backend.md.
    add_executable(test_paint_compare tests/test_paint_compare.cpp)
    target_link_libraries(test_paint_compare PRIVATE airui-win32 airui-core)
    set_target_properties(test_paint_compare PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    )
    airui_console_target(test_paint_compare)

    # Typography lab: renders one string per role through both painters
    # and prints the raster as ASCII plus coverage statistics, so a
    # "these two fonts look different" report can be turned into named
    # causes (weight, subpixel phase, baseline, advance grid).
    # NOTE: this is a MANUAL diagnostic tool, not an assertion-based test:
    # it always exits 0 (pure measurement/print) and therefore is NOT
    # registered in the ctest foreach list below.
    add_executable(test_font_lab tests/test_font_lab.cpp)
    target_link_libraries(test_font_lab PRIVATE airui-win32 airui-core)
    set_target_properties(test_font_lab PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    )
    airui_console_target(test_font_lab)

    # Whole-window frame routing: every gfx primitive must reach the
    # selected backend, and a closed frame must restore GDI untouched.
    add_executable(test_paint_frame tests/test_paint_frame.cpp)
    target_link_libraries(test_paint_frame PRIVATE airui-win32 airui-core)
    set_target_properties(test_paint_frame PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    )
    airui_console_target(test_paint_frame)
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
airui_console_target(test_skia_types)

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
    airui_console_target(test_skia_backend)

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
        airui_console_target(test_skia_plugin)
        airui_deploy_skia_plugin(test_skia_plugin)
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

# AirUI TextBox lab (examples/TextBoxLab.cpp) — self-drawn TextBox
# repro harness: single/multi-line boxes + caret-blink phase probe
add_executable(airui_textbox_lab
    examples/TextBoxLab.cpp
)
target_link_libraries(airui_textbox_lab
    airui-win32
    user32 gdi32 shell32 comctl32 shlwapi advapi32 ole32
)
set_target_properties(airui_textbox_lab PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
)

# Cross-process drivers for the TextBox lab (tools/diagnostics/*.c).
# Manual diagnostics, NOT assertion tests -> not registered in ctest.
# They send real WM_* messages to a running airui_textbox_lab.exe and
# print measurements; textbox_lab_suite.ps1 drives the T1/T2/T3 mouse
# scenarios end to end. See tools/diagnostics/README.md.
if(WIN32)
    foreach(_drv labtest menuclick boxrect)
        add_executable(${_drv} tools/diagnostics/${_drv}.c)
        target_link_libraries(${_drv} PRIVATE user32)
        set_target_properties(${_drv} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
        )
        airui_console_target(${_drv})
    endforeach()
endif()

# AirUI interactive component gallery (GUI)
add_executable(airui_gallery
    src/gallery/main.cpp
    src/gallery/GalleryDraw.cpp
    ${RESOURCE_FILE}
)
target_include_directories(airui_gallery PRIVATE ${CMAKE_SOURCE_DIR}/src)
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
target_include_directories(test_ipc_pipe PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/sqlite)
target_link_libraries(test_ipc_pipe advapi32)
set_target_properties(test_ipc_pipe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
)
airui_console_target(test_ipc_pipe)

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

# ------------------------------------------------------------
# Self-contained public-header gate (see docs/ui-library.md §6).
#
# Every PUBLIC header of airui-core / airui-win32 must be
# self-contained: a tiny TU that includes exactly that one header
# (plus a trivial main) must compile standalone. A header that only
# compiles after another header is included first is a library bug.
#
# Implementation: configure-time `check_cxx_source_compiles` per
# header, with the include dirs the library actually exposes. Robust
# under MSVC/VS (no per-header TU trickery). Guarded by
# AIRUI_CHECK_PUBLIC_HEADERS (default ON on Windows where both libs
# build; core is platform-free so it is always checkable). win32
# headers are Windows-only and only checked on WIN32.
# ------------------------------------------------------------
option(AIRUI_CHECK_PUBLIC_HEADERS
       "Compile-check that every public header is self-contained (see docs/ui-library.md §6)"
       ON)

if(AIRUI_CHECK_PUBLIC_HEADERS)
    include(CheckCXXSourceCompiles)

    # airui_check_public_headers(HEADERS <...> INCLUDE_DIRS <...>)
    #   For each header, verify a TU including exactly it compiles.
    function(airui_check_public_headers)
        set(multiValueArgs HEADERS INCLUDE_DIRS)
        cmake_parse_arguments(PH "" "" "${multiValueArgs}" ${ARGN})
        if(NOT PH_HEADERS)
            return()
        endif()
        set(_saved_incs "${CMAKE_REQUIRED_INCLUDES}")
        foreach(_inc IN LISTS PH_INCLUDE_DIRS)
            list(APPEND CMAKE_REQUIRED_INCLUDES "${_inc}")
        endforeach()
        foreach(_h IN LISTS PH_HEADERS)
            get_filename_component(_abs "${_h}" ABSOLUTE)
            set(_src "#include \"${_abs}\"\nint main(){return 0;}\n")
            # Include a content hash so editing a header invalidates the cached
            # result (check_cxx_source_compiles caches by variable name).
            file(SHA1 "${_abs}" _hsha)
            string(MAKE_C_IDENTIFIER "${_abs}_${_hsha}" _chk)
            check_cxx_source_compiles("${_src}" "PUBLIC_HEADER_OK_${_chk}")
            if(NOT PUBLIC_HEADER_OK_${_chk})
                message(FATAL_ERROR
                    "Self-containment check FAILED for public header: ${_abs}\n"
                    "A user must be able to #include this one header alone. It is "
                    "missing an #include / forward-declaration it depends on. Fix the "
                    "header (add the missing include), then reconfigure.")
            endif()
        endforeach()
        set(CMAKE_REQUIRED_INCLUDES "${_saved_incs}")
    endfunction()

    # Core public headers (platform-free -> always checkable).
    file(GLOB AIRUI_CORE_PUBLIC_HEADERS
        ${CMAKE_SOURCE_DIR}/src/airui/core/include/airui/*.h)
    airui_check_public_headers(
        HEADERS ${AIRUI_CORE_PUBLIC_HEADERS}
        INCLUDE_DIRS
            ${CMAKE_SOURCE_DIR}/src/airui/core/include
            ${CMAKE_BINARY_DIR}/generated)

    # win32 public headers (Windows-only -> only checked on WIN32).
    if(WIN32)
        file(GLOB AIRUI_WIN32_PUBLIC_HEADERS
            ${CMAKE_SOURCE_DIR}/src/airui/platform/win32/airui/*.h)
        airui_check_public_headers(
            HEADERS ${AIRUI_WIN32_PUBLIC_HEADERS}
            INCLUDE_DIRS
                ${CMAKE_SOURCE_DIR}/src/airui/platform/win32
                ${CMAKE_SOURCE_DIR}/src/airui/core/include
                ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/src/pluginapi
                ${CMAKE_BINARY_DIR}/generated)
    endif()

    # Re-runnable gate target for local/CI use. The main configure-time gate
    # above already runs on every configure; this target forces a CLEAN re-check
    # into a scratch build dir (a fresh cache means no stale per-header result is
    # reused), so after editing a header a developer can re-verify with:
    #   cmake --build build --target check-public-headers
    add_custom_target(check-public-headers
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/public-header-check
        COMMAND ${CMAKE_COMMAND}
                -S ${CMAKE_SOURCE_DIR}
                -B ${CMAKE_BINARY_DIR}/public-header-check
                -DAIRUI_CHECK_PUBLIC_HEADERS=ON
                -DAIRUI_WITH_SKIA=OFF
        COMMENT "check-public-headers: clean self-containment gate for every public header"
        VERBATIM)
endif()

# test_ui is an end-to-end script that drives a *running* airecord.exe
# (window lookup + IPC), so it is not registered as a ctest case.
enable_testing()
foreach(_t test_core_layout test_declarative_layout test_declarative_json test_declarative_interact test_declarative_render test_declarative_gallery test_controls_core test_hot_state test_shell_hal test_paint_recorder test_paint_backend test_paint_runtime test_paint_frame test_paint_compare test_skia_types test_skia_backend test_skia_plugin test_data_controls test_overlay_core test_framework_extra test_advanced_controls test_enterprise_controls test_studio_controls test_control_classes test_theme_manager test_draw_calls test_locale test_accessibility test_component_gallery test_ipc_pipe)
    # Several of these targets are platform-conditional (WIN32-only or
    # AIRUI_WITH_SKIA / Skia-plugin builds), so they legitimately do not exist
    # on every platform. Register the ones that ARE present; do not FATAL on a
    # missing target (a missing Win32-only target on Linux is expected).
    if(TARGET ${_t})
        add_test(NAME ${_t} COMMAND ${_t})
    endif()
endforeach()

# NSIS installer (best-effort; requires makensis.exe). Not added to ALL so
# the build/ctest never fail when NSIS is absent. Build explicitly with:
#   cmake --build build --config Release --target installer
add_custom_target(installer
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/packaging/build-installer.ps1
    COMMENT "Building AireRecord NSIS installer"
    VERBATIM)
