"""Author three ship-start proposals. Does not write lan5.txt.

Large-island positions are written by hand. Filler is the first fit on the
8-tile lattice, seeded, and it stays out of the reserved sea lanes. The
runtime loader does not read the JSON.

  python3 -I tools/map2_ship_layout.py --game-dir <game/app> --out-dir <dir>
"""
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import map2_layout as layout  # noqa: E402
import map2_manifest as mf  # noqa: E402
import map2_shape as shape  # noqa: E402

EDGE = 1431
GAME_NOTE = 'Scenes/0010SingleEndlos/03_1024_med_256.sze'

# Varied plain shapes. Filename suffixes are not treated as resource quality.
# People, pirate, and dummy files stay out. Each candidate repeats at most the
# shapes the nine/ten distinct masks force, and the positions are not a turn
# of another candidate.
NORD = {
    '0002': 'NORD/N_256X256_0002_2B.SCP',
    '0003': 'NORD/N_256X256_0003_2B.SCP',
    '0006': 'NORD/N_256X256_0006_2B.SCP',
    '0007': 'NORD/N_256X256_0007_2B.SCP',
    '0015': 'NORD/N_256X256_0015_5B.SCP',
    '0016': 'NORD/N_256X256_0016_3B.SCP',
    '0017': 'NORD/N_256X256_0017_3B.SCP',
    '0018': 'NORD/N_256X256_0018_2B.SCP',
    '0019': 'NORD/N_256X256_0019_2B.SCP',
}
SOUTH = {
    '0001': 'DSCHUNGEL/D_256X256_0001_0B.SCP',
    '0002': 'DSCHUNGEL/D_256X256_0002_4B.SCP',
    '0003': 'DSCHUNGEL/D_256X256_0003_4B.SCP',
    '0004': 'DSCHUNGEL/D_256X256_0004_4B.SCP',
    '0005': 'DSCHUNGEL/D_256X256_0005_5B.SCP',
    '0006': 'DSCHUNGEL/D_256X256_0006_3B.SCP',
    '0010': 'DSCHUNGEL/D_256X256_0010_6B.SCP',
    '0031': 'DSCHUNGEL/D_256X256_0031_5B.SCP',
    '0032': 'DSCHUNGEL/D_256X256_0032_2B.SCP',
    '0045': 'DSCHUNGEL/D_256X256_0045_5B.SCP',
}

# (x, y) origins. Spawns are tile coordinates of the ship, not a home island.
CANDIDATES = (
    {
        'name': 'lan5-ship-a',
        'seed': 26031,
        'filler': 18,
        'note': 'broad channels and dispersed large islands',
        'nord_keys': ('0018', '0002', '0007', '0016', '0015'),
        'south_keys': ('0001', '0032', '0003', '0031', '0045'),
        'nord': ((40, 80), (360, 320), (680, 40), (1000, 280), (720, 480)),
        'south': ((48, 760), (400, 880), (720, 1040), (1040, 760), (80, 1120)),
        'spawns': ((312, 680), (652, 680), (1108, 680), (1100, 240), (180, 1100)),
        'lanes': ((300, 560, 32, 240), (640, 600, 32, 200), (1088, 560, 32, 240)),
    },
    {
        'name': 'lan5-ship-b',
        'seed': 26032,
        'filler': 28,
        'note': 'linked channels with several nearby large islands',
        'nord_keys': ('0019', '0003', '0006', '0017', '0015'),
        'south_keys': ('0004', '0010', '0002', '0005', '0006'),
        'nord': ((80, 200), (400, 80), (720, 240), (1040, 80), (400, 400)),
        'south': ((80, 800), (400, 960), (760, 760), (1080, 960), (48, 1120)),
        'spawns': ((368, 240), (688, 160), (368, 720), (900, 600), (1200, 500)),
        'lanes': ((336, 200, 64, 160), (656, 160, 64, 200), (360, 656, 48, 144), (880, 520, 48, 240)),
    },
    {
        'name': 'lan5-ship-c',
        'seed': 26033,
        'filler': 40,
        'note': 'denser staggered archipelago with several corridors',
        'nord_keys': ('0006', '0017', '0002', '0019', '0016'),
        'south_keys': ('0045', '0031', '0001', '0005', '0010'),
        'nord': ((24, 96), (344, 256), (624, 40), (904, 304), (1168, 72)),
        'south': ((48, 792), (280, 1080), (560, 800), (880, 1120), (1160, 848)),
        'spawns': ((304, 120), (816, 600), (900, 160), (1120, 600), (360, 920)),
        'lanes': ((480, 456, 32, 304), (800, 456, 32, 304), (1120, 288, 32, 472)),
    },
)


def nice():
    try:
        os.nice(10)
    except OSError:
        pass


def facing_toward_centre(x, y):
    """0 north, 1 east, 2 south, 3 west. A stored heading, not a warehouse door."""
    dx = (EDGE // 2) - x
    dy = (EDGE // 2) - y
    if abs(dx) >= abs(dy):
        return 1 if dx > 0 else 3
    return 2 if dy > 0 else 0


def large_problems(spec):
    problems = []
    boxes = []
    for label, origins, keys in (
        ('nord', spec['nord'], spec['nord_keys']),
        ('dschungel', spec['south'], spec['south_keys']),
    ):
        if len(origins) != 5 or len(set(keys)) != 5:
            problems.append(f'{label} does not have five distinct keys')
        for index, (x, y) in enumerate(origins):
            if x % 8 or y % 8 or x < 0 or y < 0 or x + 256 > EDGE or y + 256 > EDGE:
                problems.append(f'{label} {index} at {x},{y} does not fit')
            if label == 'nord' and not shape.north_centre_ok(y, 256, EDGE):
                problems.append(f'nord {index} centre is below the line')
            if label == 'dschungel' and not shape.south_top_ok(y, EDGE):
                problems.append(f'dschungel {index} top is above the line')
            box = (x, y, 256, 256)
            for earlier in boxes:
                if shape.rect_overlap(box, earlier):
                    problems.append(f'{label} {index} overlaps another large island')
            boxes.append(box)
    points = []
    for seat, (x, y) in enumerate(spec['spawns']):
        if not (0 <= x < EDGE and 0 <= y < EDGE):
            problems.append(f'spawn {seat} is outside')
        for ox, oy, ow, oh in boxes:
            gap = max(shape_gap(x, ox, ow), shape_gap(y, oy, oh))
            if gap < 8:
                problems.append(f'spawn {seat} clearance {gap} at {x},{y}')
        for other in points:
            if max(abs(x - other[0]), abs(y - other[1])) < 64:
                problems.append(f'spawn {seat} is not separated')
        points.append((x, y))
    return problems


def shape_gap(tile, origin, size):
    if origin <= tile < origin + size:
        return 0
    if tile < origin:
        return origin - tile
    return tile - (origin + size - 1)


def build(spec, keys_by_group):
    problems = large_problems(spec)
    boxes = [(x, y, 256, 256) for x, y in spec['nord'] + spec['south']]
    for lane in spec['lanes']:
        boxes.append(lane)
    for x, y in spec['spawns']:
        boxes.append((max(0, x - 24), max(0, y - 24), 48, 48))
    placed, missed, tried = layout.place_filler(
        layout.schedule(spec['filler']), boxes, keys_by_group, EDGE, spec['seed'])
    islands = []
    ident = 0
    for key_id, (x, y) in zip(spec['nord_keys'], spec['nord']):
        islands.append({'id': ident, 'x': x, 'y': y, 'key': NORD[key_id]})
        ident += 1
    for key_id, (x, y) in zip(spec['south_keys'], spec['south']):
        islands.append({'id': ident, 'x': x, 'y': y, 'key': SOUTH[key_id]})
        ident += 1
    for slot, x, y, key, _size in placed:
        islands.append({'id': slot, 'x': x, 'y': y, 'key': key})
    spawns = []
    for seat, (x, y) in enumerate(spec['spawns']):
        spawns.append({
            'seat': seat,
            'x': x * mf.MILLI + mf.MILLI // 2,
            'y': y * mf.MILLI + mf.MILLI // 2,
            'facing': facing_toward_centre(x, y),
        })
    doc = {
        'manifest': mf.MANIFEST_NAME,
        'version': mf.MANIFEST_VERSION,
        'profile': mf.PROFILE_ID,
        'name': spec['name'],
        'edge': [EDGE, EDGE],
        'base': GAME_NOTE,
        'seed': spec['seed'],
        'generator': {'tool': 'map2_ship_layout.py', 'config': f"{spec['note']} filler={spec['filler']}"},
        'islands': islands,
        'spawns': spawns,
        'loadout': {
            'status': mf.BASELINE_LOADOUT['status'],
            'source': mf.BASELINE_LOADOUT['source'],
            'gold': mf.BASELINE_LOADOUT['gold'],
            'ship_figure': mf.BASELINE_LOADOUT['ship_figure'],
            'scout_figure': mf.BASELINE_LOADOUT['scout_figure'],
            'donkey_figure': mf.BASELINE_LOADOUT['donkey_figure'],
            'holds': [dict(item) for item in mf.BASELINE_LOADOUT['holds']],
        },
    }
    return doc, problems, missed, tried, len(placed)


def different_topology(specs):
    origins = []
    for spec in specs:
        origins.append(tuple(sorted(spec['nord'] + spec['south'])))
    return len(set(origins)) == len(origins)


def main(argv):
    nice()
    if argv[1:2] != ['--game-dir'] or argv[3:4] != ['--out-dir'] or len(argv) != 5:
        print('usage: map2_ship_layout.py --game-dir <dir> --out-dir <dir>', file=sys.stderr)
        return 2
    game, out_dir = argv[2], Path(argv[4])
    if not different_topology(CANDIDATES):
        print('layouts share a large-island origin set')
        return 1
    for spec in CANDIDATES:
        problems = large_problems(spec)
        if problems:
            print(spec['name'])
            for item in problems:
                print(f'  {item}')
            return 1
    keys_by_group = layout.distinct_keys(game)
    out_dir.mkdir(parents=True, exist_ok=True)
    for spec in CANDIDATES:
        doc, problems, missed, tried, placed = build(spec, keys_by_group)
        if problems:
            print(spec['name'], 'large-island problems', problems)
            return 1
        parsed = mf.parse(json.dumps(doc))
        text = mf.emit(parsed)
        if mf.parse(text.decode('ascii'))['content_hash'] != parsed['content_hash']:
            print(spec['name'], 'hash did not round-trip')
            return 1
        path = out_dir / f"{spec['name']}.json"
        path.write_bytes(text)
        print(
            f"{spec['name']} islands={len(doc['islands'])} filler={placed} "
            f"tried={tried} missed={len(missed)} hash={parsed['content_hash']} "
            f"bytes={len(text)}")
        for item in missed:
            print(f"  missed {item}")
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
