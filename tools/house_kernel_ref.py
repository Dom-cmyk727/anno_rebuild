#!/usr/bin/env python3
"""Independent integer reference for the pure House kernel (MAP-POP-IMPL-1 package D).

Written from the instruction spans of AnnoWelt.dll (the House tick 0x10076230-0x100765b9 and the
mood function 0x10075fd0-0x10076224), without reading the C++ kernel. It runs standalone over a
vector file (fixtures/synthetic/sim/house_kernel_vectors.txt) whose expected values were derived by
hand from the same instructions before any C++ existed.

FORMULA AND UNITS CONTRACT (all integers; "/" below is division truncating toward zero)
  Inputs: now u32 (tick), last u16, period (the gate period, 1..65535), mask u32 (player stage mask),
  E u32 (the decayed resident sum), res i32 (residents in 1/64 resident), stock[j] u32 per need,
  flags u8 (only 0x10 and 0x20 are read), prev (previous group's Wohnmax or absent), walkers
  (count, type maximum; optional). Group: wohnmax, no_downgrade, vermehr[-2..2] (i8 in 1/64 resident),
  three 21-entry i8 tables nahrung/bedarf/infra, needs [(ware, force, menge u16 >= 1)]; the goods table
  gives bau_first[ware] (stage index 0..31); goods_end / infra_end come from group_derived_ref.derive.
  1. Gate: if (now - last) mod 2**16 < period: result Skipped, nothing changes.
     Else last' = (last + period) mod 2**16.
  2. E' = trunc(127*E/128) + trunc(res/64); E' < 0 -> NotEvaluated(NegativeSmoothedE).
  3. lowest = 0x7fffffff; for each need j in order whose stage bit (mask >> bau_first) & 1 is set:
       stock'[j] = trunc(127*stock[j]/128); d = E' * menge; d == 0 -> NotEvaluated(ZeroDivisor);
       s = min(trunc(1536*stock'[j]/d), 160); lowest = min(lowest, s).   Other stocks are unchanged.
  4. spawn_condition (only when walkers given) = lowest < 102 and type_max > count.  Never a spawn.
  5. mood (E' == 0 -> 0 before any table is read; need_count == 0 -> NotEvaluated(MissingFirstNeed)):
       score(j, cap) = min(trunc(1536*stock'[j]/(E'*menge_j)), cap)
       sum, cnt over forced needs in slots 1..goods_end-1 (cap 128) and over slots goods_end..infra_end-1
       whose ware == tavern (a FORCED tavern also feeds sum/cnt with min(score, 128));
       lowest_i = min(160, score(j, 160) of every forced need in slots goods_end..infra_end-1);
       f = nahrung[trunc((score(0, 128) + 4) / 8)]  (slot 0 positional, no force test);
       avg = 128 if cnt == 0 else trunc(sum / cnt); b = bedarf[trunc((avg + 4) / 8)];
       i = infra[trunc((lowest_i + 4) / 8)].
       any of f, b, i < 0: flag 0x20 -> -2, else min(f, b, i).   (flag 0x10 is not read here)
       else flags & 0x30: -2 if flag 0x20 else -1; else trunc((f + b + i + 1) / 3).
     mood outside -2..2 -> NotEvaluated(MoodOutOfRange).
  6. growth: sub1 = trunc(res/64); res1 = res + vermehr[mood]; add1 = trunc(res1/64);
       res1 outside i32 -> NotEvaluated(Overflow).
     mood >= 0: if res1 > wohnmax*64: cap_sub = trunc(res1/64) - wohnmax, res_final = wohnmax*64,
       deferred effect UpgradeCheck; else res_final = res1.
     mood < 0: floor = 64 and arm 'remove' candidate when no_downgrade or flags & 0x30, else
       48*prev (prev absent -> NotEvaluated(PreviousCapacityUnavailable)). res1 < floor: remove arm
       (res_final = res1, deferred Remove) or, for floor 48*prev, the downgrade arm (deferred Downgrade);
       the downgrade arm only: res1 < 64 -> b64 = trunc(res1/64) - 64 and res_final = 64.
  Everything else (walker spawn, upgrade, downgrade, removal, PRNG) is a labelled deferred effect.

Vector file grammar (one record per line, '#' comments):
  table NAME v0,v1,...(21)
  group NAME wohnmax=N nodown=0|1 vermehr=a,b,c,d,e nahrung=T bedarf=T infra=T needs=ware:force:menge,...
        [bau=ware:stage,...] [roles=first,last,tavern,alias]
  vec NAME group=G now= last= period= mask= e= res= stock=a,b,.. [flags=] [prev=] [walkers=count/max]
        => key=value ...   (status= is required; every named key is checked)
  mood NAME f= b= i= flags= => mood=N
  Result keys: status last e stock lowest spawn mood res csub cadd capsub b64 arm effects reason cap
  An 'evaluated' vector must name at least mood, res and e (a nominal-success check). Exit codes: 0 all
  vectors agree; 1 a vector disagrees; 2 malformed input or zero vectors.
"""
import os
import sys
import re

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import group_derived_ref as gd  # noqa: E402  (derive() only)

GOODS = 143
RESULT_KEYS = {'status', 'last', 'e', 'stock', 'lowest', 'spawn', 'mood', 'res', 'csub', 'cadd',
               'capsub', 'b64', 'arm', 'effects', 'reason', 'cap'}


class Malformed(Exception):
    pass


def tdiv(a, b):
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def combine(f, b, i, flags):
    if f < 0 or b < 0 or i < 0:
        if flags & 0x20:
            return -2
        return min(f, b, i)
    if flags & 0x30:
        return -2 if flags & 0x20 else -1
    return tdiv(f + b + i + 1, 3)


def kernel(g, bau, roles, inp):
    """-> dict of result keys."""
    r = {'status': 'evaluated'}
    if not 1 <= inp['period'] <= 65535:
        return {'status': 'not_evaluated', 'reason': 'gate_period_out_of_domain'}
    if (inp['now'] - inp['last']) % 65536 < inp['period']:
        r.update(status='skipped', last=inp['last'])
        return r
    r['last'] = (inp['last'] + inp['period']) % 65536
    e1 = tdiv(127 * inp['e'], 128) + tdiv(inp['res'], 64)
    if e1 < 0:
        return dict(r, status='not_evaluated', reason='negative_smoothed_e')
    r['e'] = e1
    needs = g['needs']
    stock = list(inp['stock'])
    lowest = 0x7FFFFFFF
    for j, (ware, force, menge) in enumerate(needs):
        if (inp['mask'] >> bau.get(ware, 0)) & 1:
            stock[j] = tdiv(127 * stock[j], 128)
            d = e1 * menge
            if d == 0:
                return dict(r, status='not_evaluated', reason='zero_divisor')
            lowest = min(lowest, min(tdiv(1536 * stock[j], d), 160))
    r['stock'] = stock
    r['lowest'] = lowest
    if inp.get('walkers') is not None:
        r['spawn'] = int(lowest < 102 and inp['walkers'][1] > inp['walkers'][0])
    if e1 == 0:
        mood = 0
    else:
        if not needs:
            return dict(r, status='not_evaluated', reason='missing_first_need')
        dv = gd.derive([(w, bool(f)) for w, f, _ in needs], g['id_sec'], GOODS, roles)
        first, last, tavern, _alias = roles

        def score(j, cap):
            return min(tdiv(1536 * stock[j], e1 * needs[j][2]), cap)
        total, cnt, low_i = 0, 0, 160
        for j in range(1, dv['goods_end']):
            if needs[j][1]:
                total += score(j, 128)
                cnt += 1
        for j in range(dv['goods_end'], dv['infra_end']):
            if needs[j][1]:
                low_i = min(low_i, score(j, 160))
                if needs[j][0] == tavern:
                    total += score(j, 128)
                    cnt += 1
        f = g['nahrung'][tdiv(score(0, 128) + 4, 8)]
        avg = 128 if cnt == 0 else tdiv(total, cnt)
        b = g['bedarf'][tdiv(avg + 4, 8)]
        i = g['infra'][tdiv(low_i + 4, 8)]
        mood = combine(f, b, i, inp['flags'])
    r['mood'] = mood
    if not -2 <= mood <= 2:
        return dict(r, status='not_evaluated', reason='mood_out_of_range')
    res = inp['res']
    r['csub'] = tdiv(res, 64)
    res1 = res + g['vermehr'][mood + 2]
    if not -2**31 <= res1 < 2**31:
        return dict(r, status='not_evaluated', reason='overflow')
    r['cadd'] = tdiv(res1, 64)
    r['arm'] = 'none'
    r['effects'] = 'none'
    cap = g['wohnmax'] * 64
    if mood >= 0:
        r['cap'] = int(res1 > cap)
        if res1 > cap:
            r['capsub'] = tdiv(res1, 64) - g['wohnmax']
            res1 = cap
            r['effects'] = 'upgrade_check'
    else:
        remove_floor = g['nodown'] or inp['flags'] & 0x30
        if remove_floor:
            floor = 64
        elif inp.get('prev') is None:
            return dict(r, status='not_evaluated', reason='previous_capacity_unavailable')
        else:
            floor = 48 * inp['prev']
        if res1 < floor:
            if remove_floor:
                r['arm'], r['effects'] = 'remove', 'remove'
            else:
                r['arm'], r['effects'] = 'downgrade', 'downgrade'
                if res1 < 64:
                    r['b64'] = tdiv(res1, 64) - 64
                    res1 = 64
    r['res'] = res1
    return r


def ints(text):
    return [int(x) for x in text.split(',')] if text else []


def parse_kv(tokens):
    kv = {}
    for t in tokens:
        if '=' not in t:
            raise Malformed('token without =: ' + t)
        k, v = t.split('=', 1)
        kv[k] = v
    return kv


def fmt(key, val):
    if key == 'stock':
        return ','.join(map(str, val))
    return str(val)


def run(path):
    if not os.path.isfile(path):
        print('cannot read', path)
        return 2
    tables, groups, nvec, bad = {}, {}, 0, 0
    for n, raw in enumerate(open(path).read().split('\n'), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        where = '%s:%d' % (path, n)
        try:
            if parts[0] == 'table':
                v = ints(parts[2])
                if len(v) != 21:
                    raise Malformed('table needs 21 entries')
                tables[parts[1]] = v
            elif parts[0] == 'group':
                kv = parse_kv(parts[2:])
                needs = [tuple(int(x) for x in t.split(':')) for t in kv['needs'].split(',') if t]
                if any(m < 1 for _, _, m in needs):
                    raise Malformed('menge must be >= 1')
                v = ints(kv['vermehr'])
                if len(v) != 5:
                    raise Malformed('vermehr needs 5 entries')
                roles = tuple(ints(kv.get('roles', '76,90,76,29')))
                groups[parts[1]] = dict(
                    wohnmax=int(kv['wohnmax']), nodown=int(kv.get('nodown', '0')), vermehr=v,
                    nahrung=tables[kv['nahrung']], bedarf=tables[kv['bedarf']],
                    infra=tables[kv['infra']], needs=needs, id_sec={}, roles=roles,
                    bau={int(a): int(b) for a, b in (t.split(':') for t in kv.get('bau', '').split(',') if t)})
            elif parts[0] in ('vec', 'mood'):
                if '=>' not in parts:
                    raise Malformed('no =>')
                k = parts.index('=>')
                kv, exp = parse_kv(parts[2:k]), parse_kv(parts[k + 1:])
                if 'status' not in exp:
                    raise Malformed('expectation without status')
                if not set(exp) <= RESULT_KEYS:
                    raise Malformed('unknown result key')
                if parts[0] == 'mood':
                    got = {'mood': combine(int(kv['f']), int(kv['b']), int(kv['i']), int(kv['flags']))}
                    got['status'] = 'evaluated'
                else:
                    g = groups[kv['group']]
                    inp = dict(now=int(kv['now']), last=int(kv['last']), period=int(kv['period']),
                               mask=int(kv['mask'], 0), e=int(kv['e']), res=int(kv['res']),
                               stock=ints(kv['stock']), flags=int(kv.get('flags', '0'), 0),
                               prev=int(kv['prev']) if 'prev' in kv else None,
                               walkers=tuple(int(x) for x in kv['walkers'].split('/')) if 'walkers' in kv else None)
                    if len(inp['stock']) != len(g['needs']):
                        raise Malformed('stock count != need count')
                    got = kernel(g, g['bau'], g['roles'], inp)
                    if exp['status'] == 'evaluated' and not {'mood', 'res', 'e'} <= set(exp):
                        raise Malformed('an evaluated vector must name mood, res and e')
                nvec += 1
                for key, want in exp.items():
                    have = fmt(key, got.get(key, '<absent>'))
                    if have != want:
                        print('%s: %s: %s expected %s, reference computed %s' % (where, parts[1], key, want, have))
                        bad += 1
            else:
                raise Malformed('unknown record ' + parts[0])
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
        print('usage: house_kernel_ref.py VECTORS', file=sys.stderr)
        sys.exit(2)
    sys.exit(run(sys.argv[1]))
