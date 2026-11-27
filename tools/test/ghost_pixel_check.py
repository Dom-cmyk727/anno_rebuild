#!/usr/bin/env python3
"""ghost_pixel_check.py: UI-GHOST-1, the placement ghost's outline colour through the real anno1503
window (docs/BUILDINGSOBJECTS.md part B section 5.1).

    python -I tools/test/ghost_pixel_check.py <anno1503 executable> [--renderer cpu|gpu]

Drives tools/test/viewer_wiring/ui_ghost.script directly (ANNO_DEV_SCRIPT / ANNO_DEV_SESSION, the
selected_house_pixel_check.py pattern) with ANNO_DEV_GOODS_TONS=3 on lan5, and reads its three
screenshots of the 1024 x 768 window:
  - ghost_valid: the armed pioneer house 501 (action radius 24) over free own land. The yellow
    radius outline: exact 0xffff00 pixels in the map area (x < 745, y < 733), at least MIN_VALID
    of them, spread over a box wider than the footprint's;
  - ghost_refused: the same type over the inlet. The yellow footprint outline only: exact 0xffff00
    pixels, a 2:1 diamond box no larger than MAX_REFUSED_W x MAX_REFUSED_H;
  - ghost_hidden: the pointer on the panel's chrome. No exact 0xffff00 pixel in the map area.
The hidden shot is the negative control: the same view with no outline has no such pixel, so the
counts above are the outline's and not the terrain's. The panel column (x >= 779) holds the
same exact-yellow count in all three shots: the outline is drawn under the chrome, never over it.
The colour is exact (0xffff00 is AnnoFrame 0x10048471 / 0x100484f2's immediate); a wrong colour
(any other value) leaves zero exact pixels and fails the first two checks.

`--renderer cpu` (default, the CTest-registered mode) uses SDL's dummy video driver; `--renderer gpu`
needs a real window surface (manual). Needs the original game files, Pillow and the Windows-only
screenshot step (viewer.cpp); skips (exit 77) otherwise.
Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "ui_ghost.script"
YELLOW = (255, 255, 0)
# The 1024 x 768 layout (data/ui/hud.txt): the map area is left of the minimap's curl (x 745) and
# above the status bar (y 733); the panel column starts at x 779. The strip between is map below
# the curl and is not counted either way.
MAP_X1 = 745
STATUS_Y = 733
PANEL_X = 779
MIN_VALID = 200
MAX_REFUSED_W, MAX_REFUSED_H = 400, 200


def yellow(img, x0, y0, x1, y1):
    """[(x, y)] of the exact 0xffff00 pixels in [x0, x1) x [y0, y1)."""
    px = img.load()
    return [(x, y) for y in range(y0, y1) for x in range(x0, x1) if px[x, y][:3] == YELLOW]


def box(points):
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    return min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1


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

    with tempfile.TemporaryDirectory(prefix="anno_ghost_pixel_") as session:
        session_dir = Path(session) / "session"
        env = dict(os.environ, ANNO_DEV_SCRIPT=str(SCRIPT), ANNO_DEV_SESSION=str(session_dir),
                   ANNO_VIEW_QUIT_S="240", ANNO_DEV_GOODS_TONS="3")
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
        shots = {}
        for name in ("ghost_valid", "ghost_refused", "ghost_hidden"):
            png = session_dir / f"{name}.png"
            if not png.is_file():
                print(f"FAIL: {png} was not captured")
                return 1
            shots[name] = Image.open(png).convert("RGB")
        failures = []
        sizes = {img.size for img in shots.values()}
        if sizes != {(1024, 768)}:
            failures.append(f"the shots are not 1024 x 768: {sorted(sizes)}")
            print("FAILURES:\n  " + "\n  ".join(failures))
            return 1
        maps = {n: yellow(img, 0, 0, MAP_X1, STATUS_Y) for n, img in shots.items()}
        panel = {n: len(yellow(img, PANEL_X, 0, 1024, 768)) for n, img in shots.items()}
        for name, pts in maps.items():
            print(f"{name}: {len(pts)} exact 0xffff00 pixels in the map area"
                  + (f", box {box(pts)}" if pts else "") + f"; {panel[name]} in the panel area")
        if maps["ghost_hidden"]:
            failures.append(f"ghost_hidden: {len(maps['ghost_hidden'])} outline pixels with the "
                            "pointer on the chrome (expected none)")
        valid = maps["ghost_valid"]
        refused = maps["ghost_refused"]
        if len(valid) < MIN_VALID:
            failures.append(f"ghost_valid: {len(valid)} outline pixels (expected >= {MIN_VALID})")
        if not refused:
            failures.append("ghost_refused: no outline pixel")
        if valid and refused:
            _, _, vw, vh = box(valid)
            _, _, rw, rh = box(refused)
            if rw > MAX_REFUSED_W or rh > MAX_REFUSED_H:
                failures.append(f"ghost_refused: the outline's box {rw} x {rh} is larger than a "
                                f"footprint's ({MAX_REFUSED_W} x {MAX_REFUSED_H})")
            if not 1.5 <= rw / max(rh, 1) <= 2.6:
                failures.append(f"ghost_refused: the box {rw} x {rh} is not an iso diamond (2:1)")
            if vw <= rw:
                failures.append(f"ghost_valid: the radius outline's box ({vw} wide) is not wider "
                                f"than the footprint outline's ({rw})")
        if len(set(panel.values())) != 1:
            failures.append(f"the panel's exact-yellow pixels differ between the shots {panel}: "
                            "the outline was drawn over the chrome")
    if failures:
        print("FAILURES:\n  " + "\n  ".join(failures))
        return 1
    print("ghost_pixel_check: pass")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
