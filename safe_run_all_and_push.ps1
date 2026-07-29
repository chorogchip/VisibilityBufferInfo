[CmdletBinding()]
param(
    [string]$Remote = "origin",
    [string]$RunAllPath = "scripts\run_all.ps1",
    [int]$PushRetries = 5,
    [int]$MaxCommitFileMB = 95
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Get-GitOutput {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $output = & git @Arguments 2>&1
    $exitCode = $LASTEXITCODE

    if ($exitCode -ne 0) {
        throw "git $($Arguments -join ' ') failed with exit code $exitCode.`n$($output -join "`n")"
    }

    return (($output -join "`n").Trim())
}

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [switch]$AllowFailure
    )

    & git @Arguments
    $exitCode = $LASTEXITCODE

    if (-not $AllowFailure -and $exitCode -ne 0) {
        throw "git $($Arguments -join ' ') failed with exit code $exitCode."
    }

    return $exitCode
}

function Write-LogTail {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination,
        [int]$LineCount = 1000
    )

    if (Test-Path -LiteralPath $Source -PathType Leaf) {
        Get-Content -LiteralPath $Source -Tail $LineCount -ErrorAction SilentlyContinue |
            Set-Content -LiteralPath $Destination -Encoding UTF8
    }
    else {
        "No log file was created." |
            Set-Content -LiteralPath $Destination -Encoding UTF8
    }
}

function Test-GitOperationInProgress {
    param(
        [Parameter(Mandatory = $true)]
        [string]$GitDirectory
    )

    $markers = @(
        "MERGE_HEAD",
        "CHERRY_PICK_HEAD",
        "REVERT_HEAD",
        "BISECT_LOG",
        "rebase-apply",
        "rebase-merge"
    )

    foreach ($marker in $markers) {
        if (Test-Path -LiteralPath (Join-Path $GitDirectory $marker)) {
            throw "Git operation is already in progress: $marker. Finish or abort it before running this automation."
        }
    }
}

function Remove-UnsafeNewFilesFromIndex {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot
    )

    $blocked = New-Object System.Collections.Generic.List[string]
    $newFiles = @(& git diff --cached --name-only --diff-filter=A 2>$null)

    $sensitiveNamePattern = '(^|/)(\.env($|\.)|id_rsa($|\.)|id_ed25519($|\.)|credentials?\.json$|service[-_]?account.*\.json$)|\.(pem|pfx|p12|key)$'

    foreach ($relativePath in $newFiles) {
        if ([string]::IsNullOrWhiteSpace($relativePath)) {
            continue
        }

        $normalized = $relativePath -replace '\\', '/'

        if ($normalized -match $sensitiveNamePattern) {
            & git reset -q HEAD -- "$relativePath"
            if ($LASTEXITCODE -eq 0) {
                $blocked.Add("$relativePath (new file matched sensitive-name safeguard)")
            }
        }
    }

    return $blocked
}

function Remove-OversizedFilesFromIndex {
    param(
        [Parameter(Mandatory = $true)]
        [long]$MaximumBytes
    )

    $blocked = New-Object System.Collections.Generic.List[string]
    $stagedFiles = @(& git diff --cached --name-only --diff-filter=ACMR 2>$null)

    foreach ($relativePath in $stagedFiles) {
        if ([string]::IsNullOrWhiteSpace($relativePath)) {
            continue
        }

        $stageLine = (& git ls-files -s -- "$relativePath" 2>$null | Select-Object -First 1)

        if ($stageLine -notmatch '^\d+\s+([0-9a-fA-F]+)\s+\d+\s+') {
            continue
        }

        $blobHash = $Matches[1]
        $blobSizeText = (& git cat-file -s $blobHash 2>$null | Select-Object -First 1)

        [long]$blobSize = 0
        if (-not [long]::TryParse([string]$blobSizeText, [ref]$blobSize)) {
            continue
        }

        if ($blobSize -gt $MaximumBytes) {
            & git reset -q HEAD -- "$relativePath"
            if ($LASTEXITCODE -eq 0) {
                $sizeMB = [Math]::Round($blobSize / 1MB, 2)
                $blocked.Add("$relativePath ($sizeMB MB; exceeded $([Math]::Round($MaximumBytes / 1MB, 0)) MB safeguard)")
            }
        }
    }

    return $blocked
}

# Resolve repository before doing anything destructive.
$repositoryRoot = Get-GitOutput -Arguments @("rev-parse", "--show-toplevel")
Set-Location -LiteralPath $repositoryRoot

$gitDirectoryRaw = Get-GitOutput -Arguments @("rev-parse", "--git-dir")
$gitDirectory = if ([System.IO.Path]::IsPathRooted($gitDirectoryRaw)) {
    $gitDirectoryRaw
}
else {
    Join-Path $repositoryRoot $gitDirectoryRaw
}
$gitDirectory = [System.IO.Path]::GetFullPath($gitDirectory)

Test-GitOperationInProgress -GitDirectory $gitDirectory

$resolvedRunAll = Join-Path $repositoryRoot $RunAllPath
if (-not (Test-Path -LiteralPath $resolvedRunAll -PathType Leaf)) {
    throw "Cannot find run-all script: $resolvedRunAll"
}

$null = Get-GitOutput -Arguments @("remote", "get-url", $Remote)
$baseCommit = Get-GitOutput -Arguments @("rev-parse", "HEAD")

$baseBranchOutput = & git symbolic-ref --quiet --short HEAD 2>$null
$baseBranch = if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($baseBranchOutput)) {
    [string]$baseBranchOutput
}
else {
    "(detached HEAD)"
}

if ([string]::IsNullOrWhiteSpace((& git config --get user.name 2>$null))) {
    Invoke-Git -Arguments @("config", "user.name", "Run All Automation") | Out-Null
}
if ([string]::IsNullOrWhiteSpace((& git config --get user.email 2>$null))) {
    Invoke-Git -Arguments @("config", "user.email", "run-all-automation@localhost") | Out-Null
}

$hostNameSafe = if ([string]::IsNullOrWhiteSpace($env:COMPUTERNAME)) {
    "windows"
}
else {
    ($env:COMPUTERNAME -replace '[^A-Za-z0-9._-]', '-').ToLowerInvariant()
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$randomSuffix = [Guid]::NewGuid().ToString("N").Substring(0, 8)
$runId = "$timestamp-$hostNameSafe-$randomSuffix"
$automationBranch = "automation/run-all-$runId"

$lockPath = Join-Path $gitDirectory "safe-run-all-and-push.lock"
$lockStream = $null
$sleepPreventionEnabled = $false
$branchCreated = $false

try {
    try {
        $lockStream = [System.IO.File]::Open(
            $lockPath,
            [System.IO.FileMode]::CreateNew,
            [System.IO.FileAccess]::Write,
            [System.IO.FileShare]::None
        )
        $lockBytes = [System.Text.Encoding]::UTF8.GetBytes("PID=$PID`nRUN_ID=$runId`n")
        $lockStream.Write($lockBytes, 0, $lockBytes.Length)
        $lockStream.Flush()
    }
    catch {
        throw "Another run-all automation appears to be active. Lock file: $lockPath"
    }

    if (-not ("RunAllSleepControl" -as [type])) {
        Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class RunAllSleepControl
{
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint SetThreadExecutionState(uint esFlags);
}
'@
    }

    # ES_CONTINUOUS | ES_SYSTEM_REQUIRED. The display may turn off; the PC should not sleep.
    [RunAllSleepControl]::SetThreadExecutionState([uint32]0x80000001) | Out-Null
    $sleepPreventionEnabled = $true

    Write-Host "[PRECHECK] Creating isolated automation branch: $automationBranch"
    Invoke-Git -Arguments @("switch", "-c", $automationBranch) | Out-Null
    $branchCreated = $true

    Write-Host "[PRECHECK] Testing remote push permission before the long job starts..."
    Invoke-Git -Arguments @(
        "push",
        "--dry-run",
        $Remote,
        "HEAD:refs/heads/$automationBranch"
    ) | Out-Null

    $fullLogDirectory = Join-Path ([System.IO.Path]::GetTempPath()) "run-all-automation\$runId"
    $repoLogDirectory = Join-Path $repositoryRoot ".automation-logs\$runId"

    New-Item -ItemType Directory -Force -Path $fullLogDirectory | Out-Null
    New-Item -ItemType Directory -Force -Path $repoLogDirectory | Out-Null

    $stdoutPath = Join-Path $fullLogDirectory "run_all.stdout.log"
    $stderrPath = Join-Path $fullLogDirectory "run_all.stderr.log"
    $stdoutTailPath = Join-Path $repoLogDirectory "run_all.stdout.tail.log"
    $stderrTailPath = Join-Path $repoLogDirectory "run_all.stderr.tail.log"
    $summaryPath = Join-Path $repoLogDirectory "summary.json"

    $startedAt = Get-Date
    $runExitCode = 1
    $runException = $null

    Write-Host ""
    Write-Host "[READY] Preflight passed."
    Write-Host "[READY] You may lock the PC with Win+L. Do not sign out, shut down, or reboot."
    Write-Host "[RUN]   Executing: $resolvedRunAll"
    Write-Host "[RUN]   Full local logs: $fullLogDirectory"
    Write-Host ""

    try {
        $shellCommand = Get-Command pwsh.exe -ErrorAction SilentlyContinue
        if ($null -eq $shellCommand) {
            $shellCommand = Get-Command powershell.exe -ErrorAction Stop
        }

        $argumentList = @(
            "-NoLogo",
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", "`"$resolvedRunAll`""
        )

        $process = Start-Process `
            -FilePath $shellCommand.Source `
            -ArgumentList $argumentList `
            -WorkingDirectory $repositoryRoot `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath `
            -PassThru `
            -Wait

        $runExitCode = [int]$process.ExitCode
    }
    catch {
        $runException = $_.Exception.ToString()
        $runExitCode = 9001
    }

    $finishedAt = Get-Date

    Write-LogTail -Source $stdoutPath -Destination $stdoutTailPath
    Write-LogTail -Source $stderrPath -Destination $stderrTailPath

    $blockedFiles = New-Object System.Collections.Generic.List[string]

    # Stage everything, including deletions. This is the reliable equivalent of the requested git add .
    Invoke-Git -Arguments @("add", "-A") | Out-Null

    foreach ($entry in (Remove-UnsafeNewFilesFromIndex -RepositoryRoot $repositoryRoot)) {
        $blockedFiles.Add($entry)
    }

    $maximumBytes = [long]$MaxCommitFileMB * 1MB
    foreach ($entry in (Remove-OversizedFilesFromIndex -MaximumBytes $maximumBytes)) {
        $blockedFiles.Add($entry)
    }

    $result = if ($runExitCode -eq 0) { "success" } else { "run_all_failed" }

    $summary = [ordered]@{
        run_id                 = $runId
        result                 = $result
        run_all_exit_code      = $runExitCode
        run_all_exception      = $runException
        started_at             = $startedAt.ToString("o")
        finished_at            = $finishedAt.ToString("o")
        duration_minutes       = [Math]::Round(($finishedAt - $startedAt).TotalMinutes, 2)
        repository_root        = $repositoryRoot
        base_branch            = $baseBranch
        base_commit            = $baseCommit
        automation_branch      = $automationBranch
        remote                 = $Remote
        run_all_script         = $resolvedRunAll
        full_logs_local        = $fullLogDirectory
        committed_log_tails    = $repoLogDirectory
        blocked_from_commit    = @($blockedFiles)
        note                   = "The automation branch is intentionally separate so another clone can continue changing the original branch without blocking this push."
    }

    $summary | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath $summaryPath -Encoding UTF8

    # The summary was written after the first staging pass.
    Invoke-Git -Arguments @("add", "--", $repoLogDirectory) | Out-Null

    $commitMessage = if ($runExitCode -eq 0) {
        "automation: run_all succeeded ($runId)"
    }
    else {
        "automation: run_all failed with exit $runExitCode ($runId)"
    }

    & git diff --cached --quiet
    $hasStagedChanges = ($LASTEXITCODE -ne 0)

    if ($hasStagedChanges) {
        Invoke-Git -Arguments @("commit", "-m", $commitMessage) | Out-Null
    }
    else {
        Invoke-Git -Arguments @("commit", "--allow-empty", "-m", $commitMessage) | Out-Null
    }

    $env:GIT_TERMINAL_PROMPT = "0"
    $pushSucceeded = $false
    $lastPushExitCode = 1

    for ($attempt = 1; $attempt -le $PushRetries; $attempt++) {
        Write-Host "[PUSH] Attempt $attempt of $PushRetries -> $Remote/$automationBranch"

        & git push --set-upstream $Remote "HEAD:refs/heads/$automationBranch"
        $lastPushExitCode = $LASTEXITCODE

        if ($lastPushExitCode -eq 0) {
            $pushSucceeded = $true
            break
        }

        if ($attempt -lt $PushRetries) {
            $delaySeconds = [Math]::Min(300, 15 * [Math]::Pow(2, $attempt - 1))
            Write-Warning "Push failed. Retrying in $delaySeconds seconds."
            Start-Sleep -Seconds ([int]$delaySeconds)
        }
    }

    if (-not $pushSucceeded) {
        $failureMarker = Join-Path $repoLogDirectory "PUSH_FAILED.txt"
        @(
            "Push failed after $PushRetries attempts."
            "Branch: $automationBranch"
            "Remote: $Remote"
            "Last exit code: $lastPushExitCode"
            "Recovery command:"
            "git push --set-upstream $Remote HEAD:refs/heads/$automationBranch"
        ) | Set-Content -LiteralPath $failureMarker -Encoding UTF8

        Write-Error "The work was committed locally, but push failed. See: $failureMarker"
        exit 30
    }

    Write-Host ""
    Write-Host "[DONE] Remote branch: $Remote/$automationBranch"
    Write-Host "[DONE] run_all exit code: $runExitCode"
    Write-Host "[DONE] Summary: $summaryPath"

    if ($blockedFiles.Count -gt 0) {
        Write-Warning "Some files were intentionally not committed:"
        $blockedFiles | ForEach-Object { Write-Warning " - $_" }
    }

    if ($runExitCode -ne 0) {
        Write-Warning "run_all failed, but its partial results and diagnostic log tails were safely committed and pushed to the isolated automation branch."
        exit 20
    }

    exit 0
}
finally {
    if ($sleepPreventionEnabled -and ("RunAllSleepControl" -as [type])) {
        # ES_CONTINUOUS: release the keep-awake request.
        [RunAllSleepControl]::SetThreadExecutionState([uint32]0x80000000) | Out-Null
    }

    if ($null -ne $lockStream) {
        $lockStream.Dispose()
    }

    if (Test-Path -LiteralPath $lockPath) {
        Remove-Item -LiteralPath $lockPath -Force -ErrorAction SilentlyContinue
    }
}
