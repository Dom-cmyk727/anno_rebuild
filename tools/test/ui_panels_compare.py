#!/usr/bin/env python3
"""ui_panels_compare.py: the four tab panels' static chrome (UI-WINDOW-1 fix round R2) against the
original's GOG shots at 1024 x 768.

    python -I tools/test/ui_panels_compare.py <anno_ui_mock executable>

For each panel, `anno_ui_mock --live 1024x768 --screen <panel>` draws the HUD with that tab's
panel (data/ui/hud.txt elements with `panel <name>`), and every such element's rectangle is
compared with the named GOG shot, together with the four tabs' rectangles (the selected frame
on the panel's tab; fix round R3),, pixel by pixel, where ours is opaque (not the mock's black
canvas, the live_screen_compare.py convention): a pixel is bad when a channel differs by more
than 48 after the shot's RGB565 reduction is applied to ours (tools/ui_layout.py rgb565_shown, the
UI.1 tolerance COMPARE_CHANNEL); a panel passes when at most 8 % of its compared pixels are bad
(COMPARE_BAD_FRACTION) and every element on its own stays within ELEMENT_BOUND (fix round R3).
Masked out: the live names and figures this round does not draw
(UI-TABS-1) - the plank's name (masks.txt `nameplate`) and the rectangles in LIVE below, each
measured on its shot as the area where the original prints a name or a number. A negative
control compares the player panel's elements against the main menu's shot and must fail.

UI-BUILDCAT-1: the construction menu's category column too. For each of the eight categories,
`anno_ui_mock --live 1024x768 --screen construction_<category>` is compared with the GOG shot
`r2_construction_<category>` over every category tab's rectangle (hud.txt `viewbautab_<k>`, the
open category's tab in its selected frame), each tab within ELEMENT_DEFAULT. Masked out: the
hover tooltip the original prints over the clicked tab (the 17 rows above construction.txt's
click y, measured on all eight shots: rows y-23..y-8). The column's own control compares the
public menu's tabs with the farms shot (two tabs in the other frame) and must fail.

Reads ANNO1503_GAME_DIR and ANNO_UI_ORACLE_SHOTS (the GOG shots, never in git); skips (77)
without them or without Pillow. Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HERE / "tools"))

CHANNEL = 48
BAD_FRACTION = 0.08
# R3: each element on its own: at most 2 % of its compared pixels bad (they matched 0.88-1.00
# exactly after RGB565); the backgrounds keep their own measured bound (OPTION_BACKGROUND matched
# 0.79 exactly, the menu icons cover the rest; the two BILANZ backgrounds mean abs 0.00-0.35).
ELEMENT_DEFAULT = 0.02
ELEMENT_BOUND = {"player_bk": 0.02, "info_bk": 0.02, "menu_bk": 0.05}
TABS = ("tab_construction", "tab_military", "tab_help", "tab_options")
# (panel, anno_ui_mock screen, GOG shot)
PANELS = [("player", "hud_idle", "hud_idle_running"),
          ("player_list", "player_list", "r2_player_menu"),
          ("info", "info", "r2_tab_help"),
          ("main_menu", "main_menu", "r2_options")]
# Live names and figures on each shot (x, y, w, h), not drawn this round.
LIVE = {
    "player": [(800, 258, 200, 22), (930, 318, 85, 180), (930, 525, 85, 25)],
    "player_list": [(800, 258, 200, 22)],
    "info": [(800, 258, 200, 22), (930, 318, 85, 180), (930, 525, 85, 25), (930, 625, 85, 25),
             (965, 245, 59, 58),
             (880, 194, 84, 18)],  # the hover tooltip ("Info mode") the shot shows over the tabs
    "main_menu": [(800, 258, 200, 22)],
}


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    mock = Path(sys.argv[1])
    game = os.environ.get("ANNO1503_GAME_DIR")
    if not game:
        print("skipping: ANNO1503_GAME_DIR is not set (needs the original game files)")
        return 77
    shots = os.environ.get("ANNO_UI_ORACLE_SHOTS")
    if not shots:
        print("skipping: set ANNO_UI_ORACLE_SHOTS=<folder of the GOG shots> to run this comparison")
        return 77
    try:
        from PIL import Image
    except ImportError:
        print("skipping: Pillow (PIL) is not installed")
        return 77
    import ui_layout

    hud = ui_layout.parse_hud(str(HERE / "data" / "ui" / "hud.txt"))

    def rects(panel):
        # The panel's own elements, and the four tabs (R3: the selected-tab mapping is checked
        # too - the selected frame on the panel's tab, the idle frame on the others).
        out = []
        for el in hud["elements"]:
            in_panel = panel in str(el.get("panel", "")).split(",")
            if (in_panel or el["name"] in TABS) and el.get("pos") and el.get("size"):
                out.append((el["pos"][0], el["pos"][1], el["size"][0], el["size"][1], el["name"]))
        return out

    def masked(panel, x, y):
        return any(mx <= x < mx + mw and my <= y < my + mh for mx, my, mw, mh in LIVE[panel])

    def compare(ours, shot, panel, layout_panel):
        # Per element: (checked, bad) over the element's own rectangle.
        o, s = ours.load(), shot.load()
        per = {}
        for x0, y0, w, h, name in rects(layout_panel):
            checked = bad = 0
            for y in range(max(0, y0), min(768, y0 + h)):
                for x in range(max(0, x0), min(1024, x0 + w)):
                    if masked(panel, x, y) or o[x, y] == (0, 0, 0):
                        continue  # a live area, or a transparent pixel of ours (black canvas)
                    a = ui_layout.rgb565_shown(o[x, y])
                    b = s[x, y]
                    checked += 1
                    if max(abs(a[c] - b[c]) for c in range(3)) > CHANNEL:
                        bad += 1
            per[name] = (checked, bad)
        return per

    failures = []
    frames = {}
    with tempfile.TemporaryDirectory() as td:
        for panel, screen, shot_name in PANELS:
            out = Path(td) / f"{panel}.png"
            cmd = [str(mock), "--layout", str(HERE / "data" / "ui"), "--game-dir", game,
                   "--screen", screen, "--out", str(out), "--live", "1024x768"]
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
            if r.returncode != 0:
                failures.append(f"{panel}: anno_ui_mock exit {r.returncode}: {r.stderr.strip()}")
                continue
            shot_path = Path(shots) / f"{shot_name}.png"
            if not shot_path.is_file():
                failures.append(f"{panel}: {shot_path} is missing")
                continue
            ours = Image.open(out).convert("RGB")
            frames[panel] = ours
            shot = Image.open(shot_path).convert("RGB")
            per = compare(ours, shot, panel, panel)
            checked = sum(c for c, _ in per.values())
            bad = sum(b for _, b in per.values())
            frac = bad / max(checked, 1)
            ok = checked > 0 and frac <= BAD_FRACTION
            print(f"{panel} vs {shot_name}: {checked} px compared, {bad} bad ({frac:.4f}, bound "
                  f"{BAD_FRACTION}) -> {'PASS' if ok else 'FAIL'}")
            if not ok:
                failures.append(f"{panel}: {frac:.4f}")
            # R3: every element on its own (a wrong small element hides behind a big background
            # in the panel's total).
            for name, (c, b) in per.items():
                f = b / c if c else 1.0
                bound = ELEMENT_BOUND.get(name, ELEMENT_DEFAULT)
                if c == 0 or f > bound:
                    print(f"  {name}: {c} px, {b} bad ({f:.4f}, bound {bound}) -> FAIL")
                    failures.append(f"{panel}/{name}: {f:.4f} of {c}")
                elif b:
                    print(f"  {name}: {c} px, {b} bad ({f:.4f}, bound {bound})")
        # Negative control: the player panel's elements against the main menu's shot.
        if "player" in frames:
            shot = Image.open(Path(shots) / "r2_options.png").convert("RGB")
            per = compare(frames["player"], shot, "player", "player")
            checked = sum(c for c, _ in per.values())
            frac = sum(b for _, b in per.values()) / max(checked, 1)
            print(f"control: player panel vs r2_options: {frac:.4f} bad (must exceed {BAD_FRACTION})")
            if frac <= BAD_FRACTION:
                failures.append("the control did not fail")
        # UI-BUILDCAT-1: the category column on the eight construction shots.
        con = ui_layout.parse_construction(str(HERE / "data" / "ui" / "construction.txt"))
        tabs = [el for el in hud["elements"]
                if el["name"].startswith("viewbautab_") and el.get("pos") and el.get("size")]
        if len(tabs) != len(con["categories"]):
            failures.append(f"column: {len(tabs)} viewbautab elements for "
                            f"{len(con['categories'])} categories")

        def column(ours, shot, tooltip_y):
            o, s = ours.load(), shot.load()
            per = {}
            for el in tabs:
                (x0, y0), (w, h) = el["pos"], el["size"]
                checked = bad = 0
                for y in range(y0, y0 + h):
                    if tooltip_y - 24 <= y < tooltip_y - 7:
                        continue  # the hover tooltip over the clicked tab
                    for x in range(x0, x0 + w):
                        if o[x, y] == (0, 0, 0):
                            continue
                        a = ui_layout.rgb565_shown(o[x, y])
                        b = s[x, y]
                        checked += 1
                        if max(abs(a[c] - b[c]) for c in range(3)) > CHANNEL:
                            bad += 1
                per[el["name"]] = (checked, bad)
            return per

        column_frames = {}
        for cname, crec in con["categories"].items():
            out = Path(td) / f"construction_{cname}.png"
            cmd = [str(mock), "--layout", str(HERE / "data" / "ui"), "--game-dir", game,
                   "--screen", f"construction_{cname}", "--out", str(out), "--live", "1024x768"]
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
            if r.returncode != 0:
                failures.append(f"column {cname}: anno_ui_mock exit {r.returncode}: "
                                f"{r.stderr.strip()}")
                continue
            shot_path = Path(shots) / f"r2_construction_{cname}.png"
            if not shot_path.is_file():
                failures.append(f"column {cname}: {shot_path} is missing")
                continue
            ours = Image.open(out).convert("RGB")
            column_frames[cname] = ours
            shot = Image.open(shot_path).convert("RGB")
            per = column(ours, shot, int(crec["y"]))
            checked = sum(c for c, _ in per.values())
            bad = sum(b for _, b in per.values())
            worst = max((b / c if c else 1.0) for c, b in per.values()) if per else 1.0
            ok = bool(per) and worst <= ELEMENT_DEFAULT
            print(f"column {cname} vs r2_construction_{cname}: {len(per)} tabs, {checked} px, "
                  f"{bad} bad, worst tab {worst:.4f} (bound {ELEMENT_DEFAULT}) -> "
                  f"{'PASS' if ok else 'FAIL'}")
            if not ok:
                for name, (c, b) in per.items():
                    if c == 0 or b / c > ELEMENT_DEFAULT:
                        failures.append(f"column {cname}/{name}: {b} of {c} bad")
        if "public" in column_frames and "farms" in con["categories"]:
            shot = Image.open(Path(shots) / "r2_construction_farms.png").convert("RGB")
            per = column(column_frames["public"], shot, int(con["categories"]["farms"]["y"]))
            worst = max((b / c if c else 1.0) for c, b in per.values()) if per else 0.0
            print(f"control: public column vs r2_construction_farms: worst tab {worst:.4f} "
                  f"(must exceed {ELEMENT_DEFAULT})")
            if worst <= ELEMENT_DEFAULT:
                failures.append("the column control did not fail")
    for f in failures:
        print("FAIL:", f)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
