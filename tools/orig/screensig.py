"""Screen recognition by pixel signatures (T16, docs/testing/original-wine.md section 4 item 4).

A screen of the original (main menu, choose game, in game, ...) is recognised from a screenshot of
the game frame by difference hashes of a few regions:

  - each region is a box in FRACTIONS of the game frame, so the same signature works for the Wine
    runner (frame cropped out of the 1024x768 virtual desktop) and the Windows capture;
  - the region is reduced to (COLS+1) x ROWS grey pixels and every bit says "left brighter than
    right" (dHash); COLS*ROWS = 192 bits, stored as hex. A hash cannot be turned back into the
    picture, so the signature file holds no content of the original;
  - a screen matches when every region's Hamming distance is <= its max_dist; among matches the
    smallest mean distance wins. A frame that is almost uniformly dark is reported as "black"
    (intro cuts, loading) before any signature is tried.

Signatures are learned from a live screen (`learn`) and kept in tools/orig/screens_sig.json.
Needs Pillow (present in the AnnoHarness distro and on the owner's Windows Python).
"""

from __future__ import annotations

import json
import os

COLS, ROWS = 16, 12
BITS = COLS * ROWS
DEFAULT_MAX_DIST = 24          # of 192 bits; set per region when learning shows a better value
BLACK_MEAN = 12                # mean grey level (0..255) below which a frame counts as black

_HERE = os.path.dirname(os.path.abspath(__file__))
SIG_PATH = os.path.join(_HERE, "screens_sig.json")


def _crop(img, box):
    w, h = img.size
    x0, y0, x1, y1 = box
    return img.crop((round(x0 * w), round(y0 * h), round(x1 * w), round(y1 * h)))


def dhash(img, box=(0.0, 0.0, 1.0, 1.0)) -> str:
    from PIL import Image
    g = _crop(img, box).convert("L").resize((COLS + 1, ROWS), Image.BILINEAR)
    px = g.load()
    v = 0
    for y in range(ROWS):
        for x in range(COLS):
            v = (v << 1) | (1 if px[x, y] > px[x + 1, y] else 0)
    return f"{v:0{BITS // 4}x}"


def distance(a: str, b: str) -> int:
    return bin(int(a, 16) ^ int(b, 16)).count("1")


def mean_grey(img) -> float:
    g = img.convert("L").resize((64, 48))
    data = list(g.getdata())
    return sum(data) / len(data)


def load(path: str | None = None) -> dict:
    p = path or SIG_PATH
    if not os.path.exists(p):
        return {"screens": {}}
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def save(table: dict, path: str | None = None) -> None:
    with open(path or SIG_PATH, "w", encoding="utf-8", newline="\n") as f:
        json.dump(table, f, indent=1, sort_keys=True)
        f.write("\n")


def learn(img, regions=None, max_dist: int = DEFAULT_MAX_DIST) -> list[dict]:
    """Region records for the current screen (default: the whole frame)."""
    regions = regions or [[0.0, 0.0, 1.0, 1.0]]
    return [{"box": list(b), "dhash": dhash(img, b), "max_dist": max_dist} for b in regions]


def calibrate(table: dict, refs: dict) -> dict:
    """Set every region's max_dist from the reference frames: half the distance (rounded down,
    minus one) from its own hash to the same box on the nearest OTHER screen, capped at
    DEFAULT_MAX_DIST. refs = {screen name: PIL image of that screen's frame}. A pair of screens
    closer than 4 bits in some region cannot be told apart there and is reported."""
    report = {}
    for name, sig in table.get("screens", {}).items():
        for i, reg in enumerate(sig["regions"]):
            others = {o: distance(dhash(img, reg["box"]), reg["dhash"])
                      for o, img in refs.items() if o != name}
            if not others:
                continue
            nearest = min(others, key=others.get)
            reg["nearest"] = {"screen": nearest, "dist": others[nearest]}
            reg["max_dist"] = max(0, min(DEFAULT_MAX_DIST, others[nearest] // 2 - 1))
            report[f"{name}[{i}]"] = {"nearest": nearest, "dist": others[nearest], "max_dist": reg["max_dist"],
                                      "ambiguous": others[nearest] < 4}
    return report


def recognize(img, table: dict | None = None) -> dict:
    """{'screen': name | 'black' | 'unknown', 'scores': {name: mean distance or None}}."""
    table = table if table is not None else load()
    if mean_grey(img) < BLACK_MEAN:
        return {"screen": "black", "scores": {}}
    best, best_score, scores = "unknown", None, {}
    for name, sig in sorted(table.get("screens", {}).items()):
        dists = [distance(dhash(img, r["box"]), r["dhash"]) for r in sig["regions"]]
        ok = all(d <= r["max_dist"] for d, r in zip(dists, sig["regions"]))
        mean = sum(dists) / len(dists)
        scores[name] = {"mean": round(mean, 1), "match": ok}
        if ok and (best_score is None or mean < best_score):
            best, best_score = name, mean
    return {"screen": best, "scores": scores}
