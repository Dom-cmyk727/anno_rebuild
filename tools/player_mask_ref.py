#!/usr/bin/env python3
"""Independent oracle for the imported player stage masks (MAP-POP-IMPL-1 package A).

Reads every scenario (.sze) under a game directory with the Python container parser (no C++), takes
each SPIELER block's HEAD record (108 bytes) and applies the rule of AnnoWelt 0x1001d389-0x1001d3df:
  mask = HEAD[0x48:0x4c] & 0x7fffff, | 1<<24 if HEAD[0x5c] bit3, | 1<<27 if bit2, | 1<<29 if bit4,
  | 1<<30 if bit5 (everything else dropped).
Prints one line per file: "<relative path> <player index>:<mask hex> ..." in file order, then the
histogram of masks over all heads. --write FILE stores the per-file lines (the fixture); --check FILE
compares. Exit 1 on a difference or when no scenario is found.
"""
import argparse, collections, os, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sze  # noqa: E402


def decode(head48, head5c):
    m = head48 & 0x7FFFFF
    for bit, out in ((0x08, 1 << 24), (0x04, 1 << 27), (0x10, 1 << 29), (0x20, 1 << 30)):
        if head5c & bit:
            m |= out
    return m


def scenario_files(game):
    root = os.path.join(game, 'Scenes')
    found = []
    for d, _, fs in os.walk(root):
        found += [os.path.join(d, f) for f in fs if f.lower().endswith('.sze')]
    return sorted(found)


def lines(game):
    out, hist = [], collections.Counter()
    for path in scenario_files(game):
        _, kids = sze.scene(open(path, 'rb').read())
        items = []
        for name, payload in kids:
            if name != 'SPIELER':
                continue
            idx = struct.unpack_from('<I', payload, 4)[0]
            for rn, _, rp in sze.tagged(payload[8:]):
                if rn == 'HEAD':
                    assert len(rp) >= 108, path
                    mask = decode(struct.unpack_from('<I', rp, 0x48)[0], rp[0x5C])
                    items.append('%d:%08x' % (idx, mask))
                    hist[mask] += 1
                    break
        out.append(os.path.relpath(path, os.path.join(game, 'Scenes')).replace(os.sep, '/') +
                   ' ' + ' '.join(items))
    return out, hist


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('game_dir')
    ap.add_argument('--write')
    ap.add_argument('--check')
    a = ap.parse_args()
    out, hist = lines(a.game_dir)
    if not out:
        print('no scenario found', file=sys.stderr)
        return 1
    text = '\n'.join(out) + '\n'
    print('files %d heads %d' % (len(out), sum(hist.values())))
    for m, n in sorted(hist.items()):
        print('  %08x x%d' % (m, n))
    if a.write:
        open(a.write, 'w').write('# Imported player stage masks per scenario, from tools/player_mask_ref.py '
                                 '(independent of the C++ importer). <file> <player index>:<mask>...\n' + text)
    if a.check:
        want = [l for l in open(a.check).read().split('\n') if l and not l.startswith('#')]
        if want != out:
            print('differs from', a.check, file=sys.stderr)
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
