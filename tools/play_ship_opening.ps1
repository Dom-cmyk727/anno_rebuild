<#
.SYNOPSIS
  Starts the ANNO 1503 rebuild's ship-start opening for a person to play (W4-OPENING-1).

.DESCRIPTION
  Five expeditions (one ship each, no city) on a map that is an explicitly named playtest input, at peace.
  You are one seat: select your ship, sail it to an island, found a warehouse, unload, build, save.
  The controls are in docs/testing/ship-opening-playtest.md and in the window title.

.PARAMETER Seat
  Your seat, 0..4 (default 0). The ships of the others can be looked at, never ordered.
.PARAMETER Speed
  Game speed: 50, 100 or 200 (default 200: seat 0's first island is 550 tiles away, about 90 s at 200).
.PARAMETER Resume
  A quick-save file written by F5 (saves\quicksave-seat<N>.sav): continue from it. Same map and seat.
.PARAMETER Manifest
  The ship-start map (default: candidate A, data\maps\candidates\ship-start-v2\lan5-ship-a.json).
.PARAMETER GameDir
  The original game's folder (the one with Anlagen.dat). Default: $env:ANNO1503_GAME_DIR, else a
  game\app folder found above this script.
.PARAMETER Exe
  The anno1503.exe to run (default: the Release build of this checkout).
.PARAMETER Renderer
  auto (default: the GPU when there is one), gpu or cpu.

.EXAMPLE
  .\tools\play_ship_opening.ps1 -GameDir E:\anno1503\game\app
  .\tools\play_ship_opening.ps1 -Resume .\saves\quicksave-seat0.sav
#>
param(
    [ValidateRange(0, 4)][int]$Seat = 0,
    [ValidateSet(50, 100, 200)][int]$Speed = 200,
    [string]$Resume,
    [string]$Manifest,
    [string]$GameDir,
    [string]$Exe,
    [ValidateSet("auto", "gpu", "cpu")][string]$Renderer = "auto"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $Manifest) { $Manifest = Join-Path $root "data\maps\candidates\ship-start-v2\lan5-ship-a.json" }
if (-not $Exe) { $Exe = Join-Path $root "build\msvc\src\app\Release\anno1503.exe" }
if (-not $GameDir) { $GameDir = $env:ANNO1503_GAME_DIR }
if (-not $GameDir) {
    $dir = $root
    while ($dir -and -not $GameDir) {
        $candidate = Join-Path $dir "game\app"
        if (Test-Path (Join-Path $candidate "Anlagen.dat")) { $GameDir = $candidate }
        $dir = Split-Path -Parent $dir
    }
}
if (-not $GameDir -or -not (Test-Path (Join-Path $GameDir "Anlagen.dat"))) {
    throw "The original game folder was not found (it holds Anlagen.dat). Pass -GameDir <folder> or set ANNO1503_GAME_DIR."
}
if (-not (Test-Path $Exe)) { throw "anno1503.exe not found at $Exe (build it: cmake --build build/msvc --config Release)." }
if (-not (Test-Path $Manifest)) { throw "The ship-start map was not found: $Manifest" }
$arguments = @("--ship-start", $Manifest, "--seat", $Seat, "--speed", $Speed, "--game-dir", $GameDir,
               "--renderer", $Renderer)
if ($Resume) {
    if (-not (Test-Path $Resume)) { throw "The save was not found: $Resume" }
    $arguments += @("--resume", $Resume)
}
Write-Host "anno1503 ship-start opening: seat $Seat, speed $Speed, map $Manifest"
Write-Host "Controls: docs/testing/ship-opening-playtest.md. Saves go to $((Get-Location).Path)\saves (set ANNO_SAVE_DIR to change)."
& $Exe @arguments
exit $LASTEXITCODE
