#!/usr/bin/env python3
"""Independent SCRIPT2 extension of the retained published-128 transcription.

The old file and fixtures stay unchanged. Reuse its independent data/scheduler
model, append empty permission rows to players, transcribe the new header and
script scope here. No C++ encoder or generated C++ output is used.
"""
import argparse
import importlib.util
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('published128_reference', ROOT/'tools/sim_stream.py')
old = importlib.util.module_from_spec(spec)
spec.loader.exec_module(old)
base_player_bytes = old.player_bytes


def player_bytes(player):
    return base_player_bytes(player) + old.u64(0)  # SCRIPT2: no override rows in these fixtures


def script_bytes(context):
    # source length0; four fixed safety limits; policy1; context/fault;
    # cursor,lastarg,faultoffset,lastwork; empty signals and actual bindings.
    return (old.u64(0) + struct.pack('<4I',1048576,65536,128,100000) + old.u32(4096) + b'\0' + old.u32(2)
            + bytes([context,0]) + bytes(16) + old.u64(0) + old.u64(0))


def state_hash(registry,cursor,ms,last_tick,players,full_tick=False,islands=None,
               world=(0,0),steps=0,step_ms=0,monopoly=None,walk_digest=0):
    # Constructor worlds (G0/G1) deliberately have Missing context. Synthetic
    # ScenarioSpec builders (S5/S6) explicitly declare script-free context.
    context = 0 if islands is None else 1
    header = (old.u32(130) + b'SCRIPT2\0' + old.u32(5) + old.config_bytes()
              + old.rules_bytes(full_tick) + old.u64(registry) + old.u64(walk_digest)
              + old.u64(22) + b''.join(old.string(b'default') for _ in range(22)))
    clock = old.u64(ms)+old.u64(last_tick)+old.u64(2048)+old.u64(old.rng_table_digest())+old.u32(cursor)
    islands = old.islands_digest({}) if islands is None else islands
    assert ms < 450000 and step_ms < 30000
    ids = old.fnv(old.u64(1)*4 + old.u32(ms & old.M32)
                  + old.u32(step_ms if steps else 0) + old.u8(0))
    t = (ms >> 1) & old.M32
    figures = old.figures_digest(t & 0xffff if steps else 0)
    timers = old.timers_monopoly_digest(t if steps else 0, monopoly if steps else None)
    hashes = [old.fnv(header),old.fnv(clock),old.fnv(old.settings_bytes(*world)),ids,
              players,islands,figures,old.fnv(old.u64(0)),timers,old.fnv(script_bytes(context))]
    return old.fnv(b''.join(old.u64(h) for h in hashes))


old.player_bytes = player_bytes
old.state_hash = state_hash


def values():
    return old.values()


class Contracts(unittest.TestCase):
    def test_literal_default_scope(self):
        self.assertEqual(script_bytes(1).hex(),
            '0000000000000000000010000000010080000000a08601000010000000020000000100'
            '0000000000000000000000000000000000000000000000000000000000000000')
        self.assertEqual(len(script_bytes(0)),67)
        self.assertNotEqual(old.fnv(script_bytes(0)),old.fnv(script_bytes(1)))
    def test_schema_identity_is_not_just_integer130(self):
        self.assertEqual((old.u32(130)+b'SCRIPT2\0'+old.u32(5)).hex(),
                         '82000000534352495054320005000000')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--self-test',action='store_true')
    args=parser.parse_args()
    if args.self_test:
        result=unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Contracts))
        return 0 if result.wasSuccessful() else 1
    computed=values()
    for name,value in computed.items(): print(f'{name:24} {value:016x}')
    if not args.check: return 0
    path=ROOT/'fixtures/synthetic/sim/goldens_script2.txt'
    found={}
    for line in path.read_text().splitlines():
        parts=line.split('#')[0].split()
        if len(parts)==4: found[parts[0]]=(int(parts[2]),int(parts[3],16))
    required=set(computed)-{'empty_registry','empty_world'}
    bad=required-set(found)
    for name in sorted(bad): print('missing independently transcribed golden: '+name)
    for name in required & set(found):
        if found[name]!=(130,computed[name]):
            print('SCRIPT2 transcription differs: '+name); bad.add(name)
    return 1 if bad else 0


if __name__=='__main__': raise SystemExit(main())
