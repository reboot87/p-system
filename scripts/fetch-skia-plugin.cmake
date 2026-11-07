# ============================================================
# fetch-skia-plugin.cmake
# Download the latest Skia plugin artifact from GitHub Actions into
# <binary-dir>/ci-skia so the normal build can auto-deploy it.
#
# Run via:  cmake --build build --target fetch-skia-plugin
# Requires: gh (GitHub CLI) authenticated + network access.
# ============================================================
cmake_minimum_required(VERSION 3.16)

if(NOT CMAKE_BINARY_DIR)
    message(FATAL_ERROR "CMAKE_BINARY_DIR not provided")
endif()

find_program(GH_EXECUTABLE gh)
if(NOT GH_EXECUTABLE)
    message(FATAL_ERROR "gh (GitHub CLI) not found; install it and run `gh auth login`")
endif()

set(_dest "${CMAKE_BINARY_DIR}/ci-skia")
file(MAKE_DIRECTORY "${_dest}")

# Find the most recent successful CI run that produced the Skia artifact.
execute_process(
    COMMAND ${GH_EXECUTABLE} run list
            --workflow ci.yml --status success --limit 30
            --json databaseId,name,headBranch
    OUTPUT_VARIABLE _runs_json
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "gh run list failed (rc=${_rc})")
endif()

# Parse the JSON list with a tiny CMake helper (no external deps).
string(REPLACE "}," "} ;" _runs "${_runs_json}")
string(REPLACE "{" "{" _runs "${_runs}")

set(_run_id "")
set(_idx 0)
while(_runs MATCHES "\"databaseId\": *([0-9]+)")
    string(REGEX REPLACE "^[^\"]*\"databaseId\": *([0-9]+).*$" "\\1"
           _id "${_runs}")
    string(REGEX REPLACE "^[^}]*\\}" "" _runs "${_runs}")
    if(NOT _run_id)
        set(_run_id "${_id}")
    endif()
    math(EXPR _idx "${_idx} + 1")
    if(_idx GREATER 30)
        break()
    endif()
endwhile()

if(NOT _run_id)
    message(FATAL_ERROR "No successful CI run found")
endif()
message(STATUS "Latest successful CI run: ${_run_id}")

# The Skia job uploads its package as airui-skia-windows-x64.
execute_process(
    COMMAND ${GH_EXECUTABLE} run download "${_run_id}"
            --name airui-skia-windows-x64 -D "${_dest}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "gh run download failed (rc=${_rc})")
endif()

if(NOT EXISTS "${_dest}/airui-skia.dll")
    message(FATAL_ERROR "airui-skia.dll not found after download; artifact name may have changed")
endif()

message(STATUS "Skia plugin deployed to ${_dest} — rebuild to auto-copy it next to the exe")
