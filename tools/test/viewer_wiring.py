#!/usr/bin/env python3
"""viewer_wiring.py: the production play window against a scripted session (W4-WIN-CLOSE C1, C2).

    python -I tools/test/viewer_wiring.py <anno1503 executable> <case> [<new out dir>]
        [--gpu] [--trace] [--adapter N] [--env NAME=VALUE]... [--max-seconds S] [--expect-stderr TEXT]...

<case> is one of the scripts under tools/test/viewer_wiring/ (without .script):
  r1_held_publication    R1: the title and the script read one consistent state while a publication
                         is held between a turn's outcome record and its snapshot
  r4_occupancy           R4: the occupancy index follows the takes by their deltas, does nothing
                         while the tool is off, ignores another island's edit, and reseeds once
  boundary_exit_in_hold  R2: a session that ends while the chunk builders are held exits at once
                         (--gpu --max-seconds 15)
  boundary_*             R2 (GPU window, --gpu): where a change waits on its way to the screen while
                         a stage is held (chunk builders, uploads, dissolve, presents, view, size)
Default: the real viewer (src/app/src/viewer.cpp) with SDL's dummy video driver and the CPU
renderer, so it needs no display and no GPU. --gpu: a real window on the GPU renderer (the
ANNO_GPU_ADAPTER given by --adapter, default 0, with ANNO_REQUIRE_GPU=1); --trace also records the
window trace (<out>/trace). The original game files are required (ANNO1503_GAME_DIR).
The script's expectations are the assertions: a failed one ends the window with a non-zero status
and a `FAIL` line in the session's script.log. The case passes only when the window exits 0 and the
session's script.log shows the script run: present, not empty, every non-wait step of the invoked script
recorded in order up to its terminal `quit`, no FAIL line (tools/test/viewer_session.py). <case> may
also be the path of a .script file (the contract tests). Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped (no game
files; ctest SKIP_RETURN_CODE)."""
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))  # -I leaves the script's directory off the path
import viewer_session  # noqa: E402

# The window's watchdog; ANNO_VIEWER_WIRING_WATCHDOG_S shortens it for the contract tests (a test hook).
WATCHDOG_S = int(os.environ.get("ANNO_VIEWER_WIRING_WATCHDOG_S", "300"))
CASES = HERE / "tools" / "test" / "viewer_wiring"


def main(argv):
    # The session log holds the window title (an em dash). A Windows pipe defaults to the ANSI code page, and the
    # gate reads this output back as UTF-8 (found by the first W4-WIN-FINAL gate run): say UTF-8.
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    flags = {"gpu": False, "trace": False, "adapter": "0", "env": {}, "max_seconds": None, "stderr": []}
    pos = []
    it = iter(argv)
    for a in it:
        if a == "--gpu":
            flags["gpu"] = True
        elif a == "--trace":
            flags["trace"] = True
        elif a == "--adapter":
            flags["adapter"] = next(it, "0")
        elif a == "--max-seconds":
            flags["max_seconds"] = float(next(it, "0"))
        elif a == "--expect-stderr":
            flags["stderr"].append(next(it, ""))
        elif a == "--env":
            k, _, v = next(it, "=").partition("=")
            flags["env"][k] = v
        else:
            pos.append(a)
    if len(pos) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    exe, case = str(Path(pos[0]).resolve()), pos[1]
    # A case is a script of tools/test/viewer_wiring/, or (for the contract tests) the path of a script file.
    script = Path(case) if case.endswith(".script") else CASES / f"{case}.script"
    case = script.stem
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
    started = time.monotonic()
    with open(err_path, "wb") as se:
        try:
            proc = subprocess.run([exe, "--play", str(HERE / "data" / "maps" / "lan5.txt"), "--renderer", renderer],
                                  env=env, stdout=subprocess.DEVNULL, stderr=se, timeout=WATCHDOG_S)
            rc = proc.returncode
        except subprocess.TimeoutExpired:
            print(f"viewer_wiring: FAIL {case}: the window did not end within {WATCHDOG_S} s (watchdog)")
            return 1
    elapsed = time.monotonic() - started
    log_file = out / "script.log"
    log_text = log_file.read_text(encoding="utf-8", errors="replace") if log_file.is_file() else None
    for line in (log_text or "").splitlines():
        print(line)
    if flags["max_seconds"] is not None and elapsed > flags["max_seconds"]:
        print(f"viewer_wiring: FAIL {case}: the window took {elapsed:.1f} s, more than {flags['max_seconds']} s")
        return 1
    if rc != 0:
        print(f"viewer_wiring: FAIL {case}: window exit {rc}; stderr {err_path}")
        return 1
    stderr_text = err_path.read_text(encoding="utf-8", errors="replace") if err_path.is_file() else ""
    for needle in flags["stderr"]:
        if needle not in stderr_text:
            print(f"viewer_wiring: FAIL {case}: the window's stderr lacks the expected diagnostic: {needle!r}")
            return 1
    # Exit 0 is not evidence that the script ran: the session record must show it (viewer_session.py).
    found = viewer_session.problems(script.read_text(encoding="utf-8"), log_text)
    session_txt = out / "session.txt"
    if not found and not (session_txt.is_file() and session_txt.stat().st_size > 0):
        found = ["no session.txt in the session directory: the window did not write its session record"]
    if found:
        print(f"viewer_wiring: FAIL {case}: window exit 0 but the session record does not show the script run:")
        for f in found:
            print("  - " + f)
        return 1
    print(f"viewer_wiring: PASS {case}: window exit 0 after {elapsed:.1f} s, every step of the script recorded in order; session {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
