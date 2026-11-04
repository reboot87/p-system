#!/usr/bin/env bash
# ============================================================
# AirUI - core cleanliness gate (POSIX host / CI)
#
# Fails if anything under src/airui/core references a platform
# API. This is what keeps airui-core compilable on any target.
#
#   ./scripts/check-core-clean.sh
# ============================================================
set -u

here="$(cd "$(dirname "$0")" && pwd)"
core="$here/../src/airui/core"

if [ ! -d "$core" ]; then
    echo "check-core-clean: core directory not found: $core"
    exit 1
fi

pattern='windows\.h|WinUser|WinBase|\bHWND\b|\bHDC\b|\bHBRUSH\b|\bHFONT\b|\bHPEN\b|\bHMENU\b|\bHINSTANCE\b|\bHICON\b|\bHCURSOR\b|\bLRESULT\b|\bWPARAM\b|\bLPARAM\b|\bRECT\b|\bCOLORREF\b|\bPOINT\b|\bMSG\b|WM_[A-Z0-9]|\bDT_[A-Z]|GetTickCount|GetSystemMetrics|CreateWindow|DrawText|SetTextColor'

hits=$(grep -rnE "$pattern" --include='*.h' --include='*.cpp' "$core" || true)

if [ -n "$hits" ]; then
    echo "$hits"
    count=$(printf '%s\n' "$hits" | wc -l)
    echo "check-core-clean: FAILED ($count platform references in core)"
    exit 1
fi

files=$(find "$core" \( -name '*.h' -o -name '*.cpp' \) | wc -l)
echo "check-core-clean: PASSED ($files core files, 0 platform references)"
exit 0
