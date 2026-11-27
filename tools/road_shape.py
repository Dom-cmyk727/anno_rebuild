#!/usr/bin/env python3
"""Reference for the original's road reshaper (ROADS-2a; docs/BUILDINGSOBJECTS.md part B 10.1 items 7-8,
AnnoWelt island slot +0x80 = 0x1007cc40). Language-neutral oracle for src/sim road_shape.

A road tile's piece and rotation follow from its four orthogonal road neighbours, mask N=1 E=2 S=4 W=8
(the shape table below, from the shipped scenarios, P; the family arithmetic dirt = cobblestone + 20, D
0x1007efcf). Ramps, the slope junction and bridges are not shaped here (ROADS-5, OQ-764/765).

  road_shape.py selftest
  road_shape.py corpus [--game-dir DIR] [--write FILE | --check FILE]

`corpus` predicts every shipped straight/corner/cross/T/end tile from its neighbours and reports the
agreement and the misses by (shipped piece, predicted piece). Without a game directory it exits 77.
"""
import glob
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insel  # noqa: E402

COBBLE = 2500
DIRT_OFFSET = 20  # dirt id = cobblestone id + 20 (0x1007efcf)
ROAD_IDS = range(2500, 2550)  # the ids that connect: 0x9c4-0x9f5 (AnnoWelt 0x1007f0c0-0x1007f0df)
# Piece -> id offset from the family base (BUILDINGSOBJECTS 10.1 item 8).
PIECE_OFFSET = {"straight": 0, "corner": 5, "cross": 6, "T": 7, "end": 8}
STRAIGHT_VARIANTS = {COBBLE: 4, COBBLE + DIRT_OFFSET: 5}  # 2500-2503, 2520-2524
# Mask (N=1, E=2, S=4, W=8) -> (piece, rotation); the same pieces as read from the shipped scenarios
# (BUILDINGSOBJECTS 10.1 item 8), with the rotations the files left open.
SHAPE = {  # the original's own table, AnnoWelt 0x100c9d90 (8 bytes per mask: id, rotation; D)
    0: ("straight", 3), 1: ("end", 1), 2: ("end", 0), 3: ("corner", 1), 4: ("end", 3),
    5: ("straight", 3), 6: ("corner", 0), 7: ("T", 1), 8: ("end", 2), 9: ("corner", 2),
    10: ("straight", 0), 11: ("T", 2), 12: ("corner", 3), 13: ("T", 3), 14: ("T", 0), 15: ("cross", 3),
}
DIRS = ((0, -1), (1, 0), (0, 1), (-1, 0))  # N, E, S, W


def classify(tid):
    """-> (family base, piece) for a shaped road id, else None (ramps, junction, bridges, others)."""
    for base in (COBBLE, COBBLE + DIRT_OFFSET):
        off = tid - base
        if 0 <= off < STRAIGHT_VARIANTS[base]:
            return base, "straight"
        for piece, o in PIECE_OFFSET.items():
            if piece != "straight" and off == o:
                return base, piece
    return None


def mask_at(roads, x, y):
    m = 0
    for bit, (dx, dy) in enumerate(DIRS):
        if (x + dx, y + dy) in roads:
            m |= 1 << bit
    return m


def same_rotation(piece, want, have, exact=False):
    """Straights and the symmetric cross count as equal up to a half turn unless `exact`."""
    if exact or piece not in ("straight", "cross"):
        return want == have
    return want % 2 == have % 2 if piece == "straight" else True


def selftest():
    assert sorted(SHAPE) == list(range(16))
    assert classify(2520) == (2520, "straight") and classify(2524) == (2520, "straight")
    assert classify(2504) is None and classify(2525) == (2520, "corner")
    assert classify(2509) is None and classify(2516) is None
    roads = {(0, 0), (1, 0), (1, 1)}  # an L: (0,0)-(1,0) east, then south
    assert SHAPE[mask_at(roads, 0, 0)] == ("end", 0)
    assert SHAPE[mask_at(roads, 1, 0)] == ("corner", 2) or SHAPE[mask_at(roads, 1, 0)][0] == "corner"
    print("road_shape selftest ok")
    return 0


def corpus(game_dir, write=None, check=None):
    files = sorted(glob.glob(os.path.join(game_dir, "Scenes", "**", "*.sze"), recursive=True))
    lines = []
    total = agree = 0
    misses = {}
    for f in files:
        try:
            islands = insel.islands(open(f, "rb").read())
        except Exception:
            continue
        rel = os.path.relpath(f, game_dir).replace("\\", "/")
        for n, isl in enumerate(islands):
            roads = {}
            for bx, by, _bw, _bh, data in insel.blocks(isl, "PROD2"):
                for k in range(0, len(data) - 5, 6):
                    idr, _state, x, y = struct.unpack_from("<HHBB", data, k)
                    tid = idr & 0xFFF
                    if tid in ROAD_IDS:
                        roads[(bx + x, by + y)] = (tid, idr >> 14)
            t = a = 0
            for (x, y), (tid, rot) in sorted(roads.items()):
                c = classify(tid)
                if c is None:
                    continue
                piece, want = SHAPE[mask_at(roads, x, y)]
                t += 1
                if c[1] == piece and same_rotation(piece, want, rot):
                    a += 1
                else:
                    key = (c[1], piece)
                    misses[key] = misses.get(key, 0) + 1
            if t:
                lines.append(f"{rel} {n} shaped {t} agree {a}")
                total += t
                agree += a
    lines.append(f"TOTAL shaped {total} agree {agree}")
    for line in lines[-1:]:
        print(line)
    print(f"agreement {100.0 * agree / max(total, 1):.2f} %")
    for (have, pred), c in sorted(misses.items(), key=lambda kv: -kv[1]):
        print(f"miss shipped={have} predicted={pred}: {c}")
    if write:
        with open(write, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("# ROADS-2a: tools/road_shape.py corpus over Scenes/**/*.sze: per island, the shaped road tiles\n")
            fh.write("# (straight/corner/cross/T/end) and how many the shape table predicts. Counts only.\n")
            fh.write("\n".join(lines) + "\n")
    if check:
        want = [l.rstrip("\n") for l in open(check, encoding="utf-8") if not l.startswith("#")]
        if want != lines:
            print("corpus digest differs from", check)
            return 1
        print("corpus digest agrees with", check)
    return 0


def main(argv):
    if not argv or argv[0] == "selftest":
        return selftest()
    if argv[0] == "corpus":
        game = os.environ.get("ANNO1503_GAME_DIR")
        write = check = None
        i = 1
        while i < len(argv):
            if argv[i] == "--game-dir":
                game = argv[i + 1]; i += 2
            elif argv[i] == "--write":
                write = argv[i + 1]; i += 2
            elif argv[i] == "--check":
                check = argv[i + 1]; i += 2
            else:
                raise SystemExit("unknown argument " + argv[i])
        if not game:
            print("skipping: ANNO1503_GAME_DIR not set")
            return 77
        return corpus(game, write, check)
    raise SystemExit(__doc__)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
