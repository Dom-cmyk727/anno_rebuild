"""Reference reader for ANNO 1503 bitmap fonts (.zei).
Independent of crates/formats; spec: docs/formats/zei.md.

  zei.py dump [--full] <file.zei>...   same output as `anno-dump zei [--full]`
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import chunks  # noqa: E402  (tools/chunks.py, the chunk-container reference)

class Bad(Exception):
    pass

def fnv64(data):
    h = 0xcbf29ce484222325
    for b in data:
        h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return f'{h:016x}'

def charset(buf):
    """-> (size_a, size_b, reserved, [(code, width, data)]). Raises Bad."""
    tree, _ = chunks.parse_file(buf)
    if not tree or tree[0][0] != 'CHARSET' or tree[0][3] is None: raise Bad('no CHARSET')
    kids = tree[0][3]
    if [k[0] for k in kids] != ['HEAD', 'BODY']: raise Bad('no CHARSET')
    head = buf[kids[0][1] + 16:kids[0][1] + 16 + kids[0][2]]
    body = buf[kids[1][1] + 16:kids[1][1] + 16 + kids[1][2]]
    if len(head) != 16 + 256 * 8: raise Bad('bad HEAD')
    a, b = struct.unpack_from('<HH', head, 0)
    glyphs, pos = [], 0
    for c in range(256):
        w, n = struct.unpack_from('<II', head, 16 + 8 * c)
        if pos + n > len(body): raise Bad('bad BODY')
        glyphs.append((c, w, body[pos:pos + n])); pos += n
    if pos != len(body): raise Bad('bad BODY')
    return a, b, head[4:16], glyphs

def decode(w, data):
    """-> (width, rows as lists of levels or None). Only pixels widen. Raises Bad."""
    rows, i, x, ext = [[]], 0, 0, w
    while True:
        if i >= len(data): raise Bad('glyph')
        t = data[i]; i += 1
        if t in (0xfe, 0xff):
            x = 0
            if t == 0xff: break
            rows.append([]); continue
        if i >= len(data): raise Bad('glyph')
        n = data[i]; i += 1
        if i + n > len(data): raise Bad('glyph')
        x += t
        rows[-1].append((x, data[i:i + n])); i += n; x += n
        if n: ext = max(ext, x)
    if i != len(data): raise Bad('glyph')
    out = []
    for runs in rows:
        r = [None] * ext
        for x0, px in runs:
            for k, v in enumerate(px): r[x0 + k] = v
        out.append(r)
    return ext, out

def dump(path, full):
    print(f'# {path}')
    try:
        a, b, res, glyphs = charset(open(path, 'rb').read())
    except Bad as e:
        print(f'NOPARSE: {e}', file=sys.stderr); print('NOPARSE'); return False
    have = [g for g in glyphs if g[2]]
    print(f'CHARSET size_a={a} size_b={b} reserved={res.hex()} glyphs={len(have)}')
    ok = True
    for c, w, data in have:
        try:
            ext, rows = decode(w, data)
        except Bad:
            print(f'GLYPH code={c} width={w} length={len(data)} BAD'); ok = False; continue
        print(f'GLYPH code={c} width={w} length={len(data)} image={ext}x{len(rows)} hash={fnv64(data)}')
        if full:
            for r in rows: print('  ' + ''.join('.' if v is None else str(v) if v < 10 else '#' for v in r))
    return ok

if __name__ == '__main__':
    sys.stdout.reconfigure(newline='\n')  # same bytes as anno-dump on Windows too
    cmd, args = sys.argv[1:2], sys.argv[2:]
    full = args[:1] == ['--full']
    files = args[1:] if full else args
    if cmd != ['dump'] or not files:
        print(__doc__, file=sys.stderr); sys.exit(2)
    sys.exit(0 if all([dump(f, full) for f in files]) else 1)
