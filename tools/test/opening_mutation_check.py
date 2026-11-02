#!/usr/bin/env python3
"""opening_mutation_check.py: do the opening's window scripts discriminate? (W4-OPENING-1 I5)

    python -I tools/test/opening_mutation_check.py <build dir (CMake, Visual Studio multi-config)> [<work dir>] [name ...]

Each mutant edits one statement of the opening's controls (src/app/src/opening_ui.cpp) the way a real wiring
defect would, rebuilds anno1503, runs the production window against tools/test/viewer_wiring/opening_controls.script
(SDL's dummy driver, CPU renderer) and requires the run to FAIL, at the assertion that names the defect. The
control (no edit) must pass first. The source file is restored afterwards (git restore of that one file); a dirty
file is never overwritten. It is a developer tool: a build and about ten seconds of window per mutant."""
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
TARGET = HERE / "src" / "app" / "src" / "opening_ui.cpp"
MANIFEST = HERE / "data" / "maps" / "candidates" / "ship-start-v2" / "lan5-ship-a.json"

MUTANTS = {
    "suppress-move": (
        "    out.commands.push_back(sim::Command{setup_.local, cmd});\n    Tracked t;\n    t.kind = TrackKind::Move;",
        "    Tracked t;\n    t.kind = TrackKind::Move;",
        "latest_kind is 'none', expected == move"),
    "retarget-move": (
        "    sim::MoveCmd cmd;\n    cmd.ship = ship->id;",
        "    sim::MoveCmd cmd;\n    cmd.ship = feed.view->ships.back().id;",
        "latest"),
    "premature-success": (
        "    if (const ShipEventView* e = feed.view->event_for(t.ship, *out.order_id)) {\n        switch (e->kind) {",
        "    const ShipEventView* any = nullptr;\n    for (const ShipEventView& x : feed.view->events) {\n        if (x.ship == t.ship) {\n            any = &x;\n        }\n    }\n    if (const ShipEventView* e = any) {\n        switch (e->kind) {",
        "latest is 'arrived', expected != arrived"),
    "pan-is-a-click": (
        "    const bool click = !g.cancelled && g.travel < kClickSlop;",
        "    const bool click = !g.cancelled;",
        "tracked is '1', expected == 0"),
    "other-owner-controllable": (
        "    if (ship->owner != setup_.local) {\n        why = \"ship #\"",
        "    if (false) {\n        why = \"ship #\"",
        "tracked is '1', expected == 0"),
}


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", **kw)


def build(build_dir):
    p = sh(["cmake", "--build", str(build_dir), "--config", "Release", "--target", "anno1503", "--parallel", "12"])
    return p.returncode == 0, p.stdout[-600:] + p.stderr[-600:]


def window(exe, work, name):
    out = work / name / "run"
    p = sh([sys.executable, "-I", str(HERE / "tools" / "test" / "viewer_wiring.py"), str(exe), "opening_controls", str(out),
            "--app-arg", "--ship-start", "--app-arg", str(MANIFEST), "--app-arg", "--seat", "--app-arg", "0",
            "--app-arg", "--speed", "--app-arg", "200"])
    return p.returncode, p.stdout


def main(argv):
    if not argv:
        print(__doc__, file=sys.stderr)
        return 2
    build_dir = Path(argv[0]).resolve()
    work = Path(argv[1]) if len(argv) > 1 and not argv[1] in MUTANTS else Path(tempfile.mkdtemp(prefix="opening_mutation_"))
    names = [a for a in argv[1:] if a in MUTANTS] or list(MUTANTS)
    exe = build_dir / "src" / "app" / "Release" / "anno1503.exe"
    if not os.environ.get("ANNO1503_GAME_DIR"):
        print("skipping: ANNO1503_GAME_DIR is not set")
        return 77
    status = sh(["git", "-C", str(HERE), "status", "--porcelain", "--", str(TARGET)]).stdout.strip()
    if status:
        print(f"refusing to edit a dirty {TARGET}: {status}")
        return 2
    original = TARGET.read_text(encoding="utf-8")
    crlf = "\r\n" in original
    text = original.replace("\r\n", "\n")
    results, failures = [], []
    try:
        ok, tail = build(build_dir)
        if not ok:
            print("control build failed:\n" + tail)
            return 1
        rc, out = window(exe, work, "control")
        results.append(("control", rc, "PASS opening_controls" in out))
        if rc != 0:
            print("the control does not pass; mutants mean nothing:\n" + out[-1500:])
            return 1
        for name in names:
            old, new, fragment = MUTANTS[name]
            if text.count(old) != 1:
                print(f"{name}: the pattern is not unique in {TARGET.name} ({text.count(old)})")
                failures.append(name)
                continue
            TARGET.write_text(text.replace(old, new).replace("\n", "\r\n" if crlf else "\n"), encoding="utf-8", newline="")
            ok, tail = build(build_dir)
            if not ok:
                print(f"{name}: the mutant does not build:\n{tail}")
                failures.append(name)
                continue
            rc, out = window(exe, work, name)
            fail_line = next((l for l in out.splitlines() if l.startswith("FAIL") or "FAIL line" in l), "")
            killed = rc == 1 and "PASS opening_controls" not in out and fragment in out
            print(f"{name:26} exit {rc} {'KILLED' if killed else 'SURVIVED or wrong failure'}: {fail_line[:200]}")
            if not killed:
                failures.append(name)
    finally:
        TARGET.write_bytes(original)
        build(build_dir)
    print(f"mutation check: {len(names) - len(failures)} of {len(names)} killed; work dir {work}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
