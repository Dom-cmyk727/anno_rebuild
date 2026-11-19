"""UNITS-REF-1: per-figure inventory of FigurenHeader.dat (read-only, single-threaded).

python -I tools/units_inventory.py [--game-dir DIR] [--he-dir DIR] [--out FILE]

Writes one row per figure type (id, BEWOHNTYPE, ROUTETYPE, FORSCHTYPE, WARE, BAUWARE,
PATH, decoded HEAD and LOGIC fields, sprite-set count, point lists) and a summary by
BEWOHNTYPE from the user's own files to a git-ignored file (default
cache/units/inventory.md). The rows are never committed: they are the original's data in
another form (docs/UNITS.md). The game dir defaults to ANNO1503_GAME_DIR; the History
Edition dir is optional and adds the GOG/HE record diff.

Field layout: AnnoGame.dll figure-type loader 0x1000fe00 (vtable 0x10021d10 slot +0x58),
read for UNITS-REF-1 (docs/UNITS.md part B). HEAD (16 or 20 bytes): f32 [0] -> +0x2c,
f32 [4] -> +0x10 (walking speed), u8 [8] -> +0x104, u16 [0xa] -> +0xe6, u16 [0xc] -> +0xe8,
u32 [0x10] -> +0x108. LOGIC (28-44 bytes, zero-padded to 44): see LOGIC_FIELDS.
"""
import argparse, collections, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figuren as fig  # noqa: E402  (the repo's reference reader; records() only)

# (name, LOGIC offset, kind, type field, loader rule)
LOGIC_FIELDS = [
    ('f0', 0x00, 'f32', '+0x34', '<= 0 -> 1.0'),
    ('wait', 0x04, 'f32', '+0x30', '<= 0 -> 1.0 (job wait, getter +0x64(1))'),
    ('f8', 0x08, 'f32', '+0x38', '<= 0 -> 4.0'),
    ('fc', 0x0c, 'f32', '+0x3c', '<= 0 -> 4.0'),
    ('layer', 0x10, 'u8', '+0xff', 'walking layer (getter +0xac)'),
    ('cap_t', 0x11, 'u8', '+0xf2', '<< 8: carry capacity (getter +0x84)'),
    ('b12', 0x12, 'u8', '+0x14', '* 0.125 (getter +0x68(0))'),
    ('b13', 0x13, 'u8', '+0xf6', 'getter +0x88(0)'),
    ('b14', 0x14, 'u8', '+0xf7', 'getter +0x88(1)'),
    ('b15', 0x15, 'u8', '+0x101', 'getter +0x94(2)'),
    ('w16', 0x16, 'u16', '+0xf4', 'getter +0x94(1)'),
    ('flags', 0x18, 'u16', '+0xe4', '& 0xfbff (getter +0x70)'),
    ('w1a', 0x1a, 'u16', '-', 'not stored'),
    ('w1c', 0x1c, 'u16', '+0xec', 'getter +0x9c'),
    ('w1e', 0x1e, 'u16', '+0xea', 'getter +0x90'),
    ('b20', 0x20, 'u8', '+0xf9', '<< 8 via getter +0x94(0, 0)'),
    ('b21', 0x21, 'u8', '+0xfa', '<< 8 via getter +0x94(0, 1)'),
    ('b22', 0x22, 'u8', '+0xfb', '<< 8 via getter +0x94(0, 2)'),
    ('b24', 0x24, 'u8', '+0x1c', '* 0.125 (getter +0x68(2))'),
    ('b25', 0x25, 'u8', '+0x24', '* 0.125, 0 -> 7.0 (getter +0x68(4))'),
    ('b26', 0x26, 'u8', '+0xf8', 'getter +0x88(2)'),
    ('b27', 0x27, 'u8', '+0x18', 'max(b12, b27) * 0.125 (getter +0x68(1))'),
    ('b28', 0x28, 'u8', '+0x28', '* 0.125, 0 -> 2.0 (getter +0x68(5))'),
    ('b29', 0x29, 'u8', '+0x103', 'getter +0xb0(0)'),
]
GAME_RECORDS = ('HEAD', 'BEWOHNTYPE', 'ROUTETYPE', 'FORSCHTYPE', 'WARE', 'BAUWARE', 'LOGIC',
                'SCHUSSPOS', 'OBJECTPOS')


def cstr(b):
    return b.split(b'\0', 1)[0].decode('latin1')


def read(path):
    """id -> dict(records={name: payload}, order=[names], sets=n)."""
    buf = open(path, 'rb').read()
    if buf[:5] != b'ANNO\0' or buf[16:24] != b'FIGUREN\0':
        raise SystemExit(f'{path}: no ANNO/FIGUREN root')
    end = 32 + struct.unpack_from('<I', buf, 28)[0]
    out = {}
    for name, p in fig.records(buf, 32, end):
        if name != 'ENTRY':
            continue
        flag, eid = struct.unpack_from('<II', p, 0)
        rs = fig.records(p, 8, len(p))
        recs = {n: r for n, r in rs}
        sets = len(fig.entries_of(recs['HAEUSER'])) if 'HAEUSER' in recs else 0
        out[eid] = dict(flag=flag, records=recs, order=[n for n, _ in rs], sets=sets)
    return out


def head(h):
    h = h + bytes(20 - len(h)) if len(h) < 20 else h
    f0, speed = struct.unpack_from('<2f', h, 0)
    return dict(h_f0=round(f0, 4), speed=round(speed, 4), h_b8=h[8],
                h_wa=struct.unpack_from('<H', h, 0xa)[0], h_wc=struct.unpack_from('<H', h, 0xc)[0],
                h_d10=struct.unpack_from('<I', h, 0x10)[0], head_len=len(h))


def logic(l):
    n = len(l)
    l = l + bytes(44 - n) if n < 44 else l
    out = dict(logic_len=n)
    for name, off, kind, _, _ in LOGIC_FIELDS:
        if kind == 'f32':
            out[name] = round(struct.unpack_from('<f', l, off)[0], 4)
        elif kind == 'u16':
            out[name] = struct.unpack_from('<H', l, off)[0]
        else:
            out[name] = l[off]
    return out


def row(eid, e):
    r = e['records']
    d = dict(id=eid, bewohn=cstr(r.get('BEWOHNTYPE', b'')), route=cstr(r.get('ROUTETYPE', b'')),
             forsch=cstr(r.get('FORSCHTYPE', b'')), ware=cstr(r.get('WARE', b'')),
             bauware='+'.join(x for x in (cstr(s) for s in r.get('BAUWARE', b'').split(b'\0') if s) if x),
             path=cstr(r.get('PATH', b'')).replace('\\', '/'), sets=e['sets'],
             schusspos=len(r.get('SCHUSSPOS', b'')), objectpos=len(r.get('OBJECTPOS', b'')))
    d.update(head(r.get('HEAD', b'')))
    d.update(logic(r.get('LOGIC', b'')))
    return d


COLS = ['id', 'bewohn', 'route', 'forsch', 'ware', 'bauware', 'speed', 'h_f0', 'h_b8', 'h_wa', 'h_wc',
        'h_d10'] + [f[0] for f in LOGIC_FIELDS] + ['sets', 'schusspos', 'objectpos', 'path']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game-dir', default=os.environ.get('ANNO1503_GAME_DIR'))
    ap.add_argument('--he-dir')
    ap.add_argument('--out', default=os.path.join('cache', 'units', 'inventory.md'))
    a = ap.parse_args()
    if not a.game_dir:
        raise SystemExit('set ANNO1503_GAME_DIR or pass --game-dir')
    gog = read(os.path.join(a.game_dir, 'FigurenHeader.dat'))
    rows = [row(i, gog[i]) for i in sorted(gog)]
    L = [f'# Figure inventory (generated by tools/units_inventory.py; never commit)', '',
         f'Source: {a.game_dir}; {len(rows)} figure types.', '']
    L += ['## Summary by BEWOHNTYPE', '', '| BEWOHNTYPE | n | ids | ROUTETYPEs | speed | cap t | layer | wait |',
          '|---|---|---|---|---|---|---|---|']
    groups = collections.OrderedDict()
    for d in rows:
        groups.setdefault(d['bewohn'], []).append(d)
    for b, ds in sorted(groups.items()):
        ids = [d['id'] for d in ds]
        rng = lambda k: f"{min(d[k] for d in ds)}-{max(d[k] for d in ds)}"
        L.append(f"| {b} | {len(ds)} | {ids[0]}..{ids[-1]} | {','.join(sorted(set(d['route'] for d in ds)))} | "
                 f"{rng('speed')} | {rng('cap_t')} | {rng('layer')} | {rng('wait')} |")
    L += ['', '## LOGIC field map (AnnoGame 0x1001019e-0x100103a4)', '',
          '| field | LOGIC offset | kind | type field | rule |', '|---|---|---|---|---|']
    L += [f'| {n} | {o:#04x} | {k} | {t} | {r} |' for n, o, k, t, r in LOGIC_FIELDS]
    L += ['', '## Per figure type', '', '| ' + ' | '.join(COLS) + ' |', '|' + '---|' * len(COLS)]
    L += ['| ' + ' | '.join(str(d[c]) for c in COLS) + ' |' for d in rows]
    if a.he_dir:
        he = read(os.path.join(a.he_dir, 'FigurenHeader.dat'))
        L += ['', '## GOG / History Edition record diff', '',
              f'GOG {len(gog)} entries, HE {len(he)}; only in GOG: {sorted(set(gog) - set(he))}; '
              f'only in HE: {sorted(set(he) - set(gog))}.', '', '| id | records that differ |', '|---|---|']
        nd = 0
        for i in sorted(set(gog) & set(he)):
            diff = [n for n in sorted(set(gog[i]['records']) | set(he[i]['records']))
                    if n != 'HAEUSER' and n != 'COLORS' and gog[i]['records'].get(n) != he[i]['records'].get(n)]
            if gog[i]['sets'] != he[i]['sets']:
                diff.append(f"sets {gog[i]['sets']}/{he[i]['sets']}")
            if diff:
                nd += 1
                game = [n for n in diff if n in GAME_RECORDS]
                L.append(f"| {i} | {', '.join(diff)}{' (GAMEPLAY)' if game else ''} |")
        L += ['', f'{nd} entries differ outside HAEUSER/COLORS.']
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(L) + '\n')
    print(f'wrote {a.out}: {len(rows)} figure types, {len(groups)} BEWOHNTYPEs')


if __name__ == '__main__':
    main()
