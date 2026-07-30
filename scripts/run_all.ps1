$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RunScript = Join-Path $ScriptDir "run.py"

Get-ChildItem `
    -Path $ScriptDir `
    -Filter "*.json" `
    -File |
    Sort-Object Name |
    ForEach-Object {
        Write-Host ""
        Write-Host "========================================"
        Write-Host "Running: $($_.Name)"
        Write-Host "========================================"

        & python.exe $RunScript $_.FullName
    }