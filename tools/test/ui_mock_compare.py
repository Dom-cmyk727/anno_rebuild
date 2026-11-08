#!/usr/bin/env python3
"""ui_mock_compare.py: the shipped CPU reference composition (anno_ui_mock) checked pixel by
pixel against the UI.1 Python oracle's own mock (tools/ui_layout.py compose_screen), for every one
of the nine named screens (ADR 0004 "Rendering"/"Testing"; W4-UI-2 H4/C4).

    python -I tools/test/ui_mock_compare.py <anno_ui_mock executable>

Both composers read the same data/ui/ layout and the same original .tex files, so this is an
exact-match check (0 differing pixels), stronger than the fuzzy sprite-sad tolerance used for the
original's own screenshots (those carry live content - digits, portraits, water - this comparison
has none). A mismatch prints the screen name, the pixel count that differs, and the first
differing pixel's coordinates and RGB values on each side.

Needs the original game files (ANNO1503_GAME_DIR) and Pillow (PIL); skips (exit 77) without
either.
Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HERE / "tools"))

SCREENS = [
    "hud_idle",
    "construction_public",
    "construction_roads",
    "construction_farms",
    "construction_workshops",
    "construction_coastal",
    "construction_mines",
    "construction_military",
    "construction_ornamental",
]


def main(argv):
    if len(argv) != 1:
        print(__doc__)
        return 2
    (exe,) = argv
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
    import ui_layout  # tools/ui_layout.py

    layout_dir = HERE / "data" / "ui"
    failures = []
    with tempfile.TemporaryDirectory(prefix="anno_ui_mock_") as tmp:
        for screen in SCREENS:
            oracle = ui_layout.compose_screen(str(layout_dir), screen, game_dir)
            ours_path = Path(tmp) / f"{screen}.png"
            proc = subprocess.run(
                [exe, "--layout", str(layout_dir), "--screen", screen, "--game-dir", game_dir,
                 "--out", str(ours_path)],
                capture_output=True, text=True, check=False)
            if proc.returncode != 0:
                failures.append(f"{screen}: anno_ui_mock exited {proc.returncode}: "
                               f"{proc.stderr.strip()}")
                continue
            ours = Image.open(ours_path).convert("RGB")
            if ours.size != oracle.size:
                failures.append(f"{screen}: size {ours.size} != oracle {oracle.size}")
                continue
            ow, oh = oracle.size
            o_px = oracle.load()
            u_px = ours.load()
            bad = 0
            first = None
            for y in range(oh):
                for x in range(ow):
                    a, b = o_px[x, y], u_px[x, y]
                    if a != b:
                        bad += 1
                        if first is None:
                            first = (x, y, a, b)
            if bad:
                x, y, a, b = first
                failures.append(
                    f"{screen}: {bad} of {ow * oh} pixels differ; first at ({x},{y}): "
                    f"oracle={a} ours={b}")
            else:
                print(f"{screen}: exact match ({ow * oh} pixels)")
    if failures:
        print("FAILURES:")
        for f in failures:
            print(f"  {f}")
        return 1
    print(f"all {len(SCREENS)} screens: exact pixel match with the oracle")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
