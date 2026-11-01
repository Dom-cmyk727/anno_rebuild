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
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from unicorn import UC_HOOK_CODE

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


def tile_rule(x, z):
    """Shared tile truth of the builder fixtures for MIXED covered cells (the C++ test uses the same formula)."""
    return ((x + 2 * z + (x >> 3) * 5 + (z >> 3) * 3) % 7) < 5


def cmd_builder(count, seed):
    """The REAL coarse builder 0x100aae00 (pass 1 block shortcut + mask packer, pass 2 shore band) under unicorn on
    a faked world: sector table, cell->island table, island origin slots +0x50/+0x54, block records (+0x94, columns
    +0x8c). Prints the island block records and every cell word of the allocated sectors."""
    from eng import Eng, WORLD, VT, STUBS
    rng = random.Random(seed)
    out = []
    ISL_VT = 0x303000
    while len(out) < count:
        W = rng.choice([128, 192, 128, 256])
        H = rng.choice([128, 192, 128])
        cw, ch = W // 8, H // 8
        # non-overlapping aligned island rectangles in cell units
        occ = [[None] * cw for _ in range(ch)]
        islands = []
        for _ in range(rng.randrange(1, 5)):
            bw, bh = rng.randrange(2, 9), rng.randrange(2, 9)
            for _try in range(30):
                cx0, cz0 = rng.randrange(0, cw - bw + 1), rng.randrange(0, ch - bh + 1)
                if all(occ[z][x] is None for z in range(cz0, cz0 + bh) for x in range(cx0, cx0 + bw)):
                    idx = len(islands)
                    for z in range(cz0, cz0 + bh):
                        for x in range(cx0, cx0 + bw):
                            occ[z][x] = idx
                    recs = []
                    for _b in range(bw * bh):
                        r = rng.random()
                        k = 's' if r < 0.03 else 'l2' if r < 0.05 else rng.choice(['w', 'w', 'l', 'l', 'm'])
                        if k == 'w':      # all water: 19 >= 2 max
                            mn = rng.randrange(0, 10); mx = rng.randrange(mn, 10)
                        elif k == 'l':    # all land, no straddle: min >= 16
                            mn = rng.randrange(16, 40); mx = rng.randrange(mn, 60)
                        elif k == 'l2':   # all land that straddles 32 (10 <= min < 16, max >= 17)
                            mn = rng.randrange(10, 16); mx = rng.randrange(17, 60)
                        elif k == 's':    # mixed, straddles
                            mn = rng.randrange(0, 16); mx = rng.randrange(max(mn, 17), 60)
                        else:             # mixed, no straddle: max <= 16
                            mn = rng.randrange(0, 10); mx = rng.randrange(10, 17)
                        recs.append((mn, mx))
                    islands.append((cx0 * 8, cz0 * 8, bw, bh, recs))
                    break
        if not islands:
            continue
        rows = [''.join('.' if tile_rule(x, z) else '#' for x in range(W)) for z in range(H)]
        e = Eng(rows).setup()
        mu = e.mu
        import struct as st
        e.Wd, e.H = W, H
        # runtime statics of the builder, normally written by the initialisers 0x100a3b40..: 0.04 * 16, 2 * that,
        # 1.28 - 0.5, 1.28 - 0.35 (f32 arithmetic)
        def f32(v):
            return st.unpack('<f', st.pack('<f', v))[0]
        sea = f32(f32(0.04) * 16.0)
        shore = f32(sea * 2.0)
        mu.mem_write(0x100e3a2c, st.pack('<f', sea))
        mu.mem_write(0x100e3a3c, st.pack('<f', shore))
        mu.mem_write(0x100e3a34, st.pack('<f', f32(shore - 0.5)))
        mu.mem_write(0x100e3a38, st.pack('<f', f32(shore - 0.35)))
        mu.mem_write(WORLD + 0xec, st.pack('<II', W, H))
        spr, spc = (W + 0x7f) >> 6, (H + 0x7f) >> 6
        mu.mem_write(WORLD + 0x6a9c, st.pack('<II', spr, spc))
        arr = e.alloc(8 * spr * spc)
        for r in range(spc):
            for c in range(spr):
                mu.mem_write(arr + 8 * (r * spr + c), st.pack('<HHI', c * 64, r * 64, 0))
        mu.mem_write(WORLD + 0x6aa4, st.pack('<I', arr))
        table = bytearray([0xff]) * (cw * ch)
        ISL50, ISL54 = STUBS + 0x8050, STUBS + 0x8054
        mu.mem_write(ISL_VT + 0x50, st.pack('<I', ISL50))
        mu.mem_write(ISL_VT + 0x54, st.pack('<I', ISL54))
        ptrs = []
        for i, (x0, z0, bw, bh, recs) in enumerate(islands):
            o = e.alloc(0x100)
            rec = e.alloc(16 * bw * bh)
            for b, (mn, mx) in enumerate(recs):
                mu.mem_write(rec + 16 * b, bytes([0, 0, mn, mx]) + b'\0' * 12)
            mu.mem_write(o, st.pack('<I', ISL_VT))
            mu.mem_write(o + 0x8c, st.pack('<I', bw))
            mu.mem_write(o + 0x94, st.pack('<I', rec))
            mu.mem_write(WORLD + 0x6530 + 4 * i, st.pack('<I', o))
            ptrs.append(o)
            for z in range(bh):
                for x in range(bw):
                    table[(z0 // 8 + z) * cw + x0 // 8 + x] = i
        mu.mem_write(WORLD + 0x130, bytes(table))

        def hook(uc, addr, size, ud):
            if addr in (ISL50, ISL54):
                ecx = uc.reg_read(UC_X86_REG_ECX)
                i = ptrs.index(ecx)
                v = islands[i][0] if addr == ISL50 else islands[i][1]
                esp = uc.reg_read(UC_X86_REG_ESP)
                r = st.unpack('<I', uc.mem_read(esp, 4))[0]
                uc.reg_write(UC_X86_REG_EAX, v)
                uc.reg_write(UC_X86_REG_EIP, r)
                uc.reg_write(UC_X86_REG_ESP, esp + 4)
        mu.mem_write(ISL50, b'\xcc'); mu.mem_write(ISL54, b'\xcc')
        mu.hook_add(UC_HOOK_CODE, hook)
        rect = e.alloc(16)
        mu.mem_write(rect, st.pack('<4i', 0, 0, W, H))
        e.call(0x100aae00, (rect,), ecx=WORLD)
        cells = {}
        for r in range(spc):
            for c in range(spr):
                ptr = st.unpack('<I', mu.mem_read(arr + 8 * (r * spr + c) + 4, 4))[0]
                if ptr == 0:
                    continue
                words = st.unpack('<64I', mu.mem_read(ptr, 256))
                for k, w in enumerate(words):
                    cx, cz = c * 8 + k % 8, r * 8 + k // 8
                    if cx < cw and cz < ch and w != 0xfff:
                        cells[(cx, cz)] = w
        n = len(out)
        isl = ' ; '.join('island %d %d %d %d %s' % (x0, z0, bw, bh, ' '.join('%02x%02x' % (mn, mx) for mn, mx in recs))
                         for (x0, z0, bw, bh, recs) in islands)
        cw_txt = ' '.join('%d,%d,%x' % (cx, cz, w) for (cx, cz), w in sorted(cells.items(), key=lambda kv: (kv[0][1], kv[0][0])))
        out.append('builder b%d | %d %d | %s | words %s' % (n, W, H, isl, cw_txt or '-'))
    return out


def fmt_q8(k):
    """k / 256 as an exact decimal (at most 8 fractional digits)."""
    t = ('%.8f' % (k / 256.0)).rstrip('0').rstrip('.')
    return t


COST_PATCH = None


def run_engine(rows, start, goal):
    from eng import Eng
    e = Eng(rows).setup()
    if COST_PATCH:
        # class 0, type 0: the coarse cost bytes at 0x100d1140 + 0x400 + type * 8 + k (k even orthogonal, odd diagonal)
        orth, diag = COST_PATCH
        e.mu.mem_write(0x100d1140 + 0x400, bytes([orth, diag] * 4))
    r = e.full(start, goal)
    best = e.u32(e.W + 4 + 0x1044)
    learned = e.u32(e.W + 0x1f74)
    return r, best, learned


def cmd_scores(count, seed):
    """Goals at arbitrary Q24.8 positions: pins the score truncation (2304 - ftol(64 d^2)) and the best score."""
    rng = random.Random(seed)
    out = []
    while len(out) < count:
        w, h = 64, 64
        rects = []
        for _ in range(rng.randrange(0, 4)):
            x0, z0 = rng.randrange(8, 50), rng.randrange(8, 50)
            rects.append((x0, z0, x0 + rng.randrange(0, 20), z0 + rng.randrange(0, 20)))
        rows = rows_from_rects(w, h, rects)
        sx, sz = rng.randrange(2, 62), rng.randrange(2, 62)
        if rows[sz][sx] != '.':
            continue
        sk = (rng.randrange(256), rng.randrange(256))
        gx_k, gz_k = rng.randrange(4 * 256, 60 * 256), rng.randrange(4 * 256, 60 * 256)
        # d^2 * 64 not a multiple of 1024 for almost every draw
        start = (sx + sk[0] / 256.0, sz + sk[1] / 256.0)
        goal = (gx_k / 256.0, gz_k / 256.0)
        r, best, learned = run_engine(rows, start, goal)
        runs = ' '.join('%s%d' % (DIRS[d], n) for d, n in r['runs']) if r['runs'] else '-'
        end = '%d %d' % (int(r['cand'][0]), int(r['cand'][1])) if r['cand'] else '- -'
        land = ' '.join('%d,%d,%d,%d' % t for t in rects) or '-'
        out.append('score sc%d | %d %d | land %s | start %s %s | goal %s %s | %d | %s | end %s | best %d' % (
            len(out), w, h, land, fmt_q8(int(sk[0] + sx * 256)), fmt_q8(int(sk[1] + sz * 256)),
            fmt_q8(gx_k), fmt_q8(gz_k), r['status'], runs, end, best))
    return out


def cmd_ties(count, seed):
    """Mirror-symmetric obstacles about the axis through the start and a goal on that axis: equal-cost
    mirrored detours, decided only by the discovery order / FIFO ties of the original."""
    rng = random.Random(seed)
    out = []
    while len(out) < count:
        w = h = rng.choice([64, 64, 128])
        sx = rng.randrange(w // 4, 3 * w // 4)
        sz = rng.randrange(h // 2, 3 * h // 4)
        a = rng.randrange(1, 10 if w == 64 else 22)
        d = rng.randrange(0, 6)
        t = rng.randrange(1, 4 if w == 64 else 8)
        g = rng.randrange(2, 12)
        rects = [(sx - a, sz - d - t, sx + a, sz - d - 1)]
        for _ in range(rng.randrange(0, 5)):          # symmetric scatter
            x = rng.randrange(max(0, sx - 20), sx)
            z = rng.randrange(max(0, sz - 30), sz + 6)
            rects.append((x, z, x, z))
            rects.append((2 * sx - x, z, 2 * sx - x, z))
        if rng.random() < 0.5:                         # symmetric side pillars
            rects.append((sx - a - 3, sz - d - t - 4, sx - a - 2, sz - d - t + 2))
            rects.append((sx + a + 2, sz - d - t - 4, sx + a + 3, sz - d - t + 2))
        gx, gz = sx, sz - d - t - g
        if gz < 2:
            continue
        start = (sx + 0.5, sz + 0.5)
        goal = (gx + 0.5, gz + 0.5)
        o = rng.randrange(4)

        def tf(x, z):
            if o == 0:
                return x, z
            if o == 1:
                return x, h - 1 - z
            if o == 2:
                return z, x
            return h - 1 - z, x
        tr = []
        for (x0, z0, x1, z1) in rects:
            ax, az = tf(x0, z0)
            bx, bz = tf(x1, z1)
            tr.append((min(ax, bx), min(az, bz), max(ax, bx), max(az, bz)))
        s2 = tf(sx, sz)
        g2 = tf(gx, gz)
        rows = rows_from_rects(w, h, tr)
        if rows[s2[1]][s2[0]] != '.' or rows[g2[1]][g2[0]] != '.':
            continue
        r, best, learned = run_engine(rows, (s2[0] + 0.5, s2[1] + 0.5), (g2[0] + 0.5, g2[1] + 0.5))
        runs = ' '.join('%s%d' % (DIRS[dd], n) for dd, n in r['runs']) if r['runs'] else '-'
        land = ' '.join('%d,%d,%d,%d' % t4 for t4 in tr) or '-'
        out.append('route ti%d | %d %d | land %s | band - | start %s %s | goal %s %s | %d | %s | learned %d' % (
            len(out), w, h, land, fmt_f(s2[0] + 0.5), fmt_f(s2[1] + 0.5), fmt_f(g2[0] + 0.5), fmt_f(g2[1] + 0.5),
            r['status'], runs, learned))
    return out


def main():
    cmd = sys.argv[1]
    count, seed = int(sys.argv[2]), int(sys.argv[3])
    if cmd == 'packer':
        lines = cmd_packer(count, seed)
    elif cmd == 'nibble':
        lines = cmd_nibble(count, seed)
    elif cmd == 'scores':
        lines = cmd_scores(count, seed)
    elif cmd == 'ties':
        global COST_PATCH
        if len(sys.argv) > 5:
            COST_PATCH = (int(sys.argv[4]), int(sys.argv[5]))
        lines = cmd_ties(count, seed)
    elif cmd == 'builder':
        lines = cmd_builder(count, seed)
    elif cmd == 'routes':
        lines = cmd_routes(count, seed, sys.argv[4])
    else:
        sys.exit('unknown command')
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
