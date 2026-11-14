#!/usr/bin/env python3
"""Independent transcription of the registry digest of ONE hand-built building type, with and without a
HIGHMAP table (stream 135, docs/sim/salt-extraction.md section 13).

The type is the `base` of src/sim/tests/types_test.cpp "registry digest changes with every field"; the
goods and class tables and everything after the types come from tools/sim_stream.py (the same independent
transcription that pins the empty registry). Only digest_type of src/sim/src/types.cpp is transcribed
here, field by field, little-endian, u64 counts, enums as u8, booleans as u8.

  highmap_digest_ref.py            prints the two digests
  highmap_digest_ref.py --check    exit 0 only if they equal the values the C++ test pins
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sim_stream as S  # noqa: E402

u8, u32, u64, string = S.u8, S.u32, S.u64, S.string


def u32s(v):
    return u64(len(v)) + b''.join(u32(x) for x in v)


def type_bytes(highmap=b''):
    d = u32(10) + u32(1) + u8(1)               # id, prodtype, cls = RuntimeClass::Workshop (1)
    d += u32(2) + u32(3) + u32(4) + u32(5)     # width, height, research_id, cost_gold
    d += u64(1) + u32(0x29) + u32(256)         # cost_goods: one entry
    d += u32(6) + u32(7) + u32(8) + u32(9) + u32(10) + u32(11)  # max_hp can_burn action_radius flags rubble service_mask
    d += u32s([12, 13])                        # running_cost
    d += u32(1)                                # statistic_kind (default 1)
    d += u32s([14]) + u32s([15])               # output_goods, input_goods
    d += u64(2) + u8(16) + u8(17)              # head_raw
    d += u64(2) + u32(1) + u32(3) + u32(5) + u32(9)   # entrances (x6, y6)
    d += u8(1)                                 # has_prod
    # digest_production
    d += u32(10) + string(b'HANDWERK') + u8(0)  # id, prodtype, kind = BuildingClass::Workshop (0)
    d += u64(1) + string(b'STOFFE') + u64(1) + string(b'WOLLE')
    d += u32s([1, 2]) + u32s([3, 4]) + u32s([5, 6])   # capacity, output_per_step, input_per_step
    d += u32(7) + u32(8)                       # time0_128, time1_128
    d += u32s([9, 10, 11]) + u32s([12, 13, 14]) + u32s([15, 16, 17, 18, 19, 20, 21, 22, 23])
    d += u32(24) + u32(25) + u32(26) + u32(27) + u32(28)  # stages stage_shift growth_period display_rate0/1
    # special_successor is false: nothing. highmap: only when declared (count, then the bytes)
    if highmap:
        d += u64(len(highmap)) + bytes(highmap)
    return d


def digests():
    names = S.goods_names()
    return (S.registry_digest(names, types=[type_bytes()]),
            S.registry_digest(names, types=[type_bytes(bytes([1, 2, 3, 4]))]))


PINNED = (0x7adcb2c0d02b1a70, 0x4cbafff0a755efe4)  # the values src/sim/tests/types_test.cpp pins

if __name__ == '__main__':
    without, with_table = digests()
    print('without %016x\nwith %016x' % (without, with_table))
    if '--check' in sys.argv:
        try:
            rows = S.goldens()
            held = tuple(rows[name] for name in
                         ('oracle_registry_highmap_absent', 'oracle_registry_highmap_present'))
            ok = ((without, with_table) == PINNED == tuple(row[1] for row in held)
                  and all(row[0] == S.STATE_HASH_VERSION for row in held)
                  and without != with_table)
        except (KeyError, OSError, ValueError) as error:
            print('cannot compare named highmap pins:', error)
            ok = False
        print('agree with the C++ pins' if ok else 'DISAGREE with the C++ pins')
        sys.exit(0 if ok else 1)
