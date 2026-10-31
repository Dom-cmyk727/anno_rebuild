"""Hand-specified contracts the 570bb97 checker got wrong.

Expected route lengths and footprint tiles were counted on the grid. They
are not copied from a previous run of tools/map2_ship.py.
"""
import hashlib
import json
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import map2_manifest as mf
import map2_shape as shape
import map2_ship as ship
import map2_ship_selftest as helper

FAILURES = []


def expect(condition, message):
    print(('pass ' if condition else 'FAIL ') + message)
    if not condition:
        FAILURES.append(message)


def run(edge, islands, keys, spawns, options=None):
    made = dict(helper.TINY)
    if options:
        made.update(options)
    doc = helper.manifest(edge, islands, spawns)
    return ship.validate(doc, keys, made)


def test_enclosed_lake():
    # 16x16 land. The centre 4x4 is a lake. The outer ring stays land, so the
    # lake does not touch the ocean. Every 4x4 beside the lake is inland.
    blocked = {(x, y) for x in range(16) for y in range(16) if x in (0, 15) or y in (0, 15)}
    template = helper.land(16, 16, 2, blocked)
    height = bytearray(template['height'])
    for y in range(6, 10):
        for x in range(6, 10):
            height[y * 16 + x] = 0
    template['height'] = bytes(height)
    islands = [{'id': i, 'x': i * 24, 'y': 0, 'key': str(i)} for i in range(5)]
    keys = {str(i): template for i in range(5)}
    # Spawns sit in the ocean south of each island, two tiles off the rectangle.
    result = run(128, islands, keys, helper.five([(i * 24 + 8, 18) for i in range(5)]))
    expect(not result['witness'], 'an enclosed lake is not an ocean founding site')
    expect(any('covers 0 of 5' in item for item in result['problems']),
           'five enclosed lakes do not witness five islands')


def test_far_beach():
    # Island origin (10, 10), size 20x6, so it occupies x=10..29 and y=10..15.
    # Local rows y>=4 are obstructed, so every 4x4 sits on y=10..13. The south
    # rectangle edge at y=16 is only 4 tiles from spawn (20, 20); that edge has
    # no usable beach.
    # The nearest approach water is the east gutter beside the site at local
    # (16, 0). That site covers x=26..29. Water (30, 13) is the east neighbour
    # of (29, 13). The 4-connected path stays south of the island:
    # (20, 20) -> (30, 16) is 10 east + 4 north, then (30, 16) -> (30, 13) is
    # 3 north, 17 steps. The west gutter at (9, 13) is 18 and is not the minimum.
    # A bound of 12 excludes 17.
    template = helper.land(20, 6, 2, {(x, y) for x in range(20) for y in range(4, 6)})
    islands = [{'id': 0, 'x': 10, 'y': 10, 'key': 'FAR'}]
    keys = {'FAR': template}
    spawns = helper.five([(20, 20), (21, 22), (22, 24), (19, 26), (18, 28)])
    result = run(48, islands, keys, spawns, {'max_route_tiles': 12, 'min_city': 16})
    expect(not result['witness'], 'a beach past the route bound is not a useful opening')
    costs = [item[2] for item in result.get('detours', ()) if item[0] == 0 and item[1] == 0]
    expect(costs == [17], 'the path around to the far beach costs 17')


def test_tiny_city():
    islands = [{'id': i, 'x': i * 8, 'y': 0, 'key': str(i)} for i in range(5)]
    keys = {str(i): helper.land(4, 4, 2) for i in range(5)}
    result = run(48, islands, keys, helper.five([(i * 8 + 1, 5) for i in range(5)]),
                 {'min_city': 256})
    expect(not result['witness'], 'a 16-tile patch is below the 256-tile city policy')
    expect(any('covers 0 of 5' in item for item in result['problems']),
           'five warehouse patches are not five viable islands')


def test_distant_component():
    # South 4x4 is the only beach. A 20x20 mass sits north of a water gap.
    # Its coast is occupied, so it has no site. The beach component is 16.
    # The northern mass is 16x16 interior = 256 after a 2-tile occupied rim
    # on a 20x20. min_city 64 must not borrow that 256.
    height = bytearray(24 * 32)
    occupied = set()
    for y in range(20):
        for x in range(20):
            height[y * 24 + x] = 20
            if x < 2 or y < 2 or x >= 18 or y >= 18:
                occupied.add((x, y))
    for y in range(28, 32):
        for x in range(4):
            height[y * 24 + x] = 20
    template = {
        'w': 24, 'h': 32, 'climate': 2, 'height': bytes(height),
        'occupied': occupied, 'land_sha256': 'split',
    }
    islands = [{'id': i, 'x': i * 32, 'y': 0, 'key': str(i)} for i in range(5)]
    keys = {str(i): template for i in range(5)}
    result = run(160, islands, keys, helper.five([(i * 32 + 1, 34) for i in range(5)]),
                 {'min_city': 64})
    expect(not result['witness'], 'a 16-tile beach cannot borrow a distant component')
    expect(not result['witness'] or result.get('city', {}).get(0) != 256,
           'the witness does not cite the distant mass')


def test_footprint_extent():
    # Site (3, 0) covers x=3..6, y=0..3 and touches the north edge.
    # Object origin (1, 1) is outside that site. A 4x1 footprint covers
    # (1,1),(2,1),(3,1),(4,1) and intersects the site. Origin-only blocking
    # of (1, 1) does not.
    template = helper.land(8, 8, 2, {(1, 1)})
    template['placements'] = ((1, 1, 4, 1),)
    sites = ship.founding_sites(template)
    expect(shape.footprint_size(1, 4, 1) == (4, 1), 'odd rotation swaps a 1x4 HEAD to 4x1')
    expect((3, 0) not in sites, 'a footprint that overlaps a 4x4 blocks that site')
    expect((4, 4) in sites, 'a site clear of the footprint remains')


def test_extra_island():
    islands = [{'id': i, 'x': i * 8, 'y': 0, 'key': str(i)} for i in range(6)]
    keys = {str(i): helper.land(4, 4, 2 if i < 5 else 0) for i in range(5)}
    keys['5'] = helper.land(4, 4, 0, {(x, y) for x in range(4) for y in range(4)})
    result = run(64, islands, keys, helper.five([(i * 8 + 1, 6) for i in range(5)]),
                 {'min_city': 16})
    expect(len(result['witness']) == 5, 'five viable islands still match when a sixth cannot')
    text = ship.format_report(result)
    expect('PASS_TOOL_CHECKS' in text, 'an uncolonizable extra island is not a global failure')
    expect('island 5' in text and 'eligible' in text, 'the extra island is reported')
    expect(not any('island 5 has no founding site' == item for item in result['problems']),
           'the extra island is not a structural failure')


def test_hall():
    # Seats 0..2 can reach only islands 0 and 1. Seats 3 and 4 can reach
    # islands 2..4. A land bar at x=18 blocks the ocean. Every seat has an
    # option. The subset of three seats has two islands, so the matching is 4.
    islands = [
        {'id': 0, 'x': 0, 'y': 0, 'key': 'L0'},
        {'id': 1, 'x': 8, 'y': 0, 'key': 'L1'},
        {'id': 2, 'x': 28, 'y': 0, 'key': 'R0'},
        {'id': 3, 'x': 36, 'y': 0, 'key': 'R1'},
        {'id': 4, 'x': 28, 'y': 16, 'key': 'R2'},
        {'id': 5, 'x': 18, 'y': 0, 'key': 'WALL'},
    ]
    keys = {key: helper.land(4, 4, 2) for key in ('L0', 'L1', 'R0', 'R1', 'R2')}
    wall = helper.land(2, 48, 2)
    keys['WALL'] = wall
    spawns = helper.five([(1, 8), (2, 12), (3, 16), (30, 8), (32, 20)])
    result = run(48, islands, keys, spawns, {'min_city': 16})
    expect(all(result['feasible'].get(seat) for seat in range(5)),
           'every seat still has some island')
    expect(any('covers 4 of 5' in item for item in result['problems']),
           'three seats competing for two islands do not match')


def test_sharing_not_counted_twice():
    islands = [{'id': i, 'x': i * 12, 'y': 0, 'key': str(i)} for i in range(5)]
    islands.append({'id': 5, 'x': 0, 'y': 20, 'key': 'SHARE'})
    keys = {str(i): helper.land(8, 8, 2) for i in range(5)}
    keys['SHARE'] = helper.land(16, 16, 4)
    result = run(80, islands, keys, helper.five([(i * 12 + 2, 10) for i in range(5)]),
                 {'min_city': 16, 'share_min_side': 16, 'share_gap': 8})
    ids = [island for _seat, island in result['witness']]
    expect(len(ids) == 5 and len(set(ids)) == 5, 'the witness uses five distinct islands')
    expect(ids.count(5) <= 1, 'a shared island is one witness seat')
    expect(5 in result['shares'], 'the large island can still report sharing')


def test_diagonal_metric():
    # Open 2x2. Manhattan 4-connected distance from (0,0) to (1,1) is 2.
    dist = ship.bfs((0, 0), {(0, 0), (0, 1), (1, 0), (1, 1)})
    expect(dist.get((1, 1)) == 2, 'an open diagonal costs 2 orthogonal steps')


def _sample_text():
    doc = helper.manifest(16, [{'id': 0, 'x': 0, 'y': 0, 'key': 'I'}],
                          helper.five([(0, 6), (1, 6), (2, 6), (0, 7), (1, 7)]))
    public = {k: v for k, v in doc.items() if not k.startswith('_')}
    return json.dumps(public)


def test_manifest_strict():
    raw = json.loads(_sample_text())
    owned = json.loads(_sample_text())
    owned['islands'][0]['owner'] = 0
    try:
        mf.parse(json.dumps(owned))
        expect(False, 'a nested owner field is refused')
    except mf.ManifestError as err:
        expect('owner' in str(err), 'a nested owner field is refused')
    cleared = json.loads(_sample_text())
    cleared['excluded'] = []
    try:
        mf.parse(json.dumps(cleared))
        expect(False, 'a conflicting exclusion list is refused')
    except mf.ManifestError as err:
        expect('excluded' in str(err), 'a conflicting exclusion list is refused')
    floated = json.loads(_sample_text())
    floated['profile_parameters'] = {'max_route_tiles': 0.5}
    try:
        mf.parse(json.dumps(floated))
        expect(False, 'a fractional route bound is refused')
    except mf.ManifestError as err:
        expect('profile_parameters' in str(err) or 'max_route_tiles' in str(err),
               'a fractional route bound is refused')
    duplicate = '{"version":999,' + _sample_text()[1:]
    try:
        mf.parse(duplicate)
        expect(False, 'a duplicate version member is refused')
    except mf.ManifestError as err:
        expect('duplicate' in str(err), 'a duplicate version member is refused')
    unknown = json.loads(_sample_text())
    unknown['note'] = 'extra'
    try:
        mf.parse(json.dumps(unknown))
        expect(False, 'an unknown top-level field is refused')
    except mf.ManifestError as err:
        expect('note' in str(err), 'an unknown top-level field is refused')
    again = mf.parse(_sample_text())
    emitted = mf.emit(again)
    expect(mf.parse(emitted.decode('ascii'))['content_hash'] == again['content_hash'],
           'canonical emission round-trips')


def test_negative_controls():
    template = helper.land(8, 8, 2, {(1, 1)})
    template['placements'] = ((1, 1, 4, 1),)
    real_blocked = ship.blocked_tiles

    def origins_only(item):
        return set(item.get('occupied') or ())

    ship.blocked_tiles = origins_only
    try:
        leaked = (3, 0) in ship.founding_sites(template)
    finally:
        ship.blocked_tiles = real_blocked
    expect(leaked, 'without footprint expansion the overlapped site remains')
    expect((3, 0) not in ship.founding_sites(template), 'restored footprints block that site')

    real_water = ship.adjacent_water

    def rectangle_end(item, x, y):
        # Pretend every site opens onto the south edge of this 16-tile island.
        return [(8, 16)]

    ship.adjacent_water = rectangle_end
    try:
        blocked = {(x, y) for x in range(16) for y in range(16) if x in (0, 15) or y in (0, 15)}
        lake = helper.land(16, 16, 2, blocked)
        height = bytearray(lake['height'])
        for y in range(6, 10):
            for x in range(6, 10):
                height[y * 16 + x] = 0
        lake['height'] = bytes(height)
        islands = [{'id': i, 'x': i * 24, 'y': 0, 'key': str(i)} for i in range(5)]
        keys = {str(i): lake for i in range(5)}
        opened = run(128, islands, keys, helper.five([(i * 24 + 8, 18) for i in range(5)]))
    finally:
        ship.adjacent_water = real_water
    expect(len(opened['witness']) == 5, 'a rectangle-end link accepts the enclosed lake')
    closed = run(128, islands, keys, helper.five([(i * 24 + 8, 18) for i in range(5)]))
    expect(not closed['witness'], 'restored site water rejects the enclosed lake')

    real_room = ship.site_room

    def distant_credit(item, site, sizes, index_of):
        return max(sizes) if sizes else 0

    ship.site_room = distant_credit
    try:
        credited = test_distant_result()
    finally:
        ship.site_room = real_room
    expect(len(credited['witness']) == 5, 'crediting another component accepts the 16-tile beach')
    expect(not test_distant_result()['witness'], 'restored site components reject that beach')

    real_match = ship.matching

    def doubled(options):
        return ((0, 5), (1, 5), (2, 0), (3, 1), (4, 2))

    graph = {0: [5, 0], 1: [5, 1], 2: [2], 3: [3], 4: [4]}
    ship.matching = doubled
    try:
        doubled_ids = [island for _seat, island in ship.matching(graph)]
    finally:
        ship.matching = real_match
    restored_ids = [island for _seat, island in ship.matching(graph)]
    expect(doubled_ids.count(5) == 2, 'a doubled matching counts one island twice')
    expect(restored_ids.count(5) <= 1 and len(restored_ids) == 5,
           'restored matching keeps one island once')


def test_distant_result():
    height = bytearray(24 * 32)
    occupied = set()
    for y in range(20):
        for x in range(20):
            height[y * 24 + x] = 20
            if x < 2 or y < 2 or x >= 18 or y >= 18:
                occupied.add((x, y))
    for y in range(28, 32):
        for x in range(4):
            height[y * 24 + x] = 20
    template = {
        'w': 24, 'h': 32, 'climate': 2, 'height': bytes(height),
        'occupied': occupied, 'land_sha256': 'split',
    }
    islands = [{'id': i, 'x': i * 32, 'y': 0, 'key': str(i)} for i in range(5)]
    keys = {str(i): template for i in range(5)}
    return run(160, islands, keys, helper.five([(i * 32 + 1, 34) for i in range(5)]),
               {'min_city': 64})


def test_landlocked_spawns():
    # The review probe: five 32x32 land islands, each with a 16x16 lake, and
    # each ship in the middle of its lake. Production clearance, city proxy
    # and route bound. The layout envelope is off so this is only the sea rule.
    template = helper.land(32, 32, 2)
    height = bytearray(template['height'])
    for y in range(8, 24):
        for x in range(8, 24):
            height[y * 32 + x] = 0
    template['height'] = bytes(height)
    islands = [{'id': i, 'x': 40 * i, 'y': 8, 'key': str(i)} for i in range(5)]
    keys = {str(i): template for i in range(5)}
    spawns = helper.five([(40 * i + 15, 8 + 15) for i in range(5)])
    result = run(224, islands, keys, spawns,
                 {'clearance': 8, 'min_city': 256, 'max_route_tiles': 400})
    expect(not result['witness'], 'ships inside enclosed lakes do not form a witness')
    expect(bool(result['problems']), 'ships inside enclosed lakes are rejected')
    expect(all(not result['feasible'][seat] for seat in range(5)),
           'a landlocked spawn has no eligible route')


def _corner_island(open_side):
    # 20x20. Lake at local (2, 2). Water at (1, 1) and (0, 1) reaches the
    # west edge, so that water is exterior. The lake's four orthogonal
    # neighbours stay land unless open_side opens the west neighbour (1, 2).
    height = bytearray([20]) * (20 * 20)
    height[1 * 20 + 1] = 0
    height[1 * 20 + 0] = 0
    height[2 * 20 + 2] = 0
    if open_side:
        height[2 * 20 + 1] = 0
    return {
        'w': 20, 'h': 20, 'climate': 2, 'height': bytes(height),
        'occupied': set(), 'land_sha256': 'corner',
    }


def test_diagonal_contact():
    island = {'id': 0, 'x': 4, 'y': 4, 'key': 'CORNER'}
    spawns = helper.five([(6, 6), (1, 1), (1, 2), (1, 3), (30, 1)])
    closed = run(32, [island], {'CORNER': _corner_island(False)}, spawns,
                 {'clearance': 1, 'min_city': 16, 'max_route_tiles': 40})
    expect(not closed['feasible'][0], 'diagonal contact does not let a lake spawn sail')
    expect(bool(closed['problems']), 'a diagonally touching lake spawn is rejected')
    expect(0 not in {seat for seat, _island in closed['witness']},
           'a diagonally touching lake is not a witness seat')
    opened = run(32, [island], {'CORNER': _corner_island(True)}, spawns,
                 {'clearance': 1, 'min_city': 16, 'max_route_tiles': 40})
    expect(bool(opened['feasible'][0]), 'an orthogonal channel makes the same lake reachable')


def test_connected_bay():
    # 12x12 land with a north channel one tile wide. The spawn sits in the bay.
    template = helper.land(12, 12, 2)
    height = bytearray(template['height'])
    for y in range(8):
        height[y * 12 + 5] = 0
    template['height'] = bytes(height)
    islands = [{'id': i, 'x': 4 + i * 16, 'y': 4, 'key': str(i)} for i in range(5)]
    keys = {str(i): template for i in range(5)}
    spawns = helper.five([(4 + i * 16 + 5, 8) for i in range(5)])
    result = run(88, islands, keys, spawns, {'clearance': 1, 'min_city': 16, 'max_route_tiles': 80})
    expect(len(result['witness']) == 5, 'five bays open to the boundary still match')
    expect(not result['problems'], 'a boundary-connected bay is exterior sea')


def _chunk(name, payload):
    raw = name.encode('ascii')
    return raw + b'\0' * (12 - len(raw)) + struct.pack('<I', len(payload)) + payload


def _scp(obstacle_xy):
    width = 8
    info = bytes([2]) + bytes(11)
    height = _chunk('HEIGHT', struct.pack('<HHBB', 0, 0, width, width) + bytes([20]) * (width * width))
    word = 1
    ox, oy = obstacle_xy
    item = struct.pack('<HBBBB', word, 0, 0, ox, oy)
    scenery = _chunk('GELAENDE', struct.pack('<HHBB', 0, 0, width, width) + item)
    body = struct.pack('<4HI', 0, 0, width, width, 0) + _chunk('INFO', info) + height + scenery
    return _chunk('ANNO', _chunk('INSEL5', body))


def test_source_file_hash():
    files, shapes = ship.source_fields({'K': {'land_sha256': 'shape-only'}}, ['K'])
    expect(files['K'] is None, 'a missing source file hash stays missing')
    expect(shapes['K'] == 'shape-only' and files['K'] != shapes['K'],
           'a shape digest is not reported as the file hash')
    import map2_profile
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        left = root / 'Scenes' / 'Islands' / 'NORD'
        left.mkdir(parents=True)
        raw_a = _scp((1, 1))
        raw_b = _scp((2, 2))
        (left / 'a.scp').write_bytes(raw_a)
        (left / 'b.scp').write_bytes(raw_b)
        keys = {'NORD/A.SCP', 'NORD/B.SCP'}
        catalog = map2_profile.load_templates(root, keys)
        expect(catalog['NORD/A.SCP']['land_sha256'] == catalog['NORD/B.SCP']['land_sha256'],
               'the same land mask keeps one shape digest')
        expect(catalog['NORD/A.SCP']['source_sha256'] == hashlib.sha256(raw_a).hexdigest(),
               'the file hash is the raw SCP bytes')
        expect(catalog['NORD/A.SCP']['source_sha256'] != catalog['NORD/B.SCP']['source_sha256'],
               'a different obstacle record changes the file hash')
        doc = {
            'name': 'synthetic', 'content_hash': '0' * 64, 'profile': mf.PROFILE_ID,
            'version': 2, 'policy': mf.default_policy(), 'excluded': list(mf.EXCLUDED),
        }
        report = {
            'feasible': {seat: [] for seat in range(5)},
            'witness_detail': (), 'detour_detail': (), 'shares': [], 'diagnostics': [],
            'site_records': [], 'problems': [],
            'template_sha256': {key: catalog[key]['source_sha256'] for key in keys},
            'shape_sha256': {key: catalog[key]['land_sha256'] for key in keys},
            'checker': {'commit': 'abc', 'dirty_sources': True, 'source_sha256': {}},
        }
        payload = json.loads(json.dumps(ship.public_report(doc, report)))
        expect(payload['template_sha256']['NORD/A.SCP'] != payload['shape_sha256']['NORD/A.SCP'],
               'the serialized report keeps the file hash apart from the shape hash')
        expect(payload['checker']['dirty_sources'] is True, 'a dirty checker is visible in the report')
        expect(payload['policy']['sea'] == 'exterior-4', 'the report states the exterior-sea rule')


def test_sea_policy_rejected():
    doc = helper.manifest(32, [{'id': 0, 'x': 0, 'y': 0, 'key': 'N'}],
                          helper.five([(1, 1), (2, 1), (3, 1), (4, 1), (5, 1)]))
    doc['policy'] = dict(doc['policy'], sea='lake')
    try:
        mf.parse(json.dumps({key: value for key, value in doc.items() if not key.startswith('_')}))
        expect(False, 'a contradictory sea policy is refused')
    except mf.ManifestError as err:
        expect('sea' in str(err), 'a contradictory sea policy is refused')


def main():
    test_enclosed_lake()
    test_landlocked_spawns()
    test_diagonal_contact()
    test_connected_bay()
    test_far_beach()
    test_tiny_city()
    test_distant_component()
    test_footprint_extent()
    test_extra_island()
    test_hall()
    test_sharing_not_counted_twice()
    test_diagonal_metric()
    test_manifest_strict()
    test_negative_controls()
    test_source_file_hash()
    test_sea_policy_rejected()
    print(f'{len(FAILURES)} failed')
    return 1 if FAILURES else 0


if __name__ == '__main__':
    sys.exit(main())
