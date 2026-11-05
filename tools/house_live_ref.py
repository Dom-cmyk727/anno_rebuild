#!/usr/bin/env python3
"""Independent integer reference for the live House update (MAP-POP-LIVE-1 L4).

Written from docs/sim/population-live.md sections 1, 2, 6 and 10 (cases X15, X16) and the disassembly facts they cite
(AnnoWelt 0x10076230-0x100765b9, the stage pass 0x10029610, the house choice 0x10076670, billing 0x1002dac0), without
reading the C++ wiring. It runs standalone over fixtures/synthetic/sim/house_live_vectors.txt, whose expected values
were derived by hand before the C++ wiring existed.

FORMULA CONTRACT (integers; "/" truncates toward zero; u16 = modulo 65536)
  stage (city stage pass; the stored player value also gets bit 26 when it holds bit 25, 27 or 28): d = u16(t - stamp); d < 2500 -> nothing changes (the stamp stays). Else stamp = t; for each
    table record (stage, group, count) whose bit is NOT set in the city mask: if above[group] >= count set the bit.
    Then if (player & 0x7fffff) >= mask (unsigned) nothing; else player |= mask (the full dword).
  ledger (one stall purchase of the house choice and the billing): demand = menge*e*256/44236 - stock/16;
    R = res64/64; thr = R*menge*256/4096; the stall offers its whole store and is accepted only when store > thr;
    amt = min(store, demand, cap) when that is > 0 and the offer was accepted, else 0 (no purchase);
    billed += amt (the requested amount); store' = store - min(amt, store); the delivery adds amt*16 to the stock.
  cadence (the gate of the House kernel, one step per call): a call at tick t takes when u16(t - last) >= period, then
    last = u16(last + period); else nothing.
  chain (two taken gates around one purchase, hand-derived numbers of the scheduler test): kernel formulas of
    population-contract D1-D12 for the taken tick: e1 = 127*e/128 + res64/64; selected needs (stage bit of the player
    mask) decay stock = 127*stock/128; lowest = min score (cap 160) of selected needs, score = 1536*stock/(e1*menge);
    spawn = lowest < 102 and walkers < 2; mood from the three scores with the table lookups (index (s+4)>>3), flags 0:
    any negative -> min, else (f+b+i+1)/3; growth res64 += vermehr[mood+2]; the delivery of the purchase is `ledger`
    plus the IdSec replay on the substitute's slot (stock += amt*16 on both slots).
Vector grammar: see the records in the fixture. Exit codes: 0 all vectors agree; 1 a vector disagrees; 2 malformed
input, a missing file or zero vectors.
"""
import sys


class Malformed(Exception):
    pass


def tdiv(a, b):
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def ints(s):
    return [] if s in ('', 'none') else [int(x, 0) for x in s.split(',')]


def fmt(v):
    return ','.join(map(str, v)) if v else 'none'


def parse_kv(parts):
    out = {}
    for p in parts:
        if '=' not in p:
            raise Malformed('not key=value: ' + p)
        k, v = p.split('=', 1)
        out[k] = v
    return out


def rl(s):
    out = []
    for t in s.split(','):
        v, n = t.split('*')
        out += [int(v)] * int(n)
    if len(out) != 21:
        raise Malformed('table of %d entries' % len(out))
    return out


def stage(kv):
    table = [tuple(int(x) for x in r.split(':')) for r in kv['table'].split(',')]
    above = {}
    if kv.get('above', 'none') != 'none':
        for r in kv['above'].split(','):
            g, n = r.split(':')
            above[int(g)] = int(n)
    mask, stamp, t, player = int(kv['mask'], 0), int(kv['stamp'], 0), int(kv['t'], 0), int(kv['player'], 0)
    if ((t - stamp) & 0xFFFF) < 2500:
        return {'stamp': str(stamp), 'mask': hex(mask), 'player': hex(player)}
    stamp = t & 0xFFFF
    for st, g, count in table:
        if not (mask >> st) & 1 and above.get(g, 0) >= count:
            mask |= 1 << st
    if (player & 0x7FFFFF) < mask:
        player |= mask
        # SetProperty 0x21 (AnnoWelt 0x10020d51-0x10020ecb): a stored value with bit 25 (via the latch restore of
        # bit 26), 27 or 28 also gets bit 26; applied only when the OR was performed (the stored path)
        if player & (1 << 25 | 1 << 27 | 1 << 28):
            player |= 1 << 26
    return {'stamp': str(stamp), 'mask': hex(mask), 'player': hex(player)}


def ledger(kv):
    menge, e, res64 = int(kv['menge']), int(kv['e']), int(kv['res64'])
    stock, cap, store = int(kv['stock']), int(kv['cap']), int(kv['store'])
    demand = tdiv(menge * e * 256, 44236) - tdiv(stock, 16)
    thr = tdiv(tdiv(res64, 64) * menge * 256, 4096)
    amt = 0
    if store > thr:
        amt = min(store, demand, cap)
        amt = amt if amt > 0 else 0
    return {'demand': str(demand), 'thr': str(thr), 'amt': str(amt), 'store2': str(store - min(amt, store)),
            'billed': str(amt), 'delivered': str(amt * 16)}


def cadence(kv):
    last, period = int(kv['last']), int(kv['period'])
    takes = []
    for t in ints(kv['visits']):
        if ((t - last) & 0xFFFF) >= period:
            last = (last + period) & 0xFFFF
            takes.append(t)
    return {'takes': fmt(takes), 'last': str(last)}


def gate(e, res64, stock, menge, bau, player, nah, bed, inf, vermehr, walkers, wohnmax):
    e1 = tdiv(127 * e, 128) + tdiv(res64, 64)
    stock = list(stock)
    lowest = 0x7FFFFFFF
    for j in range(len(stock)):
        if (player >> bau[j]) & 1:
            stock[j] = tdiv(127 * stock[j], 128)
            lowest = min(lowest, min(tdiv(1536 * stock[j], e1 * menge[j]), 160))
    s0 = min(tdiv(1536 * stock[0], e1 * menge[0]), 128)
    others = [min(tdiv(1536 * stock[j], e1 * menge[j]), 128) for j in range(1, len(stock))]
    avg = tdiv(sum(others), len(others)) if others else 128
    f, b, i = nah[(s0 + 4) >> 3], bed[(avg + 4) >> 3], inf[(160 + 4) >> 3]
    mood = min(f, b, i) if min(f, b, i) < 0 else (f + b + i + 1) // 3
    res = res64 + vermehr[mood + 2]
    res = min(res, wohnmax * 64)
    return {'e': e1, 'stock': stock, 'lowest': lowest, 'mood': mood, 'res': res,
            'spawn': 1 if lowest < 102 and walkers < 2 else 0}


def chain(kv):
    menge, bau = ints(kv['menge']), ints(kv['bau'])
    nah, bed, inf = rl(kv['nah']), rl(kv['bed']), rl(kv['inf'])
    vermehr, wohnmax, player = ints(kv['vermehr']), int(kv['wohnmax']), int(kv['player'], 0)
    sec = int(kv['idsec'])  # the slot the replay lands on (-1 = none)
    g1 = gate(int(kv['e']), int(kv['res64']), ints(kv['stock']), menge, bau, player, nah, bed, inf, vermehr, 0, wohnmax)
    ldg = ledger({'menge': str(menge[0]), 'e': str(g1['e']), 'res64': str(g1['res']), 'stock': str(g1['stock'][0]),
                  'cap': kv['cap'], 'store': kv['store']})
    amt = int(ldg['amt'])
    stock = list(g1['stock'])
    stock[0] += amt * 16
    if sec >= 0:
        stock[sec] += amt * 16
    g2 = gate(g1['e'], g1['res'], stock, menge, bau, player, nah, bed, inf, vermehr, 0, wohnmax)
    return {'g1_e': str(g1['e']), 'g1_stock': fmt(g1['stock']), 'g1_lowest': str(g1['lowest']),
            'g1_mood': str(g1['mood']), 'g1_res': str(g1['res']), 'g1_spawn': str(g1['spawn']), 'amt': str(amt),
            'store2': ldg['store2'], 'billed': ldg['billed'], 'stock_after': fmt(stock), 'g2_e': str(g2['e']),
            'g2_stock': fmt(g2['stock']), 'g2_lowest': str(g2['lowest']), 'g2_mood': str(g2['mood']),
            'g2_res': str(g2['res']), 'g2_spawn': str(g2['spawn'])}


KEYS = {
    'stage': {'stamp', 'mask', 'player'},
    'ledger': {'demand', 'thr', 'amt', 'store2', 'billed', 'delivered'},
    'cadence': {'takes', 'last'},
    'chain': {'g1_e', 'g1_stock', 'g1_lowest', 'g1_mood', 'g1_res', 'g1_spawn', 'amt', 'store2', 'billed',
              'stock_after', 'g2_e', 'g2_stock', 'g2_lowest', 'g2_mood', 'g2_res', 'g2_spawn'},
}
RUN = {'stage': stage, 'ledger': ledger, 'cadence': cadence, 'chain': chain}


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
            got = RUN[kind](kv)
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
        print('usage: house_live_ref.py VECTORS', file=sys.stderr)
        sys.exit(2)
    sys.exit(run(sys.argv[1]))
