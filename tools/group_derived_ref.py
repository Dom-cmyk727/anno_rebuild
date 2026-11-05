#!/usr/bin/env python3
"""Independent oracle for the derived population group tables (MAP-POP-IMPL-1 package C).

Reads Data/BGruppen.dat directly ([WAREN] Id/IdSec, [BGRUPPE] Ware/Force) and the goods names of
tools/sim_stream.py's transcription of AnnoGame's name tables; applies the loader rules of AnnoGame
0x1001486b-0x100148a6 (per Ware) and 0x10014afc-0x10014bd9 (post-pass) as written in
docs/sim/population-inputs.md 2.6-2.10 - code independent of the C++ (the same rules, second
implementation, compared slot for slot). Roles default to infra 0x4c..0x5a, tavern 0x4c, alias 0x1d.

derive(needs, id_sec, goods_count, roles) is importable by the synthetic rows.
Output: per group "group <name> n <n> goods_end <a> infra_end <b> forced <ids,> lookup <id:slot,...>"
(lookup lists only entries that are not the sentinel n). --write FILE / --check FILE as in the
sibling oracles.
"""
import argparse, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sim_stream  # noqa: E402  (goods_names only)

ROLES = (0x4C, 0x5A, 0x4C, 0x1D)


def derive(needs, id_sec, goods_count, roles=ROLES):
    """needs: [(ware, force)]; id_sec: {goods: substitute}. -> dict"""
    first, last, tavern, alias = roles
    n = len(needs)
    goods_end = 0
    for slot, (ware, _) in enumerate(needs):
        k = slot + 1
        if ware < first:
            goods_end = k
        elif ware > last and goods_end == 0:
            goods_end = k
    lookup = [n] * goods_count
    infra_end, forced = 0, set()
    for slot, (ware, force) in enumerate(needs):
        lookup[ware] = slot
        if first <= ware <= last:
            infra_end = slot + 1
            if force:
                forced.add(ware)
        if ware == tavern and lookup[alias] == n:
            lookup[alias] = slot
    for g in range(first, last + 1):
        sub = id_sec.get(g, 0)
        if lookup[g] == n and lookup[sub] != n:
            lookup[g] = lookup[sub]
    return dict(n=n, goods_end=goods_end, infra_end=max(infra_end, goods_end),
                forced=sorted(forced), lookup={i: s for i, s in enumerate(lookup) if s != n})


def parse(path):
    names = [x.decode('latin-1') for x in sim_stream.goods_names()]
    text = open(path, 'rb').read().decode('latin-1')
    sec, cur, id_sec = None, None, {}
    groups, group = [], None
    for raw in text.splitlines():
        line = raw.split(';', 1)[0].strip()
        if not line:
            continue
        if line.startswith('['):
            sec = line
            continue
        if ':' not in line:
            continue
        key, val = (x.strip() for x in line.split(':', 1))
        if sec == '[WAREN]':
            if key == 'Id':
                cur = names.index(val)
            elif key == 'IdSec':
                id_sec[cur] = names.index(val)
        elif sec == '[BGRUPPE]':
            if key == 'BGruppeNr':
                group = dict(name=val, needs=[])
                groups.append(group)
            elif key == 'Ware':
                group['needs'].append([names.index(val), False])
            elif key == 'Force':
                group['needs'][-1][1] = val.upper() == 'TRUE'
    return names, id_sec, groups


def build(game):
    names, id_sec, groups = parse(os.path.join(game, 'Data', 'BGruppen.dat'))
    out = []
    for g in groups:
        d = derive([tuple(x) for x in g['needs']], id_sec, len(names))
        out.append('group %s n %d goods_end %d infra_end %d forced %s lookup %s' % (
            g['name'], d['n'], d['goods_end'], d['infra_end'],
            ','.join(map(str, d['forced'])) or '-',
            ','.join('%d:%d' % kv for kv in sorted(d['lookup'].items())) or '-'))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('game_dir')
    ap.add_argument('--write')
    ap.add_argument('--check')
    a = ap.parse_args()
    lines = build(a.game_dir)
    for l in lines:
        print(l)
    if len(lines) != 5:
        print('expected five groups', file=sys.stderr)
        return 1
    if a.write:
        open(a.write, 'w').write('# Derived population group tables from tools/group_derived_ref.py '
                                 '(independent of the C++; roles = original defaults)\n' + '\n'.join(lines) + '\n')
    if a.check:
        want = [l for l in open(a.check).read().split('\n') if l and not l.startswith('#')]
        if want != lines:
            print('differs from', a.check, file=sys.stderr)
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
