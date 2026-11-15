# ============================================================
# AirUI - core cleanliness gate (Windows host)
#
# Fails if anything under src/airui/core references a platform
# API. This is what keeps airui-core compilable on any target.
#
#   powershell -ExecutionPolicy Bypass -File scripts/check-core-clean.ps1
# ============================================================
$ErrorActionPreference = "Stop"

$core = Join-Path $PSScriptRoot "..\src\airui\core"
$core = (Resolve-Path $core).Path

# Forbidden identifiers (case-sensitive: Rect/Point/Color are fine,
# RECT/POINT/COLORREF are not). Shared single source of truth lives in
# scripts/core-forbidden.regex, read by both the .sh and the .ps1 gate.
$regexPath = Join-Path $PSScriptRoot "core-forbidden.regex"
$pattern = (Get-Content -Raw -Path $regexPath).Trim()

$hits = Get-ChildItem -Path $core -Recurse -Include *.h,*.cpp |
        Select-String -CaseSensitive -Pattern $pattern

if ($hits) {
    foreach ($h in $hits) {
        $rel = $h.Path.Substring($core.Length).TrimStart('\\')
        Write-Host ("FAIL {0}:{1}: {2}" -f $rel, $h.LineNumber, $h.Line.Trim())
    }
    Write-Host "check-core-clean: FAILED ($($hits.Count) platform references in core)"
    exit 1
}

$files = (Get-ChildItem -Path $core -Recurse -Include *.h,*.cpp).Count
Write-Host "check-core-clean: PASSED ($files core files, 0 platform references)"
exit 0
