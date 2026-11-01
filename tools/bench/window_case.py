#!/usr/bin/env python3
"""window_case.py: one scripted, traced play window on Windows (W4-WIN-4; the native counterpart of
window_case.sh, whose UNIX `time`, `perl` and `shasum` do not exist here).

    python -I tools/bench/window_case.py <anno1503.exe> <bench_picture.exe> <new out dir> \
        <commands file> <w>x<h> <seconds> <tile x>,<tile y> [on|off]

Runs `anno1503 --play data/maps/lan5.txt --commands <file> --renderer gpu` for <seconds>, asking for a
drawable of <w>x<h> and centring the camera on the tile (the same ANNO_VIEW_* variables as the shell
runner). With the trace on, the window's trace lands in <out>/trace and is aggregated into
<out>/aggregate.txt (the aggregate's exit status goes to run.txt). The output layout and the run.txt
keys are the ones tools/bench/window_table.py reads. Differences from the shell runner, stated:
- the process's peak working set (time.txt) is read from the process handle (GetProcessMemoryInfo,
  polled while it runs and once more after it exits): "Peak working set size (bytes): N";
- there is no load average: run.txt records the machine's CPU busy percentage over a 1 s sample
  before the window starts and after it ended ("cpu_busy_pct");
- the window runs natively on Windows (the SDL3/WebGPU build in the given binary); a WSLg window is
  not this tool and is never labelled native.
Needs ANNO1503_GAME_DIR. Exit: the window's status, or 2 for bad arguments."""
import ctypes
import ctypes.wintypes as wt
import hashlib
import os
import platform
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]


class PMC(ctypes.Structure):
    _fields_ = [("cb", wt.DWORD), ("PageFaultCount", wt.DWORD), ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t), ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t), ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t), ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t)]


def peak_working_set(handle):
    pmc = PMC()
    pmc.cb = ctypes.sizeof(PMC)
    ok = ctypes.windll.psapi.GetProcessMemoryInfo(wt.HANDLE(int(handle)), ctypes.byref(pmc), pmc.cb)
    return int(pmc.PeakWorkingSetSize) if ok else None


def cpu_busy_pct(seconds=1.0):
    class FT(ctypes.Structure):
        _fields_ = [("lo", wt.DWORD), ("hi", wt.DWORD)]

    def sample():
        idle, kern, user = FT(), FT(), FT()
        ctypes.windll.kernel32.GetSystemTimes(ctypes.byref(idle), ctypes.byref(kern), ctypes.byref(user))
        v = lambda f: (f.hi << 32) | f.lo
        return v(idle), v(kern) + v(user)

    i0, t0 = sample()
    time.sleep(seconds)
    i1, t1 = sample()
    total = t1 - t0
    return 100.0 * (total - (i1 - i0)) / total if total else 0.0


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def git(*args):
    return subprocess.run(["git", "-C", str(HERE), *args], capture_output=True, text=True).stdout


def total_ram_bytes():
    class MS(ctypes.Structure):
        _fields_ = [("len", wt.DWORD), ("load", wt.DWORD), ("total", ctypes.c_ulonglong),
                    ("avail", ctypes.c_ulonglong), ("tp", ctypes.c_ulonglong), ("ap", ctypes.c_ulonglong),
                    ("tv", ctypes.c_ulonglong), ("av", ctypes.c_ulonglong), ("ae", ctypes.c_ulonglong)]

    m = MS()
    m.len = ctypes.sizeof(MS)
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m))
    return m.total


def main(argv):
    if len(argv) < 7:
        print(__doc__, file=sys.stderr)
        return 2
    app, bench, out, commands, size, seconds, at = argv[:7]
    trace = argv[7] if len(argv) > 7 else "on"
    if trace not in ("on", "off"):
        print("window_case: trace is on or off", file=sys.stderr)
        return 2
    if not os.environ.get("ANNO1503_GAME_DIR"):
        print("window_case: ANNO1503_GAME_DIR is not set", file=sys.stderr)
        return 2
    out = Path(out)
    if out.exists():
        print(f"window_case: {out} exists (use a new directory)", file=sys.stderr)
        return 2
    out.mkdir(parents=True)
    amap = HERE / "data" / "maps" / "lan5.txt"
    diff = git("diff", "HEAD")
    untracked = git("ls-files", "--others", "--exclude-standard").strip().splitlines()
    state_src = diff.encode() + b"".join(
        (n + "\n").encode() + Path(HERE / n).read_bytes() for n in untracked if (HERE / n).is_file())
    state = hashlib.sha1(state_src).hexdigest()[:12]
    clean = "yes" if not diff and not untracked else "no"
    nlines = len(Path(commands).read_text().splitlines())
    lines = [
        f"window_case started={datetime.now():%Y-%m-%d %H:%M:%S}",
        f'host model="{platform.node()}" cpu="{platform.processor()}" cores={os.cpu_count()} '
        f'mem_bytes={total_ram_bytes()} os="{platform.system()} {platform.version()}" native_windows=yes',
        f"load_before=\"cpu_busy_pct={cpu_busy_pct():.1f}\"",
        f"revision={git('rev-parse', 'HEAD').strip()} clean={clean} state={state} untracked={len(untracked)}",
        f"anno1503={app} sha256={sha256(app)}",
        f"bench_picture={bench} sha256={sha256(bench)}",
        f"map={amap} sha256={sha256(amap)}",
        f"commands={commands} sha256={sha256(commands)} lines={nlines}",
        f"drawable_asked={size} seconds={seconds} at={at} trace={trace}",
        "env ANNO_VIEW_LIMITS={} ANNO_VIEW_GPU_CHUNKS={} ANNO_GPU_ADAPTER={} ANNO_VIEW_RESIZE={}".format(
            os.environ.get("ANNO_VIEW_LIMITS", "interactive"), os.environ.get("ANNO_VIEW_GPU_CHUNKS", "off"),
            os.environ.get("ANNO_GPU_ADAPTER", "default"), os.environ.get("ANNO_VIEW_RESIZE", "none")),
    ]
    (out / "run.txt").write_text("\n".join(lines) + "\n")
    env = dict(os.environ)
    env.pop("ANNO_VIEW_TRACE", None)  # a trace-off run never inherits a trace directory
    if trace == "on":
        env["ANNO_VIEW_TRACE"] = str(out / "trace")
    env["ANNO_VIEW_SIZE"], env["ANNO_VIEW_AT"], env["ANNO_VIEW_QUIT_S"] = size, at, seconds
    limit = int(float(seconds)) + 120
    with open(out / "stdout.txt", "wb") as so, open(out / "stderr.txt", "wb") as se:
        proc = subprocess.Popen([app, "--play", str(amap), "--commands", commands, "--renderer", "gpu"],
                                stdout=so, stderr=se, env=env)
        peak = 0
        started = time.monotonic()
        while proc.poll() is None:
            peak = max(peak, peak_working_set(proc._handle) or 0)
            if time.monotonic() - started > limit:
                proc.kill()  # a hung window is bounded, as the shell runner's alarm does
                break
            time.sleep(0.1)
        rc = proc.wait()
        peak = max(peak, peak_working_set(proc._handle) or 0)
    (out / "time.txt").write_text(f"Peak working set size (bytes): {peak}\n"
                                  f"Elapsed (wall clock) seconds: {time.monotonic() - started:.2f}\n")
    with open(out / "run.txt", "a") as f:
        f.write(f"window_exit={rc} ended={datetime.now():%Y-%m-%d %H:%M:%S} "
                f"load_after=\"cpu_busy_pct={cpu_busy_pct():.1f}\"\n")
    if trace == "on":
        with open(out / "aggregate.txt", "wb") as so, open(out / "aggregate.stderr", "wb") as se:
            ag = subprocess.run([bench, "--aggregate", str(out / "trace" / "events.tsv")], stdout=so, stderr=se)
        with open(out / "run.txt", "a") as f:
            f.write(f"aggregate_exit={ag.returncode}\n")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
