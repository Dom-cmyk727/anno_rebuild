"""Reference reader for ANNO 1503 .tex (palettes, textures) and .inc (ACCESS) files.
Independent of crates/formats; spec: docs/formats/tex.md, docs/formats/palette.md.

  tex.py dump [--full] <file>...   same output as `anno-dump tex [--full]`
  tex.py inc <file>...             same output as `anno-dump inc`
  tex.py colors [--full] <Anlagen.dat>...  same output as `anno-dump colors [--full]`
  tex.py reencode <file>...        per file: `<file> rle=<n> identical=<n>` (RLE round trip)
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import chunks  # noqa: E402  (tools/chunks.py, the chunk-container reference)

FORMATS_RLE = (b'BSH_RGB', b'BSH_RGBH')
FORMAT_RAW = b'TEX_RGB'

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

def subchunks(buf, start, end):
    """Exact chunk sequence in buf[start:end] -> {name: (payload_start, size)} (first wins)."""
    seq = chunks.parse(buf, start, end, 1)
    if seq is None: raise Bad('entry body is not a chunk sequence')
    subs = {}
    for n, off, sz, _ in seq: subs.setdefault(n, (off + 16, sz))
    return seq, subs

def entries(buf):
    """-> list of ('COLOR', dict) / ('TEXTURE', dict) in file order. Raises Bad."""
    tree, _ = chunks.parse_file(buf)
    if tree is None: raise Bad('no root chunk')
    _, _, size, kids = tree[0]
    if size == 0: return []
    if kids is None: raise Bad('root is not a container')
    out = []
    for name, off, sz, _ in kids:
        p, end = off + 16, off + 16 + sz
        if sz < 8: raise Bad(f'{name} shorter than its prefix')
        flag, eid = struct.unpack_from('<II', buf, p)
        seq, subs = subchunks(buf, p + 8, end)
        if name == 'COLOR':
            if len(seq) != 1 or 'BODY' not in subs or subs['BODY'][1] != 1024:
                raise Bad('bad COLOR/BODY')
            b = subs['BODY'][0]
            rgb = [(buf[b + 4*i + 2], buf[b + 4*i + 1], buf[b + 4*i]) for i in range(256)]
            out.append(('COLOR', dict(id=eid, flag=flag, rgb=rgb)))
        elif name == 'TEXTURE':
            if 'HEAD' not in subs or subs['HEAD'][1] != 36 or 'BODY' not in subs:
                raise Bad('bad TEXTURE/HEAD or BODY')
            h = subs['HEAD'][0]
            w, ht, pal, u06, u08 = struct.unpack_from('<5H', buf, h)
            u18, u1a = struct.unpack_from('<2H', buf, h + 0x18)
            b, bl = subs['BODY']
            nm = subs.get('NAME')
            out.append(('TEXTURE', dict(
                id=eid, flag=flag, width=w, height=ht, palette=pal, u06=u06, u08=u08,
                format=buf[h + 0x0a:h + 0x14], key=buf[h + 0x14:h + 0x18], u18=u18, u1a=u1a,
                u1c=buf[h + 0x1c:h + 0x24], body=buf[b:b + bl],
                name=None if nm is None else buf[nm[0]:nm[0] + nm[1]])))
        else:
            raise Bad(f'unexpected chunk {name}')
    return out

def decode_rle(data, w, h):
    """BSH_RGB row RLE -> list of w*h (None = transparent | index)."""
    px = [None] * (w * h)
    i = x = y = 0
    while True:
        if i >= len(data): raise Bad('missing end marker')
        b = data[i]; i += 1
        if b in (0xFE, 0xFF):
            y += 1; x = 0
            if b == 0xFF: break
            continue
        if i >= len(data): raise Bad('truncated run')
        count = data[i]; i += 1
        if i + count > len(data): raise Bad('truncated run')
        x += b
        if y >= h or x + count > w: raise Bad('run outside image')
        px[y * w + x:y * w + x + count] = data[i:i + count]
        i += count; x += count
    if y != h: raise Bad('row count differs from height')
    if i != len(data): raise Bad('bytes after end marker')
    return px

RLE_MAX_RUN = 253  # largest skip + count of one run

def encode_rle(px, w, h):
    """Inverse of decode_rle, byte-identical to the original encoder (tex.md)."""
    out = bytearray()
    for y in range(h):
        row = px[y * w:(y + 1) * w]; x = 0
        while x < w:
            gap = 0
            while x + gap < w and row[x + gap] is None: gap += 1
            x += gap
            n = 0
            while x + n < w and row[x + n] is not None: n += 1
            while gap > RLE_MAX_RUN: out += bytes([RLE_MAX_RUN, 0]); gap -= RLE_MAX_RUN
            if n == 0: out += bytes([gap, 0]); break
            skip = gap
            while n > 0:
                c = min(n, RLE_MAX_RUN - skip)
                out += bytes([skip, c]) + bytes(row[x:x + c]); x += c; n -= c; skip = 0
        out.append(0xFE if y < h - 1 else 0xFF)
    return bytes(out)

def reencode(path):
    try:
        ts = [t for k, t in entries(open(path, 'rb').read())
              if k == 'TEXTURE' and cstr(t['format']) in FORMATS_RLE]
        same = sum(encode_rle(decode(t), t['width'], t['height']) == t['body'] for t in ts)
    except Bad as e:
        print(f'{path} ERROR {e}'); return False
    print(f'{path} rle={len(ts)} identical={same}')
    return same == len(ts)

def decode(t):
    fmt = cstr(t['format'])
    if fmt in FORMATS_RLE: return decode_rle(t['body'], t['width'], t['height'])
    if fmt == FORMAT_RAW:
        if len(t['body']) != t['width'] * t['height']: raise Bad('raw size is not width * height')
        return list(t['body'])
    raise Bad('unknown pixel format')

def dump_tex(path, full):
    buf = open(path, 'rb').read()
    print(f'# {path}')
    try:
        es = entries(buf)
    except Bad as e:
        print(f'NOPARSE: {e}', file=sys.stderr); print('NOPARSE'); return False
    n_pal = sum(1 for k, _ in es if k == 'COLOR')
    print(f'TEX palettes={n_pal} textures={len(es) - n_pal}')
    for kind, e in es:
        if kind == 'COLOR':
            print(f"PALETTE id={e['id']} flag={e['flag']} rgb={palette_hash(e['rgb'])}")
            if full:
                for line in palette_lines(e['rgb']): print(line)
            continue
        try:
            px = decode(e)
        except Bad as err:
            print(f"texture {e['id']}: {err}", file=sys.stderr); px = None
        if px is None:
            opaque, pixels = 0, 'BAD'
        else:
            opaque = sum(1 for p in px if p is not None)
            pixels = fnv64(bytes(b for p in px for b in ((0, 0) if p is None else (1, p))))
        name = '-' if e['name'] is None else quote(e['name'])
        print(f"TEXTURE id={e['id']} flag={e['flag']} width={e['width']} height={e['height']} "
              f"palette={e['palette']} u06={e['u06']} u08={e['u08']} format={quote(e['format'])} "
              f"key={e['key'].hex()} u18={e['u18']} u1a={e['u1a']} u1c={e['u1c'].hex()} "
              f"body={len(e['body'])} name={name} opaque={opaque} pixels={pixels}")
        if full and px is not None:
            w = e['width']
            for y in range(e['height']):
                cells = ''.join('..' if p is None else f'{p:02x}' for p in px[y * w:(y + 1) * w])
                print(f'  R{y} {cells}')
    return True

def color_tables(buf):
    """Anlagen.dat ANNO/COLORS/ENTRY -> [dict(id, flag, rgb, name)]. Raises Bad.
    Layout: docs/formats/palette.md; mirrors anno_formats::colors."""
    tree, _ = chunks.parse_file(buf)
    if tree is None: raise Bad('no root chunk')
    col = next((k for k in tree[0][3] or [] if k[0] == 'COLORS'), None)
    if col is None: raise Bad('no ANNO/COLORS')
    out = []
    for name, off, sz, _ in col[3] or []:
        if name != 'ENTRY': raise Bad('child is not ENTRY')
        p = buf[off + 16:off + 16 + sz]
        if len(p) < 0x419: raise Bad('entry too short')
        u16 = lambda o: struct.unpack_from('<H', p, o)[0]
        if p[8:12] != b'BODY' or u16(0x0c) != 0: raise Bad('no BODY record')
        if u16(0x0e) != 1024: raise Bad('BODY length is not 1024')
        if p[0x410:0x414] != b'NAME' or u16(0x414) != 0: raise Bad('no NAME record')
        nm = p[0x418:]
        if u16(0x416) != len(nm): raise Bad('NAME length does not end the entry')
        if nm.find(0) != len(nm) - 1: raise Bad('NAME is not one NUL-terminated string')
        flag, eid = struct.unpack_from('<II', p, 0)
        rgb = [(p[0x10 + 4*i + 2], p[0x10 + 4*i + 1], p[0x10 + 4*i]) for i in range(256)]
        out.append(dict(id=eid, flag=flag, rgb=rgb, name=nm))
    return out

def palette_lines(rgb):
    for row in range(16):
        yield f"  P{row} " + ' '.join('%02x%02x%02x' % rgb[row * 16 + i] for i in range(16))

def palette_hash(rgb):
    return fnv64(bytes(c for e in rgb for c in e))

def dump_colors(path, full):
    buf = open(path, 'rb').read()
    print(f'# {path}')
    try:
        ts = color_tables(buf)
    except Bad as e:
        print(f'NOPARSE: {e}', file=sys.stderr); print('NOPARSE'); return False
    print(f'COLORS entries={len(ts)}')
    for t in ts:
        print(f"COLORTABLE id={t['id']} flag={t['flag']} rgb={palette_hash(t['rgb'])} name={quote(t['name'])}")
        if full:
            for line in palette_lines(t['rgb']): print(line)
    return True

def access_records(buf):
    """-> [(source bytes, number)] or None, mirroring anno_formats::inc::parse_access."""
    tree, end = chunks.parse_file(buf)
    if tree is None or end != len(buf): return None
    kids = tree[0][3]
    if kids is None or len(kids) != 1 or kids[0][0] != 'ACCESS': return None
    _, off, sz, _ = kids[0]
    rest = buf[off + 16:off + 16 + sz]
    out = []
    while rest:
        if b'\0' not in rest: return None
        rec, rest = rest.split(b'\0', 1)
        if b',' not in rec: return None
        src, digits = rec.rsplit(b',', 1)
        if not digits or not all(0x30 <= c <= 0x39 for c in digits): return None
        n = int(digits)
        if n > 0xffffffff: return None
        out.append((src, n))
    return out

def dump_inc(path):
    buf = open(path, 'rb').read()
    print(f'# {path}')
    recs = access_records(buf)
    if recs is None: print('NOPARSE'); return False
    print(f'INC records={len(recs)}')
    for src, n in recs: print(f'ACCESS source={quote(src)} number={n}')
    return True

if __name__ == '__main__':
    sys.stdout.reconfigure(newline='\n')  # same bytes as anno-dump on Windows too
    cmd, args = sys.argv[1:2], sys.argv[2:]
    full = cmd in (['dump'], ['colors']) and args[:1] == ['--full']
    files = args[1:] if full else args
    run = {'dump': lambda f: dump_tex(f, full), 'inc': dump_inc, 'colors': lambda f: dump_colors(f, full),
           'reencode': reencode}
    if len(cmd) != 1 or cmd[0] not in run or not files:
        print(__doc__, file=sys.stderr); sys.exit(2)
    ok = all([run[cmd[0]](f) for f in files])
    sys.exit(0 if ok else 1)
