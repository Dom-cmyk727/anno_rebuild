"""Reference reader for ANNO 1503 islands (INSEL5 / INSEL4) in .scp and .sze files.
Independent of crates/formats; spec: docs/formats/insel.md.

  insel.py dump [--full] <file.scp|file.sze>...   same output as `anno-dump insel [--full]`
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import chunks  # noqa: E402  (tools/chunks.py, the chunk-container reference)

BLOCK_RECORDS = ('HEIGHT', 'GFXID', 'GELAENDE', 'PROD2', 'FLUESSE')

class Bad(Exception):
    pass

def fnv64(data):
    h = 0xcbf29ce484222325
    for b in data:
        h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return f'{h:016x}'

def records(buf, p, end):
    """Exact chunk sequence in buf[p:end] -> [(name, payload)] (no child probing)."""
    seq = chunks.parse(buf, p, end, 6)  # depth 6 = maxdepth: no child probing
    if seq is None: raise Bad(f'not a chunk sequence at {p}')
    return [(n, buf[off + 16:off + 16 + sz]) for n, off, sz, _ in seq]

def islands(buf):
    """-> [dict(version, head, x, y, width, height, number, records)]. Raises Bad."""
    tree, _ = chunks.parse_file(buf)
    if tree is None or tree[0][3] is None: raise Bad('no root container')
    kids = [(n, buf[off + 16:off + 16 + sz], k) for n, off, sz, k in tree[0][3]]
    names = [n for n, _, _ in kids]
    if 'INSEL4' in names:
        i = names.index('INSEL4'); head = kids[i][1]
        if len(head) != 8: raise Bad('INSEL4 size')
        w, h, num = struct.unpack('<HHI', head)
        return [dict(version=4, head=head, x=0, y=0, width=w, height=h, number=num,
                     records=[(n, p) for n, p, _ in kids[i + 1:]])]
    if 'SCENE' in names:
        n, scene, k = kids[names.index('SCENE')]
        if k is None: raise Bad('SCENE is not a container')
        parent = records(scene, 0, len(scene))
    else:
        parent = [(n, p) for n, p, _ in kids]
    out = []
    for n, p in parent:
        if n != 'INSEL5': continue
        if len(p) < 12: raise Bad('INSEL5 head')
        x, y, w, h, num = struct.unpack_from('<4HI', p, 0)
        out.append(dict(version=5, head=p[:12], x=x, y=y, width=w, height=h, number=num,
                        records=records(p, 12, len(p))))
    return out

def blocks(isl, name):
    out = []
    for n, p in isl['records']:
        if n != name: continue
        if len(p) < 6: raise Bad('block header')
        x, y, bw, bh = struct.unpack_from('<HHBB', p, 0)
        out.append((x, y, bw, bh, p[6:]))
    return out

def grid(isl, name, cell):
    w, h = isl['width'], isl['height']
    out = [None] * (w * h)
    for x, y, bw, bh, data in blocks(isl, name):
        if len(data) != bw * bh * cell: raise Bad('block data size')
        if x + bw > w or y + bh > h: raise Bad('block outside island')
        for j in range(bh):
            for i in range(bw):
                t = (y + j) * w + x + i
                if out[t] is not None: raise Bad('tile covered twice')
                c = data[(j * bw + i) * cell:(j * bw + i + 1) * cell]
                out[t] = c[0] if cell == 1 else c[0] | c[1] << 8
    if any(v is None for v in out): raise Bad('tile not covered')
    return out

def dump(path, full):
    print(f'# {path}')
    try:
        isls = islands(open(path, 'rb').read())
    except Bad as e:
        print(f'NOPARSE: {e}', file=sys.stderr); print('NOPARSE'); return False
    ok = True
    print(f'ISLANDS count={len(isls)}')
    for k, i in enumerate(isls):
        print(f"ISLAND i={k} version={i['version']} x={i['x']} y={i['y']} width={i['width']} "
              f"height={i['height']} number={i['number']} head={i['head'].hex()} "
              f"records={','.join(n for n, _ in i['records'])}")
        for n, p in i['records']:
            if n not in BLOCK_RECORDS:
                print(f'  RECORD {n} size={len(p)} hash={fnv64(p)}')
        try:
            hs, gs = grid(i, 'HEIGHT', 1), grid(i, 'GFXID', 2)
        except Bad as e:
            print(f'  GRID BAD {e}'); ok = False
        else:
            print(f"  GRID HEIGHT min={min(hs, default=0)} max={max(hs, default=0)} hash={fnv64(hs)}")
            print(f"  GRID GFXID distinct={len(set(gs))} hash={fnv64(b for g in gs for b in (g & 255, g >> 8))}")
            # GFXID = texture << 8 | u << 5 | v << 2 | r; texture 0 = open water (insel.md, "Ground")
            water = sum(1 for g in gs if g >> 8 == 0)
            print(f"  GROUND water={water} textures={','.join(str(t) for t in sorted({g >> 8 for g in gs} - {0}))}")
            if full:
                w = max(i['width'], 1)
                for y in range(0, len(hs), w):
                    print(f'    H{y // w} {bytes(hs[y:y + w]).hex()}')
                for y in range(0, len(gs), w):
                    print(f"    G{y // w} {''.join(f'{g:04x}' for g in gs[y:y + w])}")
        present = {n for n, _ in i['records']}
        for name in ('GELAENDE', 'PROD2', 'FLUESSE'):
            if name not in present: continue
            try:
                bs = blocks(i, name)
            except Bad as e:
                print(f'  BLOCKS {name} BAD {e}'); ok = False; continue
            items = sum(len(d) // 6 for *_, d in bs)
            raw = b''.join(struct.pack('<HHBB', x, y, bw, bh) + d for x, y, bw, bh, d in bs)
            print(f'  BLOCKS {name} count={len(bs)} items={items} hash={fnv64(raw)}')
    return ok

if __name__ == '__main__':
    sys.stdout.reconfigure(newline='\n')  # same bytes as anno-dump on Windows too
    cmd, args = sys.argv[1:2], sys.argv[2:]
    full = args[:1] == ['--full']
    files = args[1:] if full else args
    if cmd != ['dump'] or not files:
        print(__doc__, file=sys.stderr); sys.exit(2)
    sys.exit(0 if all([dump(f, full) for f in files]) else 1)
