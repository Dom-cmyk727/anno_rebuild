#!/usr/bin/env python3
"""Independent oracle for the [BAUSTUFEN] stage table (MAP-POP-LIVE-1 L3).

Reads Data/BGruppen.dat [BAUSTUFEN] (`Stufe: NAME, group, count`, `Wissen: NAME, n`) and the stage-name pointer
table of AnnoGame.dll at 0x10024bd0 with a small stdlib PE reader; no C++ and no table copied from the engine.
Output (fixtures/synthetic/sim/baustufen_table.txt): one line per Stufe, ascending by stage index:
  stage <index> <name> group=<g> wissen=<w> count=<c>
--write FILE stores it, --check FILE compares; exit 1 on a difference, 2 on unreadable or empty input.
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
    stage = {cstr(u32(0x10024BD0 + 4 * i)): i for i in range(32)}
    text = open(os.path.join(game, 'Data', 'BGruppen.dat'), 'rb').read().decode('latin-1')
    sec, rec, wissen = None, {}, {}
    for raw in text.splitlines():
        line = raw.split(';', 1)[0].strip()
        if not line:
            continue
        if line.startswith('['):
            sec = line
            continue
        if sec != '[BAUSTUFEN]' or ':' not in line:
            continue
        key, val = (x.strip() for x in line.split(':', 1))
        f = [x.strip() for x in val.split(',')]
        if key == 'Stufe':
            rec[stage[f[0]]] = (f[0], int(f[1]), int(f[2]))
        elif key == 'Wissen':
            wissen[stage[f[0]]] = int(f[1])
    return ['stage %d %s group=%d wissen=%d count=%d' % (i, rec[i][0], rec[i][1], wissen.get(i, 0), rec[i][2])
            for i in sorted(rec)]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('game_dir')
    ap.add_argument('--write')
    ap.add_argument('--check')
    a = ap.parse_args()
    try:
        lines = build(a.game_dir)
    except (OSError, KeyError, ValueError, IndexError) as exc:
        print('cannot build the table:', exc, file=sys.stderr)
        return 2
    print('%d stages' % len(lines))
    if not lines:
        print('empty result', file=sys.stderr)
        return 2
    if a.write:
        open(a.write, 'w').write('# [BAUSTUFEN] stage table from tools/baustufen_ref.py (independent of the C++ reader)\n'
                                 + '\n'.join(lines) + '\n')
    if a.check:
        want = [l for l in open(a.check).read().split('\n') if l and not l.startswith('#')]
        if want != lines:
            print('differs from', a.check, file=sys.stderr)
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
