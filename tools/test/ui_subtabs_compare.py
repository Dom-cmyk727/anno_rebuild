#!/usr/bin/env python3
"""ui_subtabs_compare.py: the right panel's sub-tab columns (UI-TABS-1 stage A, docs/re/ui.md 5.4)
drawn in their selected and idle frames, in both UI paths.

    python -I tools/test/ui_subtabs_compare.py <anno_ui_mock executable> <anno1503 executable>

1. The GPU path's composer (`anno_ui_mock --live 1024x768 --screen <view>.<k>`, which is
   draw_live_screen_rgba, the layer the GPU overlay uploads) against the original's GOG shots
   that show that view at that sub-tab: every sub-tab rectangle of the view (and the player
   view's crest), pixel by pixel where ours is opaque, a pixel bad when a channel differs by more
   than 48 after the shot's RGB565 reduction (tools/ui_layout.py rgb565_shown, the UI.1 tolerance
   ui_panels_compare.py uses), each element at most 2 % bad; the live name and the options shots'
   hover tooltip are masked (NAME, TOOLTIP). A negative control draws the player
   view at sub-tab 0 against f15_ship_list (sub-tab 3 selected) and must fail on sub-tabs 0 and 3.
2. The CPU path (the live window, draw_cpu -> draw_live_screen_xrgb; viewer_wiring script
   ui_subtabs_draw.script: player 3, options 2, city 5) against the same composer at the same
   state: the sub-tab rectangles must be pixel-identical (CPU/GPU-layer parity for the columns;
   city sub-tab 5 has no GOG shot, so this is its only check).

Reads ANNO1503_GAME_DIR and ANNO_UI_ORACLE_SHOTS (the GOG shots, never in git); skips (77) without
them, without Pillow, or off Windows (the window's screenshot step is Windows-only).
Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HERE / "tools"))

CHANNEL = 48
ELEMENT_BOUND = 0.02
# (screen, GOG shot): the view at the sub-tab the shot shows (docs/re/ui.md 5.4 lists how each
# was reached: hud_idle_running the start, f15_player_menu key D, f15_city_list key C,
# f15_ship_list key S, r2_tab_help the "?" tab, r2_options and f15_options_y360..y510 the gears
# tab and its sub-tabs 1..4).
SHOTS = [("player.0", "hud_idle_running"), ("player.1", "f15_player_menu"),
         ("player.2", "f15_city_list"), ("player.3", "f15_ship_list"),
         ("info.0", "r2_tab_help"), ("options.0", "r2_options"),
         ("options.1", "f15_options_y360"), ("options.2", "f15_options_y410"),
         ("options.3", "f15_options_y460"), ("options.4", "f15_options_y510")]
# Masked out of the shots: the plank's live name (masks.txt `nameplate`, as ui_panels_compare.py
# masks it; the long name reaches under the crest) and, on the four options shots, the hover
# tooltip the runner's click at (800, y) left over the column (measured: its text sits in the 35
# rows above the click, x 755..840).
NAME = (800, 258, 200, 22)
TOOLTIP = {f"f15_options_y{y}": (740, y - 35, 120, 40) for y in (360, 410, 460, 510)}
# The window's screenshots (ui_subtabs_draw.script) and the state each shows.
LIVE = [("player.3", "subtabs_player_3"), ("options.2", "subtabs_options_2"),
        ("info.5", "subtabs_info_5")]
SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "ui_subtabs_draw.script"
LAN5 = HERE / "data" / "maps" / "lan5.txt"


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    mock, exe = Path(sys.argv[1]), Path(sys.argv[2])
    game = os.environ.get("ANNO1503_GAME_DIR")
    if not game:
        print("skipping: ANNO1503_GAME_DIR is not set (needs the original game files)")
        return 77
    shots = os.environ.get("ANNO_UI_ORACLE_SHOTS")
    if not shots:
        print("skipping: set ANNO_UI_ORACLE_SHOTS=<folder of the GOG shots> to run this comparison")
        return 77
    if os.name != "nt":
        print("skipping: the window's screenshot step is Windows-only (viewer.cpp)")
        return 77
    try:
        from PIL import Image
    except ImportError:
        print("skipping: Pillow (PIL) is not installed")
        return 77
    import ui_layout

    layout_dir = HERE / "data" / "ui"
    hud = ui_layout.parse_hud(str(layout_dir / "hud.txt"))

    def rects(view):
        out = []
        for el in hud["elements"]:
            if el.get("view") == view and "subtab" in el:
                out.append((el["pos"][0], el["pos"][1], el["size"][0], el["size"][1], el["name"]))
        if view == "player":
            el = next(e for e in hud["elements"] if e["name"] == "player_crest")
            out.append((el["pos"][0], el["pos"][1], el["size"][0], el["size"][1], el["name"]))
        return out

    # The crest is compared on its own opaque pixels only: around it the shot shows the page body
    # (on f15_ship_list and f15_city_list a few columns left of the shield, x 982..989, differ
    # from the overview's and the list's), which a placeholder does not draw.
    crest = next(e for e in hud["elements"] if e["name"] == "player_crest")
    crest_w, crest_h, crest_px = ui_layout.decode_tex_rgba(
        str(Path(game) / crest["file"]), int(crest["tex_id"]))

    def crest_opaque(x, y):
        i, j = x - crest["pos"][0], y - crest["pos"][1]
        return 0 <= i < crest_w and 0 <= j < crest_h and crest_px[(j * crest_w + i) * 4 + 3] >= 128

    def per_element(ours, theirs, view, shown, masks=()):
        o, s = ours.load(), theirs.load()
        result = {}
        for x0, y0, w, h, name in rects(view):
            checked = bad = 0
            for y in range(y0, y0 + h):
                for x in range(x0, x0 + w):
                    if o[x, y] == (0, 0, 0):
                        continue  # transparent in ours (black canvas)
                    if name == "player_crest" and not crest_opaque(x, y):
                        continue
                    if any(mx <= x < mx + mw and my <= y < my + mh for mx, my, mw, mh in masks):
                        continue
                    a = ui_layout.rgb565_shown(o[x, y]) if shown else o[x, y]
                    b = s[x, y]
                    checked += 1
                    if max(abs(a[c] - b[c]) for c in range(3)) > (CHANNEL if shown else 0):
                        bad += 1
            result[name] = (checked, bad)
        return result

    failures = []
    with tempfile.TemporaryDirectory(prefix="anno_ui_subtabs_") as td:
        drawn = {}

        def draw(screen):
            if screen not in drawn:
                out = Path(td) / f"{screen}.png"
                cmd = [str(mock), "--layout", str(layout_dir), "--game-dir", game, "--screen",
                       screen, "--out", str(out), "--live", "1024x768"]
                r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
                if r.returncode != 0:
                    failures.append(f"{screen}: anno_ui_mock exit {r.returncode}: {r.stderr}")
                    drawn[screen] = None
                else:
                    drawn[screen] = Image.open(out).convert("RGB")
            return drawn[screen]

        # 1. The composer against the GOG shots.
        for screen, shot_name in SHOTS:
            ours = draw(screen)
            shot_path = Path(shots) / f"{shot_name}.png"
            if ours is None:
                continue
            if not shot_path.is_file():
                failures.append(f"{screen}: {shot_path} is missing")
                continue
            view = screen.split(".")[0]
            masks = [NAME] + ([TOOLTIP[shot_name]] if shot_name in TOOLTIP else [])
            per = per_element(ours, Image.open(shot_path).convert("RGB"), view, True, masks)
            worst = max((b / c if c else 1.0) for c, b in per.values())
            print(f"{screen} vs {shot_name}: {len(per)} elements, "
                  f"{sum(c for c, _ in per.values())} px, worst element {worst:.4f}")
            for name, (c, b) in per.items():
                if c == 0 or b / c > ELEMENT_BOUND:
                    failures.append(f"{screen} vs {shot_name}: {name} {b} of {c} bad")
        # The negative control: the overview's column against the ship list's shot.
        ours = draw("player.0")
        if ours is not None:
            per = per_element(ours, Image.open(Path(shots) / "f15_ship_list.png").convert("RGB"),
                              "player", True, [NAME])
            failing = sorted(n for n, (c, b) in per.items() if c and b / c > ELEMENT_BOUND)
            print(f"control: player.0 vs f15_ship_list fails on {failing}")
            if failing != ["player_subtab_0", "player_subtab_3"]:
                failures.append(f"the control failed on {failing}, not on sub-tabs 0 and 3")

        # 2. The live window's CPU path against the composer, exact.
        out = Path(td) / "session"  # must not exist yet (main.cpp's own check)
        env = dict(os.environ, ANNO_DEV_SCRIPT=str(SCRIPT), ANNO_DEV_SESSION=str(out),
                   ANNO_VIEW_QUIT_S="30", SDL_VIDEODRIVER="dummy")
        proc = subprocess.run(
            [str(exe), "--play", str(LAN5), "--game-dir", game, "--construction-menu", "on",
             "--layout", str(layout_dir), "--renderer", "cpu"],
            env=env, capture_output=True, text=True, timeout=120, check=False)
        if proc.returncode != 0:
            failures.append(f"anno1503 exited {proc.returncode}: {proc.stderr[-2000:]}")
        else:
            for screen, png in LIVE:
                path = out / f"{png}.png"
                ours = draw(screen)
                if ours is None:
                    continue
                if not path.is_file():
                    failures.append(f"{screen}: {path} was not captured")
                    continue
                per = per_element(ours, Image.open(path).convert("RGB"), screen.split(".")[0],
                                  False)
                bad = sum(b for _, b in per.values())
                checked = sum(c for c, _ in per.values())
                print(f"{screen}: window (CPU) vs composer: {bad} of {checked} px differ")
                if bad or not checked:
                    failures.append(f"{screen}: window vs composer {bad} of {checked} differ: "
                                    f"{ {n: v for n, v in per.items() if v[1]} }")
    for f in failures:
        print("FAIL:", f)
    if not failures:
        print("every sub-tab column matches")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
