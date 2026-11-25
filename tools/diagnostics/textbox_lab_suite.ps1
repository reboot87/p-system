# textbox_lab_suite.ps1 - T1/T2/T3/T4 end-to-end scenarios for the
# self-drawn TextBox: mouse interactions (T1/T2/T3) and undo/redo (T4),
# driven cross-process against a running airui_textbox_lab.exe.
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
#
# T4 drives undo/redo through the edit menu (rclick + menuclick): the menu's
# 撤销/重做 call the exact same TextBox::Undo()/Redo() as the Ctrl+Z/Ctrl+Y
# shortcuts. Synthetic keyboard (SendInput) cannot be routed reliably to a
# cross-process child window (the proc reads GetKeyState(VK_CONTROL), which a
# posted WM_KEYDOWN cannot fake and SendInput only reaches the foreground
# window's focus), so the menu path is what the cross-process suite can assert.

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

# Menu edit-item rows (client y of each row CENTER; item rows 36px, the
# separator 6px). Used by menuclick and by the T4 undo/redo clicks:
#   0 撤销 y=18 | 1 重做 y=54 | 2 --- y=75 | 3 复制 y=96
#   4 剪切 y=132 | 5 粘贴 y=168
$rowUndo = 18; $rowRedo = 54; $rowCopy = 96; $rowCut = 132; $rowPaste = 168

# Open the edit menu (rclick blocks in its modal loop -> background) and click
# one row by index (0=撤销 1=重做 2=sep 3=复制 4=剪切 5=粘贴) at the given y.
# $x is where the rclick lands (it also places the caret there before the menu
# opens - use a large x to park the caret at the end of the text). Returns the
# menuclick stdout; $null if the menu never opened.
function Menu-Click($row, $y, $bgTag, $x = 40) {
    $bg = Start-Process -FilePath $driver -ArgumentList "rclick","3001",([string]$x),"10" `
            -RedirectStandardOutput (Join-Path $env:TEMP "t4_menu_$bgTag.txt") -PassThru
    Start-Sleep -Milliseconds 1200
    if ($bg.HasExited) { return $null }
    $mc = & $menu $row $y 2>&1 | Out-String
    Start-Sleep -Milliseconds 400
    return $mc
}

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
Step "T1 result"   @("getraw") "id=3001 .*\[hello x\]"

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
Step "T2 result" @("getraw") "id=3001 .*\[helX\]"

# ------------------------------------------------------------------
# T3: right-click inside a selection keeps it; the self-drawn edit menu
#     copies it, and after clearing the box, pasting restores "world".
#   dblclick 'world' -> rclick IN selection (x=75) -> sel stays [6,11)
#   -> menu row 3 (copy, y=96) -> clear -> rclick -> row 5 (paste, y=168)
#   -> getraw [world]
#   Menu rows (item rows 36px, separator 6px; click at row CENTER):
#     0 撤销 y=18 | 1 重做 y=54 | 2 --- y=75 | 3 复制 y=96
#     4 剪切 y=132 | 5 粘贴 y=168     (186px client, 188px window)
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
    $mc = & $menu 3 $rowCopy 2>&1 | Out-String
    Write-Host $mc.Trim()
    if ($LASTEXITCODE -ne 0 -or $mc -notmatch "menu closed") {
        Write-Host "  -> T3 copy click failed"; $script:fail++
    } else { Write-Host "  -> T3 copy clicked, menu closed" }
}

Step "T3 clear-2" @("clear", "3001")
Step "T3 empty"   @("getraw") "id=3001 GETTEXTLENGTH=0"

$bg2 = Start-Process -FilePath $driver -ArgumentList "rclick","3001","10","10" `
        -RedirectStandardOutput (Join-Path $env:TEMP "t3_menu_bg2.txt") -PassThru
Start-Sleep -Milliseconds 1200
if ($bg2.HasExited) {
    Write-Host "  -> T3 menu2 did not open"; $script:fail++
} else {
    Write-Host "T3 menu2 open"
    $mc2 = & $menu 5 $rowPaste 2>&1 | Out-String
    Write-Host $mc2.Trim()
    if ($LASTEXITCODE -ne 0 -or $mc2 -notmatch "menu closed") {
        Write-Host "  -> T3 paste click failed"; $script:fail++
    } else { Write-Host "  -> T3 paste clicked, menu closed" }
}
Start-Sleep -Milliseconds 400
Step "T3 result" @("getraw") "id=3001 .*\[world\]"

# ------------------------------------------------------------------
# T4: undo/redo through the edit menu (same TextBox::Undo()/Redo() as the
#     Ctrl+Z / Ctrl+Y shortcuts - the menu is the cross-process-reachable
#     surface; see the header note on synthetic keyboard).
#   clear (SetText resets both stacks) -> menu 撤销 is a no-op
#   type "hello" = 5 insert entries -> 撤销 x5 -> empty
#   -> 重做 x5 -> "hello"   (each redo re-inserts one char; 5th exhausts it)
#   -> 撤销 -> "hell" -> 重做 -> "hello" -> 撤销 -> "hell"
#   -> select-all + 复制, clear, 粘贴 back -> "hell"
#   -> 撤销 (reverts the paste) -> empty
# ------------------------------------------------------------------
Write-Host ""
Write-Host "=== T4 undo/redo via the edit menu (撤销/重做) ==="

# undo/redo stacks are cleared by SetText: menu-undo right after clear is a no-op
Step "T4 clear" @("clear", "3001")
$mc0 = Menu-Click 0 $rowUndo "nohist"
if ($null -eq $mc0) { Write-Host "  -> T4 nohist menu did not open"; $script:fail++ }
elseif ($mc0 -notmatch "menu closed") { Write-Host "  -> T4 nohist menu undo click failed"; $script:fail++ }
Start-Sleep -Milliseconds 300
Step "T4 nohist-noop" @("getraw") "id=3001 GETTEXTLENGTH=0"

Step "T4 type" @("type", "3001", "hello") "" -Soft

# 5 undos: hello -> hell -> hel -> he -> h -> (empty)
for ($i = 1; $i -le 5; $i++) {
    $mcu = Menu-Click 0 $rowUndo "undo$i"
    if ($null -eq $mcu) { Write-Host "  -> T4 undo$i menu did not open"; $script:fail++; break }
    elseif ($mcu -notmatch "menu closed") { Write-Host "  -> T4 undo$i menu click failed"; $script:fail++ }
}
Step "T4 undo-all" @("getraw") "id=3001 GETTEXTLENGTH=0"

# 5 redos: (empty) -> h -> he -> hel -> hell -> hello   [5th exhausts redo]
for ($i = 1; $i -le 5; $i++) {
    $mcr = Menu-Click 1 $rowRedo "redo$i"
    if ($null -eq $mcr) { Write-Host "  -> T4 redo$i menu did not open"; $script:fail++; break }
    elseif ($mcr -notmatch "menu closed") { Write-Host "  -> T4 redo$i menu click failed"; $script:fail++ }
}
Step "T4 redo-all" @("getraw") "id=3001 .*\[hello\]"

# alternate undo/redo: hello -> hell -> hello -> hell
$mcu2 = Menu-Click 0 $rowUndo "alt-u1"
if ($null -eq $mcu2 -or $mcu2 -notmatch "menu closed") { Write-Host "  -> T4 alt-u1 failed"; $script:fail++ }
Step "T4 alt-undo" @("getraw") "id=3001 .*\[hell\]"
$mcr2 = Menu-Click 1 $rowRedo "alt-r1"
if ($null -eq $mcr2 -or $mcr2 -notmatch "menu closed") { Write-Host "  -> T4 alt-r1 failed"; $script:fail++ }
Step "T4 alt-redo" @("getraw") "id=3001 .*\[hello\]"
$mcu3 = Menu-Click 0 $rowUndo "alt-u2"
if ($null -eq $mcu3 -or $mcu3 -notmatch "menu closed") { Write-Host "  -> T4 alt-u2 failed"; $script:fail++ }
Step "T4 alt-undo2" @("getraw") "id=3001 .*\[hell\]"

# menu 复制 (needs a live selection: select-all first, rclick INSIDE the text
# so it does not collapse the selection), then clear, then 粘贴 restores it.
# rclick x must fall within "hell" (text ends ~x=36 at this font); x=20 is
# safely inside. An x past the end parks the caret there and clears the
# selection, leaving Copy nothing to copy.
Step "T4 select-all" @("setsel", "3001", "0", "-1")
$mc4 = Menu-Click 3 $rowCopy "copy" 20
if ($null -eq $mc4 -or $mc4 -notmatch "menu closed") { Write-Host "  -> T4 copy failed"; $script:fail++ }
Step "T4 clear-copy" @("clear", "3001")
$mc5 = Menu-Click 5 $rowPaste "paste" 40
if ($null -eq $mc5 -or $mc5 -notmatch "menu closed") { Write-Host "  -> T4 paste failed"; $script:fail++ }
Step "T4 paste-back" @("getraw") "id=3001 .*\[hell\]"

# one more undo reverts the paste -> empty
$mcG = Menu-Click 0 $rowUndo "fin" 40
if ($null -eq $mcG -or $mcG -notmatch "menu closed") { Write-Host "  -> T4 final undo failed"; $script:fail++ }
Step "T4 final" @("getraw") "id=3001 GETTEXTLENGTH=0"

KillAll
Write-Host ""
if ($script:fail -eq 0) { Write-Host "ALL PASS (T1/T2/T3/T4)"; exit 0 }
else { Write-Host "$($script:fail) FAILURE(S)"; exit 1 }
