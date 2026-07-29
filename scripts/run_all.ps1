[CmdletBinding()]
param(
    [string]$ExperimentDirectory = "",
    [switch]$ValidateOnly,
    [switch]$AllowExistingResults
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Get-PropertyValue {
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if ($null -eq $Object) {
        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function Resolve-PathFromDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$BaseDirectory,
        [Parameter(Mandatory = $true)][string]$Value
    )

    if ([System.IO.Path]::IsPathRooted($Value)) {
        return [System.IO.Path]::GetFullPath($Value)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BaseDirectory $Value))
}

function Get-EstimatedRunCount {
    param([Parameter(Mandatory = $true)]$Spec)

    $repeatValue = Get-PropertyValue -Object $Spec -Name "repeat"
    $repeat = if ($null -eq $repeatValue) { 1 } else { [int]$repeatValue }

    $samples = Get-PropertyValue -Object $Spec -Name "samples"
    if ($null -ne $samples) {
        return $repeat * @($samples).Count
    }

    $sweep = Get-PropertyValue -Object $Spec -Name "sweep"
    if ($null -ne $sweep) {
        [long]$product = 1
        foreach ($property in $sweep.PSObject.Properties) {
            $values = @($property.Value)
            if ($values.Count -eq 0) {
                throw "Sweep '$($property.Name)' has no values."
            }
            $product *= $values.Count
        }
        return $repeat * $product
    }

    return $repeat
}

function Assert-ReferencedFiles {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)][string]$ExecutableDirectory,
        [Parameter(Mandatory = $true)][string]$ConfigName
    )

    $records = New-Object System.Collections.Generic.List[object]
    $base = Get-PropertyValue -Object $Spec -Name "base"
    if ($null -ne $base) {
        $records.Add($base)
    }

    $samples = Get-PropertyValue -Object $Spec -Name "samples"
    if ($null -ne $samples) {
        foreach ($sample in @($samples)) {
            $records.Add($sample)
        }
    }

    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)

    foreach ($record in $records) {
        foreach ($propertyName in @("scene_path", "camera_filepath")) {
            $value = Get-PropertyValue -Object $record -Name $propertyName
            if ($null -eq $value) {
                continue
            }

            $text = [string]$value
            if ([string]::IsNullOrWhiteSpace($text) -or $text -eq "unused") {
                continue
            }

            $resolved = Resolve-PathFromDirectory -BaseDirectory $ExecutableDirectory -Value $text
            if (-not $seen.Add($resolved)) {
                continue
            }

            if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
                throw "[$ConfigName] Referenced file does not exist: $propertyName -> $resolved"
            }
        }
    }
}

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = Split-Path -Parent $scriptDirectory
$runScript = Join-Path $scriptDirectory "run.py"

if ([string]::IsNullOrWhiteSpace($ExperimentDirectory)) {
    $ExperimentDirectory = Join-Path $scriptDirectory "material_experiments"
}
elseif (-not [System.IO.Path]::IsPathRooted($ExperimentDirectory)) {
    $ExperimentDirectory = Join-Path $repositoryRoot $ExperimentDirectory
}
$ExperimentDirectory = [System.IO.Path]::GetFullPath($ExperimentDirectory)

if (-not (Test-Path -LiteralPath $runScript -PathType Leaf)) {
    throw "Cannot find runner: $runScript"
}
if (-not (Test-Path -LiteralPath $ExperimentDirectory -PathType Container)) {
    throw "Cannot find experiment directory: $ExperimentDirectory"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
$pythonPrefix = @()
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command py.exe -ErrorAction SilentlyContinue
    $pythonPrefix = @("-3")
}
if ($null -eq $pythonCommand) {
    throw "Python 3 was not found. Install it or add python.exe/py.exe to PATH."
}

# Deliberately scan only this directory. This avoids re-running copied JSON files
# under material_experiments/results on a later invocation.
$configFiles = @(
    Get-ChildItem -LiteralPath $ExperimentDirectory -Filter "*.json" -File |
        Sort-Object Name
)

if ($configFiles.Count -eq 0) {
    throw "No experiment JSON files were found in: $ExperimentDirectory"
}

$plans = New-Object System.Collections.Generic.List[object]
$skipped = New-Object System.Collections.Generic.List[string]
[long]$estimatedTotalRuns = 0

foreach ($configFile in $configFiles) {
    try {
        $spec = Get-Content -LiteralPath $configFile.FullName -Raw -Encoding UTF8 |
            ConvertFrom-Json
    }
    catch {
        throw "Invalid JSON: $($configFile.FullName)`n$($_.Exception.Message)"
    }

    $status = Get-PropertyValue -Object $spec -Name "_status"
    if ($null -ne $status -and [string]$status -ne "runnable_now") {
        $skipped.Add("$($configFile.Name) (_status=$status)")
        continue
    }

    $executableValue = Get-PropertyValue -Object $spec -Name "executable"
    if ([string]::IsNullOrWhiteSpace([string]$executableValue)) {
        throw "[$($configFile.Name)] Missing required 'executable' field."
    }

    $executable = Resolve-PathFromDirectory `
        -BaseDirectory $configFile.DirectoryName `
        -Value ([string]$executableValue)

    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "[$($configFile.Name)] TVBPerf executable does not exist: $executable"
    }

    $executableDirectory = Split-Path -Parent $executable

    foreach ($runtimeItem in @(
        (Join-Path $executableDirectory "dxcompiler.dll"),
        (Join-Path $executableDirectory "dxil.dll"),
        (Join-Path $executableDirectory "assets\shaders")
    )) {
        if (-not (Test-Path -LiteralPath $runtimeItem)) {
            throw "[$($configFile.Name)] Required runtime item does not exist: $runtimeItem"
        }
    }

    Assert-ReferencedFiles `
        -Spec $spec `
        -ExecutableDirectory $executableDirectory `
        -ConfigName $configFile.Name

    $resultDirectory = Join-Path `
        (Join-Path $configFile.DirectoryName "results") `
        $configFile.BaseName

    if (-not $AllowExistingResults -and
        (Test-Path -LiteralPath $resultDirectory -PathType Container) -and
        $null -ne (Get-ChildItem -LiteralPath $resultDirectory -Force | Select-Object -First 1)) {
        throw "[$($configFile.Name)] Existing result directory is not empty: $resultDirectory`nRefusing to append duplicate data. Move/delete it or pass -AllowExistingResults intentionally."
    }

    $estimatedRuns = Get-EstimatedRunCount -Spec $spec
    $estimatedTotalRuns += $estimatedRuns

    $plans.Add([pscustomobject]@{
        Config = $configFile
        Executable = $executable
        EstimatedRuns = $estimatedRuns
    })
}

if ($plans.Count -eq 0) {
    throw "Experiment JSON files exist, but none are marked runnable_now."
}

Write-Host ""
Write-Host "[VALIDATION] Repository: $repositoryRoot"
Write-Host "[VALIDATION] Experiment directory: $ExperimentDirectory"
Write-Host "[VALIDATION] Configs to run: $($plans.Count)"
Write-Host "[VALIDATION] Estimated benchmark invocations: $estimatedTotalRuns"
Write-Host "[VALIDATION] Executable: $($plans[0].Executable)"
if ($skipped.Count -gt 0) {
    Write-Host "[VALIDATION] Skipped configs:"
    $skipped | ForEach-Object { Write-Host "  - $_" }
}

if ($ValidateOnly) {
    Write-Host "[VALIDATION] OK"
    exit 0
}

$failures = New-Object System.Collections.Generic.List[object]
$startedAt = Get-Date

for ($index = 0; $index -lt $plans.Count; $index++) {
    $plan = $plans[$index]
    $configFile = $plan.Config

    Write-Host ""
    Write-Host "============================================================"
    Write-Host "[$($index + 1)/$($plans.Count)] $($configFile.Name)"
    Write-Host "Estimated runs: $($plan.EstimatedRuns)"
    Write-Host "============================================================"

    try {
        & $pythonCommand.Source @pythonPrefix $runScript $configFile.FullName
        $exitCode = $LASTEXITCODE
    }
    catch {
        $exitCode = 9001
        Write-Error -ErrorAction Continue $_
    }

    if ($exitCode -ne 0) {
        $failures.Add([pscustomobject]@{
            config = $configFile.FullName
            exit_code = $exitCode
        })
        Write-Warning "Experiment failed with exit code $exitCode; continuing with the remaining configs."
    }
}

$finishedAt = Get-Date
$summaryDirectory = Join-Path $ExperimentDirectory "results"
New-Item -ItemType Directory -Force -Path $summaryDirectory | Out-Null
$summaryPath = Join-Path $summaryDirectory (
    "_run_all_summary_{0}.json" -f (Get-Date -Format "yyyyMMdd_HHmmss")
)

[ordered]@{
    status = if ($failures.Count -eq 0) { "completed" } else { "completed_with_errors" }
    started_at = $startedAt.ToString("o")
    finished_at = $finishedAt.ToString("o")
    duration_minutes = [Math]::Round(($finishedAt - $startedAt).TotalMinutes, 2)
    config_count = $plans.Count
    estimated_benchmark_invocations = $estimatedTotalRuns
    failure_count = $failures.Count
    failures = @($failures)
} | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host ""
Write-Host "[SUMMARY] $summaryPath"
Write-Host "[SUMMARY] Config failures: $($failures.Count)"

if ($failures.Count -gt 0) {
    exit 1
}

exit 0
