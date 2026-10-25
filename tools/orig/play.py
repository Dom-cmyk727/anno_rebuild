"""Play layer of the original: put the cursor on a given tile (T15.B2, docs/re/view.md section 2.3).

`goto_tile(backend, x, y)` scrolls the original's view with the arrow keys until a tile is on screen,
predicts its pixel from the camera (`gamestate.camera`) and the measured projection (CALIB below),
moves the cursor there and corrects by the hovered tile until it equals (x, y). Input goes through
`backend` (WindowsBackend -> tools/orig/control.py with its foreground/cursor guard); memory is only
read. Standard library only.

Two stack locals of the game's main thread are read (view.md sections 1 and 2.1): the cursor position
`MOUSE` (two i32, client pixels) and the hovered tile `HOVER` (two u16). They are process addresses
of the run in which they were found (the same in the two runs so far); `check_addresses` verifies
them against the cursor before use and raises when they moved (re-find with tools/orig/memdiff.py as
in view.md section 4).
"""

from __future__ import annotations

import math
import struct
import time
from collections import Counter

import gamestate

MOUSE = 0x1AF950           # i32 x, i32 y: the game's cursor in client pixels
HOVER = 0x1ACFBC           # u16 x, u16 y: the tile under the cursor (flips with a sentinel, see hover)
FRAME = (1024, 768)        # the game's client size in a scenario (the harness copy runs 1024 x 768)
MAP_RIGHT = 778            # the map area is the left 0.76 of the frame (screens.json hud)
MARGIN = 40                # a tile closer than this to the map area's edge is scrolled in first

# Measured projection per zoom group (view.md section 2.3): half tile width A and half height B in
# pixels, and the screen point (CX, CY) the camera position projects to. Filled from the calibration.
CALIB: dict[int, dict[str, float]] = {
    0: {"A": 41.70, "B": 20.62, "CX": 389.0, "CY": 300.9},     # scale 1.0
    1: {"A": 20.84, "B": 10.31, "CX": 388.9, "CY": 333.8},     # scale 0.5
    2: {"A": 10.50, "B": 5.15, "CX": 390.4, "CY": 350.0},      # scale 0.25
}

# Rotation r: tile offset from the camera (dX, dZ) in terms of u = (px - CX) / A, v = (py - CY) / B.
ROT = {0: ((-.5, .5), (.5, .5)), 1: ((-.5, -.5), (-.5, .5)),
       2: ((.5, -.5), (-.5, -.5)), 3: ((.5, .5), (.5, -.5))}

ARROWS = {"RIGHT": (-1, 1), "LEFT": (1, -1), "UP": (-1, -1), "DOWN": (1, 1)}   # rotation 0, tiles per hold


class PlayError(RuntimeError):
    pass


def mouse(be) -> tuple[int, int]:
    b = be.read(MOUSE, 8)
    if not b:
        raise PlayError("mouse position unreadable")
    return struct.unpack("<ii", b)


def hover(be, n: int = 80, lim: int = 2048) -> tuple[int, int] | None:
    """The modal valid hovered pair of n quick reads (the slot flips between the real pair and a
    sentinel several thousand times a second, view.md section 2.1); None when no valid pair showed."""
    c: Counter = Counter()
    for _ in range(n):
        b = be.read(HOVER, 4)
        if b:
            x, y = struct.unpack("<HH", b)
            if x < lim and y < lim:
                c[(x, y)] += 1
    return c.most_common(1)[0][0] if c else None


def check_addresses(be) -> None:
    """Move the cursor and require MOUSE to follow it within 2 px; raises PlayError otherwise."""
    for nx, ny in ((0.30, 0.40), (0.40, 0.45)):
        be.move(nx, ny)
        time.sleep(0.15)
        mx, my = mouse(be)
        if abs(mx - nx * FRAME[0]) > 2 or abs(my - ny * FRAME[1]) > 2:
            raise PlayError(f"MOUSE {hex(MOUSE)} reads {(mx, my)}, not the cursor: re-find the stack locals")


def camera(be) -> dict:
    cam = be.state("camera")["camera"]
    if cam is None:
        raise PlayError("camera unavailable (no scenario, or the game window is not found)")
    return cam


def pixel_of(cam: dict, tx: float, tz: float, calib: dict | None = None) -> tuple[float, float]:
    """Screen pixel of the world point (tx, tz) in tiles (tile centre = tile + 0.5)."""
    c = calib or CALIB[cam["zoom_group"]]
    (a, b), (cc, d) = ROT[cam["rotation"]]
    dx, dz = tx - cam["x"], tz - cam["z"]
    det = a * d - b * cc
    u = (d * dx - b * dz) / det
    v = (-cc * dx + a * dz) / det
    return c["CX"] + u * c["A"], c["CY"] + v * c["B"]


def tile_at(cam: dict, px: float, py: float, calib: dict | None = None) -> tuple[int, int]:
    """Predicted hovered tile for a cursor pixel (valid on flat terrain; hills shift it by a few tiles)."""
    c = calib or CALIB[cam["zoom_group"]]
    (a, b), (cc, d) = ROT[cam["rotation"]]
    u, v = (px - c["CX"]) / c["A"], (py - c["CY"]) / c["B"]
    return math.floor(cam["x"] + a * u + b * v), math.floor(cam["z"] + cc * u + d * v)


def on_map(px: float, py: float) -> bool:
    return MARGIN <= px <= MAP_RIGHT - MARGIN and MARGIN <= py <= FRAME[1] - 2 * MARGIN


def goto_tile(be, x: int, y: int, timeout: float = 20.0, log=None) -> dict:
    """Put the cursor on tile (x, y). Returns {"pixel": (px, py), "tile": (x, y), "steps": n}; raises
    PlayError when the tile is not reached within `timeout` seconds."""
    t0 = time.monotonic()
    log = log if log is not None else (lambda *_: None)
    steps = 0
    # 1. scroll until the tile's predicted pixel is inside the map area
    while True:
        cam = camera(be)
        px, py = pixel_of(cam, x + 0.5, y + 0.5)
        if on_map(px, py):
            break
        if time.monotonic() - t0 > timeout:
            raise PlayError(f"timeout while scrolling to {(x, y)}; camera {cam['x']:.1f},{cam['z']:.1f}")
        # the arrow whose screen direction (rotation-corrected) best reduces the larger pixel error
        want = (px - FRAME[0] * 0.38 > 0, py - FRAME[1] * 0.40 > 0)
        horiz = abs(px - FRAME[0] * 0.38) / max(CALIB[cam["zoom_group"]]["A"], 1) >= \
            abs(py - FRAME[1] * 0.40) / max(CALIB[cam["zoom_group"]]["B"], 1)
        key = ("RIGHT" if want[0] else "LEFT") if horiz else ("DOWN" if want[1] else "UP")
        be.key(key, hold=0.2)
        time.sleep(0.25)
        steps += 1
        log("scroll", key, cam["x"], cam["z"])
    # 2. move onto the predicted pixel, then correct by the hovered tile
    gains = (1.0, 0.5, 1.5, 0.25, 2.0, 0.75)   # cycled when a correction repeats a tried pixel (hills)
    tried: set = set()
    last_err = None
    while True:
        cam = camera(be)
        px, py = pixel_of(cam, x + 0.5, y + 0.5) if last_err is None else (px, py)
        px, py = int(round(px)), int(round(py))
        tried.add((px, py))
        be.move(px / FRAME[0], py / FRAME[1])
        time.sleep(0.12)
        mx, my = mouse(be)
        h = hover(be)
        steps += 1
        log("probe", (px, py), (mx, my), h)
        if h == (x, y):
            return {"pixel": (mx, my), "tile": h, "steps": steps}
        if time.monotonic() - t0 > timeout:
            raise PlayError(f"timeout near {(x, y)}: hovered {h} at pixel {(mx, my)}")
        if h is None:
            raise PlayError(f"no valid hovered tile at pixel {(mx, my)} while going to {(x, y)}")
        # one-tile correction: the pixel where the model puts (x, y) relative to where it puts h
        ex, ey = pixel_of(cam, x + 0.5, y + 0.5)
        hx, hy = pixel_of(cam, h[0] + 0.5, h[1] + 0.5)
        k = 0
        while True:
            gain = gains[k % len(gains)]
            cand = (int(round(mx + (ex - hx) * gain)), int(round(my + (ey - hy) * gain)))
            if cand not in tried or k > 3 * len(gains):
                break
            k += 1
        px, py = cand
        last_err = (x - h[0], y - h[1])
