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
# RECT/POINT/COLORREF are not).
$pattern = 'windows\.h|WinUser|WinBase|\bHWND\b|\bHDC\b|\bHBRUSH\b|\bHFONT\b|\bHPEN\b|\bHMENU\b|\bHINSTANCE\b|\bHICON\b|\bHCURSOR\b|\bLRESULT\b|\bWPARAM\b|\bLPARAM\b|\bRECT\b|\bCOLORREF\b|\bPOINT\b|\bMSG\b|WM_[A-Z0-9]|\bDT_[A-Z]|GetTickCount|GetSystemMetrics|CreateWindow|DrawText|SetTextColor'

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
