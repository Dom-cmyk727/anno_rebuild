#!/usr/bin/env python3
"""ui_window_compare.py: the UI layer at a 2542 x 1351 window (UI-WINDOW-1) against the History
Edition's own captures at UI scale 1.0, 1.5 and 2.0 (docs/re/he-ui-scale.md, visual check).

    python -I tools/test/ui_window_compare.py <anno_ui_mock executable>

For each scale, `anno_ui_mock --live 2542x1351 [--ui-scale s] --screen hud_idle` writes what the
window overlays (the UI layer at the transform's size, stretched bilinearly per axis onto HE's draw
rectangle when not 1:1, over black). Three regions are cut out where the stretch puts them and
compared with the same pixels of the HE capture: the right chrome's tab band (reference rows
175..244, he-ui-scale.md's band), the gold symbol and the question mark of the bottom row. Mean
absolute difference per channel (0..255), bound 0.25 everywhere (measured 2026-11-14: 0.00 at 1:1,
0.04..0.06 at 1.5 and 2.0; the uniform k of 0x5a961 gave 10.6..11.9 at the bottom row in the
UI-WINDOW-1 review); a negative control (the region 3 pixels aside) must be at least twice as far
off. The last client row must be drawn (HE's quad covers it). At 1:1 the filler picture, the
question mark and the gold symbol must match the capture exactly. The default (no --ui-scale) must
equal --ui-scale 1.0 exactly.

Reads ANNO1503_GAME_DIR (the original files) and ANNO_UI_HE_CAPTURES (a folder with
he_s10_hud_idle_win.png, he_s15_hud_idle_win.png, he_s20_hud_idle_win.png; never in git); skips
(exit 77) when either is unset (a required-asset and an opt-in skip) or Pillow is missing; fails
when the folder is set but a capture is missing. Exit: 0 pass, 1 fail, 2 bad arguments,
77 skipped.
"""
import math
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]

# (setting, capture, bound per region: mean abs per channel, 0..255; measured 2026-11-14 with the
# per-axis stretch: 0.00 at 1:1, 0.04..0.06 at 1.5 and 2.0)
BOUNDS = {"tab band": 0.25, "gold symbol": 0.25, "question mark": 0.25}
CASES = [(None, "he_s10_hud_idle_win.png", BOUNDS), (1.0, "he_s10_hud_idle_win.png", BOUNDS),
         (1.5, "he_s15_hud_idle_win.png", BOUNDS), (2.0, "he_s20_hud_idle_win.png", BOUNDS)]
# 1:1 (the default and 1.0) at the capture's own size: the bottom-anchored filler picture and the
# question mark that closes the status bar, both exact in the capture (sprite-sad 0.0).
EXACT_1TO1 = [("filler", (2297, 1095, 2542, 1351)), ("question mark", (2263, 1316, 2297, 1351)),
              ("gold symbol", (0, 1316, 33, 1351))]
W, H = 2542, 1351
BAND_Y0, BAND_Y1 = 175, 245  # reference rows of the tab band (he-ui-scale.md 2)
PANEL_W = 245
BAR_H, GOLD_W, MARK_W = 35, 33, 34  # data/ui/hud.txt statusbar, gold, fragezeichen


def mean_abs(a, b, box_a, box_b):
    ca = a.crop(box_a).convert("RGB").tobytes()
    cb = b.crop(box_b).convert("RGB").tobytes()
    assert len(ca) == len(cb) and ca
    return sum(abs(x - y) for x, y in zip(ca, cb)) / len(ca)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    mock = Path(sys.argv[1])
    game = os.environ.get("ANNO1503_GAME_DIR")
    caps = os.environ.get("ANNO_UI_HE_CAPTURES")
    # Skip wording follows tools/test/ctest_verdict.py's classes: required-asset, then opt-in.
    if not game:
        print("skipping: ANNO1503_GAME_DIR is not set (needs the original game files)")
        return 77
    if not caps:
        print("skipping: set ANNO_UI_HE_CAPTURES=<folder of the HE captures> to run this comparison")
        return 77
    try:
        from PIL import Image
    except ImportError:
        print("skipping: Pillow (PIL) is not installed")
        return 77
    caps_dir = Path(caps)
    for _, name, _ in CASES:
        if not (caps_dir / name).is_file():
            print(f"FAIL: ANNO_UI_HE_CAPTURES is set but {caps_dir / name} is missing")
            return 1
    failures = []
    frames = {}
    with tempfile.TemporaryDirectory() as td:
        for setting, name, bounds in CASES:
            out = Path(td) / f"live_{setting}.png"
            cmd = [str(mock), "--layout", str(HERE / "data" / "ui"), "--game-dir", game,
                   "--screen", "hud_idle", "--out", str(out), "--live", f"{W}x{H}"]
            if setting is not None:
                cmd += ["--ui-scale", str(setting)]
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
            if r.returncode != 0:
                failures.append(f"{cmd}: exit {r.returncode}: {r.stderr.strip()}")
                continue
            m = re.search(r"layer (\d+)x(\d+) s (\S+) k (\S+) off (-?\d+) (-?\d+) "
                          r"draw (-?\d+) (-?\d+) (-?\d+) (-?\d+)", r.stdout)
            if not m:
                failures.append(f"no layer line in {r.stdout!r}")
                continue
            ui_w, ui_h = int(m.group(1)), int(m.group(2))
            dx0, dy0, dx1, dy1 = (int(m.group(i)) for i in range(7, 11))
            kx, ky = (dx1 - dx0) / ui_w, (dy1 - dy0) / ui_h
            ours = Image.open(out)
            frames[setting] = ours.convert("RGB").tobytes()
            cap = Image.open(caps_dir / name)

            def client_box(lx0, ly0, lx1, ly1):
                # The layer rectangle in client pixels, one pixel inside its stretched edges.
                return (dx0 + math.ceil(lx0 * kx) + 1, dy0 + math.ceil(ly0 * ky) + 1,
                        dx0 + math.floor(lx1 * kx) - 1, dy0 + math.floor(ly1 * ky) - 1)

            regions = [("tab band", (ui_w - PANEL_W, BAND_Y0, ui_w, BAND_Y1)),
                       ("gold symbol", (0, ui_h - BAR_H, GOLD_W, ui_h)),
                       ("question mark", (ui_w - PANEL_W - MARK_W, ui_h - BAR_H, ui_w - PANEL_W,
                                          ui_h))]
            for what, layer_rect in regions:
                box = client_box(*layer_rect)
                box = (max(box[0], 0), box[1], min(box[2], W), min(box[3], H))
                diff = mean_abs(ours, cap, box, box)
                step = 3 if box[0] < 3 else -3
                shifted = (box[0] + step, box[1], box[2] + step, box[3])
                control = mean_abs(ours, cap, shifted, box)
                bound = bounds[what]
                ok = diff <= bound and control >= 2 * diff
                print(f"ui-scale {setting}: layer {ui_w}x{ui_h} draw {dx0},{dy0}-{dx1},{dy1} "
                      f"{what} {box}: mean abs {diff:.2f} (bound {bound:.2f}), shifted 3 px "
                      f"{control:.2f} -> {'PASS' if ok else 'FAIL'}")
                if not ok:
                    failures.append(f"ui-scale {setting} {what}: {diff:.2f} / control {control:.2f}")
            # HE's quad covers every client row: the last one is drawn (not left black).
            last = ours.crop((0, H - 1, W, H)).convert("L").tobytes()
            if sum(last) / len(last) < 40:
                failures.append(f"ui-scale {setting}: the last client row is not drawn")
            if kx == 1.0 and ky == 1.0:
                for what, b in EXACT_1TO1:
                    d = mean_abs(ours, cap, b, b)
                    print(f"  {what} {b}: mean abs {d:.2f}")
                    if d != 0.0:
                        failures.append(f"ui-scale {setting}: {what} differs ({d:.2f})")
    if None in frames and 1.0 in frames and frames[None] != frames[1.0]:
        failures.append("the default differs from --ui-scale 1.0 at 2542 x 1351")
    for f in failures:
        print("FAIL:", f)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
