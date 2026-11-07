param(
    [ValidateRange(1, 60)][int]$Seconds = 15,
    [string]$ProcessName = 'DeskToggle'
)
$ErrorActionPreference = 'Stop'
$targets = @(Get-Process -Name $ProcessName)
if ($targets.Count -ne 1) { throw 'Run exactly one DeskToggle instance first.' }
$target = $targets[0]
$before = $target.TotalProcessorTime.TotalSeconds
$watch = [System.Diagnostics.Stopwatch]::StartNew()
Start-Sleep -Seconds $Seconds
$target.Refresh()
$watch.Stop()
$cpuSeconds = $target.TotalProcessorTime.TotalSeconds - $before
[pscustomobject]@{
    SampleSeconds = [Math]::Round($watch.Elapsed.TotalSeconds, 3)
    CpuMilliseconds = [Math]::Round($cpuSeconds * 1000, 3)
    CpuPercentOfOneCore = [Math]::Round(100 * $cpuSeconds / $watch.Elapsed.TotalSeconds, 4)
    WorkingSetMiB = [Math]::Round($target.WorkingSet64 / 1MB, 3)
    PrivateBytesMiB = [Math]::Round($target.PrivateMemorySize64 / 1MB, 3)
    Threads = $target.Threads.Count
    Handles = $target.HandleCount
} | ConvertTo-Json
