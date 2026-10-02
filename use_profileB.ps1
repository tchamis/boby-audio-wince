param([string]$Drive = "D:")

$ErrorActionPreference = "Stop"
$dir = Join-Path $Drive "BobyConnect"
$active = Join-Path $dir "BobyConnect.exe"
$variant = Join-Path $dir "BobyConnect_profileB.exe"

if (!(Test-Path -LiteralPath $variant)) {
    throw "BobyConnect_profileB.exe fehlt. Installiere zuerst v0.3."
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if (Test-Path -LiteralPath $active) {
    Copy-Item -LiteralPath $active -Destination (Join-Path $dir "BobyConnect_before_profileB_$stamp.exe") -Force
}

Copy-Item -LiteralPath $variant -Destination $active -Force

Write-Host ""
Write-Host "Bluetooth-Profil-Mapping B ist jetzt aktiv."
Write-Host "Garmin sauber auswerfen und neu starten."
