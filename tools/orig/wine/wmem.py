#!/usr/bin/env python3
"""Spike: read the original's memory under Wine (root reads /proc/<pid>/mem directly).
SUPERSEDED by agent.py and tools/orig/gamestate.py (T16, docs/testing/original-wine.md
section 4); kept for reference only.


  wmem.py info                 pid, module bases (from /proc/<pid>/maps or a PE-header scan)
  wmem.py world [--seconds N]  locate the world object (vtable AnnoWelt+0xccdd8), sample the clock
  wmem.py prng                 compare the 2048-entry table at AnnoWelt+0xcb8f8 with MSVC rand() seed 1
  wmem.py city                 city objects (vtable AnnoWelt+0xc2f40): capacity, tools/wood stock
Offsets: docs/re/tick-loop.md, docs/re/production.md 5.3 (the same RVAs the Windows harness uses).
"""
import json
import os
import re
import struct
import sys
import time

GAME = '/root/anno/game'
WELT_WORLD_VT = 0xCCDD8
WELT_CITY_VT = 0xC2F40
PRNG_RVA = 0xCB8F8


def find_pid():
    for p in os.listdir('/proc'):
        if not p.isdigit():
            continue
        try:
            cmd = open(f'/proc/{p}/cmdline', 'rb').read().replace(b'\0', b' ').decode('latin1')
        except OSError:
            continue
        if '1503startup.exe' in cmd.lower() and 'explorer' not in cmd.lower():
            return int(p)
    return None


def regions(pid):
    out = []
    for line in open(f'/proc/{pid}/maps'):
        parts = line.split(None, 5)
        lo, hi = (int(x, 16) for x in parts[0].split('-'))
        out.append((lo, hi, parts[1], parts[5].strip() if len(parts) > 5 else ''))
    return out


class Mem:
    def __init__(self, pid):
        self.f = open(f'/proc/{pid}/mem', 'rb', buffering=0)

    def read(self, addr, n):
        try:
            self.f.seek(addr)
            return self.f.read(n)
        except OSError:
            return None


def pe_identity(b):
    if not b or len(b) < 0x40 or b[:2] != b'MZ':
        return None
    e = struct.unpack_from('<I', b, 0x3C)[0]
    if e + 0x5C > len(b) or b[e:e + 4] != b'PE\0\0':
        return None
    return (struct.unpack_from('<I', b, e + 8)[0], struct.unpack_from('<I', b, e + 0x50)[0])


def module_bases(pid, mem, names=('AnnoWelt.dll', 'AnnoGame.dll', 'AnnoFrame.dll', 'AnnoScene.dll', 'AnnoDisplay.dll')):
    regs = regions(pid)
    found = {}
    for n in names:
        for lo, hi, perm, path in regs:
            if path.lower().endswith('/' + n.lower()):
                found.setdefault(n, {'base': lo, 'via': 'maps'})
                break
    # Fallback: PE header scan, matched to the file on disk (TimeDateStamp, SizeOfImage).
    want = {n: pe_identity(open(os.path.join(GAME, n), 'rb').read(0x400)) for n in names if n not in found}
    if want:
        for lo, hi, perm, path in regs:
            if 'r' not in perm or lo & 0xFFFF:
                continue
            ident = pe_identity(mem.read(lo, 0x400))
            for n, w in want.items():
                if ident and ident == w and n not in found:
                    found[n] = {'base': lo, 'via': 'pe-scan'}
    return {n: hex(v['base']) + ' (' + v['via'] + ')' for n, v in found.items()}, {n: v['base'] for n, v in found.items()}


def scan_u32(pid, mem, value, limit=64):
    needle = struct.pack('<I', value)
    hits = []
    for lo, hi, perm, path in regions(pid):
        if not perm.startswith('rw') or hi - lo > (256 << 20):
            continue
        data = mem.read(lo, hi - lo)
        if not data:
            continue
        i = data.find(needle)
        while i != -1 and len(hits) < limit:
            if (lo + i) % 4 == 0:
                hits.append(lo + i)
            i = data.find(needle, i + 1)
    return hits


def msvc_rand(n):
    x, out = 1, []
    for _ in range(n):
        x = (x * 214013 + 2531011) & 0xFFFFFFFF
        out.append((x >> 16) & 0x7FFF)
    return out


def world_sample(mem, obj):
    b = mem.read(obj + 0x100, 0x10)
    if not b:
        return None
    return {'counter': struct.unpack_from('<Q', b, 0)[0], 'at108': struct.unpack_from('<I', b, 8)[0],
            'tick': struct.unpack_from('<I', b, 12)[0], 'cursor': struct.unpack('<H', mem.read(obj + 0xFC, 2))[0]}


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'info'
    pid = find_pid()
    if not pid:
        print(json.dumps({'error': 'game not running'}))
        return 1
    mem = Mem(pid)
    shown, bases = module_bases(pid, mem)
    res = {'pid': pid, 'modules': shown}
    welt = bases.get('AnnoWelt.dll')
    if cmd == 'world' and welt:
        secs = float(sys.argv[sys.argv.index('--seconds') + 1]) if '--seconds' in sys.argv else 20
        cands = scan_u32(pid, mem, welt + WELT_WORLD_VT)
        good = [c for c in cands if (s := world_sample(mem, c)) and s['counter'] and s['tick'] == s['counter'] >> 1]
        res['world_candidates'] = [hex(c) for c in cands]
        if good:
            obj = good[0]
            res['world'] = hex(obj)
            t0 = time.time()
            samples = []
            while time.time() - t0 <= secs:
                s = world_sample(mem, obj)
                s['t'] = round(time.time() - t0, 3)
                samples.append(s)
                time.sleep(1.0)
            dt = samples[-1]['t'] - samples[0]['t']
            res['seconds'] = round(dt, 3)
            res['ms_per_s'] = round((samples[-1]['counter'] - samples[0]['counter']) / dt, 1)
            res['ticks_per_s'] = round((samples[-1]['tick'] - samples[0]['tick']) / dt, 1)
            res['tick_eq_counter_shr1_all'] = all(s['tick'] == s['counter'] >> 1 for s in samples)
            res['cursor_first_last'] = [samples[0]['cursor'], samples[-1]['cursor']]
            res['n'] = len(samples)
    elif cmd == 'prng' and welt:
        raw = mem.read(welt + PRNG_RVA, 4096)
        table = list(struct.unpack('<2048H', raw))
        exp = msvc_rand(2048)
        res['prng_mismatches'] = sum(1 for a, b in zip(table, exp) if a != b)
        res['prng_head'] = table[:6]
    elif cmd == 'city' and welt:
        cities = []
        for c in scan_u32(pid, mem, welt + WELT_CITY_VT, 64):
            cap = struct.unpack('<I', mem.read(c + 0x624, 4))[0]
            if cap == 0 or cap > (5000 << 8):
                continue
            st = {g: struct.unpack('<I', mem.read(c + 0x334 + 12 * g, 4))[0] / 256 for g in (41, 42, 43)}
            inh = struct.unpack('<i', mem.read(c + 0x2F0, 4))[0]
            cities.append({'addr': hex(c), 'capacity_t': cap / 256, 'tools_t': st[41], 'wood_t': st[42],
                           'bricks_t': st[43], 'inhabitants': inh})
        res['cities'] = cities
    print(json.dumps(res, indent=1))
    return 0


if __name__ == '__main__':
    sys.exit(main())
