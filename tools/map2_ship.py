"""Geometric checks for map2-ship-start-v2. Not engine certification.

Movement is 4-connected on water tiles. Each step costs 1 tile. Diagonals are
not moves, so the cost is Manhattan, not a Euclidean length and not an upper
bound produced by a unit-cost diagonal. Clearance is the Chebyshev gap from
the spawn tile to the nearest HEIGHT>=16 tile. It is not applied along the
route.

A founding site is a 4x4 of HEIGHT>=16 clear of ANLAGEN HEAD footprints
(odd rotations swap the axes; sprite pixels are not occupancy). One of its
sides must touch a water tile in the same ocean component as the spawn.
The route ends on that water tile. An enclosed lake does not qualify.
The site's own connected free-land component must hold at least min_city
tiles. A larger component elsewhere on the island is not credited.
min_city is provisional tool policy, not an original rule and not an
equality target. Obstacle removability is unresolved; blocking the whole
documented footprint is the conservative policy.

The distinct-island witness is a matching of those linked edges. It is not
ownership. Optional sharing is reported and is not added to the matching.
An island with no such site is a diagnostic, not a failed candidate.

  python3 -I tools/map2_ship.py check <manifest> --game-dir <dir> [--json <path>]
"""
import hashlib
import json
import os
import struct
import sys
from array import array
from collections import deque
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import map2_manifest as mf
import map2_profile
import map2_shape as shape

CLIMATES = tuple(shape.CLIMATE_NAME)
INF = 0xFFFFFFFF
_HEADS = {}


def tile_gap(tile, origin, size):
    """Chebyshev component from a tile index to a half-open interval."""
    if origin <= tile < origin + size:
        return 0
    if tile < origin:
        return origin - tile
    return tile - (origin + size - 1)


def neighbours(cx, cy, sea):
    """4-connected steps. A diagonal is two of these, never one."""
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nxt = (cx + dx, cy + dy)
        if nxt in sea:
            yield nxt


def bfs(start, sea):
    dist = {start: 0}
    queue = deque([start])
    while queue:
        cell = queue.popleft()
        for nxt in neighbours(cell[0], cell[1], sea):
            if nxt not in dist:
                dist[nxt] = dist[cell] + 1
                queue.append(nxt)
    return dist


def blocked_tiles(template):
    """Full footprint tiles when the template carries placements, else origins."""
    if 'placements' in template:
        out = set()
        for x, y, fw, fh in template['placements']:
            for dy in range(fh):
                for dx in range(fw):
                    out.add((x + dx, y + dy))
        return out
    return set(template.get('occupied') or ())


def adjacent_water(template, x, y):
    """Local tiles beside the 4x4 that are not HEIGHT>=16 land. May lie outside."""
    w, h = template['w'], template['h']
    height = template['height']
    found = []
    seen = set()
    for dy in range(4):
        for dx in range(4):
            for nx, ny in ((x + dx - 1, y + dy), (x + dx + 1, y + dy),
                           (x + dx, y + dy - 1), (x + dx, y + dy + 1)):
                if (nx, ny) in seen:
                    continue
                if 0 <= nx < w and 0 <= ny < h and height[ny * w + nx] >= shape.LAND_HEIGHT:
                    continue
                seen.add((nx, ny))
                found.append((nx, ny))
    return found


def founding_sites(template):
    w, h = template['w'], template['h']
    height = template['height']
    occupied = blocked_tiles(template)
    sites = []
    span = 4
    if w < span or h < span:
        return sites
    for y in range(h - span + 1):
        for x in range(w - span + 1):
            free = True
            for dy in range(span):
                row = (y + dy) * w
                for dx in range(span):
                    if height[row + x + dx] < shape.LAND_HEIGHT or (x + dx, y + dy) in occupied:
                        free = False
                        break
                if not free:
                    break
            if free and adjacent_water(template, x, y):
                sites.append((x, y))
    return sites


def components(template):
    w, h = template['w'], template['h']
    height = template['height']
    occupied = blocked_tiles(template)
    seen = [False] * (w * h)
    sizes = []
    index_of = {}
    for start in range(w * h):
        if seen[start]:
            continue
        sx, sy = start % w, start // w
        if height[start] < shape.LAND_HEIGHT or (sx, sy) in occupied:
            seen[start] = True
            continue
        stack = [start]
        seen[start] = True
        count = 0
        cells = []
        while stack:
            cur = stack.pop()
            count += 1
            cells.append(cur)
            x, y = cur % w, cur // w
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if nx < 0 or ny < 0 or nx >= w or ny >= h:
                    continue
                nxt = ny * w + nx
                if seen[nxt]:
                    continue
                if height[nxt] < shape.LAND_HEIGHT or (nx, ny) in occupied:
                    seen[nxt] = True
                    continue
                seen[nxt] = True
                stack.append(nxt)
        sizes.append(count)
        for cur in cells:
            index_of[cur] = len(sizes) - 1
    return sizes, index_of


def site_room(template, site, sizes, index_of):
    w = template['w']
    x, y = site
    idx = index_of.get((y + 1) * w + (x + 1), index_of.get(y * w + x))
    if idx is None:
        return 0
    return sizes[idx]


def _component_id(template, x, y, index_of):
    w = template['w']
    return index_of.get((y + 1) * w + (x + 1), index_of.get(y * w + x))


def matching(options):
    """options[seat] = island ids. Returns (seat, island) pairs. One island once."""
    pair = {}

    def dfs(seat, seen):
        for island in options[seat]:
            if island in seen:
                continue
            seen.add(island)
            if island not in pair or dfs(pair[island], seen):
                pair[island] = seat
                return True
        return False

    for seat in range(5):
        dfs(seat, set())
    return tuple(sorted((seat, island) for island, seat in pair.items()))


def _prepared(doc, templates):
    islands = []
    for island in doc['islands']:
        template = templates[island['key']]
        islands.append({
            'id': island['id'],
            'x': island['x'],
            'y': island['y'],
            'w': template['w'],
            'h': template['h'],
            'key': island['key'],
            'climate': template['climate'],
            'template': template,
        })
    return islands


def geometry_problems(doc, islands, options):
    problems = []
    edge = doc['edge'][0]
    if doc['edge'][0] != doc['edge'][1]:
        problems.append('edge is not square')
    boxes = []
    for island in islands:
        if options['envelope'] and (island['x'] % 8 or island['y'] % 8):
            problems.append(f"island {island['id']} origin is not a multiple of 8")
        if island['x'] < 0 or island['y'] < 0 or island['x'] + island['w'] > edge or island['y'] + island['h'] > edge:
            problems.append(f"island {island['id']} is outside the world")
        box = (island['x'], island['y'], island['w'], island['h'])
        for earlier in boxes:
            if shape.rect_overlap(box, earlier[0]):
                problems.append(f"island {island['id']} overlaps island {earlier[1]}")
        boxes.append((box, island['id']))
        marker = shape.people_marker(island['key'])
        if marker:
            problems.append(f"island {island['id']} people template {marker}")
    if not options['envelope']:
        return problems
    if doc['edge'] != [1431, 1431]:
        problems.append('edge is not 1431')
    nord, south = [], []
    for island in islands:
        large = island['w'] == shape.LARGE and island['h'] == shape.LARGE
        if large and island['climate'] == 2:
            nord.append(island)
            if not shape.north_centre_ok(island['y'], island['h'], edge):
                problems.append(f"island {island['id']} northern centre is south of the line")
        elif large and island['climate'] == 4:
            south.append(island)
            if not shape.south_top_ok(island['y'], edge):
                problems.append(f"island {island['id']} southern top is north of the line")
        else:
            bands = {climate: (y0, y1, name) for climate, name, y0, y1 in shape.bands_for(edge)}
            if island['climate'] in bands:
                y0, y1, name = bands[island['climate']]
                if island['h'] <= y1 - y0:
                    if island['y'] < y0 or island['y'] + island['h'] > y1:
                        problems.append(f"island {island['id']} is outside the {name} band")
    if len(nord) != 5:
        problems.append(f'large nord count {len(nord)} is not 5')
    if len(south) != 5:
        problems.append(f'large dschungel count {len(south)} is not 5')
    for group, label in ((nord, 'nord'), (south, 'dschungel')):
        seen = {}
        for island in group:
            digest = island['template'].get('land_sha256')
            if digest in seen:
                problems.append(f'duplicate {label} land shape islands {seen[digest]} and {island["id"]}')
            else:
                seen[digest] = island['id']
    return problems


def _options(doc, options):
    policy = doc.get('policy') or mf.default_policy()
    made = {
        'clearance': policy['clearance'],
        'max_route_tiles': policy['max_route_tiles'],
        'min_city': policy['min_city'],
        'share_gap': policy['share_gap'],
        'share_min_side': policy['share_min_side'],
        'envelope': True,
    }
    if options:
        made.update(options)
    return made


def _land_gap(tx, ty, island):
    rg = max(tile_gap(tx, island['x'], island['w']), tile_gap(ty, island['y'], island['h']))
    if rg > 0:
        return rg
    height = island['template']['height']
    w = island['w']
    lx, ly = tx - island['x'], ty - island['y']
    best = None
    for index, value in enumerate(height):
        if value < shape.LAND_HEIGHT:
            continue
        gap = max(abs(index % w - lx), abs(index // w - ly))
        if gap == 0:
            return 0
        best = gap if best is None else min(best, gap)
    return 0 if best is None else best


def _clearance(tx, ty, islands, needed):
    best = None
    for island in islands:
        rg = max(tile_gap(tx, island['x'], island['w']), tile_gap(ty, island['y'], island['h']))
        if rg >= needed:
            gap = rg
        else:
            gap = _land_gap(tx, ty, island) if rg == 0 else _exact_gap(tx, ty, island)
        best = gap if best is None else min(best, gap)
        if best == 0:
            return 0
    return 0 if best is None else best


def _exact_gap(tx, ty, island):
    """Chebyshev distance to the nearest land tile. The rectangle is only a bound."""
    height = island['template']['height']
    w = island['w']
    ox, oy = island['x'], island['y']
    best = None
    for index, value in enumerate(height):
        if value < shape.LAND_HEIGHT:
            continue
        gap = max(abs(ox + index % w - tx), abs(oy + index // w - ty))
        if gap == 0:
            return 0
        best = gap if best is None else min(best, gap)
    return best if best is not None else INF


def _water_mask(edge, islands):
    blocked = bytearray(edge * edge)
    for island in islands:
        height = island['template']['height']
        w = island['w']
        ox, oy = island['x'], island['y']
        for y in range(island['h']):
            row = y * w
            base = (oy + y) * edge + ox
            for x in range(w):
                if height[row + x] >= shape.LAND_HEIGHT:
                    blocked[base + x] = 1
    return blocked


def _bfs_tiles(sx, sy, blocked, edge):
    dist = array('I', [INF]) * (edge * edge)
    start = sy * edge + sx
    if blocked[start]:
        return dist
    dist[start] = 0
    queue = deque([start])
    while queue:
        cur = queue.popleft()
        d = dist[cur] + 1
        x = cur - (cur // edge) * edge
        y = cur // edge
        if x > 0:
            nxt = cur - 1
            if not blocked[nxt] and dist[nxt] == INF:
                dist[nxt] = d
                queue.append(nxt)
        if x + 1 < edge:
            nxt = cur + 1
            if not blocked[nxt] and dist[nxt] == INF:
                dist[nxt] = d
                queue.append(nxt)
        if y > 0:
            nxt = cur - edge
            if not blocked[nxt] and dist[nxt] == INF:
                dist[nxt] = d
                queue.append(nxt)
        if y + 1 < edge:
            nxt = cur + edge
            if not blocked[nxt] and dist[nxt] == INF:
                dist[nxt] = d
                queue.append(nxt)
    return dist


def _annotate(island):
    template = island['template']
    sizes, index_of = components(template)
    records = []
    for x, y in founding_sites(template):
        water = adjacent_water(template, x, y)
        if not water:
            continue
        records.append({
            'x': x,
            'y': y,
            'city': site_room(template, (x, y), sizes, index_of),
            'comp': _component_id(template, x, y, index_of),
            'water': water,
        })
    return records


def _ocean_waters(island, site, edge, blocked):
    found = []
    for lx, ly in site['water']:
        wx, wy = island['x'] + lx, island['y'] + ly
        if wx < 0 or wy < 0 or wx >= edge or wy >= edge:
            continue
        if blocked[wy * edge + wx]:
            continue
        found.append((wx, wy))
    return found


def validate(doc, templates, options=None):
    made = _options(doc, options)
    missing = [island['key'] for island in doc['islands'] if island['key'] not in templates]
    problems = [f'unknown template {key}' for key in missing]
    if missing:
        return _empty(doc, problems, made)
    islands = _prepared(doc, templates)
    problems.extend(geometry_problems(doc, islands, made))
    edge = doc['edge'][0]
    blocked = _water_mask(edge, islands)
    annotated = {island['id']: _annotate(island) for island in islands}
    evaluated = sum(len(sites) for sites in annotated.values())
    feasible = {seat: [] for seat in range(5)}
    detours = []
    ocean = bytearray(edge * edge)
    for spawn in doc['spawns']:
        tile_x, tile_y = spawn['x'] // mf.MILLI, spawn['y'] // mf.MILLI
        if not (0 <= tile_x < edge and 0 <= tile_y < edge) or blocked[tile_y * edge + tile_x]:
            problems.append(f"spawn {spawn['seat']} is not clear sea")
            continue
        if _clearance(tile_x, tile_y, islands, made['clearance']) < made['clearance']:
            problems.append(f"spawn {spawn['seat']} is not clear sea")
            continue
        dist = _bfs_tiles(tile_x, tile_y, blocked, edge)
        evaluated += edge * edge
        for island in islands:
            best = None
            linked = False
            for site in annotated[island['id']]:
                if site['city'] < made['min_city']:
                    continue
                for wx, wy in _ocean_waters(island, site, edge, blocked):
                    cost = dist[wy * edge + wx]
                    if cost == INF:
                        continue
                    linked = True
                    ocean[wy * edge + wx] = 1
                    if best is None or cost < best[0]:
                        best = (cost, site, wx, wy)
            if best is None:
                continue
            cost, site, wx, wy = best
            detail = {
                'seat': spawn['seat'],
                'island': island['id'],
                'cost': cost,
                'spawn': (tile_x, tile_y),
                'site': (island['x'] + site['x'], island['y'] + site['y']),
                'water': (wx, wy),
                'city': site['city'],
            }
            if cost > made['max_route_tiles']:
                detours.append(detail)
                continue
            feasible[spawn['seat']].append(detail)
        feasible[spawn['seat']].sort(key=lambda item: (item['cost'], item['island']))
    witness_ids = matching({
        seat: [item['island'] for item in feasible[seat]] for seat in range(5)
    })
    by_pair = {(item['seat'], item['island']): item for seat in feasible for item in feasible[seat]}
    witness = []
    for seat, island in witness_ids:
        witness.append(by_pair[(seat, island)])
    if len(witness_ids) != 5:
        problems.append(f'distinct-island witness covers {len(witness_ids)} of 5')
    diagnostics = []
    for island in islands:
        ocean_site = False
        for site in annotated[island['id']]:
            if site['city'] < made['min_city']:
                continue
            for wx, wy in _ocean_waters(island, site, edge, blocked):
                if ocean[wy * edge + wx]:
                    ocean_site = True
        if not ocean_site:
            diagnostics.append(f"island {island['id']} has no eligible founding site")
    shares = _shares(islands, annotated, made, edge, blocked, ocean)
    present = {climate: [] for climate in range(len(CLIMATES))}
    for island in islands:
        if island['climate'] in present:
            present[island['climate']].append(island['id'])
    for climate, name in enumerate(CLIMATES):
        if not present[climate]:
            diagnostics.append(f'climate {name} absent everywhere')
            continue
        who = []
        for seat in range(5):
            ids = {item['island'] for item in feasible[seat]}
            if any(island_id in ids for island_id in present[climate]):
                who.append(seat)
        diagnostics.append(f"climate {name} reachable from seats {who}")
    for item in detours:
        diagnostics.append(f"detour seat {item['seat']} island {item['island']} cost {item['cost']}")
    return {
        'profile': mf.PROFILE_ID,
        'problems': problems,
        'witness': tuple((item['seat'], item['island']) for item in witness),
        'witness_detail': tuple(witness),
        'feasible': feasible,
        'detours': tuple((item['seat'], item['island'], item['cost']) for item in detours),
        'shares': shares,
        'diagnostics': diagnostics,
        'sites': {island['id']: len(annotated[island['id']]) for island in islands},
        'site_records': [
            {
                'island': island['id'],
                'sites': [
                    {
                        'x': island['x'] + site['x'],
                        'y': island['y'] + site['y'],
                        'city': site['city'],
                    }
                    for site in annotated[island['id']]
                ],
            }
            for island in islands
        ],
        'detour_detail': tuple(detours),
        'land': {island['id']: shape.land_count(island['template']['height']) for island in islands},
        'evaluated': evaluated,
        'cities': 0,
        'engine': 'NOT_RUN_ENGINE',
        'options': made,
        'policy': {
            'grid': 'tile',
            'metric': 'manhattan-4',
            'clearance_scope': 'spawn',
            'obstruction': 'anlagen-head-conservative',
        },
    }


def _shares(islands, annotated, made, edge, blocked, ocean):
    found = []
    for island in islands:
        if island['w'] < made['share_min_side'] or island['h'] < made['share_min_side']:
            continue
        sites = []
        for site in annotated[island['id']]:
            if site['city'] < made['min_city']:
                continue
            if any(ocean[wy * edge + wx] for wx, wy in _ocean_waters(island, site, edge, blocked)):
                sites.append(site)
        ok = False
        for i, left in enumerate(sites):
            for right in sites[i + 1:]:
                gap = max(abs((left['x'] + 2) - (right['x'] + 2)), abs((left['y'] + 2) - (right['y'] + 2)))
                if gap < made['share_gap']:
                    continue
                same = left['comp'] is not None and left['comp'] == right['comp']
                if same and left['city'] >= 2 * made['min_city']:
                    ok = True
                elif not same and left['city'] >= made['min_city'] and right['city'] >= made['min_city']:
                    ok = True
        if ok:
            found.append(island['id'])
    return found


def _empty(doc, problems, made):
    return {
        'profile': mf.PROFILE_ID, 'problems': problems, 'witness': (), 'witness_detail': (),
        'feasible': {}, 'detours': (), 'shares': [], 'diagnostics': [], 'sites': {},
        'land': {}, 'evaluated': 0, 'cities': 0, 'engine': 'NOT_RUN_ENGINE', 'options': made,
        'policy': {},
    }


def format_report(report):
    lines = []
    status = 'PASS_TOOL_CHECKS' if not report['problems'] else 'QUESTION'
    lines.append(f'{status} profile={report["profile"]}')
    for problem in report['problems']:
        lines.append(f'problem: {problem}')
    for item in report.get('witness_detail') or ():
        sx, sy = item['site']
        wx, wy = item['water']
        lines.append(
            f"witness seat {item['seat']} island {item['island']} cost {item['cost']} "
            f"site {sx},{sy} water {wx},{wy} city {item['city']}")
    if not report.get('witness_detail'):
        for seat, island in report['witness']:
            lines.append(f'witness seat {seat} island {island}')
    for island in report['shares']:
        lines.append(f'optional share island {island}')
    for line in report['diagnostics']:
        lines.append(f'diagnostic: {line}')
    lines.append(f"cities {report['cities']}")
    lines.append(report['engine'])
    return '\n'.join(lines) + '\n'


def anlagen_heads(path):
    """HEAD width, height by ANLAGEN id. Does not decode sprites."""
    path = str(path)
    cached = _HEADS.get(path)
    if cached is not None:
        return cached
    buf = open(path, 'rb').read()
    digest = hashlib.sha256(buf).hexdigest()
    if buf[:5] != b'ANNO\0':
        raise mf.ManifestError('Anlagen.dat has no ANNO root')
    top_end = 16 + struct.unpack_from('<I', buf, 12)[0]
    anlagen = None
    for name, payload, size in _records(buf, 16, top_end):
        if name == b'ANLAGEN':
            anlagen = (payload, size)
    if anlagen is None:
        raise mf.ManifestError('Anlagen.dat has no ANLAGEN chunk')
    heads = {}
    start, size = anlagen
    for name, payload, rec_size in _records(buf, start, start + size):
        if name != b'ENTRY' or rec_size < 8:
            continue
        eid = struct.unpack_from('<I', buf, payload + 4)[0]
        for child, child_at, child_size in _records(buf, payload + 8, payload + rec_size):
            if child == b'HEAD' and child_size >= 2:
                heads[eid] = (buf[child_at], buf[child_at + 1])
    cached = (heads, digest)
    _HEADS[path] = cached
    return cached


def _records(buf, start, end):
    p = start
    while p + 16 <= end:
        name = buf[p:p + 12].split(b'\0', 1)[0]
        size = struct.unpack_from('<I', buf, p + 12)[0]
        payload = p + 16
        if payload + size > end:
            raise mf.ManifestError('Anlagen.dat record overruns its parent')
        yield name, payload, size
        p = payload + size


def public_report(doc, report):
    """Machine-readable check. Integer keys become lists so the JSON is stable."""
    def cells(value):
        if isinstance(value, tuple):
            return [cells(item) for item in value]
        if isinstance(value, list):
            return [cells(item) for item in value]
        if isinstance(value, dict):
            return {str(key) if isinstance(key, int) else key: cells(item) for key, item in value.items()}
        return value

    alternatives = [item for seat in range(5) for item in report['feasible'].get(seat, ())]
    return {
        'name': doc['name'],
        'content_hash': doc['content_hash'],
        'profile': doc['profile'],
        'version': doc['version'],
        'policy': doc['policy'],
        'excluded': list(doc['excluded']),
        'anlagen_sha256': report.get('anlagen_sha256'),
        'template_sha256': report.get('template_sha256') or {},
        'problems': list(report['problems']),
        'witness': cells(report['witness_detail']),
        'alternatives': cells(alternatives),
        'detours': cells(report.get('detour_detail') or ()),
        'shares': list(report['shares']),
        'diagnostics': list(report['diagnostics']),
        'site_records': report.get('site_records') or [],
        'cities': 0,
        'engine': 'NOT_RUN_ENGINE',
    }


def load_and_check(path, game):
    doc = mf.parse(mf.PathText(path))
    if not mf.baseline_ok(doc):
        raise mf.ManifestError('loadout is not the proposed 03 baseline')
    heads, anlagen_sha = anlagen_heads(Path(game) / 'Anlagen.dat')
    keys = {island['key'] for island in doc['islands']}
    templates = map2_profile.load_templates(game, keys, heads)
    report = validate(doc, templates)
    report['anlagen_sha256'] = anlagen_sha
    report['template_sha256'] = {
        island['key']: templates[island['key']].get('land_sha256') for island in doc['islands']
    }
    return doc, report, templates


def main(argv):
    try:
        os.nice(10)
    except OSError:
        pass
    json_path = None
    args = list(argv[1:])
    if '--json' in args:
        index = args.index('--json')
        if index + 1 >= len(args):
            print('usage: map2_ship.py check <manifest> --game-dir <dir> [--json <path>]', file=sys.stderr)
            return 2
        json_path = args[index + 1]
        del args[index:index + 2]
    if len(args) != 4 or args[0] != 'check' or args[2] != '--game-dir':
        print('usage: map2_ship.py check <manifest> --game-dir <dir> [--json <path>]', file=sys.stderr)
        return 2
    try:
        doc, report, _templates = load_and_check(args[1], args[3])
    except (mf.ManifestError, KeyError) as err:
        print(f'error: {err}')
        return 1
    if json_path:
        payload = json.dumps(public_report(doc, report), sort_keys=True, separators=(',', ':'))
        with open(json_path, 'w', encoding='ascii', newline='\n') as handle:
            handle.write(payload)
            handle.write('\n')
    sys.stdout.write(format_report(report))
    return 0 if not report['problems'] else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
