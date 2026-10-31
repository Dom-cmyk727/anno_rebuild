"""Hand-counted expectations for the ship-start geometry checks.

Route lengths below were counted on the grid, not taken from a previous run
of this checker.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import map2_manifest as mf
import map2_ship as ship
import map2_shape as shape

FAILURES = []
TINY = {
    'step': 1,
    'clearance': 1,
    'max_route_tiles': 30,
    'min_city': 16,
    'share_gap': 8,
    'share_min_side': 16,
    'envelope': False,
}


def expect(condition, message):
    print(('pass ' if condition else 'FAIL ') + message)
    if not condition:
        FAILURES.append(message)


def land(w, h, climate, blocked=()):
    height = bytearray([20]) * (w * h)
    return {
        'w': w, 'h': h, 'climate': climate, 'height': bytes(height),
        'occupied': set(blocked), 'land_sha256': f'c{climate}-{w}x{h}-{len(blocked)}',
    }


def water(w, h):
    return {
        'w': w, 'h': h, 'climate': 0, 'height': bytes(w * h),
        'occupied': set(), 'land_sha256': f'water-{w}x{h}',
    }


def manifest(edge, islands, spawns):
    doc = {
        'manifest': mf.MANIFEST_NAME,
        'version': 1,
        'profile': mf.PROFILE_ID,
        'name': 'synthetic',
        'edge': [edge, edge],
        'base': 'Scenes/0010SingleEndlos/03_1024_med_256.sze',
        'seed': 1,
        'generator': {'tool': 'map2_ship_selftest.py', 'config': 'synthetic'},
        'islands': islands,
        'spawns': [
            {'seat': seat, 'x': x * mf.MILLI + 500, 'y': y * mf.MILLI + 500, 'facing': 0}
            for seat, x, y in spawns
        ],
        'loadout': {
            'status': 'proposed-baseline',
            'source': '03_1024_med_256 human party',
            'gold': 30000,
            'ship_figure': 1502,
            'scout_figure': 717,
            'donkey_figure': 718,
            'holds': [
                {'goods': 'wood', 'tons': 50},
                {'goods': 'food', 'tons': 10},
                {'goods': 'tools', 'tons': 50},
                {'goods': 'tools', 'tons': 30},
            ],
        },
    }
    return mf.parse(json.dumps(doc))


def five(at):
    return [(seat, at[seat][0], at[seat][1]) for seat in range(5)]


def run(edge, islands, keys, spawns, options=None):
    made = dict(TINY)
    if options:
        made.update(options)
    doc = manifest(edge, islands, spawns)
    return ship.format_report(ship.validate(doc, keys, made))


def test_corner_cut():
    sea = {(0, 0), (1, 1)}
    dist = ship.bfs((0, 0), sea)
    expect((1, 1) not in dist, 'a diagonal across blocked corners is not a step')
    sea = {(0, 0), (0, 1), (1, 1)}
    dist = ship.bfs((0, 0), sea)
    expect(dist[(1, 1)] == 2, 'the open orthogonal path costs 2')


def test_zero_cities_and_asymmetric():
    islands = [{'id': i, 'x': i * 8, 'y': 0, 'key': f'I{i}'} for i in range(5)]
    keys = {f'I{i}': land(4, 4, 2 if i < 3 else 4) for i in range(5)}
    # Island occupies y 0..3. Touching sea is y=4. Spawn y = 5+i, so the
    # straight south path is (5+i)-4 steps: seat 0 costs 1, seat 4 costs 5.
    spawns = five([(i * 8 + 1, 5 + i) for i in range(5)])
    text = run(48, islands, keys, spawns)
    expect('PASS_TOOL_CHECKS' in text, 'five separated islands pass the tool checks')
    expect('cities 0' in text, 'a ship start has zero cities')
    expect('witness seat 0 island 0 cost 1' in text, 'seat 0 reaches its island in 1 step')
    expect('witness seat 4 island 4 cost 5' in text, 'seat 4 reaches its island in 5 steps')
    expect('NOT_RUN_ENGINE' in text, 'the engine is not run')


def test_land_and_bounds_spawn():
    islands = [{'id': 0, 'x': 0, 'y': 0, 'key': 'I'}]
    keys = {'I': land(4, 4, 2)}
    text = run(16, islands, keys, five([(0, 0), (1, 0), (2, 0), (0, 1), (1, 1)]))
    expect(text.count('is not clear sea') == 5, 'a spawn on land is refused')


def test_enclosed():
    # Ring on a 12-edge. Interior spawn at (5,5). Destination outside the ring.
    islands = [
        {'id': 0, 'x': 0, 'y': 0, 'key': 'T'},
        {'id': 1, 'x': 0, 'y': 11, 'key': 'B'},
        {'id': 2, 'x': 0, 'y': 1, 'key': 'L'},
        {'id': 3, 'x': 11, 'y': 1, 'key': 'R'},
        {'id': 4, 'x': 0, 'y': 14, 'key': 'DEST'},
    ]
    keys = {
        'T': water(12, 1), 'B': water(12, 1), 'L': water(1, 10), 'R': water(1, 10),
        'DEST': land(4, 4, 2),
    }
    # World must contain dest at y=14 h=4, so edge >= 18. The ring is only
    # across x 0..11, y 0..11. Dest at y=14 is outside the ring.
    text = run(20, islands, keys, five([(4, 5), (5, 5), (6, 5), (5, 4), (5, 6)]))
    expect('reaches no founding island' in text or 'distinct-island witness covers 0 of 5' in text,
           'a ship inside a closed ring has no route out')


def test_one_island_five_beaches():
    islands = [{'id': 0, 'x': 0, 'y': 0, 'key': 'BIG'}]
    keys = {'BIG': land(16, 16, 2)}
    spawns = five([(2 + seat, 18) for seat in range(5)])
    text = run(32, islands, keys, spawns)
    expect('distinct-island witness covers 1 of 5' in text, 'five beaches on one island are not five islands')


def test_no_complete_matching():
    islands = [
        {'id': 0, 'x': 0, 'y': 0, 'key': 'A'},
        {'id': 1, 'x': 0, 'y': 16, 'key': 'B'},
        {'id': 2, 'x': 0, 'y': 8, 'key': 'WALL'},
    ]
    keys = {'A': land(4, 4, 2), 'B': land(4, 4, 4), 'WALL': water(24, 1)}
    spawns = five([(1, 5), (2, 5), (1, 12), (2, 12), (3, 12)])
    text = run(24, islands, keys, spawns)
    expect('distinct-island witness covers 2 of 5' in text, 'options without a 5-matching are refused')


def test_sharing_not_required():
    islands = [{'id': i, 'x': i * 8, 'y': 0, 'key': f'I{i}'} for i in range(5)]
    islands.append({'id': 5, 'x': 0, 'y': 16, 'key': 'SHARE'})
    keys = {f'I{i}': land(4, 4, 2) for i in range(5)}
    keys['SHARE'] = land(16, 16, 4)
    spawns = five([(i * 8 + 1, 5) for i in range(5)])
    text = run(48, islands, keys, spawns)
    expect('PASS_TOOL_CHECKS' in text, 'sharing is not required for a pass')
    expect('optional share island 5' in text, 'a large island can report two footholds')
    expect(text.count('witness seat') == 5, 'the witness still names five seats')
    witness_ids = [line.split()[4] for line in text.splitlines() if line.startswith('witness')]
    expect(witness_ids.count('5') <= 1, 'one shared island is not two witness seats')


def test_absent_climate():
    islands = [{'id': i, 'x': i * 8, 'y': 0, 'key': f'I{i}'} for i in range(5)]
    keys = {f'I{i}': land(4, 4, 2) for i in range(5)}
    text = run(48, islands, keys, five([(i * 8 + 1, 5) for i in range(5)]))
    expect('climate polar absent everywhere' in text, 'a missing climate stays visible')
    expect('climate wueste absent everywhere' in text, 'an unused climate stays visible')


def test_obstructed():
    blocked = {(x, y) for x in range(4) for y in range(4)}
    islands = [{'id': 0, 'x': 0, 'y': 0, 'key': 'BAD'}]
    keys = {'BAD': land(4, 4, 2, blocked)}
    text = run(16, islands, keys, five([(0, 6), (1, 6), (2, 6), (0, 7), (1, 7)]))
    expect('island 0 has no founding site' in text, 'an obstructed coast has no founding site')


def test_detour():
    islands = [{'id': i, 'x': 0, 'y': 0, 'key': 'FAR'} for i in range(1)]
    # one island at y=0. Spawns at y=20. Touch sea y=4. Steps = 16. Bound 10.
    keys = {'FAR': land(4, 4, 2)}
    text = run(32, [{'id': 0, 'x': 0, 'y': 0, 'key': 'FAR'}], keys,
               five([(1, 20), (2, 20), (1, 21), (2, 21), (1, 22)]), {'max_route_tiles': 10})
    expect('detour seat 0 island 0 cost 16' in text, 'a 16-step route is a detour under a bound of 10')
    expect('distinct-island witness covers 0 of 5' in text, 'a detour does not count as a useful opening')


def test_duplicate_shape_and_people():
    # envelope on, but edge is not 1431 so the count check also fires.
    # This test only checks the people marker on a tiny map with envelope off
    # by putting the marker in the key. people_marker reads the name.
    islands = [{'id': 0, 'x': 0, 'y': 0, 'key': 'NORD/N_MONGOLEN.SCP'}]
    keys = {'NORD/N_MONGOLEN.SCP': land(4, 4, 2)}
    text = run(16, islands, keys, five([(0, 6), (1, 6), (2, 6), (0, 7), (1, 7)]))
    expect('people template MONGOLEN' in text, 'a people template is refused')


def test_halfplane_literals():
    expect(shape.south_top_ok(720, 1431), 'southern top 720 passes')
    expect(not shape.south_top_ok(712, 1431), 'southern top 712 fails')
    expect(not shape.south_top_ok(704, 1431), 'southern top 704 fails')
    expect(shape.north_centre_ok(584, 256, 1431), 'northern origin 584 passes')
    expect(not shape.north_centre_ok(592, 256, 1431), 'northern origin 592 fails')


def test_manifest_and_dump():
    try:
        mf.parse('{"cities": []}')
        expect(False, 'a cities field is refused')
    except mf.ManifestError as err:
        expect('cities' in str(err), 'a cities field is refused')
    raw = manifest(16, [{'id': 0, 'x': 0, 'y': 0, 'key': 'I'}], five([(0, 6), (1, 6), (2, 6), (0, 7), (1, 7)]))
    raw['loadout']['holds'][0]['tons'] = 51
    try:
        mf.parse(json.dumps({k: v for k, v in raw.items() if not k.startswith('_')}))
        expect(False, '51 tons is refused')
    except mf.ManifestError as err:
        expect('51' in str(err) or 'tons' in str(err), '51 tons is refused')
    proc = __import__('subprocess').run(
        [sys.executable, '-I', 'tools/fixed_map.py', 'dump', 'data/maps/lan5.txt'],
        check=False, capture_output=True, text=True)
    expect(proc.returncode == 0 and 'digest c6af83f95104cd4f' in proc.stdout, 'lan5 dump digest is unchanged')
    islands = [{'id': i, 'x': i * 8, 'y': 0, 'key': f'I{i}'} for i in range(5)]
    keys = {f'I{i}': land(4, 4, 2) for i in range(5)}
    doc = manifest(48, islands, five([(i * 8 + 1, 5 + i) for i in range(5)]))
    first = ship.validate(doc, keys, dict(TINY))
    second = ship.validate(doc, keys, dict(TINY))
    expect(first['witness'] == second['witness'] == ((0, 0), (1, 1), (2, 2), (3, 3), (4, 4)),
           'the same input regenerates the same witness')


def test_controls():
    islands = [{'id': 0, 'x': 0, 'y': 0, 'key': 'BAD'}]
    keys = {'BAD': land(4, 4, 2, {(x, y) for x in range(4) for y in range(4)})}
    ship.COUNT_OBSTACLES = False
    try:
        text = run(16, islands, keys, five([(0, 6), (1, 6), (2, 6), (0, 7), (1, 7)]))
    finally:
        ship.COUNT_OBSTACLES = True
    expect('island 0 has no founding site' not in text, 'ignoring obstacles hides the blocked coast')
    restored = run(16, islands, keys, five([(0, 6), (1, 6), (2, 6), (0, 7), (1, 7)]))
    expect('island 0 has no founding site' in restored, 'obstacle detection is restored')

    islands = [{'id': 0, 'x': 0, 'y': 0, 'key': 'BIG'}]
    keys = {'BIG': land(16, 16, 2)}
    ship.REQUIRE_DISTINCT = False
    try:
        text = run(32, islands, keys, five([(2 + seat, 18) for seat in range(5)]))
    finally:
        ship.REQUIRE_DISTINCT = True
    expect('distinct-island witness covers 1 of 5' not in text, 'dropping the distinct-island rule hides five beaches')

    islands = [{'id': i, 'x': i * 8, 'y': 0, 'key': f'I{i}'} for i in range(5)]
    keys = {f'I{i}': land(4, 4, 2) for i in range(5)}
    ship.REPORT_ABSENT_CLIMATES = False
    try:
        text = run(48, islands, keys, five([(i * 8 + 1, 5) for i in range(5)]))
    finally:
        ship.REPORT_ABSENT_CLIMATES = True
    expect('climate polar absent everywhere' not in text, 'dropping absent-climate reporting hides an empty bucket')


def main():
    test_corner_cut()
    test_zero_cities_and_asymmetric()
    test_land_and_bounds_spawn()
    test_enclosed()
    test_one_island_five_beaches()
    test_no_complete_matching()
    test_sharing_not_required()
    test_absent_climate()
    test_obstructed()
    test_detour()
    test_duplicate_shape_and_people()
    test_halfplane_literals()
    test_manifest_and_dump()
    test_controls()
    print(f'{len(FAILURES)} failed')
    return 1 if FAILURES else 0


if __name__ == '__main__':
    sys.exit(main())
