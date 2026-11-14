#!/usr/bin/env python3
"""info_panel_rotation_check.py: UI-INFO-2, the info panel's preview at the four rotations, drawn
by the real anno1503 window after clicks on the rotate arrows, checked pixel by pixel against the
Python reference (tools/ui_layout.py compose_info_panel with `rotation`; docs/re/ui.md section
2.5) for the dirt road 2520 (an unscaled preview: its rotation blocks 84/82 x 41/40 fit the 135 x
135 box, so OQ-612's placeholder scaler is not involved).

    python -I tools/test/info_panel_rotation_check.py <anno1503 executable> [--renderer cpu|gpu]

The script (viewer_wiring/info_panel_rotation.script) arms 2520 and captures the panel at
rotation 0, after one, two and three clicks on rotate_right (rotations 1, 2, 3) and after one
click on rotate_left from there (2); it also expects the window's `ui.info.rotation` fact at each
step. The reference of the same rotation must match the whole panel rectangle exactly. The
reference itself equals the original GOG game's own shots at rotations 0..3
(info_panel_rotation_gog_compare.py).

Wrong-input control: each capture is also compared with the reference of the next rotation
((r + 1) mod 4), which must differ in more pixels than the limit (0) - so a window that ignores
the rotation (UI-INFO-1's preview, always rotation 0) fails on r1, r2, r3 and r2b.

`--renderer gpu` (manual, needs a real window) leaves out GPU_CORNER, as info_panel_pixel_check.py
does (its docstring has the measurement). ANNO_INFO_PANEL_KEEP=<dir> keeps the captures (never
into git).

Needs the original game files, Pillow and the Windows-only screenshot step; skips (77) otherwise.
Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HERE / "tools"))
sys.path.insert(0, str(HERE / "tools" / "test"))

SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "info_panel_rotation.script"
TYPE = 2520
CAPTURES = {"rot_2520_r0": 0, "rot_2520_r1": 1, "rot_2520_r2": 2, "rot_2520_r3": 3,
            "rot_2520_r2b": 2}
GPU_CORNER = (1017, 761, 1023, 767)


def main(argv):
    renderer = "cpu"
    pos = []
    it = iter(argv)
    for a in it:
        if a == "--renderer":
            renderer = next(it, "")
        else:
            pos.append(a)
    if len(pos) != 1 or renderer not in ("cpu", "gpu"):
        print(__doc__)
        return 2
    (exe,) = pos
    if os.name != "nt":
        print("skipping: the screenshot step is Windows-only (viewer.cpp)")
        return 77
    game_dir = os.environ.get("ANNO1503_GAME_DIR")
    if not game_dir:
        print("skipping: ANNO1503_GAME_DIR is not set (needs the original game files)")
        return 77
    if not Path(game_dir, "Anlagen.dat").is_file():
        print(f"skipping: {game_dir} is not the original game folder (no Anlagen.dat)")
        return 77
    try:
        from PIL import Image
    except ImportError:
        print("skipping: Pillow (PIL) is not installed")
        return 77
    import ui_layout
    from info_panel_pixel_check import compare

    layout_dir = str(HERE / "data" / "ui")
    rect = ui_layout.INFO_PANEL_RECT
    skip = GPU_CORNER if renderer == "gpu" else None
    limit = 0
    failures = []
    with tempfile.TemporaryDirectory(prefix="anno_info_rotation_") as session:
        session_dir = Path(session) / "session"
        env = dict(os.environ, ANNO_DEV_SCRIPT=str(SCRIPT), ANNO_DEV_SESSION=str(session_dir),
                   ANNO_VIEW_QUIT_S="240")
        if renderer == "cpu":
            env["SDL_VIDEODRIVER"] = "dummy"
        else:
            env.pop("SDL_VIDEODRIVER", None)
        proc = subprocess.run(
            [exe, "--play", str(HERE / "data" / "maps" / "lan5.txt"), "--game-dir", game_dir,
             "--construction-menu", "on", "--layout", layout_dir, "--renderer", renderer],
            env=env, capture_output=True, text=True, timeout=260, check=False)
        if proc.returncode != 0:
            print(f"anno1503 exited {proc.returncode} (a failed `expect fact ui.info.rotation` "
                  f"ends the script)\nstderr:\n{proc.stderr}")
            return 1
        for name, rotation in CAPTURES.items():
            png = session_dir / f"{name}.png"
            if not png.is_file():
                failures.append(f"{name}: {png} was not captured")
                continue
            live = Image.open(png).convert("RGB")
            if os.environ.get("ANNO_INFO_PANEL_KEEP"):
                keep = Path(os.environ["ANNO_INFO_PANEL_KEEP"])
                keep.mkdir(parents=True, exist_ok=True)
                live.save(keep / f"{name}_{renderer}_live.png")
            expected = ui_layout.compose_info_panel(layout_dir, game_dir, TYPE, base=live,
                                                    rotation=rotation)
            wrong = ui_layout.compose_info_panel(layout_dir, game_dir, TYPE, base=live,
                                                 rotation=(rotation + 1) % 4)
            bad_wrong, _, _ = compare(wrong, live, rect, skip)
            if bad_wrong <= limit:
                failures.append(f"{name}: the wrong-rotation control ({(rotation + 1) % 4}) was "
                                f"not rejected ({bad_wrong} differing px, limit {limit})")
            bad, checked, first = compare(expected, live, rect, skip)
            if bad > limit:
                x, y, a, b = first
                failures.append(f"{name}: {bad} of {checked} panel pixels differ from the "
                                f"rotation-{rotation} reference; first at ({x},{y}): "
                                f"expected={a} live={b}")
            else:
                print(f"{name}: {checked} of {checked} checked panel pixels match the rotation-"
                      f"{rotation} reference exactly (renderer {renderer}); wrong-rotation "
                      f"control rejected ({bad_wrong} differ, limit {limit})")
    if failures:
        print("FAILURES:")
        for f in failures:
            print(f"  {f}")
        return 1
    print("info panel preview matches the reference at every captured rotation")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
