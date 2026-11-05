"""Synthetic checks for the MAP.2 candidate profile.

Expected numbers are literals. The tests do not ask the profile for the
answer they then compare against itself, except by calling the function
under test and comparing it to a hand-computed value.

  python3 -I tools/map2_selftest.py
"""
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixed_map  # noqa: E402
import map2_profile as profile  # noqa: E402
import map2_shape as shape  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
FAILURES = []


def expect(condition, message):
    if condition:
        print(f'pass {message}')
    else:
        FAILURES.append(message)
        print(f'FAIL {message}')


def grid(w, h, water_column=True):
    height = bytearray(w * h)
    for y in range(h):
        for x in range(w):
            height[y * w + x] = 0 if water_column and x == 0 else 20
    return bytes(height)


def template(climate, digest, land=56, w=8, h=8, water_column=True, key_marker=None):
    return {
        'w': w, 'h': h, 'climate': climate, 'land': land,
        'height': grid(w, h, water_column), 'occupied': set(),
        'land_sha256': digest,
    }


def island(slot, x, y, key):
    return {'slot': slot, 'x': x, 'y': y, 'key': key, 'raw': [], 'ore': []}


def start(seat, slot, x, y):
    return {'seat': seat, 'island': slot, 'x': x, 'y': y, 'rotation': 0}


def world(islands, starts, edge=80):
    return {
        'version': 1, 'name': 't', 'width': edge, 'height': edge, 'base': 'b',
        'seed': 1, 'money': None, 'islands': islands, 'starts': starts,
    }


def small_profile():
    made = profile.default_profile()
    made['large'] = 8
    made['fw'] = 2
    made['fh'] = 2
    return made


def stacked_world(lands=None, hashes=None, extra=None, south_y=40):
    """Five north islands along y=0 and five south islands at south_y."""
    lands = lands or [56] * 5
    hashes = hashes or [f'n{i}' for i in range(5)]
    islands = []
    starts = []
    catalog = {}
    for seat in range(5):
        key = f'NORD/N_{seat}.SCP'
        x = seat * 16
        islands.append(island(seat, x, 0, key))
        catalog[key] = template(2, hashes[seat], lands[seat])
        starts.append(start(seat, seat, x + 1, 6))
        skey = f'DSCHUNGEL/D_{seat}.SCP'
        islands.append(island(10 + seat, x, south_y, skey))
        catalog[skey] = template(4, f's{seat}', 56)
    if extra:
        islands.append(extra[0])
        catalog[extra[0]['key']] = extra[1]
    return world(islands, starts), catalog


def test_rot90_literal():
    turned = shape.rot90(bytes([1, 2, 3, 4]), 2)
    expect(turned == bytes([3, 1, 4, 2]), 'rot90 of 1 2 / 3 4 is 3 1 / 4 2')
    expect(shape.canonical(turned, 2) == shape.canonical(bytes([1, 2, 3, 4]), 2),
           'a quarter turn has the same canonical bytes')
    mirrored = shape.reflect_x(bytes([1, 2, 3, 0]), 2)
    expect(mirrored == bytes([2, 1, 0, 3]), 'reflect_x swaps each row')
    expect(shape.canonical(mirrored, 2) == shape.canonical(bytes([1, 2, 3, 0]), 2),
           'a mirror has the same canonical bytes')


def test_tolerance_boundaries():
    inside = [1100, 975, 975, 975, 975]
    outside = [1101, 975, 975, 975, 974]
    expect(shape.within_mean(inside, 100), '1100 on a mean of 1000 passes 100 permille')
    expect(not shape.within_mean(inside, 50), '1100 on a mean of 1000 fails 50 permille')
    expect(not shape.within_mean(outside, 100), '1101 on a mean of 1000 fails 100 permille')
    expect(not shape.within_mean([], 50), 'an empty measure does not pass')
    expect(shape.within_mean([0, 0, 0, 0, 0], 50), 'five zeros are equal')
    expect(shape.worst_permille([]) is None, 'empty worst permille is absent')
    expect(shape.worst_permille([0, 0, 0, 0, 0]) == 0, 'all-zero worst permille is 0')
    expect(shape.worst_permille(inside) == 100, 'boundary worst permille is 100')


def test_halfplane_literals():
    expect(shape.north_centre_ok(40, 10, 100), 'centre 50 on edge 100 is north')
    expect(not shape.north_centre_ok(46, 10, 100), 'centre 51 on edge 100 is south of the line')
    expect(shape.south_top_ok(50, 100), 'top 50 on edge 100 is south')
    expect(not shape.south_top_ok(49, 100), 'top 49 on edge 100 is north of the line')


def test_halfplane_message():
    mp, catalog = stacked_world()
    for island in mp['islands']:
        if island['slot'] == 0:
            island['y'] = 40
            for item in mp['starts']:
                if item['seat'] == 0:
                    item['y'] = 46
    text = profile.format_report(profile.validate(mp, catalog, small_profile()))
    expect('north centre y is below the middle line' in text, 'a nord below the line is refused')


def test_duplicate_shape():
    mp, catalog = stacked_world(hashes=['same', 'same', 'n2', 'n3', 'n4'])
    text = profile.format_report(profile.validate(mp, catalog, small_profile()))
    expect('duplicate land shape' in text, 'two names with one land mask are refused')


def test_people_and_coast_and_bounds():
    mp, catalog = stacked_world()
    mp['islands'][0]['key'] = 'NORD/N_MONGOLEN.SCP'
    catalog['NORD/N_MONGOLEN.SCP'] = catalog.pop('NORD/N_0.SCP')
    text = profile.format_report(profile.validate(mp, catalog, small_profile()))
    expect('people template MONGOLEN' in text, 'a Mongolen template is refused')

    mp, catalog = stacked_world()
    mp['starts'][0]['x'] = 4
    mp['starts'][0]['y'] = 3
    catalog['NORD/N_0.SCP']['height'] = bytes([20]) * 64
    text = profile.format_report(profile.validate(mp, catalog, small_profile()))
    expect('not on the coast' in text, 'an inland kontor is refused')

    mp, catalog = stacked_world()
    mp['islands'][0]['x'] = 80
    text = profile.format_report(profile.validate(mp, catalog, small_profile()))
    expect('outside the world' in text, 'an island past the edge is refused')

    mp, catalog = stacked_world()
    mp['islands'][1]['x'] = 0
    mp['islands'][1]['y'] = 0
    text = profile.format_report(profile.validate(mp, catalog, small_profile()))
    expect('overlaps island slot' in text, 'overlapping rectangles are refused')


def test_band_and_ordinary():
    made = small_profile()
    islands = [island(1, 0, 0, 'POLAR/P.SCP')]
    catalog = {'POLAR/P.SCP': template(0, 'p', w=8, h=8)}
    mp = world(islands, [], edge=1024)
    text = profile.format_report(profile.validate(mp, catalog, made))
    expect('outside polar band' in text, 'polar y=0 on a 1024 edge is outside the band')
    islands[0]['y'] = 16
    text = profile.format_report(profile.validate(world(islands, [], edge=1024), catalog, made))
    expect('outside polar band' not in text, 'polar y=16 is inside the band')

    ordinary_islands = [
        island(1, 0, 0, 'NORD/A.SCP'),
        island(2, 16, 0, 'NORD/B.SCP'),
    ]
    ordinary_catalog = {
        'NORD/A.SCP': template(2, 'a'),
        'NORD/B.SCP': template(2, 'b'),
    }
    sizes = {1: (8, 8), 2: (8, 8)}
    ordinary = world(ordinary_islands, [start(0, 1, 1, 6)])
    expect(profile.geometry_problems(ordinary, sizes) == [], 'an ordinary two-island map has no rectangle error')
    text = profile.format_report(profile.validate(ordinary, ordinary_catalog, made))
    expect('large nord count' in text, 'the ordinary map fails the candidate profile')
    expect('overlaps' not in text and 'outside the world' not in text,
           'the ordinary map is not failed for overlap or bounds')


def test_assignment_and_ties():
    long_routes = [
        [100, 30, 1000, 1000, 1000],
        [1000, 100, 30, 1000, 1000],
        [1000, 1000, 100, 30, 1000],
        [1000, 1000, 1000, 100, 30],
        [30, 1000, 1000, 1000, 100],
    ]
    short, short_lengths = profile.assign(long_routes, 'min-spread-sum-lex')
    baseline, base_lengths = profile.assign(long_routes, 'min-spread-lex')
    expect(short == (1, 2, 3, 4, 0), 'spread then sum selects the short cycle')
    expect(sum(short_lengths) == 150, 'the short cycle totals 150')
    expect(baseline == (0, 1, 2, 3, 4), 'min-spread-lex keeps the long equal routes')
    expect(sum(base_lengths) == 500, 'the baseline total is 500')
    used, _lengths = profile.assign(long_routes, profile.ASSIGNMENT_MODE)
    expect(used == (1, 2, 3, 4, 0), 'chosen assignment must be the short cycle')

    tied = [
        [10, 10, 100, 100, 100],
        [10, 10, 100, 100, 100],
        [100, 100, 10, 100, 100],
        [100, 100, 100, 10, 100],
        [100, 100, 100, 100, 10],
    ]
    perm, lengths = profile.assign(tied, 'min-spread-sum-lex')
    expect(perm == (0, 1, 2, 3, 4), 'an equal-spread tie breaks to the lower permutation')
    expect(lengths == (10, 10, 10, 10, 10), 'the tied lengths are the tens')


def test_reach_and_zero():
    seat = profile.reach_seat(0, 20, [(0, 0), (100, 0)], (50, 80))
    expect(seat == 0, 'the nearer kontor owns the island')
    contested = profile.reach_seat(100, 2, [(0, 0), (100, 0)], (50, 0))
    expect(contested is None, 'the map centre wins when it is strictly closer')
    tied = profile.reach_seat(100, 0, [(0, 0), (100, 0)], (50, 80))
    expect(tied == 0, 'a distance tie goes to the lower seat')
    height = bytes([20]) * 16
    expect(shape.point_tile_dist2(1, 1, 0, 0) == 0, 'a point inside a tile has distance 0')
    expect(profile.nearest_land_dist2(1, 1, 0, 0, height, 4, 4) == 0, 'land under the point is a zero route')

    mp, catalog = stacked_world()
    mp['islands'].append(island(50, 0, 16, 'PRAERIE/P.SCP'))
    catalog['PRAERIE/P.SCP'] = template(3, 'praerie', land=40)
    text = profile.format_report(profile.validate(mp, catalog, small_profile()))
    expect('reach climate praerie absent for seat 1' in text, 'a one-sided climate bucket is absent, not zero')
    expect('zero route length' not in text, 'separated islands are not a zero route')


def test_unfair_home_land():
    mp, catalog = stacked_world(lands=[1060, 985, 985, 985, 985])
    report = profile.validate(mp, catalog, small_profile())
    expect('fairness/home_land' in report['failed_gates'], 'unfair home land must fail')
    large = next(item for item in report['measures'] if item['name'] == 'large-land/nord')
    expect(large['ok'], 'the same lands still pass the 100 permille large-land gate')


def test_lan5_dump_unchanged():
    proc = subprocess.run(
        [sys.executable, '-I', str(ROOT / 'tools' / 'fixed_map.py'), 'dump',
         str(ROOT / 'data' / 'maps' / 'lan5.txt')],
        capture_output=True, text=True)
    expect(proc.returncode == 0, 'fixed_map dump of lan5 exits 0')
    expect('digest c6af83f95104cd4f' in proc.stdout, 'lan5 digest stays c6af83f95104cd4f')
    expect(proc.stdout.count('\nisland ') == 21, 'lan5 still dumps 21 islands')
    data = (ROOT / 'data' / 'maps' / 'lan5.txt').read_bytes()
    expect(len(data) == 1476, 'lan5.txt stays 1476 bytes')


def main():
    test_rot90_literal()
    test_tolerance_boundaries()
    test_halfplane_literals()
    test_halfplane_message()
    test_duplicate_shape()
    test_people_and_coast_and_bounds()
    test_band_and_ordinary()
    test_assignment_and_ties()
    test_reach_and_zero()
    test_unfair_home_land()
    test_lan5_dump_unchanged()
    print(f'{len(FAILURES)} failed')
    return 1 if FAILURES else 0


if __name__ == '__main__':
    sys.exit(main())
