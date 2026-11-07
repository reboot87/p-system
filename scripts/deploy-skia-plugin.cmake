# ============================================================
# deploy-skia-plugin.cmake
#
# Best-effort, zero-manual-step deployment of the prebuilt Skia plugin
# next to a freshly built exe. The goal: after a normal
#   cmake --build
# (the command the developer already runs) airui-skia.dll and its
# transitive vcpkg runtime DLLs are sitting beside the exe, so the app
# uses the Skia backend on a plain double-click with no DLL copy and no
# separate fetch command.
#
# It also AUTO-UPDATES: a marker file (airui-skia.ci-run) records which
# CI run the deployed DLL came from. When a plugin is already present we
# do one cheap `gh run list` and, if CI has published a newer successful
# run, re-pull the plugin. Offline / no `gh` keeps whatever is there.
#
# If the plugin cannot be obtained at all (no network, no `gh`, CI down)
# the script does NOT fail the build -- the app silently falls back to
# GDI.
#
# Invoked by airui_deploy_skia_plugin() as a POST_BUILD step. Required
# -D args: TARGET_DIR (exe dir), BINARY_DIR (cmake build dir),
# PLUGIN_DIR (explicit override, may be empty).
#
# Only *.dll files are copied, so a developer's own freshly-built
# airecord.exe is never clobbered by the CI bundle's exe.
# ============================================================
if(NOT DEFINED TARGET_DIR OR NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "deploy-skia-plugin.cmake requires -DTARGET_DIR and -DBINARY_DIR")
endif()

# Records the CI run id the deployed plugin came from.
set(_marker "${TARGET_DIR}/airui-skia.ci-run")

# --- locate a local bundle first (CI pre-download / manual) --------
# Doing this before any `gh` call means the CI path (which pre-downloads
# into <build>/ci-all/airui-skia-windows-x64) never needs a token here.
set(_src "")
if(PLUGIN_DIR AND EXISTS "${PLUGIN_DIR}/airui-skia.dll")
    set(_src "${PLUGIN_DIR}")
endif()
foreach(_d
        "${BINARY_DIR}/ci-all/airui-skia-windows-x64"
        "${BINARY_DIR}/ci-skia"
        "${BINARY_DIR}/ci-skia/airui-skia-windows-x64")
    if(_src)
        break()
    endif()
    if(EXISTS "${_d}/airui-skia.dll")
        set(_src "${_d}")
    endif()
endforeach()

# --- gh helpers (only used when a download/update check is needed) --
find_program(_gh gh)
if(_gh AND DEFINED ENV{GITHUB_TOKEN})
    # gh in Actions needs GH_TOKEN explicitly; execute_process has no ENV
    # keyword so set it on the cmake process (children inherit it).
    set(ENV GH_TOKEN "$ENV{GITHUB_TOKEN}")
endif()

function(_deploy_latest_run_id out)
    set(${out} "" PARENT_SCOPE)
    if(NOT _gh)
        return()
    endif()
    execute_process(
        COMMAND "${_gh}" run list --workflow ci.yml --status success --limit 1
                --json databaseId --jq ".[0].databaseId"
        OUTPUT_VARIABLE _id
        RESULT_VARIABLE _rc
        TIMEOUT 30)
    string(STRIP "${_id}" _id)
    if(_rc EQUAL 0 AND _id MATCHES "^[0-9]+$")
        set(${out} "${_id}" PARENT_SCOPE)
    endif()
endfunction()

set(_have_dll FALSE)
if(EXISTS "${TARGET_DIR}/airui-skia.dll")
    set(_have_dll TRUE)
endif()

# ------------------------------------------------------------------
# Case A: plugin already deployed -> only (re)act if CI has a newer one.
# ------------------------------------------------------------------
if(_have_dll)
    _deploy_latest_run_id(_latest)
    if(NOT _latest)
        message(STATUS "deploy-skia: plugin present; update check unavailable, keeping current")
        return()
    endif()
    set(_have_run "")
    if(EXISTS "${_marker}")
        file(READ "${_marker}" _have_run)
        string(STRIP "${_have_run}" _have_run)
    endif()
    if("${_have_run}" STREQUAL "${_latest}")
        message(STATUS "deploy-skia: plugin up to date (run ${_latest})")
        return()
    endif()
    message(STATUS "deploy-skia: newer plugin available (run ${_latest}); refreshing")
    if(NOT _src AND _gh)
        file(MAKE_DIRECTORY "${BINARY_DIR}/ci-skia")
        execute_process(
            COMMAND "${_gh}" run download "${_latest}"
                    --name airui-skia-windows-x64 -D "${BINARY_DIR}/ci-skia"
            RESULT_VARIABLE _rc2
            TIMEOUT 300)
        if(_rc2 EQUAL 0)
            file(GLOB_RECURSE _found "${BINARY_DIR}/ci-skia/airui-skia.dll")
            if(_found)
                get_filename_component(_src "${_found}" DIRECTORY)
            endif()
        endif()
    endif()
    if(_src)
        message(STATUS "deploy-skia: copying Skia DLLs from ${_src}")
        file(COPY "${_src}/" DESTINATION "${TARGET_DIR}" FILES_MATCHING PATTERN "*.dll")
        file(WRITE "${_marker}" "${_latest}\n")
    else()
        message(STATUS "deploy-skia: refresh failed; keeping the current plugin")
    endif()
    return()
endif()

# ------------------------------------------------------------------
# Case B: no plugin yet -> deploy from local bundle, else download.
# ------------------------------------------------------------------
if(_src)
    message(STATUS "deploy-skia: copying Skia DLLs from ${_src}")
    file(COPY "${_src}/" DESTINATION "${TARGET_DIR}" FILES_MATCHING PATTERN "*.dll")
    return()
endif()

_deploy_latest_run_id(_latest)
if(_gh AND _latest)
    message(STATUS "deploy-skia: downloading artifact 'airui-skia-windows-x64' from CI run ${_latest}")
    file(MAKE_DIRECTORY "${BINARY_DIR}/ci-skia")
    execute_process(
        COMMAND "${_gh}" run download "${_latest}"
                --name airui-skia-windows-x64 -D "${BINARY_DIR}/ci-skia"
        RESULT_VARIABLE _rc2
        TIMEOUT 300)
    if(_rc2 EQUAL 0)
        file(GLOB_RECURSE _found "${BINARY_DIR}/ci-skia/airui-skia.dll")
        if(_found)
            get_filename_component(_src "${_found}" DIRECTORY)
        endif()
    endif()
    if(_src)
        message(STATUS "deploy-skia: copying Skia DLLs from ${_src}")
        file(COPY "${_src}/" DESTINATION "${TARGET_DIR}" FILES_MATCHING PATTERN "*.dll")
        file(WRITE "${_marker}" "${_latest}\n")
        return()
    endif()
endif()

if(_gh)
    message(STATUS "deploy-skia: no plugin available; app will fall back to GDI")
else()
    message(STATUS "deploy-skia: 'gh' not found and no local bundle; app will use GDI")
endif()
