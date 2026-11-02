#!/usr/bin/env python3
"""Independent byte transcription of a POPULATED NavMotion (MAP-NAV-3 N4; stream 123,
docs/sim/ship-navigation.md 12.5 and 12.14).

The C++ writer is `fields(archive, NavMotion)` in state_fields.hpp. This script writes the same
bytes from the field list of the specification (little endian; an optional is one presence byte and
the payload; an enum is one byte; a boolean one byte; a sequence is a u64 count and its elements),
not from the C++ code. The fixture fixtures/synthetic/sim/nav/nav_state_vectors.txt holds the
structured states AND the bytes; the C++ test builds each state from the structured fields,
serialises it with the production archive, and asserts byte equality, then loads a world that holds
the state and checks that it advances exactly like the continuous one. A round trip alone cannot
detect a field that both the writer and the reader forget.

There is no per-figure digest stage in the stream (the figure bytes are hashed inside the figures
scope with the rest of the world), so the oracle is the byte string itself.

Usage:
  nav_state_ref.py write <vectors.txt>   regenerate the fixture
  nav_state_ref.py check <vectors.txt>   the fixture equals this script's output (exit 1 otherwise)
  nav_state_ref.py selftest              hand-derived anchor bytes and deliberately wrong writers
"""
import struct
import sys

UNIT_Q32 = 2 ** 32
IDLE_SPEED = 42949672                       # f32(0.01) * 2^32
SCALE = 500
POS_UNITS_PER_Q8 = SCALE << 24              # pos_u per Q24.8 unit: 500 * 2^24
SHIP_SPEED = 154618816                      # type 1502, 0.036 tile per stepper unit
TURN_STEP = 472446400                      # type 1502
DIAG_STEP = 11863283 << 9                  # L_d in q32

KIND = {'Idle': 0, 'Turn': 1, 'TurnPhase': 2, 'Move': 3}


def u8(x): return struct.pack('<B', x)
def u16(x): return struct.pack('<H', x)
def u32(x): return struct.pack('<I', x)
def u64(x): return struct.pack('<Q', x)
def i32(x): return struct.pack('<i', x)
def i64(x): return struct.pack('<q', x)


def opt(v, enc):
    return u8(0) if v is None else u8(1) + enc(v)


def encode(s, mut=None):
    """The bytes of one NavMotion state `s` (a dict)."""
    out = b''
    out += opt(s['order'], lambda o: u64(o[0]) + u8(1 if o[1] else 0) + i32(o[2]) + i32(o[3]))
    out += opt(s['ticket'], u64)
    g = s['seg']
    out += u8(KIND[g[0]]) + u8(g[1]) + u32(g[2]) + u32(g[3]) + u64(g[4])
    out += u64(len(s['nodes']))
    for n in s['nodes']:
        out += u8(KIND[n[0]]) + u8(n[1]) + u8(n[2]) + u64(n[3]) + u32(n[4])
    out += u16(s['last'])
    if mut == 'swap_pos':
        out += i64(s['pos_u'][1]) + i64(s['pos_u'][0])
    else:
        out += i64(s['pos_u'][0]) + i64(s['pos_u'][1])
    if mut != 'drop_res24':
        out += u32(s['res24'][0]) + u32(s['res24'][1])
    if mut != 'drop_health':
        out += opt(s['health'], u32)       # stream 123
    return out


def pos_u(q8, fraction):
    """pos_u of a position given as Q24.8 plus a fraction of one Q8 unit (0 <= fraction < 500 * 2^24)."""
    assert 0 <= fraction < POS_UNITS_PER_Q8
    return q8 * POS_UNITS_PER_Q8 + fraction


def states():
    out = []
    tile = lambda t: t * 256 + 128

    def add(name, note, pos_q8, dir16, **kw):
        s = dict(name=name, note=note, pos_q8=pos_q8, dir16=dir16, order=None, ticket=None,
                 seg=('Idle', 0, 100, IDLE_SPEED, IDLE_SPEED * SCALE), nodes=[], last=0,
                 res24=(0, 0), health=None, fraction=(0, 0))
        s.update(kw)
        s['pos_u'] = (pos_u(pos_q8[0], s['fraction'][0]), pos_u(pos_q8[1], s['fraction'][1]))
        out.append(s)

    diag_nodes = [('Turn', 0, 4, TURN_STEP, IDLE_SPEED),
                  ('Move', 4, 0, 3 * UNIT_Q32, SHIP_SPEED),
                  ('Move', 5, 0, 2 * DIAG_STEP, SHIP_SPEED)]
    # A diagonal (SE, dir8 3) movement of factor 87 with 2^31 of 500ths left, a routed order, a
    # turn node and two movement nodes (an orthogonal run of 3 and a diagonal run of 2), `last` 16
    # ticks before the u16 wrap, a residue on both axes and a fractional position.
    add('diag_moving_full', 'moving diagonal, residue and position fraction, routed order, pending turn + movement nodes, last near wrap, health at max',
        (tile(30), tile(100)), 6,
        order=(7, True, tile(60), tile(100)),
        seg=('Move', 3, 87, SHIP_SPEED, 2 ** 31),
        nodes=diag_nodes, last=0xFFF0, res24=(12345, 0xFFFFFF),
        fraction=(123456789, POS_UNITS_PER_Q8 - 1), health=6000)
    add('waiting_unrouted', 'accepted order waiting for its planning slot: unrouted, with its ticket, no plan, idle segment',
        (tile(50), tile(120)), 8,
        order=(9, False, tile(80), tile(120)), ticket=8, last=5, health=6000)
    add('diag_moving_partial', 'the first state at 1500 of 6000 hit points and another residue',
        (tile(30), tile(100)), 6,
        order=(7, True, tile(60), tile(100)),
        seg=('Move', 3, 73, SHIP_SPEED, 2 ** 31 + 12345),
        nodes=diag_nodes, last=0xFFF0, res24=(1, 2),
        fraction=(1, 2), health=1500)
    add('waiting_unknown_health', 'a waiting order of a ship whose health is unknown (an import without the field)',
        (tile(70), tile(140)), 8,
        order=(11, False, tile(100), tile(140)), ticket=3, last=65535, health=None)
    add('idle_zero_health', 'idle at 0 hit points (a legal value; the speed term is then the floor)',
        (tile(90), tile(160)), 8, last=300, health=0)
    return out


def render(mut=None):
    lines = ['# MAP-NAV-3 N4: populated NavMotion states and their bytes (stream 123), from tools/nav_state_ref.py.',
             '# state NAME | pos Q8X Q8Y | dir16 N | order ID ROUTED TX TY or none | ticket N or none',
             '# seg KIND DIR8 FACTOR SPEED REMAINING | node KIND DIR8 FACING LENGTH SPEED (repeated, next first)',
             '# last U16 | pos_u X Y | res24 A B | health N or unknown | hex BYTES (the NavMotion alone, optional prefix included)']
    for s in states():
        lines.append(f"state {s['name']}")
        lines.append(f"# {s['note']}")
        lines.append(f"pos {s['pos_q8'][0]} {s['pos_q8'][1]}")
        lines.append(f"dir16 {s['dir16']}")
        o = s['order']
        lines.append('order none' if o is None else f"order {o[0]} {1 if o[1] else 0} {o[2]} {o[3]}")
        lines.append('ticket none' if s['ticket'] is None else f"ticket {s['ticket']}")
        lines.append('seg ' + ' '.join(str(x) for x in s['seg']))
        for n in s['nodes']:
            lines.append('node ' + ' '.join(str(x) for x in n))
        lines.append(f"last {s['last']}")
        lines.append(f"pos_u {s['pos_u'][0]} {s['pos_u'][1]}")
        lines.append(f"res24 {s['res24'][0]} {s['res24'][1]}")
        lines.append('health unknown' if s['health'] is None else f"health {s['health']}")
        lines.append('hex ' + encode(s, mut).hex())
        lines.append('end')
    return '\n'.join(lines) + '\n'


def selftest():
    # Hand-derived anchor: the default idle motion (no order, no ticket, factor 100, 0.01 speed,
    # remaining 0.01 * 500, no nodes, last 0, positions 0, no residue, health unknown) is
    #   00 | 00 | 00 00 | 64 00 00 00 | 28 5C 8F 02 | 20 FE FF FF 04 00 00 00 | 00*8 | 00 00 |
    #   00*8 | 00*8 | 00*4 | 00*4 | 00
    idle = dict(order=None, ticket=None, seg=('Idle', 0, 100, IDLE_SPEED, IDLE_SPEED * SCALE),
                nodes=[], last=0, pos_u=(0, 0), res24=(0, 0), health=None)
    want = bytes.fromhex('00' '00' '0000' '64000000' '285c8f02' '20feffff04000000' + '00' * 8 + '0000' +
                         '00' * 8 + '00' * 8 + '00' * 4 + '00' * 4 + '00')
    assert encode(idle) == want, encode(idle).hex()
    assert len(want) == 1 + 1 + 2 + 4 + 4 + 8 + 8 + 2 + 16 + 8 + 1
    # health present adds 1 + 4 bytes (u8 flag then u32) at the end
    assert encode(dict(idle, health=0x01020304)) == want[:-1] + bytes.fromhex('01' '04030201')
    # a different health is a different byte string; so is every deliberately wrong writer
    base = {s['name']: encode(s) for s in states()}
    for mut in ('swap_pos', 'drop_res24', 'drop_health'):
        changed = [s['name'] for s in states() if encode(s, mut) != base[s['name']]]
        if not changed:
            print('selftest: mutant', mut, 'was not detected by any state')
            return 1
        print('selftest: mutant', mut, 'detected by', len(changed), 'states')
    # the states are the ones the brief names
    names = [s['name'] for s in states()]
    assert names[0] == 'diag_moving_full' and any(s['res24'] != (0, 0) for s in states())
    assert any(s['health'] is None for s in states()) and any(s['health'] not in (None, 6000) for s in states())
    return 0


def main(argv):
    if len(argv) == 2 and argv[1] == 'selftest':
        return selftest()
    if len(argv) == 3 and argv[1] == 'write':
        open(argv[2], 'w').write(render())
        return 0
    if len(argv) == 3 and argv[1] == 'check':
        if selftest():
            return 1
        if open(argv[2]).read() != render():
            print('nav state vectors differ from the transcription')
            return 1
        print('nav state vectors agree')
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
