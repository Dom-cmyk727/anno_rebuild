#!/usr/bin/env python3
"""roads_ghost_pixel_check.py: ROADS-3 / UI-ROADS-1 pixel proof through the real anno1503 window.

    python -I tools/test/roads_ghost_pixel_check.py <anno1503 executable> [--renderer cpu|gpu] [--save-dir <dir>]

Drives tools/test/viewer_wiring/ui_roads.script with ANNO_DEV_GOLD=20 on lan5 and inspects four
1024 x 768 screenshots:
  - road_hidden: pointer on panel chrome -> 0 exact 0xffff00 outline pixels in map area.
  - road_refused: pointer over occupied pioneer house -> yellow 1x1 diamond (0xffff00) only.
  - road_drag_phase0: 7-tile live L-drag ghost in blink phase 0 -> 0 exact 0xffff00 pixels (no
    radius/refusal outline), all 7 road tiles visible vs road_hidden on dry land.
  - road_drag_phase1: same L-drag in blink phase 1 -> 0 exact 0xffff00 pixels, 4 affordable tiles
    stay drawn while the 3-tile unaffordable tail blinks off (diff(phase0, hidden) > diff(phase1, hidden)
    and diff(phase0, phase1) >= MIN_TAIL_PX in the land ROI).

Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "ui_roads.script"
YELLOW = (255, 255, 0)
MAP_X1 = 745
STATUS_Y = 733
PANEL_X = 779
# Tightened dry-land L-drag band (excludes animated southern coast/trees below y=515).
LAND_ROI = (350, 350, 520, 515)
MAX_REFUSED_W, MAX_REFUSED_H = 120, 60
MIN_PHASE0_PX = 150
MIN_PHASE0_W = 60
MIN_PHASE1_PX = 80
MIN_TAIL_PX = 40


def yellow(img, x0, y0, x1, y1):
    px = img.load()
    return [(x, y) for y in range(y0, y1) for x in range(x0, x1) if px[x, y][:3] == YELLOW]


def diff_pts(a, b, rect):
    pa, pb = a.load(), b.load()
    x0, y0, x1, y1 = rect
    return [(x, y) for y in range(y0, y1) for x in range(x0, x1) if pa[x, y][:3] != pb[x, y][:3]]


def box(points):
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    return min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1


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
    (exe,) = pos
    exe_path = Path(exe).resolve()
    if not exe_path.is_file():
        print(f"FAIL: executable not found: {exe}")
        return 1
    exe = str(exe_path)
    if os.name != "nt":
        print("skipping: the screenshot step is Windows-only (viewer.cpp)")
        return 77
    game_dir = os.environ.get("ANNO1503_GAME_DIR")
    if not game_dir or not Path(game_dir, "Anlagen.dat").is_file():
        print("skipping: ANNO1503_GAME_DIR is unset or lacks Anlagen.dat")
        return 77
    try:
        from PIL import Image
    except ImportError:
        print("skipping: Pillow (PIL) is not installed")
        return 77

    with tempfile.TemporaryDirectory(prefix="anno_roads_pixel_") as session:
        session_dir = Path(session) / "session"
        env = dict(os.environ, ANNO_DEV_SCRIPT=str(SCRIPT), ANNO_DEV_SESSION=str(session_dir),
                   ANNO_VIEW_QUIT_S="240", ANNO_DEV_GOLD="20")
        env.pop("ANNO_DEV_GOODS_TONS", None)
        if renderer == "cpu":
            env["SDL_VIDEODRIVER"] = "dummy"
        else:
            env.pop("SDL_VIDEODRIVER", None)
        proc = subprocess.run(
            [exe, "--play", str(HERE / "data" / "maps" / "lan5.txt"), "--game-dir", game_dir,
             "--construction-menu", "on", "--layout", str(HERE / "data" / "ui"),
             "--renderer", renderer],
            env=env, capture_output=True, text=True, timeout=260, check=False)
        log = session_dir / "script.log"
        if proc.returncode != 0:
            print(f"anno1503 exited {proc.returncode}\nstderr:\n{proc.stderr}")
            if log.is_file():
                print(log.read_text(encoding="utf-8", errors="replace"))
            return 1
        names = ("road_hidden", "road_refused", "road_drag_phase0", "road_drag_phase1")
        if save_dir is not None:
            save_dir.mkdir(parents=True, exist_ok=True)
            for n in names:
                src = session_dir / f"{n}.png"
                if src.is_file():
                    shutil.copy2(src, save_dir / f"{n}.png")
        shots = {}
        for name in names:
            png = session_dir / f"{name}.png"
            if not png.is_file():
                print(f"FAIL: {png} was not captured")
                return 1
            shots[name] = Image.open(png).convert("RGB")
        failures = []
        sizes = {img.size for img in shots.values()}
        if sizes != {(1024, 768)}:
            print(f"FAIL: shots not 1024x768: {sorted(sizes)}")
            return 1

        ymaps = {n: yellow(img, 0, 0, MAP_X1, STATUS_Y) for n, img in shots.items()}
        ypanels = {n: len(yellow(img, PANEL_X, 0, 1024, 768)) for n, img in shots.items()}
        for name, pts in ymaps.items():
            print(f"{name}: {len(pts)} exact 0xffff00 px in map"
                  + (f", box {box(pts)}" if pts else "") + f"; {ypanels[name]} in panel")
        for n in ("road_hidden", "road_drag_phase0", "road_drag_phase1"):
            if ymaps[n]:
                failures.append(f"{n}: unexpected 0xffff00 outline pixels ({len(ymaps[n])})")
        ref = ymaps["road_refused"]
        if not ref:
            failures.append("road_refused: no 0xffff00 outline pixel drawn")
        else:
            _, _, rw, rh = box(ref)
            if rw > MAX_REFUSED_W or rh > MAX_REFUSED_H:
                failures.append(f"road_refused: box {rw}x{rh} exceeds 1x1 cap {MAX_REFUSED_W}x{MAX_REFUSED_H}")
            if not 1.4 <= rw / max(rh, 1) <= 2.6:
                failures.append(f"road_refused: box {rw}x{rh} is not a 2:1 iso diamond")
        if len(set(ypanels.values())) != 1:
            failures.append(f"panel yellow pixels differ across shots {ypanels}")

        d0 = diff_pts(shots["road_drag_phase0"], shots["road_hidden"], LAND_ROI)
        d1 = diff_pts(shots["road_drag_phase1"], shots["road_hidden"], LAND_ROI)
        dtail = diff_pts(shots["road_drag_phase0"], shots["road_drag_phase1"], LAND_ROI)
        b0 = box(d0) if d0 else (0, 0, 0, 0)
        b1 = box(d1) if d1 else (0, 0, 0, 0)
        btail = box(dtail) if dtail else (0, 0, 0, 0)
        print(f"land_roi diff phase0-hidden={len(d0)} box={b0}; phase1-hidden={len(d1)} box={b1}; "
              f"phase0-phase1={len(dtail)} box={btail}")
        if len(d0) < MIN_PHASE0_PX or b0[2] < MIN_PHASE0_W:
            failures.append(f"road_drag_phase0: too few/narrow changed pixels vs hidden ({len(d0)}, w={b0[2]})")
        if len(d1) < MIN_PHASE1_PX:
            failures.append(f"road_drag_phase1: 4 affordable prefix tiles missing ({len(d1)} px)")
        if len(dtail) < MIN_TAIL_PX or len(d0) <= len(d1):
            failures.append(f"blink tail missing/static: diff(p0,p1)={len(dtail)}, "
                            f"diff(p0,h)={len(d0)}, diff(p1,h)={len(d1)}")

    if failures:
        print("FAILURES:\n  " + "\n  ".join(failures))
        return 1
    print("roads_ghost_pixel_check: pass")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
