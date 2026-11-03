#!/usr/bin/env python3
"""Independent oracle for the goods population metadata (MAP-POP-IMPL-1 package B).

Reads Data/BGruppen.dat [WAREN] (BauFirst, IdSec per Id) and the 34 stage-name pointers of
AnnoGame.dll at 0x10024bd0 with a small stdlib PE reader; no C++ and no table copied from the engine.
Output (the fixture fixtures/synthetic/sim/waren_metadata.txt):
  stage <index> <name>            for indices 0..31 (the compiled mapping the engine must equal)
  not_stage <index> <name>        for 32 and 33 (GOTT, SPIELER: refused by the engine)
  bau <goods name> <index>        every goods with a BauFirst key (23 in the shipped file)
  idsec <goods name> <goods name> every goods with an IdSec key (4)
--write FILE stores it, --check FILE compares; exit 1 on a difference.
"""
import argparse, os, struct, sys


def pe_reader(path):
    d = open(path, 'rb').read()
    e = struct.unpack_from('<I', d, 0x3C)[0]
    n = struct.unpack_from('<H', d, e + 6)[0]
    opt = struct.unpack_from('<H', d, e + 20)[0]
    base = struct.unpack_from('<I', d, e + 24 + 28)[0]
    so = e + 24 + opt
    secs = [struct.unpack_from('<8sIIII', d, so + 40 * i) for i in range(n)]

    def off(va):
        r = va - base
        for _, vs, a, rs, ro in secs:
            if a <= r < a + max(vs, rs):
                return ro + r - a
        raise ValueError(hex(va))

    def u32(va):
        return struct.unpack_from('<I', d, off(va))[0]

    def cstr(va):
        o = off(va)
        return d[o:d.index(b'\0', o)].decode('latin-1')
    return u32, cstr


def build(game):
    u32, cstr = pe_reader(os.path.join(game, 'AnnoGame.dll'))
    out = []
    for i in range(34):
        name = cstr(u32(0x10024BD0 + 4 * i))
        out.append(('stage %d %s' if i < 32 else 'not_stage %d %s') % (i, name))
    stage = {cstr(u32(0x10024BD0 + 4 * i)): i for i in range(32)}
    text = open(os.path.join(game, 'Data', 'BGruppen.dat'), 'rb').read().decode('latin-1')
    sec, cur, bau, sec_ids = None, None, [], []
    for raw in text.splitlines():
        line = raw.split(';', 1)[0].strip()
        if not line:
            continue
        if line.startswith('['):
            sec, cur = line, None
            continue
        if sec != '[WAREN]' or ':' not in line:
            continue
        key, val = (x.strip() for x in line.split(':', 1))
        if key == 'Id':
            cur = val
        elif key == 'BauFirst':
            bau.append('bau %s %d' % (cur, stage[val]))
        elif key == 'IdSec':
            sec_ids.append('idsec %s %s' % (cur, val))
    return out + bau + sec_ids


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('game_dir')
    ap.add_argument('--write')
    ap.add_argument('--check')
    a = ap.parse_args()
    lines = build(a.game_dir)
    nbau = sum(1 for l in lines if l.startswith('bau '))
    nsec = sum(1 for l in lines if l.startswith('idsec '))
    print('stages 32, bau %d, idsec %d' % (nbau, nsec))
    if nbau == 0 or nsec == 0:
        print('empty result', file=sys.stderr)
        return 1
    if a.write:
        open(a.write, 'w').write('# [WAREN] population metadata from tools/waren_ref.py (independent of the C++ reader)\n'
                                 + '\n'.join(lines) + '\n')
    if a.check:
        want = [l for l in open(a.check).read().split('\n') if l and not l.startswith('#')]
        if want != lines:
            print('differs from', a.check, file=sys.stderr)
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
