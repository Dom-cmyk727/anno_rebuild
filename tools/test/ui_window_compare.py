#!/usr/bin/env python3
"""ui_window_compare.py: the UI layer at a 2542 x 1351 window (UI-WINDOW-1) against the History
Edition's own captures at UI scale 1.0, 1.5 and 2.0 (docs/re/he-ui-scale.md, visual check).

    python -I tools/test/ui_window_compare.py <anno_ui_mock executable>

For each scale, `anno_ui_mock --live 2542x1351 [--ui-scale s] --screen hud_idle` writes what the
window overlays (the UI layer at the transform's size, stretched bilinearly when not 1:1, over
black). The right chrome's tab band (reference rows 175..244, he-ui-scale.md's band) is cut out
where the transform puts it and compared with the same pixels of the HE capture: mean absolute
difference per channel (0..255). The bounds (0.25 / 1.5 / 2.5) are tighter than he-ui-scale.md's
own figures for the GOG band resized by PIL against the same captures (3.50 / 4.60 / 4.25; first
measured here 2026-11-14: 0.00 / 0.87 / 1.75); a negative control (the band 3 pixels to the left)
must be at least twice as far off. At 1:1 the filler picture, the question mark and the gold
symbol must match the capture exactly. The default (no --ui-scale) must equal --ui-scale 1.0
exactly.

Reads ANNO1503_GAME_DIR (the original files) and ANNO_UI_HE_CAPTURES (a folder with
he_s10_hud_idle_win.png, he_s15_hud_idle_win.png, he_s20_hud_idle_win.png; never in git); skips
(exit 77) when either is unset (a required-asset and an opt-in skip) or Pillow is missing; fails
when the folder is set but a capture is missing. Exit: 0 pass, 1 fail, 2 bad arguments,
77 skipped.
"""
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]

# (setting, capture, he-ui-scale.md's measured mean abs diff for the GOG band at the best k)
CASES = [(None, "he_s10_hud_idle_win.png", 0.25), (1.0, "he_s10_hud_idle_win.png", 0.25),
         (1.5, "he_s15_hud_idle_win.png", 1.5), (2.0, "he_s20_hud_idle_win.png", 2.5)]
# 1:1 (the default and 1.0) at the capture's own size: the bottom-anchored filler picture and the
# question mark that closes the status bar, both exact in the capture (sprite-sad 0.0).
EXACT_1TO1 = [("filler", (2297, 1095, 2542, 1351)), ("question mark", (2263, 1316, 2297, 1351)),
              ("gold symbol", (0, 1316, 33, 1351))]
W, H = 2542, 1351
BAND_Y0, BAND_Y1 = 175, 245  # reference rows of the tab band (he-ui-scale.md 2)
PANEL_W = 245


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
        for setting, name, bound in CASES:
            out = Path(td) / f"live_{setting}.png"
            cmd = [str(mock), "--layout", str(HERE / "data" / "ui"), "--game-dir", game,
                   "--screen", "hud_idle", "--out", str(out), "--live", f"{W}x{H}"]
            if setting is not None:
                cmd += ["--ui-scale", str(setting)]
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
            if r.returncode != 0:
                failures.append(f"{cmd}: exit {r.returncode}: {r.stderr.strip()}")
                continue
            m = re.search(r"layer (\d+)x(\d+) s (\S+) k (\S+) off (-?\d+) (-?\d+)", r.stdout)
            if not m:
                failures.append(f"no layer line in {r.stdout!r}")
                continue
            ui_w, k = int(m.group(1)), float(m.group(4))
            ours = Image.open(out)
            frames[setting] = ours.convert("RGB").tobytes()
            cap = Image.open(caps_dir / name)
            x0 = int((ui_w - PANEL_W) * k)
            box = (x0 + 1, int(BAND_Y0 * k) + 1, W - 1, int(BAND_Y1 * k) - 1)
            diff = mean_abs(ours, cap, box, box)
            shifted = (box[0] - 3, box[1], box[2] - 3, box[3])
            control = mean_abs(ours, cap, shifted, box)
            ok = diff <= bound and control >= 2 * diff
            print(f"ui-scale {setting}: layer {m.group(1)}x{m.group(2)} k {k:.6f} panel x {x0} "
                  f"band {box}: mean abs {diff:.2f} (bound {bound:.2f}), "
                  f"3 px left {control:.2f} -> {'PASS' if ok else 'FAIL'}")
            if not ok:
                failures.append(f"ui-scale {setting}: {diff:.2f} / control {control:.2f}")
            if k == 1.0:
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
