# Regenerates extras/trophies (icons, SFM files and TROPHY.TRP).
# Usage (Windows PowerShell 5.1):  powershell -ExecutionPolicy Bypass -File extras\scripts\trophies\make_trophies.ps1
# The NP communication ID must be "<VITA_TITLEID>_00" (see CMakeLists.txt).

param(
    [string]$TitleId = "BLNS20001"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$out = Join-Path $root "extras\trophies"

Add-Type -AssemblyName System.Drawing
Add-Type -Path (Join-Path $PSScriptRoot "TrophyPack.cs") -ReferencedAssemblies System.Drawing
[TrophyPack]::Generate($out, "$($TitleId)_00")

Write-Host "Trophy pack written to $out"
