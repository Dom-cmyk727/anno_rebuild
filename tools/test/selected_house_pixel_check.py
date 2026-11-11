#!/usr/bin/env python3
"""selected_house_pixel_check.py: W4-NEXT-2 K, row 18's selected-house panel, checked pixel by
pixel against the UI.1 oracle through the real anno1503.exe window - the same evidence-gap pattern
`ui_dynamic_draw_check.py` already closed for rows 5 and 10 (independent review of commit
0f232698, 2026-11-11: the committed tests for row 18 asserted only on title text and command
outcomes, never on the panel's actual pixels, so a silently broken or wrongly-positioned draw
would still pass every then-committed check).

    python -I tools/test/selected_house_pixel_check.py <anno1503 executable> [--renderer cpu|gpu]

Drives `tools/test/viewer_wiring/selected_house_draw_check.script` directly (the same dev-script
mechanism `ui_dynamic_draw_check.py` uses: `ANNO_DEV_SCRIPT`/`ANNO_DEV_SESSION`, not
`viewer_wiring.py`'s wrapper, so this stays a single, simple process) and compares its two
screenshots:
  - `selected_house_live`: `compose_screen`'s own `hud_idle` base, with one more `blit_rgba` of
    each of pionier_bk (974), house_portrait (1) and need_icon_0 (727) at their measured
    `data/ui/hud.txt` positions - the same two-step draw order `draw_live_screen_xrgb`/`_rgba`
    themselves use (`ui_mock.hpp`'s own doc comment).
  - `deselected_live`: `compose_screen`'s unmodified `hud_idle` base (no overrides) - the panel
    must be entirely absent once Escape deselects.

The deliberately wrong input: `selected_house_live` is also compared against the unmodified
`hud_idle` base (no panel) and must be REJECTED; `deselected_live` is also compared against the
WITH-panel image and must be REJECTED - proving this check is sensitive to the panel actually
being drawn (or not), not just agreeing with whatever the window drew.

`--renderer cpu` (default, the only CTest-registered mode) requires an exact match on opaque
oracle pixels, matching `ui_dynamic_draw_check.py`'s own bar; `--renderer gpu` is manual/
interactive (needs a real window surface).

Needs the original game files, Pillow (PIL) and a capture that only exists on Windows
(viewer.cpp's screenshot step: __WIN32__); skips (exit 77) otherwise.
Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HERE / "tools"))

SCRIPT = HERE / "tools" / "test" / "viewer_wiring" / "selected_house_draw_check.script"


def expected_image(layout_dir, game_dir, overrides):
    """compose_screen's own hud_idle base, with one more blit_rgba of each (element_name, tex_id)
    pair in `overrides` on top - the same draw order draw_live_screen_xrgb/rgba itself uses."""
    import ui_layout
    from PIL import Image

    base = ui_layout.compose_screen(layout_dir, "hud_idle", game_dir)
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
    """Only pixels inside `rects` where `expected` is opaque are checked - the same convention
    ui_dynamic_draw_check.py's own compare() uses, duplicated rather than imported for the same
    reason: this script's own negative control must not be confused with that script's screens."""
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

    with tempfile.TemporaryDirectory(prefix="anno_selected_house_pixel_") as session:
        session_dir = Path(session) / "session"
        # G3 (2026-11-11 fix review, finding 3): 240, matching viewer_wiring.py's own bound, not
        # the unjustified 30 F1 left in place - the script now waits on facts (`wait fact ...
        # within 5000`), and a slow or contended run should get the same margin every other
        # viewer_wiring-driven session gets, not a fifth of it.
        env = dict(os.environ, ANNO_DEV_SCRIPT=str(SCRIPT), ANNO_DEV_SESSION=str(session_dir),
                  ANNO_VIEW_QUIT_S="240")
        if renderer == "cpu":
            env["SDL_VIDEODRIVER"] = "dummy"
        else:
            env.pop("SDL_VIDEODRIVER", None)
        proc = subprocess.run(
            [exe, "--play", str(HERE / "data" / "maps" / "lan5.txt"), "--game-dir", game_dir,
             "--construction-menu", "on", "--layout", str(layout_dir), "--renderer", renderer],
            env=env, capture_output=True, text=True, timeout=260, check=False)
        if proc.returncode != 0:
            print(f"anno1503 exited {proc.returncode}\nstderr:\n{proc.stderr}")
            return 1

        channel_tolerance = 48 if renderer == "gpu" else 0
        failures = []
        rects = [rect("pionier_bk"), rect("house_portrait"), rect("need_icon_0")]
        with_panel = expected_image(str(layout_dir), game_dir,
                                    [("pionier_bk", 974), ("house_portrait", 1),
                                     ("need_icon_0", 727)])
        no_panel = expected_image(str(layout_dir), game_dir, [])

        png = session_dir / "selected_house_live.png"
        if not png.is_file():
            failures.append(f"selected_house_live: {png} was not captured")
        else:
            live = Image.open(png).convert("RGB")
            bad_wrong, checked_wrong, _ = compare(no_panel, live, rects, width, height,
                                                  channel_tolerance)
            if bad_wrong == 0:
                failures.append(
                    "selected_house_live: the deliberately wrong input (no panel at all) was "
                    "not rejected - the comparison is not sensitive to the panel's own pixels")
            bad, checked, first = compare(with_panel, live, rects, width, height,
                                          channel_tolerance)
            if bad:
                x, y, a, b = first
                failures.append(f"selected_house_live: {bad} of {checked} pixels differ; "
                                f"first at ({x},{y}): expected={a} live={b}")
            else:
                print(f"selected_house_live: exact match on {checked} opaque pixels "
                     f"(wrong-input control correctly rejected: {bad_wrong} of {checked_wrong})")

        png = session_dir / "deselected_live.png"
        if not png.is_file():
            failures.append(f"deselected_live: {png} was not captured")
        else:
            live = Image.open(png).convert("RGB")
            bad_wrong, checked_wrong, _ = compare(with_panel, live, rects, width, height,
                                                  channel_tolerance)
            if bad_wrong == 0:
                failures.append(
                    "deselected_live: the deliberately wrong input (the panel, still drawn) was "
                    "not rejected - the comparison cannot tell the panel is gone")
            bad, checked, first = compare(no_panel, live, rects, width, height, channel_tolerance)
            if bad:
                x, y, a, b = first
                failures.append(f"deselected_live: {bad} of {checked} pixels differ; "
                                f"first at ({x},{y}): expected={a} live={b}")
            else:
                print(f"deselected_live: exact match on {checked} opaque pixels "
                     f"(wrong-input control correctly rejected: {bad_wrong} of {checked_wrong})")

    if failures:
        print("FAILURES:")
        for f in failures:
            print(f"  {f}")
        return 1
    print("row 18's panel matches the oracle selected and absent, and the wrong-input controls "
         "were both rejected")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
