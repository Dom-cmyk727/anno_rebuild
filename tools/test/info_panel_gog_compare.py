#!/usr/bin/env python3
"""info_panel_gog_compare.py: UI-INFO-1 fix round F (F8). The info panel reference
(tools/ui_layout.py compose_info_panel, which the live window equals exactly -
info_panel_pixel_check.py) checked against the original GOG game's own Wine-runner shots of the
15 Citizen-start info panels, per pixel after the RGB565 reduction (docs/re/ui.md section 2.5).

    python -I tools/test/info_panel_gog_compare.py

Reads ANNO1503_GAME_DIR (the original files) and ANNO_UI_ORACLE_SHOTS (a folder holding the
`r2_info_*` / `f12_info_*` PNGs from cache/orig/ui-1/shots/; screenshots are never in git), and
skips with a message when either is missing. Passes when, on every shot, the name, the figures
and the chrome differ in 0 pixels, and the whole panel differs in 0 pixels for the three types
whose preview rule is proven (2520, 104, 708). The other previews are OQ-612: their counts are
printed, not judged.

Exit: 0 pass, 1 fail, 77 skipped.
"""
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HERE / "tools"))

SHOTS = {
    2520: "r2_info_dirt_road", 104: "f12_info_field_104", 708: "f12_info_field_708",
    801: "r2_info_forester_801", 1301: "r2_info_market_1301", 1302: "f12_info_kontor_1302",
    501: "r2_info_pioneer_house", 1310: "f12_info_stall_1310", 1314: "f12_info_stall_1314",
    807: "r2_info_sheep_807", 805: "r2_info_hunter_805", 809: "f12_info_farm_809",
    1023: "r2_info_tannery_1023", 1001: "r2_info_weaver_1001", 810: "f12_info_fisher_810",
}
PREVIEW_PROVEN = {2520, 104, 708}


def main() -> int:
    game_dir = os.environ.get("ANNO1503_GAME_DIR")
    if not game_dir or not Path(game_dir, "Anlagen.dat").is_file():
        print("skipping: ANNO1503_GAME_DIR is not set to the original game folder")
        return 77
    shots_dir = os.environ.get("ANNO_UI_ORACLE_SHOTS")
    if not shots_dir or not Path(shots_dir).is_dir():
        print("skipping: ANNO_UI_ORACLE_SHOTS is not set to a folder of the GOG info-panel shots")
        return 77
    try:
        from PIL import Image
    except ImportError:
        print("skipping: Pillow (PIL) is not installed")
        return 77
    import ui_layout

    layout_dir = str(HERE / "data" / "ui")
    failures = []
    found = 0
    for type_id, name in SHOTS.items():
        png = Path(shots_dir, name + ".png")
        if not png.is_file():
            failures.append(f"{type_id}: {png} is missing")
            continue
        found += 1
        shot = Image.open(png).convert("RGB")
        mock = ui_layout.compose_info_panel(layout_dir, game_dir, type_id, base=shot)
        res = ui_layout.info_compare(mock, shot)
        r = res["by_region"]
        print(f"{type_id} {name}: {res['bad']} of {res['checked']} px differ {r}")
        if r["name"] or r["figures"] or r["chrome"]:
            failures.append(f"{type_id}: name/figures/chrome differ {r}")
        if type_id in PREVIEW_PROVEN and res["bad"]:
            failures.append(f"{type_id}: the proven preview rule differs in {r['preview']} px")
    if failures:
        print("FAILURES:")
        for f in failures:
            print(f"  {f}")
        return 1
    print(f"info panel reference matches the GOG shots ({found} shots)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
