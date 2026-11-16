# ============================================================
# build-installer.ps1 - build the AireRecord NSIS installer.
#
#   powershell -File packaging\build-installer.ps1
#
# Requires NSIS (makensis.exe). It is auto-detected from the standard
# install path; override with -Makensis <path> or the MAKENSIS env var.
# ============================================================
param(
    [string]$Makensis = ""
)
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

if (-not $Makensis) { $Makensis = $env:MAKENSIS }
if (-not $Makensis) {
    $candidates = @(
        "C:\Program Files (x86)\NSIS\makensis.exe",
        "C:\Program Files\NSIS\makensis.exe"
    )
    foreach ($c in $candidates) { if (Test-Path $c) { $Makensis = $c; break } }
}
if (-not $Makensis) { throw "makensis.exe not found. Install NSIS or pass -Makensis <path>." }
if (-not (Test-Path $Makensis)) { throw "makensis not found at: $Makensis" }

# ensure the installer output dir exists (the .nsi OutFile points there)
New-Item -ItemType Directory -Force -Path "build\installer" | Out-Null

# Pass the absolute repo root so NSIS resolves the source paths regardless
# of its working directory / ${__FILEDIR__} quirks.
$repoRoot = (Get-Location).Path
Write-Host "makensis: $Makensis (root=$repoRoot)"
& $Makensis "/DROOT=$repoRoot" "packaging\airecord.nsi"
if ($LASTEXITCODE -ne 0) { throw "makensis failed (rc=$LASTEXITCODE)" }
Write-Host "Installer built -> build\installer"