#!/usr/bin/env python3
"""ui_dynamic_draw_check.py: W4-NEXT-1 part U rows 5 (demolish mode's pickaxe frame) and 10 (the
pause overlay), checked against the UI.1 oracle through the real anno1503.exe window - the
orchestrator's interim review (2026-11-10, finding 6) found neither row had this: the packet
requires, for every screen, CPU drawing checked against the oracle with a deliberately wrong
input the check rejects (plus GPU and bench_frame, covered manually/elsewhere - see below).

    python -I tools/test/ui_dynamic_draw_check.py <anno1503 executable> [--renderer cpu|gpu]

`tools/ui_layout.py compose_screen` has no concept of either dynamic state (it always blits the
pickaxe's idle frame, id 659, and never draws the pause overlay at all - it composes one of the
UI.1 oracle's nine *named*, static screens). So the "oracle" compared against here is built the
same way `ui_mock.cpp`'s own live-draw functions build it: `compose_screen`'s own static base,
then one more `blit_rgba` of the exact sprite the C++ code also draws on top (the pickaxe's
active frame, id 660, over its own measured rectangle for row 5; `pause_parchment` then
`pause_button` over theirs for row 10) - the same decoded texture files, the same measured
positions from `data/ui/hud.txt`, not a second, independent description of the chrome.

The deliberately wrong input: for each row, an "expected" image built from the *other* frame (row
5: still id 659, the idle pickaxe; row 10: the oracle's unmodified base, no parchment at all) is
compared against the same live capture and must be REJECTED (found different) - proving this
script's own comparison is sensitive to the thing it is supposed to check, not just agreeing with
whatever the live window drew.

`--renderer cpu` (the default, the only mode this is registered as a CTest case for) requires an
exact match, matching `live_screen_compare.py`'s own established CPU bar; `--renderer gpu` is a
manual/interactive check (needs a real window surface, docs/testing/ and that script's own doc
comment explain why GPU checks are not run under CTest here).

bench_frame numbers for these two flags are not produced by this script: `bench_frame` has no
concept of the construction-menu/HUD overlay at all (it benchmarks the world scene only, same
note `live_screen_compare.py`'s own W4-UI-2 history already made) - see the REPORTS.md entry
alongside this script for the actual before/after numbers, measured separately with
`anno_ui_mock`/the window's own frame timing the same way W4-UI-2's V6 did.

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

SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "ui_dynamic_draw_check.script"


def expected_image(layout_dir, screen, game_dir, overrides):
    """compose_screen's own base for `screen`, with one more `blit_rgba` of each
    (element_name, tex_id) pair in `overrides` on top - the same two-step draw order
    draw_live_screen_xrgb/rgba itself uses for a flag compose_screen does not model."""
    import ui_layout
    from PIL import Image

    base = ui_layout.compose_screen(layout_dir, screen, game_dir)
    rgb = bytearray(base.tobytes())
    w, h = base.size
    hud = ui_layout.parse_hud(os.path.join(layout_dir, "hud.txt"))
    by_name = {el["name"]: el for el in hud["elements"]}
    for name, tex_id in overrides:
        el = by_name[name]
        sprite = ui_layout.decode_tex_rgba(
            os.path.join(game_dir, el["file"].replace("/", os.sep)), tex_id)
        if sprite is not None:
            ui_layout.blit_rgba(rgb, w, h, sprite, el["pos"][0], el["pos"][1])
    return Image.frombytes("RGB", (w, h), bytes(rgb))


def compare(expected, live, rects, width, height, channel_tolerance=0):
    """Only pixels inside `rects` where `expected` is opaque (not compose_screen's all-black
    background) are checked - live_screen_compare.py's own `compare()` convention, duplicated
    rather than imported so this script's own negative control (comparing the same rects against
    a deliberately wrong `expected`) cannot be confused with that script's unrelated screens."""
    e, l = expected.load(), live.load()
    bad = checked = 0
    first = None
    for x0, y0, w, h in rects:
        for y in range(max(0, y0), min(height, y0 + h)):
            for x in range(max(0, x0), min(width, x0 + w)):
                a = e[x, y]
                if a == (0, 0, 0):
                    continue
                checked += 1
                b = l[x, y]
                if max(abs(a[c] - b[c]) for c in range(3)) > channel_tolerance:
                    bad += 1
                    if first is None:
                        first = (x, y, a, b)
    return bad, checked, first


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
    width, height = hud["resolution"]
    by_name = {el["name"]: el for el in hud["elements"]}

    def rect(name):
        el = by_name[name]
        x, y = el["pos"]
        w, h = el["size"]
        return (x, y, w, h)

    with tempfile.TemporaryDirectory(prefix="anno_ui_dynamic_") as session:
        session_dir = Path(session) / "session"
        env = dict(os.environ, ANNO_DEV_SCRIPT=str(SCRIPT), ANNO_DEV_SESSION=str(session_dir),
                  ANNO_VIEW_QUIT_S="30")
        if renderer == "cpu":
            env["SDL_VIDEODRIVER"] = "dummy"
        else:
            env.pop("SDL_VIDEODRIVER", None)
        proc = subprocess.run(
            [exe, "--play", str(HERE / "data" / "maps" / "lan5.txt"), "--game-dir", game_dir,
             "--construction-menu", "on", "--layout", str(layout_dir), "--renderer", renderer],
            env=env, capture_output=True, text=True, timeout=120, check=False)
        if proc.returncode != 0:
            print(f"anno1503 exited {proc.returncode}\nstderr:\n{proc.stderr}")
            return 1

        channel_tolerance = 48 if renderer == "gpu" else 0
        failures = []

        # Row 5: the pickaxe's active frame (660) over construction_public's own base.
        png = session_dir / "construction_demolish_live.png"
        if not png.is_file():
            failures.append(f"construction_demolish: {png} was not captured")
        else:
            live = Image.open(png).convert("RGB")
            correct = expected_image(str(layout_dir), "construction_public", game_dir,
                                     [("abrissicon", 660)])
            wrong = expected_image(str(layout_dir), "construction_public", game_dir,
                                   [("abrissicon", 659)])  # the deliberately wrong input
            rects = [rect("abrissicon")]
            bad_wrong, checked_wrong, _ = compare(wrong, live, rects, width, height,
                                                  channel_tolerance)
            if bad_wrong == 0:
                failures.append(
                    "construction_demolish: the deliberately wrong input (idle frame 659) was "
                    "not rejected - the comparison is not sensitive to the pickaxe's own frame")
            bad, checked, first = compare(correct, live, rects, width, height, channel_tolerance)
            if bad:
                x, y, a, b = first
                failures.append(f"construction_demolish: {bad} of {checked} pixels differ; "
                                f"first at ({x},{y}): expected={a} live={b}")
            else:
                print(f"construction_demolish: exact match on {checked} opaque pixels "
                     f"(wrong-input control correctly rejected: {bad_wrong} of {checked_wrong})")

        # Row 10: the pause parchment and its button over hud_idle's own base.
        png = session_dir / "hud_paused_live.png"
        if not png.is_file():
            failures.append(f"hud_paused: {png} was not captured")
        else:
            live = Image.open(png).convert("RGB")
            correct = expected_image(str(layout_dir), "hud_idle", game_dir,
                                     [("pause_parchment", 1512), ("pause_button", 1513)])
            wrong = expected_image(str(layout_dir), "hud_idle", game_dir, [])  # no overlay at all
            rects = [rect("pause_parchment"), rect("pause_button")]
            bad_wrong, checked_wrong, _ = compare(wrong, live, rects, width, height,
                                                  channel_tolerance)
            if bad_wrong == 0:
                failures.append(
                    "hud_paused: the deliberately wrong input (no pause overlay) was not "
                    "rejected - the comparison is not sensitive to the pause chrome")
            bad, checked, first = compare(correct, live, rects, width, height, channel_tolerance)
            if bad:
                x, y, a, b = first
                failures.append(f"hud_paused: {bad} of {checked} pixels differ; "
                                f"first at ({x},{y}): expected={a} live={b}")
            else:
                print(f"hud_paused: exact match on {checked} opaque pixels "
                     f"(wrong-input control correctly rejected: {bad_wrong} of {checked_wrong})")

    if failures:
        print("FAILURES:")
        for f in failures:
            print(f"  {f}")
        return 1
    print("rows 5 and 10 match the oracle, and the wrong-input controls were both rejected")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
