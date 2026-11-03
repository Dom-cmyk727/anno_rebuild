"""P: which projection handedness do non-square Anlagen sprites fit?

For every ANLAGEN entry with HEAD w != h and four rotation blocks, take the
first frame of each rotation block at zoom group 2, find the lowest opaque
row and its mean column, and compare its offset from the anchor with the
bottom vertex of the footprint diamond (corner (x+w', y+h') seen from the
centre (x+w'/2, y+h'/2), w' x h' = the footprint at that rotation: (w, h)
for even rotations, (h, w) for odd ones, the sim's convention):

  old projection (screen x = (x - y) tw/2):  dx = (w' - h') tw/4
  new projection (screen x = (y - x) tw/2):  dx = (h' - w') tw/4

The renderer's test "non-square buildings: every rotation block fits its
footprint in the original's handedness" (src/render/tests/
sprite_fidelity_test.cpp) asserts the counts this prints (docs/render/iso.md,
"Tiles").

Usage: python -I tools/footprint_handedness.py <game_dir>
"""
import os, struct, sys

game = sys.argv[1]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figuren as F  # noqa: E402

buf = open(os.path.join(game, 'Anlagen.dat'), 'rb').read()
top = F.records(buf, 16, 16 + struct.unpack_from('<I', buf, 12)[0])
anl = next(p for n, p in top if n == 'ANLAGEN')
vols = {}
for n, p in top:
    if n == 'TEXTURES':
        g, _ = struct.unpack_from('<II', p, 0)
        vols[g] = p[8:]
GROUP, TW = 2, 20

def lowest_col(fr, data):
    px = F.decode(F.cstr(fr['format']), data[fr['offset']:fr['offset'] + fr['length']],
                  fr['width'], fr['height'])
    w = fr['width']
    rows = {}
    for i, p in enumerate(px):
        if p is not None:
            rows.setdefault(i // w, []).append(i % w)
    if not rows:
        return None
    y = max(rows)
    cols = rows[y]
    return sum(cols) / len(cols)

fits = {'old': 0, 'new': 0, 'mixed': 0}
rows = []
for n, p in F.records(anl, 0, len(anl)):
    e = F.entry(p)
    head = next((rp for rn, rp in e['records'] if rn == 'HEAD'), None)
    if head is None or not e['gfx']:
        continue
    w, h = head[0], head[1]
    if w == h:
        continue
    g = e['gfx'][0]
    anim = next((rp for rn, rp in g['records'] if rn == 'ANIM'), None)
    frames = struct.unpack_from('<H', anim, 2)[0] if anim and len(anim) >= 4 else 1
    fs = next((s for s in g['sets'] if s['group'] == GROUP), None)
    if fs is None or frames == 0 or len(fs['frames']) != 4 * frames:
        continue
    verdict = []
    for r in range(4):
        fr = fs['frames'][r * frames]
        c = lowest_col(fr, vols[GROUP])
        if c is None:
            verdict.append('?')
            continue
        dx = c - fr['ax']
        wr, hr = (w, h) if r % 2 == 0 else (h, w)
        old, new = (wr - hr) * TW / 4, (hr - wr) * TW / 4
        verdict.append('new' if abs(dx - new) < abs(dx - old) else 'old')
    kind = verdict[0] if all(v == verdict[0] for v in verdict) else 'mixed'
    fits[kind if kind in fits else 'mixed'] += 1
    rows.append((e['id'], w, h, frames, verdict))
for r in rows:
    print(*r)
blocks = 4 * len(rows)
new = sum(v.count('new') for *_, v in rows)
old = sum(v.count('old') for *_, v in rows)
majority = sum(v.count('new') >= 3 for *_, v in rows)
print(f'types={len(rows)} blocks={blocks} fit_new={new} fit_old={old} '
      f'unanimous={fits["new"]} majority={majority}')
