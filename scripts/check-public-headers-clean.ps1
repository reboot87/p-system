# ============================================================
# AirUI - public header platform-purity gate (Windows host)
#
# Mirrors check-public-headers-clean.sh. Fails if any PUBLIC
# header (core/include/airui/*.h or platform/win32/airui/*.h)
# OR ANY HEADER REACHED THROUGH ITS #include "..." CLOSURE
# references a platform API/type, including in comments.
#
# This is the transitive guard: a public header that stays clean
# itself but #includes a private header full of Win32 types still
# fails, because consumers see those types too.
#
# System headers <...> are not expanded -- but a public header that
# literally #includes <windows.h> is still caught by the text scan.
#
# Headers still being migrated are listed in
# scripts/public-headers-exempt.txt (one basename per line):
# exempting an entry header skips its whole closure.
#
#   powershell -ExecutionPolicy Bypass -File scripts/check-public-headers-clean.ps1
# ============================================================
$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$coreParent = Join-Path $root "src\airui\core\include"
$winParent  = Join-Path $root "src\airui\platform\win32"
$coreDir = Join-Path $coreParent "airui"
$winDir  = Join-Path $winParent "airui"

$regexPath = Join-Path $PSScriptRoot "core-forbidden.regex"
$pattern = (Get-Content -Raw -Path $regexPath).Trim()

$exemptPath = Join-Path $PSScriptRoot "public-headers-exempt.txt"
$exempt = @()
if (Test-Path $exemptPath) {
    $exempt = @(Get-Content -Path $exemptPath |
                ForEach-Object { $_.Trim() } |
                Where-Object { $_ -and -not $_.StartsWith('#') })
}

$publicHeaders = @()
foreach ($dir in @($coreDir, $winDir)) {
    if (Test-Path $dir) {
        $publicHeaders += Get-ChildItem -Path $dir -Filter *.h -File
    }
}

$failures = New-Object System.Collections.Generic.List[string]
$checked = 0
$exemptCount = 0
$closureCount = 0

$includeRe = '^\s*#\s*include\s+"([^"]+)"'

function Check-Closure {
    param([string]$Entry)
    $queue = New-Object System.Collections.Generic.Queue[string]
    $queue.Enqueue($Entry)
    $seen = New-Object 'System.Collections.Generic.HashSet[string]'

    while ($queue.Count -gt 0) {
        $f = $queue.Dequeue()
        if (-not (Test-Path -LiteralPath $f)) { continue }
        $key = (Resolve-Path -LiteralPath $f).Path.ToLowerInvariant()
        if (-not $seen.Add($key)) { continue }
        $script:closureCount++

        $rel = $f.Substring($root.Length + 1)
        $hits = Select-String -LiteralPath $f -CaseSensitive -Pattern $pattern
        if ($hits) {
            foreach ($hit in $hits) {
                $script:failures.Add(("FAIL {0} -> {1}:{2}: {3}" -f `
                    (Split-Path $Entry -Leaf), $rel, $hit.LineNumber, $hit.Line.Trim()))
            }
        }

        $dir = Split-Path $f -Parent
        foreach ($line in Get-Content -LiteralPath $f) {
            if ($line -match $includeRe) {
                $inc = $Matches[1]
                $resolved = $null
                $candidates = @((Join-Path $dir $inc),
                                (Join-Path $coreParent $inc),
                                (Join-Path $winParent $inc))
                foreach ($c in $candidates) {
                    if (Test-Path -LiteralPath $c) { $resolved = $c; break }
                }
                if ($resolved) { $queue.Enqueue($resolved) }
            }
        }
    }
}

foreach ($h in $publicHeaders) {
    if ($exempt -contains $h.Name) { $exemptCount++; continue }
    $checked++
    Check-Closure -Entry $h.FullName
}

if ($failures.Count -gt 0) {
    foreach ($line in $failures) { Write-Host $line }
    Write-Host ("check-public-headers-clean: FAILED ({0} platform references across {1} files in public include closures)" -f $failures.Count, $closureCount)
    exit 1
}

Write-Host ("check-public-headers-clean: PASSED ({0} public headers checked, {1} exempt, {2} files in closures, 0 platform references)" -f $checked, $exemptCount, $closureCount)
exit 0
