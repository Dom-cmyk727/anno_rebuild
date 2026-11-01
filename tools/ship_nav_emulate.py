#!/usr/bin/env python3
"""Generator of the emulated-original route fixtures (MAP-NAV-2 package C).

The expected values of fixtures/synthetic/sim/nav/{packer_vectors,route_emulated}.txt come from the
ORIGINAL code: the real AnnoWelt.dll bytes (mask packer 0x100a4130, start nibble 0x1000fa50, the whole
global route engine ctor 0x10012030 / SetWindow / Run 0x10011500 / Trace 0x10011640) executed under
unicorn, with only the world object faked (width, height and the region exporter: kind 9 = coarse
cells from the real packer, kind 10 = tile bytes 8 water / 9 land). This tool needs the private harness
of the evidence notes (NOT in git; it reads the game file AnnoWelt.dll, never writes under game/):

  ANNO_NAV_HARNESS=<repo>/cache/map2-nav1  python with capstone, pefile, unicorn
  ship_nav_emulate.py packer  <count> <seed>              > packer_vectors.txt body
  ship_nav_emulate.py nibble  <count> <seed>              > nibble body
  ship_nav_emulate.py routes  <count> <seed> <profile>    > route_emulated.txt body

Profiles: mix (random coasts), learn (keep only queries that needed learned masks), edge (starts and
goals on block / sector edges), band (shore-band cells injected into the coarse export).
The map generators are seeded and deterministic; nothing here is evidence by itself - the expected
columns are what the original code printed.
"""
import os
import random
import struct
import sys

HARNESS = os.environ.get('ANNO_NAV_HARNESS')
if not HARNESS:
    sys.exit('set ANNO_NAV_HARNESS to the private cache/map2-nav1 directory')
sys.path[:0] = [os.path.join(HARNESS, 'dumpF'), os.path.join(HARNESS, 'dumpJ')]

DIRS = ['W', 'SW', 'S', 'SE', 'E', 'NE', 'N', 'NW']


def rows_from_rects(w, h, rects):
    g = [['.'] * w for _ in range(h)]
    for (x0, z0, x1, z1) in rects:
        for z in range(max(z0, 0), min(z1, h - 1) + 1):
            for x in range(max(x0, 0), min(x1, w - 1) + 1):
                g[z][x] = '#'
    return [''.join(r) for r in g]


def cmd_packer(count, seed):
    from packer import pack
    rng = random.Random(seed)
    seen = set()
    out = []

    def emit(tiles):
        key = ''.join(tiles)
        if key in seen:
            return
        seen.add(key)
        out.append('pack %s | %03x' % (key.replace('w', '.'), pack(tiles)))

    # structured cases first: edges, corners, columns, rows, corridors
    def mk(coords):
        g = [['#'] * 8 for _ in range(8)]
        for (x, z) in coords:
            g[z][x] = 'w'
        return [''.join(r) for r in g]
    emit(['w' * 8] * 8)
    emit(['#' * 8] * 8)
    for c in [(0, 0), (7, 0), (0, 7), (7, 7), (3, 0), (0, 3), (7, 3), (3, 7), (3, 3)]:
        emit(mk([c]))
    for c in [[(0, 0), (1, 0)], [(0, 0), (0, 1)], [(7, 7), (6, 7)], [(7, 0), (7, 1)]]:
        emit(mk(c))
    emit(mk([(x, 0) for x in range(8)]))
    emit(mk([(x, 7) for x in range(8)]))
    emit(mk([(0, z) for z in range(8)]))
    emit(mk([(7, z) for z in range(8)]))
    emit(mk([(3, z) for z in range(8)]))
    emit(mk([(x, 3) for x in range(8)]))
    emit(mk([(x, x) for x in range(8)]))  # diagonal: no 4-connection
    emit(mk([(x, x) for x in range(8)] + [(x + 1, x) for x in range(7)]))  # staircase
    while len(out) < count:
        density = rng.choice([0.1, 0.25, 0.4, 0.55, 0.7, 0.85, 0.95])
        tiles = [''.join('w' if rng.random() < density else '#' for _ in range(8)) for _ in range(8)]
        if rng.random() < 0.3:  # carve a random corridor
            g = [list(r) for r in tiles]
            x, z = rng.randrange(8), rng.randrange(8)
            for _ in range(rng.randrange(4, 14)):
                g[z][x] = 'w'
                dx, dz = rng.choice([(1, 0), (-1, 0), (0, 1), (0, -1)])
                x, z = min(7, max(0, x + dx)), min(7, max(0, z + dz))
            tiles = [''.join(r) for r in g]
        emit(tiles)
    return out


def cmd_nibble(count, seed):
    from nib import nibble
    rng = random.Random(seed)
    out = []
    for _ in range(count):
        density = rng.choice([0.15, 0.3, 0.5, 0.7, 0.9])
        tiles = [''.join('w' if rng.random() < density else '#' for _ in range(8)) for _ in range(8)]
        lx, lz = rng.randrange(8), rng.randrange(8)
        val = nibble(tiles, lx + 0.5, lz + 0.5)
        key = ''.join(tiles).replace('w', '.')
        out.append('nibble %s | %d %d | %04x' % (key, lx, lz, val & 0xf000))
    return out


def fmt_f(v):
    return ('%.6f' % v).rstrip('0').rstrip('.') if '.' in ('%.6f' % v) else str(v)


def gen_rects(rng, w, h, profile):
    rects = []
    n = rng.randrange(2, 9)
    for _ in range(n):
        kind = rng.choice(['block', 'wall', 'wall', 'bar', 'gapwall', 'stagger', 'lake', 'scatter'])
        if kind == 'block':
            x0, z0 = rng.randrange(w), rng.randrange(h)
            rects.append((x0, z0, x0 + rng.randrange(1, 40), z0 + rng.randrange(1, 40)))
        elif kind == 'wall':
            x0 = rng.randrange(8, w - 8)
            t = rng.randrange(1, 4)
            z0, z1 = rng.randrange(0, h // 2), rng.randrange(h // 2, h)
            rects.append((x0, z0, x0 + t - 1, z1))
        elif kind == 'bar':
            z0 = rng.randrange(8, h - 8)
            t = rng.randrange(1, 4)
            x0, x1 = rng.randrange(0, w // 2), rng.randrange(w // 2, w)
            rects.append((x0, z0, x1, z0 + t - 1))
        elif kind == 'gapwall':
            x0 = rng.randrange(8, w - 8)
            t = rng.randrange(1, 3)
            gaps = sorted(rng.sample(range(2, h - 2), rng.randrange(1, 4)))
            prev = 0
            for g in gaps:
                if g - 1 >= prev:
                    rects.append((x0, prev, x0 + t - 1, g - 1))
                prev = g + rng.randrange(1, 3)
            rects.append((x0, prev, x0 + t - 1, h - 1))
        elif kind == 'stagger':
            x0 = rng.randrange(8, w - 8)
            g1 = rng.randrange(4, h - 8)
            g2 = g1 + rng.randrange(2, 5)
            rects.append((x0, 0, x0, g1))
            rects.append((x0, g1 + 2, x0, h - 1))
            rects.append((x0 + 1, 0, x0 + 1, g2))
            rects.append((x0 + 1, g2 + 2, x0 + 1, h - 1))
        elif kind == 'lake':
            x0, z0 = rng.randrange(4, w - 30), rng.randrange(4, h - 30)
            a, b = rng.randrange(8, 24), rng.randrange(8, 24)
            rects += [(x0, z0, x0 + a, z0 + 1), (x0, z0 + b - 1, x0 + a, z0 + b),
                      (x0, z0, x0 + 1, z0 + b), (x0 + a - 1, z0, x0 + a, z0 + b)]
        else:
            for _ in range(rng.randrange(3, 12)):
                x, z = rng.randrange(w), rng.randrange(h)
                rects.append((x, z, x, z))
    # a diagonal bar sometimes
    if rng.random() < 0.25:
        x0, z0 = rng.randrange(w // 2), rng.randrange(h // 2)
        for i in range(rng.randrange(10, 40)):
            rects.append((x0 + i, z0 + i, x0 + i, z0 + i))
            if rng.random() < 0.5:
                rects.append((x0 + i + 1, z0 + i, x0 + i + 1, z0 + i))
    return rects


def pick_point(rng, w, h, water, profile, want_water):
    for _ in range(200):
        if profile == 'edge':
            xs = [0, 7, 8, 15, 16, 63, 64, 127, 128, w - 1, w - 8]
            x = rng.choice([v for v in xs if v < w] + [rng.randrange(w)])
            z = rng.choice([v for v in xs if v < h] + [rng.randrange(h)])
        else:
            x, z = rng.randrange(w), rng.randrange(h)
        if water(x, z) == want_water:
            return x, z
    return rng.randrange(w), rng.randrange(h)


def cmd_routes(count, seed, profile):
    from eng import Eng
    import eng as E
    rng = random.Random(seed)
    sizes = [(64, 64), (64, 64), (128, 64), (128, 128), (192, 128), (136, 72)]
    if profile == 'edge':
        sizes = [(128, 128), (192, 128), (136, 72), (72, 136)]

    class EngBand(Eng):
        band = frozenset()

        def fill(s, kind, a2, rectp, dest, stride):
            if kind != 9:
                return Eng.fill(s, kind, a2, rectp, dest, stride)
            x0, z0, x1, z1 = struct.unpack('<4i', s.mu.mem_read(rectp, 16))
            buf = bytearray()
            for cz in range(8):
                for cx in range(8):
                    tx, tz = x0 + cx * 8, z0 + cz * 8
                    if tx >= s.Wd or tz >= s.H or tx < 0 or tz < 0:
                        m, t = 0, 0
                    else:
                        m = s.cellmask(tx // 8, tz // 8)
                        t = 3 if (tx // 8, tz // 8) in s.band else 0
                    buf += struct.pack('<HH', m, t)
            s.mu.mem_write(dest, bytes(buf))
            return 1

    out = []
    tries = 0
    while len(out) < count and tries < count * 40:
        tries += 1
        w, h = rng.choice(sizes)
        rects = gen_rects(rng, w, h, profile)
        rows = rows_from_rects(w, h, rects)

        def water(x, z):
            return 0 <= x < w and 0 <= z < h and rows[z][x] == '.'
        sx, sz = pick_point(rng, w, h, water, profile, rng.random() < 0.97)
        gx, gz = pick_point(rng, w, h, water, profile, rng.random() < 0.75)
        fr = [rng.choice([0.5, 0.5, 0.5, 0.0, 0.25, 0.75, 0.984375]) for _ in range(4)]
        start = (sx + fr[0], sz + fr[1])
        goal = (gx + fr[2], gz + fr[3])
        band = set()
        if profile == 'band':
            for _ in range(rng.randrange(1, 4)):
                bx0, bz0 = rng.randrange(w // 8), rng.randrange(h // 8)
                for i in range(rng.randrange(1, 9)):
                    if rng.random() < 0.5:
                        band.add((min(bx0 + i, w // 8 - 1), bz0))
                    else:
                        band.add((bx0, min(bz0 + i, h // 8 - 1)))
        e = EngBand(rows).setup()
        e.band = frozenset(band)
        r = e.full(start, goal)
        learned = e.u32(e.W + 0x1f74)
        if profile == 'learn' and learned == 0:
            continue
        runs = ' '.join('%s%d' % (DIRS[d], n) for d, n in r['runs']) if r['runs'] else '-'
        land = ' '.join('%d,%d,%d,%d' % t for t in rects) or '-'
        bandtxt = ' '.join('%d,%d' % c for c in sorted(band)) or '-'
        out.append('route %s%d | %d %d | land %s | band %s | start %s %s | goal %s %s | %d | %s | learned %d' % (
            profile[0:2], len(out), w, h, land, bandtxt, fmt_f(start[0]), fmt_f(start[1]),
            fmt_f(goal[0]), fmt_f(goal[1]), r['status'], runs, learned))
    return out


def main():
    cmd = sys.argv[1]
    count, seed = int(sys.argv[2]), int(sys.argv[3])
    if cmd == 'packer':
        lines = cmd_packer(count, seed)
    elif cmd == 'nibble':
        lines = cmd_nibble(count, seed)
    elif cmd == 'routes':
        lines = cmd_routes(count, seed, sys.argv[4])
    else:
        sys.exit('unknown command')
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
