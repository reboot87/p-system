#!/usr/bin/env bash
# ============================================================
# AirUI - public header platform-purity gate (POSIX host)
#
# Mirrors check-public-headers-clean.ps1. Fails if any PUBLIC
# header (core/include/airui/*.h, platform/win32/airui/*.h and
# platform/skia/airui/*.h) OR ANY HEADER REACHED THROUGH ITS
# #include "..." CLOSURE references a platform API/type/macro,
# including in comments.
#
# This is the transitive guard: a public header that stays clean
# itself but #includes a private header full of Win32 types still
# fails, because consumers see those types too.
#
# System headers <...> are not expanded (the standard library and
# the platform SDK are not our surface) -- but a public header that
# literally #includes <windows.h> is still caught by the text scan.
#
# Headers still being migrated are listed in
# scripts/public-headers-exempt.txt (one basename per line):
# exempting an entry header skips its whole closure.
# ============================================================
set -eu

root="$(cd "$(dirname "$0")/.." && pwd)"
core_parent="$root/src/airui/core/include"
win_parent="$root/src/airui/platform/win32"
skia_parent="$root/src/airui/platform/skia"
core_dir="$core_parent/airui"
win_dir="$win_parent/airui"
skia_dir="$skia_parent/airui"
pattern="$(cat "$root/scripts/core-forbidden.regex")"
exempt_file="$root/scripts/public-headers-exempt.txt"

canonical() {
    local p="$1" d b
    d="$(cd "$(dirname "$p")" 2>/dev/null && pwd -P)" || d="$(dirname "$p")"
    b="$(basename "$p")"
    printf '%s/%s' "$d" "$b"
}

failures=0
checked=0
exempt_count=0
closure_files=0

check_closure() {
    local entry="$1"
    local -a queue=("$entry")
    local -A seen=()
    while [ "${#queue[@]}" -gt 0 ]; do
        local f="${queue[0]}"
        queue=("${queue[@]:1}")
        [ -f "$f" ] || continue
        local key
        key="$(canonical "$f")"
        if [ -n "${seen[$key]:-}" ]; then continue; fi
        seen[$key]=1
        closure_files=$((closure_files + 1))

        local rel="${f#$root/}"
        local hit
        while IFS= read -r hit; do
            if [ -z "$hit" ]; then continue; fi
            printf 'FAIL %s -> %s: %s\n' "$(basename "$entry")" "$rel" "$hit"
            failures=$((failures + 1))
        done < <(grep -nE "$pattern" "$f" || true)

        local inc resolved
        while IFS= read -r inc; do
            if [ -z "$inc" ]; then continue; fi
            resolved=""
            local c
            for c in "$(dirname "$f")/$inc" "$core_parent/$inc" "$win_parent/$inc" "$skia_parent/$inc"; do
                if [ -f "$c" ]; then resolved="$c"; break; fi
            done
            if [ -n "$resolved" ]; then queue+=("$resolved"); fi
        done < <(sed -nE 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*"([^"]+)".*/\1/p' "$f")
    done
    return 0
}

for dir in "$core_dir" "$win_dir" "$skia_dir"; do
    [ -d "$dir" ] || continue
    for f in "$dir"/*.h; do
        [ -f "$f" ] || continue
        base="$(basename "$f")"
        if [ -f "$exempt_file" ] && grep -qxF "$base" "$exempt_file"; then
            exempt_count=$((exempt_count + 1))
            continue
        fi
        checked=$((checked + 1))
        check_closure "$f"
    done
done

if [ "$checked" -eq 0 ]; then
    printf 'check-public-headers-clean: FAILED (0 public headers found - root discovery is broken)\n'
    exit 1
fi

if [ "$failures" -gt 0 ]; then
    printf 'check-public-headers-clean: FAILED (%d platform references across %d files in public include closures)\n' "$failures" "$closure_files"
    exit 1
fi
printf 'check-public-headers-clean: PASSED (%d public headers checked, %d exempt, %d files in closures, 0 platform references)\n' "$checked" "$exempt_count" "$closure_files"
