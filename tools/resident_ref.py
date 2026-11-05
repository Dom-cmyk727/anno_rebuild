#!/usr/bin/env python3
"""Independent integer reference for the resident/supplier transactions (MAP-POP-LIVE-1 L1).

Written from docs/sim/population-live.md section 2 (AnnoWelt.dll: house demand 0x100765c0, house choice
0x10076670 with the IdSec helper 0x10076960, house delivery 0x10076610, walker purchase step B arrival
0x10056480, city billing 0x1002dac0 and the supplier wrappers) without reading the C++ module. It runs
standalone over a vector file (fixtures/synthetic/sim/resident_vectors.txt) whose expected values were
derived by hand before any C++ existed.

FORMULA AND UNITS CONTRACT (all integers; "/" below truncates toward zero; amounts are 8.8 tons, 256 = 1 t)
  demand(menge, E, stock)   = trunc(menge*E*256 / 44236) - trunc(stock / 16)      (signed, delivered units)
  A delivery adds amount*16 to a house stock, so stock/16 is in delivered units.
  s(slot)    = min(trunc(1536*stock[slot] / (E*menge[slot])), 160); E*menge == 0 -> fault choice_divisor
  urg(slot)  = max(128 - s(slot), urg(next)) along next = lookup[id_sec[ware[slot]]] (id_sec != 0 and the
               lookup is a real slot), followed at most goods_count steps (a cycle is cut there).
  choice(supplier out A,B ; avail a,b ; enabled ; carried cargo of the house's walkers):
    a candidate is zeroed (skipped) when some carried cargo slot has a NON-ZERO amount and goods == it
    (A and B are tested independently); A is evaluated first, then B. A candidate g with
    slot = lookup[g] >= need_count is skipped. urg is computed first (a zero divisor is a fault, never
    "not chosen"); g is accepted only if urg >= 0 and avail(g) > trunc(R*menge[slot]*256 / 4096) with
    R = trunc(res64/64) (signed, strict) and enabled(g). A is accepted at once (best = urg); B only if
    its urg is strictly greater than best (best starts at -1). Result: score = best or -1, goods.
  supplier avail: MarketStall = city store of g (no reservation); PublicService, School, Doctor = 256;
    FireBrigade = 512; Inn = tavern stock; any other class 0. enabled: MarketStall = flag0 when g == out0,
    else flag1 (the bytes test `out0 != g`, so every other goods reads flag1; the ALL goods 0x8e arm and
    the Castle flag +0x22 are not modelled); every other class true.
  bill(goods, amount, take) over [first..last] (default 26..40): outside the range -> returns amount, no
    change. Else billed[goods] += amount (the REQUESTED amount; u32 checked: an overflow refuses the whole
    call, status billed_overflow); take == 0 -> returns amount; else t = min(amount, store[goods]),
    store -= t, returns t. A goods index outside the tables is status out_of_table.
  supplier bill: MarketStall take=1; PublicService and School take=0; Doctor and FireBrigade return 0 and
    change nothing; Inn (0x100756e0): t = min(tavern, amount), tavern -= t, bills its input good with take=0
    for the CLIPPED amount t (even t == 0: billed += 0, which still checks the range and the u32 overflow),
    returns t (the taken-stat +0x1a is not modelled, U); any other class returns 0.
  deliver(goods, amount) (0x10076610): slot = lookup[goods]; no slot -> nothing (touched 0). Else
    stock[slot] += amount*16 and, when id_sec[goods] != 0, the replay is a RECURSIVE deliver of the
    substitute (house vtable slot 0xc0 is 0x10076610 itself): it adds at the substitute's slot if it has
    one and replays that substitute's own IdSec in turn; a substitute without a slot ends the chain. The
    chain is followed along substitutes that have a need slot. The original recurses unboundedly (a cycle
    crashes it); MAP-POP-CLOSE-1 G2: when the walk would visit a goods id twice (a cycle or self-loop) the call
    is refused whole, status idsec_cycle, stock unchanged, touched 0 (goods ids are compared, not slots: two
    goods sharing one need slot are a legal chain). Every add is checked against u32 over the whole chain
    before any is applied; any overflow refuses the call (status refused).
  idsec(goods, house): registry check (derive_group): from every goods id g with lookup[g] a need slot, follow
    id_sec[g] while the substitute also has a need slot; visiting a goods id twice is status cycle, else ok.
    The goods count does not matter (unrelated goods cannot change the answer).
  buy (walker step B arrival): i = first cargo slot with goods == 0 (none -> nothing). Pass: choice over the
    own cargo plus the others' cargo; score < 12 or none -> stop; amt = min(avail(g), demand(g), cap)
    (signed); amt > 0 -> bill, then slot goods = g and slot amount += returned, i += 1. Second pass only if
    i < n and slot i is still empty (goods == 0); no third pass. A bill failure leaves the cargo unchanged.

Vector file grammar (one record per line, '#' comments):
  goods NAME count=N [sec=g:s,...]                     the goods table: size and IdSec substitutes
  house NAME needs=ware:menge,... lookup=g:slot,...    a group's needs and its goods -> slot lookup
  demand NAME menge= e= stock= => demand=
  amt NAME avail= demand= cap= => amount=
  choice NAME goods= house= e= res= stock=a,b out=A,B avail=a,b [enabled=1,1] [carried=g:a,..] => status= [score= goods= fault=]
  avail NAME cls= goods= [city=g:v,..] [count=N] [tavern=N] [out=A,B] [flags=f0,f1] => avail= enabled= | fault=
  bill NAME goods= amount= take= [range=first,last] [count=N] store=g:v,.. billed=g:v,.. => status= returned= store= billed=
  sbill NAME cls= goods= amount= [tavern=N] [input=g] [range=first,last] [count=N] store=.. billed=.. => status= returned= store= billed= tavern=
  deliver NAME goods= house= goods_id= amount= stock=a,b => status= stock= touched=   (status ok|refused|idsec_cycle)
  idsec NAME goods= house= => status=ok|cycle
  innrec NAME rec=b0,..,b9|none => status=decoded|skipped tavern= taken=   (the Inn scenario record, MAP-POP-CLOSE-1 C2)
  buy NAME goods= house= e= res= stock=.. cls= out=A,B [flags=] [city=] [billed=] [tavern=] [input=] cargo=g:a,..
      [others=g:a,..] cap= [range=] [count=] => status= cargo= city= billed= purchases= [tavern=] [fault=]
Exit codes: 0 all vectors agree; 1 a vector disagrees; 2 malformed input, a missing file or zero vectors.
"""
import sys

CLASSES = ('Plain', 'Workshop', 'DualWorkshop', 'WeaponsWorkshop', 'Farm', 'FurTrapper', 'Mine', 'SmallFarm',
           'Quarry', 'Hunter', 'Whaler', 'Shipyard', 'Cannonworks', 'Market', 'MarketCombi', 'MarketStall',
           'Inn', 'PublicService', 'Doctor', 'School', 'Castle', 'FireBrigade', 'House', 'Tower', 'CannonTower')
U32 = 0xFFFFFFFF


class Malformed(Exception):
    pass


class Fault(Exception):
    def __init__(self, name):
        Exception.__init__(self, name)
        self.name = name


def tdiv(a, b):
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def ints(s):
    return [] if s in ('', 'none') else [int(x, 0) for x in s.split(',')]


def pairs(s):
    out = []
    if s in ('', 'none'):
        return out
    for t in s.split(','):
        a, b = t.split(':')
        out.append((int(a), int(b)))
    return out


def sparse(s):
    d = {}
    for a, b in pairs(s):
        d[a] = b
    return d


def fmt_sparse(d):
    items = sorted((k, v) for k, v in d.items() if v != 0)
    return ','.join('%d:%d' % kv for kv in items) if items else 'none'


def fmt_pairs(p):
    return ','.join('%d:%d' % kv for kv in p) if p else 'none'


def fmt_list(v):
    return ','.join(map(str, v)) if v else 'none'


# ---- the operations -------------------------------------------------------------------------------

def demand(menge, e, stock):
    return tdiv(menge * e * 256, 44236) - tdiv(stock, 16)


def urgency(goods, house, e, stock, slot):
    need = house['needs']
    n = len(need)

    def s_of(k):
        d = e * need[k][1]
        if d == 0:
            raise Fault('choice_divisor')
        return min(tdiv(1536 * stock[k], d), 160)

    urg = 128 - s_of(slot)
    cur = slot
    for _ in range(goods['count']):
        sec = goods['sec'].get(need[cur][0], 0)
        if sec == 0:
            break
        nxt = house['lookup'].get(sec, n)
        if nxt >= n:
            break
        urg = max(urg, 128 - s_of(nxt))
        cur = nxt
    return urg


def choice(goods, house, e, res64, stock, out, avail, enabled, carried):
    cand = list(out)
    for g, a in carried:
        if a != 0:
            for k in range(2):
                if cand[k] == g:
                    cand[k] = 0
    best, got = -1, 0
    n = len(house['needs'])
    for k in range(2):
        g = cand[k]
        if g == 0:
            continue
        slot = house['lookup'].get(g, n)
        if slot >= n:
            continue
        urg = urgency(goods, house, e, stock, slot)
        thr = tdiv(tdiv(res64, 64) * house['needs'][slot][1] * 256, 4096)
        if urg >= 0 and avail[k] > thr and enabled[k]:
            if k == 0 or urg > best:
                best, got = urg, g
    return best, got


def supplier_avail(cls, g, city, count, tavern):
    if cls == 'MarketStall':
        if g >= count:
            raise Fault('goods_out_of_table')
        return city.get(g, 0)
    if cls in ('PublicService', 'School', 'Doctor'):
        return 256
    if cls == 'FireBrigade':
        return 512
    if cls == 'Inn':
        return tavern
    return 0


def supplier_enabled(cls, g, out, flags):
    if cls == 'MarketStall':
        return flags[0] if g == out[0] else flags[1]
    return True


def bill(first, last, count, store, billed, g, amount, take):
    """-> (status, returned); mutates store and billed only on 'ok'."""
    if g < first or g > last:
        return 'ok', amount
    if g >= count:
        return 'out_of_table', 0
    if billed.get(g, 0) + amount > U32:
        return 'billed_overflow', 0
    billed[g] = billed.get(g, 0) + amount
    if take == 0:
        return 'ok', amount
    t = min(amount, store.get(g, 0))
    store[g] = store.get(g, 0) - t
    return 'ok', t


def supplier_bill(m, first, last, count, store, billed, g, amount):
    cls = m['cls']
    if cls == 'MarketStall':
        return bill(first, last, count, store, billed, g, amount, 1)
    if cls in ('PublicService', 'School'):
        return bill(first, last, count, store, billed, g, amount, 0)
    if cls == 'Inn':
        t = min(m['tavern'][0], amount)
        st, _ = bill(first, last, count, store, billed, m['input'], t, 0)
        if st != 'ok':
            return st, 0
        m['tavern'][0] -= t
        return 'ok', t
    return 'ok', 0


def chain_walk(goods, house, g):
    """-> (slots of the chain, cycle?)  (follows substitutes that have a need slot; a goods id seen twice = cycle)"""
    n = len(house['needs'])
    slot = house['lookup'].get(g, n)
    if slot >= n:
        return [], False
    chain, seen, cur = [slot], {g}, g
    while True:
        sec = goods['sec'].get(cur, 0)
        if sec == 0:
            break
        s2 = house['lookup'].get(sec, n)
        if s2 >= n:
            break
        if sec in seen:
            return chain, True
        seen.add(sec)
        chain.append(s2)
        cur = sec
    return chain, False


def idsec(goods, house):
    n = len(house['needs'])
    for g in sorted(house['lookup']):
        if house['lookup'][g] < n and chain_walk(goods, house, g)[1]:
            return 'cycle'
    return 'ok'


def deliver(goods, house, stock, g, amount):
    chain, cyc = chain_walk(goods, house, g)
    if cyc:
        return 'idsec_cycle', 0, stock
    if not chain:
        return 'ok', 0, stock
    add = {}
    for k in chain:
        add[k] = add.get(k, 0) + amount * 16
    for k, v in add.items():
        if stock[k] + v > U32:
            return 'refused', 0, stock
    new = list(stock)
    for k, v in add.items():
        new[k] += v
    return 'ok', len(chain), new


def buy(v):
    goods, house = v['goods'], v['house']
    n = len(v['cargo'])
    cargo = [list(c) for c in v['cargo']]
    purchases = 0
    m = {'cls': v['cls'], 'tavern': [v['tavern']], 'input': v['input']}
    i = next((k for k in range(n) if cargo[k][0] == 0), None)
    status, fault = 'ok', ''
    if i is None:
        return finish(v, cargo, purchases, m, status, fault)
    try:
        for npass in range(2):
            if npass == 1 and not (i < n and cargo[i][0] == 0):
                break
            carried = [tuple(c) for c in cargo] + list(v['others'])
            out = v['out']
            avail = [supplier_avail(v['cls'], out[k], v['city'], v['count'], m['tavern'][0]) for k in range(2)]
            en = [supplier_enabled(v['cls'], out[k], out, v['flags']) for k in range(2)]
            score, g = choice(goods, house, v['e'], v['res'], v['stock'], out, avail, en, carried)
            if score < 12:
                break
            k = 0 if g == out[0] else 1
            slot = house['lookup'][g]
            amt = min(avail[k], demand(house['needs'][slot][1], v['e'], v['stock'][slot]), v['cap'])
            if amt > 0:
                st, r = supplier_bill(m, v['first'], v['last'], v['count'], v['city'], v['billed'], g, amt)
                if st != 'ok':
                    status, fault = 'fault', st
                    break
                if cargo[i][1] + r > U32:
                    status, fault = 'fault', 'overflow'
                    break
                cargo[i][0] = g
                cargo[i][1] += r
                i += 1
                purchases += 1
    except Fault as f:
        status, fault = 'fault', f.name
    return finish(v, cargo, purchases, m, status, fault)


def finish(v, cargo, purchases, m, status, fault):
    r = {'status': status, 'cargo': fmt_pairs([tuple(c) for c in cargo]), 'city': fmt_sparse(v['city']),
         'billed': fmt_sparse(v['billed']), 'purchases': str(purchases), 'tavern': str(m['tavern'][0])}
    if fault:
        r['fault'] = fault
    return r


# ---- vector file ------------------------------------------------------------------------------------

def parse_kv(parts):
    kv = {}
    for p in parts:
        if '=' not in p:
            raise Malformed('not key=value: ' + p)
        k, v = p.split('=', 1)
        kv[k] = v
    return kv


KEYS = {
    'demand': {'demand'},
    'amt': {'amount'},
    'choice': {'status', 'score', 'goods', 'fault'},
    'avail': {'avail', 'enabled', 'fault'},
    'bill': {'status', 'returned', 'store', 'billed'},
    'sbill': {'status', 'returned', 'store', 'billed', 'tavern'},
    'deliver': {'status', 'stock', 'touched'},
    'idsec': {'status'},
    'innrec': {'status', 'tavern', 'taken'},
    'buy': {'status', 'cargo', 'city', 'billed', 'purchases', 'tavern', 'fault'},
}


def run_vector(kind, kv, tables):
    G = tables['goods']
    H = tables['house']
    if kind == 'demand':
        return {'demand': str(demand(int(kv['menge']), int(kv['e']), int(kv['stock'])))}
    if kind == 'amt':
        return {'amount': str(min(int(kv['avail']), int(kv['demand']), int(kv['cap'])))}
    if kind == 'choice':
        en = [bool(int(x)) for x in kv.get('enabled', '1,1').split(',')]
        try:
            score, g = choice(G[kv['goods']], H[kv['house']], int(kv['e']), int(kv['res']), ints(kv['stock']),
                              ints(kv['out']), ints(kv['avail']), en, pairs(kv.get('carried', 'none')))
        except Fault as f:
            return {'status': 'fault', 'fault': f.name, 'score': '-1', 'goods': '0'}
        return {'status': 'chosen' if score >= 0 else 'none', 'score': str(score), 'goods': str(g)}
    if kind == 'avail':
        cls = kv['cls']
        if cls not in CLASSES:
            raise Malformed('unknown class ' + cls)
        out = ints(kv.get('out', '0,0'))
        flags = [bool(int(x)) for x in kv.get('flags', '1,1').split(',')]
        try:
            a = supplier_avail(cls, int(kv['goods']), sparse(kv.get('city', 'none')), int(kv.get('count', '96')),
                               int(kv.get('tavern', '0')))
        except Fault as f:
            return {'fault': f.name}
        return {'avail': str(a), 'enabled': str(int(supplier_enabled(cls, int(kv['goods']), out, flags)))}
    if kind == 'bill':
        rng = ints(kv.get('range', '26,40'))
        store, billed = sparse(kv['store']), sparse(kv['billed'])
        st, r = bill(rng[0], rng[1], int(kv.get('count', '96')), store, billed, int(kv['goods']),
                     int(kv['amount']), int(kv['take']))
        return {'status': st, 'returned': str(r), 'store': fmt_sparse(store), 'billed': fmt_sparse(billed)}
    if kind == 'sbill':
        cls = kv['cls']
        if cls not in CLASSES:
            raise Malformed('unknown class ' + cls)
        rng = ints(kv.get('range', '26,40'))
        store, billed = sparse(kv['store']), sparse(kv['billed'])
        m = {'cls': cls, 'tavern': [int(kv.get('tavern', '0'))], 'input': int(kv.get('input', '0'))}
        st, r = supplier_bill(m, rng[0], rng[1], int(kv.get('count', '96')), store, billed, int(kv['goods']),
                              int(kv['amount']))
        return {'status': st, 'returned': str(r), 'store': fmt_sparse(store), 'billed': fmt_sparse(billed),
                'tavern': str(m['tavern'][0])}
    if kind == 'innrec':
        # Inn Load 0x10075740: a record of exactly 10 bytes is read (tavern stock u16 at +2 -> Inn +0x18, taken
        # statistic u16 at +8 -> Inn +0x1a); any other length is skipped whole (the stream's skip at 0x10075792)
        # and the Inn keeps its constructor numbers (0, 0). The layout mirrors Save 0x100757c0.
        rec = ints(kv['rec'])
        if len(rec) != 10:
            return {'status': 'skipped', 'tavern': '0', 'taken': '0'}
        return {'status': 'decoded', 'tavern': str(rec[2] | rec[3] << 8), 'taken': str(rec[8] | rec[9] << 8)}
    if kind == 'idsec':
        return {'status': idsec(G[kv['goods']], H[kv['house']])}
    if kind == 'deliver':
        stock = ints(kv['stock'])
        st, touched, new = deliver(G[kv['goods']], H[kv['house']], stock, int(kv['goods_id']), int(kv['amount']))
        return {'status': st, 'stock': fmt_list(new), 'touched': str(touched)}
    if kind == 'buy':
        cls = kv['cls']
        if cls not in CLASSES:
            raise Malformed('unknown class ' + cls)
        rng = ints(kv.get('range', '26,40'))
        v = dict(goods=G[kv['goods']], house=H[kv['house']], e=int(kv['e']), res=int(kv['res']),
                 stock=ints(kv['stock']), cls=cls, out=ints(kv['out']),
                 flags=[bool(int(x)) for x in kv.get('flags', '1,1').split(',')],
                 city=sparse(kv.get('city', 'none')), billed=sparse(kv.get('billed', 'none')),
                 tavern=int(kv.get('tavern', '0')), input=int(kv.get('input', '0')), cargo=pairs(kv['cargo']),
                 others=pairs(kv.get('others', 'none')), cap=int(kv['cap']), first=rng[0], last=rng[1],
                 count=int(kv.get('count', '96')))
        return buy(v)
    raise Malformed('unknown record ' + kind)


def run(path):
    try:
        text = open(path).read()
    except OSError:
        print('cannot read', path)
        return 2
    tables = {'goods': {}, 'house': {}}
    nvec, bad = 0, 0
    for n, raw in enumerate(text.split('\n'), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        where = '%s:%d' % (path, n)
        try:
            kind = parts[0]
            if kind == 'goods':
                kv = parse_kv(parts[2:])
                tables['goods'][parts[1]] = {'count': int(kv['count']), 'sec': sparse(kv.get('sec', 'none'))}
            elif kind == 'house':
                kv = parse_kv(parts[2:])
                needs = pairs(kv['needs'])
                if any(m < 0 for _, m in needs):
                    raise Malformed('negative menge')
                tables['house'][parts[1]] = {'needs': needs, 'lookup': sparse(kv.get('lookup', 'none'))}
            elif kind in KEYS:
                if '=>' not in parts:
                    raise Malformed('no =>')
                k = parts.index('=>')
                kv, exp = parse_kv(parts[2:k]), parse_kv(parts[k + 1:])
                if not exp or not set(exp) <= KEYS[kind]:
                    raise Malformed('empty or unknown result key')
                if kind == 'choice' and 'status' not in exp:
                    raise Malformed('a choice vector must name status')
                if kind in ('choice', 'bill', 'sbill', 'deliver', 'buy') and 'status' not in exp and kind != 'avail':
                    raise Malformed('vector must name status')
                got = run_vector(kind, kv, tables)
                nvec += 1
                for key, want in exp.items():
                    have = got.get(key, '<absent>')
                    if have != want:
                        print('%s: %s: %s expected %s, reference computed %s' % (where, parts[1], key, want, have))
                        bad += 1
            else:
                raise Malformed('unknown record ' + kind)
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
        print('usage: resident_ref.py VECTORS', file=sys.stderr)
        sys.exit(2)
    sys.exit(run(sys.argv[1]))
