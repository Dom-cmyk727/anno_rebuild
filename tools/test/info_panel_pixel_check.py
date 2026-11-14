#!/usr/bin/env python3
"""info_panel_pixel_check.py: UI-INFO-1, the construction menu's building info panel (name,
preview, four material costs, gold cost, upkeep) drawn by the real anno1503 window, checked pixel
by pixel against the Python reference (tools/ui_layout.py compose_info_panel; docs/re/ui.md
section 2.5) for four armed cells: forester 801, market 1301, Kontor 1302, dirt road 2520.

    python -I tools/test/info_panel_pixel_check.py <anno1503 executable> [--renderer cpu|gpu]

The window draws its figures from the live type registry (ConstructionMenu::info_panel_content);
the reference takes them from data/ui/construction.txt's recorded costs (the RE snapshot of the
original's panel) and the name from Texte.dat - two independent paths that must agree. The whole
panel rectangle (804, 302) 220 x 466 is compared (INFOUNTERGRUND is opaque, so every pixel there
is the panel's): `--renderer cpu` (the CTest case) exactly; `--renderer gpu` (manual, needs a real
window) within the F2.1 tolerance (48 per channel, at most 8% of the pixels).

Wrong-input control: each capture is also compared with the reference of a different type
(805, the hunter: other name, preview and figures) and must be rejected.

ANNO_INFO_PANEL_KEEP=<dir> keeps the captures (for a look by eye; never into git).

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

SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "info_panel_draw_check.script"
TYPES = (801, 1301, 1302, 2520)
WRONG_TYPE = 805


def compare(expected, live, rect, tolerance):
    x0, y0, w, h = rect
    e, l = expected.load(), live.load()
    bad = 0
    first = None
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            a, b = e[x, y], l[x, y]
            if max(abs(a[c] - b[c]) for c in range(3)) > tolerance:
                bad += 1
                if first is None:
                    first = (x, y, a, b)
    return bad, w * h, first


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

    layout_dir = str(HERE / "data" / "ui")
    rect = ui_layout.INFO_PANEL_RECT
    tolerance = 48 if renderer == "gpu" else 0
    failures = []
    with tempfile.TemporaryDirectory(prefix="anno_info_panel_") as session:
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
            print(f"anno1503 exited {proc.returncode}\nstderr:\n{proc.stderr}")
            return 1
        for type_id in TYPES:
            png = session_dir / f"info_{type_id}_live.png"
            if not png.is_file():
                failures.append(f"{type_id}: {png} was not captured")
                continue
            live = Image.open(png).convert("RGB")
            if os.environ.get("ANNO_INFO_PANEL_KEEP"):
                keep = Path(os.environ["ANNO_INFO_PANEL_KEEP"])
                keep.mkdir(parents=True, exist_ok=True)
                live.save(keep / f"info_{type_id}_{renderer}_live.png")
            expected = ui_layout.compose_info_panel(layout_dir, game_dir, type_id, base=live)
            wrong = ui_layout.compose_info_panel(layout_dir, game_dir, WRONG_TYPE, base=live)
            bad_wrong, _, _ = compare(wrong, live, rect, tolerance)
            if bad_wrong == 0:
                failures.append(f"{type_id}: the wrong-input control ({WRONG_TYPE}) was not "
                                "rejected")
            bad, checked, first = compare(expected, live, rect, tolerance)
            limit = 0 if renderer == "cpu" else int(checked * 0.08)
            if bad > limit:
                x, y, a, b = first
                failures.append(f"{type_id}: {bad} of {checked} panel pixels differ; first at "
                                f"({x},{y}): expected={a} live={b}")
            else:
                print(f"{type_id}: {checked - bad} of {checked} panel pixels match "
                      f"({bad} outside tolerance {tolerance}); wrong-input control rejected "
                      f"({bad_wrong} differ)")
    if failures:
        print("FAILURES:")
        for f in failures:
            print(f"  {f}")
        return 1
    print("info panel matches the reference for every checked type")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
