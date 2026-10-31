"""Reproducible MAP.2 candidate layouts. Does not write data/maps/lan5.txt.

  python3 -I tools/map2_layout.py --game-dir <game/app> --out-dir <dir>

The engine loads the text files. This script is offline. Same arguments write
the same bytes. Large-island positions are authored. Filler positions are the
first fit on the 8-tile lattice in a seeded order.
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insel  # noqa: E402
import map2_profile as profile  # noqa: E402
import map2_shape as shape  # noqa: E402
import world_measure as wm  # noqa: E402

EDGE = 1431
# Filler weights are the non-256 part of 03's climate/size cross-tab.
FILLER_WEIGHTS = (
    (0, 64, 1), (0, 80, 2),
    (1, 80, 2), (1, 128, 1),
    (2, 64, 1), (2, 80, 2),
    (3, 80, 1), (3, 128, 3),
    (4, 64, 2), (4, 80, 2), (4, 128, 1),
    (5, 64, 1), (5, 80, 1), (5, 128, 3),
)
NORD_KEYS = (
    'NORD/N_256X256_0002_2B.SCP',
    'NORD/N_256X256_0003_2B.SCP',
    'NORD/N_256X256_0007_2B.SCP',
    'NORD/N_256X256_0018_2B.SCP',
    'NORD/N_256X256_0019_2B.SCP',
)
SOUTH_KEYS = (
    'DSCHUNGEL/D_256X256_0002_4B.SCP',
    'DSCHUNGEL/D_256X256_0003_4B.SCP',
    'DSCHUNGEL/D_256X256_0005_5B.SCP',
    'DSCHUNGEL/D_256X256_0006_3B.SCP',
    'DSCHUNGEL/D_256X256_0010_6B.SCP',
)
# Authored large-island origins. Each southern island sits on the opposite
# ray from its home, so a kontor facing the map centre also faces that partner.
CANDIDATES = (
    {
        'name': 'lan5-a',
        'seed': 15031,
        'filler': 30,
        'note': 'opposite-ray partners on a 152 degree arc, coverage-matched filler',
        'rotate': 0,
        'homes': ((40, 448), (240, 144), (584, 24), (928, 144), (1128, 448)),
        'souths': ((1128, 720), (928, 1032), (584, 1144), (240, 1032), (40, 720)),
    },
    {
        'name': 'lan5-b',
        'seed': 15032,
        'filler': 30,
        'note': 'opposite-ray partners on a 144 degree arc, coverage-matched filler',
        'rotate': 1,
        'homes': ((64, 416), (264, 144), (584, 40), (912, 144), (1112, 416)),
        'souths': ((1072, 744), (888, 1000), (584, 1096), (288, 1000), (104, 744)),
    },
    {
        'name': 'lan5-c',
        'seed': 15033,
        'filler': 45,
        'note': 'opposite-ray partners on a 140 degree arc, denser filler',
        'rotate': 2,
        'homes': ((32, 384), (248, 104), (584, 0), (928, 104), (1144, 384)),
        'souths': ((1088, 768), (888, 1024), (584, 1120), (280, 1024), (88, 768)),
    },
)


def nice():
    try:
        os.nice(10)
    except OSError:
        pass


def largest_remainder(count, weights):
    total = sum(weights)
    base = [count * weight // total for weight in weights]
    left = count - sum(base)
    order = sorted(range(len(weights)), key=lambda i: (-(count * weights[i] % total), i))
    for index in order[:left]:
        base[index] += 1
    return base


def schedule(filler_count):
    counts = largest_remainder(filler_count, [item[2] for item in FILLER_WEIGHTS])
    specs = [(climate, size, count) for (climate, size, _), count in zip(FILLER_WEIGHTS, counts) if count]
    # Larger rectangles first so a 128 is not squeezed out by earlier 80s.
    specs.sort(key=lambda item: (-item[1], item[0]))
    return specs


def order_key(seed, index, x, y):
    return (seed * 2246822519 + x * 3266489917 + y * 668265263 + index * 374761393) & 0xFFFFFFFF


def distinct_keys(game):
    """First path in sorted order for each distinct land mask, by climate and size."""
    root = Path(game) / 'Scenes' / 'Islands'
    groups = {}
    for path in sorted(root.rglob('*.scp')):
        upper = path.name.upper()
        if 'PIRAT' in upper or upper.endswith('DUMMY.SCP') or shape.people_marker(path.name):
            continue
        try:
            isl = insel.islands(path.read_bytes())[0]
            height = insel.grid(isl, 'HEIGHT', 1)
        except (insel.Bad, OSError):
            continue
        info = next((payload for name, payload in isl['records'] if name == 'INFO'), b'')
        if len(info) < 10 or info[8] != 0 or info[9] != 0:
            continue
        prefix = wm.climate_of(path.name)
        if prefix is None or info[0] != prefix:
            continue
        if isl['width'] != isl['height'] or isl['width'] not in (64, 80, 128):
            continue
        mask, _oriented = shape.shape_ids(height, isl['width'])
        digest = shape.sha256_hex(mask)
        key = path.relative_to(root).as_posix().upper()
        bucket = groups.setdefault((prefix, isl['width']), {})
        bucket.setdefault(digest, key)
    picked = {}
    for group, members in groups.items():
        picked[group] = tuple(sorted(members.values()))
    return picked


def place_filler(spec_list, boxes, keys_by_group, edge, seed):
    bands = {climate: (y0, y1) for climate, _name, y0, y1 in shape.bands_for(edge)}
    placed = []
    missed = []
    tried = 0
    cursor = {group: 0 for group in keys_by_group}
    slot = 100
    index = 0
    for climate, size, count in spec_list:
        group = (climate, size)
        pool = keys_by_group.get(group) or ()
        y0, y1 = bands[climate]
        if size > y1 - y0 or not pool:
            missed.append({'climate': climate, 'size': size, 'count': count, 'reason': 'no room or no template'})
            continue
        xs = list(range(0, edge - size + 1, 8))
        ys = list(range(y0, y1 - size + 1, 8))
        lattice = sorted(((order_key(seed, index, x, y), x, y) for y in ys for x in xs))
        for _copy in range(count):
            key = pool[cursor[group] % len(pool)]
            cursor[group] += 1
            found = None
            for _rank, x, y in lattice:
                tried += 1
                box = (x, y, size, size)
                blocked = False
                for earlier in boxes:
                    gap = shape.edge_gap(box, earlier)
                    if gap is None or gap < 8:
                        blocked = True
                        break
                if not blocked:
                    found = (x, y)
                    break
            if found is None:
                for _rank, x, y in lattice:
                    tried += 1
                    box = (x, y, size, size)
                    if any(shape.edge_gap(box, earlier) is None for earlier in boxes):
                        continue
                    found = (x, y)
                    break
            if found is None:
                missed.append({'climate': climate, 'size': size, 'reason': 'lattice full'})
                continue
            x, y = found
            boxes.append((x, y, size, size))
            placed.append((slot, x, y, key, size))
            slot += 1
            index += 1
    return placed, missed, tried


def large_ok(homes, souths):
    problems = []
    boxes = []
    for label, origins in (('nord', homes), ('dschungel', souths)):
        for index, (x, y) in enumerate(origins):
            if x % 8 or y % 8 or x < 0 or y < 0 or x + 256 > EDGE or y + 256 > EDGE:
                problems.append(f'{label} {index} at {x},{y} does not fit')
            if label == 'nord' and not shape.north_centre_ok(y, 256, EDGE):
                problems.append(f'nord {index} centre is below the line')
            if label == 'dschungel' and not shape.south_top_ok(y, EDGE):
                problems.append(f'dschungel {index} top is above the line')
            box = (x, y, 256, 256)
            for earlier in boxes:
                if shape.edge_gap(box, earlier) is None:
                    problems.append(f'{label} {index} overlaps another large island')
            boxes.append(box)
    return problems


def rotate(keys, shift):
    shift %= len(keys)
    return keys[shift:] + keys[:shift]


def build(spec, keys_by_group, catalog):
    homes = spec['homes']
    souths = spec['souths']
    problems = large_ok(homes, souths)
    nord_keys = rotate(NORD_KEYS, spec['rotate'])
    south_keys = rotate(SOUTH_KEYS, spec['rotate'])
    islands = []
    boxes = []
    for index, ((x, y), key) in enumerate(zip(homes, nord_keys)):
        islands.append((index, x, y, key, 256))
        boxes.append((x, y, 256, 256))
    for index, ((x, y), key) in enumerate(zip(souths, south_keys)):
        islands.append((10 + index, x, y, key, 256))
        boxes.append((x, y, 256, 256))
    filler, missed, tried = place_filler(schedule(spec['filler']), boxes, keys_by_group, EDGE, spec['seed'])
    islands.extend(filler)
    # Coastal 4x4 on the side toward the map centre. Rotation stays 0:
    # the footprint is square, and the original door direction is unread.
    starts = []
    for index, (x, y) in enumerate(homes):
        template = catalog[nord_keys[index]]
        sites = profile.coast_sites(template['height'], template['occupied'], 256, 256, 4, 4)
        toward_x = (EDGE // 2) - (x + 128)
        toward_y = (EDGE // 2) - (y + 128)
        site = profile.facing_site(sites, 256, 256, 4, 4, toward_x, toward_y)
        if site is None:
            problems.append(f'nord {index} has no coastal kontor')
            site = (0, 0)
        starts.append((index, index, x + site[0], y + site[1], 0))
    mp = layout_mp(spec, islands, [(s[0], s[1], s[2], s[3]) for s in starts])
    return {
        'spec': spec,
        'islands': islands,
        'starts': starts,
        'problems': problems,
        'missed': missed,
        'tried': tried,
        'mp': mp,
    }


def layout_mp(spec, islands, starts):
    return {
        'version': 1,
        'name': spec['name'],
        'width': EDGE,
        'height': EDGE,
        'base': 'Scenes/0010SingleEndlos/03_1024_med_256.sze',
        'seed': spec['seed'],
        'money': None,
        'islands': [
            {'slot': slot, 'x': x, 'y': y, 'key': key, 'raw': [], 'ore': []}
            for slot, x, y, key, _size in islands
        ],
        'starts': [
            {'seat': seat, 'island': island, 'x': x, 'y': y, 'rotation': 0}
            for seat, island, x, y in starts
        ],
    }


def render(spec, islands, starts):
    lines = [
        f'# {spec["name"]}: {spec["note"]}.',
        f'# Offline layout, seed {spec["seed"]}. tools/map2_layout.py writes these bytes.',
        '# Large islands: five NORD 256 on or above the middle, five DSCHUNGEL 256 below it.',
        '# Raw materials are not listed; the loader does not apply them.',
        'anno1503-fixed-map 1',
        f'name {spec["name"]}',
        f'edge {EDGE} {EDGE}',
        'base Scenes/0010SingleEndlos/03_1024_med_256.sze',
        f'seed {spec["seed"]}',
    ]
    for slot, x, y, key, _size in islands:
        lines.append(f'island {slot} {x} {y} {key}')
    for seat, island, x, y, rotation in starts:
        lines.append(f'start {seat} {island} {x} {y} {rotation}')
    return ('\n'.join(lines) + '\n').encode('ascii')


def main(argv):
    nice()
    game = None
    out_dir = None
    args = argv[1:]
    while args:
        if args[0] == '--game-dir' and len(args) >= 2:
            game = args[1]
            args = args[2:]
        elif args[0] == '--out-dir' and len(args) >= 2:
            out_dir = Path(args[1])
            args = args[2:]
        else:
            print('usage: map2_layout.py --game-dir <dir> --out-dir <dir>', file=sys.stderr)
            return 2
    if game is None or out_dir is None:
        print('usage: map2_layout.py --game-dir <dir> --out-dir <dir>', file=sys.stderr)
        return 2
    print('scanning filler templates', flush=True)
    keys_by_group = distinct_keys(game)
    needed = set(NORD_KEYS) | set(SOUTH_KEYS)
    for pool in keys_by_group.values():
        needed.update(pool[:4])
    print(f'loading {len(needed)} templates', flush=True)
    catalog = profile.load_templates(game, needed)
    # Filler placement only needs keys; validate needs every key that was placed.
    # Load the rest after placement if a later key was used. Pools cycle from the start,
    # and only the first few of a long pool are used when count is small, but a pool
    # can be cycled fully. Load every representative; counts are small.
    catalog = profile.load_templates(game, set(NORD_KEYS) | set(SOUTH_KEYS) | {
        key for pool in keys_by_group.values() for key in pool
    })
    out_dir.mkdir(parents=True, exist_ok=True)
    for spec in CANDIDATES:
        print(f'build {spec["name"]}', flush=True)
        # Restrict each group to a stable prefix so the load stays bounded and
        # the cycle is still deterministic. Four shapes are enough to vary filler.
        limited = {group: pool[:4] for group, pool in keys_by_group.items()}
        built = build(spec, limited, catalog)
        text = render(spec, built['islands'], built['starts'])
        path = out_dir / f'{spec["name"]}.txt'
        path.write_bytes(text)
        print(
            f'wrote {path} islands={len(built["islands"])} tried={built["tried"]} '
            f'large_problems={len(built["problems"])} missed={len(built["missed"])}')
        for problem in built['problems']:
            print(' ', problem)
        for miss in built['missed']:
            print('  missed', miss)
    return 0


if __name__ == '__main__':
    sys.stdout.reconfigure(newline='\n')
    sys.exit(main(sys.argv))
