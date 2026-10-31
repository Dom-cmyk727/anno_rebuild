"""Candidate profile for a five-player original-like fixed map.

This is not the generic fixed-map check. `tools/fixed_map.py check` stays
valid for every layout, including lan5. The profile is opt-in:

  python3 -I tools/map2_profile.py validate <file> --game-dir <game/app>

A passing report names the profile and the permille limits. Proposed limits
are not an owner approval. ±10% large-island land is 100 permille. ±5%
fairness is 50 permille. The old packet's ±10% distances and ±25% filler are
printed as sensitivity and are not gates.

Exit status is 0 only when every structural rule and every gate passes.
"""
import itertools
import math
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixed_map  # noqa: E402
import map2_shape as shape  # noqa: E402

# Mutations in the verification step flip these names and restore them.
HALFPLANE_CHECK = True
CLIMATE_BAND_CHECK = True
SHAPE_BY_MASK = True
ASSIGNMENT_MODE = 'min-spread-sum-lex'
FAIRNESS_PERMILLE = 50
LARGE_LAND_PERMILLE = 100
PROFILE_ID = 'map2-proposed-v1'


def default_profile():
    return {
        'id': PROFILE_ID,
        'large': shape.LARGE,
        'large_land_permille': LARGE_LAND_PERMILLE,
        'fairness_permille': FAIRNESS_PERMILLE,
        'seats': 5,
        'fw': 4,
        'fh': 4,
        'assignment': ASSIGNMENT_MODE,
        'filler_sensitivity_permille': 250,
        'distance_sensitivity_permille': 100,
    }


def geometry_problems(mp, sizes):
    """Rectangle rules shared with an ordinary fixed map. No candidate rule."""
    problems = []
    boxes = []
    for island in mp['islands']:
        size = sizes.get(island['slot'])
        if size is None:
            problems.append(f"island slot {island['slot']}: unknown template '{island['key']}'")
            continue
        w, h = size
        if island['x'] % 8 or island['y'] % 8:
            problems.append(f"island slot {island['slot']}: origin is not a multiple of 8")
        if island['x'] < 0 or island['y'] < 0 or island['x'] + w > mp['width'] or island['y'] + h > mp['height']:
            problems.append(f"island slot {island['slot']}: outside the world")
        box = (island['x'], island['y'], w, h, island['slot'])
        for earlier in boxes:
            if shape.rect_overlap(box[:4], earlier[:4]):
                problems.append(f"island slot {island['slot']}: overlaps island slot {earlier[4]}")
        boxes.append(box)
    return problems


def coast_sites(height, occupied, w, h, fw, fh):
    sites = []
    for y in range(h - fh + 1):
        for x in range(w - fw + 1):
            free = True
            for dy in range(fh):
                for dx in range(fw):
                    if height[(y + dy) * w + (x + dx)] < shape.LAND_HEIGHT or (x + dx, y + dy) in occupied:
                        free = False
                        break
                if not free:
                    break
            if not free:
                continue
            water = False
            for dy in range(fh):
                for dx in range(fw):
                    for nx, ny in ((x + dx - 1, y + dy), (x + dx + 1, y + dy),
                                   (x + dx, y + dy - 1), (x + dx, y + dy + 1)):
                        if nx < 0 or ny < 0 or nx >= w or ny >= h or height[ny * w + nx] < shape.LAND_HEIGHT:
                            water = True
            if water:
                sites.append((x, y))
    return sites


def facing_site(sites, w, h, fw, fh, toward_x, toward_y):
    """Prefer the coastal footprint whose centre faces (toward_x, toward_y).

    The vector is in world tiles from the island centre. Tie: smaller local y,
    then smaller local x. Square footprints do not change with rotation, so
    this chooses a side, not a door. Door direction is not in the evidence.
    """
    if not sites:
        return None

    def key(site):
        sx, sy = site
        score = (2 * (sx + fw // 2) - w) * toward_x + (2 * (sy + fh // 2) - h) * toward_y
        return (-score, sy, sx)

    return min(sites, key=key)


def coast_problem(start, island, template, fw, fh):
    local_x = start['x'] - island['x']
    local_y = start['y'] - island['y']
    w, h = template['w'], template['h']
    if local_x < 0 or local_y < 0 or local_x + fw > w or local_y + fh > h:
        return f"start seat {start['seat']}: the kontor footprint is outside island slot {island['slot']}"
    height = template['height']
    occupied = template['occupied']
    for dy in range(fh):
        for dx in range(fw):
            if height[(local_y + dy) * w + (local_x + dx)] < shape.LAND_HEIGHT or (local_x + dx, local_y + dy) in occupied:
                return f"start seat {start['seat']}: the kontor is not on free land"
    water = False
    for dy in range(fh):
        for dx in range(fw):
            for nx, ny in ((local_x + dx - 1, local_y + dy), (local_x + dx + 1, local_y + dy),
                           (local_x + dx, local_y + dy - 1), (local_x + dx, local_y + dy + 1)):
                if nx < 0 or ny < 0 or nx >= w or ny >= h or height[ny * w + nx] < shape.LAND_HEIGHT:
                    water = True
    if not water:
        return f"start seat {start['seat']}: the kontor is not on the coast"
    return None


def assign(distances, mode):
    """distances[seat][south] is a non-negative integer length.

    min-spread-lex minimises (max-min), then the permutation tuple.
    min-spread-sum-lex then minimises the sum, so equal spreads prefer the
    shorter routes. seat-order keeps permutation identity.
    """
    count = len(distances)
    if mode == 'seat-order':
        perms = (tuple(range(count)),)
    else:
        perms = itertools.permutations(range(count))
    best = None
    chosen = None
    lengths = None
    for perm in perms:
        vals = tuple(distances[seat][perm[seat]] for seat in range(count))
        spread = max(vals) - min(vals)
        total = sum(vals)
        if mode == 'min-spread-lex':
            key = (spread, perm)
        elif mode == 'seat-order':
            key = (perm,)
        else:
            key = (spread, total, perm)
        if best is None or key < best:
            best = key
            chosen = perm
            lengths = vals
    return chosen, lengths


def doubled_dist2(ax, ay, bx2, by2):
    return (2 * ax - bx2) ** 2 + (2 * ay - by2) ** 2


def reach_seat(cx2, cy2, kontors, map_xy):
    """Nearest Kontor, tie to the lower seat. Contested when the map centre is strictly closer."""
    best = None
    best_seat = None
    for seat, (kx, ky) in enumerate(kontors):
        dist = doubled_dist2(kx, ky, cx2, cy2)
        if best is None or dist < best or (dist == best and seat < best_seat):
            best = dist
            best_seat = seat
    centre = doubled_dist2(map_xy[0], map_xy[1], cx2, cy2)
    if centre < best:
        return None
    return best_seat


def nearest_land_dist2(px, py, origin_x, origin_y, height, w, h):
    best = None
    for y in range(h):
        row = y * w
        wy = origin_y + y
        for x in range(w):
            if height[row + x] < shape.LAND_HEIGHT:
                continue
            dist = shape.point_tile_dist2(px, py, origin_x + x, wy)
            if best is None or dist < best:
                best = dist
    return best


def radial_regular(points, origin, permille):
    """True when radii and consecutive angle gaps are each within ±permille of their mean."""
    if len(points) < 3:
        return False
    ox, oy = origin
    radii = []
    angled = []
    for x, y in points:
        dx = x - ox
        dy = y - oy
        radii.append(shape.milli_of(dx * dx + dy * dy))
        angled.append(math.atan2(dy, dx))
    angled.sort()
    gaps = []
    count = len(angled)
    for index, angle in enumerate(angled):
        nxt = angled[(index + 1) % count]
        gap = nxt - angle
        if gap <= 0:
            gap += 2 * math.pi
        gaps.append(int(round(gap * 1000)))
    if any(radius == 0 for radius in radii):
        return False
    return shape.within_mean(radii, permille) and shape.within_mean(gaps, permille)


def _measure(name, values, permille, gate, note=None):
    if values is None:
        return {
            'name': name, 'values': None, 'worst_permille': None,
            'limit': permille, 'ok': False, 'gate': gate, 'note': note or 'undefined',
        }
    worst = shape.worst_permille(values)
    ok = shape.within_mean(values, permille)
    text = note
    if values and sum(values) == 0:
        text = 'all zero'
    return {
        'name': name, 'values': list(values), 'worst_permille': worst,
        'limit': permille, 'ok': ok, 'gate': gate, 'note': text,
    }


def validate(mp, templates, profile=None):
    profile = profile or default_profile()
    problems = []
    large = profile['large']
    seats_n = profile['seats']
    fw, fh = profile['fw'], profile['fh']
    edge = mp['height']
    by_slot = {island['slot']: island for island in mp['islands']}
    sized = {}
    for island in mp['islands']:
        template = templates.get(island['key'])
        if template is None:
            problems.append(f"island slot {island['slot']}: unknown template '{island['key']}'")
            continue
        sized[island['slot']] = (template['w'], template['h'])
        marker = shape.people_marker(island['key'])
        if marker:
            problems.append(f"island slot {island['slot']}: people template {marker}")
    problems.extend(geometry_problems(mp, sized))

    def is_large(island, climate):
        template = templates.get(island['key'])
        return template is not None and template['w'] == large and template['h'] == large and template['climate'] == climate

    nord = [island for island in mp['islands'] if is_large(island, 2)]
    south = [island for island in mp['islands'] if is_large(island, 4)]
    if len(nord) != seats_n:
        problems.append(f"large nord count {len(nord)} is not {seats_n}")
    if len(south) != seats_n:
        problems.append(f"large dschungel count {len(south)} is not {seats_n}")
    if HALFPLANE_CHECK:
        for island in nord:
            template = templates[island['key']]
            if not shape.north_centre_ok(island['y'], template['h'], edge):
                problems.append(
                    f"island slot {island['slot']}: north centre y is below the middle line")
        for island in south:
            if not shape.south_top_ok(island['y'], edge):
                problems.append(
                    f"island slot {island['slot']}: south top y is above the middle line")
    if SHAPE_BY_MASK:
        for group, label in ((nord, 'nord'), (south, 'dschungel')):
            seen = {}
            for island in group:
                digest = templates[island['key']].get('land_sha256')
                if digest is None:
                    problems.append(f"island slot {island['slot']}: no land shape")
                    continue
                if digest in seen:
                    problems.append(
                        f"duplicate land shape {label} slots {seen[digest]} and {island['slot']}")
                else:
                    seen[digest] = island['slot']
    else:
        for group, label in ((nord, 'nord'), (south, 'dschungel')):
            seen = {}
            for island in group:
                if island['key'] in seen:
                    problems.append(
                        f"duplicate template name {label} slots {seen[island['key']]} and {island['slot']}")
                else:
                    seen[island['key']] = island['slot']

    large_slots = {island['slot'] for island in nord + south}
    if CLIMATE_BAND_CHECK:
        bands = {climate: (y0, y1, name) for climate, name, y0, y1 in shape.bands_for(edge)}
        for island in mp['islands']:
            if island['slot'] in large_slots:
                continue
            template = templates.get(island['key'])
            if template is None or template['climate'] not in bands:
                continue
            y0, y1, name = bands[template['climate']]
            if template['h'] <= y1 - y0:
                if island['y'] < y0 or island['y'] + template['h'] > y1:
                    problems.append(
                        f"island slot {island['slot']}: outside {name} band {y0}..{y1}")
            else:
                mid = ((y0 + y1) // 2 - template['h'] // 2) & ~7
                if island['y'] != mid:
                    problems.append(
                        f"island slot {island['slot']}: tall {name} island is not on the band midpoint")

    starts = sorted(mp['starts'], key=lambda item: item['seat'])
    if [item['seat'] for item in starts] != list(range(seats_n)):
        problems.append(f"start seats must be 0..{seats_n - 1}")
    nord_slots = {island['slot'] for island in nord}
    used = []
    for start in starts:
        if start['island'] not in nord_slots:
            problems.append(f"start seat {start['seat']}: not on a large nord island")
        elif start['island'] in used:
            problems.append(f"start island slot {start['island']} is used twice")
        else:
            used.append(start['island'])
        island = by_slot.get(start['island'])
        template = templates.get(island['key']) if island else None
        if island and template:
            message = coast_problem(start, island, template, fw, fh)
            if message:
                problems.append(message)

    measures = []
    assignment = None
    baseline = None
    if len(nord) == seats_n and len(south) == seats_n and len(starts) == seats_n and not any(
            item['island'] not in nord_slots for item in starts):
        south_sorted = sorted(south, key=lambda item: item['slot'])
        kontors = [(start['x'] + fw // 2, start['y'] + fh // 2) for start in starts]
        distances = []
        zero_route = False
        for kx, ky in kontors:
            row = []
            for island in south_sorted:
                template = templates[island['key']]
                dist2 = nearest_land_dist2(
                    kx, ky, island['x'], island['y'], template['height'], template['w'], template['h'])
                if dist2 is None:
                    problems.append(f"island slot {island['slot']}: no land")
                    dist2 = 0
                if dist2 == 0:
                    zero_route = True
                row.append(shape.milli_of(dist2))
            distances.append(row)
        if zero_route:
            problems.append('zero route length')
        chosen, chosen_lengths = assign(distances, profile['assignment'])
        base_perm, base_lengths = assign(distances, 'min-spread-lex')
        assignment = {
            'mode': profile['assignment'],
            'perm': list(chosen),
            'south_slots': [south_sorted[index]['slot'] for index in chosen],
            'lengths': list(chosen_lengths),
            'spread': max(chosen_lengths) - min(chosen_lengths),
            'total': sum(chosen_lengths),
        }
        baseline = {
            'mode': 'min-spread-lex',
            'perm': list(base_perm),
            'south_slots': [south_sorted[index]['slot'] for index in base_perm],
            'lengths': list(base_lengths),
            'spread': max(base_lengths) - min(base_lengths),
            'total': sum(base_lengths),
        }
        home_land = [templates[by_slot[start['island']]['key']]['land'] for start in starts]
        south_land = [templates[south_sorted[index]['key']]['land'] for index in chosen]
        measures.append(_measure('large-land/nord', home_land, profile['large_land_permille'], True))
        measures.append(_measure(
            'large-land/dschungel',
            [templates[island['key']]['land'] for island in south_sorted],
            profile['large_land_permille'], True))
        fair = profile['fairness_permille']
        measures.append(_measure('fairness/home_land', home_land, fair, True))
        measures.append(_measure('fairness/paired_south_land', south_land, fair, True))
        measures.append(_measure('fairness/pair_distance', list(chosen_lengths), fair, True))
        rivals = []
        centres = []
        map_xy = (mp['width'] // 2, mp['height'] // 2)
        for seat, (kx, ky) in enumerate(kontors):
            others = [
                shape.milli_of((kx - ox) ** 2 + (ky - oy) ** 2)
                for other, (ox, oy) in enumerate(kontors) if other != seat
            ]
            rivals.append(min(others))
            centres.append(shape.milli_of((kx - map_xy[0]) ** 2 + (ky - map_xy[1]) ** 2))
        measures.append(_measure('fairness/rival_distance', rivals, fair, True))
        measures.append(_measure('fairness/centre_distance', centres, fair, True))
        sens = profile.get('distance_sensitivity_permille')
        if sens is not None:
            measures.append(_measure('sensitivity/pair_distance', list(chosen_lengths), sens, False))
            measures.append(_measure('sensitivity/rival_distance', rivals, sens, False))
            measures.append(_measure('sensitivity/centre_distance', centres, sens, False))
        reach = {seat: {} for seat in range(seats_n)}
        contested = []
        for island in mp['islands']:
            if island['slot'] in large_slots:
                continue
            template = templates.get(island['key'])
            if template is None:
                continue
            cx2 = 2 * island['x'] + template['w']
            cy2 = 2 * island['y'] + template['h']
            owner = reach_seat(cx2, cy2, kontors, map_xy)
            if owner is None:
                contested.append(island['slot'])
                continue
            climate = template['climate']
            reach[owner][climate] = reach[owner].get(climate, 0) + template['land']
        climates = sorted({climate for seat in reach.values() for climate in seat})
        for climate in climates:
            name = shape.CLIMATE_NAME[climate] if climate < len(shape.CLIMATE_NAME) else str(climate)
            present = [seat for seat in range(seats_n) if climate in reach[seat]]
            if len(present) != seats_n:
                missing = [seat for seat in range(seats_n) if climate not in reach[seat]]
                problems.append(
                    f"reach climate {name} absent for seat {missing[0]}")
                measures.append(_measure(
                    f'fairness/reach/{name}', None, fair, True, note='absent'))
            else:
                values = [reach[seat][climate] for seat in range(seats_n)]
                measures.append(_measure(f'fairness/reach/{name}', values, fair, True))
                filler_limit = profile.get('filler_sensitivity_permille')
                if filler_limit is not None:
                    measures.append(_measure(
                        f'sensitivity/filler/{name}', values, filler_limit, False))
        home_points = []
        for start in starts:
            island = by_slot[start['island']]
            template = templates[island['key']]
            home_points.append((island['x'] + template['w'] // 2, island['y'] + template['h'] // 2))
        if radial_regular(home_points, map_xy, 50):
            problems.append('radial symmetry of the five home islands')
        assignment['contested_slots'] = contested
    structural = [item for item in problems if not item.startswith('reach climate')]
    # Reach absence is a fairness failure. It stays in problems.
    failed_gates = [item['name'] for item in measures if item['gate'] and not item['ok']]
    ok = not problems and not failed_gates
    return {
        'ok': ok,
        'profile': profile['id'],
        'problems': problems,
        'structural': structural,
        'measures': measures,
        'assignment': assignment,
        'baseline': baseline,
        'failed_gates': failed_gates,
    }


def format_report(report):
    lines = []
    status = 'PASS' if report['ok'] else 'FAIL'
    lines.append(f"{status} profile={report['profile']}")
    for problem in report['problems']:
        lines.append(f"problem: {problem}")
    for measure in report['measures']:
        gate = 'gate' if measure['gate'] else 'sensitivity'
        flag = 'PASS' if measure['ok'] else 'FAIL'
        values = measure['values']
        value_text = '-' if values is None else ','.join(str(value) for value in values)
        lines.append(
            f"measure {measure['name']} {flag} {gate} limit={measure['limit']} "
            f"worst={measure['worst_permille']} values={value_text}"
            + (f" note={measure['note']}" if measure['note'] else ''))
    if report.get('assignment'):
        chosen = report['assignment']
        base = report['baseline']
        lines.append(
            f"assignment {chosen['mode']} south={chosen['south_slots']} "
            f"lengths={list(chosen['lengths'])} spread={chosen['spread']} total={chosen['total']}")
        lines.append(
            f"baseline {base['mode']} south={base['south_slots']} "
            f"lengths={list(base['lengths'])} spread={base['spread']} total={base['total']}")
        if chosen['perm'] != base['perm']:
            lines.append('assignment differs from min-spread-lex; the shorter sum was preferred')
    if report['ok']:
        lines.append(
            f"PASS profile={report['profile']} "
            f"large_land_permille={LARGE_LAND_PERMILLE} fairness_permille={FAIRNESS_PERMILLE}")
    else:
        lines.append(
            f"FAIL profile={report['profile']} gates={','.join(report['failed_gates']) or 'structural'}")
    return '\n'.join(lines) + '\n'


def load_templates(game, keys, heads=None):
    import insel
    root = Path(game) / 'Scenes' / 'Islands'
    found = {}
    for path in root.rglob('*.scp'):
        key = path.relative_to(root).as_posix().upper().replace('\\', '/')
        if key in keys:
            found[key] = path
    catalog = {}
    for key in keys:
        path = found.get(key)
        if path is None:
            continue
        isl = insel.islands(path.read_bytes())[0]
        height = bytes(insel.grid(isl, 'HEIGHT', 1))
        occupied = set()
        placements = []
        for name in ('GELAENDE', 'FLUESSE', 'PROD2'):
            for x, y, _bw, _bh, data in insel.blocks(isl, name):
                for index in range(len(data) // 6):
                    item = data[index * 6:(index + 1) * 6]
                    if heads is None:
                        # Historical profile: one origin tile. Ship checks pass heads.
                        occupied.add((x + item[4], y + item[5]))
                        continue
                    word = item[0] | (item[1] << 8)
                    oid = word & 0xFFF
                    rotation = (word >> 14) & 3
                    if oid not in heads:
                        raise KeyError(f'ANLAGEN id {oid} has no HEAD')
                    hw, hh = heads[oid]
                    fw, fh = shape.footprint_size(hw, hh, rotation)
                    ox, oy = x + item[4], y + item[5]
                    placements.append((ox, oy, fw, fh))
                    for dy in range(fh):
                        ty = oy + dy
                        if ty < 0 or ty >= isl['height']:
                            continue
                        for dx in range(fw):
                            tx = ox + dx
                            if 0 <= tx < isl['width']:
                                occupied.add((tx, ty))
        info = next((payload for rec_name, payload in isl['records'] if rec_name == 'INFO'), b'')
        land_hash = None
        if isl['width'] == isl['height']:
            mask, _oriented = shape.shape_ids(height, isl['width'])
            land_hash = shape.sha256_hex(mask)
        catalog[key] = {
            'w': isl['width'],
            'h': isl['height'],
            'climate': info[0] if info else None,
            'land': shape.land_count(height),
            'height': height,
            'occupied': occupied,
            'land_sha256': land_hash,
        }
        if heads is not None:
            catalog[key]['placements'] = tuple(placements)
    return catalog


def main(argv):
    sys.stdout.reconfigure(newline='\n')
    if len(argv) < 2 or argv[1] != 'validate':
        print('usage: map2_profile.py validate <file> --game-dir <dir>', file=sys.stderr)
        return 2
    game = None
    path = None
    args = argv[2:]
    while args:
        if args[0] == '--game-dir' and len(args) >= 2:
            game = args[1]
            args = args[2:]
        elif path is None:
            path = args[0]
            args = args[1:]
        else:
            print('usage: map2_profile.py validate <file> --game-dir <dir>', file=sys.stderr)
            return 2
    if path is None or game is None:
        print('usage: map2_profile.py validate <file> --game-dir <dir>', file=sys.stderr)
        return 2
    mp, err = fixed_map.parse(Path(path).read_bytes())
    if err:
        print(f'error: line {err[0]}: {err[1]}')
        return 1
    keys = {island['key'] for island in mp['islands']}
    catalog = load_templates(game, keys)
    report = validate(mp, catalog, default_profile())
    sys.stdout.write(format_report(report))
    return 0 if report['ok'] else 1


if __name__ == '__main__':
    try:
        os.nice(10)
    except OSError:
        pass
    sys.exit(main(sys.argv))
