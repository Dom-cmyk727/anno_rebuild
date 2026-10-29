"""The first five-player layout (data/maps/lan5.txt), a proposal.

  python -I tools/fixed_map_layout.py

Reads the island templates and the Kontor footprint from the game folder
(read-only) and writes data/maps/lan5.txt. Re-running writes the same bytes.
The positions are integers. The search that chooses them uses float only to
place a rotated copy of one region; the file does not contain that float.

Each start region is the same four templates, rotated about the map centre:
a 256 nord home at radius 480, a 128 dschungel island on one side, a 64
steppe island on the other, and an 80 praerie island outward of the home.
The middle is one contested 128 tundra island. A ring at radius 180 does not
fit beside these islands, so the contested middle is that one island.
The template of each (climate, size) is the first eligible file in sorted
path order whose HEIGHT grid parses. PIRAT and DUMMY are excluded.

The Kontor is Anlagen entry 1302, a 4 x 4, on free land (HEIGHT >= 16, no
GELAENDE/FLUESSE/PROD2 origin) with a side on water, the site furthest along
the outward direction. Rotation 0: the footprint is square.
"""
import math
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insel  # noqa: E402

GAME = Path(os.environ.get('ANNO1503_GAME_DIR', r'E:\anno1503\game\app'))
OUT = Path(__file__).resolve().parent.parent / 'data' / 'maps' / 'lan5.txt'
EDGE = 1431
HOME_R = 480
GAP = 16


def snap(value):
    """Nearest multiple of 8. A tie rounds toward +infinity (v+4)."""
    return int((value + 4) // 8) * 8


def choose(root):
    groups = {}
    for path in sorted(root.rglob('*.scp')):
        name = path.name.upper()
        if 'PIRAT' in name or name.endswith('DUMMY.SCP'):
            continue
        key = path.relative_to(root).as_posix().upper()
        try:
            isl = insel.islands(path.read_bytes())[0]
            height = insel.grid(isl, 'HEIGHT', 1)
        except Exception:
            continue
        info = next((rp for rn, rp in isl['records'] if rn == 'INFO'), b'')
        if not info or info[0] > 6:
            continue
        w, h = isl['width'], isl['height']
        if w != h or w not in (64, 80, 128, 256):
            continue
        groups.setdefault((info[0], w), (key, path, height, isl))
    return groups


def box_at(cx, cy, size):
    x = snap(cx - size / 2)
    y = snap(cy - size / 2)
    return x, y


def overlaps(a, b):
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]


def occupied(isl):
    out = set()
    for name in ('GELAENDE', 'FLUESSE', 'PROD2'):
        for x, y, _bw, _bh, data in insel.blocks(isl, name):
            for i in range(len(data) // 6):
                item = data[i * 6:(i + 1) * 6]
                out.add((x + item[4], y + item[5]))
    return out


def coast(height, occ, w, h, fw, fh, outward):
    sites = []
    for y in range(h - fh + 1):
        for x in range(w - fw + 1):
            free = True
            for dy in range(fh):
                for dx in range(fw):
                    if height[(y + dy) * w + (x + dx)] < 16 or (x + dx, y + dy) in occ:
                        free = False
            if not free:
                continue
            water = False
            for dy in range(fh):
                for dx in range(fw):
                    for nx, ny in ((x + dx - 1, y + dy), (x + dx + 1, y + dy),
                                   (x + dx, y + dy - 1), (x + dx, y + dy + 1)):
                        if nx < 0 or ny < 0 or nx >= w or ny >= h or height[ny * w + nx] < 16:
                            water = True
            if water:
                sites.append((x, y))
    if not sites:
        return None

    def score(site):
        sx, sy = site
        return ((sx + fw / 2) - w / 2) * outward[0] + ((sy + fh / 2) - h / 2) * outward[1]

    return max(sites, key=score)


def main():
    root = GAME / 'Scenes' / 'Islands'
    groups = choose(root)
    want = {
        'home': (2, 256),
        'side': (4, 128),
        'other': (5, 64),
        'outward': (3, 80),
        'centre': (1, 128),
    }
    missing = [name for name, key in want.items() if key not in groups]
    if missing:
        print('no template for', ', '.join(missing))
        return 1
    picked = {name: groups[key] for name, key in want.items()}
    centre = EDGE / 2
    islands = []
    starts = []
    fw, fh = 4, 4

    def add(slot, kind, x, y):
        key, _path, height, isl = picked[kind]
        size = isl['width']
        islands.append((slot, x, y, key, size))
        return size

    for seat in range(5):
        theta = -math.pi / 2 + seat * 2 * math.pi / 5
        outward = (math.cos(theta), math.sin(theta))
        hx, hy = box_at(centre + HOME_R * outward[0], centre + HOME_R * outward[1], 256)
        add(seat, 'home', hx, hy)
        hcx, hcy = hx + 128, hy + 128
        actual = (hcx - centre, hcy - centre)
        length = math.hypot(*actual) or 1
        out_v = (actual[0] / length, actual[1] / length)
        tan_v = (-out_v[1], out_v[0])

        def beside(vx, vy, size):
            # Far enough that the axis-aligned boxes stay apart after the
            # origin is snapped to a multiple of 8 (at most 4 tiles each).
            half = size / 2
            denom = max(abs(vx), abs(vy), 1e-9)
            distance = (128 + half + GAP + 8) / denom
            return box_at(hcx + vx * distance, hcy + vy * distance, size)

        add(10 + seat * 3, 'side', *beside(tan_v[0], tan_v[1], 128))
        add(11 + seat * 3, 'other', *beside(-tan_v[0], -tan_v[1], 64))
        add(12 + seat * 3, 'outward', *beside(out_v[0], out_v[1], 80))
        local = coast(picked['home'][2], occupied(picked['home'][3]), 256, 256, fw, fh, out_v)
        if local is None:
            print(f'seat {seat}: no coastal site for the kontor')
            return 1
        starts.append((seat, seat, hx + local[0], hy + local[1], 0))

    add(100, 'centre', *box_at(centre, centre, 128))

    boxes = [(x, y, size, size, slot) for slot, x, y, _key, size in islands]
    failed = False
    for x, y, w, h, slot in boxes:
        if x % 8 or y % 8 or x < 0 or y < 0 or x + w > EDGE or y + h > EDGE:
            print(f'slot {slot} at ({x},{y}) size {w} does not fit in {EDGE}')
            failed = True
    for i, a in enumerate(boxes):
        for b in boxes[i + 1:]:
            if overlaps(a, b):
                print(f'slot {a[4]} ({a[0]},{a[1]} {a[2]}) overlaps slot {b[4]} ({b[0]},{b[1]} {b[2]})')
                failed = True
    if failed:
        return 1

    lines = [
        '# lan5, a proposal (docs/plan/lan-mvp.md section 8 question 6).',
        '# Five rotations of one region around a contested middle.',
        '# Generated by tools/fixed_map_layout.py. Raw materials are not listed:',
        '# the loader does not apply them (docs/sim/fixed-map.md section 7).',
        'anno1503-fixed-map 1',
        'name lan5',
        f'edge {EDGE} {EDGE}',
        'base Scenes/0010SingleEndlos/03_1024_med_256.sze',
        'seed 1',
    ]
    for slot, x, y, key, _size in islands:
        lines.append(f'island {slot} {x} {y} {key}')
    for seat, island, x, y, rot in starts:
        lines.append(f'start {seat} {island} {x} {y} {rot}')
    text = '\n'.join(lines) + '\n'
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(text.encode('ascii'))
    print(f'wrote {OUT} ({len(islands)} islands)')
    for slot, x, y, key, size in islands:
        print(f'  {slot:3} {x:4} {y:4} {size:3} {key}')
    for start in starts:
        print('  start', start)
    return 0


if __name__ == '__main__':
    sys.exit(main())
