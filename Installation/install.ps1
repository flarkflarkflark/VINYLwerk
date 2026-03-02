Write-Host 'Installing VINYLwerk for REAPER...' -ForegroundColor Cyan

$reaperPath = "$env:APPDATA\REAPER"
if (-not (Test-Path $reaperPath)) {
    Write-Error "REAPER resource directory not found at $reaperPath"
    exit 1
}

$installDir = "$reaperPath\ScriptslarkAUDIO\VINYLwerk"
if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

# Copy files
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Copy-Item "$scriptDir\..\Scripts\VINYLwerk.lua" "$installDir" -Force

if (Test-Path "$scriptDir\..uildinylwerk_cli_artefacts\Releaseinylwerk_cli.exe") {
    Copy-Item "$scriptDir\..uildinylwerk_cli_artefacts\Releaseinylwerk_cli.exe" "$installDir" -Force
} elseif (Test-Path "$scriptDirinylwerk_cli.exe") {
    Copy-Item "$scriptDirinylwerk_cli.exe" "$installDir" -Force
}

Write-Host "Successfully installed to $installDir" -ForegroundColor Green
Write-Host 'Please restart REAPER and load the script from the Actions list.'
