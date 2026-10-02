param([string]$Drive = "D:")

$ErrorActionPreference = "Stop"

$manualExe = Join-Path $Drive "ManualReader\ManualReader.exe"
$manualBackup = Join-Path $Drive "ManualReader\ManualReader_original.exe"

if (!(Test-Path -LiteralPath $manualBackup)) {
    throw "ManualReader_original.exe nicht gefunden."
}

Copy-Item -LiteralPath $manualBackup -Destination $manualExe -Force

Write-Host ""
Write-Host "Handbuch wurde wiederhergestellt."
Write-Host "BobyConnect und dein autorunce.mscr bleiben unangetastet."
