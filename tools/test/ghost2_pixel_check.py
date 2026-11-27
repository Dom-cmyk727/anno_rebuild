#!/usr/bin/env python3
"""ghost2_pixel_check.py: UI-GHOST-2 pixel proof through the real anno1503 window.

    python -I tools/test/ghost2_pixel_check.py <anno1503 executable> [--renderer cpu|gpu] [--save-dir <dir>]

Runs both UI-GHOST-2 scenarios on lan5:
  1. ui_ghost2 (default --occupied-outline off, ANNO_DEV_GOODS_TONS=6):
     - forest_baseline vs forest_valid: valid ghost over (1188, 516) hides the forest pixels.
     - forest_moved_away / forest_cancelled: moving away and right-click cancel restore (1188, 516)
       byte-for-byte in the footprint ROI.
     - forest_unaff_phase0 vs forest_unaff_phase1: unaffordable phase 0 hides objects under the
       footprint, phase 1 restores them byte-for-byte outside yellow outline pixels.
     - ghost_refused & occupied_default_off: yellow 0xffff00 refusal box present, 0 0xff4040 red
       occupied-outline pixels when ADAPTATION is default OFF.
     - forest_placed: placing pioneer house 501 over (1188, 516) leaves the footprint permanently
       changed/cleared.
  2. ui_ghost2_occupied (--occupied-outline on, ANNO_DEV_GOODS_TONS=3):
     - occupied_on_kontor: Reject::Occupied draws both 0xffff00 cursor refusal box and 0xff4040
       4x4 Kontor blocking outline, with chrome staying untouched.
     - occupied_on_hidden: 0 yellow and 0 red map pixels.
     - occupied_on_terrain_refused: yellow refusal box only, 0 red pixels.

Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
SCRIPT_MAIN = HERE / "tools" / "test" / "viewer_wiring" / "ui_ghost2.script"
SCRIPT_OCC = HERE / "tools" / "test" / "viewer_wiring" / "ui_ghost2_occupied.script"

YELLOW = (255, 255, 0)
OCCUPIED_RED = (255, 64, 64)
MAP_X1 = 745
STATUS_Y = 733
PANEL_X = 779
# Centred 2x2 / 3x3 ROI around look (1188, 516) -> window centre (512, 384).
TREE_ROI = (470, 340, 554, 428)
MIN_HIDE_PX = 40
MIN_OCC_RED_PX = 80


def find_color(img, rgb, x0, y0, x1, y1):
    px = img.load()
    return [(x, y) for y in range(y0, y1) for x in range(x0, x1) if px[x, y][:3] == rgb]


def diff_pts(a, b, rect, ignore_colors=()):
    pa, pb = a.load(), b.load()
    x0, y0, x1, y1 = rect
    out = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            ca, cb = pa[x, y][:3], pb[x, y][:3]
            if ca == cb:
                continue
            if ca in ignore_colors or cb in ignore_colors:
                continue
            out.append((x, y))
    return out


def box(points):
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    return min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1


def run_script(exe, game_dir, script, renderer, goods_tons, extra_args, save_dir, names, Image):
    with tempfile.TemporaryDirectory(prefix="anno_ghost2_pixel_") as session:
        session_dir = Path(session) / "session"
        env = dict(os.environ, ANNO_DEV_SCRIPT=str(script), ANNO_DEV_SESSION=str(session_dir),
                   ANNO_VIEW_QUIT_S="240", ANNO_DEV_GOODS_TONS=str(goods_tons))
        if renderer == "cpu":
            env["SDL_VIDEODRIVER"] = "dummy"
        else:
            env.pop("SDL_VIDEODRIVER", None)
        cmd = [
            exe, "--play", str(HERE / "data" / "maps" / "lan5.txt"),
            "--game-dir", game_dir,
            "--construction-menu", "on", "--layout", str(HERE / "data" / "ui"),
            "--renderer", renderer, *extra_args,
        ]
        proc = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=260, check=False)
        log = session_dir / "script.log"
        if proc.returncode != 0:
            print(f"anno1503 exited {proc.returncode} for {script.name}\nstderr:\n{proc.stderr}")
            if log.is_file():
                print(log.read_text(encoding="utf-8", errors="replace"))
            return None
        shots = {}
        for name in names:
            png = session_dir / f"{name}.png"
            if not png.is_file():
                print(f"FAIL: {png} was not captured")
                return None
            if save_dir is not None:
                save_dir.mkdir(parents=True, exist_ok=True)
                shutil.copy2(png, save_dir / f"{name}.png")
            img = Image.open(png).convert("RGB")
            if img.size != (1024, 768):
                print(f"FAIL: {name} size {img.size} != (1024, 768)")
                return None
            shots[name] = img
        return shots


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

    main_names = (
        "forest_baseline", "forest_valid", "forest_moved_away", "forest_cancelled",
        "ghost_refused", "occupied_default_off", "forest_unaff_phase0",
        "forest_unaff_phase1", "forest_placed",
    )
    shots = run_script(exe, game_dir, SCRIPT_MAIN, renderer, 6, [], save_dir, main_names, Image)
    if shots is None:
        return 1

    failures = []
    for n, img in shots.items():
        rpts = find_color(img, OCCUPIED_RED, 0, 0, MAP_X1, STATUS_Y)
        if rpts:
            failures.append(f"{n}: unexpected 0xff4040 pixels ({len(rpts)}) when ADAPTATION is default OFF")

    for n in ("forest_baseline", "forest_cancelled", "forest_placed"):
        ypts = find_color(shots[n], YELLOW, 0, 0, MAP_X1, STATUS_Y)
        if ypts:
            failures.append(f"{n}: unexpected 0xffff00 outline pixels ({len(ypts)})")

    for n in ("ghost_refused", "occupied_default_off"):
        ypts = find_color(shots[n], YELLOW, 0, 0, MAP_X1, STATUS_Y)
        if not ypts:
            failures.append(f"{n}: missing 0xffff00 refusal rectangle pixels")
        else:
            print(f"{n}: yellow box={box(ypts)} ({len(ypts)} px)")

    d_valid = diff_pts(shots["forest_valid"], shots["forest_baseline"], TREE_ROI, (YELLOW,))
    d_moved = diff_pts(shots["forest_moved_away"], shots["forest_baseline"], TREE_ROI)
    d_cancel = diff_pts(shots["forest_cancelled"], shots["forest_baseline"], TREE_ROI)
    d_p0 = diff_pts(shots["forest_unaff_phase0"], shots["forest_baseline"], TREE_ROI, (YELLOW,))
    d_p1 = diff_pts(shots["forest_unaff_phase1"], shots["forest_baseline"], TREE_ROI, (YELLOW,))
    d_placed = diff_pts(shots["forest_placed"], shots["forest_baseline"], TREE_ROI)
    print(f"tree_roi diffs vs baseline: valid={len(d_valid)} moved={len(d_moved)} "
          f"cancel={len(d_cancel)} unaff_p0={len(d_p0)} unaff_p1={len(d_p1)} placed={len(d_placed)}")

    if len(d_valid) < MIN_HIDE_PX:
        failures.append(f"forest_valid: ghost did not change/hide enough tree ROI px ({len(d_valid)})")
    if d_moved:
        failures.append(f"forest_moved_away: tree ROI did not restore identically ({len(d_moved)} px diff)")
    if d_cancel:
        failures.append(f"forest_cancelled: tree ROI did not restore identically ({len(d_cancel)} px diff)")
    if len(d_p0) < MIN_HIDE_PX:
        failures.append(f"forest_unaff_phase0: phase 0 did not hide tree ROI ({len(d_p0)} px diff)")
    if d_p1:
        failures.append(f"forest_unaff_phase1: phase 1 did not restore tree ROI ({len(d_p1)} px diff)")
    if len(d_placed) < MIN_HIDE_PX:
        failures.append(f"forest_placed: placed building did not alter/clear tree ROI ({len(d_placed)} px diff)")

    occ_names = ("occupied_on_kontor", "occupied_on_hidden", "occupied_on_terrain_refused")
    occ_shots = run_script(exe, game_dir, SCRIPT_OCC, renderer, 3,
                           ["--occupied-outline", "on"], save_dir, occ_names, Image)
    if occ_shots is None:
        return 1

    ok_y = find_color(occ_shots["occupied_on_kontor"], YELLOW, 0, 0, MAP_X1, STATUS_Y)
    ok_r = find_color(occ_shots["occupied_on_kontor"], OCCUPIED_RED, 0, 0, MAP_X1, STATUS_Y)
    if not ok_y:
        failures.append("occupied_on_kontor: missing 0xffff00 cursor refusal rectangle")
    if len(ok_r) < MIN_OCC_RED_PX:
        failures.append(f"occupied_on_kontor: too few 0xff4040 blocking outline px ({len(ok_r)})")
    else:
        print(f"occupied_on_kontor: yellow box={box(ok_y)} ({len(ok_y)} px), "
              f"red box={box(ok_r)} ({len(ok_r)} px)")

    if find_color(occ_shots["occupied_on_hidden"], YELLOW, 0, 0, MAP_X1, STATUS_Y):
        failures.append("occupied_on_hidden: unexpected 0xffff00 map pixel")
    if find_color(occ_shots["occupied_on_hidden"], OCCUPIED_RED, 0, 0, MAP_X1, STATUS_Y):
        failures.append("occupied_on_hidden: unexpected 0xff4040 map pixel")
    if not find_color(occ_shots["occupied_on_terrain_refused"], YELLOW, 0, 0, MAP_X1, STATUS_Y):
        failures.append("occupied_on_terrain_refused: missing 0xffff00 cursor refusal rectangle")
    if find_color(occ_shots["occupied_on_terrain_refused"], OCCUPIED_RED, 0, 0, MAP_X1, STATUS_Y):
        failures.append("occupied_on_terrain_refused: unexpected 0xff4040 pixel on TerrainRefused")

    panel_y = {len(find_color(img, YELLOW, PANEL_X, 0, 1024, 768)) for img in occ_shots.values()}
    panel_r = {len(find_color(img, OCCUPIED_RED, PANEL_X, 0, 1024, 768)) for img in occ_shots.values()}
    if len(panel_y) != 1 or len(panel_r) != 1:
        failures.append(f"chrome pixels altered under outlines: panel_y={panel_y}, panel_r={panel_r}")

    if failures:
        print("FAILURES:\n  " + "\n  ".join(failures))
        return 1
    print(f"ghost2_pixel_check ({renderer}): pass")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
