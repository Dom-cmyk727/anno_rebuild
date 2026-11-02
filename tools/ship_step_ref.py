#!/usr/bin/env python3
"""Exact-rational reference model of the ship stepper (MAP-NAV-2 package D; contract
docs/sim/ship-navigation.md 12.4, original docs/re/ship-navigation.md 7.2, 7.3).

The model is the MATHEMATICAL one: continuous time in ticks, exact rationals (fractions.Fraction),
no integer scales, no rounding. It does not share an algorithm with the C++ integer stepper, which
works in q32/500 units with explicit floors. A vector pins the model's position after every visit
and the visit at which the voyage ends; the C++ test allows the stated tolerance (a few units of
q32/500 from the floor at speed changes and the dropped diagonal residue).

Usage:
  ship_step_ref.py write  <vectors.txt>    regenerate the fixture
  ship_step_ref.py check  <vectors.txt>    the fixture equals the model's output (exit 1 otherwise)
  ship_step_ref.py selftest                the model rejects deliberately wrong variants of its own
                                           hand-derived anchor cases
  ship_step_ref.py write-factor <f.txt>    regenerate the speed-factor vectors (MAP-NAV-3 N4)
  ship_step_ref.py check-factor <f.txt>    the factor fixture equals the model's output
"""
import sys
from fractions import Fraction as F
from math import floor

IDLE_SPEED = 42949672                  # f32(0.01) * 2^32
UNIT = 2 ** 32
LD = 11863283 * 2 ** 9                 # diagonal step length, q32
C = F(11863283, 2 ** 24)               # diagonal axis component per unit length
def _chunk_limit():
    # 1.0 + f32(1e-4) in q32, derived from the f32 bit pattern itself (not typed in): a node longer
    # than this is cut into a 1.0 chunk and a rest
    import struct
    f = struct.unpack('<f', struct.pack('<f', 1e-4))[0]
    return F(f) * UNIT + UNIT


CHUNK_LIMIT = _chunk_limit()
assert floor(CHUNK_LIMIT) == UNIT + 429496 and CHUNK_LIMIT > UNIT + 429496  # 2^32 + 429496.72
DX = [-1, -1, 0, 1, 1, 1, 0, -1]
DY = [0, 1, 1, 1, 0, -1, -1, -1]


def segments(nodes, turn_step, V, mut=None):
    """[(duration_ticks, dx_per_tick, dy_per_tick)] for a committed plan; positions in q32 tiles."""
    segs = []
    first = next(n for n in nodes if n[0] == 'M')
    facing = 2 * first[1]                      # the commit sets the facing directly

    def turn_seg(length):
        rate = F(IDLE_SPEED * 100, 500)        # q32 per tick
        segs.append((F(length) / rate, F(0), F(0), rate, False))

    for n in nodes:
        if n[0] == 'T':
            facing = n[1] & 15
            turn_seg(turn_step)
            continue
        _, dir8, length, speed = n
        want = 2 * dir8
        while True:
            ds = (want - facing) & 15
            d = min(ds, 16 - ds)
            if d < (4 if mut == 'dead_zone4' else 3):
                break
            facing = (facing + (-1 if ds >= 8 else 1)) & 15
            turn_seg(turn_step)
        facing = want
        rate = F(speed * V, 500)               # q32 along the heading per tick
        if dir8 % 2 == 0:
            vx, vy = DX[dir8] * rate, DY[dir8] * rate
        else:
            k = F(1) if mut == 'diag_unit' else C
            vx, vy = DX[dir8] * rate * k, DY[dir8] * rate * k
        segs.append((F(length) / rate, vx, vy, rate, dir8 % 2 == 1))
    return segs


def position_at(segs, t):
    t = F(t)
    x = y = F(0)
    for dur, vx, vy, _rate, _diag in segs:
        use = min(t, dur)
        x += vx * use
        y += vy * use
        t -= use
        if t <= 0:
            break
    return x, y


def total_ticks(segs):
    return sum((s[0] for s in segs), F(0))


def tolerance(segs):
    """The error bound of the integer stepper against this model, in units of q32/500 (derived from the
    carry terms, not fitted). Entering segment i with a leftover converted at a rate change floors once:
    the budget lost is under one unit of segment i's rate, i.e. under 1 / r_i ticks, and that shift of
    time costs at most max(r_j, j >= i) per tick afterwards, so under 1 + max_j(r_j) / r_i units. A
    diagonal segment end drops a residue fraction under one unit. The publication floor is outside the
    comparison (it compares pos_u). One unit of slack covers the final partial step."""
    total = F(1)
    for i in range(1, len(segs)):
        later = max(s[3] for s in segs[i:])
        total += 1 + later / segs[i][3]
    total += sum(1 for s in segs if s[4])
    return floor(total) + 1


def run_case(c, mut=None):
    segs = segments(c['nodes'], c['turn'], c['V'], mut)
    total = total_ticks(segs)
    cum, arrive, rows = 0, 0, []
    for i, d in enumerate(c['visits'], 1):
        cum += d
        x, y = position_at(segs, cum)
        rows.append((i, floor(x * 500), floor(y * 500)))
        if arrive == 0 and cum >= total and d > 0:
            arrive = i
    tol = tolerance(segs)
    return rows, arrive, tol, total


def node_text(n):
    return f'T:{n[1]}' if n[0] == 'T' else f'M:{n[1]}:{n[2]}:{n[3]}'


def cases():
    S = 154618816          # 0.036 tile per stepper unit (type 1502)
    S2 = 2 ** 26           # 1/64
    S3 = 3 * 2 ** 25       # 3/128
    TURN = 472446400       # type 1502 f0 0.11
    out = []

    def add(name, nodes, visits, V=100, turn=TURN, last=0, pos=(0, 0), note='', stride=1, cover=True):
        if cover:
            # repeat the visit pattern until the plan has ended, then one more visit (the ship idles)
            total = total_ticks(segments(nodes, turn, V))
            seq, cum = [], 0
            while cum < total:
                for v in visits:
                    seq.append(v)
                    cum += v
                    if cum >= total:
                        break
            seq.append(visits[-1])
            visits = seq
        out.append(dict(name=name, nodes=nodes, visits=visits, V=V, turn=turn, last=last,
                        pos=pos, note=note, stride=stride))

    add('axis_exact_boundary', [('M', 4, UNIT, S2)], [320, 1], cover=False, note='1.0 unit at 2^26 / 100 percent ends exactly on a 320-tick visit')
    add('axis_one_tick_visits', [('M', 4, 3 * UNIT, S)], [1] * 150 + [1000])
    add('axis_five_tick_visits', [('M', 2, 10 * UNIT, S)], [5] * 120)
    add('axis_irregular_visits', [('M', 0, 7 * UNIT, S)], [1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597, 400])
    add('diag_each', [('M', 3, 7 * LD, S)], [17, 50, 3, 100, 250, 64, 31, 900])
    add('diag_all_four', [('M', 1, 4 * LD, S), ('M', 3, 4 * LD, S), ('M', 5, 4 * LD, S), ('M', 7, 4 * LD, S)], [37] * 40)
    add('mixed_directions', [('M', 4, 3 * UNIT, S), ('M', 2, 2 * UNIT, S), ('M', 3, 4 * LD, S), ('M', 0, 5 * UNIT, S)], [50] * 40)
    add('turn_phases_90', [('M', 4, 2 * UNIT, S), ('M', 2, 2 * UNIT, S)], [5] * 120)
    add('turn_phases_135', [('M', 4, 2 * UNIT, S), ('M', 1, 3 * LD, S)], [5] * 120)
    add('turn_phases_180', [('M', 4, 2 * UNIT, S), ('M', 0, 2 * UNIT, S)], [5] * 160)
    add('turn_nodes_first', [('T', 12 & 15), ('T', 13), ('T', 14), ('M', 4, 2 * UNIT, S)], [11] * 40, note='planner turn nodes before the first run')
    rev = [('T', f & 15) for f in range(8, 16)]
    add('planner_reversal_8_nodes', rev + [('M', 4, 4 * UNIT, S)], [50] * 20, note='facing 8 to 0 turned + by the planner, 8 nodes')
    add('speed_factor_87', [('M', 4, 5 * UNIT, S), ('M', 3, 3 * LD, S)], [29] * 60, V=87)
    add('turn_phases_at_v65', [('M', 4, 2 * UNIT, S), ('M', 2, 2 * UNIT, S), ('M', 0, 2 * UNIT, S)], [5] * 200, V=65, note='turn segments run at the idle speed and V = 100 whatever the ship factor')
    add('speed_ratio_extreme', [('M', 4, 2 * UNIT, 2 ** 27), ('M', 4, UNIT, 200000), ('M', 4, 2 * UNIT, 2 ** 26)], [1, 7, 40, 3, 90], note='a rate ratio of 671 between neighbours: the bound is derived from the rates')
    add('speed_factor_65', [('M', 6, 5 * UNIT, S), ('M', 7, 3 * LD, S)], [29] * 60, V=65)
    add('speed_change_fraction', [('M', 4, 2 * UNIT, S2), ('M', 4, 3 * UNIT, S3), ('M', 4, 2 * UNIT, S2)], [97] * 60)
    add('speed_change_mid_visit', [('M', 4, UNIT // 3 * 3, S2), ('M', 4, UNIT, 3 * S2), ('M', 3, LD, S)], [400, 77, 5000], cover=False)
    add('sub_tile_start', [('M', 3, 4 * LD, S), ('M', 4, 2 * UNIT, S)], [13] * 80, pos=(12345, -6789))
    add('negative_start', [('M', 7, 6 * LD, S), ('M', 0, 3 * UNIT, S)], [41] * 60, pos=(-100000, -50000))
    add('stamp_wrap', [('M', 4, 5 * UNIT, S)], [30] * 40, last=65500, note='visits cross the u16 tick wrap')
    add('stamp_wrap_big_visit', [('M', 2, 9 * UNIT, S)], [65535, 65535, 1000], last=40000, cover=False)
    add('diag_chunking', [('M', 5, 10 * LD, S)], [60] * 60)
    long_nodes = []
    for i in range(100):
        long_nodes.append(('M', 4 if i % 2 == 0 else 5, 10 * UNIT if i % 2 == 0 else 10 * LD, S))
    add('long_voyage', long_nodes, [4000, 1999, 3001], stride=40, note='about 1200 tiles in 100 segments')
    add('long_voyage_one_tick', long_nodes[:2], [1], stride=500, note='one-tick visits')
    return out


def render():
    lines = ['# MAP-NAV-2 D: stepper vectors from tools/ship_step_ref.py (exact-rational model; not the C++ integrator).',
             '# case NAME | v PERCENT | turn Q32 | last U16 | pos XQ8 YQ8 | nodes ... (M:dir8:len_q32:speed_q32 or T:facing)',
             '# visits TICKS... (increments; the C++ test sets now = last + cumulative mod 65536)',
             '# at VISIT DX DY: floor(model displacement * 500) per axis after that visit (units of q32/500 tile)',
             '# arrive VISIT: first visit whose cumulative ticks reach the exact end of the plan; tol N: allowed |error| in those units']
    for c in cases():
        rows, arrive, tol, total = run_case(c)
        lines.append(f"case {c['name']}")
        if c['note']:
            lines.append(f"# {c['note']}")
        lines.append(f"v {c['V']}")
        lines.append(f"turn {c['turn']}")
        lines.append(f"last {c['last']}")
        lines.append(f"pos {c['pos'][0]} {c['pos'][1]}")
        lines.append('nodes ' + ' '.join(node_text(n) for n in c['nodes']))
        lines.append('visits ' + ' '.join(str(v) for v in c['visits']))
        lines.append(f'arrive {arrive}')
        lines.append(f'tol {tol}')
        lines.append(f'visitcount {len(rows)}')
        lines.append(f'total {floor(total * 1000)}')
        for i, dx, dy in rows:
            if i % c['stride'] == 0 or i == len(rows) or i == arrive:
                lines.append(f'at {i} {dx} {dy}')
        lines.append('end')
    return '\n'.join(lines) + '\n'


def anchors():
    """Hand-derived anchors (no model code involved): 1.0 unit east at 2^26 q32 per stepper unit and
    100 percent: rate = 2^26 * 100 / 500 q32 per tick, so the unit takes 2^32 * 500 / (2^26 * 100) = 320
    ticks and a 160-tick visit is half way: 2^31 q32 = 1073741824000 in units of q32/500."""
    c = dict(nodes=[('M', 4, UNIT, 2 ** 26)], visits=[160, 160, 5], V=100, turn=472446400)
    rows, arrive, _, total = run_case(c)
    assert rows[0][1:] == (2 ** 31 * 500, 0), rows
    assert rows[1][1:] == (2 ** 32 * 500, 0), rows
    assert arrive == 2 and total == 320
    # diagonal: one SE step of length L_d takes L_d * 500 / (2^26 * 100) ticks and moves each axis by
    # L_d * 11863283 / 2^24 (< 1.0 tile: the 2 c^2 = 0.99999997 shortfall of 12.4)
    c = dict(nodes=[('M', 3, LD, 2 ** 26)], visits=[100000], V=100, turn=472446400)
    rows, arrive, _, _ = run_case(c)
    assert rows[0][1] == rows[0][2] == floor(F(LD * 11863283, 2 ** 24) * 500) and rows[0][1] < UNIT * 500, rows
    # a reversal (facing 8 -> 0) by the stepper's own phases: d = 8, six phases, -1 each way
    segs = segments([('M', 4, UNIT, 2 ** 26), ('M', 0, UNIT, 2 ** 26)], 472446400, 100)
    assert len(segs) == 1 + 6 + 1, len(segs)


def selftest():
    anchors()
    if factor_selftest():
        return 1
    base = {n: run_case(c) for n, c in ((c['name'], c) for c in cases())}
    wrong = 0
    for mut in ('dead_zone4', 'diag_unit'):
        changed = [n for n, c in ((c['name'], c) for c in cases()) if run_case(c, mut)[0:2] != base[n][0:2]]
        if not changed:
            print('selftest: mutant', mut, 'was not detected by any case')
            return 1
        print('selftest: mutant', mut, 'detected by', len(changed), 'cases')
        wrong += 1
    return 0


# ---- the speed factor V (MAP-NAV-3 N4; docs/re/ship-navigation.md 7.5, docs/sim/ship-navigation.md
# 12.8 and 12.14). Exact rationals, one explicit truncation per term of the original formula:
#   L = 100 - trunc(penalty * S / ((n0 + n1) * cap))   when owner == volk (S = the first n0 holds,
#                                                       each clamped, the sum clamped; a zero divisor
#                                                       = no cargo term), else L = 100
#   V = trunc((floor + trunc(span * hp / maxhp)) * L / 100), hp = maxhp when unknown, clamped to 1 .. 100
def factor_model(c, mut=None):
    total_cap = (c['n0'] + c['n1']) * c['cap']
    loss = F(0)
    if c['owner'] == c['volk'] and total_cap != 0:
        s = F(0)
        for a in c['holds'][:c['n0']]:
            s = min(F(total_cap), s + min(F(a), F(total_cap)))
        loss = F(floor(F(c['penalty']) * s / total_cap))
    level = 100 - loss
    hp = c['maxhp'] if (c['hp'] is None or mut == 'full_only') else min(c['hp'], c['maxhp'])
    term = floor(F(c['span']) * hp / c['maxhp'])
    if mut == 'round_term':
        term = round(F(c['span']) * hp / c['maxhp'])
    v = floor(F(c['floor'] + term) * level / 100)
    return max(1, min(100, v))


def factor_cases():
    out = []
    base = dict(floor=65, span=35, penalty=17, volk=1, maxhp=6000, n0=4, n1=2, cap=1000 * 256,
                owner=1, hp=None, holds=[])

    def add(name, note, **kw):
        c = dict(base)
        c.update(kw)
        c['name'], c['note'] = name, note
        out.append(c)

    cap = 1000 * 256
    full = [cap] * 4
    add('full_empty', 'hp = max_hp: the term is the whole span', hp=6000)
    add('unknown_equals_full', 'unknown health = the full-health term (adaptation 12.14)', hp=None)
    add('half_empty', 'trunc(35 * 3000 / 6000) = 17', hp=3000)
    add('zero_hp', 'hp 0 is legal: the term is 0, V is the floor', hp=0)
    add('one_hp', 'trunc(35 / 6000) = 0', hp=1)
    add('almost_full', 'trunc(35 * 5999 / 6000) = 34, not rounded to 35', hp=5999)
    add('third', 'trunc(35 * 2000 / 6000) = 11 (11.67 truncates)', hp=2000)
    add('full_cargo_full_hp', 'L 89, V 89', hp=6000, holds=full)
    add('full_cargo_half_hp', 'V = trunc((65 + 17) * 89 / 100) = 72', hp=3000, holds=full)
    add('full_cargo_zero_hp', 'V = trunc(65 * 89 / 100) = 57', hp=0, holds=full)
    add('half_hold_third_hp', 'cargo loss 1, term 11', hp=2000, holds=[cap // 2])
    add('cargo_unknown_hp', 'unknown health with cargo = full-health cargo', hp=None, holds=full)
    add('other_owner_damaged', 'a non-Volk owner has no cargo term, the health term stays', hp=3000, owner=2, holds=full)
    add('other_owner_zero_hp', 'floor only', hp=0, owner=2, holds=full)
    add('type_200_half', 'another type: max_hp 200, trunc(35 * 100 / 200) = 17', maxhp=200, hp=100)
    add('type_200_199', 'trunc(35 * 199 / 200) = 34', maxhp=200, hp=199)
    add('type_65535_one_short', 'max_hp 65535: trunc(35 * 65534 / 65535) = 34', maxhp=65535, hp=65534)
    add('type_1_zero', 'max_hp 1: hp 0 -> floor', maxhp=1, hp=0)
    add('rules_33_65_25_damaged', 'span 33 floor 65 penalty 25, hp 4000/6000: trunc(33 * 2/3) = 22', span=33, penalty=25, hp=4000, holds=full)
    add('rules_floor10_span90', 'floor 10 span 90, hp 1/3: term 30', floor=10, span=90, hp=2000)
    add('rules_floor1_span0', 'floor 1 span 0: V = 1 whatever the health', floor=1, span=0, hp=3000, holds=full)
    add('over_capacity_clamped', 'amounts above the capacity are clamped', hp=3000, holds=[2 ** 63] * 4)
    add('no_capacity', 'zero capacity: no cargo term', hp=3000, cap=0, holds=full)
    add('volk_rule_2', 'the Volk number is a rule: owner 2 pays when volk is 2', hp=3000, volk=2, owner=2, holds=full)
    return out


def factor_render():
    lines = ['# MAP-NAV-3 N4: speed-factor vectors from tools/ship_step_ref.py (exact-rational model, written from the',
             '# formula of docs/re/ship-navigation.md 7.5, not from the C++).',
             '# factor NAME | floor F span S penalty P volk K | maxhp M n0 N n1 N cap C | owner O | hp H (or unknown) |',
             '# holds A,B,... (raw 1/256 ton; "-" none) | expect V']
    for c in factor_cases():
        holds = ','.join(str(a) for a in c['holds']) or '-'
        hp = 'unknown' if c['hp'] is None else str(c['hp'])
        lines.append(f"# {c['note']}")
        lines.append(f"factor {c['name']} floor {c['floor']} span {c['span']} penalty {c['penalty']} volk {c['volk']} "
                     f"maxhp {c['maxhp']} n0 {c['n0']} n1 {c['n1']} cap {c['cap']} owner {c['owner']} hp {hp} "
                     f"holds {holds} expect {factor_model(c)}")
    return '\n'.join(lines) + '\n'


def factor_anchors():
    """Hand-derived (no model code): full health empty = floor + span = 100; half of 6000 hp = 65 + 17 = 82;
    hp 0 = 65; full cargo (4 of 6 hold-capacities, penalty 17): loss trunc(17 * 4 / 6) = 11, L 89: 100 * 89 / 100 = 89."""
    base = dict(floor=65, span=35, penalty=17, volk=1, maxhp=6000, n0=4, n1=2, cap=256000, owner=1, hp=6000, holds=[])
    assert factor_model(base) == 100
    assert factor_model(dict(base, hp=3000)) == 82
    assert factor_model(dict(base, hp=0)) == 65
    assert factor_model(dict(base, hp=None)) == 100
    assert factor_model(dict(base, holds=[256000] * 4)) == 89
    assert factor_model(dict(base, holds=[256000] * 4, hp=3000)) == 72


def factor_selftest():
    factor_anchors()
    for mut in ('full_only', 'round_term'):
        changed = [c['name'] for c in factor_cases() if factor_model(c, mut) != factor_model(c)]
        if not changed:
            print('selftest: factor mutant', mut, 'was not detected by any case')
            return 1
        print('selftest: factor mutant', mut, 'detected by', len(changed), 'cases')
    return 0


def main(argv):
    if len(argv) >= 2 and argv[1] == 'selftest':
        anchors()
        return selftest()
    if len(argv) == 3 and argv[1] == 'write':
        open(argv[2], 'w').write(render())
        return 0
    if len(argv) == 3 and argv[1] == 'check':
        anchors()
        if open(argv[2]).read() != render():
            print('stepper vectors differ from the model output')
            return 1
        print('stepper vectors agree')
        return 0
    if len(argv) == 3 and argv[1] == 'write-factor':
        open(argv[2], 'w').write(factor_render())
        return 0
    if len(argv) == 3 and argv[1] == 'check-factor':
        factor_anchors()
        if open(argv[2]).read() != factor_render():
            print('speed-factor vectors differ from the model output')
            return 1
        print('speed-factor vectors agree')
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
