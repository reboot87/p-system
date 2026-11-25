# list_lab_suite.ps1 — ListBox multi/single-select cross-process scenarios (3b).
#
# Drivers: tools/diagnostics/listtest.exe (click -> client (30, rowTop+24),
# mods via wParam: MK_CONTROL=8, MK_SHIFT=4; state via selstate -> mirror
# EDIT text + LB_GETSEL/LB_GETSELCOUNT).
# Scenarios (3201 multi, 3202 single):
#   L1 plain click row0     -> sel=[0]
#   L2 Ctrl+click row2      -> sel=[0,2]
#   L3 Ctrl+click row4      -> sel=[0,2,4]
#   L4 Shift+click row3     -> range 0..3 added -> sel=[0,2,3,4]
#   L5 Ctrl+click row3      -> toggled off
#   L6 Ctrl+click row3      -> toggled back
#   L7 3202 plain click row5 -> sel=[5]
#   L8 3202 Ctrl+click row1  -> replaced -> sel=[1]
#
# Exit 0 = ALL PASS, 1 = failure(s).

param([string]$BuildDir = "build")

$fail = 0
$exe = "$BuildDir\Release\listtest.exe"
$find = "$BuildDir\findwin.exe"
if ((-not (Test-Path $exe)) -or (-not (Test-Path $find))) {
    Write-Host "drivers missing (listtest/findwin): build airui_textbox_lab first"
    exit 1
}

function Check([bool]$ok, [string]$what, [string]$expect, [object]$got)
{
    if ($ok) { Write-Host "  -> PASS  $what" }
    else {
        Write-Host "  -> FAIL  $what : expect [$expect] got [$got]"
        $script:fail = $script:fail + 1
    }
}

function ClientTop([string]$listOut, [int]$id, [int]$row)
{
    $m = [regex]::Match($listOut, ("list id=$id row" + $row + " client=\(\d+,(\d+),\d+,\d+\) "))
    if ($m.Success) { return [int]$m.Groups[1].Value }
    return -1
}

function Click([int]$h, [int]$id, [int]$row, [int]$mods, [string]$listOut)
{
    $top = ClientTop $listOut $id $row
    if ($top -lt 0) {
        Check $false ("click row" + $row + " of id=" + $id) "row rect present" "MISSING"
        return
    }
    & $exe click $h $id 30 ([int]$top + 24) ([string]$mods) > $null
    Start-Sleep -Milliseconds 500
}

function SelSet([string]$s, [int]$id)
{
    $m = [regex]::Match($s, ("id=$id LB_CURSEL=(-?\d+) selcount=(-?\d+) sel=\[([^\]]*)\]"))
    if (-not $m.Success) { return $null }
    $set = @()
    foreach ($tok in ($m.Groups[3].Value -split ",")) {
        $v = $tok.Trim(" ,").Trim()
        if ($v -eq "") { continue }
        $n = [int]$v
        if ($n -ge 0) { $set += $n }
    }
    return ($set | Sort-Object -Unique)
}

function SelOf([int]$h, [int]$id)
{
    $s = (& $exe selstate $h | Out-String)
    return (SelSet $s $id)
}

# ---- find the lab window ----
$h = $null
for ($i = 0; $i -lt 30; $i++) {
    $h = (& $find TextBoxLab | ForEach-Object { $_.Split(" ")[0] } | Sort-Object -Descending | Select-Object -First 1)
    if ($h) { break }
    Start-Sleep -Milliseconds 500
}
if (-not $h) { Write-Host "FAIL: lab window not found"; exit 1 }
Write-Host "lab hwnd=$h"

# ---- baseline rect ----
$rectOut = (& $exe rect $h | Out-String)
$hasBase = $rectOut -and ($rectOut -match "id=3201 LB_CURSEL")
if (-not $hasBase) {
    Check $hasBase "baseline rect (3201 present)" "id=3201 line" $rectOut
    exit 1
}

Write-Host "=== L1..L6 multi list (3201) ==="

# L1: plain click row0 -> sel=[0]
Click $h 3201 0 0 $rectOut
$r = SelOf $h 3201
Check (($r -ne $null) -and ($r.Count -eq 1) -and ($r -ccontains 0)) "L1 plain click row0" "[0]" $r

# L2: ctrl+click row2 -> sel=[0,2]
Click $h 3201 2 8 $rectOut
$r = SelOf $h 3201
Check (($r -ne $null) -and ($r.Count -eq 2) -and ($r -ccontains 0) -and ($r -ccontains 2)) "L2 ctrl+click row2" "[0,2]" $r

# L3: ctrl+click row4 -> sel=[0,2,4]
Click $h 3201 4 8 $rectOut
$r = SelOf $h 3201
Check (($r -ne $null) -and ($r.Count -eq 3) -and ($r -ccontains 0) -and ($r -ccontains 2) -and ($r -ccontains 4)) "L3 ctrl+click row4" "[0,2,4]" $r

# L4: shift+click row3 -> range from anchor(0) to 3 -> {0,1,2,3,4} (1 is added too)
Click $h 3201 3 4 $rectOut
$r = SelOf $h 3201
Check (($r -ne $null) -and ($r -ccontains 0) -and ($r -ccontains 1) -and ($r -ccontains 2) -and ($r -ccontains 3) -and ($r -ccontains 4)) "L4 shift+click row3 -> 0..3 added" "[0,1,2,3,4]" $r

# L5: ctrl+click row3 -> toggle off (3 must not be in the set)
Click $h 3201 3 8 $rectOut
$r = SelOf $h 3201
Check (($r -ne $null) -and (-not ($r -ccontains 3))) "L5 ctrl+click row3 -> toggle off" "[no 3]" $r

# L6: ctrl+click row3 again -> toggle back (5 items: 0,1,2,3,4)
Click $h 3201 3 8 $rectOut
$r = SelOf $h 3201
Check (($r -ne $null) -and ($r.Count -eq 5) -and ($r -ccontains 3)) "L6 ctrl+click row3 -> toggled back" "[0,1,2,3,4]" $r

Write-Host "=== L7..L8 single list (3202) ==="

function SingleOf([int]$h)
{
    $s = (& $exe selstate $h | Out-String)
    $m = [regex]::Match($s, "id=3202 LB_CURSEL=(-?\d+) selcount=(-?\d+) sel=\[([^\]]*)\]")
    if (-not $m.Success) { return $null }
    $set = @()
    foreach ($tok in ($m.Groups[3].Value -split ",")) {
        $v = $tok.Trim(" ,").Trim()
        if ($v -eq "") { continue }
        $n = [int]$v
        if ($n -ge 0) { $set += $n }
    }
    return ($set | Sort-Object -Unique)
}

# L7: plain click row5 -> sel=[5]
Click $h 3202 5 0 $rectOut
$r = SingleOf $h
Check (($r -ne $null) -and ($r.Count -eq 1) -and ($r[0] -eq 5)) "L7 plain click row5" "[5]" $r

# L8: ctrl+click row1 -> replace -> sel=[1]
Click $h 3202 1 8 $rectOut
$r = SingleOf $h
Check (($r -ne $null) -and ($r.Count -eq 1) -and ($r[0] -eq 1)) "L8 ctrl+click row1 -> replace" "[1]" $r

Write-Host ""
if ($fail -eq 0) { Write-Host "ALL PASS (3b listbox multi/single-select)"; exit 0 }
Write-Host ("$fail FAILURE(S)")
exit 1
