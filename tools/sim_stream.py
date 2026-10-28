#!/usr/bin/env python3
"""Independent transcription of the simulation's state stream (docs/sim/state.md
§9.3, stage S7): the registry digest of the empty registry and the state hash of
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
Since stage S6 every island has the scope cities after tiles (per city slot a scope
city with an optional City: the stock, the goods settings, the build goods, the
trade lists and the scope index with one scope list per building list and one scope
building per list slot). The S5 scenarios have one empty city per island; the
script transcribes the city and, for the golden s6_g1_create, a weaver and a farm
(the Workshop and Farm payloads). The registry digest of the synthetic registry
(make_synthetic_types) is a literal taken from types_test, because transcribing the
type table is out of scope here: everything else of that golden is independent.
Since stage S7 the stream has the scopes ids (after settings), figures and events
(after islands), and World::advance ticks the islands: the script transcribes the
island tick (the sweep cursors, the object manager, the city ring, the city tick and
the building scheduler: stamps, cursors and visit counts, with the production gate
asserted closed) for the horizon of the goldens s5_islands_step100 and
s6_g1_step100, so the island state after 100 advances is held to an independent
reading of the spec too. The G1 run of the spec (20 000 steps) is not transcribed:
its goldens (s7_g1_*) come from the C++ and are protected by the semantic asserts of
slice_test.cpp.
Since stage S12 the ids scope carries the two millisecond accumulators (7.5 minute
event, monopoly refresh) and the draw-guard flag, the figure registry's stamp moves
with every advance (phase B over its one empty slot), and the scope timers_monopoly
follows events: the script timers (their last tick) and the monopoly table, whose
refresh body runs on the first advance only (D5) and which the script transcribes
(docs/sim/state.md section 5.10: the climate masks, the one Volk-1 city per island).
The 7.5 minute event is asserted not to fire within the horizon of the goldens.
The S12 fix-up added Rules::timer0_clamp (owner decision on OQ-133), one more byte of the
header's Rules.
Stage S13 (stream 108) widened the drought record's city mask to u64 count + u32 words;
the transcribed worlds hold no drought (presence byte 0), so only the version moved.
S14 stage 2 (stream 110, PRNG timeline 2) added SimConfig::links (the island-link
selection's tables) and Rules::island_candidate_order to the header; --check also refuses
a scenario_corpus.txt whose version header line names other versions.
T24 (stream 111) made the market class tick (tick 7, payload Market 6 in the class table),
added the figure types (u64 count 0 for the empty registry) and the types' entrance
points to the registry digest, the cart reservations to the payloads (the Workshop's
`reserved` list after `activity`, the Farm's u32 after `activity`) and the stamp and the
transport state to the figure record (the transcribed registries hold no figure).
T33.4 (stream 112) added the immutable walk-table digest after the registry digest.
T33.5 stage 2 (stream 113) adds foot_dx (u16), foot_dy (u16), foot_rotation
(u8) and, in fix-up F1, foot_anchor (u8) to each tile.
T33.5 stage 3 (stream 114) adds Rules::removal_clear_tile, one byte of the
header. The tile columns are unchanged.

  sim_stream.py            print the values
  sim_stream.py --check    compare them with fixtures/synthetic/sim/goldens.txt
                           (g0_*, g1_*, s5_*, s6_*) and the version header line of
                           scenario_corpus.txt; exit 1 on a difference

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
STATE_HASH_VERSION = 115
PRNG_TIMELINE_VERSION = 2


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
    (13, 7, 6, 500),   # Market 0x1006b6d0 (T24: the cart send)
    (14, 0, 0, 500),   # MarketCombi
    (15, 0, 0, 5000),  # MarketStall
    (16, 0, 0, 5000),  # Inn
    (17, 0, 0, 500),   # PublicService 0x1006b4ec
    (18, 0, 0, 500),   # Doctor
    (19, 0, 0, 500),   # School (0x1006b4e0, shared)
    (20, 0, 0, 0),     # Castle (no stamp gate)
    (21, 0, 0, 500),   # FireBrigade
    (22, 8, 7, 2500),  # House 0x10076230 (P1: TickKind::House, PayloadTag::House; the tick does nothing)
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
    d += u64(0)  # no figure types (T24)
    d += u64(0)  # no group table (P1)
    return fnv(d)


def synthetic_walk_tables_digest():
    """walk_cells.cpp synthetic_walk_tables(), through walk_tables_digest()."""
    mapper = [0] * 512
    mapper[0] = 0xFFF
    d = u32(0) + u32(0) + u64(len(mapper)) + b''.join(u16(x) for x in mapper)
    layers = []
    for layer_no in range(5):
        layer = bytearray(0x400 + 8 * 8)
        for code in (0, 2, 4):
            layer[code] = 0 if layer_no == 3 and code == 2 else 0xFF
            for direction in range(8):
                layer[0x400 + 8 * code + direction] = 1
        layers.append(bytes(layer))
    d += u64(len(layers))
    for layer in layers:
        d += u64(len(layer)) + layer
    d += u64(5)
    for _ in range(5):
        for k in range(9):
            d += u16(1 << k) + u8(k % 3) + u8(k // 3)
    return fnv(d)


def config_bytes():
    b = b''.join(u32(x) for x in [256, 16, 15, 32, 31, 5, 28, 20, 32, 5, 65, 56, 0x29, 5, 2,
                                  8, 1, 10, 100, 8])
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
    b += link_tables_bytes()
    return b


def link_tables_bytes():
    """SimConfig::links (S14 stage 2, docs/re/annoscene.md section 5 step 3): the counts,
    the climate masks, the ore exclusion table, the width ranks and the rank scores of
    the island-link selection, as read from AnnoScene.dll."""
    b = b''.join(u32(x) for x in [20, 9, 25, 5, 50])
    b += u32s([0x18000, 0xF009, 0xF949, 0xF06B, 0xF6B9, 0xF04D, 0xF04D])
    b += u32s([0, 5, 0x25, 1, 3, 0xB, 0xB])
    b += u32s([0, 1, 2, 3, 4, 5, 0, 7, 8])
    # Row k = the ore kind to place, column e = a kind the link has: 1 = refused
    # (0x10009130: the byte at 0x1003AF40 + 7k + e is 0xFF).
    exclusion = ['100000001', '011111100', '001000011', '111111101', '011111100',
                 '001001001', '011111100', '000000000', '000000000']
    b += u64(81) + bytes(int(c) for row in exclusion for c in row)
    raw = ([256, 128, 0, 0] + [256, 128, 80, 80] + [80, 128, 256, 64] * 4
           + [64, 80, 128, 256] * 19)
    ore = ([256, 128, 0, 0] + [256, 128, 80, 80] + [128, 256, 80, 64] * 13
           + [64, 80, 128, 256] * 10)
    b += u32s(raw) + u32s(ore) + u32s([4, 3, 2, 1]) + u32s([116, 6, 3, 1])
    return b


def rules_bytes(full_tick=False):
    # slot_policy, spatial_order, events_drain_new, events_max_dispatch, six flags (flush,
    # monopoly_every_30s, player_table_full_tick, drought centre, atomic, empty city),
    # event750_keep_overshoot, prng_mode, interim_figure_return_ticks, free_build_cheat,
    # enforce_research, stat_decay_wide, bankrupt_deadline_wrap_safe, timer0_clamp,
    # island_candidate_order (S14 stage 2: Sorted = 0), removal_clear_tile (F37)
    flags = bytes([0, 0, 1 if full_tick else 0, 0, 0, 0])
    return (u8(0) + u8(0) + u8(0) + u64(1 << 20) + flags + u8(0) + u8(0) + u32(0) + u8(0)
            + u8(0) + u8(0) + u8(0) + u8(0) + u8(0) + u8(0))


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
    """tiles: a list of (object, phase, owner, aux, walk_code, walk_mask,
    foot_dx, foot_dy, foot_rotation, foot_anchor) in row-major order. The
    scope `tiles`: u32 w, u32 h, u32 chunk_edge, then the digest of every
    chunk (row-major chunk order) over its in-bounds tiles (object u32,
    phase u8, owner u32, aux u32, walk_code u16, walk_mask u16, foot_dx u16,
    foot_dy u16, foot_rotation u8, foot_anchor u8)."""
    cx = (w + CHUNK_EDGE - 1) // CHUNK_EDGE
    cy = (h + CHUNK_EDGE - 1) // CHUNK_EDGE
    d = u32(w) + u32(h) + u32(CHUNK_EDGE)
    for ky in range(cy):
        for kx in range(cx):
            c = b''
            for y in range(ky * CHUNK_EDGE, min(h, (ky + 1) * CHUNK_EDGE)):
                for x in range(kx * CHUNK_EDGE, min(w, (kx + 1) * CHUNK_EDGE)):
                    o, ph, ow, ax, wc, wm, fdx, fdy, frot, fanc = tiles[y * w + x]
                    c += (u32(o) + u8(ph) + u32(ow) + u32(ax) + u16(wc) + u16(wm)
                          + u16(fdx) + u16(fdy) + u8(frot) + u8(fanc))
            d += u64(fnv(c))
    return fnv(d)


# ---- cities and buildings (stage S6)

CITY_SLOTS = 16
WAREHOUSE_GOODS = 56
BUILD_GOODS = 5
TRADE_LISTS = 2
LIST_COUNT = 6
STATUS_NONE = u8(0) + u8(0) + u16(0)   # code, icons, time


def list_digest(hint, cursor, slots):
    """One building list: i64 hint, u32 cursor, u64 slot count, one child scope per
    slot (an empty slot hashes as the single byte 0, a building as 1 + its bytes)."""
    d = struct.pack('<q', hint) + u32(cursor) + u64(len(slots))
    for b in slots:
        d += u64(fnv(u8(1) + b) if b is not None else fnv(u8(0)))
    return fnv(d)


def index_digest(stamp, lists):
    """The scope index: u16 stamp, u64 list count, one child scope per list."""
    return fnv(u16(stamp) + u64(len(lists)) + b''.join(u64(x) for x in lists))


def city_digest(number, owner, buildings_by_list=None, tick_stamp=0, stock=None,
                index_stamp=0, cursors=None):
    """One present city. buildings_by_list: {list id: (hint, [bytes or None, ...])};
    a list that is not given is a fresh one (hint -1, cursor 0, one empty slot).
    cursors: {list id: scheduler cursor}; index_stamp: the index's shared stamp."""
    buildings_by_list = buildings_by_list or {}
    cursors = cursors or {}
    lists = []
    for lid in range(LIST_COUNT):
        hint, slots = buildings_by_list.get(lid, (-1, [None]))
        lists.append(list_digest(hint, cursors.get(lid, 0), slots))
    stock = stock or [0] * WAREHOUSE_GOODS
    b = u32(number) + u32(owner) + u16(tick_stamp)
    b += u32(50 * 256) + u32s(stock)
    b += u64(WAREHOUSE_GOODS) + b''.join(u8(0) + struct.pack('<i', -1)
                                         for _ in range(WAREHOUSE_GOODS))
    b += u64(BUILD_GOODS) + b''.join(u32(0) + u64(0) for _ in range(BUILD_GOODS))
    b += u64(TRADE_LISTS) + b''.join(u64(0) for _ in range(TRADE_LISTS))
    b += u64(index_digest(index_stamp, lists))
    return fnv(u8(1) + b)


def cities_digest(cities):
    """The scope cities: u64 slot count, one child scope per slot."""
    return fnv(u64(CITY_SLOTS) + b''.join(u64(cities.get(i, fnv(u8(0)))) for i in range(CITY_SLOTS)))


def workshop_bytes(out, inn, activity=0):
    # last, switched_on, suspended, blocked, out, in, activity, reserved (T24), carriers_out
    return (u16(0) + u8(1) + u8(0) + u8(0) + u64(1) + u32(out) + u64(1) + u32(inn) + u64(1)
            + u32(activity) + u64(1) + u32(0) + u32(0) + STATUS_NONE)


def farm_bytes(out=0, raw=0):
    # last, switched_on, suspended, blocked, out, raw, activity, reserved (T24), workers_out
    return (u16(0) + u8(1) + u8(0) + u8(0) + u32(out) + u32(raw) + u16(0) + u32(0) + u32(0)
            + STATUS_NONE)


def building_bytes(serial, island, type_id, x, y, byte0e, tag, payload):
    return (u64((island << 32) + serial) + u32(type_id) + struct.pack('<i', x)
            + struct.pack('<i', y) + u8(0) + u8(byte0e) + u8(0) + u8(tag) + payload)


def island_digest(slot, x0, y0, w, h, climate, owner_runs, objects, deposits, terrain=None,
                  cities=None, next_serial=1, sweep=(0, 0, 0, 0, 0), e300=0, objmgr=(0, 0),
                  ring=(0, 0)):
    tiles = [[0, 0, NEUTRAL_OWNER, 0, 2, 0, 0, 0, 0, 0] for _ in range(w * h)]
    at = 0
    for owner, length in owner_runs:
        for _ in range(length):
            tiles[at][2] = owner
            at += 1
    for index, obj, phase in objects:
        tiles[index][0] = obj
        tiles[index][1] = phase
        # Ids 9001..9010 are make_synthetic_types: empty HEAD, 1x1, empty WALKMASK,
        # flags_word 0. place_object_record stores code 4 (HEAD bytes 0x2f and
        # 0x30 are both 0, the constructor default) and mask 0, and the first
        # dword: offset 0, rotation 0, and the block anchor. Each transcribed
        # building is the only written tile of its world 8x8 block, so the
        # anchor is 1. create_building writes the same record on the same tile,
        # so one store matches both. An id the registry does not hold is written
        # on the named tile only and does not take an anchor. The S5 islands use
        # the empty registry, so 1201/1202/1203/2001/2002 stay at the reset.
        if obj in range(9001, 9011):
            tiles[index][4] = 4
            tiles[index][5] = 0
            tiles[index][9] = 1
    td = terrain_digest(*terrain) if terrain else 0
    b = (u32(slot) + struct.pack('<i', x0) + struct.pack('<i', y0) + u32(w) + u32(h)
         + u32(climate) + u32(next_serial) + u64(td))
    b += u16(sweep[0]) + b''.join(u32(v) for v in sweep[1:])   # sweep: stamp, grow x/y, fauna x/y
    b += u16(e300)                    # event300_stamp
    b += u16(objmgr[0]) + u32(objmgr[1])    # objmgr
    b += u16(ring[0]) + u32(ring[1])        # ring
    b += u8(0)                        # no drought
    b += u64(len(deposits))
    for goods, x, y, remaining, reserve in deposits:
        b += string(goods) + struct.pack('<i', x) + struct.pack('<i', y) + u32(remaining)
        b += u8(1 if reserve else 0)
    b += u64(0)                       # reserved_object_count
    b += u64(tiles_digest(w, h, tiles))
    b += u64(cities_digest(cities or {}))
    return fnv(u8(1) + b)


SCHEDULE = [5, 1, 4, 3, 2]   # ListLayout::original().schedule


class IslandSim:
    """The island tick of docs/sim/state.md sections 5.4 to 5.6 for the default
    configuration, as far as the goldens need it: the two sweep cursors (the growth
    chunk visit itself writes no tile before stage S13), the object manager over the
    deposits, the city ring over 16 slots, and per present city the building
    scheduler (stamp, cursor and visit count of the five scheduled lists) with the
    city stamp. `cities` is {number: {list id: slot list}} where a slot is None (a
    hole) or a building (anything else; its `last` is 0 in the goldens). The 300 s
    event and the production steps are asserted not to happen within the horizon
    (the gate of a building with last 0 opens at tick 500, the 300 s gate at tick
    150000): a golden run that reached them would need more transcription."""

    def __init__(self, w, h, n_deposits, cities):
        self.w, self.h, self.n_deposits = w, h, n_deposits
        self.chunks = ((w + CHUNK_EDGE - 1) // CHUNK_EDGE) * ((h + CHUNK_EDGE - 1) // CHUNK_EDGE)
        self.sweep = [0, 0, 0, 0, 0]       # stamp, grow x, grow y, fauna x, fauna y
        self.e300 = 0
        self.objmgr = [0, 0]
        self.ring = [0, 0]
        self.cities = {no: {'lists': lists, 'cursor': {}, 'index_stamp': 0, 'tick_stamp': 0}
                       for no, lists in cities.items()}

    def step_cursor(self, x, y):
        x += CHUNK_EDGE
        if x >= self.w:
            x = 0
            y += CHUNK_EDGE
            if y >= self.h:
                y = 0
        return x, y

    def city_tick(self, c, t):
        for lid in SCHEDULE:
            slots = c['lists'].get(lid, [None])
            n = len(slots)
            d = (t - c['index_stamp']) & 0xFFFF
            cur = c['cursor'].get(lid, 0)
            for _ in range(min(d * n // 500 + 1, n)):
                if slots[cur] is not None:
                    assert (t - 0) & 0xFFFF < 500, 'a production step would run (not transcribed)'
                cur = (cur + 1) % n
            c['cursor'][lid] = cur
        c['index_stamp'] = t & 0xFFFF
        c['tick_stamp'] = t & 0xFFFF

    def tick(self, t):
        d = (t - self.sweep[0]) & 0xFFFF
        n1 = min(self.chunks * d // 2000 + 1, self.chunks) if self.chunks else 0
        n2 = min(self.chunks * d // 15000 + 1, self.chunks) if self.chunks else 0
        for _ in range(n1):
            self.sweep[1], self.sweep[2] = self.step_cursor(self.sweep[1], self.sweep[2])
        for _ in range(n2):
            self.sweep[3], self.sweep[4] = self.step_cursor(self.sweep[3], self.sweep[4])
        n = self.n_deposits
        d = (t - self.objmgr[0]) & 0xFFFF
        for _ in range(min(d * n // 5000 + 1, n) if n else 0):
            self.objmgr[1] = (self.objmgr[1] + 1) % n
        self.objmgr[0] = t & 0xFFFF
        d = (t - self.ring[0]) & 0xFFFF
        for _ in range(min(d * CITY_SLOTS // 50 + 1, CITY_SLOTS)):
            if self.ring[1] in self.cities:
                self.city_tick(self.cities[self.ring[1]], t)
            self.ring[1] = (self.ring[1] + 1) % CITY_SLOTS
        self.ring[0] = t & 0xFFFF
        assert (t // 500 - self.e300) & 0xFFFF < 300, 'the 300 s event would run (not transcribed)'
        self.sweep[0] = t & 0xFFFF

    def run(self, steps, step_ms):
        ms = 0
        for _ in range(steps):
            ms += step_ms
            self.tick((ms >> 1) & M32)


def two_island_scenario(steps=0, step_ms=10):
    """synthetic::two_island_scenario() after `steps` advances: (players, island
    digests by slot)."""
    players = {0: new_player(0), 1: new_player(1, kind=12, money=25000, name=b'AI')}
    height = [i % 7 for i in range(600)]
    gfx = [(i * 3) & 0xFFFF for i in range(600)]
    fertility = [i % 5 for i in range(600)]
    sim0 = IslandSim(64, 64, 1, {0: {}})
    sim3 = IslandSim(30, 20, 2, {1: {}})
    sim0.run(steps, step_ms)
    sim3.run(steps, step_ms)

    def state(sim):
        return dict(sweep=tuple(sim.sweep), e300=sim.e300, objmgr=tuple(sim.objmgr),
                    ring=tuple(sim.ring))

    def city(sim, no, owner):
        c = sim.cities[no]
        return city_digest(no, owner, tick_stamp=c['tick_stamp'], index_stamp=c['index_stamp'],
                           cursors=c['cursor'])
    islands = {
        0: island_digest(0, 100, 100, 64, 64, 2, [(15, 128), (0, 64 * 62)],
                         [(200, 1201, 0), (201, 1202, 3), (4095, 1203, 7)],
                         [(b'EISENERZ', 110, 120, 2000 * 256, True)],
                         cities={0: city(sim0, 0, 0)}, **state(sim0)),
        3: island_digest(3, 300, 200, 30, 20, 6, [(1, 300)],
                         [(0, 2001, 0), (599, 2002, 1)],
                         [(b'GOLD', 301, 202, 123456, False),
                          (b'STEINE', 329, 219, 2000 * 256, True)],
                         (height, gfx, fertility), cities={1: city(sim3, 1, 1)}, **state(sim3)),
    }
    return players, islands


def islands_digest(by_slot):
    """The islands scope: u64 slot count (highest slot + 1), one child scope per slot
    (an absent slot hashes as the single byte 0)."""
    n = max(by_slot) + 1 if by_slot else 0
    return fnv(u64(n) + b''.join(u64(by_slot.get(i, fnv(u8(0)))) for i in range(n)))


# The registry digest of synthetic::make_synthetic_types() (the golden test in
# building_state_test.cpp checks it);
# a literal here because the type table is not transcribed by this script.
SYNTHETIC_REGISTRY = 0x20BD3120BCF7E4F2


def g1_scenario(steps=0, step_ms=10):
    """synthetic::g1_scenario() after `steps` advances: one player, island 0 (64 x 64
    at 100,100) with city 0 holding the weaver (9001, 5 t of wool) at (110,110) and
    the grain farm (9002, 5 t of raw material) at (120,110), both in the production
    list L4: slot 0 and slot 1 (the list grows from 1 to 10 with the second), hint 1."""
    weaver = building_bytes(1, 0, 9001, 110, 110, 0x86, 1, workshop_bytes(0, 5 * 256))
    farm = building_bytes(2, 0, 9002, 120, 110, 0x86, 3, farm_bytes(0, 5 * 256))
    production = (1, [weaver, farm] + [None] * 8)
    sim = IslandSim(64, 64, 1, {0: {4: production[1]}})
    sim.run(steps, step_ms)
    c = sim.cities[0]
    island = island_digest(
        0, 100, 100, 64, 64, 2, [(15, 128), (0, 64 * 62)],
        [(200, 1201, 0), (201, 1202, 3), (4095, 1203, 7), (10 * 64 + 10, 9001, 0),
         (10 * 64 + 20, 9002, 0)],
        [(b'EISENERZ', 110, 120, 2000 * 256, True)],
        cities={0: city_digest(0, 0, {4: production}, tick_stamp=c['tick_stamp'],
                               index_stamp=c['index_stamp'], cursors=c['cursor'])},
        next_serial=3, sweep=tuple(sim.sweep), e300=sim.e300, objmgr=tuple(sim.objmgr),
        ring=tuple(sim.ring))
    return {0: new_player(0)}, {0: island}


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


def figures_digest(stamp=0):
    """The empty figure registry: i64 hint -1, u32 cursor 0, u64 slot count 1 and its
    child scope (an empty slot hashes as the single byte 0), u16 stamp, empty expiry
    list (the deferred list left the stream with T22: it never holds a figure, OQ-131).
    Phase B of every advance visits the one slot (clamp(d*1/50, 1, 1) = 1, the cursor
    stays 0) and writes the stamp: the 16-bit tick of the last advance."""
    return fnv(struct.pack('<q', -1) + u32(0) + u64(1) + u64(fnv(u8(0))) + u16(stamp) + u64(0))


MONOPOLY_MASKS = [0x18000, 0xF009, 0xF949, 0xF06B, 0xF6B9, 0xF04D, 0xF04D]


def monopoly_entries(islands, players, seconds):
    """The monopoly table after the refresh body ran once (docs/sim/state.md section 5.10,
    AnnoWelt 0x1009d2a0): `islands` is a list in table order of (climate, [owner of
    each present city in number order]); `players` maps a slot to its Volk. For the raw
    slots 0..19 and each island whose climate mask has bit s: no single Volk-1 city
    clears the entry and ends the scan for s; otherwise a different owner takes the
    entry with the game seconds."""
    entries = [(0, None)] * 28
    for s in range(20):
        for climate, owners in islands:
            if not (MONOPOLY_MASKS[climate] >> s) & 1:
                continue
            qualifying = [o for o in owners if players.get(o) == 1]
            owner = qualifying[0] if len(qualifying) == 1 else None
            if owner is None:
                entries[s] = (0, None)
                break
            if entries[s][1] != owner:
                entries[s] = (seconds, owner)
    return entries


def timers_monopoly_digest(last_tick=0, entries=None):
    """The timers_monopoly scope: u32 last_tick, five stopped timers (u64 5 + u8 each,
    u64 5 + u32 each) and the 28 monopoly entries (u32 time, optional u32 player)."""
    entries = entries or [(0, None)] * 28
    b = u32(last_tick) + u64(5) + bytes(5) + u64(5) + bytes(20) + u64(28)
    for time_s, player in entries:
        b += u32(time_s) + (u8(0) if player is None else u8(1) + u32(player))
    return fnv(b)


def state_hash(registry, cursor, ms, last_tick, players, full_tick=False, islands=None,
               world=(0, 0), steps=0, step_ms=0, monopoly=None, walk_digest=0):
    """`steps` advances of `step_ms` reached `ms`; `monopoly` is the table its first
    advance wrote (None: all entries empty)."""
    header = (u32(STATE_HASH_VERSION) + u32(PRNG_TIMELINE_VERSION) + config_bytes()
              + rules_bytes(full_tick))
    header += (u64(registry) + u64(walk_digest) + u64(22)
               + b''.join(string(b'default') for _ in range(22)))
    clock_rng = u64(ms) + u64(last_tick) + u64(2048) + u64(rng_table_digest()) + u32(cursor)
    islands = islands_digest({}) if islands is None else islands
    # The 7.5 minute accumulator holds every ms (below 450000 within these goldens: the
    # event would draw), the monopoly accumulator the first step only (D5: the body ran
    # once, from acc 0, and only the body adds; a first step of 30000 ms or more would
    # run it again).
    assert ms < 450000, 'the 7.5 minute event would fire (not transcribed)'
    assert step_ms < 30000, 'the monopoly refresh would run again (not transcribed)'
    acc750 = ms & M32
    acc_monopoly = step_ms if steps > 0 else 0
    ids = fnv(u64(1) + u64(1) + u32(acc750) + u32(acc_monopoly) + u8(0))
    events = fnv(u64(0))                # no timed events
    t = (ms >> 1) & M32
    figures = figures_digest(t & 0xFFFF if steps > 0 else 0)
    timers = timers_monopoly_digest(t if steps > 0 else 0, monopoly if steps > 0 else None)
    return fnv(u64(fnv(header)) + u64(fnv(clock_rng)) + u64(fnv(settings_bytes(*world)))
               + u64(ids) + u64(players) + u64(islands) + u64(figures) + u64(events)
               + u64(timers))


def values():
    registry = registry_digest(goods_names())
    out = {'empty_registry': registry,
           'empty_world': state_hash(registry, 0, 0, 0, players_digest(0, 0))}
    for steps in (1, 100, 1000):  # G0: seed 1234, advance(10) x steps
        out['g0_step%d' % steps] = state_hash(registry, 1234, 10 * steps, 10 * steps // 2,
                                              players_digest(*player_table_after(steps, 10)),
                                              steps=steps, step_ms=10)
    for steps in (100, 700):  # G1: seed 1234, full-tick rule, advance(500) x steps
        slots, stamp = g1_after(steps, 500)
        out['g1_players_step%d' % steps] = state_hash(
            registry, 1234, 500 * steps, 500 * steps // 2, players_digest(stamp, 0, slots), True,
            steps=steps, step_ms=500)
    # S5: the synthetic two-island scenario, seed 1234, advance(10) x steps
    for steps in (0, 100):
        players, islands = two_island_scenario(steps)
        isl = islands_digest(islands)
        slots, stamp, cursor = table_run(players, steps, 10)
        key = 's5_islands_create' if steps == 0 else 's5_islands_step%d' % steps
        # Island 0 (climate 2) holds city 0 of player 0, island 3 (climate 6) city 1 of
        # player 1, both Volk 1; the first advance is at tick 5, game second 0.
        monopoly = monopoly_entries([(2, [0]), (6, [1])], {0: 1, 1: 1}, 0)
        out[key] = state_hash(registry, 1234, 10 * steps, 10 * steps // 2,
                              players_digest(stamp, cursor, slots), islands=isl,
                              world=(512, 512), steps=steps, step_ms=10, monopoly=monopoly)
    # S6: the G1 scenario (a city with a weaver and a farm), seed 1234, advance(10) x steps
    for steps in (0, 100):
        players, islands = g1_scenario(steps)
        isl = islands_digest(islands)
        slots, stamp, cursor = table_run(players, steps, 10)
        key = 's6_g1_create' if steps == 0 else 's6_g1_step%d' % steps
        monopoly = monopoly_entries([(2, [0])], {0: 1}, 0)
        out[key] = state_hash(SYNTHETIC_REGISTRY, 1234, 10 * steps, 10 * steps // 2,
                              players_digest(stamp, cursor, slots), islands=isl, world=(512, 512),
                              steps=steps, step_ms=10, monopoly=monopoly,
                              walk_digest=synthetic_walk_tables_digest())
    return out


def goldens():
    found = {}
    path = ROOT / 'fixtures/synthetic/sim/goldens.txt'
    for line in path.read_text(encoding='utf-8').splitlines():
        parts = line.split('#')[0].split()
        if len(parts) == 4:
            found[parts[0]] = (int(parts[2]), int(parts[3], 16))
    return found


def corpus_versions():
    """(kStateHashVersion, kPrngTimelineVersion) from the version header line of
    fixtures/synthetic/sim/scenario_corpus.txt (written by scenario_test.cpp; its create
    hashes move with both versions), or None when the file has none."""
    path = ROOT / 'fixtures/synthetic/sim/scenario_corpus.txt'
    for line in path.read_text(encoding='utf-8').splitlines():
        m = re.match(r'# kStateHashVersion=(\d+) kPrngTimelineVersion=(\d+)$', line.strip())
        if m:
            return int(m.group(1)), int(m.group(2))
    return None


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
    # The scenario corpus file cannot be recomputed here (it needs the game files), but a
    # header from another stream or timeline version means its hashes are stale.
    versions = corpus_versions()
    if versions != (STATE_HASH_VERSION, PRNG_TIMELINE_VERSION):
        print('scenario_corpus.txt: header %s, this script is for kStateHashVersion=%d '
              'kPrngTimelineVersion=%d (re-baseline it with the game dir)' %
              (versions, STATE_HASH_VERSION, PRNG_TIMELINE_VERSION))
        bad += 1
    print('goldens agree' if bad == 0 else '%d difference(s)' % bad)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
