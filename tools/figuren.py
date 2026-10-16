"""Reference reader for ANNO 1503 FigurenHeader.dat and Figuren<g><nn>.dat.
Independent of crates/formats; spec: docs/formats/figuren.md.

  figuren.py dump [--full] <FigurenHeader.dat>...  same output as `anno-dump figuren [--full]`
  figuren.py frames <game_dir> <entry id>...       same output as `anno-dump frames`
  figuren.py anlagen [--full] <Anlagen.dat>...     same output as `anno-dump anlagen [--full]`
"""
import os, re, struct, sys

NAME = re.compile(rb'[A-Z][A-Z0-9_]{1,11}$')
FORMATS = {b'BSH_RGBV': (1, False), b'BSH_RGBH': (1, False), b'BSH_RGBZ': (2, False),
           b'BSH_RGBAV': (1, True), b'BSH_RGBAZ': (2, True), b'BSH_ALPHA': (0, False),
           b'BSH_RGBWZ': (2, False), b'BSH_RGBZM': (2, False), b'BSH_RGBAH': (1, True)}  # last three: Anlagen.dat only

class Bad(Exception):
    pass

def cstr(b):
    return b.split(b'\0', 1)[0]

def fnv64(data):
    h = 0xcbf29ce484222325
    for b in data:
        h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return f'{h:016x}'

def quote(raw):
    out = []
    for b in cstr(raw):
        out.append(chr(b) if 0x20 <= b <= 0x7e and b not in (0x22, 0x5c) else f'\\x{b:02x}')
    return '"' + ''.join(out) + '"'

def records(buf, p, end):
    """Exact sequence of (name, payload) records (12-byte name, u32 size) in buf[p:end]."""
    out = []
    while p < end:
        if p + 16 > end: raise Bad('truncated record header')
        name = cstr(buf[p:p + 12])
        if not NAME.match(name): raise Bad(f'bad record name at {p}')
        size = struct.unpack_from('<I', buf, p + 12)[0]
        if p + 16 + size > end: raise Bad(f'record past end at {p}')
        out.append((name.decode(), buf[p + 16:p + 16 + size]))
        p += 16 + size
    return out

def entries_of(payload):
    rs = records(payload, 0, len(payload))
    if any(n != 'ENTRY' for n, _ in rs): raise Bad('child is not ENTRY')
    return [p for _, p in rs]

def color_table(p):
    """COLORS/ENTRY payload, Anlagen.dat layout (docs/formats/palette.md)."""
    if len(p) < 0x419 or p[8:12] != b'BODY' or p[0x410:0x414] != b'NAME': raise Bad('bad colour table')
    flag, tid = struct.unpack_from('<II', p, 0)
    rgb = [(p[0x10 + 4*i + 2], p[0x10 + 4*i + 1], p[0x10 + 4*i]) for i in range(256)]
    return dict(flag=flag, id=tid, rgb=rgb, name=p[0x418:])

def frame(raw):
    w, h, pal, u06 = struct.unpack_from('<4H', raw, 0)
    ax, ay, off, ln, u20 = struct.unpack_from('<2h3I', raw, 0x14)  # anchors are signed (re/annodisplay.md §4)
    return dict(width=w, height=h, palette=pal, u06=u06, format=raw[8:0x14], ax=ax, ay=ay,
                offset=off, length=ln, u20=u20)

def gfx_set(payload, prefix_len):
    rs = records(payload, prefix_len, len(payload))
    sets, shadow = [], None
    for n, p in rs:
        if n == 'TEXTURES':
            group, cnt = struct.unpack_from('<II', p, 0)
            if len(p) != 8 + 36 * cnt: raise Bad('TEXTURES size is not 8 + 36 n')
            sets.append(dict(group=group, raw=p, frames=[frame(p[8 + 36*k:44 + 36*k]) for k in range(cnt)]))
        elif n == 'SHADOW':
            shadow = gfx_set(p, 8)
    return dict(prefix=payload[:prefix_len], records=rs, sets=sets, shadow=shadow)

def entry(p):
    """ENTRY payload (FigurenHeader.dat FIGUREN or Anlagen.dat ANLAGEN)."""
    if len(p) < 8: raise Bad('ENTRY prefix')
    flag, eid = struct.unpack_from('<II', p, 0)
    rs = records(p, 8, len(p))
    e = dict(flag=flag, id=eid, records=rs, palettes=[], ids=[], gfx=[])
    for rn, rp in rs:
        if rn == 'COLORS' and 0 < len(rp) < 16:  # ANLAGEN: u32 colour-table ids
            if len(rp) % 4: raise Bad('COLORS id list')
            e['ids'] = list(struct.unpack(f'<{len(rp) // 4}I', rp))
        elif rn == 'COLORS': e['palettes'] = [color_table(t) for t in entries_of(rp)]
        if rn == 'HAEUSER': e['gfx'] = [gfx_set(s, 16) for s in entries_of(rp)]
    return e

def parse(buf):
    if buf[:5] != b'ANNO\0' or buf[16:24] != b'FIGUREN\0': raise Bad('no ANNO/FIGUREN root')
    end = 32 + struct.unpack_from('<I', buf, 28)[0]
    if end > len(buf) or (len(buf) - end) % 24: raise Bad('bad FIGUREN size or volume headers')
    ents = []
    for n, p in records(buf, 32, end):
        if n != 'ENTRY': raise Bad('FIGUREN child is not ENTRY')
        ents.append(entry(p))
    vols = []
    for q in range(end, len(buf), 24):
        if cstr(buf[q:q + 12]) != b'TEXTURES': raise Bad('bad volume header')
        vols.append(struct.unpack_from('<III', buf, q + 12))
    return ents, vols

def dump_set(label, indent, g, full):
    other = b''.join(p for n, p in g['records'] if n not in ('TEXTURES', 'SHADOW'))
    print(f"{indent}{label} prefix={g['prefix'].hex()} records={','.join(n for n, _ in g['records'])} other={fnv64(other)}")
    for s in g['sets']:
        print(f"{indent}  TEXTURES group={s['group']} frames={len(s['frames'])} hash={fnv64(s['raw'])}")
        if full:
            for k, f in enumerate(s['frames']):
                print(f"{indent}    FRAME k={k} width={f['width']} height={f['height']} palette={f['palette']} "
                      f"u06={f['u06']} format={quote(f['format'])} anchor={f['ax']},{f['ay']} "
                      f"offset={f['offset']} length={f['length']} u20={f['u20']}")
    if g['shadow'] is not None:
        dump_set('SHADOW', indent + '  ', g['shadow'], full)

def dump_entry(e, full):
    print(f"ENTRY id={e['id']} flag={e['flag']} records={','.join(n for n, _ in e['records'])}")
    for n, p in e['records']:
        if n not in ('COLORS', 'HAEUSER'):
            print(f'  RECORD {n} size={len(p)} hash={fnv64(p)}')
    if e['ids']:
        print(f"  COLORIDS ids={','.join(map(str, e['ids']))}")
    for i, t in enumerate(e['palettes']):
        rgb = fnv64(b for c in t['rgb'] for b in c)
        print(f"  COLORTABLE i={i} id={t['id']} flag={t['flag']} rgb={rgb} name={quote(t['name'])}")
    for i, g in enumerate(e['gfx']):
        dump_set(f'SET i={i}', '  ', g, full)

def dump(path, full):
    print(f'# {path}')
    try:
        ents, vols = parse(open(path, 'rb').read())
    except Bad as e:
        print(f'NOPARSE: {e}', file=sys.stderr); print('NOPARSE'); return False
    print(f'FIGUREN entries={len(ents)} volumes={len(vols)}')
    for size, group, frames in vols:
        print(f'VOLUME group={group} size={size} frames={frames}')
    for e in ents:
        dump_entry(e, full)
    return True

def dump_anlagen(path, full):
    print(f'# {path}')
    try:
        buf = open(path, 'rb').read()
        top = records(buf, 16, 16 + struct.unpack_from('<I', buf, 12)[0])
        anl = next((p for n, p in top if n == 'ANLAGEN'), None)
        if buf[:5] != b'ANNO\0' or anl is None: raise Bad('no ANNO/ANLAGEN')
        ents = []
        for n, p in records(anl, 0, len(anl)):
            if n != 'ENTRY': raise Bad('ANLAGEN child is not ENTRY')
            ents.append(entry(p))
        vols = [(struct.unpack_from('<II', p, 0), len(p) - 8) for n, p in top if n == 'TEXTURES']
    except (Bad, struct.error) as e:
        print(f'NOPARSE: {e}', file=sys.stderr); print('NOPARSE'); return False
    print(f'ANLAGEN entries={len(ents)} volumes={len(vols)}')
    for (group, frames), size in vols:
        print(f'VOLUME group={group} frames={frames} bytes={size}')
    for e in ents:
        dump_entry(e, full)
    return True

def decode(fmt, data, w, h):
    """Frame RLE -> list of w*h (None | (index, z, level)). Raises Bad."""
    px = [None] * (w * h)
    if not data: return px
    bpp, levels = FORMATS[fmt]
    i = x = y = 0
    while True:
        if i >= len(data): raise Bad('missing end marker')
        b = data[i]; i += 1
        if b in (0xFE, 0xFF):
            y += 1; x = 0
            if b == 0xFF: break
            continue
        if i >= len(data): raise Bad('truncated run')
        c = data[i]; i += 1
        level = c >> 6 if levels else 0
        if levels: c &= 0x3F
        if i + c * bpp > len(data): raise Bad('truncated run')
        x += b
        if y >= h: raise Bad('run outside image')
        for k in range(c):
            # BSH_RGBWZ: stored pixel x lands on column 2x + (y & 1) (re/annodisplay.md §3)
            col = 2 * (x + k) + (y & 1) if fmt == b'BSH_RGBWZ' else x + k
            if col >= w: raise Bad('run outside image')
            if bpp == 0: v = (0, 0, 0)
            elif bpp == 1: v = (data[i + k], 0, level)
            else: v = (data[i + 2*k + 1], data[i + 2*k], level)
            px[y * w + col] = v
        i += c * bpp; x += c
    if y != h: raise Bad('row count differs from height')
    if i != len(data): raise Bad('bytes after end marker')
    return px

def volume(game_dir, group):
    out, part = [], 0
    while True:
        p = os.path.join(game_dir, f'Figuren{group}{part:02}.dat')
        if not os.path.exists(p): break
        out.append(open(p, 'rb').read()); part += 1
    return b''.join(out)

def frames(game_dir, ids):
    try:
        ents, _ = parse(open(os.path.join(game_dir, 'FigurenHeader.dat'), 'rb').read())
    except Bad as e:
        print(f'{game_dir}: {e}', file=sys.stderr); return False
    vols = [volume(game_dir, g) for g in range(3)]
    ok = True
    for sid in ids:
        e = next((e for e in ents if str(e['id']) == sid), None)
        if e is None:
            print(f'ENTRY id={sid} MISSING'); ok = False; continue
        print(f"ENTRY id={sid} sets={len(e['gfx'])}")
        for i, g in enumerate(e['gfx']):
            for shadow, s in ((0, g), (1, g['shadow'])):
                for fs in (s or {'sets': []})['sets']:
                    for k, f in enumerate(fs['frames']):
                        print(f"FRAME set={i} shadow={shadow} group={fs['group']} k={k} ", end='')
                        fmt = cstr(f['format'])
                        try:
                            if fmt not in FORMATS or fs['group'] >= len(vols): raise Bad('unknown format')
                            v = vols[fs['group']]
                            if f['offset'] + f['length'] > len(v): raise Bad('outside volume')
                            px = decode(fmt, v[f['offset']:f['offset'] + f['length']], f['width'], f['height'])
                            opaque = sum(p is not None for p in px)
                            pixels = fnv64(b for p in px for b in ((0,) if p is None else (1,) + p))
                        except Bad:
                            opaque, pixels, ok = 0, 'BAD', False
                        print(f"width={f['width']} height={f['height']} format={quote(f['format'])} "
                              f"opaque={opaque} pixels={pixels}")
    return ok

if __name__ == '__main__':
    sys.stdout.reconfigure(newline='\n')  # same bytes as anno-dump on Windows too
    cmd, args = sys.argv[1:2], sys.argv[2:]
    if cmd == ['dump']:
        full = args[:1] == ['--full']
        files = args[1:] if full else args
        if not files: print(__doc__, file=sys.stderr); sys.exit(2)
        ok = all([dump(f, full) for f in files])
    elif cmd == ['anlagen']:
        full = args[:1] == ['--full']
        files = args[1:] if full else args
        if not files: print(__doc__, file=sys.stderr); sys.exit(2)
        ok = all([dump_anlagen(f, full) for f in files])
    elif cmd == ['frames'] and len(args) >= 2:
        ok = frames(args[0], args[1:])
    else:
        print(__doc__, file=sys.stderr); sys.exit(2)
    sys.exit(0 if ok else 1)
