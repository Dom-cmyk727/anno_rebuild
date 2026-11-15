#!/usr/bin/env python3
"""zoom_sweep_contract.py: ZOOM-MEASURE-1's pass criterion for a camera sweep (docs/render/
zoom-smoothness.md's Proof, ADR 0007 decision 8's acceptance test).

    python -I tools/test/zoom_sweep_contract.py <bench_frame executable> <game_dir> <map file>
        [--res WxH] [--mode legs|wheel|pan|zoompan] [--water enhanced|original]
        [--seconds s] [--control] [--readiness-only] [--warmup-frames n] [--busy-ms ms]
        [--require-env NAME]

Runs bench_frame's --zoom-sweep and parses its one "baseline: ..." summary line. Fails unless every
frame has 0 non-ready chunks, 0 stand-in draws, 0 dissolves in progress, 0 open-sea draws, and a
busy time of at most --busy-ms (default 2.0, the 500 Hz display's frame budget), after excluding
--warmup-frames (default 4: the pass's own first few frames, before GPU/driver caches settle, are
not a readiness failure - bench_frame's own --zoom-sweep-skip does this exclusion, not a looser
threshold here).

--readiness-only drops the busy-time check: only the four readiness counters (non-ready, stand-in,
dissolves, open-sea) must be 0. ZOOM-SMOOTH-1 step 8 registers `--control --readiness-only` in
CTest (below) because the busy-time check, even under --control, is not yet 0 (docs/perf/README.md:
2 of 1,980 control frames exceed the 2 ms budget, measured as GPU/driver submission jitter, not
content readiness - ZOOM-MEASURE-1 review note 2) and the review says not to just raise the
threshold. ZOOM-SMOOTH-3 (window-thread spikes, judged by total busy time) is where that gets a
real measurement and a decided criterion; until then the busy-time number is reported, not gated.

--require-env NAME makes the run skip (exit 77, printing "skipping: set NAME=1 to run it") unless
that environment variable is set to a non-empty value: ZOOM-SMOOTH-1 step 8's CTest registration
passes --require-env ANNO_ZOOM_SWEEP_PROOF (docs/perf/README.md names it), so this needs a GPU and
real frame time and does not run by default - ZOOM-MEASURE-1's own choice not to register this in
CTest at all was that the camera-sweep modes fail by design (relaxing them would hide a regression);
--control --readiness-only does not have that problem, but it still wants an adapter and wall-clock
pacing (real sleep_for calls, not instant), so it stays opt-in like the project's other ANNO_PROBE/
ANNO_SIM_TRANSPORT_STATS-style measurement tests rather than running in every `gate`. A manual
invocation without --require-env (as docs/perf/README.md's existing recipes use) is unaffected.

--control runs the "everything resident" case instead of the camera sweep: --zoom-sweep-mode pan
--zoom-sweep-pan-px 0 --zoom-sweep-warm, a camera that never moves at all (zoom-smoothness.md's own
"any level, camera at rest: 0" row) - this is the proof that the contract above is not impossible to
pass, not a sweep. Needs the original game files and a GPU; skips (exit 77) without ANNO1503_GAME_DIR.
Exit: 0 pass, 1 fail (numbers printed either way), 2 bad arguments, 77 skipped.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]

BASELINE_RE = re.compile(
    r"^baseline: .*?frames (\d+), non-ready frames (\d+), most non-ready at once (\d+), "
    r"stand-in draws (\d+), dissolves (\d+), open-sea draws (\d+), wave switches (\d+), "
    r"frames in wave fade (\d+), busy ms p50 ([\d.]+), p99 ([\d.]+), max ([\d.]+), "
    r"frames > [\d.]+ ms (\d+), frames > [\d.]+ ms \d+$"
)


def main(argv):
    flags = {"res": "2560x1369", "mode": "legs", "water": "enhanced", "seconds": "1",
             "control": False, "readiness_only": False, "warmup_frames": "4", "busy_ms": "2.0",
             "require_env": None}
    pos = []
    it = iter(argv)
    for a in it:
        if a == "--res":
            flags["res"] = next(it, "")
        elif a == "--mode":
            flags["mode"] = next(it, "")
        elif a == "--water":
            flags["water"] = next(it, "")
        elif a == "--seconds":
            flags["seconds"] = next(it, "")
        elif a == "--control":
            flags["control"] = True
        elif a == "--readiness-only":
            flags["readiness_only"] = True
        elif a == "--warmup-frames":
            flags["warmup_frames"] = next(it, "")
        elif a == "--busy-ms":
            flags["busy_ms"] = next(it, "")
        elif a == "--require-env":
            flags["require_env"] = next(it, "")
        else:
            pos.append(a)
    if len(pos) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    if flags["require_env"] and not os.environ.get(flags["require_env"]):
        print(f"skipping: set {flags['require_env']}=1 to run it")
        return 77
    exe, game_dir, map_file = pos
    # ANNO1503_GAME_DIR overrides the positional game_dir when set (CLAUDE.md: "never add original
    # files to fixtures/ or anywhere else in git"; a worktree's own game/app does not exist, since
    # game/ is git-ignored and worktrees do not share working-tree-only files - a CMake-passed
    # literal ${PROJECT_SOURCE_DIR}/game/app, as step 8's CTest registration passes, is therefore
    # wrong in a worktree even though the env var is set and correct).
    env_dir = os.environ.get("ANNO1503_GAME_DIR")
    if env_dir:
        game_dir = env_dir
    elif not Path(game_dir, "Anlagen.dat").is_file():
        print("skipping: ANNO1503_GAME_DIR is not set (needs the original game files)")
        return 77
    args = [exe, game_dir, map_file, "--renderer", "gpu", "--limits", "interactive",
            "--derived", "3", "--groups", "0,1,2", "--styles", flags["water"],
            "--res", flags["res"], "--zoom-sweep", flags["seconds"],
            "--zoom-sweep-skip", flags["warmup_frames"]]
    if flags["control"]:
        args += ["--zoom-sweep-mode", "pan", "--zoom-sweep-pan-px", "0", "--zoom-sweep-warm"]
    else:
        args += ["--zoom-sweep-mode", flags["mode"]]
        if flags["mode"] in ("pan", "zoompan"):
            args += ["--zoom-sweep-pan-px", "1200"]
        if flags["mode"] == "wheel":
            args += ["--zoom-sweep-wheel-rate", "8"]
    proc = subprocess.run(args, capture_output=True, text=True, timeout=600)
    print(proc.stdout)
    if proc.stderr.strip():
        print(proc.stderr, file=sys.stderr)
    if proc.returncode != 0:
        print(f"zoom_sweep_contract: bench_frame exited {proc.returncode}", file=sys.stderr)
        return 1
    lines = [m for m in (BASELINE_RE.match(l) for l in proc.stdout.splitlines()) if m]
    if not lines:
        print("zoom_sweep_contract: no 'baseline: ...' line in bench_frame's output", file=sys.stderr)
        return 1
    m = lines[-1]
    (frames, non_ready, most_missing, stand, dissolves, open_sea, wave_switches, wave_fade,
     p50, p99, busy_max, over_budget) = m.groups()
    busy_ms = float(flags["busy_ms"])
    checks = [
        ("non-ready frames", int(non_ready), 0),
        ("stand-in draws", int(stand), 0),
        ("dissolves", int(dissolves), 0),
        ("open-sea draws", int(open_sea), 0),
    ]
    if not flags["readiness_only"]:
        checks.append(("frames over the busy budget", int(over_budget), 0))
    failed = [(name, value) for name, value, limit in checks if value > limit]
    print(f"zoom_sweep_contract: {map_file}, {flags['res']}, {flags['water']} water, "
          f"{'control' if flags['control'] else flags['mode']} mode, "
          f"{flags['warmup_frames']} warm-up frames excluded, busy budget {busy_ms} ms: "
          f"frames={frames} non_ready={non_ready} most_missing={most_missing} stand={stand} "
          f"dissolves={dissolves} open_sea={open_sea} wave_switches={wave_switches} "
          f"wave_fade={wave_fade} busy_p50={p50} busy_p99={p99} busy_max={busy_max} "
          f"over_budget={over_budget}")
    if failed:
        print("zoom_sweep_contract: FAIL — " +
              ", ".join(f"{name} {value} > 0" for name, value in failed), file=sys.stderr)
        return 1
    print("zoom_sweep_contract: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
