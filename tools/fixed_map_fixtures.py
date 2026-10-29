"""Synthetic game folder for the fixed-map loader tests.

  python -I tools/fixed_map_fixtures.py

Writes fixtures/synthetic/fixed_map_game/. Not original bytes. Stdlib only.
"""
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / 'fixtures' / 'synthetic' / 'fixed_map_game'


def chunk(name, payload):
    field = name.encode('ascii') + b'\x00' * (12 - len(name))
    return field + struct.pack('<I', len(payload)) + payload


def scp(climate):
    head = struct.pack('<HHHHI', 0, 0, 32, 32, 0)
    info = bytes([climate]) + bytes(11)
    height = struct.pack('<HHBB', 0, 0, 32, 32) + bytes([20]) * (32 * 32)
    insel5 = head + chunk('INFO', info) + chunk('HEIGHT', height)
    return chunk('ANNO', chunk('INSEL5', insel5))


def base_sze():
    head = bytearray(32)
    struct.pack_into('<II', head, 0, 800, 600)
    head[8] = 4   # prop0, not the WorldSettings default
    head[9] = 9   # prop2, not the default 2
    head[0x0A] = 1
    head[0x0B] = 1  # prop10, in 0..2
    struct.pack_into('<H', head, 0x0C, 2550)
    player = bytearray(108)
    struct.pack_into('<H', player, 0, 8)       # kind
    struct.pack_into('<i', player, 4, 25000)   # money, not PlayerSpec's 10000
    player[8:12] = b'Base'
    player[0x4C] = 3
    tagged = b'HEAD\x00\x00' + struct.pack('<H', 108) + bytes(player)
    spieler = struct.pack('<II', 0, 0) + tagged
    scene = chunk('HEAD', bytes(head)) + chunk('SPIELER', spieler)
    return chunk('ANNO', chunk('SCENE', scene))


def main():
    (ROOT / 'Scenes' / 'Islands' / 'NORD').mkdir(parents=True, exist_ok=True)
    (ROOT / 'Scenes' / 'Islands' / 'DSCHUNGEL').mkdir(parents=True, exist_ok=True)
    (ROOT / 'Scenes' / 'base.sze').write_bytes(base_sze())
    (ROOT / 'Scenes' / 'Islands' / 'NORD' / 'N_A.SCP').write_bytes(scp(2))
    (ROOT / 'Scenes' / 'Islands' / 'DSCHUNGEL' / 'D_A.SCP').write_bytes(scp(4))
    print('wrote', ROOT)


if __name__ == '__main__':
    main()
