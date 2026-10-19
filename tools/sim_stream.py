#!/usr/bin/env python3
"""Independent transcription of the simulation's state stream (docs/sim/state.md
§9.3, stage S3): the registry digest of the empty registry and the state hash of
the empty world, from the spec and from the goods names in
src/sim/src/goods_table.cpp, without any C++ code.

  sim_stream.py            print the values
  sim_stream.py --check    compare them with fixtures/synthetic/sim/goldens.txt
                           (g0_step1, g0_step100, g0_step1000); exit 1 on a
                           difference

The class table below is Appendix C (compare constants of the class ticks read
from AnnoWelt.dll, see docs/re/production.md §10); the stream layout is the one
config_test.cpp transcribes byte by byte. A change of the stream (a field of
SimConfig, Rules, WorldSettings, a seam, the class table) changes both this file
and the C++ code, and goldens.txt is re-baselined with the values printed here.
"""
import argparse
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MASK = (1 << 64) - 1
STATE_HASH_VERSION = 101
PRNG_TIMELINE_VERSION = 1


def fnv(data, h=0xCBF29CE484222325):
    for b in data:
        h ^= b
        h = (h * 0x100000001B3) & MASK
    return h


def u8(x): return struct.pack('<B', x)
def u16(x): return struct.pack('<H', x)
def u32(x): return struct.pack('<I', x)
def u64(x): return struct.pack('<Q', x)
def i64(x): return struct.pack('<q', x)
def string(s): return u64(len(s)) + s
def u32s(items): return u64(len(items)) + b''.join(u32(x) for x in items)


def goods_names():
    """The 143 goods names of goods_table.cpp as bytes (\\xNN escapes decoded)."""
    src = (ROOT / 'src/sim/src/goods_table.cpp').read_text(encoding='utf-8')
    body = src[src.index('kGoodsNames = {') + len('kGoodsNames = {'):src.index('"ALL"};') + 5]
    names = []
    for m in re.finditer(r'"((?:[^"\\]|\\x[0-9A-Fa-f]{2})*)"', body):
        raw, out, i = m.group(1), bytearray(), 0
        while i < len(raw):
            if raw[i] == '\\':
                out.append(int(raw[i + 2:i + 4], 16))
                i += 4
            else:
                out.append(ord(raw[i]))
                i += 1
        names.append(bytes(out))
    assert len(names) == 143, len(names)
    return names


# goods with a research requirement (BGruppen [WAREN] Forsch, Appendix A)
FORSCH = {46: 1, 47: 4, 48: 9, 49: 12, 50: 1, 51: 3, 52: 6, 53: 38}

# (class, tick, payload, gate period), one row per RuntimeClass in enum order
CLASSES = [
    (0, 0, 0, 0),      # Plain
    (1, 1, 1, 500),    # Workshop
    (2, 0, 0, 500),    # DualWorkshop
    (3, 2, 2, 500),    # WeaponsWorkshop
    (4, 3, 3, 500),    # Farm
    (5, 4, 3, 500),    # FurTrapper
    (6, 5, 5, 500),    # Mine
    (7, 6, 4, 500),    # SmallFarm
    (8, 0, 0, 0),      # Quarry (stub 0x10001030)
    (9, 0, 0, 500),    # Hunter 0x10069f5d
    (10, 0, 0, 500),   # Whaler 0x10073737
    (11, 0, 0, 500),   # Shipyard 0x10074ea1
    (12, 0, 0, 500),   # Cannonworks
    (13, 0, 0, 500),   # Market
    (14, 0, 0, 500),   # MarketCombi
    (15, 0, 0, 5000),  # MarketStall
    (16, 0, 0, 5000),  # Inn
    (17, 0, 0, 500),   # PublicService 0x1006b4ec
    (18, 0, 0, 500),   # Doctor
    (19, 0, 0, 500),   # School (0x1006b4e0, shared)
    (20, 0, 0, 0),     # Castle (no stamp gate)
    (21, 0, 0, 500),   # FireBrigade
    (22, 0, 0, 2500),  # House
    (23, 0, 0, 166),   # Tower
    (24, 0, 0, 166),   # CannonTower 0x1006ab6a
]


def registry_digest(names):
    d = u64(0)  # no types
    d += u64(len(names))
    for i, n in enumerate(names):
        d += string(n) + u32(FORSCH.get(i, 0))
    d += u64(len(CLASSES))
    for cls, tick, payload, gate in CLASSES:
        d += u8(cls) + u8(tick) + u8(payload) + u32(gate)
    return fnv(d)


def config_bytes():
    b = b''.join(u32(x) for x in [256, 16, 15, 32, 31, 5, 28, 20, 32, 5, 65, 56, 0x29, 5, 2, 255,
                                  8, 1, 10, 8])
    b += b''.join(u32(x) for x in [500, 50, 50, 250, 50, 5000, 2000, 15000, 7500, 300, 20, 450000,
                                   10, 30000])
    b += b''.join(u32(x) for x in [2000, 15, 104, 105])
    b += i64(-3000) + u32(150000)
    b += b''.join(u32(x) for x in [0x32, 1000, 120, 50, 8, 4])
    b += u32s([0x18000, 0xF009, 0xF949, 0xF06B, 0xF6B9, 0xF04D, 0xF04D])
    b += u8(0) + u32(0)  # multiplayer, host_player
    b += u64(8) + b''.join(u16(x) for x in [107, 110, 107, 109, 107, 109, 107, 109])
    b += u16(106)
    b += u64(8) + bytes([1, 2, 0, 1, 1, 0, 0, 32])
    b += u64(6)
    for name, mask in [(b'stall', 0x20), (b'public', 1), (b'military', 0x10), (b'house', 4),
                       (b'production', 8), (b'market', 2)]:
        b += string(name) + u32(mask)
    b += u32s([5, 1, 4, 3, 2]) + u32s([4, 1, 5, 3, 0, 2]) + u32s([1, 3, 4, 2, 0, 5])
    b += u32s([4, 5, 1, 3, 2, 0])
    b += u32(1) + u32(32) + u32(4)
    b += b''.join(u32(x) for x in [5, 3, 1, 2, 4, 0])
    return b


def rules_bytes():
    return (u8(0) + u8(0) + u8(0) + u64(1 << 20) + bytes(6) + u8(0) + u8(0) + u32(0) + u8(0)
            + u8(0))


def settings_bytes():
    return (u32(0) + u32(0) + u8(0) + u8(1) + u8(2) + u16(2550) + u8(1) + u32(0) + u32(0) + u8(0)
            + u8(0) + u32(0) + u8(0) + u8(0) + u32(3) + u32(100))


def rng_table_digest():
    x, values = 1, []
    for _ in range(2048):
        x = (x * 214013 + 2531011) & 0xFFFFFFFF
        values.append((x >> 16) & 0x7FFF)
    return fnv(b''.join(u16(v) for v in values))


def state_hash(registry, cursor, ms, last_tick):
    header = u32(STATE_HASH_VERSION) + u32(PRNG_TIMELINE_VERSION) + config_bytes() + rules_bytes()
    header += u64(registry) + u64(22) + b''.join(string(b'default') for _ in range(22))
    clock_rng = u64(ms) + u64(last_tick) + u64(2048) + u64(rng_table_digest()) + u32(cursor)
    return fnv(u64(fnv(header)) + u64(fnv(clock_rng)) + u64(fnv(settings_bytes())))


def values():
    registry = registry_digest(goods_names())
    out = {'empty_registry': registry, 'empty_world': state_hash(registry, 0, 0, 0)}
    for steps in (1, 100, 1000):  # G0: seed 1234, advance(10) x steps
        out['g0_step%d' % steps] = state_hash(registry, 1234, 10 * steps, 10 * steps // 2)
    return out


def goldens():
    found = {}
    path = ROOT / 'fixtures/synthetic/sim/goldens.txt'
    for line in path.read_text(encoding='utf-8').splitlines():
        parts = line.split('#')[0].split()
        if len(parts) == 4:
            found[parts[0]] = (int(parts[2]), int(parts[3], 16))
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--check', action='store_true', help='compare with goldens.txt')
    args = ap.parse_args()
    computed = values()
    for name, value in computed.items():
        print('%-16s %016x' % (name, value))
    if not args.check:
        return 0
    bad = 0
    for name, (version, value) in goldens().items():
        if name not in computed:
            continue
        if version != STATE_HASH_VERSION:
            print('%s: golden is for stream version %d, this script for %d' %
                  (name, version, STATE_HASH_VERSION))
            bad += 1
        elif value != computed[name]:
            print('%s: goldens.txt has %016x, the transcription %016x' %
                  (name, value, computed[name]))
            bad += 1
    print('goldens agree' if bad == 0 else '%d difference(s)' % bad)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
