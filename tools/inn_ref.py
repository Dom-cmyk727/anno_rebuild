#!/usr/bin/env python3
"""Independent integer reference for the Inn (MAP-POP-CLOSE-2 K1).

Written from docs/re/population.md section 18 (AnnoWelt.dll: Inn tick 0x100755f0, take 0x100756e0, unload
0x100756d0, base priority 0x10072870 and base pick 0x100728f0) without reading the C++ module. It runs
standalone over a vector file (fixtures/synthetic/sim/inn_vectors.txt) whose expected values were derived
by hand before any C++ existed.

CONTRACT (all integers; units are 8.8 tons, 256 = 1 t; "//" floors, every operand here is non-negative
unless a record says signed)
  tick(last, tick, taken, stock, flags, limit, count):
    diff = (tick - last) mod 65536; diff < 5000 -> gate closed, nothing changes, send 0.
    open: last = (last + 5000) mod 65536; taken = (31*taken) // 32 (BEFORE the switched-off test, so a
    switched-off Inn still decays); flags & 0x30 != 0 -> send 0; not (limit > count) -> send 0 (signed);
    send iff stock < 0x100 or stock < (taken >> 1) with the DECAYED taken.
  take(stock, taken, request signed): n = min(request, stock) (signed); taken = (taken + n) mod 65536;
    stock = (stock - n) mod 65536; returns n (a negative request keeps n negative).
  unload(stock, amount): stock = (stock + amount) mod 65536 (no clip, no x16, 8.8 unit).
  priority(stock, need): need == 0 -> refused (the original traps); steps = stock // need; steps < 30 -> 128
    (0x80), steps < 60 -> 96 (0x60), else -1.
  offer(stock, need, input, avail, cap, out=A,B): a source qualifies for the input goods when its output 0 is
    ALL (0x8e) or equals the goods, or (output 0 not ALL) its output 1 equals the goods; avail >= 256 and
    priority > 0 and cap != 0; score = priority + (avail*192) // cap; -1 when it does not qualify.

Vector grammar (one record per line, '#' comments):
  tick NAME last= tick= taken= stock= flags= limit= count= => gate=open|closed last= taken= send=0|1
  take NAME stock= taken= request= => returned= stock= taken=
  unload NAME stock= amount= => stock=
  priority NAME stock= need= => prio=N|refused
  offer NAME stock= need= input= avail= cap= out=A,B => score=
Exit codes: 0 all vectors agree; 1 a vector disagrees; 2 malformed input, a missing file or zero vectors.
"""
import sys

ALL = 0x8E


class Malformed(Exception):
    pass


def parse_kv(parts):
    kv = {}
    for p in parts:
        if '=' not in p:
            raise Malformed('not key=value: ' + p)
        k, v = p.split('=', 1)
        kv[k] = v
    return kv


def tick(last, t, taken, stock, flags, limit, count):
    if (t - last) % 65536 < 5000:
        return {'gate': 'closed', 'last': str(last), 'taken': str(taken), 'send': '0'}
    last = (last + 5000) % 65536
    taken = (31 * taken) // 32
    send = 0
    if flags & 0x30 == 0 and limit > count:
        send = 1 if (stock < 0x100 or stock < (taken >> 1)) else 0
    return {'gate': 'open', 'last': str(last), 'taken': str(taken), 'send': str(send)}


def take(stock, taken, request):
    n = min(request, stock)
    return {'returned': str(n), 'stock': str((stock - n) % 65536), 'taken': str((taken + n) % 65536)}


def priority(stock, need):
    if need == 0:
        return None
    steps = stock // need
    return 128 if steps < 30 else (96 if steps < 60 else -1)


def offer(stock, need, goods, avail, cap, out):
    if out[0] == ALL or out[0] == goods:
        ok = True
    else:
        ok = out[1] == goods
    if not ok or avail < 256 or cap == 0:
        return -1
    p = priority(stock, need)
    if p is None or p <= 0:
        return -1
    return p + (avail * 192) // cap


def run_vector(kind, kv):
    if kind == 'tick':
        return tick(int(kv['last']), int(kv['tick']), int(kv['taken']), int(kv['stock']), int(kv['flags']),
                    int(kv['limit']), int(kv['count']))
    if kind == 'take':
        return take(int(kv['stock']), int(kv['taken']), int(kv['request']))
    if kind == 'unload':
        return {'stock': str((int(kv['stock']) + int(kv['amount'])) % 65536)}
    if kind == 'priority':
        p = priority(int(kv['stock']), int(kv['need']))
        return {'prio': 'refused' if p is None else str(p)}
    if kind == 'offer':
        out = [int(x) for x in kv['out'].split(',')]
        if len(out) != 2:
            raise Malformed('out needs two goods')
        return {'score': str(offer(int(kv['stock']), int(kv['need']), int(kv['input']), int(kv['avail']),
                                   int(kv['cap']), out))}
    raise Malformed('unknown record ' + kind)


KEYS = {
    'tick': {'gate', 'last', 'taken', 'send'},
    'take': {'returned', 'stock', 'taken'},
    'unload': {'stock'},
    'priority': {'prio'},
    'offer': {'score'},
}


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
            if kind not in KEYS:
                raise Malformed('unknown record ' + kind)
            if '=>' not in parts:
                raise Malformed('no =>')
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
        print('usage: inn_ref.py VECTORS', file=sys.stderr)
        sys.exit(2)
    sys.exit(run(sys.argv[1]))
