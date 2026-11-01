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


K5 = [[0, 0, 1, 0, 0], [0, 1, 2, 1, 0], [1, 2, 3, 2, 1], [0, 1, 2, 1, 0], [0, 0, 1, 0, 0]]
K3 = [[1, 1, 1], [1, 2, 1], [1, 1, 1]]
DENS = [[0, 1, 2, 3], [1, 1, 2, 3], [2, 2, 3, 3], [3, 3, 3, 3]]  # state' = DENS[state][kernel value]


def density(stamps, tile, mut=None):
    """Ship-density at a tile from route-engine stamps (docs/re/ship-navigation.md 9.4): each stamp is
    (x, z, kernel, R); its tile is the TRUNCATED point; the kernel cell is tile - (stamp tile - R); the values combine in
    record order through DENS."""
    state = 0
    for sx, sz, kern, R in stamps:
        tx = round(sx) if mut == 'round_tile' else math.trunc(sx)
        tz = round(sz) if mut == 'round_tile' else math.trunc(sz)
        col, row = tile[0] - (tx - R), tile[1] - (tz - R)
        if 0 <= col <= 2 * R and 0 <= row <= 2 * R:
            k = (K5 if kern == 'K5' else K3)[row][col]
            state = max(state, k) if mut == 'max_combine' else DENS[state][k]
    return state


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
    if kind == 'density':
        # density <K5|K3> <R> <x,z> ... tile <x> <z>
        i = args.index('tile')
        stamps = [(float(a.split(',')[0]), float(a.split(',')[1]), args[0], int(args[1])) for a in args[2:i]]
        return [str(density(stamps, (int(args[i + 1]), int(args[i + 2])), mut))]
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


DIRS = {'W': (-1, 0), 'SW': (-1, 1), 'S': (0, 1), 'SE': (1, 1), 'E': (1, 0), 'NE': (1, -1), 'N': (0, -1), 'NW': (-1, -1)}


def parse_route(line):
    parts = [x.strip() for x in line.split('|')]
    name = parts[0].split()[1]
    W, H = map(int, parts[1].split())
    land = set()
    for rect in parts[2].split()[1:]:
        x0, z0, x1, z1 = map(int, rect.split(','))
        land.update((x, z) for z in range(z0, z1 + 1) for x in range(x0, x1 + 1))
    return dict(name=name, W=W, H=H, land=land, start=tuple(map(float, parts[3].split()[1:])),
                goal=tuple(map(float, parts[4].split()[1:])), status=int(parts[5]),
                runs=[] if parts[6] == '-' else parts[6].split(),
                stamped=len(parts) > 7 and parts[7] != 'stamps -', end=tuple(map(int, parts[8].split()[1:])) if len(parts) > 8 else None,
                expect=parts[9][len('expect '):] if len(parts) > 9 else None)


def path_legality(rec, mut=None):
    """Expands the runs into single steps and checks, independently of the endpoint: run syntax and cap, the map
    bounds, that every entered tile is water (the start tile is the only land tile that may be left), and that a
    diagonal step has BOTH side tiles enterable (no corner cutting). Returns (problems, end tile)."""
    problems = []
    W, H, land = rec['W'], rec['H'], rec['land']
    st = (math.floor(rec['start'][0]), math.floor(rec['start'][1]))
    x, z = st
    cap = 9 if mut == 'cap9' else 10

    def enterable(t):
        return t == st or (0 <= t[0] < W and 0 <= t[1] < H and t not in land)

    for r in rec['runs']:
        d = r.rstrip('0123456789')
        length = int(r[len(d):]) if r[len(d):] else 0
        if d not in DIRS or length > cap or length < 1:
            problems.append('bad run %s' % r)
            continue
        dx, dz = DIRS[d]
        if mut == 'flip_z':
            dz = -dz
        for _ in range(length):
            nx, nz = x + dx, z + dz
            if not (0 <= nx < W and 0 <= nz < H):
                if mut != 'no_bounds_check':
                    problems.append('step leaves the map at %s' % ((nx, nz),))
            elif (nx, nz) in land and mut != 'no_land_check':
                problems.append('step enters land at %s' % ((nx, nz),))
            elif dx != 0 and dz != 0 and mut != 'allow_corner_cut' and not (enterable((x + dx, z)) and enterable((x, z + dz))):
                problems.append('diagonal step cuts a corner at %s' % ((x, z),))
            x, z = nx, nz
    return problems, (x, z)


def route_problems(rec, mut=None):
    """Three SEPARATE checks (passing one proves nothing about the others): (1) path legality; (2) the independent
    status / goal-candidate reference (`passage`; for stamped records the recorded end tile is the only reference until
    the Offer rule is modelled); (3) exact run equality is NOT checked here - the expected runs are emulated-original data
    that only the C++ port can reproduce."""
    problems, end = path_legality(rec, mut)
    st, cx, cz, _ = passage(rec['W'], rec['H'], rec['land'], rec['start'], rec['goal'])
    if rec['stamped'] and rec['end'] is not None:
        cx, cz = rec['end']
    if st != rec['status'] and not rec['stamped']:
        problems.append('status %s, passage reference %s' % (rec['status'], st))
    if rec['status'] == 0 and end != (cx, cz):
        problems.append('runs end on %s, candidate %s' % (end, (cx, cz)))
    if rec['status'] != 0 and rec['runs']:
        problems.append('runs on a non-route status')
    return problems


def check_routes(path, mut=None):
    """Cross-checks the route-run vectors against the independent passage reference and the legality walk."""
    bad, count = [], 0
    for n, line in enumerate(open(path), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        rec = parse_route(line)
        count += 1
        problems = route_problems(rec, mut)
        if problems:
            bad.append((n, rec['name'], problems))
    if count < 30:
        bad.append((0, 'fixture', ['fewer than 30 route vectors']))
    return bad


def check_counterexamples(path):
    """Each record of the counterexample file is a deliberately CORRUPT route (valid endpoint) that must be rejected with
    the named reason; records marked `expect accept` are valid controls that must pass."""
    bad, seen_accept, seen_reject = [], 0, 0
    for n, line in enumerate(open(path), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        rec = parse_route(line)
        problems = route_problems(rec)
        want = rec['expect']
        if want == 'accept':
            seen_accept += 1
            if problems:
                bad.append((n, rec['name'], ['control rejected: %s' % problems]))
        else:
            seen_reject += 1
            if not any(want in p for p in problems):
                bad.append((n, rec['name'], ['expected a problem containing %r, got %s' % (want, problems)]))
    if seen_accept < 1 or seen_reject < 3:
        bad.append((0, 'fixture', ['need >= 1 accepted control and >= 3 rejected corruptions']))
    return bad


# ---- goal-candidate selection with stamps (MAP-NAV-2 A3) ----------------------------------------------------

_LINE = [3, 4, 2, 5, 1, 6, 0, 7]
CAND_ORDER = [
    [(c, r) for c in range(7, -1, -1) for r in _LINE],   # arrival labels 0x80/0x81 (from the west side): columns 7..0
    [(c, r) for r in range(8) for c in _LINE],           # 0x82/0x83: rows 0..7
    [(c, r) for c in range(8) for r in _LINE],           # 0x84/0x85: columns 0..7
    [(c, r) for r in range(7, -1, -1) for c in _LINE],   # 0x86/0x87: rows 7..0
    [(3, 3), (4, 3), (3, 4), (4, 4), (2, 3), (2, 4), (5, 3), (5, 4), (3, 2), (4, 2), (3, 5), (4, 5), (2, 2), (5, 2), (2, 5),
     (5, 5), (3, 1), (4, 1), (2, 1), (5, 1), (3, 6), (4, 6), (2, 6), (5, 6), (1, 3), (1, 4), (1, 2), (1, 5), (6, 3), (6, 4),
     (6, 2), (6, 5), (1, 1), (6, 1), (1, 6), (6, 6), (3, 0), (4, 0), (3, 7), (4, 7), (0, 3), (0, 4), (7, 3), (7, 4), (2, 0),
     (5, 0), (2, 7), (5, 7), (0, 2), (0, 5), (7, 2), (7, 5), (1, 0), (6, 0), (1, 7), (6, 7), (0, 1), (0, 6), (7, 1), (7, 6),
     (0, 0), (7, 0), (0, 7), (7, 7)]]


def cand_group(label, mut=None):
    if mut == 'one_group':
        return 4
    return ((label + 0x80) & 0xFF) >> 1 if 0x80 <= label <= 0x87 else 4


def select_candidate(water, goal, stamps, pops, mut=None):
    """Python transcription of docs/re/ship-navigation.md 9.4 (callback + Offer + goal-test order). `pops` = the coarse pop
    order of the goal-flag cells (cx, cz, label). Returns (tile or None, best score)."""
    best, has, stored = 0, False, None
    for cx, cz, label in pops:
        for col, row in CAND_ORDER[cand_group(label, mut)]:
            t = (cx * 8 + col, cz * 8 + row)
            if not water(t):
                continue
            d2 = (t[0] + 0.5 - goal[0]) ** 2 + (t[1] + 0.5 - goal[1]) ** 2
            sc = 2304 - int(64 * d2)
            if sc <= 0:
                continue
            if not (sc > best or (mut == 'offer_equal' and sc >= best)):
                continue
            dens = density(stamps, t, 'max_combine' if mut == 'max_combine' else None)
            if dens == 0:
                stored, has, best = (t, 0), True, sc
                if sc >= 2304:
                    return t, best
            elif has and (dens > stored[1] if mut == 'last_wins_tie' else dens >= stored[1]):
                pass
            else:
                stored, has = (t, dens), True
                if mut == 'best_on_congested':
                    best = sc
    return (stored[0] if stored else None), best


def check_offers(path, mut=None):
    bad, count = [], 0
    for n, line in enumerate(open(path), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = [x.strip() for x in line.split('|')]
        name = parts[0].split()[1]
        start = tuple(map(float, parts[1].split()[1:]))
        goal = tuple(map(float, parts[2].split()[1:]))
        stamps = []
        if parts[3] != 'stamps -':
            for tok in parts[3].split()[1:]:
                x, z, k = tok.split(',')
                stamps.append((float(x), float(z), k, 2 if k == 'K5' else 1))
        pops = [tuple(int(v) for v in tok.split(',')) for tok in parts[4].split()[1:]]
        end = tuple(map(int, parts[5].split()[1:]))
        best = int(parts[6].split()[1])
        runs = parts[7].split()
        count += 1
        tile, sc = select_candidate(lambda t: 0 <= t[0] < 64 and 0 <= t[1] < 64, goal, stamps, pops, mut)
        problems = []
        if tile != end:
            problems.append('end tile %s, reference %s' % (end, tile))
        if sc != best:
            problems.append('best score %s, reference %s' % (best, sc))
        rec = dict(W=64, H=64, land=set(), start=start, runs=runs)
        leg, walked = path_legality(rec)
        problems += leg
        if walked != end:
            problems.append('runs end on %s, recorded end %s' % (walked, end))
        if problems:
            bad.append((n, name, problems))
    if count < 16:
        bad.append((0, 'fixture', ['fewer than 16 offer cases']))
    return bad


def check_objects(path):
    bad, ntile, nshore = [], 0, 0
    for n, line in enumerate(open(path), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        if line.startswith('objtile'):
            left, right = [x.strip() for x in line.split('|')]
            h, base = map(int, left.split()[1:])
            ntile += 1
            got = 'blocked' if ((h + base) & 0xFF) > 19 else 'free'
            if got != right:
                bad.append((n, 'objtile %d %d: expected %s got %s' % (h, base, right, got)))
        elif line.startswith('shore'):
            nshore += 1
            parts = [x.strip() for x in line.split('|')]
            W = H = 64
            land = set()
            if parts[1] != 'land -':
                for rect in parts[1].split()[1:]:
                    x0, z0, x1, z1 = map(int, rect.split(','))
                    land.update((x, z) for z in range(z0, z1 + 1) for x in range(x0, x1 + 1))
            start = tuple(map(float, parts[3].split()[1:]))
            end = tuple(map(int, parts[8].split()[1:]))
            for field in (parts[6], parts[7]):
                toks = field.split()
                runs = toks[2:] if toks[0] == 'no_band' or toks[0] == 'band' else toks
                prob, walked = path_legality(dict(W=W, H=H, land=land, start=start, runs=runs))
                if prob or walked != end:
                    bad.append((n, 'shore walk %s: %s end %s' % (parts[0], prob, walked)))
    if ntile < 12 or nshore < 7:
        bad.append((0, 'fixture truncated'))
    return bad


def main_goal(argv):
    if argv[1] == 'offer-check':
        bad = check_offers(argv[2])
        for b in bad:
            print('OFFER MISMATCH line %d %s: %s' % b)
        print('ship offer cases', 'agree' if not bad else 'DIFFER')
        return 1 if bad else 0
    if argv[1] == 'offer-selftest':
        if check_offers(argv[2]):
            print('selftest: the unmutated offer reference already fails')
            return 1
        ok = True
        for m in ['best_on_congested', 'offer_equal', 'last_wins_tie', 'one_group']:
            caught = bool(check_offers(argv[2], m))
            print('mutant %-18s %s' % (m, 'caught' if caught else 'SURVIVED'))
            ok = ok and caught
        print('offer selftest', 'ok' if ok else 'FAILED')
        return 0 if ok else 1
    bad = check_objects(argv[2])
    for b in bad:
        print('OBJECT/SHORE MISMATCH line %d: %s' % b)
    print('ship object and shore cases', 'agree' if not bad else 'DIFFER')
    return 1 if bad else 0


# ---- planning admission (MAP-NAV-2 A2) ------------------------------------------------------------------------

def admission_sim(rule, n, dtick, ships, quota, advances, adversary, seed=0):
    """Models the registry cursor/cadence (visit k = clamp(floor(dtick*n/50), 1, n) slots per Advance from a circular cursor,
    slots visited in ascending order) and the planning admission of every ship in `ships` (slot indices) that has a pending
    order. Rules: `quota_first` = the first `quota` visited pending ships per Advance (the rejected first-come rule);
    `fifo_reserve` = a visited pending ship is admitted iff the number of OLDER pending tickets is smaller than the quota
    left in this Advance (a replacement keeps the ship's ticket; a served or cancelled ticket is removed). Adversaries:
    `replace_served` = a served ship immediately receives a new order (a new, youngest ticket) and, under fifo_reserve,
    other pending ships are replaced every Advance (their tickets stay). Returns (service counts per ship, max wait in
    Advances for any ticket from its issue to its service)."""
    import random
    rng = random.Random(seed)
    k = max(1, min(n, (dtick * n) // 50))
    seq = 0
    tickets = {}          # slot -> (seq, issued advance)
    for sl in ships:
        seq += 1
        tickets[sl] = (seq, 0)
    served = {sl: 0 for sl in ships}
    max_wait = 0
    cursor = 0
    for a in range(advances):
        visited = [(cursor + i) % n for i in range(k)]
        remaining = quota
        for sl in sorted(visited):
            if sl not in tickets:
                continue
            mine = tickets[sl][0]
            older = sum(1 for (q, _) in tickets.values() if q < mine)
            ok = remaining > 0 if rule == 'quota_first' else older < remaining  # fifo_reserve and the mutant fifo_renew
            if rule == 'fifo_no_reserve':
                ok = remaining > 0 and older == 0
            if ok:
                remaining -= 1
                served[sl] += 1
                max_wait = max(max_wait, a - tickets[sl][1])
                del tickets[sl]
                if adversary in ('replace_served', 'random', 'replace_all'):
                    seq += 1
                    tickets[sl] = (seq, a)
        if adversary == 'replace_all':  # every pending ship is re-commanded every Advance; the correct rule KEEPS tickets
            if rule == 'fifo_renew':    # mutant: a replacement renews the ticket (moves the ship to the back)
                for sl in sorted(tickets):
                    seq += 1
                    tickets[sl] = (seq, tickets[sl][1])
        if adversary == 'random':
            for sl in ships:  # random replacement / cancellation / re-command; replacement KEEPS the ticket
                r = rng.random()
                if sl in tickets and r < 0.3 and rule != 'quota_first':
                    pass  # replaced: ticket unchanged
                elif sl in tickets and r < 0.35:
                    del tickets[sl]  # cancelled
                elif sl not in tickets and r < 0.5:
                    seq += 1
                    tickets[sl] = (seq, a)
        cursor = (cursor + k) % n
    for (_, issued) in tickets.values():  # a ticket still unserved at the end has waited at least this long (starvation)
        max_wait = max(max_wait, advances - issued)
    return [served[sl] for sl in ships], max_wait


def main_admission(argv):
    path = argv[2]
    bad = []
    for n, line in enumerate(open(path), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        left, right = [x.strip() for x in line.split('|')]
        f = left.split()
        rule, nn, dtick = f[1], int(f[2]), int(f[3])
        ships = [int(x) for x in f[4].split(',')]
        quota, adv, adversary = int(f[5]), int(f[6]), f[7]
        counts, _ = admission_sim(rule, nn, dtick, ships, quota, adv, adversary)
        if right.startswith('counts'):
            want = [int(x) for x in right.split()[1:]]
            if counts != want:
                bad.append((n, 'counts %s expected %s' % (counts, want)))
        else:  # bound <max wait advances formula>: every ticket served within (older+1)*C + ready slack
            C = -(-nn // max(1, min(nn, (dtick * nn) // 50)))
            _, mw = admission_sim(rule, nn, dtick, ships, quota, adv, adversary, seed=int(right.split()[1]))
            bound = (len(ships) + 1) * C
            expect_ok = right.split()[2] == 'within'
            if (mw <= bound) != expect_ok:
                bad.append((n, 'max wait %d, bound %d, expected %s' % (mw, bound, right.split()[2])))
    for b in bad:
        print('ADMISSION MISMATCH line %d: %s' % b)
    print('ship admission cases', 'agree' if not bad else 'DIFFER')
    return 1 if bad else 0


def main_passage(argv):
    if argv[1] in ('route-check', 'route-selftest'):
        if argv[1] == 'route-check':
            bad = check_routes(argv[2])
            for b in bad:
                print('ROUTE MISMATCH line %d %s: %s' % b)
            print('ship route vectors', 'agree' if not bad else 'DIFFER')
            return 1 if bad else 0
        if check_routes(argv[2]):
            print('selftest: the unmutated route cross-check already fails')
            return 1
        ok = True
        for m in ['cap9', 'flip_z']:
            caught = bool(check_routes(argv[2], m))
            print('mutant %-17s %s' % (m, 'caught' if caught else 'SURVIVED'))
            ok = ok and caught
        # legality mutants must let a counterexample ESCAPE (i.e. they are detected by the counterexample file)
        cx = argv[2].replace('route_runs.txt', 'route_counterexamples.txt')
        for m in ['allow_corner_cut', 'no_land_check', 'no_bounds_check']:
            escaped = False
            for line in open(cx):
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                rec = parse_route(line)
                if rec['expect'] != 'accept' and not route_problems(rec, m):
                    escaped = True
            print('legality mutant %-17s %s' % (m, 'caught by a counterexample' if escaped else 'SURVIVED'))
            ok = ok and escaped
        print('route selftest', 'ok' if ok else 'FAILED')
        return 0 if ok else 1
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
    if len(argv) == 3 and argv[1] in ('offer-check', 'offer-selftest', 'object-check'):
        return main_goal(argv)
    if len(argv) == 3 and argv[1] == 'admission-check':
        return main_admission(argv)
    if len(argv) == 3 and argv[1] == 'route-reject':
        bad = check_counterexamples(argv[2])
        for b in bad:
            print('COUNTEREXAMPLE ESCAPED line %d %s: %s' % b)
        print('ship route counterexamples', 'rejected as required' if not bad else 'NOT REJECTED')
        return 1 if bad else 0
    if len(argv) == 3 and argv[1] in ('passage-check', 'passage-selftest', 'route-check', 'route-selftest'):
        return main_passage(argv)
    if len(argv) != 3 or argv[1] not in ('check', 'selftest'):
        print(__doc__)
        return 2
    if argv[1] == 'check':
        bad = check(argv[2])
        kinds = {r[1] for r in load(argv[2])}
        expected = {'speedfactor', 'turn', 'dir8', 'dir16vec', 'chunks', 'turn_ticks', 'integrate', 'density'}
        if kinds != expected:
            print('record kinds present %s, expected %s' % (sorted(kinds), sorted(expected)))
            return 1
        for b in bad:
            print('MISMATCH line %d %s %s: expected %s got %s' % b)
        print('ship movement vectors', 'agree' if not bad else 'DIFFER')
        return 1 if bad else 0
    # selftest: every deliberate mistake must be caught by at least one vector
    mutants = ['round_hp', 'thresh2', 'reversal_plus', 'no_half', 'odd', 'limit1', 'overshoot', 'floor_visit',
               'drop_leftover', 'round_tile', 'max_combine']
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
