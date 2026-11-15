#!/usr/bin/env bash
# ============================================================
# AirUI - core cleanliness gate (POSIX host / CI)
#
# Fails if anything under src/airui/core references a platform
# API. This is what keeps airui-core compilable on any target.
#
#   ./scripts/check-core-clean.sh
# ============================================================
set -eu

here="$(cd "$(dirname "$0")" && pwd)"
core="$here/../src/airui/core"

if [ ! -d "$core" ]; then
    echo "check-core-clean: core directory not found: $core"
    exit 1
fi

pattern=$(cat "$here/core-forbidden.regex")

if hits=$(grep -rnE "$pattern" --include='*.h' --include='*.cpp' "$core"); then
    printf '%s\n' "$hits"
    count=$(printf '%s\n' "$hits" | wc -l)
    echo "check-core-clean: FAILED ($count platform references in core)"
    exit 1
else
    rc=$?
    if [ "$rc" -ne 1 ]; then
        echo "check-core-clean: grep failed (rc=$rc)" >&2
        exit 2
    fi
fi

files=$(find "$core" \( -name '*.h' -o -name '*.cpp' \) | wc -l)
echo "check-core-clean: PASSED ($files core files, 0 platform references)"
exit 0
