#!/usr/bin/env python3
"""Independent reference for the hunter building's tick arithmetic (MAP-NEEDS-2, read-only RE of AnnoWelt.dll 0x10069f40,
sha256 f220a878510f7407...). The engine does NOT implement the hunter (class Hunter is inert); these vectors are the
decoded contract only, no engine counterpart exists.

  hunter_ref.py VECTORS

Record (key=value tokens, then `=>` and the expected keys):
  hunter s0= s1= split= rate= need= cap0= cap1= worker= susp= raw= out0= out1= act0= act1= last= t=
         => run= made0= made1= raw= out0= out1= act0= act1= last= elig=
`elig` = the dispatch tests (0x1006a31b-0x1006a361) pass, i.e. the target search 0x10071da0(8) would run; what that search finds
(wildlife) and the job-0xd figure are NOT modelled. Exit codes: 0 agree, 1 disagree, 2 malformed/missing/zero vectors.
"""
import sys

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

def hunter(s0, s1, split, rate, need, cap0, cap1, worker, susp, raw, out0, out1, act0, act1, last, t):
    r = dict(run=0, made0=0, made1=0, raw=raw, out0=out0, out1=out1, act0=act0, act1=act1, last=last, elig=0)
    if ((t - last) & 0xFFFF) < 0x1F4:                    # 0x10069f5d: cmp cx, 0x1f4
        return r
    r['run'] = 1
    r['last'] = (last + 0x1F4) & 0xFFFF                  # 0x10069f68-0x10069f6d
    if not s0 and not s1:                                # 0x10069f71-0x10069f7d: both line switches 0 -> return
        return r
    act0 = (act0 * 127) >> 7                             # 0x10069f83-0x10069fb3: both activities age by 127/128
    act1 = (act1 * 127) >> 7
    r['act0'], r['act1'] = act0, act1
    if susp:                                             # 0x10069fb7-0x10069fc5: flag bits 4/5 -> return
        return r
    ratio = 0 if worker else 0x80                        # 0x10069fce-0x10069fea: a worker out -> ratio 0, skip production
    produced = False
    if ratio != 0:
        q = (raw << 7) // need                           # 0x10069ff6-0x1006a01b (idiv)
        ratio = min(0x80, q)                             # 0x1006a01b-0x1006a02f
        if ratio >= 0x40:                                # 0x1006a031: below 0x40 -> starved (message), no production
            total = (rate * ratio) >> 7                  # 0x1006a070-0x1006a07f: rate of OUTPUT 0 only, times ratio
            if s0 and cap0 > out0:                       # 0x1006a096: cmp bp, cx ; jbe -> line 0 full
                share = (split * total) >> 7             # 0x1006a09e-0x1006a0b2
                r['made0'] = share
                out0 = (out0 + share) & 0xFFFF
                act0 = (act0 + (share << 4)) & 0xFFFF
                produced = True
            if s1 and cap1 > out1:                       # 0x1006a1c4: line 1 full test
                share = ((0x80 - split) * total) >> 7    # 0x1006a1d1-0x1006a1ea
                r['made1'] = share
                out1 = (out1 + share) & 0xFFFF
                act1 = (act1 + (share << 4)) & 0xFFFF
                produced = True
    if produced:                                         # 0x1006a2e8-0x1006a306: raw -= need * ratio >> 7
        raw -= (ratio * need) >> 7
    r.update(raw=raw, out0=out0, out1=out1, act0=act0, act1=act1)
    if ratio >= 0x80:                                    # 0x1006a312: a full ratio returns, no dispatch
        return r
    if worker:                                           # 0x1006a31b-0x1006a329: a worker is out -> return
        return r
    if 384 // rate < raw // need:                        # 0x1006a331-0x1006a363: 0x180 / rate  <  raw / need -> return
        return r
    r['elig'] = 1
    return r

KEYS = {'run', 'made0', 'made1', 'raw', 'out0', 'out1', 'act0', 'act1', 'last', 'elig'}

def run(path):
    try:
        text = open(path).read()
    except OSError:
        print('cannot read', path); return 2
    n = bad = 0
    for ln, raw in enumerate(text.split('\n'), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        try:
            if parts[0] != 'hunter' or '=>' not in parts:
                raise Malformed('record')
            k = parts.index('=>')
            a, exp = kv(parts[2:k]), kv(parts[k + 1:])
            if not exp or not set(exp) <= KEYS:
                raise Malformed('result keys')
            g = hunter(*(int(a[x]) for x in ('s0', 's1', 'split', 'rate', 'need', 'cap0', 'cap1', 'worker', 'susp', 'raw',
                                             'out0', 'out1', 'act0', 'act1', 'last', 't')))
            n += 1
            for key, want in exp.items():
                if str(g[key]) != want:
                    print('%s:%d: %s: %s expected %s, reference computed %s' % (path, ln, parts[1], key, want, g[key]))
                    bad += 1
        except (Malformed, KeyError, ValueError, IndexError, ZeroDivisionError) as exc:
            print('%s:%d: malformed: %r' % (path, ln, exc)); return 2
    if n == 0:
        print('no vectors in', path); return 2
    print('%d vectors, %d disagreements' % (n, bad))
    return 1 if bad else 0

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('usage: hunter_ref.py VECTORS', file=sys.stderr); sys.exit(2)
    sys.exit(run(sys.argv[1]))
