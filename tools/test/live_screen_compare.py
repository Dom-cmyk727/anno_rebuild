#!/usr/bin/env python3
"""live_screen_compare.py: the construction menu's live-window drawing (W4-UI-2 H1/C1 second
slice, viewer.cpp's draw_cpu -> draw_live_screen_xrgb, and draw_gpu -> draw_live_screen_rgba)
checked against the UI.1 Python oracle's own mock (tools/ui_layout.py compose_screen), through
the real anno1503.exe window - not the standalone anno_ui_mock CLI tools/test/ui_mock_compare.py
already checks.

    python -I tools/test/live_screen_compare.py <anno1503 executable> [--renderer cpu|gpu]

The live window also draws the real game world under/around the UI chrome, which the oracle's own
canvas (all black outside a drawn element) never represents - so this is not a whole-frame exact
match (tools/test/ui_mock_compare.py's own comparison, between two black-background composers,
stays a true exact match). Here, only pixels where the oracle painted something *opaque* (not its
black background) are compared: the live window must show exactly that pixel there too, whatever
is drawn further out, over the game world, which is the oracle's own frame does not model. For the
construction grid, drawn-state uses the sim's live construction_eligibility, not
construction.txt's own recorded state (C2) - a fresh Citizen start matches both (verified
separately, 2026-11-07's C2 check, 0 of 148 mismatches), so the two still agree pixel for pixel
here.

`--renderer cpu` (the default) requires an exact match (ADR 0004 "Rendering": no float-rounding
source for UI sprites on that path, and this checkpoint's own measurement bears that out - 0 of
N). `--renderer gpu` uses the project's own established F2.1 tolerance instead
(tools/ui_layout.py's COMPARE_CHANNEL=48 per channel, COMPARE_BAD_FRACTION=0.08 of the checked
pixels) rather than bit-exact equality: a live GPU run found 54 of roughly 200,000+ checked pixels
differing by 9-12 per channel, all at mainuntergrund's own bottom-right corner pixel (1023, 761) -
a pre-existing CPU-vs-GPU world-render edge difference the UI chrome's own corner pixel is not
fully opaque at, not something the UI overlay composition itself gets wrong (every other checked
pixel, including every gold/goods/residents digit and the whole grid, is bit-exact). Comfortably
inside the tolerance (max diff 12 of 48; 54 of 200,000+ is far under the 8% fraction) - this is the
packet's own "within its tolerance" criterion, not a loosened bar invented for this script.

Two launches of anno1503.exe (UI-WINDOW-1 fix round R5), each with the same --renderer handling:
  1. `--play data/maps/lan5.txt` with construction_menu_draw_check.script: construction_public and
     construction_roads. lan5 has the developer city, so the status bar shows its material and
     resident counters, which those two oracle shots draw too.
  2. `--ship-start data/maps/candidates/ship-start-v2/lan5-ship-a.json --seat 0` with
     live_screen_hud_idle.script: hud_idle. The oracle shot hud_idle_running is taken before the
     first founding, when the status bar shows no counters (owner rule R4, docs/re/ui.md 5.2:
     counters only once the player has a city). On lan5 the counters are drawn and hud_idle
     differs in the status bar, which is why the idle HUD is compared on the ship start. No masks
     are added for the counter icons; the comparison stays exact.

Needs the original game files, Pillow (PIL) and a capture that only exists on Windows (viewer.cpp
screenshot step: __WIN32__); skips (exit 77) otherwise.
Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HERE / "tools"))

SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "construction_menu_draw_check.script"
LAN5 = HERE / "data" / "maps" / "lan5.txt"
# UI-WINDOW-1 fix round R5: the idle HUD is taken on the ship start, before any city exists.
HUD_IDLE_SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "live_screen_hud_idle.script"
SHIP_START = HERE / "data" / "maps" / "candidates" / "ship-start-v2" / "lan5-ship-a.json"

# Element names compose_screen actually draws for each screen kind (its own blit_el call list,
# docs/re/ui.md section 2.8) - never every element in hud.txt, most of which belong to other
# screens (the info panel, the pause overlay, the rating screen, ...).
HUD_NAMES = [
    "weltkarte", "mainuntergrund", "tab_construction", "tab_military", "tab_help", "tab_options",
    "statusbar", "gold", "fragezeichen", "rosh_0", "rosh_1", "rosh_2", "zoom_plus",
    "zoom_minus", "lupe", "compass_w", "compass_e",
]
# UI-STATUS-1 B: the fertility row's slots (data/ui/hud.txt rosh_<k>). The oracle draws the NORD row
# 6, 8, 11 every GOG shot shows; the live window draws the row of the island under the cursor
# (docs/re/ui.md 5.6). On lan5 the scripts hover the developer city's NORD island first, so the two
# agree; on the ship start no island is discovered before the first founding (the rebuild has no
# ship reveal, OQ-701), the row is hidden, and its slots are left out of the hud_idle comparison.
FERTILITY_SLOTS = ["rosh_0", "rosh_1", "rosh_2"]
CONSTRUCTION_EXTRA_NAMES = [
    "baumat_tools", "baumat_wood", "baumat_bricks", "baumat_marble", "einwohner", "plankegross",
    "abrissicon",
]


def rects_for(hud, names):
    by_name = {el["name"]: el for el in hud["elements"]}
    out = []
    for name in names:
        el = by_name.get(name)
        if el and el.get("pos") and el.get("size"):
            x, y = el["pos"]
            w, h = el["size"]
            out.append((x, y, w, h, name))
    return out


def grid_rects(con, category):
    g = con["grid"]
    ox, oy, stride, sprite = (int(g["origin_x"]), int(g["origin_y"]), int(g["cell_px"]),
                              int(g["sprite"]))
    out = []
    for cell in con["cells"]:
        if cell["category"] != category:
            continue
        x = ox + int(cell["col"]) * stride
        y = oy + int(cell["row"]) * stride
        out.append((x, y, sprite, sprite, "cell_%s_%s" % (cell["col"], cell["row"])))
    return out


def compare(oracle, live, rects, width, height, masked, channel_tolerance=0):
    """Only where `oracle` painted something opaque (not its all-black background) AND the pixel
    is not in `masked` (data/ui/masks.txt's own live-content rectangles - the gold/goods digits
    this checkpoint started drawing live sit inside the "statusbar" element's own rectangle, which
    the oracle paints as opaque chrome; masks.txt is the project's own established exclusion for
    exactly this, docs/re/ui.md's "Masks" paragraph, not a new rule invented for this script): the
    live capture must match within `channel_tolerance` per RGB channel (0: exact, the CPU
    renderer's own target; a positive value: the F2.1 convention, tools/ui_layout.py's own
    COMPARE_CHANNEL, for the GPU renderer's own allowed tolerance). Returns (bad, checked,
    first_diff, bad_by_name)."""
    o, l = oracle.load(), live.load()
    bad = checked = 0
    first = None
    bad_by_name = []
    for x0, y0, w, h, name in rects:
        el_bad = 0
        for y in range(max(0, y0), min(height, y0 + h)):
            for x in range(max(0, x0), min(width, x0 + w)):
                if (x, y) in masked:
                    continue
                a = o[x, y]
                if a == (0, 0, 0):
                    continue
                checked += 1
                b = l[x, y]
                if max(abs(a[c] - b[c]) for c in range(3)) > channel_tolerance:
                    bad += 1
                    el_bad += 1
                    if first is None:
                        first = (name, x, y, a, b)
        if el_bad:
            bad_by_name.append((name, el_bad))
    return bad, checked, first, bad_by_name


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
    import ui_layout

    layout_dir = HERE / "data" / "ui"
    hud = ui_layout.parse_hud(str(layout_dir / "hud.txt"))
    con = ui_layout.parse_construction(str(layout_dir / "construction.txt"))
    width, height = hud["resolution"]
    masked = ui_layout.load_mask_set(str(layout_dir))

    # GPU needs a real, visible window (a Win32 surface to request a compatible adapter against -
    # confirmed by hand: --renderer gpu under SDL_VIDEODRIVER=dummy exits "no Win32 window
    # handle", never falling back silently). This matches viewer_wiring.py's own established
    # --gpu flag (unused by any registered CTest case so far, for exactly this reason): a GPU
    # check here is a manual/interactive-desktop verification, like bench_frame, not something
    # CI's headless environment can run. --renderer cpu (the default) keeps the dummy driver and
    # is the only mode registered as a CTest case.
    with tempfile.TemporaryDirectory(prefix="anno_live_screen_") as session:
        def launch(script, start_args, name):
            # One window session with the same --renderer handling; returns its screenshot folder.
            out = Path(session) / name  # must not exist yet (main.cpp's own check)
            env = dict(os.environ, ANNO_DEV_SCRIPT=str(script), ANNO_DEV_SESSION=str(out),
                       ANNO_VIEW_QUIT_S="30")
            if renderer == "cpu":
                env["SDL_VIDEODRIVER"] = "dummy"
            else:
                env.pop("SDL_VIDEODRIVER", None)
            proc = subprocess.run(
                [exe, *start_args, "--game-dir", game_dir, "--construction-menu", "on",
                 "--layout", str(layout_dir), "--renderer", renderer],
                env=env, capture_output=True, text=True, timeout=120, check=False)
            if proc.returncode != 0:
                print(f"anno1503 ({name}) exited {proc.returncode}\nstderr:\n{proc.stderr}")
                return None
            return out

        # Launch 1, lan5 (the developer city): the two construction screens. Their oracle shots
        # draw the status bar's material and resident icons, which a city shows (R4 rule).
        session_dir = launch(SCRIPT, ["--play", str(LAN5)], "lan5")
        # Launch 2, the ship start (no city yet): the idle HUD, the state the oracle shot
        # hud_idle_running shows (no counters before the first founding, docs/re/ui.md 5.2).
        idle_dir = launch(HUD_IDLE_SCRIPT, ["--ship-start", str(SHIP_START), "--seat", "0"],
                          "ship_start")
        if session_dir is None or idle_dir is None:
            return 1

        failures = []
        idle_names = [n for n in HUD_NAMES if n not in FERTILITY_SLOTS]
        idle_masked = masked | ui_layout.load_mask_set(
            str(layout_dir), [(x, y, w, h) for x, y, w, h, _ in rects_for(hud, FERTILITY_SLOTS)])
        checks = [
            ("hud_idle", idle_dir / "hud_idle_live.png", rects_for(hud, idle_names), idle_masked),
            ("construction_public",
             session_dir / "construction_public_live.png",
             rects_for(hud, HUD_NAMES + CONSTRUCTION_EXTRA_NAMES) + grid_rects(con, "public"),
             masked),
            # Category-switching's own screenshot proof (W4-UI-2): a second category's grid draws
            # correctly too, not only the menu's default-open "public" - the script's click_px
            # exercises ConstructionMenu::click's SwitchCategory path for real, live.
            ("construction_roads",
             session_dir / "construction_roads_live.png",
             rects_for(hud, HUD_NAMES + CONSTRUCTION_EXTRA_NAMES) + grid_rects(con, "roads"),
             masked),
        ]
        for screen, png, rects, screen_masked in checks:
            if not png.is_file():
                failures.append(f"{screen}: {png} was not captured")
                continue
            oracle = ui_layout.compose_screen(str(layout_dir), screen, game_dir)
            live = Image.open(png).convert("RGB")
            if live.size != (width, height):
                failures.append(f"{screen}: captured size {live.size} != {(width, height)}")
                continue
            # F2.1's own convention (tools/ui_layout.py COMPARE_CHANNEL/COMPARE_BAD_FRACTION):
            # the GPU renderer is allowed up to channel_tolerance per channel and up to 8% of the
            # checked pixels bad; the CPU renderer's own target stays bit-exact (tolerance 0).
            channel_tolerance = 48 if renderer == "gpu" else 0
            bad, checked, first, bad_by_name = compare(oracle, live, rects, width, height,
                                                       screen_masked, channel_tolerance)
            bad_fraction = (bad / checked) if checked else 0.0
            if bad and (renderer == "cpu" or bad_fraction > 0.08):
                x, y, a, b = first[1], first[2], first[3], first[4]
                failures.append(
                    f"{screen}: {bad} of {checked} opaque-oracle pixels differ (fraction "
                    f"{bad_fraction:.4f}); first at {first[0]} ({x},{y}): oracle={a} live={b}; "
                    f"elements: {bad_by_name}")
            elif bad:
                print(f"{screen}: {bad} of {checked} pixels outside tolerance "
                     f"(fraction {bad_fraction:.4f}, within the 0.08 F2.1 bound); elements: "
                     f"{bad_by_name}")
            else:
                print(f"{screen}: exact match on {checked} opaque-oracle pixels")

    if failures:
        print("FAILURES:")
        for f in failures:
            print(f"  {f}")
        return 1
    print("live window drawing matches the oracle on every checked screen")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
