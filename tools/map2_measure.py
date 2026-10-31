"""Re-measure the nine endless worlds and the large island templates.

  python3 -I tools/map2_measure.py --game-dir <game/app> --out <evidence.json>

Reads original files and writes counts, hashes and gaps. It does not write
masks. Rectangle coverage is the union of island rectangles. HEIGHT land is
reported only for islands whose grids are in the scenario (INSEL5). A link
has no template yet, so its land is not invented.

Also runs tools/world_measure.py world on each file and keeps that stdout.
"""
import json
import os
import subprocess
import sys
from itertools import combinations
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insel  # noqa: E402
import map2_shape as shape  # noqa: E402
import sze  # noqa: E402
import world_measure as wm  # noqa: E402

ENDLESS = (
    '00_1024_easyentry_01.sze',
    '01_1024_easy_256.sze',
    '02_1024_easy_128.sze',
    '03_1024_med_256.sze',
    '04_1024_med_128.sze',
    '05_800_Med_128.sze',
    '06_1024_heavy_256.sze',
    '07_1024_heavy_128.sze',
    '08_800_heavy_128.sze',
)
SIZES = (64, 80, 128, 256)


def nice():
    try:
        os.nice(10)
    except OSError:
        pass


def edge_n(n=5, base=1280):
    return (shape.isqrt(base * base * n) + 1) // 2


def percentile(values, numer, denom):
    if not values:
        return None
    ordered = sorted(values)
    index = (numer * (len(ordered) - 1)) // denom
    return ordered[index]


def largest_remainder(count, weights):
    total = sum(weights)
    base = [count * weight // total for weight in weights]
    left = count - sum(base)
    order = sorted(range(len(weights)), key=lambda i: (-(count * weights[i] % total), i))
    for index in order[:left]:
        base[index] += 1
    return base


def rect_area(bodies):
    return sum(body['w'] * body['h'] for body in bodies)


def union_area(bodies, width, height):
    overlaps = 0
    for i, left in enumerate(bodies):
        box = (left['x'], left['y'], left['w'], left['h'])
        for right in bodies[i + 1:]:
            if shape.rect_overlap(box, (right['x'], right['y'], right['w'], right['h'])):
                overlaps += 1
    if overlaps == 0:
        return rect_area(bodies), overlaps
    grid = bytearray(width * height)
    for body in bodies:
        x0 = max(0, body['x'])
        y0 = max(0, body['y'])
        x1 = min(width, body['x'] + body['w'])
        y1 = min(height, body['y'] + body['h'])
        for y in range(y0, y1):
            row = y * width
            grid[row + x0:row + x1] = b'\x01' * (x1 - x0)
    return sum(grid), overlaps


def nearest_stats(bodies):
    gaps = []
    centres = []
    overlaps = 0
    for i, left in enumerate(bodies):
        box = (left['x'], left['y'], left['w'], left['h'])
        cx = 2 * left['x'] + left['w']
        cy = 2 * left['y'] + left['h']
        best_gap = None
        best_centre = None
        for j, right in enumerate(bodies):
            if i == j:
                continue
            gap = shape.edge_gap(box, (right['x'], right['y'], right['w'], right['h']))
            if gap is None:
                overlaps += 1
                continue
            if best_gap is None or gap < best_gap:
                best_gap = gap
            ox = 2 * right['x'] + right['w']
            oy = 2 * right['y'] + right['h']
            dist2 = (cx - ox) * (cx - ox) + (cy - oy) * (cy - oy)
            milli = shape.isqrt(dist2 * 250000)
            if best_centre is None or milli < best_centre:
                best_centre = milli
        if best_gap is not None:
            gaps.append(best_gap)
        if best_centre is not None:
            centres.append(best_centre)
    return gaps, centres, overlaps // 2


def clark_evans(centre_milli, count, area):
    """R = mean nearest-neighbour / (0.5 / sqrt(density)). No edge correction."""
    if count < 2 or area <= 0 or not centre_milli:
        return None
    mean = sum(centre_milli) / len(centre_milli) / 1000.0
    expected = 0.5 / ((count / area) ** 0.5)
    if expected == 0:
        return None
    return mean / expected


def world_record(path):
    data = path.read_bytes()
    _edition, kids = sze.scene(data)
    placed = insel.islands(data)
    head = next(payload for name, payload in kids if name == 'HEAD')
    import struct
    width, height = struct.unpack_from('<II', head, 0)
    links, drawn = wm.rects_of(kids, placed)
    bodies = []
    for link in links:
        bodies.append({**link, 'resolved': False})
    resolved_land = 0
    resolved_height_pos = 0
    resolved_area = 0
    for isle, one in zip(drawn, placed):
        body = {**isle, 'resolved': True}
        try:
            grid = insel.grid(one, 'HEIGHT', 1)
        except insel.Bad:
            grid = None
        if grid is not None:
            resolved_land += shape.land_count(grid)
            resolved_height_pos += sum(1 for value in grid if value > 0)
            resolved_area += one['width'] * one['height']
            body['land'] = shape.land_count(grid)
        bodies.append(body)
    union, overlaps = union_area(bodies, width, height)
    gaps, centres, pair_overlaps = nearest_stats(bodies)
    by_size = {}
    by_climate = {}
    cross = {}
    for body in bodies:
        size = (body['w'], body['h'])
        by_size[size] = by_size.get(size, 0) + 1
        climate = body['climate']
        by_climate[climate] = by_climate.get(climate, 0) + 1
        cross_key = (climate, body['w'], body['h'])
        cross[cross_key] = cross.get(cross_key, 0) + 1
    area = width * height
    return {
        'file': path.name,
        'width': width,
        'height': height,
        'area': area,
        'islands': len(bodies),
        'links': len(links),
        'insel5': len(drawn),
        'per_million_hundredths': (len(bodies) * 100_000_000 + area // 2) // area,
        'rectangle_sum': rect_area(bodies),
        'rectangle_union': union,
        'rectangle_overlaps': overlaps,
        'rectangle_share_tenths': wm.pct10(union, area),
        'resolved_land_height_ge_16': resolved_land,
        'resolved_height_gt_0': resolved_height_pos,
        'resolved_rectangle_tiles': resolved_area,
        'actual_land_complete': len(links) == 0 and len(drawn) == len(bodies),
        'sizes': {f'{w}x{h}': count for (w, h), count in sorted(by_size.items())},
        'climates': {str(key): count for key, count in sorted(by_climate.items(), key=lambda item: (item[0] is None, item[0]))},
        'cross': [
            {'climate': climate, 'w': w, 'h': h, 'count': count}
            for (climate, w, h), count in sorted(cross.items(), key=lambda item: (item[0][0] is None, item[0][0] or 0, item[0][1], item[0][2]))
        ],
        'nearest_gap': {
            'min': min(gaps) if gaps else None,
            'p10': percentile(gaps, 1, 10),
            'median': wm.median_low(gaps),
            'max': max(gaps) if gaps else None,
            'definition': 'per island, minimum floored Euclidean edge gap to another rectangle; median_low; p10 index is floor(0.10*(n-1))',
        },
        'centre_spacing_milli': {
            'min': min(centres) if centres else None,
            'p10': percentile(centres, 1, 10),
            'median': wm.median_low(centres),
            'max': max(centres) if centres else None,
            'clark_evans': None if clark_evans(centres, len(bodies), area) is None else round(clark_evans(centres, len(bodies), area), 4),
            'definition': 'nearest neighbour of rectangle centres, floor(euclidean*1000); Clark-Evans has no edge correction; R>1 is more even than Poisson',
        },
        'overlap_pairs': pair_overlaps,
    }


def load_large(path, islands_root):
    name = path.name
    upper = name.upper()
    key = path.relative_to(islands_root).as_posix().upper()
    record = {
        'key': key,
        'file': name,
        'people': shape.people_marker(name),
        'dummy': upper.endswith('DUMMY.SCP'),
        'bug': upper.endswith('.BUG'),
        'pirat_name': 'PIRAT' in upper,
    }
    try:
        isl = insel.islands(path.read_bytes())[0]
        height = insel.grid(isl, 'HEIGHT', 1)
    except (insel.Bad, OSError) as exc:
        record['parse'] = exc.__class__.__name__
        return record
    info = next((payload for rec_name, payload in isl['records'] if rec_name == 'INFO'), b'')
    record['parse'] = 'ok'
    record['w'] = isl['width']
    record['h'] = isl['height']
    record['info0'] = info[0] if info else None
    record['info2'] = info[2] if len(info) > 2 else None
    record['info8'] = info[8] if len(info) > 8 else None
    record['info9'] = info[9] if len(info) > 9 else None
    record['land'] = shape.land_count(height)
    record['height_gt_0'] = sum(1 for value in height if value > 0)
    if isl['width'] == isl['height']:
        mask, oriented = shape.shape_ids(height, isl['width'])
        record['land_sha256'] = shape.sha256_hex(mask)
        record['height_sha256'] = shape.sha256_hex(oriented)
    else:
        record['land_sha256'] = None
        record['height_sha256'] = None
    prefix = wm.climate_of(name)
    record['prefix_climate'] = prefix
    record['eligible_scan'] = (
        record['parse'] == 'ok'
        and not record['pirat_name']
        and not record['dummy']
        and not record['bug']
        and record['info8'] == 0
        and record['info9'] == 0
        and prefix is not None
        and record['info0'] == prefix
        and isl['width'] == 256
        and isl['height'] == 256
    )
    record['candidate_pool'] = record['eligible_scan'] and record['people'] is None
    return record


def shape_table(rows):
    groups = {}
    for row in rows:
        digest = row.get('land_sha256')
        if not digest:
            continue
        groups.setdefault(digest, []).append({
            'key': row['key'],
            'land': row.get('land'),
            'height_sha256': row.get('height_sha256'),
            'people': row.get('people'),
            'candidate_pool': row.get('candidate_pool', False),
        })
    table = []
    for digest, members in groups.items():
        members.sort(key=lambda item: item['key'])
        hashes = {item['height_sha256'] for item in members}
        table.append({
            'land_sha256': digest,
            'height_hashes_agree': len(hashes) == 1,
            'count': len(members),
            'members': members,
        })
    table.sort(key=lambda item: item['members'][0]['key'])
    return table


def quintuples(rows):
    """One representative per distinct land shape, then every combination of five."""
    groups = {}
    for row in rows:
        if not row.get('candidate_pool') or not row.get('land_sha256'):
            continue
        groups.setdefault(row['land_sha256'], []).append(row)
    reps = []
    for digest, members in groups.items():
        members.sort(key=lambda item: item['key'])
        reps.append(members[0])
    reps.sort(key=lambda item: item['key'])
    found = []
    for combo in combinations(reps, 5):
        lands = [item['land'] for item in combo]
        found.append({
            'keys': [item['key'] for item in combo],
            'lands': lands,
            'worst_permille': shape.worst_permille(lands),
            'within_50': shape.within_mean(lands, 50),
            'within_100': shape.within_mean(lands, 100),
        })
    found.sort(key=lambda item: (item['worst_permille'], item['keys']))
    return {
        'distinct_shapes': len(reps),
        'copied_into': [
            {'land_sha256': digest, 'keys': [item['key'] for item in members]}
            for digest, members in groups.items() if len(members) > 1
        ],
        'quintuples': len(found),
        'within_50': sum(1 for item in found if item['within_50']),
        'within_100': sum(1 for item in found if item['within_100']),
        'best': found[:8],
    }


def scale_block(reference, edge, area_1024):
    """Scale a 1024 size mix by area, then show the owner's ten 256 islands."""
    area = edge * edge
    weights = [reference.get(size, 0) for size in SIZES]
    link_count = sum(weights)
    scaled_links = (link_count * area + (area_1024 // 2)) // area_1024
    scaled = largest_remainder(scaled_links, weights)
    scaled_map = {size: count for size, count in zip(SIZES, scaled)}
    rect = sum(scaled_map[size] * size * size for size in SIZES)
    owner = dict(scaled_map)
    owner[256] = 10
    owner_rect = sum(owner[size] * size * size for size in SIZES)
    ref_rect = sum(reference.get(size, 0) * size * size for size in SIZES)
    ref_share = ref_rect / area_1024
    target_union = (ref_rect * area + area_1024 // 2) // area_1024
    filler_budget = target_union - 10 * 256 * 256
    filler_weights = [reference.get(size, 0) for size in (64, 80, 128)]
    filler_ref_area = sum(reference.get(size, 0) * size * size for size in (64, 80, 128))
    if filler_budget > 0 and filler_ref_area > 0 and sum(filler_weights) > 0:
        filler_count = (sum(filler_weights) * filler_budget + filler_ref_area // 2) // filler_ref_area
        filler_counts = largest_remainder(filler_count, filler_weights)
    else:
        filler_counts = [0, 0, 0]
    matched = {64: filler_counts[0], 80: filler_counts[1], 128: filler_counts[2], 256: 10}
    matched_rect = sum(matched[size] * size * size for size in SIZES)
    return {
        'reference_counts': {str(size): reference.get(size, 0) for size in SIZES},
        'reference_rectangle_tiles': ref_rect,
        'reference_share_tenths': wm.pct10(ref_rect, area_1024),
        'area_ratio_note': f'{area}/{area_1024}',
        'proportional_counts': {str(size): scaled_map[size] for size in SIZES},
        'proportional_islands': sum(scaled_map.values()),
        'proportional_rectangle_tiles': rect,
        'proportional_share_tenths': wm.pct10(rect, area),
        'owner_10x256_plus_proportional_filler': {
            'counts': {str(size): owner[size] for size in SIZES},
            'islands': sum(owner.values()),
            'rectangle_tiles': owner_rect,
            'share_tenths': wm.pct10(owner_rect, area),
            'note': 'keeps the scaled 64/80/128 counts and replaces the scaled 256 count with 10',
        },
        'owner_10x256_coverage_matched_filler': {
            'counts': {str(size): matched[size] for size in SIZES},
            'islands': sum(matched.values()),
            'rectangle_tiles': matched_rect,
            'share_tenths': wm.pct10(matched_rect, area),
            'filler_budget_tiles': filler_budget,
            'note': '10 large islands, filler counts scaled so rectangle tiles stay near the reference share',
        },
        'conflict': (
            'Ten 256-square islands are 10*65536 tiles. '
            'The proportional scale of the reference mix does not yield a 256-count of 10. '
            'Keeping both the reference rectangle share and the proportional filler counts is not possible.'
        ),
    }


def main(argv):
    nice()
    game = None
    out = None
    log_dir = None
    args = argv[1:]
    while args:
        if args[0] == '--game-dir' and len(args) >= 2:
            game = Path(args[1])
            args = args[2:]
        elif args[0] == '--out' and len(args) >= 2:
            out = Path(args[1])
            args = args[2:]
        elif args[0] == '--log-dir' and len(args) >= 2:
            log_dir = Path(args[1])
            args = args[2:]
        else:
            print('usage: map2_measure.py --game-dir <dir> --out <json> [--log-dir <dir>]', file=sys.stderr)
            return 2
    if game is None or out is None:
        print('usage: map2_measure.py --game-dir <dir> --out <json>', file=sys.stderr)
        return 2
    endless = game / 'Scenes' / '0010SingleEndlos'
    tool = Path(__file__).resolve().parent / 'world_measure.py'
    worlds = []
    for name in ENDLESS:
        path = endless / name
        if log_dir is not None:
            log_dir.mkdir(parents=True, exist_ok=True)
            proc = subprocess.run(
                [sys.executable, '-I', str(tool), 'world', str(path)],
                capture_output=True, text=True)
            (log_dir / f'world-{name}.txt').write_text(proc.stdout + proc.stderr)
            if proc.returncode != 0:
                print(f'world_measure failed {name} exit {proc.returncode}', file=sys.stderr)
                return 1
        print(f'measure {name}', flush=True)
        worlds.append(world_record(path))
    edge = edge_n()
    reference = next(item for item in worlds if item['file'].startswith('03_'))
    ref_sizes = {}
    for label, count in reference['sizes'].items():
        w, h = label.split('x')
        if w == h and int(w) in SIZES:
            ref_sizes[int(w)] = count
    islands_root = game / 'Scenes' / 'Islands'
    large = []
    for folder in ('Nord', 'Dschungel'):
        for path in sorted((islands_root / folder).glob('*256x256*')):
            print(f'template {path.name}', flush=True)
            large.append(load_large(path, islands_root))
    nord = [row for row in large if row['key'].startswith('NORD/')]
    south = [row for row in large if row['key'].startswith('DSCHUNGEL/')]
    evidence = {
        'land_height': shape.LAND_HEIGHT,
        'land_definition': 'HEIGHT >= 16, fixed_map.py _land_count and iso kWaterline. Not HEIGHT > 0 and not the classifier at average 19.',
        'edge_1431': edge,
        'bands_1431': [
            {'climate': climate, 'name': name, 'y0': y0, 'y1_exclusive': y1}
            for climate, name, y0, y1 in shape.bands_for(edge)
        ],
        'worlds': worlds,
        'scale_from_03': scale_block(ref_sizes, edge, reference['area']),
        'nord_256': {
            'files': nord,
            'scp': sum(1 for row in nord if row['file'].upper().endswith('.SCP')),
            'bug': sum(1 for row in nord if row['bug']),
            'dummy': sum(1 for row in nord if row['dummy']),
            'eligible_scan': sum(1 for row in nord if row.get('eligible_scan')),
            'candidate_pool': sum(1 for row in nord if row.get('candidate_pool')),
            'shapes': shape_table(nord),
            'groups': quintuples(nord),
        },
        'dschungel_256': {
            'files': south,
            'scp': sum(1 for row in south if row['file'].upper().endswith('.SCP')),
            'bug': sum(1 for row in south if row['bug']),
            'dummy': sum(1 for row in south if row['dummy']),
            'eligible_scan': sum(1 for row in south if row.get('eligible_scan')),
            'candidate_pool': sum(1 for row in south if row.get('candidate_pool')),
            'shapes': shape_table(south),
            'groups': quintuples(south),
        },
    }
    # Hashes only: drop nothing else bulky. Height grids were never stored.
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(evidence, indent=2, sort_keys=True) + '\n')
    print(f'wrote {out}')
    return 0


if __name__ == '__main__':
    sys.stdout.reconfigure(newline='\n')
    sys.exit(main(sys.argv))
