#!/usr/bin/env python3
"""Independent reference for the goal cell's key nibble (MAP-POP-CLOSE-2 K2, OQ-463).

Written from docs/re/population.md section 19 (AnnoWelt.dll: the goal pass of the search set-up
0x10003d90, the goal loop 0x10003fb6..0x1000415f whose last store is `or word [cell], ax` at
0x1000414b with ax from 0x10003aa0; the entry rule `(dest.low & geom) != 0`, 0x10006a81 and its
three siblings) without reading the C++ module. It runs over a vector file
(fixtures/synthetic/sim/walk_goal_vectors.txt) whose values were derived by hand before any C++.

CONTRACT
  keys(mask9, col, row): the tile's 3 x 3 sub-cells, bit (col + 3 * row) of mask9 set = BLOCKED. The
    sub-cell (col, row) is the seed and counts as reached whether or not it is blocked; free
    sub-cells are flooded through their four neighbours (up, left, right, down). The result has
    0x8000 if any top-row sub-cell was reached, 0x1000 for the left column, 0x2000 for the bottom
    row, 0x4000 for the right column (the same nibble as a start cell).
  cell(low, mask9, col, row): the goal cell's low word after the goal pass is low | keys. A step is
    allowed into it iff (low' & geom) != 0 with geom = 0x2111 for a step UP, 0x8444 DOWN, 0x4888
    LEFT, 0x1222 RIGHT (a step up enters from the bottom edge, so it needs the bottom key).

Vector grammar (one record per line, '#' comments):
  keys NAME mask9= col= row= => keys=
  cell NAME low= mask9= col= row= => low= up= down= left= right=
Exit codes: 0 all vectors agree; 1 a vector disagrees; 2 malformed input, a missing file or zero vectors.
"""
import sys

GEOM = {'up': 0x2111, 'down': 0x8444, 'left': 0x4888, 'right': 0x1222}


class Malformed(Exception):
    pass


def keys(mask9, col, row):
    if not (0 <= col < 3 and 0 <= row < 3 and 0 <= mask9 < 512):
        raise Malformed('out of range')
    reached = {(col, row)}
    todo = [(col, row)]
    while todo:
        x, y = todo.pop()
        for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < 3 and 0 <= ny < 3 and (nx, ny) not in reached and not (mask9 >> (nx + 3 * ny)) & 1:
                reached.add((nx, ny))
                todo.append((nx, ny))
    k = 0
    for x, y in reached:
        if y == 0:
            k |= 0x8000
        if x == 0:
            k |= 0x1000
        if y == 2:
            k |= 0x2000
        if x == 2:
            k |= 0x4000
    return k


def run_vector(kind, kv):
    mask9, col, row = int(kv['mask9'], 0), int(kv['col']), int(kv['row'])
    k = keys(mask9, col, row)
    if kind == 'keys':
        return {'keys': '0x%04x' % k}
    low = int(kv['low'], 0) | k
    out = {'low': '0x%04x' % low}
    for d, g in GEOM.items():
        out[d] = '1' if low & g else '0'
    return out


KEYS = {'keys': {'keys'}, 'cell': {'low', 'up', 'down', 'left', 'right'}}


def parse_kv(parts):
    kv = {}
    for p in parts:
        if '=' not in p:
            raise Malformed('not key=value: ' + p)
        k, v = p.split('=', 1)
        kv[k] = v
    return kv


def run(path):
    try:
        text = open(path).read()
    except OSError:
        print('cannot read', path)
        return 2
    nvec, bad = 0, 0
    for n, raw in enumerate(text.split('\n'), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        where = '%s:%d' % (path, n)
        try:
            kind = parts[0]
            if kind not in KEYS or '=>' not in parts:
                raise Malformed('unknown record or no =>')
            k = parts.index('=>')
            kv, exp = parse_kv(parts[2:k]), parse_kv(parts[k + 1:])
            if not exp or not set(exp) <= KEYS[kind]:
                raise Malformed('empty or unknown result key')
            got = run_vector(kind, kv)
            nvec += 1
            for key, want in exp.items():
                have = got.get(key, '<absent>')
                if have != want:
                    print('%s: %s: %s expected %s, reference computed %s' % (where, parts[1], key, want, have))
                    bad += 1
        except (Malformed, KeyError, ValueError, IndexError) as exc:
            print('%s: malformed: %r' % (where, exc))
            return 2
    if nvec == 0:
        print('no vectors in', path)
        return 2
    print('%d vectors, %d disagreements' % (nvec, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('usage: walk_goal_ref.py VECTORS', file=sys.stderr)
        sys.exit(2)
    sys.exit(run(sys.argv[1]))
