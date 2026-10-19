#!/usr/bin/env python3
"""Independent transcription of the simulation's state stream (docs/sim/state.md
§9.3, stage S5): the registry digest of the empty registry and the state hash of
the empty world, from the spec and from the goods names in
src/sim/src/goods_table.cpp, without any C++ code. Since stage S4 the stream has
the players scope (32 slots, the table's stamp and cursor), whose cursor the
script advances with its own transcription of the player table tick
(min(d*32/50 + 1, 32) slots per advance). The golden G1 (g1_players_step100,
g1_players_step700) is a populated table run with the full-tick rule: the script
transcribes the per-player rules too (upkeep gate and booking, statistics decay,
bankruptcy grace period), so MSVC, GCC and Clang agree on the player arithmetic.
Since stage S5 the stream has the islands scope after players. The golden
s5_islands_create / s5_islands_step100 is the synthetic two-island scenario
(synthetic::two_island_scenario: two players, islands in slots 0 and 3, owner runs,
objects, deposits, a terrain, an island size that is not a multiple of the chunk
edge): the script builds the same tile columns and hashes them chunk by chunk, so
the clipped edge chunks, the terrain digest and the deposit order are held to an
independent reading of the spec.

  sim_stream.py            print the values
  sim_stream.py --check    compare them with fixtures/synthetic/sim/goldens.txt
                           (g0_*, g1_*, s5_*); exit 1 on a difference

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
STATE_HASH_VERSION = 103
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


def rules_bytes(full_tick=False):
    # slot_policy, spatial_order, events_drain_new, events_max_dispatch, six flags (flush,
    # monopoly_every_30s, player_table_full_tick, drought centre, atomic, empty city),
    # event750_keep_overshoot, prng_mode, interim_figure_return_ticks, free_build_cheat,
    # enforce_research, stat_decay_wide, bankrupt_deadline_wrap_safe
    flags = bytes([0, 0, 1 if full_tick else 0, 0, 0, 0])
    return (u8(0) + u8(0) + u8(0) + u64(1 << 20) + flags + u8(0) + u8(0) + u32(0) + u8(0)
            + u8(0) + u8(0) + u8(0))


def settings_bytes(width=0, height=0):
    return (u32(width) + u32(height) + u8(0) + u8(1) + u8(2) + u16(2550) + u8(1) + u32(0) + u32(0) + u8(0)
            + u8(0) + u32(0) + u8(0) + u8(0) + u32(3) + u32(100))


def rng_table_digest():
    x, values = 1, []
    for _ in range(2048):
        x = (x * 214013 + 2531011) & 0xFFFFFFFF
        values.append((x >> 16) & 0x7FFF)
    return fnv(b''.join(u16(v) for v in values))


def player_table_after(steps, step_ms):
    """(stamp, cursor) of the empty player table after `steps` advances of
    `step_ms` game milliseconds from tick 0 (docs/sim/state.md §5.10: d is the
    16-bit tick difference to the table's stamp, the count min(d*32/50 + 1, 32),
    the cursor moves by the count modulo 32, the stamp becomes the 16-bit tick)."""
    stamp, cursor, ms = 0, 0, 0
    for _ in range(steps):
        ms += step_ms
        t = (ms >> 1) & 0xFFFFFFFF
        d = (t - stamp) & 0xFFFF
        cursor = (cursor + min(d * 32 // 50 + 1, 32)) % 32
        stamp = t & 0xFFFF
    return stamp, cursor


def new_player(index, **over):
    """A player with the original's initial values (docs/sim/state.md section 4.5)."""
    p = dict(index=index, kind=8, volk=1, money=10000, ua=0, ub=0, rem=0, stamp=0, sa=0, sb=0,
             flags=1, flags_a0=0, colour_slot=0, default_colour=0xFFFFFFFF, state=0, deadline=0,
             defeat=False, free_build=False, name=b'')
    p.update(over)
    return p


def player_bytes(p):
    b = (u32(p['index']) + u32(p['kind']) + u32(p['volk']) + i64(p['money']) + u32(p['ua'])
         + u32(p['ub']) + u32(p['rem']) + u16(p['stamp']) + u32(p['sa']) + u32(p['sb'])
         + u32(p['flags']) + u32(p['flags_a0']) + u32(p['colour_slot'])
         + u32(p['default_colour']) + u8(p['state']) + u32(p['deadline'])
         + u8(1 if p['defeat'] else 0) + u8(1 if p['free_build'] else 0))
    b += string(p['name'])
    b += u64(65) + bytes(65)   # research state
    b += u64(65) + bytes(130)  # research progress
    return b


def players_digest(stamp, cursor, slots=None):
    """The players scope: u64 32, one child scope per slot (its digest is the FNV of
    the presence byte and the player, an empty slot of the absent byte), u16 stamp,
    u32 cursor."""
    slots = slots or {}
    digests = [fnv(u8(1) + player_bytes(slots[i])) if i in slots else fnv(u8(0))
               for i in range(32)]
    return fnv(u64(32) + b''.join(u64(d) for d in digests) + u16(stamp) + u32(cursor))


M32 = (1 << 32) - 1


def player_tick(p, t):
    """One player's tick for the default configuration (docs/sim/state.md section 5.10:
    upkeep gate 2000 on a 16-bit difference, sum over 15, decay 104/105 in u32,
    bankruptcy at <= -3000 with a 150000-tick grace, neutral player 31); the hooks
    do nothing."""
    if ((t - p['stamp']) & 0xFFFF) >= 2000:
        s = (p['ua'] + p['ub'] + p['rem']) & M32
        p['money'] -= s // 15
        p['rem'] = s % 15
        p['stamp'] = t & 0xFFFF
        p['sa'] = ((p['sa'] * 104) & M32) // 105
        p['sb'] = ((p['sb'] * 104) & M32) // 105
    if p['index'] != 31 and p['state'] == 0:
        if p['money'] <= -3000:
            if p['deadline'] == 0:
                p['deadline'] = (t + 150000) & M32
            elif t > p['deadline']:
                p['state'] = 2
        else:
            p['deadline'] = 0
        p['defeat'] = False


def table_run(slots, steps, step_ms):
    """(slots, stamp, cursor) of a populated player table after `steps` advances of
    `step_ms` from tick 0 with the default visit rule: min(d*32/50 + 1, 32) slots
    from the cursor, each present one ticked, the cursor moving one slot per visit,
    the stamp becoming the 16-bit tick."""
    stamp, cursor, ms = 0, 0, 0
    for _ in range(steps):
        ms += step_ms
        t = (ms >> 1) & M32
        d = (t - stamp) & 0xFFFF
        for _ in range(min(d * 32 // 50 + 1, 32)):
            if cursor in slots:
                player_tick(slots[cursor], t)
            cursor = (cursor + 1) % 32
        stamp = t & 0xFFFF
    return slots, stamp, cursor


# ---- islands (stage S5): the synthetic two-island scenario ----

CHUNK_EDGE = 8
NEUTRAL_OWNER = 15


def terrain_digest(height, gfx, fertility):
    d = (u64(len(height)) + bytes(height) + u64(len(gfx)) + b''.join(u16(v) for v in gfx)
         + u64(len(fertility)) + bytes(fertility))
    h = fnv(d)
    return 1 if h == 0 else h


def tiles_digest(w, h, tiles):
    """tiles: a list of (object, phase, owner, aux) in row-major order. The scope
    `tiles`: u32 w, u32 h, u32 chunk_edge, then the digest of every chunk (row-major
    chunk order) over its in-bounds tiles (object u32, phase u8, owner u32, aux u32)."""
    cx = (w + CHUNK_EDGE - 1) // CHUNK_EDGE
    cy = (h + CHUNK_EDGE - 1) // CHUNK_EDGE
    d = u32(w) + u32(h) + u32(CHUNK_EDGE)
    for ky in range(cy):
        for kx in range(cx):
            c = b''
            for y in range(ky * CHUNK_EDGE, min(h, (ky + 1) * CHUNK_EDGE)):
                for x in range(kx * CHUNK_EDGE, min(w, (kx + 1) * CHUNK_EDGE)):
                    o, ph, ow, ax = tiles[y * w + x]
                    c += u32(o) + u8(ph) + u32(ow) + u32(ax)
            d += u64(fnv(c))
    return fnv(d)


def island_digest(slot, x0, y0, w, h, climate, owner_runs, objects, deposits, terrain=None):
    tiles = [[0, 0, NEUTRAL_OWNER, 0] for _ in range(w * h)]
    at = 0
    for owner, length in owner_runs:
        for _ in range(length):
            tiles[at][2] = owner
            at += 1
    for index, obj, phase in objects:
        tiles[index][0] = obj
        tiles[index][1] = phase
    td = terrain_digest(*terrain) if terrain else 0
    b = (u32(slot) + struct.pack('<i', x0) + struct.pack('<i', y0) + u32(w) + u32(h)
         + u32(climate) + u32(1) + u64(td))
    b += u16(0) + u32(0) * 4          # sweep
    b += u16(0)                       # event300_stamp
    b += u16(0) + u32(0)              # objmgr
    b += u16(0) + u32(0)              # ring
    b += u8(0)                        # no drought
    b += u64(len(deposits))
    for goods, x, y, remaining, reserve in deposits:
        b += string(goods) + struct.pack('<i', x) + struct.pack('<i', y) + u32(remaining)
        b += u8(1 if reserve else 0)
    b += u64(0)                       # reserved_object_count
    b += u64(tiles_digest(w, h, tiles))
    return fnv(u8(1) + b)


def two_island_scenario():
    """synthetic::two_island_scenario(): (players, island digests by slot)."""
    players = {0: new_player(0), 1: new_player(1, kind=12, money=25000, name=b'AI')}
    height = [i % 7 for i in range(600)]
    gfx = [(i * 3) & 0xFFFF for i in range(600)]
    fertility = [i % 5 for i in range(600)]
    islands = {
        0: island_digest(0, 100, 100, 64, 64, 2, [(15, 128), (0, 64 * 62)],
                         [(200, 1201, 0), (201, 1202, 3), (4095, 1203, 7)],
                         [(b'EISENERZ', 110, 120, 2000 * 256, True)]),
        3: island_digest(3, 300, 200, 30, 20, 6, [(1, 300)],
                         [(0, 2001, 0), (599, 2002, 1)],
                         [(b'GOLD', 301, 202, 123456, False),
                          (b'STEINE', 329, 219, 2000 * 256, True)],
                         (height, gfx, fertility)),
    }
    return players, islands


def islands_digest(by_slot):
    """The islands scope: u64 slot count (highest slot + 1), one child scope per slot
    (an absent slot hashes as the single byte 0)."""
    n = max(by_slot) + 1 if by_slot else 0
    return fnv(u64(n) + b''.join(u64(by_slot.get(i, fnv(u8(0)))) for i in range(n)))


def g1_slots():
    return {
        0: new_player(0, money=500, ua=300, ub=30, sa=1050, sb=210, name=b'Anno'),
        1: new_player(1, money=-2990, ua=600, defeat=True),
        2: new_player(2, ua=14, rem=7, sa=50000000, sb=40000000, stamp=1000, colour_slot=3),
        4: new_player(4, money=100, ua=15, state=3, defeat=True),
        7: new_player(7, money=-9999, state=8),
        31: new_player(31, money=-9000, ua=150),
    }


def g1_after(steps, step_ms):
    """(slots, stamp) of the G1 table after `steps` advances of `step_ms` game
    milliseconds from tick 0 with the full-tick rule (every slot, in order, on each
    advance; the cursor stays 0)."""
    slots, stamp, ms = g1_slots(), 0, 0
    for _ in range(steps):
        ms += step_ms
        t = (ms >> 1) & M32
        for i in range(32):
            if i in slots:
                player_tick(slots[i], t)
        stamp = t & 0xFFFF
    return slots, stamp


def state_hash(registry, cursor, ms, last_tick, players, full_tick=False, islands=None,
               world=(0, 0)):
    header = (u32(STATE_HASH_VERSION) + u32(PRNG_TIMELINE_VERSION) + config_bytes()
              + rules_bytes(full_tick))
    header += u64(registry) + u64(22) + b''.join(string(b'default') for _ in range(22))
    clock_rng = u64(ms) + u64(last_tick) + u64(2048) + u64(rng_table_digest()) + u32(cursor)
    islands = islands_digest({}) if islands is None else islands
    return fnv(u64(fnv(header)) + u64(fnv(clock_rng)) + u64(fnv(settings_bytes(*world)))
               + u64(players) + u64(islands))


def values():
    registry = registry_digest(goods_names())
    out = {'empty_registry': registry,
           'empty_world': state_hash(registry, 0, 0, 0, players_digest(0, 0))}
    for steps in (1, 100, 1000):  # G0: seed 1234, advance(10) x steps
        out['g0_step%d' % steps] = state_hash(registry, 1234, 10 * steps, 10 * steps // 2,
                                              players_digest(*player_table_after(steps, 10)))
    for steps in (100, 700):  # G1: seed 1234, full-tick rule, advance(500) x steps
        slots, stamp = g1_after(steps, 500)
        out['g1_players_step%d' % steps] = state_hash(
            registry, 1234, 500 * steps, 500 * steps // 2, players_digest(stamp, 0, slots), True)
    # S5: the synthetic two-island scenario, seed 1234, advance(10) x steps
    players, islands = two_island_scenario()
    isl = islands_digest(islands)
    for steps in (0, 100):
        slots, stamp, cursor = table_run(players, steps, 10)
        key = 's5_islands_create' if steps == 0 else 's5_islands_step%d' % steps
        out[key] = state_hash(registry, 1234, 10 * steps, 10 * steps // 2,
                              players_digest(stamp, cursor, slots), islands=isl,
                              world=(512, 512))
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
