# ============================================================
# build-installer.ps1 - build the AireRecord NSIS installer.
#
#   powershell -File packaging\build-installer.ps1
#
# Requires NSIS (makensis.exe). It is auto-detected from the standard
# install path; override with -Makensis <path> or the MAKENSIS env var.
#
# Skia plugin gating:
#   The x64 Skia bundle (build\ci-all\airui-skia-windows-x64\airui-skia.dll)
#   is only packaged when it is a CURRENT ABI-v2 plugin -- i.e. it exports
#   `airui_skia_describe` (introduced in P0-3). A stale ABI-v1 bundle (built
#   before P0-3) exports only `airui_skia_abi`; shipping it would make the
#   installed app load + reject the plugin and silently fall back to GDI while
#   the user thinks Skia is present. We detect the exports below and only pass
#   /DSHIP_SKIA=1 to makensis for a fresh bundle. See airecord.nsi.
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

# ---- Skia bundle ABI detection (no dumpbin dependency) ----
# Returns $true if the DLL exports `airui_skia_describe` (ABI v2 / fresh).
# Parses the PE export table directly from the bytes.
function Test-SkiaAbiV2([string]$dllPath) {
    if (-not $dllPath -or -not (Test-Path $dllPath)) { return $false }
    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $dllPath))
    if ($bytes.Length -lt 0x40) { return $false }

    # DOS header -> e_lfanew at 0x3C
    $pe = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($pe + 4 -gt $bytes.Length) { return $false }
    if ([System.Text.Encoding]::ASCII.GetString($bytes, $pe, 4) -ne "PE`0`0") { return $false }

    # Optional header
    $opt = $pe + 24
    $magic = [BitConverter]::ToUInt16($bytes, $opt)
    # Data directory array starts at a FIXED offset from the optional header:
    # 112 (0x70) for PE32+ (magic 0x20B), 96 (0x60) for PE32 (magic 0x10B).
    $dd = if ($magic -eq 0x20B) { $opt + 112 } elseif ($magic -eq 0x10B) { $opt + 96 } else { return $false }
    # Export dir is data directory index 0 (RVA + size, 8 bytes each)
    $exportRva = [BitConverter]::ToUInt32($bytes, $dd)
    $exportSize = [BitConverter]::ToUInt32($bytes, $dd + 4)
    if ($exportRva -eq 0) { return $false }

    # Section table follows the optional header. Build RVA->offset map.
    $secCount = [BitConverter]::ToUInt16($bytes, $pe + 6)
    $secTab = $opt + [BitConverter]::ToUInt16($bytes, $pe + 20)  # SizeOfOptionalHeader
    $rvaToOff = 0
    for ($i = 0; $i -lt $secCount; $i++) {
        $s = $secTab + ($i * 40)
        $va   = [BitConverter]::ToUInt32($bytes, $s + 12)
        $size = [BitConverter]::ToUInt32($bytes, $s + 8)
        $raw  = [BitConverter]::ToUInt32($bytes, $s + 20)
        if ($exportRva -ge $va -and $exportRva -lt ($va + $size)) {
            $rvaToOff = $raw + ($exportRva - $va)
            break
        }
    }
    if ($rvaToOff -eq 0 -or $rvaToOff -ge $bytes.Length) { return $false }

    $numNames = [BitConverter]::ToUInt32($bytes, $rvaToOff + 24)
    $namesRva = [BitConverter]::ToUInt32($bytes, $rvaToOff + 32)

    # Map the names array RVA to a file offset (same section normally).
    $namesOff = $rvaToOff + ($namesRva - $exportRva)
    for ($n = 0; $n -lt $numNames; $n++) {
        $nameRva = [BitConverter]::ToUInt32($bytes, $namesOff + ($n * 4))
        $nameOff = $rvaToOff + ($nameRva - $exportRva)
        $end = $nameOff
        while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) { $end++ }
        $name = [System.Text.Encoding]::ASCII.GetString($bytes, $nameOff, $end - $nameOff)
        if ($name -eq "airui_skia_describe") { return $true }
    }
    return $false
}

# ---- Version injection (single source of truth) ----
# Parse `project(AireRecord VERSION x.y.z)` from CMakeLists.txt -- the same
# declaration that configure_file() turns into AIRUI_VERSION (cmake/
# Version.h.in). Passing /DAPP_VERSION keeps the installer version, product
# name and the uninstall-registry DisplayVersion in lockstep with the app's
# own version string; the .nsi only carries a fallback define for direct
# makensis invocations.
$cmakeLists = Join-Path (Get-Location) "CMakeLists.txt"
$appVersion = ""
foreach ($line in Get-Content $cmakeLists -Encoding UTF8) {
    if ($line -match '^\s*project\s*\(\s*AireRecord\s+VERSION\s+([0-9]+\.[0-9]+(\.[0-9]+)?)') {
        $appVersion = $Matches[1]
        break
    }
}
if (-not $appVersion) { throw "Could not parse project(AireRecord VERSION ...) from CMakeLists.txt" }
Write-Host "Installer version: $appVersion (from CMakeLists.txt)"

$skiaDll = Join-Path (Get-Location) "build\ci-all\airui-skia-windows-x64\airui-skia.dll"
$shipSkia = Test-SkiaAbiV2 $skiaDll
if ($shipSkia) {
    Write-Host "Skia bundle detected as ABI-v2 (exports airui_skia_describe) -> shipping it."
} else {
    Write-Host "Skia bundle missing or STALE (ABI-v1, does not export airui_skia_describe)."
    Write-Host "  NOT shipping the x64 Skia plugin; the installed x64 app will use honest GDI."
    Write-Host "  Rebuild the plugin from current source (P0-3) in the CI job to re-enable Skia."
}

# Pass the absolute repo root so NSIS resolves the source paths regardless
# of its working directory / ${__FILEDIR__} quirks.
$repoRoot = (Get-Location).Path
Write-Host "makensis: $Makensis (root=$repoRoot)"
$args = @("/DROOT=$repoRoot", "/DAPP_VERSION=$appVersion")
if ($shipSkia) { $args += "/DSHIP_SKIA=1" }
$args += "packaging\airecord.nsi"
& $Makensis $args
if ($LASTEXITCODE -ne 0) { throw "makensis failed (rc=$LASTEXITCODE)" }
Write-Host "Installer built -> build\installer"