#!/usr/bin/env python3
"""Independent integer reference for the House transition rules (MAP-POP-LIVE-1 L3).

Written from docs/sim/population-live.md section 5 and docs/re/population.md 5 / 12.4 plus the disassembly of
AnnoWelt SetGroup 0x10076b90 and UpgradeCheck 0x10075dd0, without reading the C++ module. It runs standalone
over fixtures/synthetic/sim/house_transition_vectors.txt, whose expected values were derived by hand before any
C++ existed.

FORMULA CONTRACT (integers; "/" truncates toward zero)
  ladder: next = id - id%stride + stride (must be in `have`, else none). prev = id - id%stride - stride if it is in
    `have`, else id - id%stride - stride + 1 if in `have`, else none (negative ids never exist).
  variant: count == 0 -> no draw, type = base. Else idx = r mod count (r unsigned 16-bit), type = base + idx*step.
  matgate: for every k with amount_k != 0 the signed test amount_k <= store_k - setting_k must hold. If every test
    holds: spent_k += amount_k and store_k -= min(amount_k, store_k) (all k); else nothing changes.
  upcheck (UpgradeCheck): noup -> result 0. E == 0 -> fault zero_divisor. Score of a slot with the CURRENT group's
    Menge: s = min(stock*1536/(E*menge_cur), 128). food = min(stock0*1536/(E*menge_next0), 128) (the NEXT group's
    Menge0). Goods loop j = 1..next.goods_end-1 and infra loop j = next.goods_end..next.infra_end-1 look at FORCED needs
    of the next group only; the slot is cur.lookup[ware] (missing -> fault missing_slot). Goods scores sum/count;
    infra scores min (start 160); an infra need whose ware is the tavern role also joins the goods sum/count.
    f = nah[(food+4)>>3]; mean = count ? sum/count : 128; b = bed[(mean+4)>>3]; i = inf[(min_infra+4)>>3];
    result = 1 iff f > 0 and b > 0 and i > 0 (strict).
  setgroup (0x10076b90): flag[ware] = 1 for every ware of the NEW group. Old group present: for each old need j in
    0..old.infra_end-1: s = min(stock_j*1536/(E*menge_old_j), 160); sat[ware] = s when flag[ware] else max(sat, s);
    flag[ware] = 0; when s >= 102 the IdSec chain from the ware is walked while the substitute still has its flag set
    (sat = s, flag = 0). If old.num > new.num (downgrade): every new need j in 0..new.infra_end-1 whose flag is still set
    gets sat = 102, flag 0, and E = (trunc(res64/64)) << 7. Without an old group and new.num != 0 (first assignment):
    for each new need j in 0..new.infra_end-1 with force set, in order, r = next draw, sat = 128 - (r & 31), flag = 0;
    then E = wohn << 7 and res64 = wohn << 6. Finally for j in 0..n-1: flagged ware -> stock = (E*menge/12 for j == 0,
    else 0); otherwise stock = menge_new*sat*E/1536 (new E).
Vector grammar: see the records in the fixture. Exit codes: 0 all vectors agree; 1 a vector disagrees; 2 malformed
input, a missing file or zero vectors.
"""
import sys


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


def fmt(v):
    return ','.join(map(str, v)) if v else 'none'


def pairs(s):
    out = {}
    if s in ('', 'none'):
        return out
    for t in s.split(','):
        a, b = t.split(':')
        out[int(a)] = int(b)
    return out


def table(s):
    out = []
    for t in s.split(','):
        v, n = t.split('*')
        out += [int(v)] * int(n)
    if len(out) != 21:
        raise Malformed('table of %d entries' % len(out))
    return out


def ladder(id_, stride, have):
    base = id_ - id_ % stride
    nxt = base + stride
    nxt = nxt if nxt in have else None
    prev = None
    for c in (base - stride, base - stride + 1):
        if c >= 0 and c in have:
            prev = c
            break
    return nxt, prev


def variant(count, step, base, r):
    if count == 0:
        return 0, 0, base
    idx = (r & 0xFFFF) % count
    return 1, idx, base + idx * step


def matgate(amounts, store, setting, spent):
    ok = all(a == 0 or a <= s - t for a, s, t in zip(amounts, store, setting))
    if not ok:
        return False, list(store), list(spent)
    return True, [s - min(a, s) for a, s in zip(amounts, store)], [p + a for a, p in zip(amounts, spent)]


def upcheck(e, stock, cur, nxt, nah, bed, inf, tavern, noup):
    if noup:
        return {'result': 0}
    if e == 0:
        raise Fault('zero_divisor')

    def score(slot, menge, cap):
        return min(tdiv(stock[slot] * 1536, e * menge), cap)

    food = score(0, nxt['needs'][0][1], 128)
    total, count, low = 0, 0, 160
    for j in range(1, nxt['goods_end']):
        ware, _, force = nxt['needs'][j]
        if not force:
            continue
        slot = cur['lookup'].get(ware)
        if slot is None:
            raise Fault('missing_slot')
        s = score(slot, cur['needs'][slot][1], 128)
        total += s
        count += 1
    for j in range(nxt['goods_end'], nxt['infra_end']):
        ware, _, force = nxt['needs'][j]
        if not force:
            continue
        slot = cur['lookup'].get(ware)
        if slot is None:
            raise Fault('missing_slot')
        s = score(slot, cur['needs'][slot][1], 128)
        low = min(low, s)
        if ware == tavern:
            total += s
            count += 1
    f = nah[(food + 4) >> 3]
    mean = tdiv(total, count) if count else 128
    b = bed[(mean + 4) >> 3]
    i = inf[(low + 4) >> 3]
    return {'result': int(f > 0 and b > 0 and i > 0), 'f': f, 'b': b, 'i': i}


def setgroup(goods, old, new, e, res64, stock, draws):
    flag = {}
    for ware, _, _ in new['needs']:
        flag[ware] = 1
    sat = {}
    used = 0
    sec = goods['sec']
    if old is not None:
        for j in range(old['infra_end']):
            ware, menge, _ = old['needs'][j]
            d = e * menge
            if d == 0:
                raise Fault('zero_divisor')
            s = min(tdiv(stock[j] * 1536, d), 160)
            if flag.get(ware, 0):
                sat[ware] = s
                flag[ware] = 0
            elif s > sat.get(ware, 0):
                sat[ware] = s
            if s >= 102:
                nxt = sec.get(ware, 0)
                steps = 0
                while flag.get(nxt, 0):
                    steps += 1
                    if steps > goods['count']:
                        raise Fault('idsec_cycle')
                    flag[nxt] = 0
                    sat[nxt] = s
                    nxt = sec.get(nxt, 0)
        if old['num'] > new['num']:
            for j in range(new['infra_end']):
                ware = new['needs'][j][0]
                if flag.get(ware, 0):
                    sat[ware] = 102
                    flag[ware] = 0
            e = (tdiv(res64, 64)) << 7
    elif new['num'] != 0:
        # MAP-POP-CLOSE-1 G3: the supported domain is checked BEFORE any draw (sat <= 128 for every draw, so the worst
        # case does not depend on them): E and residents fit u32 / i32 and no stock can leave u32.
        pe, pr = new['wohn'] << 7, new['wohn'] << 6
        if pe > 0xFFFFFFFF or pr > 0x7FFFFFFF:
            raise Fault('overflow')
        for j in range(new['infra_end']):
            if new['needs'][j][2] and tdiv(new['needs'][j][1] * 128 * pe, 1536) > 0xFFFFFFFF:
                raise Fault('overflow')
        if new['needs'] and tdiv(pe * new['needs'][0][1], 12) > 0xFFFFFFFF:
            raise Fault('overflow')
        for j in range(new['infra_end']):
            ware, _, force = new['needs'][j]
            if force:
                if used >= len(draws):
                    raise Malformed('not enough draws')
                sat[ware] = 128 - (draws[used] & 31)
                used += 1
                flag[ware] = 0
        e = new['wohn'] << 7
        res64 = new['wohn'] << 6
    out = []
    for j, (ware, menge, _) in enumerate(new['needs']):
        if flag.get(ware, 0):
            out.append(tdiv(e * menge, 12) if j == 0 else 0)
        else:
            out.append(tdiv(menge * sat.get(ware, 0) * e, 1536))
        if out[-1] > 0xFFFFFFFF:
            raise Fault('overflow')
    return out, e, res64, used


KEYS = {
    'ladder': {'next', 'prev'},
    'adjacent': {'ok'},
    'variant': {'draw', 'idx', 'type'},
    'matgate': {'ok', 'store', 'spent'},
    'upcheck': {'result', 'f', 'b', 'i', 'fault'},
    'setgroup': {'stock', 'e', 'res64', 'used', 'fault'},
}


def parse_kv(parts):
    kv = {}
    for p in parts:
        if '=' not in p:
            raise Malformed('not key=value: ' + p)
        k, v = p.split('=', 1)
        kv[k] = v
    return kv


def parse_group(kv):
    needs = []
    for t in kv['needs'].split(','):
        w, m, f = t.split(':')
        if int(m) < 0 or int(f) not in (0, 1):
            raise Malformed('bad need ' + t)
        needs.append((int(w), int(m), int(f)))
    g = {'num': int(kv['num']), 'needs': needs, 'infra_end': int(kv.get('infra_end', len(needs))),
         'goods_end': int(kv.get('goods_end', 0)), 'lookup': pairs(kv.get('lookup', 'none')),
         'wohn': int(kv.get('wohn', 0))}
    if g['infra_end'] > len(needs):
        raise Malformed('infra_end beyond the needs')
    return g


def run_vector(kind, kv, groups, goods):
    if kind == 'adjacent':
        # SetGroup(cur + 0x118) on upgrade, (cur - 0x118) on downgrade: the new group is the adjacent record
        old, new = int(kv['old']), int(kv['new'])
        want = old + 1 if kv['dir'] == 'up' else old - 1
        return {'ok': str(int(new == want and new >= 0))}
    if kind == 'ladder':
        n, p = ladder(int(kv['id']), int(kv['stride']), set(ints(kv['have'])))
        return {'next': 'none' if n is None else str(n), 'prev': 'none' if p is None else str(p)}
    if kind == 'variant':
        d, idx, t = variant(int(kv['count']), int(kv['step']), int(kv['base']), int(kv['r']))
        return {'draw': str(d), 'idx': str(idx), 'type': str(t)}
    if kind == 'matgate':
        ok, store, spent = matgate(ints(kv['amounts']), ints(kv['store']), ints(kv['setting']), ints(kv['spent']))
        return {'ok': str(int(ok)), 'store': fmt(store), 'spent': fmt(spent)}
    if kind == 'upcheck':
        try:
            r = upcheck(int(kv['e']), ints(kv['stock']), groups[kv['cur']], groups[kv['next']], table(kv['nah']),
                        table(kv['bed']), table(kv['inf']), int(kv.get('tavern', '-1')), int(kv.get('noup', '0')))
        except Fault as f:
            return {'fault': f.name}
        return {k: str(v) for k, v in r.items()}
    if kind == 'setgroup':
        old = None if kv['old'] == 'none' else groups[kv['old']]
        try:
            stock, e, res, used = setgroup(goods[kv['goods']], old, groups[kv['new']], int(kv['e']), int(kv['res64']),
                                           ints(kv['stock']), ints(kv['draws']))
        except Fault as f:
            return {'fault': f.name, 'used': '0'}   # a fault never draws (G3)
        return {'stock': fmt(stock), 'e': str(e), 'res64': str(res), 'used': str(used)}
    raise Malformed('unknown record ' + kind)


def run(path):
    try:
        text = open(path).read()
    except OSError:
        print('cannot read', path)
        return 2
    groups, goods = {}, {}
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
                goods[parts[1]] = {'count': int(kv['count']), 'sec': pairs(kv.get('sec', 'none'))}
            elif kind == 'grp':
                groups[parts[1]] = parse_group(parse_kv(parts[2:]))
            elif kind in KEYS:
                if '=>' not in parts:
                    raise Malformed('no =>')
                k = parts.index('=>')
                kv, exp = parse_kv(parts[2:k]), parse_kv(parts[k + 1:])
                if not exp or not set(exp) <= KEYS[kind]:
                    raise Malformed('empty or unknown result key')
                got = run_vector(kind, kv, groups, goods)
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
        print('usage: house_transition_ref.py VECTORS', file=sys.stderr)
        sys.exit(2)
    sys.exit(run(sys.argv[1]))
