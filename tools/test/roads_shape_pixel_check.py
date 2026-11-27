#!/usr/bin/env python3
"""roads_shape_pixel_check.py: ROADS-2b ghost-vs-commit pixel proof through the real anno1503 window.

    python -I tools/test/roads_shape_pixel_check.py <anno1503 executable> [--renderer cpu|gpu] [--save-dir <dir>]

Drives tools/test/viewer_wiring/ui_roads_shape.script on lan5 (default start view) and compares
three 1024 x 768 screenshots in the map area:
  - shape_hidden: armed, pointer on the panel, no ghost (before any road);
  - shape_l_ghost: the shaped L's ghost (8 tiles: end caps, straights, the corner) before release;
  - shape_l_commit: the committed L, pointer back on the panel.
The road area is where both the ghost and the commit differ from shape_hidden. Inside its box the
ghost and the commit must agree: a wrong piece or rotation in the preview (or the commit) differs
by a whole tile sprite, so diff(ghost, commit) must stay under MAX_MISMATCH of diff(ghost, hidden).

Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "ui_roads_shape.script"
MAP = (0, 0, 745, 733)  # left of the panel, above the status bar (as roads_ghost_pixel_check.py)
MIN_ROAD_PX = 400  # 8 road tiles at the default zoom
MAX_MISMATCH = 0.10
NAMES = ("shape_hidden", "shape_l_ghost", "shape_l_commit")


def diff_set(a, b, rect):
    pa, pb = a.load(), b.load()
    x0, y0, x1, y1 = rect
    return {(x, y) for y in range(y0, y1) for x in range(x0, x1) if pa[x, y][:3] != pb[x, y][:3]}


def box(points):
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def main(argv):
    renderer = "cpu"
    save_dir = None
    pos = []
    it = iter(argv)
    for a in it:
        if a == "--renderer":
            renderer = next(it, "")
        elif a == "--save-dir":
            save_dir = Path(next(it, ""))
        else:
            pos.append(a)
    if len(pos) != 1 or renderer not in ("cpu", "gpu"):
        print(__doc__)
        return 2
    exe_path = Path(pos[0]).resolve()
    if not exe_path.is_file():
        print(f"FAIL: executable not found: {pos[0]}")
        return 1
    if os.name != "nt":
        print("skipping: the screenshot step is Windows-only (viewer.cpp)")
        return 77
    game_dir = os.environ.get("ANNO1503_GAME_DIR")
    if not game_dir:
        print("skipping: ANNO1503_GAME_DIR is not set (needs the original game files)")
        return 77
    if not Path(game_dir, "Anlagen.dat").is_file():
        print(f"skipping: Anlagen.dat is not in this game directory ({game_dir})")
        return 77
    try:
        from PIL import Image
    except ImportError:
        print("skipping: Pillow (PIL) is not installed")
        return 77

    with tempfile.TemporaryDirectory(prefix="anno_roads_shape_") as session:
        session_dir = Path(session) / "session"
        env = dict(os.environ, ANNO_DEV_SCRIPT=str(SCRIPT), ANNO_DEV_SESSION=str(session_dir),
                   ANNO_VIEW_QUIT_S="240")
        env.pop("ANNO_DEV_GOLD", None)
        env.pop("ANNO_DEV_GOODS_TONS", None)
        if renderer == "cpu":
            env["SDL_VIDEODRIVER"] = "dummy"
        else:
            env.pop("SDL_VIDEODRIVER", None)
        proc = subprocess.run(
            [str(exe_path), "--play", str(HERE / "data" / "maps" / "lan5.txt"), "--game-dir",
             game_dir, "--construction-menu", "on", "--layout", str(HERE / "data" / "ui"),
             "--renderer", renderer],
            env=env, capture_output=True, text=True, timeout=260, check=False)
        log = session_dir / "script.log"
        if proc.returncode != 0:
            print(f"anno1503 exited {proc.returncode}\nstderr:\n{proc.stderr}")
            if log.is_file():
                print(log.read_text(encoding="utf-8", errors="replace"))
            return 1
        if save_dir is not None:
            save_dir.mkdir(parents=True, exist_ok=True)
            for n in NAMES:
                if (session_dir / f"{n}.png").is_file():
                    shutil.copy2(session_dir / f"{n}.png", save_dir / f"{n}.png")
        shots = {}
        for n in NAMES:
            png = session_dir / f"{n}.png"
            if not png.is_file():
                print(f"FAIL: {png} was not captured")
                return 1
            shots[n] = Image.open(png).convert("RGB")
        if {img.size for img in shots.values()} != {(1024, 768)}:
            print(f"FAIL: shots not 1024x768: {sorted({img.size for img in shots.values()})}")
            return 1

        ghost = diff_set(shots["shape_l_ghost"], shots["shape_hidden"], MAP)
        commit = diff_set(shots["shape_l_commit"], shots["shape_hidden"], MAP)
        road = ghost & commit
        if len(road) < MIN_ROAD_PX:
            print(f"FAIL: road area too small: ghost {len(ghost)}, commit {len(commit)}, "
                  f"both {len(road)} < {MIN_ROAD_PX}")
            return 1
        roi = box(road)
        in_roi = [p for p in ghost if roi[0] <= p[0] < roi[2] and roi[1] <= p[1] < roi[3]]
        mismatch = diff_set(shots["shape_l_ghost"], shots["shape_l_commit"], roi)
        ratio = len(mismatch) / max(len(in_roi), 1)
        print(f"road box {roi}: ghost-hidden {len(in_roi)} px, ghost-commit {len(mismatch)} px "
              f"({ratio:.3f}, max {MAX_MISMATCH})")
        if ratio > MAX_MISMATCH:
            print("FAIL: the ghost's pieces differ from the committed road")
            return 1
    print("roads_shape_pixel_check: pass")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
