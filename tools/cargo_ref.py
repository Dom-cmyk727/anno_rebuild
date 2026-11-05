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

  cargo_ref.py selftest                  the rule's own checks (hand-derived numbers, independent of the vector files), the load vector
                                         file, and the load mutants (each must be caught)
  cargo_ref.py check [vectors.txt]       run a vector file (unload: `case` blocks; load: `loadcase` blocks; default: both fixtures); exit 0/1
  cargo_ref.py mutants [load_vectors]    run the load vectors against the broken load rules (LOAD_MUTANTS); exit 0 only if every one is rejected

MAP-CARGO-2 L1 adds the LOAD (own warehouse -> ship hold), again from the disassembly alone (docs/re/ship-cargo.md section 11):
  AnnoWelt.dll 0x100ac360 flag 0, 0x100ac498-0x100ac8e7   plan_load = the REBUILD rule (ADAPTATION of the arm: goods bound, wide
                                                           arithmetic, "empty" = goods 0 and amount 0, refusals named),
                                                           orig_load = the ORIGINAL's 32-bit arithmetic kept as evidence (like orig_unload)

Vector file (fixtures/synthetic/sim/cargo_vectors.txt), one `case` per block:
  goods_count N | cap U | stock G U | hold SHIP G U | req SHIP IDX G TONS -> ok U | reject REASON
  endhold SHIP G:U ... | endstock G U | orig RET G:U STOCK     (the original's result for the first request)
  range NAME ship TX TY bld X0 Y0 W H -> accept|reject
  tile QX QY -> TX TY                  (Q24.8 position to tile)
  cityset NAME ship TX TY owned X,Y ... -> in|out
Load vector file (fixtures/synthetic/sim/cargo_load_vectors.txt), one `loadcase` per block (see the header of that file):
  holdcap U | goods_count N | nholds N|none | nholds_b N | storecap U | stock G U | hold SHIP G U
  load SHIP IDX G TONS -> ok MOVED RESOLVED | reject REASON ; unload SHIP IDX G TONS -> ok MOVED | reject REASON
  endhold SHIP G:U ... | endstock G U | orig RET RESOLVED G:U STOCK     (the original's result for the first load; G:U = hold 0 when RESOLVED is -1)
Only the Python standard library is used.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_VECTORS = os.path.join(HERE, '..', 'fixtures', 'synthetic', 'sim', 'cargo_vectors.txt')
LOAD_VECTORS = os.path.join(HERE, '..', 'fixtures', 'synthetic', 'sim', 'cargo_load_vectors.txt')

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
    if ha == 0:                            # goods id with amount 0 (loadable state): the hold holds none of the goods
        return ('reject', 'HoldGoodsMismatch')
    free = max(0, cap - stock.get(goods, 0))
    if free == 0:
        return ('reject', 'StockFull')
    credit = min(tons * TON, ha, free)
    # credit > 0 here: tons > 0, ha > 0 and free > 0 were all checked above
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


# ------------------------------------------------------------------ the LOAD, REBUILD rule (MAP-CARGO-2 L1)
def resolve_hold(holds, n0, hold_cap, pref, goods, mutant=None):
    """The hold a load goes to, or None. ORIGINAL order (0x100ac4b5-0x100ac56f): the named hold if it holds `goods` and has at least one
    whole ton free; else the search over holds 0..n0-1: the first empty hold unless a hold of `goods` with free > 0 exists, then the one
    with the most free space (strictly greater, lowest index on ties). ADAPTATION: an empty hold is goods 0 AND amount 0.
    `mutant` (tests only) breaks one hold-selection rule: threshold_255 (preferred needs 255 free, not 256), tie_ge (ties go to the
    highest index), empty_first (an empty hold beats a same-goods hold with room)."""
    g, a = holds[pref]
    if g == goods and hold_cap - a >= (TON - 1 if mutant == 'threshold_255' else TON):
        return pref
    cand, best = None, 0
    cand_empty = False
    for k in range(min(n0, len(holds))):
        g, a = holds[k]
        if g == 0 and a == 0:
            if cand is None:
                cand, cand_empty = k, True
        elif g == goods:
            free = hold_cap - a
            if mutant == 'empty_first' and cand_empty:
                continue
            if free > 0 and (best <= free if mutant == 'tie_ge' else best < free):
                best, cand, cand_empty = free, k, False
    return cand


def plan_load(holds, n0, hold_cap, pref, goods, tons, stock, goods_count=DEFAULT_GOODS, mutant=None):
    """-> ('ok', moved, resolved_index, new_holds, new_stock) or ('reject', REASON). Order of the checks = docs/sim/ship-cargo.md 12.
    n0: the type's cargo hold count (None = the type has no cargo facts). `mutant` selects a deliberately broken rule (tests only)."""
    if tons == 0:
        return ('reject', 'BadValue')
    if n0 is None:
        return ('reject', 'NoCargoFacts')
    if pref >= min(len(holds), n0):
        return ('reject', 'BadIndex')
    if goods == 0 or goods >= goods_count:
        return ('reject', 'UnsupportedGoods')
    idx = resolve_hold(holds, n0, hold_cap, pref, goods, mutant)
    if idx is None:
        return ('reject', 'HoldsFull')
    room = hold_cap - holds[idx][1]
    if room <= 0:
        return ('reject', 'HoldsFull')
    avail = stock.get(goods, 0) if mutant == 'no_floor' else (stock.get(goods, 0) // TON) * TON  # rounded DOWN to whole tons (0x100ac5bf)
    if avail == 0:
        return ('reject', 'StockShort')
    x = min(tons * TON, room, avail)
    new_holds = list(holds)
    new_holds[idx] = (goods, holds[idx][1] + x)
    new_stock = dict(stock)
    if mutant == 'credit_only':                           # broken: the hold is credited, the store is not debited
        pass
    elif mutant == 'debit_request':                       # broken: the store loses the request although the hold got less
        new_stock[goods] = stock.get(goods, 0) - min(tons * TON, stock.get(goods, 0))
    else:
        new_stock[goods] = stock.get(goods, 0) - x
    return ('ok', x, idx, new_holds, new_stock)


def orig_load(holds, n0, hold_cap, pref, goods, tons, stock, goods_count=DEFAULT_GOODS):
    """Transcription of AnnoWelt.dll 0x100ac498-0x100ac8e7 (flag 0, owners equal). -> (return, resolved index or -1, new_holds, new_stock).
    32-bit registers, signed compares where the code uses jl/jle/jge, hold amount a u16 (callers keep amounts <= 0xFFFF).
    A goods id outside the table reads and writes out of the city's stock table in the original (defect E1): not modelled (raises)."""
    if goods >= goods_count:
        raise ValueError('goods id outside the stock table: the original reads/writes out of bounds')
    nothing = (0, -1, list(holds), dict(stock))
    if pref >= n0:                                         # 0x100ac476: cmp esi, eax; jl  (rec[1] byte)
        return nothing
    hg, ha = holds[pref]
    cand = None
    if hg == goods and _s32(hold_cap - ha) >= 0x100:       # 0x100ac4b8-0x100ac4d9
        cand = pref
    else:                                                  # 0x100ac4df-0x100ac56f
        if n0 <= 0:
            return nothing
        best = 0
        for k in range(n0):
            g, a = holds[k]
            if g == 0:
                if cand is None:
                    cand = k
            elif g == goods:
                free = _s32(hold_cap - a)
                if free > 0 and best < free:
                    best, cand = free, k
        if cand is None:
            return nothing
    ca = holds[cand][1]
    esi = (tons & 0xFFFF) << 8                             # 0x100ac58b-0x100ac591
    free = _s32(hold_cap - ca)
    if esi > free:                                         # 0x100ac5a6-0x100ac5aa  cmp esi, eax; jle
        esi = free
    avail = _s32(stock.get(goods, 0) & 0xFFFFFF00)         # 0x100ac5bc-0x100ac5bf  and eax, 0xffffff00
    if esi > avail:                                        # 0x100ac5c4-0x100ac5c8  cmp esi, eax; jle
        esi = avail
    if esi == 0:                                           # 0x100ac5ca-0x100ac5cc
        return nothing
    nh = list(holds)                                       # 0x100ac8b3-0x100ac8d5: hold slot +0 (set goods), +0xc (word add), city +0x84
    nh[cand] = (goods, (ca + (esi & 0xFFFF)) & 0xFFFF)
    s = stock.get(goods, 0)
    take = min(esi & M32, s)                               # 0x1002da90: amount = min(amount, stock)
    ns = dict(stock)
    ns[goods] = (s - take) & M32
    return esi, cand, nh, ns


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
    # ---- load: hand-derived numbers independent of the vector file (docs/sim/ship-cargo.md 12; every value is worked out in the comment)
    e4 = [(0, 0)] * 4
    # empty hold, 5 t of 10 t of wood: x = min(1280, 12800, 2560) = 1280
    eq(plan_load(e4, 4, 12800, 0, 42, 5, {42: 2560}), ('ok', 1280, 0, [(42, 1280), (0, 0), (0, 0), (0, 0)], {42: 1280}), 'load empty')
    # stock 600 = 2 t + 88 units: only the whole tons move, the 88 stay
    eq(plan_load(e4, 4, 12800, 0, 42, 10, {42: 600})[1], 512, 'load whole tons only')
    eq(plan_load(e4, 4, 12800, 0, 42, 1, {42: 255}), ('reject', 'StockShort'), 'load below one ton')
    # different goods in the named hold: redirected to the first empty hold (not refused)
    eq(plan_load([(26, 5), (0, 0), (0, 0), (0, 0)], 4, 12800, 0, 42, 1, {42: 5000})[2], 1, 'redirect to the first empty hold')
    # a same-goods hold with any room beats the empty named hold; ties go to the lowest index
    eq(resolve_hold([(0, 0), (42, 12700), (42, 12700), (0, 0)], 4, 12800, 0, 42), 1, 'same goods with room beats empty, lowest index')
    eq(resolve_hold([(42, 12800), (26, 1), (26, 1), (26, 1)], 4, 12800, 0, 42), None, 'full hold, no other room')
    # the original and the rebuild agree on the ordinary rows; the original loses nothing here (credit == debit)
    for ha in (0, 1, 255, 256, 12544, 12545, 12700, 12799):
        for st in (0, 255, 256, 600, 5000, 12800):
            for tons in (1, 3, 50):
                hs = [(42, ha) if ha else (0, 0), (0, 0), (41, 7), (0, 0)]
                r = plan_load(hs, 4, 12800, 0, 42, tons, {42: st})
                o = orig_load(hs, 4, 12800, 0, 42, tons, {42: st})
                if r[0] != 'ok':
                    eq((o[0], o[2], o[3]), (0, hs, {42: st}), f'original refuses too {ha} {st} {tons}')
                    continue
                _, x, idx, nh, ns = r
                eq((o[0], o[1], o[2], o[3]), (x, idx, nh, ns), f'same as the original {ha} {st} {tons}')
                eq(sum(a for _, a in hs) + st, sum(a for _, a in nh) + ns[42], f'load conserves {ha} {st} {tons}')
                eq(st - ns[42], nh[idx][1] - hs[idx][1], f'debit == credit {ha} {st} {tons}')
    # defect E2: stock 2^32-1 and a hold with one unit of room make the original's signed compare negative
    o = orig_load([(42, 65279)], 1, 65280, 0, 42, 1, {42: M32})
    eq((o[0], o[2], o[3]), (-256, [(42, 65023)], {42: 255}), 'orig E2')
    eq(plan_load([(42, 65279)], 1, 65280, 0, 42, 1, {42: M32})[1], 1, 'rebuild E2 wide')
    # the load mutants are caught by the vector file (see mutants())
    # The load vector file is a required input: a missing file is a FAILURE, never a skip (MAP-CARGO-2 L2).
    if not os.path.exists(LOAD_VECTORS):
        raise SystemExit(f'cargo_ref selftest FAILED: the load vector file is missing: {LOAD_VECTORS}')
    eq(check(LOAD_VECTORS, quiet=True), 0, 'load vector file')
    eq(mutants(LOAD_VECTORS, quiet=True), 0, 'load mutants are caught')
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


def parse_state(t):
    return tuple(int(x) for x in t.split(':'))


def run_load_case(name, lines, planner=plan_load):
    """One `loadcase` block (see the header of fixtures/synthetic/sim/cargo_load_vectors.txt)."""
    goods_count, hold_cap, store_cap, nholds = DEFAULT_GOODS, DEFAULT_CAP, DEFAULT_CAP, 'auto'
    stock0, ships, steps, endholds, endstock, orig = {}, {}, [], {}, {}, None
    for ln in lines:
        t = ln.split()
        if t[0] == 'goods_count':
            goods_count = int(t[1])
        elif t[0] == 'holdcap':
            hold_cap = int(t[1])
        elif t[0] == 'storecap':
            store_cap = int(t[1])
        elif t[0] == 'nholds':
            nholds = None if t[1] == 'none' else int(t[1])
        elif t[0] == 'nholds_b':
            pass  # the type's SECOND hold group (hold_count_b): by the original it is NOT a transfer bound (0x100ac476), so it changes nothing here
        elif t[0] == 'stock':
            stock0[int(t[1])] = int(t[2])
        elif t[0] == 'hold':
            ships.setdefault(t[1], []).append((int(t[2]), int(t[3])))
        elif t[0] in ('load', 'unload'):
            arrow = t.index('->')
            want = ('ok',) + tuple(int(v) for v in t[arrow + 2:]) if t[arrow + 1] == 'ok' else ('reject', t[arrow + 2])
            steps.append((t[0], t[1], int(t[2]), int(t[3]), int(t[4]), want))
        elif t[0] == 'endhold':
            endholds[t[1]] = [parse_state(p) for p in t[2:]]
        elif t[0] == 'endstock':
            endstock[int(t[1])] = int(t[2])
        elif t[0] == 'orig':
            orig = (int(t[1]), int(t[2]), parse_state(t[3]), int(t[4]))
        else:
            raise SystemExit(f'{name}: unknown line {ln!r}')
    n0 = len(next(iter(ships.values()))) if nholds == 'auto' else nholds
    bad = []
    total0 = sum(a for h in ships.values() for _, a in h) + sum(stock0.values())
    first_load = next(((s[1], s[2], s[3], s[4]) for s in steps if s[0] == 'load'), None)
    if orig is not None and first_load is not None:
        ship, pref, g, tons = first_load
        o = orig_load(ships[ship], n0, hold_cap, pref, g, tons, stock0, goods_count)
        got = (o[0], o[1], o[2][max(o[1], 0)], o[3].get(g, 0))
        if got != orig:
            bad.append(f'orig: got {got}, expected {orig}')
    stock = dict(stock0)
    for kind, ship, idx, g, tons, want in steps:
        if kind == 'load':
            r = planner(ships[ship], n0, hold_cap, idx, g, tons, stock, goods_count)
            got = ('ok', r[1], r[2]) if r[0] == 'ok' else r
        elif n0 is not None and idx >= n0:
            r = ('reject', 'BadIndex')      # unload step 9 (0x100ac476): valid cargo indices end at n0 - 1; nholds none = no facts, the carried holds bound it
            got = r
        else:
            r = plan_unload(ships[ship], idx, g, tons, stock, store_cap, goods_count)
            got = ('ok', r[1]) if r[0] == 'ok' else r
        if got != want:
            bad.append(f'{kind} {ship} {idx} {g} {tons}: got {got}, expected {want}')
            continue
        if r[0] == 'ok':
            ships[ship], stock = (r[3], r[4]) if kind == 'load' else (r[2], r[3])
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


def read_blocks(path, keyword):
    blocks, cur = [], None
    for raw in open(path):
        line = raw.split('#', 1)[0].rstrip()
        if not line.strip():
            continue
        if line.startswith(keyword + ' '):
            cur = (line.split()[1], [])
            blocks.append(cur)
        elif cur is not None:
            cur[1].append(line)
    return blocks


def check_load(path, planner=plan_load, quiet=False):
    fails, blocks = 0, read_blocks(path, 'loadcase')
    for name, lines in blocks:
        for b in run_load_case(name, lines, planner):
            if not quiet:
                print(f'FAIL loadcase {name}: {b}')
            fails += 1
    if not quiet:
        print(f'cargo_ref check (load): {len(blocks)} cases, {fails} failures')
    return fails


LOAD_MUTANTS = ('credit_only', 'debit_request', 'threshold_255', 'tie_ge', 'empty_first', 'no_floor')


def mutants(path, quiet=False):
    """Every broken load rule must fail at least one case of the load vector file. -> number of mutants that SURVIVED."""
    survived = 0
    for m in LOAD_MUTANTS:
        caught = check_load(path, lambda *a, **k: plan_load(*a, mutant=m, **k), quiet=True)
        if not quiet:
            print(f'mutant {m}: {"caught by " + str(caught) + " failures" if caught else "SURVIVED"}')
        survived += 0 if caught else 1
    return survived


def check(path=None, quiet=False):
    if path is None:
        return 1 if (check(DEFAULT_VECTORS, quiet) or check(LOAD_VECTORS, quiet)) else 0
    if read_blocks(path, 'loadcase'):
        return 1 if check_load(path, quiet=quiet) else 0
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
    if not quiet:
        print(f'cargo_ref check: {count} cases, {fails} failures')
    return 1 if fails else 0


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'selftest'
    if cmd == 'selftest':
        selftest()
        return 0
    if cmd == 'check':
        return check(sys.argv[2] if len(sys.argv) > 2 else None)
    if cmd == 'mutants':
        return 1 if mutants(sys.argv[2] if len(sys.argv) > 2 else LOAD_VECTORS) else 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main())
