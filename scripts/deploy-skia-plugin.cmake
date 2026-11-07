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
# If the plugin cannot be obtained (no network, no `gh`, CI down) the
# script does NOT fail the build -- the app silently falls back to GDI.
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

if(EXISTS "${TARGET_DIR}/airui-skia.dll")
    message(STATUS "deploy-skia: plugin already present next to exe")
    return()
endif()

set(_search "")
if(PLUGIN_DIR)
    list(APPEND _search "${PLUGIN_DIR}")
endif()
list(APPEND _search
    "${BINARY_DIR}/ci-all/airui-skia-windows-x64"
    "${BINARY_DIR}/ci-skia"
    "${BINARY_DIR}/ci-skia/airui-skia-windows-x64")

set(_src "")
foreach(_d ${_search})
    if(EXISTS "${_d}/airui-skia.dll")
        set(_src "${_d}")
        break()
    endif()
endforeach()

if(NOT _src)
    find_program(_gh gh)
    if(_gh)
        # In GitHub Actions, gh needs GH_TOKEN explicitly (it does not
        # auto-use GITHUB_TOKEN); locally gh falls back to stored creds.
        # CMake's execute_process has no ENV keyword, so we set the
        # variable on the cmake process; children inherit it.
        if(DEFINED ENV{GITHUB_TOKEN})
            set(ENV GH_TOKEN "$ENV{GITHUB_TOKEN}")
        endif()
        message(STATUS "deploy-skia: fetching artifact 'airui-skia-windows-x64' from CI (best-effort)")
        # Latest successful CI run that produced the Skia artifact.
        execute_process(
            COMMAND "${_gh}" run list --workflow ci.yml --status success --limit 1
                    --json databaseId --jq ".[0].databaseId"
            OUTPUT_VARIABLE _run_id
            RESULT_VARIABLE _rc)
        string(STRIP "${_run_id}" _run_id)
        if(_rc EQUAL 0 AND _run_id MATCHES "^[0-9]+$")
            file(MAKE_DIRECTORY "${BINARY_DIR}/ci-skia")
            execute_process(
                COMMAND "${_gh}" run download "${_run_id}"
                        --name airui-skia-windows-x64 -D "${BINARY_DIR}/ci-skia"
                RESULT_VARIABLE _rc2)
            if(_rc2 EQUAL 0)
                # gh extracts the artifact under a folder named after it;
                # locate the DLL wherever it landed.
                file(GLOB_RECURSE _found "${BINARY_DIR}/ci-skia/airui-skia.dll")
                if(_found)
                    get_filename_component(_src "${_found}" DIRECTORY)
                endif()
            endif()
        endif()
    else()
        message(STATUS "deploy-skia: 'gh' not found; skipping (app will use GDI)")
    endif()
endif()

if(_src)
    message(STATUS "deploy-skia: copying Skia DLLs from ${_src}")
    file(COPY "${_src}/" DESTINATION "${TARGET_DIR}" FILES_MATCHING PATTERN "*.dll")
else()
    message(STATUS "deploy-skia: no plugin available; app will fall back to GDI")
endif()
