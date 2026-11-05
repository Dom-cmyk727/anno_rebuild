#!/usr/bin/env python3
"""Independent reference for the early cloth chain's contracts (MAP-NEEDS-2 N1).

  early_needs_ref.py VECTORS

Every rule is transcribed from the disassembly named beside it (AnnoWelt.dll sha256 f220a878510f7407...), not from the engine.
Records (key=value tokens, then `=>` and the expected keys):

  gate    last= t=                                          => run= last=
  farm    on= susp= max= rate= need= cap= wout= raw= out= act= last= t= => run= made= raw= out= act= status= send=
          (the Farm tick 0x1006ea90; max= is type max_figures(0), wout= the attached figures)
  route   id=                                               => job= kind= limit=
          (the farm family's job is the figure type's ROUTETYPE index; the shared job's case table 0x1003b0e0)
  credit  home= goods= in1= raw= amount=                    => raw=
  pick    limit= nodes=                                     => tile=
  trip    cap= tiles=                                       => amount= cuts=
  wstep   susp= n_in= need= rate= cap= in= out= carriers= max=  => made= in= out= status= send=
  offer   sum= need= avail= cap= prio_in=                   => prio= score=
  visit   delay= stamp= t=                                  => delay= surplus= worked= stamp=
          (the figure's first micro-step: 5 ticks owed, consumed across visits; elapsed = (u16)(t - stamp); 0 elapsed = no work)
  arrive  home= goods= in1= raw= amount=                    => raw= lost=
          (step C's arrival: no home or cargo goods 0 delivers nothing, the cargo is lost)
  attach  slots= fig=                                       => slots= ret=
  detach  slots= fig= times=                                => slots=
          (Farm attach 0x10074540 / detach 0x1006f070: five pointer slots)
  create  flags= entrance=                                  => fig=
          (this+0x118 = 0x10072d20 returns null when flag bits 4/5 are set or the entrance search 0x10072b50 fails)
  send    tests= flags= entrance= input0=                   => order= draws= attached= cargo=
          (the dispatch order: tests, one world draw, this+0x118, then the cargo goods; a null figure skips attach and cargo)
  stepa   cand= claimed= reach=                             => result=
  farmrun raw= rate= need= cap= steps=                      => out= raw= steps=
  wrun    in= rate= need= cap= steps=                       => out= in= steps=

Exit codes: 0 all vectors agree; 1 a vector disagrees; 2 malformed input, a missing file or zero vectors.
"""
import sys

MAX_RUN = 100000          # bound of the *run records: a reference never loops without end


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
    # 0x1006eaa7: cx = (t - last) as 16 bits; below 0x1f4 -> return; else last := last + 0x1f4 (word, 0x1006eab2-0x1006eabb)
    if ((t - last) & 0xFFFF) < 0x1F4:
        return False, last
    return True, (last + 0x1F4) & 0xFFFF


def age_farm(a):
    # 0x1006eacc-0x1006eadf: (a*255 + adj) >> 8, adj 0 for a >= 0
    return (a * 255) >> 8


def farm(on, susp, mx, rate, need, cap, wout, raw, out, act, last, t):
    """0x1006ea90. Returns dict(run made raw out act status send)."""
    r = dict(run=0, made=0, raw=raw, out=out, act=act, status=0, send=0)
    run, _ = gate(last, t)
    if not run:
        return r
    r['run'] = 1
    if not on:                                   # 0x1006eabb: byte [+0x22] == 0 -> return (the stamp already advanced)
        return r
    act = age_farm(act)
    r['act'] = act
    if susp:                                     # 0x1006eae3-0x1006eaf1: flag bits 4/5 -> return
        return r
    made = 0
    produce = True
    if mx == 1 and wout > 0:                     # 0x1006eb0e-0x1006eb2e: max_figures == 1 and a slot is taken: skip production
        produce = False
    if produce:
        if out >= cap:                           # 0x1006eb40: cmp bp, [+0x18]; jbe -> storage full (status 2)
            r['status'] = 2
        else:
            q = (raw << 7) // need               # 0x1006eb66-0x1006eb71 (idiv)
            if q > 128:
                q = 128
            if q < 64:                           # 0x1006eb81: below 0x40 -> raw material missing (status 1)
                r['status'] = 1
            else:
                raw -= (q * need) >> 7           # 0x1006eb93-0x1006eba3
                made = (q * rate) >> 7           # 0x1006ebb3-0x1006ebc2
                out = (out + made) & 0xFFFF      # 0x1006ec34 add word
                act = (act + (made << 4)) & 0xFFFF   # 0x1006ec38-0x1006ec3b
    r.update(made=made, raw=raw, out=out, act=act)
    # dispatch tests 0x1006ed78-0x1006ede4
    if mx <= 1 and made != 0:                    # 0x1006ed78-0x1006ed85: cmp al,1; ja -> go on; else made != 0 -> return
        return r
    if wout >= mx:                               # 0x1006eda5-0x1006edac: attached figures >= max -> return
        return r
    if 384 // rate < raw // need:                # 0x1006edc1-0x1006ede4: 0x180 / rate  <  raw / need -> return (signed idiv)
        return r
    r['send'] = 1
    return r


# The shared job's case table (AnnoWelt 0x1003b0e0, docs/re/forestry.md 2.4): jobs 0x29/0x2a -> the worker case, jobs
# 0x2d/0x2e -> the animal case; the step-A callback limit is 0 for the worker and 0x14 for the animal (0x10058bfd / 0x1005932d).
# A farm's job is its figure type's ROUTETYPE index (0x1006ee27, slot +0x54); the names are the table at AnnoGame 0x10024fb8.
def route(rid):
    job = rid                                       # the farm passes the ROUTETYPE index itself as the job number
    if job in (0x29, 0x2A):
        return job, 'worker', 0
    if job in (0x2D, 0x2E):
        return job, 'animal', 20
    return job, 'none', 0


def credit(home, goods, in1, raw, amount):
    if home == 'farm':
        # 0x1006ee70: [+0x1c] += amount as a word, no goods test, one raw store
        return [(raw[0] + amount) & 0xFFFF]
    # SmallFarm 0x1006f960: slot = (goods == input goods 1) ? 1 : 0
    s = 1 if goods == in1 else 0
    raw = list(raw)
    raw[s] = (raw[s] + amount) & 0xFFFF
    return raw


def visit(delay, stamp, t):
    # 0x10034760 step 3 (docs/re/forestry.md 2.13): no elapsed ticks -> no work and the stamp is NOT written; else the stamp
    # := t, the saved first micro-step (5 ticks at the spawn) is consumed first, the surplus goes on to the body
    e = (t - stamp) & 0xFFFF
    if e == 0:
        return delay, 0, 0, stamp
    used = min(delay, e)
    delay -= used
    e -= used
    if delay > 0:
        return delay, 0, 0, t & 0xFFFF
    return delay, e, 1, t & 0xFFFF


def arrive(home, goods, in1, raw, amount):
    # 0x10057bd0 arrival: with a home and cargo goods != 0 -> home +0xc0(goods, amount); else nothing, the cargo is lost
    if home == 'none' or goods == 0:
        return list(raw), amount
    return credit(home, goods, in1, raw, amount), 0


def attach(slots, fig):
    # 0x10074540: first free of five slots; all full -> -1 (no counter exists)
    slots = list(slots)
    for i in range(5):
        if slots[i] == 0:
            slots[i] = fig
            return slots, 0
    return slots, -1


def detach(slots, fig, times):
    # 0x1006f070: clears the first slot that equals the figure; no match -> nothing
    slots = list(slots)
    for _ in range(times):
        for i in range(5):
            if slots[i] == fig:
                slots[i] = 0
                break
    return slots


def create(flags, entrance):
    # 0x10072d20: refused while (f >> 1 & 0x10) | (f & 0x10) != 0 (flag bits 5/4) or without a reachable entrance (0x10072b50)
    if (((flags >> 1) & 0x10) | (flags & 0x10)) != 0 or not entrance:
        return 0
    return 1


def send(tests, flags, entrance, input0):
    # 0x1006eda5-0x1006ede4 tests, 0x1006edf3-0x1006ee06 one world draw (discarded), 0x1006ee2d this+0x118, 0x1006ee4d cargo
    order = []
    draws = attached = cargo = 0
    if not tests:
        return order, draws, attached, cargo
    order.append('tests')
    order.append('draw')
    draws = 1
    order.append('create')
    if create(flags, entrance):                  # 0x1006ee33 test eax,eax; je -> skips attach and cargo
        order.append('cargo')
        attached = 1
        cargo = input0
    return order, draws, attached, cargo


def stepa(cand, claimed, reach):
    # candidates minus claimed; none, or none reachable by the walk -> the trip ends empty
    return 'take' if (cand - claimed) > 0 and reach > 0 else 'none'


def pick(limit, nodes):
    # callback 0x10058a40: claimed -> continue; unclaimed goal -> take it, go on only while limit > score
    tile = None
    for x, y, score, claimed in nodes:
        if claimed:
            continue
        tile = (x, y)
        if not (limit > score):
            break
    return tile


def trip(cap, tiles):
    # 0x1005c260: after a cut the job repeats step A while capacity > amount; every cut credits 256 (0x10054530);
    # step A ends the trip when no tile is left (the cargo is then delivered as it is)
    amount, cuts = 0, 0
    while cap > amount and cuts < tiles:
        amount += 256
        cuts += 1
    return amount, cuts


def wstep(susp, n_in, need, rate, cap, ins, out, carriers, mx):
    """Workshop tick 0x100680b0 after the gate (docs/re/production.md 4.1)."""
    ins = list(ins)
    status = 0
    made = 0
    if susp:
        return made, ins, out, status, 0
    ratio = 128
    for i in range(n_in):
        ratio = min(ratio, (ins[i] << 7) // need)
    if n_in > 0 and ratio < 64:
        status = 1
    elif out >= cap:
        status = 2
    else:
        made = (rate * ratio) >> 7
        out += made
        for i in range(n_in):
            ins[i] -= (need * ratio) >> 7
    send = 1 if (carriers < mx and n_in > 0) else 0
    return made, ins, out, status, send


def offer(total, need, avail, cap, prio_in):
    # workshop input priority 0x10068500: steps = (stock + reserved + carried) / need; < 30 -> 0x80, < 60 -> 0x60, else -1
    # source score 0x10072974-0x100729a1: avail >= 1 t (256), priority > 0, score = priority + avail * 192 / cap
    steps = total // need
    prio = 0x80 if steps < 30 else (0x60 if steps < 60 else -1)
    if prio_in is not None:
        prio = prio_in
    if avail < 256 or prio <= 0 or cap == 0:
        return prio, -1
    return prio, prio + avail * 192 // cap


def fmt(v):
    return ','.join(map(str, v))


def run_vector(kind, a):
    if kind == 'gate':
        r, l = gate(int(a['last']), int(a['t']))
        return {'run': str(int(r)), 'last': str(l)}
    if kind == 'farm':
        r = farm(int(a['on']), int(a['susp']), int(a['max']), int(a['rate']), int(a['need']), int(a['cap']),
                 int(a['wout']), int(a['raw']), int(a['out']), int(a['act']), int(a['last']), int(a['t']))
        return {k: str(v) for k, v in r.items()}
    if kind == 'route':
        j, k, l = route(int(a['id']))
        return {'job': hex(j), 'kind': k, 'limit': str(l)}
    if kind == 'credit':
        return {'raw': fmt(credit(a['home'], int(a['goods']), int(a['in1']), ints(a['raw']), int(a['amount'])))}
    if kind == 'pick':
        nodes = []
        for n in a['nodes'].split(';'):
            x, y, s, c = [int(v) for v in n.split(':')]
            nodes.append((x, y, s, c))
        t = pick(int(a['limit']), nodes)
        return {'tile': 'none' if t is None else '%d:%d' % t}
    if kind == 'trip':
        am, cu = trip(int(a['cap']), int(a['tiles']))
        return {'amount': str(am), 'cuts': str(cu)}
    if kind == 'wstep':
        m, i, o, s, snd = wstep(int(a['susp']), int(a['n_in']), int(a['need']), int(a['rate']), int(a['cap']),
                                ints(a['in']), int(a['out']), int(a['carriers']), int(a['max']))
        return {'made': str(m), 'in': fmt(i), 'out': str(o), 'status': str(s), 'send': str(snd)}
    if kind == 'offer':
        p, s = offer(int(a['sum']), int(a['need']), int(a['avail']), int(a['cap']),
                     int(a['prio_in']) if 'prio_in' in a else None)
        return {'prio': str(p), 'score': str(s)}
    if kind == 'visit':
        d, sur, w, st = visit(int(a['delay']), int(a['stamp']), int(a['t']))
        return {'delay': str(d), 'surplus': str(sur), 'worked': str(w), 'stamp': str(st)}
    if kind == 'arrive':
        r2, lost = arrive(a['home'], int(a['goods']), int(a['in1']), ints(a['raw']), int(a['amount']))
        return {'raw': fmt(r2), 'lost': str(lost)}
    if kind == 'attach':
        sl, ret = attach(ints(a['slots']), int(a['fig']))
        return {'slots': fmt(sl), 'ret': str(ret)}
    if kind == 'detach':
        return {'slots': fmt(detach(ints(a['slots']), int(a['fig']), int(a['times'])))}
    if kind == 'create':
        return {'fig': str(create(int(a['flags'], 0), int(a['entrance'])))}
    if kind == 'send':
        o, dr, at, cg = send(int(a['tests']), int(a['flags'], 0), int(a['entrance']), int(a['input0']))
        return {'order': '>'.join(o) if o else 'none', 'draws': str(dr), 'attached': str(at), 'cargo': str(cg)}
    if kind == 'stepa':
        return {'result': stepa(int(a['cand']), int(a['claimed']), int(a['reach']))}
    if kind == 'farmrun':
        raw, out, rate, need, cap, steps = int(a['raw']), 0, int(a['rate']), int(a['need']), int(a['cap']), 0
        while steps < min(int(a['steps']), MAX_RUN):
            r = farm(1, 0, 5, rate, need, cap, 0, raw, out, 0, 0, 500)   # one due step at a time
            if r['made'] == 0:
                break
            raw, out, steps = r['raw'], r['out'], steps + 1
        return {'out': str(out), 'raw': str(raw), 'steps': str(steps)}
    if kind == 'wrun':
        ins, out, rate, need, cap, steps = int(a['in']), 0, int(a['rate']), int(a['need']), int(a['cap']), 0
        while steps < min(int(a['steps']), MAX_RUN):
            m, i, o, s, _ = wstep(0, 1, need, rate, cap, [ins], out, 0, 1)
            if m == 0:
                break
            ins, out, steps = i[0], o, steps + 1
        return {'out': str(out), 'in': str(ins), 'steps': str(steps)}
    raise Malformed('unknown record ' + kind)


KEYS = {
    'gate': {'run', 'last'},
    'farm': {'run', 'made', 'raw', 'out', 'act', 'status', 'send'},
    'route': {'job', 'kind', 'limit'},
    'credit': {'raw'},
    'pick': {'tile'},
    'trip': {'amount', 'cuts'},
    'wstep': {'made', 'in', 'out', 'status', 'send'},
    'offer': {'prio', 'score'},
    'visit': {'delay', 'surplus', 'worked', 'stamp'},
    'arrive': {'raw', 'lost'},
    'attach': {'slots', 'ret'},
    'detach': {'slots'},
    'create': {'fig'},
    'send': {'order', 'draws', 'attached', 'cargo'},
    'stepa': {'result'},
    'farmrun': {'out', 'raw', 'steps'},
    'wrun': {'out', 'in', 'steps'},
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
        except (Malformed, KeyError, ValueError, IndexError, ZeroDivisionError) as exc:
            print('%s:%d: malformed: %r' % (path, ln, exc))
            return 2
    if n == 0:
        print('no vectors in', path)
        return 2
    print('%d vectors, %d disagreements' % (n, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('usage: early_needs_ref.py VECTORS', file=sys.stderr)
        sys.exit(2)
    sys.exit(run(sys.argv[1]))
