#!/usr/bin/env python3
"""Emulation of the ORIGINAL class-0x3000 (mine/quarry) terrain helper against tools/mine_terrain_ref.py
(MAP-EXTRACT-1 X1; evidence class E, docs/sim/salt-extraction.md).

The real bytes of AnnoGame.dll 0x10008a80 (the helper), 0x10008d90 (the terrain height sampler),
0x10015e90 (ray/triangle) and 0x10016d60 (_ftol) run under unicorn on a synthetic type object
(footprint bytes at +6/+7, the HIGHMAP table pointer at +0x234) and a synthetic vertex grid. Nothing
of the game is written; the DLL is read only. This is NOT a run of the game.

  ANNO1503_GAME_DIR=<game> mine_terrain_emulate.py trace            print the sample sequence per rotation
  ANNO1503_GAME_DIR=<game> mine_terrain_emulate.py compare <count> [seed]   reference vs original

Needs a Python with `pefile` and `unicorn` (cache/map2-founding-evidence/venv has both).
"""
import os
import random
import struct
import sys

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

BASE = 0x10000000
STACK = 0x30800000
TYPE = 0x30100000
TABLE = 0x30200000
GRID = 0x30300000
RET = 0x41000000
A80, SAMPLER, FTOL = 0x10008A80, 0x10008D90, 0x10016D60


class Original:
    def __init__(self, game_dir):
        pe = pefile.PE(os.path.join(game_dir, 'AnnoGame.dll'), fast_load=True)
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
        img = pe.get_memory_mapped_image()[:size]
        self.u = Uc(UC_ARCH_X86, UC_MODE_32)
        self.u.mem_map(BASE, size)
        self.u.mem_write(BASE, img)
        for a, n in ((STACK, 0x10000), (TYPE, 0x1000), (TABLE, 0x1000), (GRID, 0x100000), (RET, 0x1000)):
            self.u.mem_map(a, n)
        self.trace = None
        self.u.hook_add(UC_HOOK_CODE, self._hook, begin=SAMPLER, end=SAMPLER)
        self.u.hook_add(UC_HOOK_CODE, self._hook_ret, begin=0x10008B28, end=0x10008B28)

    def _hook(self, uc, addr, size, user):
        if self.trace is None:
            return
        ecx = uc.reg_read(UC_X86_REG_ECX)
        x, y, z = struct.unpack('<fff', bytes(uc.mem_read(ecx, 12)))
        self.trace.append([x, z])

    def _hook_ret(self, uc, addr, size, user):
        pass

    def helper(self, w, h, table, grid, stride, rotation, offset, trace=False):
        """0x10008a80(this=type, heights, stride, rotation, offset) -> (0|1, [(x, z)...])."""
        u = self.u
        t = bytearray(0x400)
        t[6], t[7] = w, h
        struct.pack_into('<I', t, 0x234, TABLE)
        u.mem_write(TYPE, bytes(t))
        u.mem_write(TABLE, bytes(table))
        u.mem_write(GRID, bytes(grid))
        sp = STACK + 0x8000
        for v in reversed([GRID, stride, rotation, offset]):
            sp -= 4
            u.mem_write(sp, struct.pack('<I', v & 0xFFFFFFFF))
        sp -= 4
        u.mem_write(sp, struct.pack('<I', RET))
        u.reg_write(UC_X86_REG_ESP, sp)
        u.reg_write(UC_X86_REG_ECX, TYPE)
        self.trace = [] if trace else None
        u.emu_start(A80, RET, count=400000)
        assert u.reg_read(UC_X86_REG_EIP) == RET
        return u.reg_read(UC_X86_REG_EAX) & 0xFF, self.trace

    def sample(self, grid, stride, x, z):
        """0x10008d90(vec, heights, stride, 1.0) -> float height at (x, z)."""
        u = self.u
        u.mem_write(GRID, bytes(grid))
        vec = STACK + 0x100
        u.mem_write(vec, struct.pack('<fff', x, 0.0, z))
        sp = STACK + 0x8000
        for v in reversed([GRID, stride, struct.unpack('<I', struct.pack('<f', 1.0))[0]]):
            sp -= 4
            u.mem_write(sp, struct.pack('<I', v & 0xFFFFFFFF))
        sp -= 4
        u.mem_write(sp, struct.pack('<I', RET))
        u.reg_write(UC_X86_REG_ESP, sp)
        u.reg_write(UC_X86_REG_ECX, vec)
        self.trace = None
        u.emu_start(SAMPLER, RET, count=400000)
        assert u.reg_read(UC_X86_REG_EIP) == RET
        return struct.unpack('<f', bytes(u.mem_read(vec + 4, 4)))[0]


def main():
    game = os.environ['ANNO1503_GAME_DIR']
    o = Original(game)
    if sys.argv[1] == 'trace':
        w, h = 3, 4
        table = [255] * (4 * w * h)
        grid = [0] * (16 * 16)
        for rot in range(4):
            ok, tr = o.helper(w, h, table, grid, 16, rot, 0, trace=True)
            print('rotation', rot, 'result', ok, 'samples', len(tr))
            print(' ', [(round(x, 3), round(z, 3)) for x, z in tr])
        return 0
    if sys.argv[1] == 'vectors':
        n = bad = 0
        for line in open(sys.argv[2]).read().splitlines():
            line = line.split('#', 1)[0].strip()
            if not line.startswith('terrain '):
                continue
            lhs, rhs = line.split('=>')
            kv = dict(t.split('=', 1) for t in lhs.split()[2:])
            w, h, rot, off = int(kv['w']), int(kv['h']), int(kv['rot']), int(kv['off'])
            table = [int(x) for x in kv['table'].split(',')]
            rows = [[int(x) for x in r.split(',')] for r in kv['heights'].split('/')]
            stride = len(rows[0])
            grid = [v for r in rows for v in r] + [0] * 64
            ok, _ = o.helper(w, h, table, grid, stride, rot, off)
            got = 'accept' if ok else 'refuse'
            n += 1
            if got != rhs.strip():
                bad += 1
                print('ORIGINAL DISAGREES', lhs.split()[1], 'expected', rhs.strip(), 'original', got)
        print('terrain vectors through the original bytes:', n, 'disagree', bad)
        return 0 if bad == 0 else 1
    if sys.argv[1] == 'compare':
        import mine_terrain_ref as ref
        count = int(sys.argv[2])
        rnd = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 1)
        agree = disagree = acc = 0
        stats = {}
        for i in range(count):
            w, h = rnd.randint(1, 4), rnd.randint(1, 4)
            rot = rnd.randint(0, 3)
            ww, hh = (w, h) if rot % 2 == 0 else (h, w)
            kind = rnd.randint(0, 3)
            pool = [255, 32, 16, 12, 4, 0, 1, 31, 33, 128]
            table = [rnd.choice(pool) if kind else rnd.randint(0, 255) for _ in range(4 * w * h)]
            if kind == 2:   # near-flat terrain on levels
                base = rnd.choice([0, 16, 32, 48, 64, 96, 160, 240])
                heights = [[min(255, base + rnd.choice([0, 0, 0, 1, 4, 11, 12, 16])) for _ in range(ww + 1)] for _ in range(hh + 1)]
            elif kind == 3:  # extremes
                heights = [[rnd.choice([0, 255, 254, 1]) for _ in range(ww + 1)] for _ in range(hh + 1)]
            else:
                heights = [[rnd.randint(0, 255) for _ in range(ww + 1)] for _ in range(hh + 1)]
            offset = rnd.choice([0, 16, 32, 48, 64, 96, 255, rnd.randint(0, 255), rnd.randint(0, 0xFFFF)])
            stride = ww + 1 + rnd.randint(0, 3)
            grid = [0] * (stride * (hh + 1))
            for z in range(hh + 1):
                for x in range(ww + 1):
                    grid[z * stride + x] = heights[z][x]
            ok, _ = o.helper(w, h, table, grid + [0] * 64, stride, rot, offset)
            mine = ref.accepts(w, h, table, heights, rot, offset)
            if bool(ok) == mine:
                agree += 1
                acc += ok
            else:
                disagree += 1
                if disagree <= 5:
                    print('DISAGREE', dict(w=w, h=h, rot=rot, offset=offset, stride=stride, original=ok, reference=mine))
        print('cases', count, 'agree', agree, 'accepted', acc, 'refused', agree - acc, 'DISAGREE', disagree)
        return 0 if disagree == 0 else 1
    return 2


if __name__ == '__main__':
    sys.exit(main())
