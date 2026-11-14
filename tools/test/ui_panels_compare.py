#!/usr/bin/env python3
"""ui_panels_compare.py: the four tab panels' static chrome (UI-WINDOW-1 fix round R2) against the
original's GOG shots at 1024 x 768.

    python -I tools/test/ui_panels_compare.py <anno_ui_mock executable>

For each panel, `anno_ui_mock --live 1024x768 --screen <panel>` draws the HUD with that tab's
panel (data/ui/hud.txt elements with `panel <name>`), and every such element's rectangle is
compared with the named GOG shot, pixel by pixel, where ours is opaque (not the mock's black
canvas, the live_screen_compare.py convention): a pixel is bad when a channel differs by more
than 48 after the shot's RGB565 reduction is applied to ours (tools/ui_layout.py rgb565_shown, the
UI.1 tolerance COMPARE_CHANNEL); a panel passes when at most 8 % of its compared pixels are bad
(COMPARE_BAD_FRACTION). Masked out: the live names and figures this round does not draw
(UI-TABS-1) - the plank's name (masks.txt `nameplate`) and the rectangles in LIVE below, each
measured on its shot as the area where the original prints a name or a number. A negative
control compares the player panel's elements against the main menu's shot and must fail.

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
             (965, 245, 59, 58)],
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
        out = []
        for el in hud["elements"]:
            if panel in str(el.get("panel", "")).split(",") and el.get("pos") and el.get("size"):
                out.append((el["pos"][0], el["pos"][1], el["size"][0], el["size"][1], el["name"]))
        return out

    def masked(panel, x, y):
        return any(mx <= x < mx + mw and my <= y < my + mh for mx, my, mw, mh in LIVE[panel])

    def compare(ours, shot, panel, layout_panel):
        o, s = ours.load(), shot.load()
        bad = checked = 0
        worst = {}
        for x0, y0, w, h, name in rects(layout_panel):
            for y in range(max(0, y0), min(768, y0 + h)):
                for x in range(max(0, x0), min(1024, x0 + w)):
                    if masked(panel, x, y) or o[x, y] == (0, 0, 0):
                        continue  # a live area, or a transparent pixel of ours (black canvas)
                    a = ui_layout.rgb565_shown(o[x, y])
                    b = s[x, y]
                    checked += 1
                    if max(abs(a[c] - b[c]) for c in range(3)) > CHANNEL:
                        bad += 1
                        worst[name] = worst.get(name, 0) + 1
        return bad, checked, worst

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
            bad, checked, worst = compare(ours, shot, panel, panel)
            frac = bad / max(checked, 1)
            ok = checked > 0 and frac <= BAD_FRACTION
            print(f"{panel} vs {shot_name}: {checked} px compared, {bad} bad ({frac:.4f}, bound "
                  f"{BAD_FRACTION}) -> {'PASS' if ok else 'FAIL'}"
                  + (f"; bad by element {sorted(worst.items(), key=lambda kv: -kv[1])[:5]}"
                     if worst else ""))
            if not ok:
                failures.append(f"{panel}: {frac:.4f}")
        # Negative control: the player panel's elements against the main menu's shot.
        if "player" in frames:
            shot = Image.open(Path(shots) / "r2_options.png").convert("RGB")
            bad, checked, _ = compare(frames["player"], shot, "player", "player")
            frac = bad / max(checked, 1)
            print(f"control: player panel vs r2_options: {frac:.4f} bad (must exceed {BAD_FRACTION})")
            if frac <= BAD_FRACTION:
                failures.append("the control did not fail")
    for f in failures:
        print("FAIL:", f)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
