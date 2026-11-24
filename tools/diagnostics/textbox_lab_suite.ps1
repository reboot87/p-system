# textbox_lab_suite.ps1 - T1/T2/T3 end-to-end scenarios for the self-drawn
# TextBox mouse interactions, driven cross-process against a running
# airui_textbox_lab.exe.
#
# Prereq (Windows, repo already configured + built):
#   cmake --build build --config Release `
#       --target airui_textbox_lab labtest menuclick boxrect
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\diagnostics\textbox_lab_suite.ps1
#   powershell -ExecutionPolicy Bypass -File tools\diagnostics\textbox_lab_suite.ps1 -BuildDir .\build
#
# Exit code: 0 = all scenarios PASS, 1 = at least one FAIL (or setup error).
# Manual smoke suite, not a ctest case (needs a visible window + a few
# seconds of message pumping between steps). See tools/diagnostics/README.md.

param(
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Continue"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $BuildDir) { $BuildDir = Join-Path $repoRoot "build" }
$rel   = Join-Path $BuildDir "Release"
$lab   = Join-Path $rel "airui_textbox_lab.exe"
$driver = Join-Path $rel "labtest.exe"
$menu  = Join-Path $rel "menuclick.exe"

$script:fail = 0

# Runs one driver command; when -Expect is given, the combined stdout must
# match the regex. -Soft = informational only (never counted as a failure).
function Step($name, [string[]]$cmdArgs, [string]$Expect = "", [switch]$Soft) {
    $out = (& $driver @cmdArgs 2>&1 | Out-String)
    Write-Host $out.Trim()
    if ($Expect -ne "") {
        if ($out -match $Expect) {
            Write-Host "  -> $name PASS"
        } else {
            if ($Soft) { Write-Host "  -> $name (soft, no match for /$Expect/)" }
            else { Write-Host "  -> $name FAIL (expected /$Expect/)"; $script:fail++ }
        }
    }
    return $out
}

function KillAll() {
    Get-Process airui_textbox_lab, labtest -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 600
}

if (-not (Test-Path $lab)) {
    Write-Host "FATAL: $lab not found (build the targets first, see header)"
    exit 1
}
if (-not (Test-Path $driver) -or -not (Test-Path $menu)) {
    Write-Host "FATAL: $driver / $menu not found (build the labtest+menuclick targets)"
    exit 1
}

KillAll
if (Get-Process airui_textbox_lab, labtest -ErrorAction SilentlyContinue) {
    Write-Host "FATAL: could not kill leftover lab/labtest processes"
    exit 1
}

$proc = Start-Process -FilePath $lab -PassThru
Start-Sleep -Seconds 2
Write-Host "lab pid=$($proc.Id)  box3001:"
$rect = & $driver rect 2>&1 | Out-String
Write-Host (($rect -split "`n") | Select-String "id=3001")

# ------------------------------------------------------------------
# T1: double-click selects the whole ASCII word, typing replaces it
#   "hello world" -> dblclick 'world' -> sel=[6,11) -> type "x" -> "hello x"
# ------------------------------------------------------------------
Write-Host ""
Write-Host "=== T1 dblclick selects word, typing replaces it ==="
Step "T1 clear"    @("clear", "3001")
Step "T1 type"     @("type", "3001", "hello world") "" -Soft
Step "T1 dblclick" @("dblclick", "3001", "75", "10")
Step "T1 sel"      @("getsel", "3001") "sel=\[6,11\)"
Step "T1 type-x"   @("type", "3001", "x") "" -Soft
Step "T1 result"   @("getraw") "\[hello x\]"

# ------------------------------------------------------------------
# T2: left-button drag selects a range, typing replaces it
#   "hello world" -> drag x=30..100 -> sel=[3,11) -> type "X" -> "helX"
# ------------------------------------------------------------------
Write-Host ""
Write-Host "=== T2 drag-select, typing replaces it ==="
Step "T2 clear" @("clear", "3001")
Step "T2 type"  @("type", "3001", "hello world") "" -Soft
Step "T2 drag"  @("drag", "3001", "30", "100", "10")
Step "T2 sel"   @("getsel", "3001") "sel=\[3,11\)"
Step "T2 type-X" @("type", "3001", "X") "" -Soft
Step "T2 result" @("getraw") "\[helX\]"

# ------------------------------------------------------------------
# T3: right-click inside a selection keeps it; the self-drawn edit menu
#     copies it, and after clearing the box, pasting restores "world".
#   dblclick 'world' -> rclick IN selection (x=75) -> sel stays [6,11)
#   -> menu item 0/3 (copy) -> clear -> rclick -> menu item 2/3 (paste)
#   -> getraw [world]
# ------------------------------------------------------------------
Write-Host ""
Write-Host "=== T3 right-click menu copy/paste round-trip ==="
Step "T3 clear"    @("clear", "3001")
Step "T3 type"     @("type", "3001", "hello world") "" -Soft
Step "T3 dblclick" @("dblclick", "3001", "75", "10")
Step "T3 sel"      @("getsel", "3001") "sel=\[6,11\)"

# rclick blocks in the menu's modal loop -> run in the background
$bg = Start-Process -FilePath $driver -ArgumentList "rclick","3001","75","10" `
        -RedirectStandardOutput (Join-Path $env:TEMP "t3_menu_bg.txt") -PassThru
Start-Sleep -Milliseconds 1200
if ($bg.HasExited) {
    Write-Host "  -> T3 menu did not open (rclick returned early)"; $script:fail++
} else {
    Write-Host "T3 menu open (rclick blocked in RunPopupMenu)"
    Step "T3 sel preserved" @("getsel", "3001") "sel=\[6,11\)"
    $mc = & $menu 0 3 2>&1 | Out-String
    Write-Host $mc.Trim()
    if ($LASTEXITCODE -ne 0 -or $mc -notmatch "menu closed") {
        Write-Host "  -> T3 copy click failed"; $script:fail++
    } else { Write-Host "  -> T3 copy clicked, menu closed" }
}

Step "T3 clear-2" @("clear", "3001")
Step "T3 empty"   @("getraw") "GETTEXTLENGTH=0"

$bg2 = Start-Process -FilePath $driver -ArgumentList "rclick","3001","10","10" `
        -RedirectStandardOutput (Join-Path $env:TEMP "t3_menu_bg2.txt") -PassThru
Start-Sleep -Milliseconds 1200
if ($bg2.HasExited) {
    Write-Host "  -> T3 menu2 did not open"; $script:fail++
} else {
    Write-Host "T3 menu2 open"
    $mc2 = & $menu 2 3 2>&1 | Out-String
    Write-Host $mc2.Trim()
    if ($LASTEXITCODE -ne 0 -or $mc2 -notmatch "menu closed") {
        Write-Host "  -> T3 paste click failed"; $script:fail++
    } else { Write-Host "  -> T3 paste clicked, menu closed" }
}
Start-Sleep -Milliseconds 400
Step "T3 result" @("getraw") "\[world\]"

KillAll
Write-Host ""
if ($script:fail -eq 0) { Write-Host "ALL PASS (T1/T2/T3)"; exit 0 }
else { Write-Host "$($script:fail) FAILURE(S)"; exit 1 }
