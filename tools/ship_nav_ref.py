#!/usr/bin/env python3
"""Independent reference for the evidenced ship movement rules (MAP-NAV-1 N4).

Usage:
  ship_nav_ref.py check    <vectors.txt>   run every vector; exit 1 on any mismatch
  ship_nav_ref.py selftest <vectors.txt>   run deliberately wrong variants; each must fail a vector
  ship_nav_ref.py passage-check|passage-selftest <passage_cases.txt>   route reachability / goal acceptance

The vectors are hand-derived (see their header); this file implements the written rules of
docs/re/ship-navigation.md section 7 / cache evidence RC-movement.md 3.2, 3.5, 7. Read-only, no
original data. Passage (route) checks are added by the same tool in N2 (see `passage` records).
"""
import math
import sys
from fractions import Fraction


def itrunc(n, d):  # C integer division (toward zero)
    q = abs(n) // abs(d)
    return q if (n < 0) == (d < 0) else -q


def speed_factor(maxhp, hp, option, n0, n1, cap, S, mut=None):
    L = 100
    if option:
        L = 100 - itrunc(17 * S, (n0 + n1) * cap)
    hp_part = itrunc(35 * hp, maxhp)
    if mut == 'round_hp':
        hp_part = int(Fraction(35 * hp, maxhp) + Fraction(1, 2))
    return itrunc((hp_part + 65) * L, 100)


def turn_plan(facing, want, mut=None):
    ds = (want - facing) & 15
    d = min(ds, 16 - ds)
    thresh = 2 if mut == 'thresh2' else 3
    if d < thresh:
        return ds, d, 0, 0, []
    direction = -1 if ds >= 8 else 1
    if mut == 'reversal_plus':
        direction = 1 if ds >= 8 else direction
    steps = d - 2
    seq, f = [], facing
    for _ in range(steps):
        f = (f + direction) & 15
        seq.append(f)
    return ds, d, steps, direction, seq


def dir16_vec(dx, dz, mut=None):
    theta = math.atan2(dx, dz)  # from +z toward +x
    if theta < 0:
        theta += 2 * math.pi
    k = math.floor(8 * theta / math.pi + 0.5)
    if mut == 'no_half':
        k = math.floor(8 * theta / math.pi)
    return (k + 4) & 15


def dir8_to_dir16(code, mut=None):
    return (2 * code + (1 if mut == 'odd' else 0)) & 15


def chunks(length, mut=None):
    out, rem = [], Fraction(length)
    limit = Fraction(10001, 10000)
    if mut == 'limit1':
        limit = Fraction(1)
    while rem > limit:
        out.append(Fraction(1))
        rem -= 1
    out.append(rem)
    return out


def turn_ticks(f0, mut=None):
    return Fraction(f0) / Fraction('0.01') * 5


def integrate(speed, lens, visit_ticks, n, mut=None):
    """Exact-rational integrator along one axis (docs/re/ship-navigation.md 7.2): one visit consumes
    visit_ticks/5 stepper units; the final partial step of a segment uses exactly remaining/speed time and the
    leftover time carries into the next segment inside the same visit; after the last segment the ship idles
    (zero velocity). Returns (position, 1-based visit at which the last segment finished or 0)."""
    speed = Fraction(speed)
    segs = [Fraction(x) for x in lens]
    pos, rem, i, done = Fraction(0), segs[0], 0, 0
    for v in range(1, n + 1):
        t = Fraction(visit_ticks, 5)
        while done == 0 and t != 0:
            P = t * speed
            if mut == 'overshoot':
                step, time = P, t           # never use remaining/speed for the last partial step
            elif P <= rem:
                step, time = P, t
            else:
                step, time = rem, rem / speed
            if mut == 'floor_visit':
                step = Fraction(math.floor(step * 256), 256)
            pos += step
            rem -= step
            t -= time
            if rem <= 0:
                if i + 1 < len(segs):
                    i += 1
                    rem = segs[i]
                    if mut == 'drop_leftover':
                        t = Fraction(0)
                else:
                    done = v
            if mut == 'floor_visit':
                t = Fraction(0)
    return pos, done


def fmt_frac(f):
    f = Fraction(f)
    return str(f.numerator) if f.denominator == 1 else repr(float(f))


def run_record(kind, args, mut=None):
    if kind == 'speedfactor':
        maxhp, hp, option, n0, n1, cap, S = map(int, args)
        return [str(speed_factor(maxhp, hp, option, n0, n1, cap, S, mut))]
    if kind == 'turn':
        ds, d, steps, direction, seq = turn_plan(int(args[0]), int(args[1]), mut)
        return [str(x) for x in (ds, d, steps, direction, *seq)]
    if kind == 'dir8':
        return [str(dir8_to_dir16(int(args[0]), mut))]
    if kind == 'dir16vec':
        return [str(dir16_vec(float(args[0]), float(args[1]), mut))]
    if kind == 'chunks':
        return [fmt_frac(c) if Fraction(c).denominator == 1 else repr(float(c))
                for c in chunks(args[0], mut)]
    if kind == 'integrate':
        pos, done = integrate(args[0], [Fraction(x) for x in args[1].split(',')], int(args[2]), int(args[3]), mut)
        return [fmt_frac(pos), str(done)]
    if kind == 'turn_ticks':
        t = turn_ticks(args[0], mut)
        return [fmt_frac(t)]
    raise ValueError('unknown record ' + kind)


def norm(tokens):
    out = []
    for t in tokens:
        try:
            v = Fraction(t)
            out.append(str(v.numerator) if v.denominator == 1 else repr(float(v)))
        except ValueError:
            out.append(t)
    return out


def load(path):
    recs = []
    for n, line in enumerate(open(path), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        left, right = line.split('|')
        toks = left.split()
        recs.append((n, toks[0], toks[1:], right.split()))
    return recs


def check(path, mut=None):
    bad = []
    for n, kind, args, want in load(path):
        got = run_record(kind, args, mut)
        if norm(got) != norm(want):
            bad.append((n, kind, args, want, got))
    return bad


# ---- passage (route reachability and goal acceptance) -------------------------------------------

def parse_passage(path):
    cases = []
    for n, line in enumerate(open(path), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = [x.strip() for x in line.split('|')]
        name = parts[0].split()[1]
        W, H = map(int, parts[1].split())
        land = set()
        for rect in parts[2].split()[1:]:
            x0, z0, x1, z1 = map(int, rect.split(','))
            for z in range(z0, z1 + 1):
                for x in range(x0, x1 + 1):
                    land.add((x, z))
        sx, sz = map(float, parts[3].split()[1:])
        gx, gz = map(float, parts[4].split()[1:])
        cases.append((n, name, W, H, land, (sx, sz), (gx, gz), parts[5], parts[6].split(), parts[7]))
    return cases


def passage(W, H, land, start, goal, mut=None):
    """Returns (status, cx, cz, score). status 0 route, 2 start tile chosen, -2 none."""
    st = (math.floor(start[0]), math.floor(start[1]))

    def water(t):
        return 0 <= t[0] < W and 0 <= t[1] < H and t not in land

    def enterable(t):
        return water(t) if mut == 'start_must_be_water' else (t == st or water(t))

    if mut == 'start_must_be_water' and not water(st):
        return -2, None, None, None
    seen, todo = {st}, [st]
    while todo:
        x, z = todo.pop()
        for dx in (-1, 0, 1):
            for dz in (-1, 0, 1):
                if dx == 0 and dz == 0:
                    continue
                t = (x + dx, z + dz)
                if t in seen or not enterable(t):
                    continue
                if dx != 0 and dz != 0 and mut != 'corner_cut':  # no corner cutting
                    if not (enterable((x + dx, z)) and enterable((x, z + dz))):
                        continue
                seen.add(t)
                todo.append(t)
    limit = 7.0 if mut == 'radius7' else 6.0
    best = None
    for t in seen:
        if not water(t):
            continue
        d2 = (t[0] + 0.5 - goal[0]) ** 2 + (t[1] + 0.5 - goal[1]) ** 2
        ok = d2 <= limit * limit if mut == 'radius_le' else d2 < limit * limit
        if not ok:
            continue
        score = 2304 - int(64 * d2)
        # Tie break: lowest tile only so the tool is deterministic. NOT evidence: the original breaks equal scores by
        # local ship congestion (docs/re 9.4); no fixture may assert a tie.
        if best is None or score > best[0] or (score == best[0] and t < best[1]):
            best = (score, t)
    if best is None:
        return -2, None, None, None
    score, t = best
    return (2 if t == st and mut != 'start_not_special' else 0), t[0], t[1], score


def exterior4(W, H, land, start, goal):
    st = (math.floor(start[0]), math.floor(start[1]))
    gt = (math.floor(goal[0]), math.floor(goal[1]))

    def water(t):
        return 0 <= t[0] < W and 0 <= t[1] < H and t not in land

    if not water(st) or not water(gt):
        return 'no'
    seen, todo, border = {st}, [st], False
    while todo:
        x, z = todo.pop()
        border = border or x in (0, W - 1) or z in (0, H - 1)
        for t in ((x + 1, z), (x - 1, z), (x, z + 1), (x, z - 1)):
            if t not in seen and water(t):
                seen.add(t)
                todo.append(t)
    return 'ok' if gt in seen and border else 'no'


def check_passage(path, mut=None):
    bad = []
    for n, name, W, H, land, start, goal, e4, want, source in parse_passage(path):
        status, cx, cz, score = passage(W, H, land, start, goal, mut)
        got = [str(status)] + (['-', '-', '-'] if cx is None else [str(cx), str(cz), str(score)])
        ext = exterior4(W, H, land, start, goal)
        if got != want or (mut is None and ext != e4):
            bad.append((n, name, e4, want, ext, got))
    return bad


def main_passage(argv):
    if argv[1] == 'passage-check':
        if len(parse_passage(argv[2])) < 16:
            print('fewer than 16 passage cases: the fixture is truncated or misnamed')
            return 1
        bad = check_passage(argv[2])
        for b in bad:
            print('MISMATCH line %d %s: exterior4 %s expected %s, got exterior4 %s result %s' % b)
        print('ship passage cases', 'agree' if not bad else 'DIFFER')
        return 1 if bad else 0
    ok = True
    if check_passage(argv[2]):
        print('selftest: the unmutated passage reference already fails')
        return 1
    for m in ['corner_cut', 'radius7', 'radius_le', 'start_must_be_water', 'start_not_special']:
        caught = bool(check_passage(argv[2], m))
        print('mutant %-18s %s' % (m, 'caught' if caught else 'SURVIVED'))
        ok = ok and caught
    print('passage selftest', 'ok' if ok else 'FAILED')
    return 0 if ok else 1


def main(argv):
    if len(argv) == 3 and argv[1] in ('passage-check', 'passage-selftest'):
        return main_passage(argv)
    if len(argv) != 3 or argv[1] not in ('check', 'selftest'):
        print(__doc__)
        return 2
    if argv[1] == 'check':
        bad = check(argv[2])
        kinds = {r[1] for r in load(argv[2])}
        expected = {'speedfactor', 'turn', 'dir8', 'dir16vec', 'chunks', 'turn_ticks', 'integrate'}
        if kinds != expected:
            print('record kinds present %s, expected %s' % (sorted(kinds), sorted(expected)))
            return 1
        for b in bad:
            print('MISMATCH line %d %s %s: expected %s got %s' % b)
        print('ship movement vectors', 'agree' if not bad else 'DIFFER')
        return 1 if bad else 0
    # selftest: every deliberate mistake must be caught by at least one vector
    mutants = ['round_hp', 'thresh2', 'reversal_plus', 'no_half', 'odd', 'limit1', 'overshoot', 'floor_visit',
               'drop_leftover']
    ok = True
    if check(argv[2]):
        print('selftest: the unmutated reference already fails')
        return 1
    for m in mutants:
        caught = bool(check(argv[2], m))
        print('mutant %-14s %s' % (m, 'caught' if caught else 'SURVIVED'))
        ok = ok and caught
    print('selftest', 'ok' if ok else 'FAILED')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
