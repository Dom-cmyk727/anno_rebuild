# One-second host and process memory samples (docs/perf/README.md, T10c).
# Writes CSV and flushes every row. Exits 2 if available physical memory or
# available commit falls below the stop thresholds, so a diagnostic run can
# be killed before the machine runs out of memory. Logs belong in cache\.
param(
    [Parameter(Mandatory = $true)][string]$OutFile,
    [int]$PidToWatch = 0,
    [string]$ProcessName = "",
    [string]$Label = "",
    [int]$IntervalMs = 1000,
    [uint64]$StopBelowBytes = 2147483648,
    [uint64]$StopBelowCommitBytes = 2147483648,
    [int]$StopExitCode = 2
)

$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class AnnoMem {
    [StructLayout(LayoutKind.Sequential)]
    public struct MEMORYSTATUSEX {
        public uint dwLength;
        public uint dwMemoryLoad;
        public ulong ullTotalPhys;
        public ulong ullAvailPhys;
        public ulong ullTotalPageFile;
        public ulong ullAvailPageFile;
        public ulong ullTotalVirtual;
        public ulong ullAvailVirtual;
        public ulong ullAvailExtendedVirtual;
    }
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX lpBuffer);

    [StructLayout(LayoutKind.Sequential)]
    public struct PROCESS_MEMORY_COUNTERS_EX {
        public uint cb;
        public uint PageFaultCount;
        public ulong PeakWorkingSetSize;
        public ulong WorkingSetSize;
        public ulong QuotaPeakPagedPoolUsage;
        public ulong QuotaPagedPoolUsage;
        public ulong QuotaPeakNonPagedPoolUsage;
        public ulong QuotaNonPagedPoolUsage;
        public ulong PagefileUsage;
        public ulong PeakPagefileUsage;
        public ulong PrivateUsage;
    }
    [DllImport("psapi.dll", SetLastError = true)]
    public static extern bool GetProcessMemoryInfo(IntPtr hProcess, out PROCESS_MEMORY_COUNTERS_EX counters, uint size);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr h);
}
"@

function Get-HostMem {
    $s = New-Object AnnoMem+MEMORYSTATUSEX
    $s.dwLength = [System.Runtime.InteropServices.Marshal]::SizeOf([type][AnnoMem+MEMORYSTATUSEX])
    if (-not [AnnoMem]::GlobalMemoryStatusEx([ref]$s)) {
        throw "GlobalMemoryStatusEx failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    return $s
}

function Get-ProcMem([int]$procId) {
    $access = 0x1000 -bor 0x0400 # PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_QUERY_INFORMATION
    $h = [AnnoMem]::OpenProcess($access, $false, $procId)
    if ($h -eq [IntPtr]::Zero) {
        return $null
    }
    try {
        $c = New-Object AnnoMem+PROCESS_MEMORY_COUNTERS_EX
        $c.cb = [System.Runtime.InteropServices.Marshal]::SizeOf([type][AnnoMem+PROCESS_MEMORY_COUNTERS_EX])
        $ok = [AnnoMem]::GetProcessMemoryInfo($h, [ref]$c, $c.cb)
        if (-not $ok) { return $null }
        $threads = ""
        try {
            $p = Get-Process -Id $procId -ErrorAction Stop
            $threads = $p.Threads.Count
        } catch {
            $threads = ""
        }
        return [pscustomobject]@{
            Ws = $c.WorkingSetSize
            Private = $c.PrivateUsage
            PeakPrivate = $c.PeakPagefileUsage
            PeakWs = $c.PeakWorkingSetSize
            Threads = $threads
        }
    } finally {
        [void][AnnoMem]::CloseHandle($h)
    }
}

$dir = Split-Path -Parent $OutFile
if ($dir -and -not (Test-Path $dir)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}
$utf8 = New-Object System.Text.UTF8Encoding $false
$fs = New-Object System.IO.FileStream($OutFile, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
$sw = New-Object System.IO.StreamWriter($fs, $utf8)
$sw.AutoFlush = $true
$sw.WriteLine("t_iso,elapsed_s,avail_phys,total_phys,avail_commit,commit_limit,commit_charge,pid,working_set,private,peak_private,peak_ws,threads,label")

$start = [Diagnostics.Stopwatch]::StartNew()
$stopped = $false
try {
    while ($true) {
        $hostMem = Get-HostMem
        $commit = $hostMem.ullTotalPageFile - $hostMem.ullAvailPageFile
        $now = [DateTimeOffset]::Now.ToString("o")
        $elapsed = [math]::Round($start.Elapsed.TotalSeconds, 3)

        $targets = @()
        if ($PidToWatch -gt 0) {
            $targets += $PidToWatch
        }
        if ($ProcessName) {
            Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | ForEach-Object { $targets += $_.Id }
        }
        $targets = $targets | Select-Object -Unique
        if ($targets.Count -eq 0) {
            $sw.WriteLine("$now,$elapsed,$($hostMem.ullAvailPhys),$($hostMem.ullTotalPhys),$($hostMem.ullAvailPageFile),$($hostMem.ullTotalPageFile),$commit,,,,,,$Label")
        } else {
            foreach ($id in $targets) {
                $pm = Get-ProcMem ([int]$id)
                if ($null -eq $pm) {
                    $sw.WriteLine("$now,$elapsed,$($hostMem.ullAvailPhys),$($hostMem.ullTotalPhys),$($hostMem.ullAvailPageFile),$($hostMem.ullTotalPageFile),$commit,$id,,,,,,$Label")
                } else {
                    $sw.WriteLine("$now,$elapsed,$($hostMem.ullAvailPhys),$($hostMem.ullTotalPhys),$($hostMem.ullAvailPageFile),$($hostMem.ullTotalPageFile),$commit,$id,$($pm.Ws),$($pm.Private),$($pm.PeakPrivate),$($pm.PeakWs),$($pm.Threads),$Label")
                }
            }
        }
        if ($hostMem.ullAvailPhys -lt $StopBelowBytes -or $hostMem.ullAvailPageFile -lt $StopBelowCommitBytes) {
            $sw.WriteLine("# STOP avail_phys=$($hostMem.ullAvailPhys) avail_commit=$($hostMem.ullAvailPageFile) below phys $StopBelowBytes commit $StopBelowCommitBytes")
            $stopped = $true
            break
        }
        # Leave once a watched pid has exited and we were not asked to follow a name.
        if ($PidToWatch -gt 0 -and -not $ProcessName) {
            $alive = Get-Process -Id $PidToWatch -ErrorAction SilentlyContinue
            if (-not $alive) { break }
        }
        if ($ProcessName -and -not (Get-Process -Name $ProcessName -ErrorAction SilentlyContinue)) {
            if ($PidToWatch -le 0) { break }
        }
        Start-Sleep -Milliseconds $IntervalMs
    }
} finally {
    $sw.Dispose()
    $fs.Dispose()
}
if ($stopped) { exit $StopExitCode }
exit 0
