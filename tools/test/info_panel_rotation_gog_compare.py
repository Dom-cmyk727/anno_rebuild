#!/usr/bin/env python3
"""info_panel_rotation_gog_compare.py: UI-INFO-2. The info panel reference at rotations 0..3
(tools/ui_layout.py compose_info_panel with `rotation`, which the live window equals exactly -
info_panel_rotation_check.py) checked against the original GOG game's own Wine-runner shots taken
after clicks on its rotate arrows and its `.` / `,` keys (docs/re/ui.md section 2.5).

    python -I tools/test/info_panel_rotation_gog_compare.py

Reads ANNO1503_GAME_DIR (the original files) and ANNO_UI_ROTATION_SHOTS (a folder holding the
`u2_*` PNGs of the UI-INFO-2 capture, cache/orig/ui-info-2/shots/ in the packet's worktree;
screenshots are never in git), and skips with a message when either is missing.

Dirt road 2520 (unscaled preview, rule proven): after 0..4 clicks on rotate_right and one on
rotate_left the whole panel must differ in 0 pixels from the reference at the expected rotation
(0, 1, 2, 3, 0, 3), and the reference at each other rotation must differ (wrong-input control).

Pioneer house 501 (scaled preview: OQ-612's placeholder, so no pixel judgement): the reference
rotation nearest to the shot (fewest differing preview pixels) must be the expected one for each
of the 13 shots (left arrow 3, 2, 1, 0; right arrow 1, 2, 3, 0; `.` 1, 2; `,` 1, 0). This checks
which view comes next, not pixels; the name, figures and chrome must still differ in 0 pixels.

Exit: 0 pass, 1 fail, 77 skipped.
"""
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HERE / "tools"))

ROAD = 2520
ROAD_SHOTS = {"u2_2520_start": 0, "u2_2520_R1": 1, "u2_2520_R2": 2, "u2_2520_R3": 3,
              "u2_2520_R4": 0, "u2_2520_R4L1": 3}
HOUSE = 501
HOUSE_SHOTS = {"u2_501_start": 0, "u2_501_L1": 3, "u2_501_L2": 2, "u2_501_L3": 1,
               "u2_501_L4": 0, "u2_501_R1": 1, "u2_501_R2": 2, "u2_501_R3": 3, "u2_501_R4": 0,
               "u2_501_dot1": 1, "u2_501_dot2": 2, "u2_501_comma1": 1, "u2_501_comma2": 0}
ROTATIONS = range(4)


def main() -> int:
    game_dir = os.environ.get("ANNO1503_GAME_DIR")
    if not game_dir or not Path(game_dir, "Anlagen.dat").is_file():
        print("skipping: ANNO1503_GAME_DIR is not set to the original game folder")
        return 77
    shots_dir = os.environ.get("ANNO_UI_ROTATION_SHOTS")
    if not shots_dir or not Path(shots_dir).is_dir():
        print("skipping: ANNO_UI_ROTATION_SHOTS is not set to a folder of the UI-INFO-2 shots")
        return 77
    try:
        from PIL import Image
    except ImportError:
        print("skipping: Pillow (PIL) is not installed")
        return 77
    import ui_layout

    layout_dir = str(HERE / "data" / "ui")
    failures = []

    def per_rotation(type_id, shot):
        out = []
        for r in ROTATIONS:
            mock = ui_layout.compose_info_panel(layout_dir, game_dir, type_id, base=shot,
                                                rotation=r)
            out.append(ui_layout.info_compare(mock, shot))
        return out

    for name, expect in {**ROAD_SHOTS, **HOUSE_SHOTS}.items():
        png = Path(shots_dir, name + ".png")
        if not png.is_file():
            failures.append(f"{name}: {png} is missing")
            continue
        type_id = ROAD if name.startswith("u2_2520") else HOUSE
        shot = Image.open(png).convert("RGB")
        res = per_rotation(type_id, shot)
        previews = [x["by_region"]["preview"] for x in res]
        r = res[expect]["by_region"]
        print(f"{name}: expected rotation {expect}; differing preview px per rotation "
              f"{previews}; at {expect}: {res[expect]['bad']} of {res[expect]['checked']} {r}")
        if r["name"] or r["figures"] or r["chrome"]:
            failures.append(f"{name}: name/figures/chrome differ {r}")
        if type_id == ROAD:
            if res[expect]["bad"]:
                failures.append(f"{name}: the rotation-{expect} reference differs in "
                                f"{res[expect]['bad']} px")
            for other in ROTATIONS:
                if other != expect and res[other]["bad"] == 0:
                    failures.append(f"{name}: the wrong-rotation control {other} was not "
                                    f"rejected")
        else:
            nearest = previews.index(min(previews))
            if nearest != expect or previews.count(min(previews)) != 1:
                failures.append(f"{name}: nearest rotation {nearest}, expected {expect} "
                                f"({previews})")
    if failures:
        print("FAILURES:")
        for f in failures:
            print(f"  {f}")
        return 1
    print(f"info panel reference matches the GOG rotation shots ({len(ROAD_SHOTS)} exact, "
          f"{len(HOUSE_SHOTS)} by nearest view)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
