#!/usr/bin/env python3
"""Independent reference for the small farm's supply contract (MAP-SUPPLY-1 S1).

  small_farm_ref.py VECTORS

Every rule below is transcribed from the disassembly named beside it (AnnoWelt.dll sha256 f220a878510f740c...),
not from the engine. Records (key=value tokens, then `=>` and the expected keys):

  gate   last= t=                              => run= last=
  line   raw= out= cap= rate= need=            => made= raw= out= status=
  tick   s0= s1= susp= c= raw= out= act= last= t= draw= [fail=1]  => run= made= raw= out= act= last= worker= animal= draws= counters= cargo=
         (fail=1: the send this+0x118 returned no figure: the attempt and the draw happen, the counter and the cargo do not)
  credit goods= in1= raw= amount=              => raw=
  pick   mode= limit= nodes=                   => tile=
  cut    id= pred_prod= phase=                 => ok= id= phase=

The small farm 809 constants of `tick` are fixed (rates 13/4, needs 128/50, capacity 4 t = 1024 per line, 3 figures, animal
table 107 110 107 109 107 109 107 109, limits 1 2 0 1 1 0 0 32, base 106); `line` and the other records carry their own.
Exit codes: 0 all vectors agree; 1 a vector disagrees; 2 malformed input, a missing file or zero vectors.
"""
import sys

KINDS = [107, 110, 107, 109, 107, 109, 107, 109]   # u16 table 0x100c8d48
LIMITS = [1, 2, 0, 1, 1, 0, 0, 32]                 # bytes 0x100c8d59..
BASE = 106                                         # byte 0x100c8d58
R0, N0, R1, N1, CAP, MAXF = 13, 128, 4, 50, 1024, 3


class Malformed(Exception):
    pass


def kv(parts):
    out = {}
    for p in parts:
        if '=' not in p:
            raise Malformed('token ' + p)
        k, v = p.split('=', 1)
        out[k] = v
    return out


def ints(s):
    return [int(x, 0) for x in s.split(',')] if s not in ('', 'none') else []


def gate(last, t):
    # 0x1006f66c-0x1006f68a: cx = (t - last) as 16 bits; below 0x1f4 -> return; else last := last + 0x1f4 (word)
    if ((t - last) & 0xFFFF) < 0x1F4:
        return False, last
    return True, (last + 0x1F4) & 0xFFFF


def age(a):
    # 0x1006f6b2-0x1006f6cb: (a*255 + (sign adjust, 0 for a >= 0)) >> 8
    return (a * 255) >> 8


def line(raw, out, cap, rate, need):
    # 0x1006f440 (docs/re/production.md 4.5)
    if out >= cap:
        return 0, raw, out, 2
    q = (raw << 7) // need
    if q > 128:
        q = 128
    if q < 64:
        return 0, raw, out, 1
    used = (q * need) >> 7
    if used & 0xFFFF == 0:
        return 0, raw, out, 0
    raw -= used
    made = (q * rate) >> 7
    return made, raw, (out + made) & 0xFFFF, 0


def wants(rate, need, raw):
    # 384 / rate >= raw / need, both signed integer divisions (idiv): 0x1006f756-0x1006f779 and 0x1006f8a8-0x1006f8cb
    return 384 // rate >= raw // need


def tick(s0, s1, susp, c, raw, out, act, last, t, draws, fail=0):
    run, last2 = gate(last, t)
    res = dict(run=int(run), made=[0, 0], raw=list(raw), out=list(out), act=list(act), last=last2, worker=0, animal=0,
               draws=0, counters=list(c), cargo=0)
    if not run:
        return res
    if not s0 and not s1:                                  # 0x1006f680-0x1006f695: after the stamp
        return res
    if susp:                                               # 0x1006f69b-0x1006f6a9: both lines age, switched on or not
        res['act'] = [age(act[0]), age(act[1])]
        return res
    if s0:
        res['act'][0] = age(act[0])
        m, r2, o2, _ = line(raw[0], out[0], CAP, R0, N0)
        res['made'][0], res['raw'][0], res['out'][0] = m, r2, o2
        res['act'][0] = (res['act'][0] + (m << 4)) & 0xFFFF
        if (m & 0xFFFF) == 0 and c[0] < 1 and wants(R0, N0, res['raw'][0]):
            res['worker'] = 1                              # job 0x2a
            if not fail:                                   # 0x1006f797-0x1006f799: a null figure skips the cargo and the attach
                res['counters'][0] += 1                    # attach 0x1006fdb0 raises byte [+0x2a + type - 106]
                res['cargo'] = 68                          # type+0xb0(0): input goods 0
    if s1:
        res['act'][1] = age(act[1])
        m, r2, o2, _ = line(raw[1], out[1], CAP, R1, N1)
        res['made'][1], res['raw'][1], res['out'][1] = m, r2, o2
        res['act'][1] = (res['act'][1] + (m << 4)) & 0xFFFF
        d = draws[0]
        res['draws'] = 1                                   # one draw, after the line step, before the limit tests
        kind = KINDS[d & 7]
        i = kind - BASE
        if LIMITS[i] <= c[i] - (1 if (m & 0xFFFF) != 0 else 0):
            return res
        if MAXF <= sum(c[1:5]):
            return res
        if wants(R1, N1, res['raw'][1]):
            res['animal'] = kind                           # job 0x2e
            if not fail:                                   # 0x1006f8fd: je skips the cargo on a null figure
                res['counters'][i] += 1
                res['cargo'] = 69                          # type+0xb0(1): input goods 1
    return res


def credit(goods, in1, raw, amount):
    # 0x1006f960: slot = (goods == input goods 1) ? 1 : 0; raw[slot] += amount as a word, no clip
    s = 1 if goods == in1 else 0
    raw = list(raw)
    raw[s] = (raw[s] + amount) & 0xFFFF
    return raw


def pick(mode, limit, nodes):
    # callback 0x10058a40: a claimed tile -> continue (answer 1); an unclaimed goal -> ctx := tile, best := node, then
    # `limit > node score (u16 at node+8, slot 0xa0 = 0x10003200)` continues, else the search stops
    tile = None
    for x, y, score, claimed in nodes:
        if claimed:
            continue
        tile = (x, y)
        if not (limit > score):
            break
    return tile


def cut(id_, pred_prod, phase):
    # 0x10087340: predecessor id-1 must be PRODTYPE ROHSTWACHS (226); the tile id becomes id-1, phase &= 0x80
    if pred_prod != 226:
        return 0, id_, phase
    return 1, id_ - 1, phase & 0x80


def fmt(v):
    return ','.join(map(str, v))


def run_vector(kind, a):
    if kind == 'gate':
        r, l = gate(int(a['last']), int(a['t']))
        return {'run': str(int(r)), 'last': str(l)}
    if kind == 'line':
        m, r, o, s = line(int(a['raw']), int(a['out']), int(a['cap']), int(a['rate']), int(a['need']))
        return {'made': str(m), 'raw': str(r), 'out': str(o), 'status': str(s)}
    if kind == 'tick':
        r = tick(int(a['s0']), int(a['s1']), int(a['susp']), ints(a['c']), ints(a['raw']), ints(a['out']),
                 ints(a['act']), int(a['last']), int(a['t']), ints(a.get('draw', '0')), int(a.get('fail', '0')))
        return {'run': str(r['run']), 'made': fmt(r['made']), 'raw': fmt(r['raw']), 'out': fmt(r['out']),
                'act': fmt(r['act']), 'last': str(r['last']), 'worker': str(r['worker']), 'animal': str(r['animal']),
                'draws': str(r['draws']), 'counters': fmt(r['counters']), 'cargo': str(r['cargo'])}
    if kind == 'credit':
        return {'raw': fmt(credit(int(a['goods']), int(a['in1']), ints(a['raw']), int(a['amount'])))}
    if kind == 'pick':
        nodes = []
        for n in a['nodes'].split(';'):
            x, y, s, c = [int(v) for v in n.split(':')]
            nodes.append((x, y, s, c))
        t = pick(a['mode'], int(a['limit']), nodes)
        return {'tile': 'none' if t is None else '%d:%d' % t}
    if kind == 'cut':
        ok, i, p = cut(int(a['id']), int(a['pred_prod']), int(a['phase'], 0))
        return {'ok': str(ok), 'id': str(i), 'phase': hex(p)}
    raise Malformed('unknown record ' + kind)


KEYS = {
    'gate': {'run', 'last'},
    'line': {'made', 'raw', 'out', 'status'},
    'tick': {'run', 'made', 'raw', 'out', 'act', 'last', 'worker', 'animal', 'draws', 'counters', 'cargo'},
    'credit': {'raw'},
    'pick': {'tile'},
    'cut': {'ok', 'id', 'phase'},
}


def run(path):
    try:
        text = open(path).read()
    except OSError:
        print('cannot read', path)
        return 2
    n, bad = 0, 0
    for ln, raw in enumerate(text.split('\n'), 1):
        line_ = raw.split('#', 1)[0].strip()
        if not line_:
            continue
        parts = line_.split()
        try:
            kind = parts[0]
            if kind not in KEYS or '=>' not in parts:
                raise Malformed('record')
            k = parts.index('=>')
            args, exp = kv(parts[2:k]), kv(parts[k + 1:])
            if not exp or not set(exp) <= KEYS[kind]:
                raise Malformed('result keys')
            got = run_vector(kind, args)
            n += 1
            for key, want in exp.items():
                if got.get(key) != want:
                    print('%s:%d: %s: %s expected %s, reference computed %s' % (path, ln, parts[1], key, want, got.get(key)))
                    bad += 1
        except (Malformed, KeyError, ValueError, IndexError) as exc:
            print('%s:%d: malformed: %r' % (path, ln, exc))
            return 2
    if n == 0:
        print('no vectors in', path)
        return 2
    print('%d vectors, %d disagreements' % (n, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('usage: small_farm_ref.py VECTORS', file=sys.stderr)
        sys.exit(2)
    sys.exit(run(sys.argv[1]))
