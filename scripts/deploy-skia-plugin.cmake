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

# --- trusted local bundle (CI pre-download or explicit override) ---
# Only these two locations are guaranteed to match the CURRENT CI run.
# Doing this before any `gh` call means the CI path (which pre-downloads
# into <build>/ci-all/airui-skia-windows-x64) never needs a token here.
set(_bundle "")
if(PLUGIN_DIR AND EXISTS "${PLUGIN_DIR}/airui-skia.dll")
    set(_bundle "${PLUGIN_DIR}")
elseif(EXISTS "${BINARY_DIR}/ci-all/airui-skia-windows-x64/airui-skia.dll")
    set(_bundle "${BINARY_DIR}/ci-all/airui-skia-windows-x64")
endif()

# --- our own download cache, only ever an OFFLINE fallback ----------
# Deliberately NOT treated as "current": it may hold an older run's
# files. (Reusing it while stamping the new run id froze deployed
# plugins at an old build forever.)
set(_cache "${BINARY_DIR}/ci-skia")
set(_cache_src "")
if(EXISTS "${_cache}/airui-skia.dll")
    set(_cache_src "${_cache}")
elseif(EXISTS "${_cache}/airui-skia-windows-x64/airui-skia.dll")
    set(_cache_src "${_cache}/airui-skia-windows-x64")
endif()

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

# Download the newest successful CI bundle into <build>/ci-skia and
# return the directory holding airui-skia.dll (empty on failure).
function(_deploy_download out)
    set(${out} "" PARENT_SCOPE)
    if(NOT _gh)
        return()
    endif()
    _deploy_latest_run_id(_latest)
    if(NOT _latest)
        return()
    endif()

    message(STATUS "deploy-skia: downloading artifact 'airui-skia-windows-x64' from CI run ${_latest}")
    # Wipe first: a partial/stale cache must not be mistaken for the
    # freshly downloaded bundle.
    file(REMOVE_RECURSE "${_cache}")
    file(MAKE_DIRECTORY "${_cache}")
    execute_process(
        COMMAND "${_gh}" run download "${_latest}"
                --name airui-skia-windows-x64 -D "${_cache}"
        RESULT_VARIABLE _rc
        TIMEOUT 300)
    if(NOT _rc EQUAL 0)
        return()
    endif()

    file(GLOB_RECURSE _found "${_cache}/airui-skia.dll")
    if(_found)
        list(GET _found 0 _first)
        get_filename_component(_dir "${_first}" DIRECTORY)
        set(${out} "${_dir}" PARENT_SCOPE)
        file(WRITE "${_marker}" "${_latest}\n")
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
    set(_src "${_bundle}")
    if(NOT _src)
        # Force a real download; the on-disk cache belongs to an older run.
        _deploy_download(_src)
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
# Case B: no plugin yet -> local bundle, else download, else cache.
# ------------------------------------------------------------------
set(_src "${_bundle}")
if(NOT _src)
    _deploy_download(_src)
endif()
if(NOT _src)
    set(_src "${_cache_src}")
endif()
if(_src)
    message(STATUS "deploy-skia: copying Skia DLLs from ${_src}")
    file(COPY "${_src}/" DESTINATION "${TARGET_DIR}" FILES_MATCHING PATTERN "*.dll")
    return()
endif()

if(_gh)
    message(STATUS "deploy-skia: no plugin available; app will fall back to GDI")
else()
    message(STATUS "deploy-skia: 'gh' not found and no local bundle; app will use GDI")
endif()
