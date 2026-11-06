#!/usr/bin/env python3
"""Independent byte/digest reference for the figure-type part of the registry digest
(task MAP-FOUND-3 package C).

Derived from the WRITTEN contract (docs/sim/ship-founding.md section 17.3a), not from the C++
serializer:

    registry digest stream = u64 count(types) + types
                           + u64 count(goods)  + goods
                           + u64 count(classes) + classes
                           + u64 count(figures) + figures
                           + u64 count(groups) + groups + u64 count(stages) + stages
    FNV-1a 64 (offset 0xcbf29ce484222325, prime 0x100000001b3) over the little-endian bytes.
    One figure = u32 id, u32 speed_q24, u32 capacity_88, u32 layer, u32 load_ticks,
                 u32 bewohntype, u32 range_88, u8 hull_present (0/1), i64 hull (0 when absent),
                 u8 nav_present (0/1) and seven u32 (speed_q32, turn_step_q32, max_hp, hold_count_a,
                 hold_count_b, nav_class, stamp_radius_88; all 0 when absent)  [stream 122],
                 u32 route (the ROUTETYPE index, 0 = none; MAP-ECON-1 E2, docs/sim/forestry.md 4)
                 [stream 126].
    Figures are listed in ASCENDING id (the registry seal sorts them; ids are unique).

The vectors cover only figures (types, goods, classes, groups empty: the C++ test calls
TypeRegistry::digest_of with empty spans). Usage:
    figure_digest_ref.py --write fixtures/synthetic/sim/figure_digest_vectors.txt
    figure_digest_ref.py --check fixtures/synthetic/sim/figure_digest_vectors.txt
"""
import struct
import sys

MASK = (1 << 64) - 1


def fnv(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & MASK
    return h


def figure_bytes(f):
    id_, speed, cap, layer, load, bew, rng, hull = f[:8]
    nav = f[8] if len(f) > 8 else None
    route = f[9] if len(f) > 9 else 0
    ware = f[10] if len(f) > 10 else ''   # stream 135: the ware of an ore symbol (tagged, only when set)
    present = hull is not None
    return (struct.pack('<7I', id_, speed, cap, layer, load, bew, rng)
            + struct.pack('<B', 1 if present else 0)
            + struct.pack('<q', hull if present else 0)
            + struct.pack('<B', 1 if nav is not None else 0)
            + struct.pack('<7I', *(nav if nav is not None else (0,) * 7))
            + struct.pack('<I', route)
            + ((struct.pack('<B', 1) + struct.pack('<Q', len(ware)) + ware.encode()) if ware else b''))


def digest(figures):
    assert [f[0] for f in figures] == sorted(f[0] for f in figures), "ascending id"
    d = struct.pack('<Q', 0) * 3          # no types, goods, classes
    d += struct.pack('<Q', len(figures))
    d += b''.join(figure_bytes(f) for f in figures)
    d += struct.pack('<Q', 0)             # no groups
    d += struct.pack('<Q', 0)             # no stage table (stream 128, L3)
    return fnv(d)


# (id, speed_q24, capacity_88, layer, load_ticks, bewohntype, range_88, hull or None)
BASE = (1502, 0x0400, 0, 2, 0, 113, 4096, 397)


def with_(i, v):
    f = list(BASE)
    f[i] = v
    return tuple(f)


CASES = [
    ('hull_positive', [BASE]),
    ('hull_absent', [with_(7, None)]),
    ('hull_zero', [with_(7, 0)]),
    ('hull_shipped_negative_391', [(391, 0x0400, 0, 2, 0, 113, 4096, -27487789056)]),
    ('hull_edge_positive', [with_(7, (1 << 39) - 1)]),
    ('hull_edge_negative', [with_(7, -((1 << 39) - 1))]),
    ('changed_id', [with_(0, 1503)]),
    ('changed_speed', [with_(1, 0x0401)]),
    ('changed_capacity', [with_(2, 1)]),
    ('changed_layer', [with_(3, 3)]),
    ('changed_load_ticks', [with_(4, 1)]),
    ('changed_bewohntype', [with_(5, 114)]),
    ('changed_range', [with_(6, 4097)]),
    ('nav_present', [BASE + ((154618822, 472446402, 6000, 4, 2, 0, 1280),)]),
    ('nav_changed_speed', [BASE + ((154618823, 472446402, 6000, 4, 2, 0, 1280),)]),
    ('nav_changed_turn', [BASE + ((154618822, 472446403, 6000, 4, 2, 0, 1280),)]),
    ('nav_changed_hp', [BASE + ((154618822, 472446402, 6001, 4, 2, 0, 1280),)]),
    ('nav_changed_hold_a', [BASE + ((154618822, 472446402, 6000, 5, 2, 0, 1280),)]),
    ('nav_changed_hold_b', [BASE + ((154618822, 472446402, 6000, 4, 3, 0, 1280),)]),
    ('nav_changed_class', [BASE + ((154618822, 472446402, 6000, 4, 2, 1, 1280),)]),
    ('nav_changed_stamp', [BASE + ((154618822, 472446402, 6000, 4, 2, 0, 1281),)]),
    # stream 126: the route (figure 102's MAEHER = 42 = 0x2a, and one value more)
    ('route_maeher', [(102, 33554, 256, 2, 6500, 0, 1792, None, None, 42)]),
    ('route_changed', [(102, 33554, 256, 2, 6500, 0, 1792, None, None, 43)]),
    # stream 135: an ore symbol's ware (figure 1562 = SALZSTEIN) and a different ware
    ('ware_salz', [(1562, 33554, 256, 2, 6500, 145, 1792, None, None, 0, 'SALZSTEIN')]),
    ('ware_gold', [(1562, 33554, 256, 2, 6500, 145, 1792, None, None, 0, 'GOLD')]),
    ('two_ascending', [(391, 0x0400, 0, 2, 0, 113, 4096, -27487789056), BASE]),
    ('three_ascending', [(391, 0x0400, 0, 2, 0, 113, 4096, -27487789056),
                         (717, 0x0200, 0, 1, 0, 97, 1792, None), BASE]),
]


def render():
    out = ['# Figure-type digest vectors, generated by tools/figure_digest_ref.py (independent of',
           '# the C++ serializer). case <name> <fnv64 hex>, then one "fig id speed cap layer load',
           '# bewohntype range hull" line per figure in ascending id; hull "-" is absent.']
    for name, figs in CASES:
        out.append('')
        out.append('case %s %016x' % (name, digest(figs)))
        for f in figs:
            base = ' '.join('-' if v is None else str(v) for v in f[:8])
            nav = (' nav ' + ' '.join(str(v) for v in f[8])) if len(f) > 8 and f[8] is not None else ''
            route = (' route %d' % f[9]) if len(f) > 9 and f[9] else ''
            ware = (' ware %s' % f[10]) if len(f) > 10 and f[10] else ''
            out.append('fig ' + base + nav + route + ware)
    return '\n'.join(out) + '\n'


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--write':
        open(sys.argv[2], 'w').write(render())
        return 0
    if len(sys.argv) == 3 and sys.argv[1] == '--check':
        ok = open(sys.argv[2]).read() == render()
        print('figure digest vectors', 'agree' if ok else 'DIFFER')
        digests = [digest(f) for _, f in CASES]
        assert len(set(digests)) == len(digests), 'every case must have its own identity'
        return 0 if ok else 1
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main())
