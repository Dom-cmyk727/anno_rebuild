#!/usr/bin/env python3
"""Emulation of the ORIGINAL island placement predicate against tools/placement_ref.py
(MAP-BUILD-1 package B; evidence class E of docs/re/ship-placement.md).

The real bytes of AnnoWelt.dll 0x1007c790 (island slot +0x6c, with the island's own slot +0x88
0x10083c80 and _ftol 0x100b5f90) run under unicorn on a synthetic island. Every type call it makes
(registry lookup +0x70 and type slots +0x50 +0x54 +0x5c +0x60 +0x68 +0x6c +0xd0) is routed to the
real bytes of AnnoGame.dll (0x10007d20.., 0x10008f90, 0x10008920, 0x1000a5f0) running in a second
unicorn instance on synthetic type objects. Nothing of the game is written; the DLLs are read only.
This is NOT a run of the game: the world, the registry object and the type objects are synthetic.

  ANNO1503_GAME_DIR=<game> placement_emulate.py random <count> [seed]   random islands vs the reference
  ANNO1503_GAME_DIR=<game> placement_emulate.py vectors [vectors.txt]   the fixture file through the original code

Needs a Python with `pefile` and `unicorn` (cache/map2-founding-evidence/venv has both). Exit 0 only if
every compared case agreed; cases in a domain the reference calls UNSUPPORTED are executed and counted
but not compared. Instruction budget exceeded or an emulator error is a failure (exit 2).
"""
import hashlib
import os
import random
import struct
import sys

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UcError
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import placement_ref as pr  # noqa: E402

BASE = 0x10000000
BUDGET = 400000


class Incomplete(Exception):
    pass


def load(path):
    pe = pefile.PE(path, fast_load=True)
    size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
    return pe.get_memory_mapped_image()[:size], size, hashlib.sha256(open(path, 'rb').read()).hexdigest()


class Harness:
    TOK = 0x22000000          # type tokens in the AnnoWelt instance: TOK + id * 0x40
    TVT = 0x23000000          # fake type vtable
    REG = 0x21000000
    REGVT = 0x21001000
    ISL = 0x20000000
    TILES = 0x24000000
    HEIGHTS = 0x25000000
    REC = 0x26000000
    STUB = 0x40000000
    UAOBJ = 0x30100000        # AnnoGame instance: type objects at UAOBJ + id * 0x400
    UASCR = 0x30700000
    ASTACK = 0x30800000
    SLOTS = {0x50: 0, 0x54: 0, 0x5C: 0, 0x60: 0, 0xD0: 0, 0x68: 0x10, 0x6C: 8}   # slot -> ret bytes

    def __init__(self, game_dir):
        welt, wsize, self.welt_sha = load(os.path.join(game_dir, 'AnnoWelt.dll'))
        game, gsize, self.game_sha = load(os.path.join(game_dir, 'AnnoGame.dll'))
        self.w = Uc(UC_ARCH_X86, UC_MODE_32)
        self.w.mem_map(BASE, wsize)
        self.w.mem_write(BASE, welt)
        for addr, n in ((self.ISL, 0x1000), (self.REG, 0x2000), (self.TOK, 0x100000), (self.TVT, 0x1000),
                        (self.TILES, 0x100000), (self.HEIGHTS, 0x100000), (self.REC, 0x1000),
                        (self.STUB, 0x2000), (0x2E000000, 0x100000)):
            self.w.mem_map(addr, n)
        self.a = Uc(UC_ARCH_X86, UC_MODE_32)
        self.a.mem_map(BASE, gsize)
        self.a.mem_write(BASE, game)
        for addr, n in ((self.UAOBJ, 0x600000), (self.UASCR, 0x100000), (self.ASTACK, 0x10000), (0x41000000, 0x1000)):
            self.a.mem_map(addr, n)
        self.real = {s: struct.unpack('<I', self.a.mem_read(0x1002185C + s, 4))[0] for s in self.SLOTS}
        # fake registry: vtable slot +0x70 -> stub (ret 4)
        self.w.mem_write(self.REG, struct.pack('<I', self.REGVT))
        self.w.mem_write(self.REGVT + 0x70, struct.pack('<I', self.STUB + 0x100))
        self.w.mem_write(self.STUB + 0x100, b'\xc2\x04\x00')
        for s, n in self.SLOTS.items():
            addr = self.STUB + 0x200 + s * 0x10
            self.w.mem_write(self.TVT + s, struct.pack('<I', addr))
            self.w.mem_write(addr, b'\xc3' if n == 0 else bytes([0xC2, n, 0]))
        self.w.hook_add(UC_HOOK_CODE, self._hook, begin=self.STUB, end=self.STUB + 0x1FFF)
        self.types, self.hbytes = {}, 0
        try:
            self.w.mem_map(0x41000000, 0x1000)
        except UcError:
            pass
        # The static initialisers of the module's own globals (image bss is zero until they run):
        # 0x1007b340: [0x100e2d88] = f32(0.04f * 16.0f) = 0.64f;  0x1007b3a0: [0x100e339c] = 0.64f + 0.64f = 1.28f.
        for init in (0x1007B340, 0x1007B3A0):
            self.w.reg_write(UC_X86_REG_ESP, 0x2E07F000)
            self.w.mem_write(0x2E07F000, struct.pack('<I', 0x41000000))
            self.w.emu_start(init, 0x41000000, count=100)
        self.threshold_global = struct.unpack('<f', self.w.mem_read(0x100E339C, 4))[0]

    # --- AnnoGame side -----------------------------------------------------------------
    def a_call(self, va, ecx, args):
        a = self.a
        sp = self.ASTACK + 0x8000
        for v in reversed(args):
            sp -= 4
            a.mem_write(sp, struct.pack('<I', v & 0xFFFFFFFF))
        sp -= 4
        a.mem_write(sp, struct.pack('<I', 0x41000000))
        a.reg_write(UC_X86_REG_ESP, sp)
        a.reg_write(UC_X86_REG_ECX, ecx)
        a.emu_start(va, 0x41000000, count=BUDGET)
        if a.reg_read(UC_X86_REG_EIP) != 0x41000000:
            raise Incomplete('AnnoGame function %#x did not return' % va)
        return a.reg_read(UC_X86_REG_EAX)

    def _hook(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        arg = lambda i: struct.unpack('<I', uc.mem_read(esp + 4 + 4 * i, 4))[0]
        if address == self.STUB + 0x100:                       # registry slot +0x70(type id)
            tid = arg(0)
            uc.reg_write(UC_X86_REG_EAX, self.TOK + tid * 0x40 if tid in self.types else 0)
            return
        slot = (address - self.STUB - 0x200) // 0x10
        this = uc.reg_read(UC_X86_REG_ECX)
        obj = lambda tok: self.UAOBJ + ((tok - self.TOK) // 0x40) * 0x400
        if slot == 0x6C:
            r = self.a_call(self.real[slot], obj(this), [obj(arg(0)), arg(1)])
        elif slot == 0x68:
            p, stride, rot, fl = arg(0), arg(1), arg(2), arg(3)
            off = p - self.HEIGHTS
            self.a.mem_write(self.UASCR, bytes(uc.mem_read(self.HEIGHTS, self.hbytes)))
            r = self.a_call(self.real[slot], obj(this), [self.UASCR + off, stride, rot, fl])
        else:
            r = self.a_call(self.real[slot], obj(this), [])
        uc.reg_write(UC_X86_REG_EAX, r & 0xFFFFFFFF)

    # --- AnnoWelt side -----------------------------------------------------------------
    def set_types(self, reg):
        self.types = reg
        for tid, t in reg.items():
            o = bytearray(0x400)
            struct.pack_into('<I', o, 0, 0x1002185C)
            o[6], o[7] = t.w, t.h
            struct.pack_into('<H', o, 0x10, t.prodtype)
            struct.pack_into('<H', o, 0x12, t.flags)
            struct.pack_into('<H', o, 0x14, 0x100 if t.replaceable else 0)
            self.a.mem_write(self.UAOBJ + tid * 0x400, bytes(o))
            self.w.mem_write(self.TOK + tid * 0x40, struct.pack('<I', self.TVT))

    def run(self, isl_params, vertex_rows, objs, tid, x, y, rot, flags, rec8):
        """isl_params (x0, y0, W, H); vertex_rows: list of H+2 rows (bytes, W+1 each); objs {(x, y): id}."""
        x0, y0, W, H = isl_params
        w = self.w
        isl = bytearray(0x40)
        struct.pack_into('<I', isl, 0, 0x100CAC2C)
        struct.pack_into('<HHHH', isl, 4, x0, y0, W, H)
        struct.pack_into('<III', isl, 0xC, self.TILES, 0, self.HEIGHTS)
        struct.pack_into('<I', isl, 0x1C, self.REG)
        w.mem_write(self.ISL, bytes(isl))
        tiles = bytearray(W * H * 8)
        for (tx, ty), o in objs.items():
            if x0 <= tx < x0 + W and y0 <= ty < y0 + H:
                struct.pack_into('<H', tiles, ((ty - y0) * W + (tx - x0)) * 8, o)
        w.mem_write(self.TILES, bytes(tiles))
        hb = b''.join(vertex_rows)
        self.hbytes = len(hb)
        w.mem_write(self.HEIGHTS, hb)
        w.mem_write(self.REC, struct.pack('<HHHBxB', tid, x, y, rot, rec8) + b'\0')
        sp = 0x2E080000
        for v in (0, flags, self.REC):
            sp -= 4
            w.mem_write(sp, struct.pack('<I', v))
        sp -= 4
        w.mem_write(sp, struct.pack('<I', 0x41000000))
        w.reg_write(UC_X86_REG_ESP, sp)
        w.reg_write(UC_X86_REG_ECX, self.ISL)
        w.emu_start(0x1007C790, 0x41000000, count=BUDGET)
        if w.reg_read(UC_X86_REG_EIP) != 0x41000000:
            raise Incomplete('0x1007c790 did not return')
        return w.reg_read(UC_X86_REG_EAX) & 0xFF, w.mem_read(self.REC + 8, 1)[0]


def vertex_rows_for(isl_params, vertex):
    x0, y0, W, H = isl_params
    return [bytes(vertex(x0 + c, y0 + r) for c in range(W + 1)) for r in range(H + 2)]


def compare(h, reg, isl_params, vertex, objs, tid, x, y, rot, flags, rec8, stats, bad):
    x0, y0, W, H = isl_params
    ref_isl = pr.Island(x0, y0, W, H, vertex, lambda tx, ty: objs.get((tx, ty), 0))
    verdict, reason, rb = pr.place(ref_isl, reg, tid, x, y, rot, flags, rec8)
    try:
        got, gb = h.run(isl_params, vertex_rows_for(isl_params, vertex), objs, tid, x, y, rot, flags, rec8)
    except UcError:
        if verdict == pr.UNSUPPORTED and reg[tid].flags & 0x7000 == 0x3000:
            stats['not runnable (class 0x3000 reads the type HIGHMAP table at +0x234)'] = \
                stats.get('not runnable (class 0x3000 reads the type HIGHMAP table at +0x234)', 0) + 1
            return
        raise
    stats['executed'] += 1
    if verdict == pr.UNSUPPORTED:
        stats['unsupported'] += 1
        return
    stats['compared'] += 1
    want = 1 if verdict == pr.ACCEPT else 0
    stats['accept' if want else 'reject'] += 1
    stats['reason ' + (reason or 'ACCEPT')] = stats.get('reason ' + (reason or 'ACCEPT'), 0) + 1
    if got != want or (want and gb != rb):
        stats['mismatch'] += 1
        if len(bad) < 5:
            bad.append((tid, x, y, rot, hex(flags), isl_params, 'ref', verdict, reason, rb, 'orig', got, gb))


def registry():
    T = pr.Type
    return {
        1: T(2, 2, 97, 0x84), 2: T(3, 3, 1, 0x84, True), 3: T(4, 3, 34, 0x84, True), 4: T(4, 4, 35, 0x2087),
        5: T(1, 3, 35, 0x2087), 6: T(1, 1, 257, 0x1886), 7: T(1, 1, 193, 0x4386), 8: T(1, 1, 226, 0x29),
        9: T(1, 1, 321, 0x8018), 10: T(1, 1, 385, 0x26), 11: T(2, 2, 2, 0x84), 12: T(2, 2, 97, 0x94),
        13: T(1, 2, 129, 0x84), 14: T(3, 1, 4, 0x84), 15: T(2, 3, 194, 0x4186), 16: T(2, 2, 289, 0x84),
        17: T(5, 2, 66, 0x84), 18: T(3, 3, 3, 0x3096), 19: T(1, 1, 323, 0x3A), 20: T(2, 2, 6, 0x2087),
        21: T(1, 1, 225, 0x29), 22: T(2, 2, 1, 0x84), 23: T(2, 2, 7, 0x84, True)}


def random_cases(h, reg, count, seed):
    rng = random.Random(seed)
    stats = dict(executed=0, compared=0, unsupported=0, accept=0, reject=0, mismatch=0)
    bad = []
    ids = list(reg)
    levels = [0, 0x10, 0x1F, 0x20, 0x2F, 0x30, 0x30, 0x31, 0x3F, 0x40, 0x50, 0x60]
    for _ in range(count):
        W, H = rng.randint(2, 12), rng.randint(2, 12)
        x0, y0 = rng.randint(0, 6), rng.randint(0, 6)
        base = rng.choice(levels)
        kind = rng.random()
        grid = {}
        for vy in range(y0, y0 + H + 2):
            for vx in range(x0, x0 + W + 1):
                if kind < 0.55:
                    v = base
                elif kind < 0.8:
                    v = base if rng.random() > 0.08 else rng.choice(levels)
                else:
                    v = rng.choice(levels)
                grid[(vx, vy)] = v
        if rng.random() < 0.6:                       # the runtime boundary (column W, rows H and H + 1) is 0
            for (vx, vy) in grid:
                if vx == x0 + W or vy >= y0 + H:
                    grid[(vx, vy)] = 0
        tid = rng.choice(ids)
        if reg[tid].flags & 0x7000 == 0x2000 and rng.random() < 0.7:     # coast-like rim: shore row 0x30, water opposite
            for (vx, vy) in grid:
                grid[(vx, vy)] = 0x30 if rng.random() < 0.6 else rng.choice([0x10, 0x1F, 0x30])
        vertex = lambda vx, vy, g=grid: g.get((vx, vy), 0)
        objs = {}
        if rng.random() < 0.5:
            for _ in range(rng.randint(1, 4)):
                objs[(x0 + rng.randrange(W), y0 + rng.randrange(H))] = rng.choice(ids)
        x = rng.randint(max(0, x0 - 1), x0 + W)
        y = rng.randint(max(0, y0 - 1), y0 + H)
        if rng.random() < 0.5:
            fw = reg[tid].w
            x = rng.randint(x0, max(x0, x0 + W - fw))
            y = rng.randint(y0, max(y0, y0 + H - reg[tid].h))
        if rng.random() < 0.7:
            for (vx, vy) in list(grid):              # make the footprint's own window flat more often
                pass
        flags = rng.choice([0x2000000, 0x8000000, 0x2000000, 0x1000000, 0x1000000 | 0x2000000,
                            0x400000 | 0x2000000, 0x10000000 | 0x2000000, 0x2000010, 0, 0x8000000])
        compare(h, reg, (x0, y0, W, H), vertex, objs, tid, x, y, rng.randrange(4), flags,
                rng.choice([0, 0x1F, 0x20, 0x21, 0x30, 0xFF]), stats, bad)
    return stats, bad


def flatten_footprints(rng, reg, grid, x, y, tid, rot):
    t = reg[tid]
    fw, fh = (t.w, t.h) if rot % 2 == 0 else (t.h, t.w)
    lvl = rng.choice([0x20, 0x30, 0x40, 0x31, 0x1F])
    for dy in range(fh + 1):
        for dx in range(fw + 1):
            if (x + dx, y + dy) in grid:
                grid[(x + dx, y + dy)] = lvl


def random_cases_flat(h, reg, count, seed):
    """Same generator, but the footprint's window is flattened first so that accepts are common."""
    rng = random.Random(seed)
    stats = dict(executed=0, compared=0, unsupported=0, accept=0, reject=0, mismatch=0)
    bad = []
    ids = [i for i in reg if reg[i].flags & 0x7000 != 0x2000]
    for _ in range(count):
        W, H = rng.randint(3, 12), rng.randint(3, 12)
        x0, y0 = rng.randint(0, 4), rng.randint(0, 4)
        grid = {(vx, vy): rng.choice([0x20, 0x30, 0x40]) for vy in range(y0, y0 + H + 2) for vx in range(x0, x0 + W + 1)}
        for (vx, vy) in grid:
            if vx == x0 + W or vy >= y0 + H:
                grid[(vx, vy)] = 0
        tid, rot = rng.choice(ids), rng.randrange(4)
        t = reg[tid]
        fw, fh = (t.w, t.h) if rot % 2 == 0 else (t.h, t.w)
        x = rng.randint(x0, max(x0, x0 + W - fw))
        y = rng.randint(y0, max(y0, y0 + H - fh))
        flatten_footprints(rng, reg, grid, x, y, tid, rot)
        if rng.random() < 0.4:                       # one spoiled vertex inside the window
            grid[(x + rng.randint(0, fw), y + rng.randint(0, fh))] = rng.choice([0x1F, 0x21, 0x2F, 0x41, 0x50])
        objs = {}
        if rng.random() < 0.4:
            for _ in range(rng.randint(1, 3)):
                objs[(x + rng.randrange(max(fw, 1)), y + rng.randrange(max(fh, 1)))] = rng.choice(list(reg))
        vertex = lambda vx, vy, g=grid: g.get((vx, vy), 0)
        compare(h, reg, (x0, y0, W, H), vertex, objs, tid, x, y, rot,
                rng.choice([0x2000000, 0x2000000, 0x8000000, 0x1000000, 0x400000]), rng.choice([0x1F, 0x20, 0x30]), stats, bad)
    return stats, bad


def run_vectors(h, path):
    reg, cases = pr.parse_vectors(open(path).read())
    h.set_types(reg)
    stats = dict(executed=0, compared=0, unsupported=0, accept=0, reject=0, mismatch=0)
    bad = []
    for c in cases:
        isl = case_params(c)
        x0, y0, W, H = isl
        tid, x, y, rot, flags, rec8 = c['place']
        rows = [c['rows'].get(r, bytes(W + 1)).ljust(W + 1, b'\0') for r in range(H + 2)]
        verdict, reason, rb = pr.run_case(reg, c)
        try:
            got, gb = h.run(isl, rows, c['objs'], tid, x, y, rot, flags, rec8)
        except UcError as e:
            if verdict != pr.UNSUPPORTED:
                raise
            stats['unsupported'] += 1
            print('  %-34s reference: unsupported (the original faulted: %s)' % (c['name'], e))
            continue
        stats['executed'] += 1
        if verdict == pr.UNSUPPORTED:
            stats['unsupported'] += 1
            print('  %-34s reference: unsupported (original returned %d, byte %d)' % (c['name'], got, gb))
            continue
        stats['compared'] += 1
        want = 1 if verdict == pr.ACCEPT else 0
        ok = got == want and (not want or gb == rb)
        if not ok:
            stats['mismatch'] += 1
        print('  %-34s original %d byte %-3d reference %s %s %d  %s' % (c['name'], got, gb, verdict, reason, rb, 'ok' if ok else 'MISMATCH'))
    return stats, bad


def case_params(c):
    return c['island']


def main(argv):
    gd = os.environ.get('ANNO1503_GAME_DIR')
    if not gd or len(argv) < 2:
        print(__doc__)
        return 2
    h = Harness(gd)
    print('AnnoWelt.dll sha256 %s' % h.welt_sha)
    print('AnnoGame.dll sha256 %s' % h.game_sha)
    print('static globals after 0x1007b340 / 0x1007b3a0: [0x100e339c] = %r (f32)' % h.threshold_global)
    try:
        if argv[1] == 'random':
            reg = registry()
            h.set_types(reg)
            n = int(argv[2]) if len(argv) > 2 else 5000
            seed = int(argv[3]) if len(argv) > 3 else 11
            s1, b1 = random_cases(h, reg, n, seed)
            s2, b2 = random_cases_flat(h, reg, n, seed + 1)
            for name, s, b in (('mixed', s1, b1), ('flattened footprint', s2, b2)):
                print('%s: seed %d' % (name, seed if name == 'mixed' else seed + 1))
                print('  ' + ', '.join('%s %d' % kv for kv in sorted(s.items())))
                for rec in b:
                    print('  MISMATCH', rec)
            bad = s1['mismatch'] + s2['mismatch']
        else:
            path = argv[2] if len(argv) > 2 else pr.DEFAULT_VECTORS
            s, b = run_vectors(h, path)
            print('vectors: ' + ', '.join('%s %d' % kv for kv in sorted(s.items())))
            bad = s['mismatch']
    except (Incomplete, UcError) as e:
        print('INCOMPLETE/EMULATOR ERROR: %s' % e)
        return 2
    print('VERDICT: %s' % ('PASS' if bad == 0 else 'FAIL (%d mismatches)' % bad))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
