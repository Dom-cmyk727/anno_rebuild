"""Reference reader for ANNO 1503 scenarios (.sze): tagged records, players
(SPIELER), ships (SCHIFF) and soldiers (SOLDAT).
Independent of crates/formats; spec: docs/formats/sze.md.

  sze.py dump [--full] <file.sze>...   same output as `anno-dump sze [--full]`
"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import chunks  # noqa: E402  (tools/chunks.py, the chunk-container reference)

NAME = re.compile(rb'[A-Z][A-Z0-9_]{0,5}')
LONG_MARKER = 0xFF00
UNITS = ('SCHIFF', 'SOLDAT')

# (parent, record) -> prefix length before a nested record sequence (sze.md, "Nesting").
NESTED = {
    ('SPIELER', 'SCHIFF'): 16, ('SPIELER', 'SOLDAT'): 16, ('SPIELER', 'STADT2'): 6,
    ('SPIELER', 'DIPL'): 0, ('SPIELER', 'FORSCH'): 0, ('STADT2', 'PROD'): 0,
    ('SCHIFF', 'PRIVAT'): 0, ('SOLDAT', 'PRIVAT'): 0, ('ENTRY', 'PRIVAT'): 0,
    ('PRIVAT', 'CREW'): 0, ('PRIVAT', 'ROUTE'): 0, ('PRIVAT', 'ESEL'): 8,
    ('CREW', 'ENTRY'): 6,
}

class Bad(Exception):
    pass

def fnv64(data):
    h = 0xcbf29ce484222325
    for b in data:
        h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return f'{h:016x}'

def quote(raw):
    """Printable ASCII except `"` and `\\` as is, everything else as \\xHH."""
    return '"' + ''.join(chr(b) if 0x20 <= b <= 0x7e and b not in (0x22, 0x5c) else f'\\x{b:02x}'
                         for b in raw) + '"'

def tagged(p):
    """Exact record sequence -> [(name, long, payload)]. Raises Bad."""
    out, o = [], 0
    while o < len(p):
        if len(p) - o < 8: raise Bad(f'truncated record header at {o}')
        field = p[o:o + 6]
        name = field.split(b'\0', 1)[0]
        if not NAME.fullmatch(name) or any(field[len(name):]):
            raise Bad(f'invalid record name at {o}')
        field, = struct.unpack_from('<H', p, o + 6)
        start, long, ln = o + 8, field >= LONG_MARKER, field
        if long:  # 24-bit length: marker low byte = bits 16-23 (sze.md)
            if len(p) - start < 2: raise Bad(f'truncated record header at {o}')
            ln = (field & 0xFF) << 16 | struct.unpack_from('<H', p, start)[0]
            start += 2
        if start + ln > len(p): raise Bad(f'record at {o} past end')
        out.append((name.decode(), long, p[start:start + ln]))
        o = start + ln
    return out

def scene(buf):
    """-> (edition, [(name, payload)] of SCENE children). Raises Bad."""
    tree, _ = chunks.parse_file(buf)
    if not tree or tree[0][0] not in ('ANNO', 'ANNOADDON1') or tree[0][3] is None:
        raise Bad('no ANNO/SCENE or ANNOADDON1/SCENE container')
    for n, off, sz, kids in tree[0][3]:
        if n == 'SCENE' and kids is not None:
            return tree[0][0], [(k, buf[o + 16:o + 16 + s]) for k, o, s, _ in kids]
    raise Bad('no ANNO/SCENE or ANNOADDON1/SCENE container')

def unit(p):
    """SCHIFF/SOLDAT payload -> dict. Raises Bad."""
    if len(p) < 16: raise Bad('unit shorter than its prefix')
    uid, fig, owner, attr, res, xb, yb = struct.unpack_from('<HHBBHII', p, 0)
    return dict(id=uid, figure=fig, owner=owner, attr=attr, reserved=res, x=xb, y=yb,
                records=tagged(p[16:]))

def nested(parent, name, p, depth):
    """--full: the nested sequence of record `name` (if any), indented by depth. -> ok"""
    k = NESTED.get((parent, name))
    if k is None: return True
    pad = '  ' * depth
    try:
        if len(p) < k: raise Bad('shorter than its prefix')
        kids = tagged(p[k:])
    except Bad:
        print(f'{pad}NESTED BAD'); return False
    if k: print(f'{pad}PREFIX {p[:k].hex()}')
    ok = True
    for n, long, r in kids:
        print(f'{pad}{n} size={len(r)} long={int(long)} hash={fnv64(r)}')
        ok = nested(name, n, r, depth + 1) and ok
    return ok

def dump(path, full):
    print(f'# {path}')
    try:
        edition, kids = scene(open(path, 'rb').read())
    except Bad as e:
        print(f'NOPARSE: {e}', file=sys.stderr); print('NOPARSE'); return False
    players = [p for n, p in kids if n == 'SPIELER']
    print(f'SCENE edition={edition} children={len(kids)} players={len(players)}')
    ok = True
    for n, p in kids:
        if n != 'INSELLNK': continue
        # INSEL5 head, then a NUL-terminated template path (sze.md, "Island links").
        if len(p) < 12 or not p.endswith(b'\0') or b'\0' in p[12:-1]:
            print('LINK BAD'); ok = False; continue
        x, y, w, h, num = struct.unpack_from('<4HI', p, 0)
        print(f'LINK x={x} y={y} width={w} height={h} number={num} path={quote(p[12:-1])}')
    for k, p in enumerate(players):
        if len(p) < 8:
            print(f'PLAYER i={k} BAD'); ok = False; continue
        flag, index = struct.unpack_from('<II', p, 0)
        try:
            recs = tagged(p[8:])
        except Bad as e:
            print(f'NOPARSE: player {k}: {e}', file=sys.stderr)
            print(f'PLAYER i={k} index={index} flag={flag} BAD'); ok = False; continue
        print(f'PLAYER i={k} index={index} flag={flag} records={len(recs)}')
        for n, long, r in recs:
            print(f'  RECORD {n} size={len(r)} long={int(long)} hash={fnv64(r)}')
            if n in UNITS:
                try:
                    u = unit(r)
                except Bad:
                    print(f'  UNIT {n} BAD'); ok = False
                else:
                    print(f"  UNIT {n} id={u['id']} figure={u['figure']} owner={u['owner']} "
                          f"attr={u['attr']} reserved={u['reserved']} x={u['x']:08x} y={u['y']:08x} "
                          f"records={','.join(m for m, _, _ in u['records'])}")
            if full:
                ok = nested('SPIELER', n, r, 2) and ok
    return ok

if __name__ == '__main__':
    sys.stdout.reconfigure(newline='\n')  # same bytes as anno-dump on Windows too
    cmd, args = sys.argv[1:2], sys.argv[2:]
    full = args[:1] == ['--full']
    files = args[1:] if full else args
    if cmd != ['dump'] or not files:
        print(__doc__, file=sys.stderr); sys.exit(2)
    sys.exit(0 if all([dump(f, full) for f in files]) else 1)
