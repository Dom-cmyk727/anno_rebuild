"""Geometric checks for map2-ship-start-v1. Not engine certification.

Sea model, all integers:
- Grid step defaults to 8 tiles. Cell (c, r) covers tiles
  [c*step, min(edge, (c+1)*step)) by the same in y.
- A cell is blocked when its tile span intersects an island rectangle.
  Bays inside a rectangle are not sailed. Reaching the rectangle is not a
  path to a particular beach.
- Moves are 4-connected. A diagonal step is legal only when both orthogonal
  neighbours are open. Cost is steps times step, an upper bound on the coarse
  path, not a sailing time.
- A spawn tile must be at least `clearance` tiles, Chebyshev, from every
  island tile. Distance 0 is inside a rectangle.

A founding site is a 4x4 of HEIGHT >= 16, clear of recorded obstacle origins,
with one side on water (outside the island or HEIGHT < 16). Obstacle records
block their origin tile only. Graphic extents beyond that tile are not in the
record and are not invented. Connected 4-neighbour free land is a city-space
estimate, not a service area.

The distinct-island witness is a matching, not ownership. Optional sharing is
reported and is not added to that matching.

  python3 -I tools/map2_ship.py check <manifest> --game-dir <dir>
"""
import os
import sys
from collections import deque
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import map2_manifest as mf
import map2_profile
import map2_shape as shape

# Mutants in the verification step flip these and restore them.
COUNT_OBSTACLES = True
REQUIRE_DISTINCT = True
REPORT_ABSENT_CLIMATES = True

CLIMATES = tuple(shape.CLIMATE_NAME)
DEFAULTS = {
    'step': 8,
    'clearance': 8,
    'max_route_tiles': 400,
    'min_city': 256,
    'share_gap': 32,
    'envelope': True,
}


def tile_gap(tile, origin, size):
    """Chebyshev component from a tile index to a half-open interval."""
    if origin <= tile < origin + size:
        return 0
    if tile < origin:
        return origin - tile
    return tile - (origin + size - 1)


def point_clearance(tile_x, tile_y, islands):
    best = None
    for island in islands:
        gap = max(tile_gap(tile_x, island['x'], island['w']),
                  tile_gap(tile_y, island['y'], island['h']))
        best = gap if best is None else min(best, gap)
    return best


def cell_blocked(cx, cy, step, edge, islands):
    x0, y0 = cx * step, cy * step
    x1, y1 = min(edge, x0 + step), min(edge, y0 + step)
    for island in islands:
        if x0 < island['x'] + island['w'] and island['x'] < x1 and y0 < island['y'] + island['h'] and island['y'] < y1:
            return True
    return False


def neighbours(cx, cy, sea):
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nxt = (cx + dx, cy + dy)
        if nxt in sea:
            yield nxt
    for dx, dy in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        nxt = (cx + dx, cy + dy)
        if nxt in sea and (cx + dx, cy) in sea and (cx, cy + dy) in sea:
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


def touches(cx, cy, step, edge, island):
    x0, y0 = cx * step, cy * step
    x1, y1 = min(edge, x0 + step), min(edge, y0 + step)
    ix, iy, iw, ih = island['x'], island['y'], island['w'], island['h']
    y_overlap = y0 < iy + ih and iy < y1
    x_overlap = x0 < ix + iw and ix < x1
    return (y_overlap and (x1 == ix or x0 == ix + iw)) or (x_overlap and (y1 == iy or y0 == iy + ih))


def founding_sites(template):
    w, h = template['w'], template['h']
    height = template['height']
    occupied = template['occupied'] if COUNT_OBSTACLES else set()
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
            if not free:
                continue
            water = False
            for dy in range(span):
                for dx in range(span):
                    for nx, ny in ((x + dx - 1, y + dy), (x + dx + 1, y + dy),
                                   (x + dx, y + dy - 1), (x + dx, y + dy + 1)):
                        if nx < 0 or ny < 0 or nx >= w or ny >= h or height[ny * w + nx] < shape.LAND_HEIGHT:
                            water = True
            if water:
                sites.append((x, y))
    return sites


def components(template):
    w, h = template['w'], template['h']
    height = template['height']
    occupied = template['occupied'] if COUNT_OBSTACLES else set()
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
    idx = index_of.get((y + 1) * w + (x + 1))
    if idx is None:
        idx = index_of.get(y * w + x)
    if idx is None:
        return 0
    return sizes[idx]


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


def validate(doc, templates, options=None):
    made = dict(DEFAULTS)
    if options:
        made.update(options)
    missing = [island['key'] for island in doc['islands'] if island['key'] not in templates]
    problems = [f'unknown template {key}' for key in missing]
    if missing:
        return _empty(doc, problems, made)
    islands = _prepared(doc, templates)
    problems.extend(geometry_problems(doc, islands, made))
    edge = doc['edge'][0]
    step = made['step']
    cols = (edge + step - 1) // step
    sea = set()
    for cy in range(cols):
        for cx in range(cols):
            if not cell_blocked(cx, cy, step, edge, islands):
                sea.add((cx, cy))
    evaluated = 0
    spawn_dist = {}
    for spawn in doc['spawns']:
        tile_x, tile_y = spawn['x'] // mf.MILLI, spawn['y'] // mf.MILLI
        gap = point_clearance(tile_x, tile_y, islands)
        if gap is None or gap < made['clearance']:
            problems.append(f"spawn {spawn['seat']} is not clear sea")
            continue
        cell = (tile_x // step, tile_y // step)
        if cell not in sea:
            problems.append(f"spawn {spawn['seat']} is not clear sea")
            continue
        dist = bfs(cell, sea)
        evaluated += len(dist)
        spawn_dist[spawn['seat']] = dist
    site_info = {}
    for island in islands:
        sites = founding_sites(island['template'])
        sizes, index_of = components(island['template'])
        rooms = [site_room(island['template'], site, sizes, index_of) for site in sites]
        site_info[island['id']] = (sites, rooms, sizes)
        evaluated += len(sites)
    touch = {}
    for island in islands:
        cells = [cell for cell in sea if touches(cell[0], cell[1], step, edge, island)]
        touch[island['id']] = cells
    feasible = {seat: [] for seat in range(5)}
    detours = []
    for seat, dist in spawn_dist.items():
        for island in islands:
            costs = [dist[cell] for cell in touch[island['id']] if cell in dist]
            if not costs:
                continue
            sites, _rooms, _sizes = site_info[island['id']]
            if not sites:
                continue
            cost = min(costs) * step
            if cost > made['max_route_tiles']:
                detours.append((seat, island['id'], cost))
                continue
            feasible[seat].append((cost, island['id']))
        feasible[seat].sort()
    for island in islands:
        sites, _rooms, _sizes = site_info[island['id']]
        if sites:
            continue
        if any(cell in spawn_dist.get(seat, {}) for seat in range(5) for cell in touch[island['id']]):
            problems.append(f"island {island['id']} has no founding site")
    witness = matching({seat: [island for _cost, island in feasible[seat]] for seat in range(5)})
    if REQUIRE_DISTINCT and len(witness) != 5:
        problems.append(f'distinct-island witness covers {len(witness)} of 5')
    shares = _shares(islands, site_info, made)
    diagnostics = []
    present = {climate: [] for climate in range(len(CLIMATES))}
    for island in islands:
        if island['climate'] in present:
            present[island['climate']].append(island['id'])
    for climate, name in enumerate(CLIMATES):
        if not present[climate]:
            if REPORT_ABSENT_CLIMATES:
                diagnostics.append(f'climate {name} absent everywhere')
            continue
        who = [seat for seat in range(5) if any(island_id in present[climate] for _cost, island_id in feasible[seat])]
        diagnostics.append(f"climate {name} reachable from seats {who}")
    for seat, island, cost in detours:
        diagnostics.append(f'detour seat {seat} island {island} cost {cost}')
    return {
        'profile': mf.PROFILE_ID,
        'problems': problems,
        'witness': witness,
        'feasible': feasible,
        'shares': shares,
        'diagnostics': diagnostics,
        'sites': {island['id']: len(site_info[island['id']][0]) for island in islands},
        'land': {island['id']: shape.land_count(island['template']['height']) for island in islands},
        'evaluated': evaluated,
        'cities': 0,
        'engine': 'NOT_RUN_ENGINE',
        'options': made,
    }


def _shares(islands, site_info, made):
    found = []
    for island in islands:
        sites, _rooms, _sizes = site_info[island['id']]
        if len(sites) < 2:
            continue
        template = island['template']
        sizes, index_of = components(template)
        w = template['w']
        rooms = []
        idents = []
        for x, y in sites:
            ident = index_of.get((y + 1) * w + (x + 1), index_of.get(y * w + x))
            idents.append(ident)
            rooms.append(sizes[ident] if ident is not None else 0)
        ok = False
        for i, left in enumerate(sites):
            for j in range(i + 1, len(sites)):
                right = sites[j]
                gap = max(abs((left[0] + 2) - (right[0] + 2)), abs((left[1] + 2) - (right[1] + 2)))
                if gap < made['share_gap']:
                    continue
                same = idents[i] is not None and idents[i] == idents[j]
                if same and rooms[i] >= 2 * made['min_city']:
                    ok = True
                elif not same and rooms[i] >= made['min_city'] and rooms[j] >= made['min_city']:
                    ok = True
        if ok:
            found.append(island['id'])
    return found


def _empty(doc, problems, made):
    return {
        'profile': mf.PROFILE_ID, 'problems': problems, 'witness': (), 'feasible': {},
        'shares': [], 'diagnostics': [], 'sites': {}, 'land': {}, 'evaluated': 0,
        'cities': 0, 'engine': 'NOT_RUN_ENGINE', 'options': made,
    }


def format_report(report):
    lines = []
    status = 'PASS_TOOL_CHECKS' if not report['problems'] else 'QUESTION'
    lines.append(f'{status} profile={report["profile"]}')
    for problem in report['problems']:
        lines.append(f'problem: {problem}')
    for seat, island in report['witness']:
        cost = next(item[0] for item in report['feasible'].get(seat, ()) if item[1] == island)
        lines.append(f'witness seat {seat} island {island} cost {cost}')
    for island in report['shares']:
        lines.append(f'optional share island {island}')
    for line in report['diagnostics']:
        lines.append(f'diagnostic: {line}')
    lines.append(f"cities {report['cities']}")
    lines.append(report['engine'])
    return '\n'.join(lines) + '\n'


def load_and_check(path, game):
    doc = mf.parse(mf.PathText(path))
    if not mf.baseline_ok(doc):
        raise mf.ManifestError('loadout is not the proposed 03 baseline')
    keys = {island['key'] for island in doc['islands']}
    templates = map2_profile.load_templates(game, keys)
    return doc, validate(doc, templates)


def main(argv):
    try:
        os.nice(10)
    except OSError:
        pass
    if len(argv) != 5 or argv[1] != 'check' or argv[3] != '--game-dir':
        print('usage: map2_ship.py check <manifest> --game-dir <dir>', file=sys.stderr)
        return 2
    try:
        _doc, report = load_and_check(argv[2], argv[4])
    except mf.ManifestError as err:
        print(f'error: {err}')
        return 1
    sys.stdout.write(format_report(report))
    return 0 if not report['problems'] else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
