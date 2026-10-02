param([string]$Drive = "D:")

$ErrorActionPreference = "Stop"

function P([string]$rel) {
    Join-Path $Drive $rel
}

$srcA = Join-Path $PSScriptRoot "BobyConnect.exe"
$srcB = Join-Path $PSScriptRoot "BobyConnect_profileB.exe"
$srcLauncher = Join-Path $PSScriptRoot "BobyManualLauncher.exe"

if (!(Test-Path -LiteralPath $srcA)) { throw "BobyConnect.exe fehlt im Paket." }
if (!(Test-Path -LiteralPath $srcLauncher)) { throw "BobyManualLauncher.exe fehlt im Paket." }
if (!(Test-Path -LiteralPath (P "autorunce.mscr"))) { throw "Garmin-Laufwerk stimmt nicht: autorunce.mscr fehlt auf $Drive" }

$bobyDir = P "BobyConnect"
$manualDir = P "ManualReader"
$bobyDest = Join-Path $bobyDir "BobyConnect.exe"
$bobyProfileB = Join-Path $bobyDir "BobyConnect_profileB.exe"
$manualExe = Join-Path $manualDir "ManualReader.exe"
$manualBackup = Join-Path $manualDir "ManualReader_original.exe"

if (!(Test-Path -LiteralPath $manualDir)) { throw "ManualReader-Ordner fehlt: $manualDir" }
if (!(Test-Path -LiteralPath $manualExe)) { throw "Original ManualReader.exe fehlt: $manualExe" }

if (!(Test-Path -LiteralPath $bobyDir)) {
    New-Item -ItemType Directory -Path $bobyDir | Out-Null
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"

if (Test-Path -LiteralPath $bobyDest) {
    Copy-Item -LiteralPath $bobyDest -Destination (Join-Path $bobyDir "BobyConnect_before_v03_$stamp.exe") -Force
}

# Original-Handbuch nur beim ersten Mal sichern.
if (!(Test-Path -LiteralPath $manualBackup)) {
    Copy-Item -LiteralPath $manualExe -Destination $manualBackup -Force
}

Copy-Item -LiteralPath $srcA -Destination $bobyDest -Force
if (Test-Path -LiteralPath $srcB) {
    Copy-Item -LiteralPath $srcB -Destination $bobyProfileB -Force
}
Copy-Item -LiteralPath $srcLauncher -Destination $manualExe -Force

"v0.3 installed $(Get-Date -Format s)" | Set-Content -LiteralPath (Join-Path $bobyDir "v03_installed.txt") -Encoding ASCII

Write-Host ""
Write-Host "=== BOBY CONNECT 0.3 INSTALLIERT ==="
Write-Host "BobyConnect: $bobyDest"
Write-Host "VW Handbuch -> BobyConnect: aktiviert"
Write-Host "Original-Handbuch: $manualBackup"
Write-Host ""
Write-Host "Dein funktionierender autorunce.mscr wurde NICHT veraendert."
Write-Host "Garmin sauber auswerfen und neu starten."
