#!/usr/bin/env python3
"""ui_balance_rows_compare.py: the balance pages' figure rows (UI-TABS-1 stage B, docs/re/ui.md 5.5)
against the original's GOG shots, and the window's CPU path against the GPU layer's composer.

    python -I tools/test/ui_balance_rows_compare.py <anno_ui_mock executable> <anno1503 executable>

1. The GPU path's composer (`anno_ui_mock --live 1024x768 --screen <page> --rows ...`, which is
   draw_live_screen_rgba, the layer the GPU overlay uploads) with the figures each GOG shot shows:
   the overview on hud_idle_running (0, 0, 15, 0, 0, 16, -31), the city page on f15_kontor and
   r2_tab_help (0, 0, 15, 0, 0, 0, -15, 0). The text pixels are the pixels the rows change against
   the same screen drawn without figures; every one must equal the shot exactly after the shot's
   RGB565 reduction (tools/ui_layout.py rgb565_shown), and every row must change some.
   Negative controls, each of which must fail: a wrong figure (16 drawn as 15), the text one row
   lower (`text_dy -2`), the balance without its red (`negative_rgb` removed) and the text not cut
   to its gadget (`clip box` removed), the last three on a copy of data/ui changed in one field.
2. The CPU path (the live window, draw_cpu -> draw_live_screen_xrgb; viewer_wiring script
   ui_balance_rows.script on lan5 with `--rules living`, which asserts the figures as facts)
   against the composer drawn with those same figures: every row rectangle pixel-identical.

Reads ANNO1503_GAME_DIR and ANNO_UI_ORACLE_SHOTS (the GOG shots, never in git); skips (77) without
them, without Pillow, or off Windows (the window's screenshot step is Windows-only).
Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HERE / "tools"))

PLAYER_GOG = [0, 0, 15, 0, 0, 16, -31]
CITY_GOG = [0, 0, 15, 0, 0, 0, -15, 0]
# (anno_ui_mock screen, GOG shot, row prefix, figures)
SHOTS = [("hud_idle", "hud_idle_running", "player", PLAYER_GOG),
         ("info", "f15_kontor", "city", CITY_GOG),
         ("info", "r2_tab_help", "city", CITY_GOG)]
SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "ui_balance_rows.script"
LAN5 = HERE / "data" / "maps" / "lan5.txt"
# The window's screenshots and the screen each shows.
LIVE = [("hud_idle", "balance_player", "player"), ("info", "balance_city", "city")]


def rows_arg(prefix, figures):
    return ",".join(f"{prefix}.{k}={v}" for k, v in enumerate(figures))


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

    def row_rects(prefix):
        out = {}
        for el in hud["elements"]:
            v = el.get("value", "")
            if isinstance(v, str) and v.startswith(prefix + "."):
                out[v] = (el["pos"][0], el["pos"][1], el["size"][0], el["size"][1])
        return out

    failures = []
    with tempfile.TemporaryDirectory(prefix="anno_ui_balance_") as td:
        cache = {}

        def draw(screen, rows="", layout=layout_dir):
            key = (screen, rows, str(layout))
            if key not in cache:
                out = Path(td) / f"draw_{len(cache)}.png"
                cmd = [str(mock), "--layout", str(layout), "--game-dir", game, "--screen", screen,
                       "--out", str(out), "--live", "1024x768"]
                if rows:
                    cmd += ["--rows", rows]
                r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
                if r.returncode != 0:
                    failures.append(f"{screen} {rows}: anno_ui_mock exit {r.returncode}: {r.stderr}")
                    cache[key] = None
                else:
                    cache[key] = Image.open(out).convert("RGB")
            return cache[key]

        def against_shot(screen, shot, prefix, figures, layout=layout_dir):
            """Per row: (text pixels, pixels that differ from the shot)."""
            ours = draw(screen, rows_arg(prefix, figures), layout)
            bare = draw(screen, "", layout)
            if ours is None or bare is None:
                return None
            o, b, s = ours.load(), bare.load(), shot.load()
            per = {}
            for name, (x0, y0, w, h) in row_rects(prefix).items():
                text = bad = 0
                # The text may reach outside the gadget when it is not cut (the control).
                for y in range(y0 - 5, y0 + h + 5):
                    for x in range(x0 - 5, x0 + w + 1):
                        if o[x, y] == b[x, y]:
                            continue
                        text += 1
                        if ui_layout.rgb565_shown(o[x, y]) != s[x, y]:
                            bad += 1
                per[name] = (text, bad)
            return per

        # 1. The composer against the GOG shots.
        loaded = {}
        for screen, shot_name, prefix, figures in SHOTS:
            path = Path(shots) / f"{shot_name}.png"
            if not path.is_file():
                failures.append(f"{path} is missing")
                continue
            loaded[shot_name] = Image.open(path).convert("RGB")
            per = against_shot(screen, loaded[shot_name], prefix, figures)
            if per is None:
                continue
            print(f"{screen} vs {shot_name}: " + ", ".join(
                f"{n} {b}/{t}" for n, (t, b) in sorted(per.items())))
            for name, (text, bad) in per.items():
                if text == 0 or bad:
                    failures.append(f"{screen} vs {shot_name}: {name} {bad} of {text} text px differ")

        # Negative controls: each must make at least one row fail.
        def control(label, screen, shot_name, prefix, figures, layout=layout_dir, want=None):
            if shot_name not in loaded:
                return
            per = against_shot(screen, loaded[shot_name], prefix, figures, layout)
            if per is None:
                return
            failing = sorted(n for n, (t, b) in per.items() if t == 0 or b)
            print(f"control {label}: fails on {failing}")
            if not failing or (want is not None and failing != want):
                failures.append(f"control {label} failed on {failing}, expected {want or 'some'}")

        wrong = list(PLAYER_GOG)
        wrong[5] = 15
        control("16 drawn as 15", "hud_idle", "hud_idle_running", "player", wrong,
                want=["player.5"])

        def changed_layout(tag, edit):
            d = Path(td) / f"layout_{tag}"
            shutil.copytree(layout_dir, d)
            p = d / "hud.txt"
            text = p.read_text(encoding="ascii")
            new = edit(text)
            if new == text:
                failures.append(f"control {tag}: the edit changed nothing")
            p.write_text(new, encoding="ascii")
            return d

        lower = changed_layout("dy", lambda t: t.replace(" text_dy -3 ", " text_dy -2 "))
        control("text one row lower", "info", "f15_kontor", "city", CITY_GOG, lower)
        white = changed_layout("red", lambda t: t.replace(" negative_rgb ff0000", ""))
        control("balance not red", "info", "f15_kontor", "city", CITY_GOG, white, ["city.6"])
        uncut = changed_layout("clip", lambda t: t.replace(" clip box ", " "))
        control("text not cut", "hud_idle", "hud_idle_running", "player", PLAYER_GOG, uncut,
                ["player.2", "player.5", "player.6"])

        # 2. The live window's CPU path against the composer, exact over every row rectangle.
        facts = dict(re.findall(r"^expect fact hud\.row\.(\S+) == (-?\d+)\s*$",
                                SCRIPT.read_text(encoding="ascii"), re.M))
        out = Path(td) / "session"  # must not exist yet (main.cpp's own check)
        env = dict(os.environ, ANNO_DEV_SCRIPT=str(SCRIPT), ANNO_DEV_SESSION=str(out),
                   ANNO_VIEW_QUIT_S="30", SDL_VIDEODRIVER="dummy")
        proc = subprocess.run(
            [str(exe), "--play", str(LAN5), "--rules", "living", "--game-dir", game,
             "--construction-menu", "on", "--layout", str(layout_dir), "--renderer", "cpu"],
            env=env, capture_output=True, text=True, timeout=120, check=False)
        log = out / "script.log"
        if proc.returncode != 0:
            failures.append(f"anno1503 exited {proc.returncode}: {proc.stderr[-2000:]}")
        elif log.is_file() and "FAIL" in log.read_text(encoding="utf-8", errors="replace"):
            failures.append(f"ui_balance_rows.script failed: {log.read_text(errors='replace')}")
        else:
            for screen, png, prefix in LIVE:
                path = out / f"{png}.png"
                rows = ",".join(f"{k}={v}" for k, v in sorted(facts.items())
                                if k.startswith(prefix + "."))
                ours = draw(screen, rows)
                if ours is None:
                    continue
                if not path.is_file():
                    failures.append(f"{screen}: {path} was not captured")
                    continue
                win = Image.open(path).convert("RGB")
                o, w_ = ours.load(), win.load()
                checked = bad = 0
                for x0, y0, w, h in row_rects(prefix).values():
                    for y in range(y0, y0 + h):
                        for x in range(x0, x0 + w):
                            checked += 1
                            if o[x, y] != w_[x, y]:
                                bad += 1
                print(f"{screen}: window (CPU) vs composer ({rows}): {bad} of {checked} px differ")
                if bad or not checked:
                    failures.append(f"{screen}: window vs composer {bad} of {checked} px differ")
    for f in failures:
        print("FAIL:", f)
    if not failures:
        print("every balance row matches")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
