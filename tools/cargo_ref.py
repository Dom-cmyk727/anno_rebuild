#!/usr/bin/env python3
"""Independent reference for the manual ship -> own-warehouse unloading rule
(MAP-CARGO-1 U1; trace: docs/re/ship-cargo.md, contract: docs/sim/ship-cargo.md).

It transcribes, from the disassembly alone and with no C++ in sight:
  AnnoWelt.dll 0x100ac360   world slot +0x100, the "Trade" function, its UNLOAD arm (flag != 0) for two ships' owner
                            equal to the city's owner (the same-owner arm, 0x100ac91f-0x100accde)
  AnnoWelt.dll 0x1002da50 / 0x1002da90 / 0x1003b9a0   city stock add / remove, hold take (clear on empty)
  AnnoWelt.dll 0x1003f200   ship slot +0xc4 mode 0: the cities near the ship and the warehouse range test
and the REBUILD's adapted rule (plan_unload) that never loses cargo. `orig_unload` is kept only to show, per vector, what
the original's 32-bit register arithmetic does (evidence of the difference, never an engine expectation).

  cargo_ref.py selftest                  the rule's own checks (hand-derived numbers, independent of the vector file)
  cargo_ref.py check [vectors.txt]       run a vector file (default: the fixture); exit 0/1

Vector file (fixtures/synthetic/sim/cargo_vectors.txt), one `case` per block:
  goods_count N | cap U | stock G U | hold SHIP G U | req SHIP IDX G TONS -> ok U | reject REASON
  endhold SHIP G:U ... | endstock G U | orig RET G:U STOCK     (the original's result for the first request)
  range NAME ship TX TY bld X0 Y0 W H -> accept|reject
  tile QX QY -> TX TY                  (Q24.8 position to tile)
  cityset NAME ship TX TY owned X,Y ... -> in|out
Only the Python standard library is used.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_VECTORS = os.path.join(HERE, '..', 'fixtures', 'synthetic', 'sim', 'cargo_vectors.txt')

TON = 256                                  # raw units per ton (hold slot +8 / city stock are 8.8 tons)
M32 = 0xFFFFFFFF
DEFAULT_GOODS = 0x38                       # 0x1002da50 refuses g >= 0x38
DEFAULT_CAP = 50 * TON                     # 12800, city+0x624 for one market-class building (table 0x100dcc84[1] = 50)
HALF_WIDTHS = (5, 5, 5, 4, 3, 2)           # table built at 0x1003f217-0x1003f24e (radius 5), see docs/re/ship-cargo.md


# ---------------------------------------------------------------- the REBUILD rule (ADAPTATION of the original arm)
def plan_unload(holds, index, goods, tons, stock, cap, goods_count=DEFAULT_GOODS):
    """-> ('ok', transferred, new_holds, new_stock) or ('reject', REASON).
    holds: list of (goods, amount) in order; stock: dict goods -> units (missing = 0); cap: units of EVERY goods.
    Unbounded Python ints stand for the engine's checked u64 arithmetic (the caller never exceeds 2^64)."""
    if tons == 0:
        return ('reject', 'BadValue')
    if index >= len(holds):
        return ('reject', 'BadIndex')
    if goods == 0 or goods >= goods_count:
        return ('reject', 'UnsupportedGoods')
    hg, ha = holds[index]
    if hg != goods:
        return ('reject', 'HoldGoodsMismatch')
    free = max(0, cap - stock.get(goods, 0))
    if free == 0:
        return ('reject', 'StockFull')
    credit = min(tons * TON, ha, free)
    assert credit > 0                      # ha > 0 for a hold with a goods id (invariant), tons > 0, free > 0
    new_holds = list(holds)
    left = ha - credit
    new_holds[index] = (goods, left) if left else (0, 0)       # 0x1003b9a0: an emptied hold loses its goods id
    new_stock = dict(stock)
    new_stock[goods] = stock.get(goods, 0) + credit
    assert new_stock[goods] <= cap
    return ('ok', credit, new_holds, new_stock)


# ------------------------------------------------------------ the ORIGINAL's arithmetic (evidence only)
def _s32(v):
    v &= M32
    return v - (1 << 32) if v & 0x80000000 else v


def orig_unload(holds, index, goods, tons, stock, cap, goods_count=DEFAULT_GOODS):
    """Transcription of 0x100ac91f-0x100accde for a same-owner pair. -> (return, new_holds, new_stock).
    The hold amount is a u16 and the stock a u32 as in the original (callers keep hold amounts <= 0xFFFF)."""
    s = stock.get(goods, 0)
    hg, ha = holds[index]
    if hg != goods:                                  # 0x100ac90a: cmp eax, [esp+0x10]; jne -> return 0
        return 0, list(holds), dict(stock)
    esi = (tons & 0xFFFF) << 8                       # mov si, [rec+2]; shl esi, 8
    if esi > ha:                                     # 0x100ac936: cmp esi, eax; jle
        esi = ha
    edi = _s32(((0xFF - s) + cap) & M32 & 0xFFFFFF00)   # 0x100ac953-0x100ac961 (32-bit, then signed compares)
    if esi > edi:                                    # 0x100ac967: cmp esi, edi; jle
        esi = edi
    if esi == 0:                                     # 0x100ac96d: test esi, esi; jne
        return 0, list(holds), dict(stock)
    nh = list(holds)                                 # 0x100accbf: hold slot +0x10 (0x1003b9a0), take = esi signed
    if esi >= ha:                                    # jge: emptied -> amount 0, goods id 0
        nh[index] = (0, 0)
    else:
        nh[index] = (hg, (ha - esi) & 0xFFFF)
    ns = dict(stock)                                 # city slot +0x80 (0x1002da50)
    if goods < goods_count:
        e = esi & M32
        sum32 = (s + e) & M32
        if cap < sum32:                              # cmp ecx, edi; jae -> clamp when the sum is above the capacity
            e = (cap - s) & M32
        ns[goods] = (s + e) & M32
    return esi, nh, ns


# --------------------------------------------------------------------------------- range (0x1003f200, mode 0)
def tile_of(q):
    """Ship tile from a non-negative Q24.8 position: __ftol truncation of x/1.0 (0x100b5f90, divisor 0x100c4b18 = 1.0)."""
    return q >> 8


def axis_distance(p, lo, length):
    """0 when p lies inside [lo, lo + length - 1], else the distance to the nearer edge tile (0x1003f47f-0x1003f4cf)."""
    hi = lo + length - 1
    if lo <= p <= hi:
        return 0
    return min(abs(lo - p), abs(hi - p))


def in_range(ship_tile, rect, half=HALF_WIDTHS):
    tx, ty = ship_tile
    x0, y0, w, h = rect
    r = len(half) - 1
    dy = axis_distance(ty, y0, h)
    if dy > r:                                       # cmp ebx, 5; jg
        return False
    dx = axis_distance(tx, x0, w)
    return dx <= half[dy]                            # cmp ecx, [table + dy]; jg


def outline(ship_tile, half=HALF_WIDTHS):
    """The 24 (with repeats at k = 0) tiles whose owners make the city set (0x1003f297-0x1003f3b2)."""
    tx, ty = ship_tile
    pts = []
    for k, w in enumerate(half):
        pts += [(tx - w, ty - k), (tx + w, ty - k), (tx - w, ty + k), (tx + w, ty + k)]
    return pts


def city_near(ship_tile, owned_tiles, half=HALF_WIDTHS):
    own = set(owned_tiles)
    return any(p in own for p in outline(ship_tile, half))


# ------------------------------------------------------------------------------------------------- selftest
def selftest():
    n = 0

    def eq(a, b, what):
        nonlocal n
        n += 1
        if a != b:
            raise AssertionError(f'{what}: got {a!r}, expected {b!r}')

    # exact whole hold, wood 38 t into an empty stock
    r = plan_unload([(42, 9728)], 0, 42, 38, {}, DEFAULT_CAP)
    eq(r, ('ok', 9728, [(0, 0)], {42: 9728}), 'exact')
    # the table of half widths from the recurrence of 0x1003f217 (radius 5)
    tab = [0] * 8
    ecx, eax, edi, edx, ebx, esi = 0, 5, 25, 20, 10, 0
    while True:
        edi += -1 - esi
        tab[ecx] = eax
        if edi <= edx:
            tab[eax] = ecx
            ebx -= 2
            eax -= 1
            edx -= ebx
        ecx += 1
        esi += 2
        if not ecx <= eax:
            break
    eq(tuple(tab[:6]), HALF_WIDTHS, 'half widths')
    # partial by capacity: free 5 t of a 30 t hold
    r = plan_unload([(41, 7680)], 0, 41, 30, {41: 11520}, DEFAULT_CAP)
    eq(r, ('ok', 1280, [(41, 6400)], {41: 12800}), 'partial')
    # the original loses the excess of a fractional free space, the rebuild keeps it aboard
    o = orig_unload([(42, 768)], 0, 42, 3, {42: 12672}, DEFAULT_CAP)
    eq((o[0], o[1], o[2]), (256, [(42, 512)], {42: 12800}), 'orig fractional free space')
    r = plan_unload([(42, 768)], 0, 42, 3, {42: 12672}, DEFAULT_CAP)
    eq(r, ('ok', 128, [(42, 640)], {42: 12800}), 'rebuild fractional free space')
    # conservation on every accepted case of a small exhaustive sweep (hold, stock, capacity, request)
    for ha in (1, 255, 256, 257, 384, 768, 12800):
        for st in (0, 1, 255, 256, 12544, 12672, 12799, 12800, 13000, 13100):
            for tons in (1, 2, 50):
                r = plan_unload([(42, ha)], 0, 42, tons, {42: st}, DEFAULT_CAP)
                if r[0] != 'ok':
                    eq(r, ('reject', 'StockFull') if st >= DEFAULT_CAP else r, 'only full stock refuses here')
                    continue
                _, credit, nh, ns = r
                eq(ha + st, nh[0][1] + ns[42], f'conservation {ha} {st} {tons}')
                eq(credit, ha - nh[0][1], 'debit == credit')
                if nh[0][1] == 0:
                    eq(nh[0][0], 0, 'emptied hold clears its goods id')
                eq(ns[42] <= DEFAULT_CAP, True, 'no stock above capacity')
                if st <= DEFAULT_CAP:             # ordinary state: the credit is the original's credit
                    o = orig_unload([(42, ha)], 0, 42, tons, {42: st}, DEFAULT_CAP)
                    eq(o[2][42] if o[0] else st, ns[42], f'same stock as the original {ha} {st} {tons}')
    # range: boundary of the digital disc
    eq(in_range((10, 10), (15, 10, 4, 4)), True, 'dx5 dy0')
    eq(in_range((10, 10), (16, 10, 4, 4)), False, 'dx6 dy0')
    eq(in_range((10, 10), (14, 13, 4, 4)), True, 'dx4 dy3')
    eq(in_range((10, 10), (15, 13, 4, 4)), False, 'dx5 dy3')
    eq(tile_of(2815), 10, 'tile truncation')
    eq(len(set(outline((10, 10)))), 22, 'outline has 22 distinct tiles (the k = 0 pair repeats twice)')
    print(f'cargo_ref selftest: {n} checks ok')


# ----------------------------------------------------------------------------------------------- the checker
def fmt_holds(holds):
    return ' '.join(f'{g}:{a}' for g, a in holds)


def parse_expect(tokens):
    if tokens[0] == 'ok':
        return ('ok', int(tokens[1]))
    return ('reject', tokens[1])


def run_case(name, lines):
    goods_count, cap = DEFAULT_GOODS, DEFAULT_CAP
    stock0, ships = {}, {}
    reqs, endholds, endstock, orig = [], {}, {}, None
    for ln in lines:
        t = ln.split()
        if t[0] == 'goods_count':
            goods_count = int(t[1])
        elif t[0] == 'cap':
            cap = int(t[1])
        elif t[0] == 'stock':
            stock0[int(t[1])] = int(t[2])
        elif t[0] == 'hold':
            ships.setdefault(t[1], []).append((int(t[2]), int(t[3])))
        elif t[0] == 'req':
            arrow = t.index('->')
            reqs.append((t[1], int(t[2]), int(t[3]), int(t[4]), parse_expect(t[arrow + 1:])))
        elif t[0] == 'endhold':
            endholds[t[1]] = [tuple(int(x) for x in p.split(':')) for p in t[2:]]
        elif t[0] == 'endstock':
            endstock[int(t[1])] = int(t[2])
        elif t[0] == 'orig':
            orig = (int(t[1]), [tuple(int(x) for x in p.split(':')) for p in [t[2]]], int(t[3]))
        else:
            raise SystemExit(f'{name}: unknown line {ln!r}')
    bad = []
    total0 = sum(a for h in ships.values() for _, a in h) + sum(stock0.values())
    if orig is not None and reqs:
        ship, idx, g, tons, _ = reqs[0]
        o = orig_unload(ships[ship], idx, g, tons, stock0, cap, goods_count)
        got = (o[0], [o[1][idx]], o[2].get(g, 0))
        if got != orig:
            bad.append(f'orig: got {got}, expected {orig}')
    stock = dict(stock0)
    for ship, idx, g, tons, want in reqs:
        r = plan_unload(ships[ship], idx, g, tons, stock, cap, goods_count)
        got = ('ok', r[1]) if r[0] == 'ok' else r
        if got != want:
            bad.append(f'req {ship} {idx} {g} {tons}: got {got}, expected {want}')
            continue
        if r[0] == 'ok':
            ships[ship], stock = r[2], r[3]
    for ship, want in endholds.items():
        if ships[ship] != want:
            bad.append(f'endhold {ship}: got {fmt_holds(ships[ship])}, expected {fmt_holds(want)}')
    for g, want in endstock.items():
        if stock.get(g, 0) != want:
            bad.append(f'endstock {g}: got {stock.get(g, 0)}, expected {want}')
    for g in stock0:
        if g not in endstock and stock.get(g, 0) != stock0[g]:
            bad.append(f'stock {g} changed without an endstock line')
    total1 = sum(a for h in ships.values() for _, a in h) + sum(stock.values())
    if total0 != total1:
        bad.append(f'not conserved: {total0} -> {total1}')
    return bad


def check(path):
    cases, cur, fails, count = [], None, 0, 0
    extra = []
    for raw in open(path):
        line = raw.split('#', 1)[0].rstrip()
        if not line.strip():
            continue
        if line.startswith('case '):
            cur = (line.split()[1], [])
            cases.append(cur)
        elif line.startswith(('range ', 'tile ', 'cityset ')):
            extra.append(line)
            cur = None
        elif cur is not None:
            cur[1].append(line)
    for name, lines in cases:
        count += 1
        for b in run_case(name, lines):
            print(f'FAIL case {name}: {b}')
            fails += 1
    for line in extra:
        count += 1
        t = line.split()
        arrow = t.index('->')
        if t[0] == 'range':
            got = 'accept' if in_range((int(t[3]), int(t[4])), tuple(int(x) for x in t[6:10])) else 'reject'
            want = t[arrow + 1]
        elif t[0] == 'tile':
            got = ' '.join(str(tile_of(int(v))) for v in t[1:3])
            want = ' '.join(t[arrow + 1:])
        else:
            owned = [tuple(int(v) for v in p.split(',')) for p in t[t.index('owned') + 1:arrow]]
            got = 'in' if city_near((int(t[3]), int(t[4])), owned) else 'out'
            want = t[arrow + 1]
        if got != want:
            print(f'FAIL {t[0]} {t[1]}: got {got}, expected {want}')
            fails += 1
    print(f'cargo_ref check: {count} cases, {fails} failures')
    return 1 if fails else 0


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'selftest'
    if cmd == 'selftest':
        selftest()
        return 0
    if cmd == 'check':
        return check(sys.argv[2] if len(sys.argv) > 2 else DEFAULT_VECTORS)
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main())
