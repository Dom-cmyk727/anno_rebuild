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
"""
import sys
from fractions import Fraction as F
from math import floor

IDLE_SPEED = 42949672                  # f32(0.01) * 2^32
UNIT = 2 ** 32
LD = 11863283 * 2 ** 9                 # diagonal step length, q32
C = F(11863283, 2 ** 24)               # diagonal axis component per unit length
CHUNK_LIMIT = F(2 ** 37 + 13421773, 32)  # a node longer than this is cut (1.0 + f32(1e-4), q32)
DX = [-1, -1, 0, 1, 1, 1, 0, -1]
DY = [0, 1, 1, 1, 0, -1, -1, -1]


def segments(nodes, turn_step, V, mut=None):
    """[(duration_ticks, dx_per_tick, dy_per_tick)] for a committed plan; positions in q32 tiles."""
    segs = []
    first = next(n for n in nodes if n[0] == 'M')
    facing = 2 * first[1]                      # the commit sets the facing directly

    def turn_seg(length):
        rate = F(IDLE_SPEED * 100, 500)        # q32 per tick
        segs.append((F(length) / rate, F(0), F(0)))

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
        segs.append((F(length) / rate, vx, vy))
    return segs


def position_at(segs, t):
    t = F(t)
    x = y = F(0)
    for dur, vx, vy in segs:
        use = min(t, dur)
        x += vx * use
        y += vy * use
        t -= use
        if t <= 0:
            break
    return x, y


def total_ticks(segs):
    return sum((s[0] for s in segs), F(0))


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
    # every segment start can floor once, every diagonal segment end drops one residue
    tol = 2 * len(segs) + 2
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
    add('speed_factor_65', [('M', 6, 5 * UNIT, S), ('M', 7, 3 * LD, S)], [29] * 60, V=65)
    add('speed_change_fraction', [('M', 4, 2 * UNIT, S2), ('M', 4, 3 * UNIT, S3), ('M', 4, 2 * UNIT, S2)], [97] * 60)
    add('speed_change_mid_visit', [('M', 4, UNIT // 3 * 3, S2), ('M', 4, UNIT, 3 * S2), ('M', 3, LD, S)], [400, 77, 5000], cover=False)
    add('sub_tile_start', [('M', 3, 4 * LD, S), ('M', 4, 2 * UNIT, S)], [13] * 80, pos=(12345, -6789))
    add('negative_start', [('M', 7, 6 * LD, S), ('M', 0, 3 * UNIT, S)], [41] * 60, pos=(-100000, -50000))
    add('stamp_wrap', [('M', 4, 5 * UNIT, S)], [30] * 40, last=65500, note='visits cross the u16 tick wrap')
    add('stamp_wrap_big_visit', [('M', 2, 9 * UNIT, S)], [65535, 65535, 1000], last=40000, cover=False)
    add('one_chunk_boundary', [('M', 4, UNIT + 419431, S)], [50] * 30, note='just above 1.0 + f32(1e-4): cut in two chunks')
    add('below_chunk_limit', [('M', 4, UNIT + 419430, S)], [50] * 30, note='at most 1.0 + f32(1e-4): installed whole')
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
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
