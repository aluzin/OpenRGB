# SPDX-License-Identifier: GPL-2.0-or-later
# Run only on an ephemeral GitHub-hosted Windows runner, never on a workstation.
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') { throw 'GitHub-hosted runner required' }
$exe = (Resolve-Path (Join-Path $PSScriptRoot 'build/native.exe')).Path
$dir = Join-Path $PSScriptRoot ('build/native-results-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $dir | Out-Null
$workers = [System.Collections.Generic.List[object]]::new()
function Start-Worker($mode, $role, $name) {
    $id = [guid]::NewGuid().ToString('N')
    $w = [pscustomobject]@{Ready="$dir/$id.ready"; Stop="$dir/$id.stop"; Result="$dir/$id.result"; Task=$null; Process=$null}
    $arguments = @($mode, $role, $name, $w.Ready, $w.Stop, $w.Result)
    if ($role -eq 'system') {
        $w.Task = "OpenRGB-MutexTests-$id"
        $escaped = @($exe) + $arguments | ForEach-Object { "'" + $_.Replace("'", "''") + "'" }
        $command = '$env:GITHUB_ACTIONS=''true''; & ' + ($escaped -join ' ') + '; exit $LASTEXITCODE'
        $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
        $action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -NonInteractive -EncodedCommand $encoded"
        $principal = New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
        $settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 2)
        Register-ScheduledTask -TaskName $w.Task -Action $action -Principal $principal -Settings $settings | Out-Null
        $workers.Add($w)
        Start-ScheduledTask -TaskName $w.Task
    } else {
        $quoted = $arguments | ForEach-Object { '"' + $_ + '"' }
        $w.Process = Start-Process -FilePath $exe -ArgumentList ($quoted -join ' ') -WindowStyle Hidden -PassThru
        $workers.Add($w)
    }
    return $w
}
function Await-File($path, $w) {
    $deadline = [DateTime]::UtcNow.AddSeconds(40)
    while (!(Test-Path -LiteralPath $path)) {
        if ((Test-Path -LiteralPath $w.Result) -and $path -ne $w.Result) { throw (Get-Content -Raw $w.Result) }
        if ([DateTime]::UtcNow -gt $deadline) { throw "Worker timeout: $path" }
        Start-Sleep -Milliseconds 100
    }
}
function Check-Worker($w) {
    Await-File $w.Result $w
    # Wait for process/task completion, not merely creation of its result file.
    if ($w.Process) {
        if (!$w.Process.WaitForExit(10000)) { throw 'Worker did not exit' }
        if ($w.Process.ExitCode -ne 0) { throw (Get-Content -Raw $w.Result) }
    } else {
        $deadline = [DateTime]::UtcNow.AddSeconds(10)
        while ((Get-ScheduledTask -TaskName $w.Task).State -eq 'Running') {
            if ([DateTime]::UtcNow -gt $deadline) { throw 'SYSTEM worker did not exit' }
            Start-Sleep -Milliseconds 100
        }
        if ((Get-ScheduledTaskInfo -TaskName $w.Task).LastTaskResult -ne 0) { throw (Get-Content -Raw $w.Result) }
    }
    $result = Get-Content -Raw $w.Result
    if (!$result.StartsWith('PASS ')) { throw $result }
    Write-Output $result.Trim()
}
function Scenario($ownerMode, $ownerRole, $clientMode, $clientRole) {
    $name = 'Global\OpenRGB.MutexTests.' + [guid]::NewGuid().ToString()
    $owner = Start-Worker $ownerMode $ownerRole $name
    Await-File $owner.Ready $owner
    try {
        $client = Start-Worker $clientMode $clientRole $name
        Check-Worker $client
        if ($ownerMode -eq 'legacy') {
            # The runner has administrative membership but the legacy object
            # denies full access. Reduced-rights fallback must succeed.
            Check-Worker (Start-Worker 'probe' 'runner' $name)
            Check-Worker (Start-Worker 'denied' 'medium' $name)
        }
    } finally { Set-Content -LiteralPath $owner.Stop -Value 'stop' }
    Check-Worker $owner
}
try {
    Scenario 'hold' 'system' 'probe' 'medium'
    Scenario 'hold' 'medium' 'probe' 'system'
    Scenario 'lock' 'system' 'timeout' 'medium'
    Scenario 'lock' 'medium' 'timeout' 'system'
    Scenario 'legacy' 'system' 'denied' 'medium'
    Check-Worker (Start-Worker 'abandoned' 'runner' ('Global\OpenRGB.MutexTests.' + [guid]::NewGuid().ToString()))
    Write-Output 'PASS: real Win32 cross-process identity, startup-order, timeout, legacy-ACL and abandoned-owner tests'
} finally {
    foreach ($w in $workers) {
        if (Test-Path -LiteralPath $w.Ready) { Set-Content -LiteralPath $w.Stop -Value 'stop' }
        if ($w.Process -and !$w.Process.HasExited) { $w.Process | Stop-Process -ErrorAction SilentlyContinue }
        if ($w.Task) {
            Stop-ScheduledTask -TaskName $w.Task -ErrorAction SilentlyContinue
            Unregister-ScheduledTask -TaskName $w.Task -Confirm:$false -ErrorAction SilentlyContinue
        }
    }
}
