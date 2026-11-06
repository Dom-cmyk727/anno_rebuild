#!/usr/bin/env python3
"""Deterministic INTEGER reference of the original class-0x3000 (mine/quarry) terrain helper
AnnoGame.dll 0x10008a80 (MAP-EXTRACT-1 X1; docs/sim/salt-extraction.md section 11).

  accepts(w, h, table, heights, rotation, offset) -> bool

w, h        the type's footprint bytes (type +6, +7), 1..255 tiles
table       the HIGHMAP bytes, 2h rows x 2w columns, row-major (type +0x234)
heights     the vertex grid of the footprint window: heights[z][x], z in 0..H', x in 0..W'
            where (W', H') = (w, h) for rotation 0 and 2, (h, w) for rotation 1 and 3
rotation    0..3
offset      the fourth argument: the window's `lo` (the lowest cell minimum & 0xf0, the ordinary analysis'
            mode-2 result for flag word 0x3096; NOT the highest); only its low byte is used

Rule (D, confirmed against the original bytes by tools/mine_terrain_emulate.py):
for every table cell k = (row r, column c) the sample point is in quarter tiles
  rotation 0: X = 1 + 2c,        Z = 1 + 2r
  rotation 1: X = 1 + 2r,        Z = 4w - 1 - 2c
  rotation 2: X = 4w - 1 - 2c,   Z = 4h - 1 - 2r
  rotation 3: X = 4h - 1 - 2r,   Z = 1 + 2c
and the terrain height there is the original's triangle interpolation of the four vertices
a=(x,z) b=(x+1,z) c=(x,z+1) d=(x+1,z+1) of tile x = X // 4, z = Z // 4 (diagonal b-c), in
quarter units (all weights are multiples of 1/4, so the value is an exact integer / 4):
  fraction (1,1) quarters: (2a + b + c) / 4        (3,3): (2d + b + c) / 4
  fraction (1,3): (b + 3c) / 4                     (3,1): (3b + c) / 4
The site is REFUSED as soon as  table[k] <= trunc(height) - (offset & 0xff)  (signed compare);
accepted when no cell refuses. Heights are bytes, so trunc is a floor division.
"""


def sample_point(w, h, rotation, r, c):
    if rotation == 0:
        return 1 + 2 * c, 1 + 2 * r
    if rotation == 1:
        return 1 + 2 * r, 4 * w - 1 - 2 * c
    if rotation == 2:
        return 4 * w - 1 - 2 * c, 4 * h - 1 - 2 * r
    if rotation == 3:
        return 4 * h - 1 - 2 * r, 1 + 2 * c
    raise ValueError('rotation')


def height4(heights, X, Z):
    """4 x the interpolated height at quarter-tile point (X, Z), both odd."""
    x, z = X // 4, Z // 4
    fx, fz = X % 4, Z % 4
    a, b = heights[z][x], heights[z][x + 1]
    c, d = heights[z + 1][x], heights[z + 1][x + 1]
    if (fx, fz) == (1, 1):
        return 2 * a + b + c
    if (fx, fz) == (3, 3):
        return 2 * d + b + c
    if (fx, fz) == (1, 3):
        return b + 3 * c
    if (fx, fz) == (3, 1):
        return 3 * b + c
    raise ValueError('sample points are odd quarters')


def placement_accepts(w, h, table, heights, rotation):
    """The terrain part of a placement of a 0x3096 type: the ordinary window analysis gives
    lo = min over the footprint cells of (min of the cell's four vertices & 0xf0); the helper runs with
    offset = lo (flag word 0x3096 & 3 = 2); step 7 of the island function then needs lo >= 32 (INHERITED
    from tools/placement_ref.py and docs/re/ship-founding.md section 10.2 step 7, not re-derived here)."""
    ww, hh = (w, h) if rotation % 2 == 0 else (h, w)
    cellmin = [min(heights[z][x], heights[z][x + 1], heights[z + 1][x], heights[z + 1][x + 1]) & 0xF0
               for z in range(hh) for x in range(ww)]
    lo = min(cellmin)
    return accepts(w, h, table, heights, rotation, lo) and lo >= 32


def accepts(w, h, table, heights, rotation, offset):
    rows, cols = 2 * h, 2 * w
    assert len(table) == rows * cols
    off = offset & 0xFF
    for r in range(rows):
        for c in range(cols):
            X, Z = sample_point(w, h, rotation, r, c)
            ww, hh = (w, h) if rotation % 2 == 0 else (h, w)
            assert 0 < X < 4 * ww and 0 < Z < 4 * hh, 'sample point outside the footprint window'
            if table[r * cols + c] <= height4(heights, X, Z) // 4 - off:
                return False
    return True


# --- deposit load (D: AnnoWelt.dll deposit class 0x100637a0, load method 0x100634e0) -----------
KTON = 256


def deposit_load(amount_tons, goods, flag=1):
    """Initial state of a deposit loaded from an OCCUR ENTRY2 (docs/sim/salt-extraction.md section 12).

    The constructor sets remaining = 0x7d000 (2,000 t) and the reserve flag = 1; the load method then
    sets remaining = (u16 amount) << 8 (the file amount in tons, 8.8). For an ore goods id 1..8 with the
    flag set and remaining < 0x64000 (1,600 t) it resets remaining to 0x7d000 (2,000 t). Returns
    (remaining_88, flag); the flag is never cleared by the load.
    """
    remaining = (amount_tons & 0xFFFF) << 8
    if 1 <= goods <= 8 and flag and remaining < 0x64000:
        remaining = 0x7D000
    return remaining, flag


# --- vector files ------------------------------------------------------------------------------
import sys  # noqa: E402


def _kv(tokens):
    out = {}
    for t in tokens:
        if '=' not in t:
            raise ValueError('bad token %r' % t)
        k, v = t.split('=', 1)
        out[k] = v
    return out


def run(path):
    """Executes the vectors; prints '<n> vectors' first. 0 all agree, 1 a disagreement, 2 malformed or none."""
    n = bad = 0
    try:
        lines = open(path).read().splitlines()
    except OSError as e:
        print('cannot read %s: %s' % (path, e))
        return 2
    results = []
    for ln, line in enumerate(lines, 1):
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        try:
            lhs, rhs = line.split('=>')
            toks = lhs.split()
            kind, name = toks[0], toks[1]
            kv = _kv(toks[2:])
            if kind in ('terrain', 'place'):
                w, h, rot = int(kv['w']), int(kv['h']), int(kv['rot'])
                off = int(kv.get('off', 0))
                table = [int(x) for x in kv['table'].split(',')]
                heights = [[int(x) for x in row.split(',')] for row in kv['heights'].split('/')]
                if kind == 'place':
                    got = 'accept' if placement_accepts(w, h, table, heights, rot) else 'refuse'
                else:
                    got = 'accept' if accepts(w, h, table, heights, rot, off) else 'refuse'
                want = rhs.strip()
                if want not in ('accept', 'refuse'):
                    raise ValueError('result must be accept or refuse')
                ok = got == want
                detail = 'expected %s, got %s' % (want, got)
            elif kind == 'deposit':
                rem, flag = deposit_load(int(kv['amount']), int(kv['goods']), int(kv['flag']))
                want = _kv(rhs.split())
                ok = int(want['remaining']) == rem and int(want['flag']) == flag
                detail = 'expected %s, got remaining=%d flag=%d' % (rhs.strip(), rem, flag)
            else:
                raise ValueError('unknown kind %r' % kind)
        except (ValueError, KeyError, IndexError, AssertionError) as e:
            print('line %d: malformed: %s' % (ln, e))
            return 2
        n += 1
        if not ok:
            bad += 1
            results.append('line %d (%s): %s' % (ln, name, detail))
    if n == 0:
        print('no vectors')
        return 2
    print('%d vectors' % n)
    for r in results:
        print('DISAGREE', r)
    return 1 if bad else 0


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('usage: mine_terrain_ref.py VECTORS')
        sys.exit(2)
    sys.exit(run(sys.argv[1]))
