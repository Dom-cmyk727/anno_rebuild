#!/usr/bin/env python3
"""Independent reference for the evidenced ship movement rules (MAP-NAV-1 N4).

Usage:
  ship_nav_ref.py check    <vectors.txt>   run every vector; exit 1 on any mismatch
  ship_nav_ref.py selftest <vectors.txt>   run deliberately wrong variants; each must fail a vector

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


def main(argv):
    if len(argv) != 3 or argv[1] not in ('check', 'selftest'):
        print(__doc__)
        return 2
    if argv[1] == 'check':
        bad = check(argv[2])
        for b in bad:
            print('MISMATCH line %d %s %s: expected %s got %s' % b)
        print('ship movement vectors', 'agree' if not bad else 'DIFFER')
        return 1 if bad else 0
    # selftest: every deliberate mistake must be caught by at least one vector
    mutants = ['round_hp', 'thresh2', 'reversal_plus', 'no_half', 'odd', 'limit1']
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
