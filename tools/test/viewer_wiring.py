#!/usr/bin/env python3
"""viewer_wiring.py: the production play window against a scripted session (W4-WIN-CLOSE C1, C2).

    python -I tools/test/viewer_wiring.py <anno1503 executable> <case> [<new out dir>]
        [--gpu] [--trace] [--adapter N] [--env NAME=VALUE]...

<case> is one of the scripts under tools/test/viewer_wiring/ (without .script):
  r1_held_publication    R1: the title and the script read one consistent state while a publication
                         is held between a turn's outcome record and its snapshot
  r4_occupancy           R4: the occupancy index follows the takes by their deltas, does nothing
                         while the tool is off, ignores another island's edit, and reseeds once
  boundary_*             R2 (GPU window, --gpu): where a change waits on its way to the screen while
                         a stage is held (chunk builders, uploads, dissolve, presents, view, size)
Default: the real viewer (src/app/src/viewer.cpp) with SDL's dummy video driver and the CPU
renderer, so it needs no display and no GPU. --gpu: a real window on the GPU renderer (the
ANNO_GPU_ADAPTER given by --adapter, default 0, with ANNO_REQUIRE_GPU=1); --trace also records the
window trace (<out>/trace). The original game files are required (ANNO1503_GAME_DIR).
The script's expectations are the assertions: a failed one ends the window with a non-zero status
and a `FAIL` line in the session's script.log. The case passes only when the window exits 0, the
log has no FAIL and its last step ran. Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped (no game
files; ctest SKIP_RETURN_CODE)."""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
CASES = HERE / "tools" / "test" / "viewer_wiring"


def main(argv):
    flags = {"gpu": False, "trace": False, "adapter": "0", "env": {}}
    pos = []
    it = iter(argv)
    for a in it:
        if a == "--gpu":
            flags["gpu"] = True
        elif a == "--trace":
            flags["trace"] = True
        elif a == "--adapter":
            flags["adapter"] = next(it, "0")
        elif a == "--env":
            k, _, v = next(it, "=").partition("=")
            flags["env"][k] = v
        else:
            pos.append(a)
    if len(pos) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    exe, case = str(Path(pos[0]).resolve()), pos[1]
    script = CASES / f"{case}.script"
    if not script.is_file() or not Path(exe).is_file():
        print(f"viewer_wiring: no such case or executable: {case} {exe}", file=sys.stderr)
        return 2
    if not os.environ.get("ANNO1503_GAME_DIR"):
        print("skipping: ANNO1503_GAME_DIR is not set (the window needs the original game files)")
        return 77
    out = Path(pos[2]) if len(pos) > 2 else Path(tempfile.mkdtemp(prefix=f"viewer_wiring_{case}_")) / "run"
    if out.exists():
        print(f"viewer_wiring: {out} exists (use a new directory)", file=sys.stderr)
        return 2
    out.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, ANNO_DEV_SCRIPT=str(script), ANNO_DEV_SESSION=str(out),
               ANNO_VIEW_SIZE="1280x720", ANNO_VIEW_AT="1052,444", ANNO_VIEW_QUIT_S="240")
    env.pop("ANNO_VIEW_TRACE", None)
    if flags["gpu"]:
        env.pop("SDL_VIDEODRIVER", None)
        env.update(ANNO_REQUIRE_GPU="1", ANNO_GPU_ADAPTER=flags["adapter"])
        renderer = "gpu"
        if flags["trace"]:
            env["ANNO_VIEW_TRACE"] = str(out.parent / f"{out.name}.trace")
    else:
        env["SDL_VIDEODRIVER"] = "dummy"
        renderer = "cpu"
        if flags["trace"]:
            print("viewer_wiring: --trace needs --gpu", file=sys.stderr)
            return 2
    env.update(flags["env"])
    err_path = out.parent / f"{out.name}.stderr.txt"
    with open(err_path, "wb") as se:
        try:
            proc = subprocess.run([exe, "--play", str(HERE / "data" / "maps" / "lan5.txt"), "--renderer", renderer],
                                  env=env, stdout=subprocess.PIPE, stderr=se, timeout=300)
            rc = proc.returncode
        except subprocess.TimeoutExpired:
            print("viewer_wiring: FAIL: the window did not end within 300 s (watchdog)")
            return 1
    log_file = out / "script.log"
    log = log_file.read_text(encoding="utf-8").splitlines() if log_file.is_file() else []
    fails = [line for line in log if "FAIL" in line]
    steps = (CASES / f"{case}.script").read_text(encoding="utf-8")
    last = [ln.strip() for ln in steps.splitlines() if ln.strip() and not ln.strip().startswith("#")][-1]
    ran_to_end = last == "quit" and not fails and rc == 0
    for line in log:
        print(line)
    if rc != 0 or fails or not ran_to_end:
        print(f"viewer_wiring: FAIL {case}: window exit {rc}; {len(fails)} failed step(s); stderr {err_path}")
        return 1
    print(f"viewer_wiring: PASS {case}: window exit 0, no failed step; session {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
