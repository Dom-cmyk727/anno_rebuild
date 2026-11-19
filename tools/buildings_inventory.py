"""BUILDINGS-REF-1 part A: per-type inventory of Anlagen.dat (read-only, single-threaded).

python -I tools/buildings_inventory.py [--game-dir DIR] [--he-dir DIR] [--out FILE]

Writes the per-type tables of every building and object (footprint, flag-word class, radius,
crop object, goods, costs, upkeep, sprite sets, entrance arrows) from the user's own game files to
a git-ignored file (default cache/buildings/inventory.md). The tables are never committed: they
are the original's data in another form (docs/BUILDINGSOBJECTS.md part A). The game dir defaults
to ANNO1503_GAME_DIR; the History Edition dir is optional (adds the GOG/HE diff).

Reads ANLAGEN/ENTRY with the repo's Python oracle tools/figuren.py (records(), entry()).
Offsets: docs/re/production.md section 2 (HEAD, PROD3, WARE, ROHSTOFF), task/UI-PRODBAR-1
production.md (HEAD [0x28] crop object, f32 [0x14]), task/UI-INFO-2b ui.md 2.5 and
tools/ui_layout.py anlagen_entrance_arrows (OBJECTPOS/CONPO2), task/FERTILITY-1a
ship-placement.md section 3 / 3a (flag word classes).
"""
import collections, gc, os, struct, sys
sys.path.insert(0, r'E:\anno1503\tools')
import figuren as fig

SIDE = {0: '-x', 1: '+y', 2: '+x', 3: '-y'}  # ui.md 2.5 side offsets (-0.5,0),(0,0.5),(0.5,0),(0,-0.5)

GROUPS = [
    ('houses', {'PIONIER', 'SIEDLER', 'BUERGER', 'KAUFLEUTE', 'ARISTOKRATEN'}),
    ('production', {'HANDWERK', 'HANDWERKDUAL', 'HANDWERKWAFFEN', 'MINE', 'STEINBRUCH', 'EISLOCH',
                    'STEINMETZ', 'KANONENBAU'}),
    ('farms and fields', {'PLANTAGE', 'WEIDETIER', 'SCHAFFARM', 'UNIVERSAL', 'JAGDHAUS', 'PELZJAEGER',
                          'FISCHER', 'WALFAENGER'}),
    ('public', {'MARKT', 'MARKTSTAND', 'KAPELLE', 'KIRCHE', 'SCHULE', 'HOCHSCHULE', 'BADEHAUS', 'ARZT',
                'GERICHT', 'FEUERWEHR', 'BIBLIOTHEK', 'WIRT', 'GAUKLERPLATZ', 'GALGEN', 'PAVILLION',
                'BRUNNEN', 'ZIERDE', 'DENKMAL', 'TRIUMPH', 'SCHLOSS'}),
    ('military', {'MILITAR', 'WACHTURM', 'KANONENTURM', 'MAUER', 'MAUERUP', 'TOR', 'ZAUN'}),
    ('ships and harbour', {'KONTOR', 'WERFT', 'PIER'}),
    ('roads', {'STRASSE', 'BRUECKE', 'RAMPE', 'RAMPELOW', 'RAMPEHIGH', 'PLATZ'}),
    ('nature objects', {'ROHSTOFF', 'ROHSTWACHS', 'ROHSTDUERR', 'FELSEN', 'KLIPPE', 'STEIN', 'RUINE',
                        'TIERHOEHLE', 'FLUSS', 'WASSERFALL', 'MEER'}),
]


def group_of(pt):
    for g, s in GROUPS:
        if pt in s:
            return g
    return 'others'


def names(path):
    """Texte.dat [ANLAGEN] id:name, first line per id (ui_layout.anlagen_names)."""
    out, sec = {}, None
    if not os.path.exists(path):
        return out
    for line in open(path, 'rb').read().split(b'\n'):
        line = line.rstrip(b'\r')
        if line.startswith(b'[') and line.endswith(b']'):
            sec = line[1:-1]
            continue
        if sec == b'ANLAGEN' and b':' in line:
            k, v = line.split(b':', 1)
            if k.strip().isdigit():
                out.setdefault(int(k), v.decode('cp1252', 'replace').strip())
    return out


def strs(rp):
    return [s.decode('latin1') for s in rp.split(b'\0') if s]


def objectpos(rp):
    subs = []
    i = 0
    while i + 8 <= len(rp):
        nm = rp[i:i + 6].split(b'\0')[0].decode('latin1')
        sz = struct.unpack_from('<H', rp, i + 6)[0]
        subs.append((nm, rp[i + 8:i + 8 + sz]))
        i += 8 + sz
    return subs


def load(path):
    buf = open(path, 'rb').read()
    top = fig.records(buf, 16, 16 + struct.unpack_from('<I', buf, 12)[0])
    anl = next((p for n, p in top if n == 'ANLAGEN'), None)
    if anl is None:
        return None
    out = {}
    for n, p in fig.records(anl, 0, len(anl)):
        if n != 'ENTRY':
            continue
        e = fig.entry(p)
        r = dict(e['records'])
        head = r.get('HEAD', b'').ljust(56, b'\0')
        prod3 = r.get('PROD3', b'').ljust(68, b'\0')
        t = {'id': e['id'], 'head_len': len(r.get('HEAD', b'')), 'head': head}
        t['ptype'] = strs(r.get('PRODTYPE', b''))[0] if strs(r.get('PRODTYPE', b'')) else '?'
        t['w'], t['h'] = head[0], head[1]
        t['bricks'], t['wood'], t['tools'], t['marble'] = head[3], head[4], head[5], head[6]
        t['gold'] = struct.unpack_from('<H', head, 8)[0]
        t['stages'] = head[0x10]
        t['radius'] = head[0x11]
        t['clim'] = head[0x13]
        t['fert'] = head[0x19]
        t['F'] = struct.unpack_from('<H', head, 0x1c)[0]
        t['crop'] = struct.unpack_from('<H', head, 0x28)[0]
        t['dry'] = struct.unpack_from('<H', head, 0x26)[0]
        t['hp'] = struct.unpack_from('<H', head, 0x2c)[0]
        t['burn'] = head[0x31]
        t['up_off'], t['up_on'] = struct.unpack_from('<HH', prod3, 0)
        t['out'] = [s for s in strs(r.get('WARE', b'')) if s != 'UNUSED']
        t['inp'] = [s for s in strs(r.get('ROHSTOFF', b'')) if s != 'UNUSED']
        t['forsch'] = strs(r.get('FORSCHTYPE', b''))
        t['entrance'] = r.get('ENTRANCE', b'')
        t['walkmask'] = r.get('WALKMASK', b'')
        t['highmap'] = len(r.get('HIGHMAP', b'')) if 'HIGHMAP' in r else None
        t['conpo2'], t['conpos'], t['subs'] = None, None, []
        if 'OBJECTPOS' in r:
            for nm, d in objectpos(r['OBJECTPOS']):
                t['subs'].append(nm)
                if nm == 'CONPO2':
                    t['conpo2'] = d
                elif nm == 'CONPOS':
                    t['conpos'] = d
        # sprite sets and rotation blocks: set 0, zoom group 0, frames / ANIM n (ui_layout rule)
        t['title'] = ''
        if e['gfx']:
            tr = next((rp for rn, rp in e['gfx'][0]['records'] if rn == 'TITLE'), b'')
            t['title'] = fig.cstr(tr).decode('cp1252', 'replace').strip()
        t['sets'] = len(e['gfx'])
        t['frames'], t['anim'], t['blocks'] = 0, 0, 0
        if e['gfx']:
            g0 = e['gfx'][0]
            anim = next((rp for rn, rp in g0['records'] if rn == 'ANIM'), None)
            nn = max(struct.unpack_from('<H', anim, 2)[0], 1) if anim and len(anim) >= 4 else 1
            fs = next((s for s in g0['sets'] if s['group'] == 0), None)
            nf = len(fs['frames']) if fs else 0
            t['frames'], t['anim'] = nf, nn
            t['blocks'] = max(nf // nn, 1) if nf else 0
        out[t['id']] = t
    del buf
    gc.collect()
    return out


def arrows(t):
    d = t['conpo2']
    if d is None:
        return None
    return [(d[k], d[k + 1], d[k + 2]) for k in range(0, len(d) - 2, 3)]


def side_counts(t):
    a = arrows(t)
    if a is None:
        return ''
    c = [0, 0, 0, 0]
    bad = 0
    for _, _, s in a:
        if s <= 3:
            c[s] += 1
        else:
            bad += 1
    txt = '/'.join(str(x) for x in c)
    return txt + (f' +{bad}bad' if bad else '') + (f' tail{len(t["conpo2"]) % 3}' if len(t['conpo2']) % 3 else '')


def arrow_raw(t):
    a = arrows(t)
    if a is None:
        return ''
    return ' '.join(f'{x},{y},{s}' for x, y, s in a)


def fmt_conpos(t):
    return t['conpos'].hex(' ') if t['conpos'] is not None else ''


COLS = ['id', 'name', 'PRODTYPE', 'WxH', 'F', 'F&3', 'F&0x7000', 'r [0x11]', 'crop [0x28]', 'in', 'out',
        'gold', 'tools', 'wood', 'bricks', 'marble', 'upkeep on/off', 'sets', 'rot', 'arrows -x/+y/+x/-y',
        'CONPO2 (x3,y3,side)']
EVID = ['P', 'P (Texte.dat)', 'P+D', 'P+D', 'P+D', 'D', 'D', 'P+D (radius meaning L outside market/farm)',
        'P+D', 'P+D', 'P+D', 'P+D, meaning L', 'P+D, L', 'P+D, L', 'P+D, L', 'P+D, L',
        'P+D (help-confirmed)', 'P', 'L (frames/ANIM)', 'P+D', 'P+D']


def row(t, nm):
    return [str(t['id']), label(t, nm), t['ptype'], f"{t['w']}x{t['h']}",
            f"0x{t['F']:04x}", str(t['F'] & 3), f"0x{t['F'] & 0x7000:04x}", str(t['radius']),
            str(t['crop']) if t['crop'] else '', ','.join(t['inp']), ','.join(t['out']),
            str(t['gold']), str(t['tools']), str(t['wood']), str(t['bricks']), str(t['marble']),
            f"{t['up_on']}/{t['up_off']}" if (t['up_on'] or t['up_off']) else '',
            str(t['sets']), f"{t['blocks']}" + (f" (n{t['anim']})" if t['anim'] > 1 else ''),
            side_counts(t), arrow_raw(t)]


def label(t, nm):
    n = nm.get(t['id']) or (('~' + t['title']) if t.get('title') else '?')
    return n.replace('|', '/')


def md_table(rows):
    out = ['| ' + ' | '.join(COLS) + ' |', '|' + '---|' * len(COLS)]
    for r in rows:
        out.append('| ' + ' | '.join(r) + ' |')
    return out


DIFF_KEYS = [('ptype', 'PRODTYPE'), (('w', 'h'), 'footprint'), ('F', 'flag word'), ('radius', 'radius'),
             ('crop', 'crop'), ('conpo2', 'CONPO2'), ('conpos', 'CONPOS'),
             (('gold', 'tools', 'wood', 'bricks', 'marble'), 'costs'), (('up_on', 'up_off'), 'upkeep'),
             (('inp', 'out'), 'goods'), ('head', 'HEAD bytes (any)')]


def val(t, k):
    if isinstance(k, tuple):
        return tuple(t[x] for x in k)
    return t[k]


def show(t, k):
    if k == 'conpo2':
        return arrow_raw(t) or '-'
    v = val(t, k)
    if k == 'F':
        return f'0x{v:04x}'
    if k == 'head':
        return '-'
    if k == 'conpos':
        return v.hex(' ') if v else '-'
    return str(v)


def diff(gog, other, nm, fname):
    lines = []
    only_g = sorted(set(gog) - set(other))
    only_o = sorted(set(other) - set(gog))
    lines.append(f'- Types only in GOG ({len(only_g)}): ' +
                 (', '.join(f"{i} {label(gog[i], nm)}" for i in only_g) or 'none'))
    lines.append(f'- Types only in HE {fname} ({len(only_o)}): ' +
                 (', '.join(f"{i} {other[i]['ptype']} {other[i]['w']}x{other[i]['h']}" for i in only_o) or 'none'))
    rows = []
    head_only = []
    for i in sorted(set(gog) & set(other)):
        a, b = gog[i], other[i]
        fields = []
        for k, lab in DIFF_KEYS[:-1]:
            if val(a, k) != val(b, k):
                fields.append(f'{lab}: {show(a, k)} -> {show(b, k)}')
        if fields:
            rows.append(f'| {i} | {label(a, nm)} | {a["ptype"]} | ' + '; '.join(fields) + ' |')
        elif a['head'] != b['head']:
            idx = [f'0x{k:02x}' for k in range(56) if a['head'][k] != b['head'][k]]
            head_only.append(f"{i} ({','.join(idx)})")
    out = lines + ['', f'Types in both whose footprint, flags, arrows, costs, radius, crop, upkeep, goods or '
                       f'PRODTYPE differ ({len(rows)}):', '',
                   '| id | name (GOG) | PRODTYPE (GOG) | GOG -> HE |', '|---|---|---|---|'] + rows
    out += ['', f'Other HEAD differences only (bytes listed; none of the columns above) ({len(head_only)}): ' +
            (', '.join(head_only) or 'none')]
    return out, len(rows), only_g, only_o


def main():
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--game-dir', default=os.environ.get('ANNO1503_GAME_DIR'))
    ap.add_argument('--he-dir', default=None)
    ap.add_argument('--out', default=os.path.join('cache', 'buildings', 'inventory.md'))
    a = ap.parse_args()
    if not a.game_dir or not os.path.isfile(os.path.join(a.game_dir, 'Anlagen.dat')):
        sys.exit('buildings_inventory: set --game-dir or ANNO1503_GAME_DIR to a folder with Anlagen.dat')
    gog_dir, he_dir, out_path = a.game_dir, a.he_dir, a.out
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    nm = names(os.path.join(gog_dir, 'Texte.dat'))
    gog = load(os.path.join(gog_dir, 'Anlagen.dat'))
    md = []
    by_group = collections.OrderedDict((g, []) for g, _ in GROUPS)
    by_group['others'] = []
    for t in gog.values():
        by_group[group_of(t['ptype'])].append(t)
    counts = {g: len(v) for g, v in by_group.items()}
    md.append('## 2. Per-type tables (GOG `Anlagen.dat`, 921 types)')
    md.append('')
    md.append('Column evidence: ' + '; '.join(f'`{c}` {e}' for c, e in zip(COLS, EVID)) + '.')
    md.append('')
    for g, ts in by_group.items():
        pts = collections.Counter(t['ptype'] for t in ts)
        md.append(f'### 2.{list(by_group).index(g) + 1} {g} ({len(ts)} types)')
        md.append('')
        md.append('PRODTYPEs: ' + ', '.join(f'{k} {v}' for k, v in sorted(pts.items())))
        md.append('')
        md += md_table([row(t, nm) for t in sorted(ts, key=lambda t: t['id'])])
        md.append('')
    # anomalies
    anom = ['## 2.10 Arrow anomalies and CONPOS', '']
    for t in sorted(gog.values(), key=lambda t: t['id']):
        a = arrows(t)
        if a is not None and (any(s > 3 for _, _, s in a) or len(t['conpo2']) % 3):
            anom.append(f"- {t['id']} {label(t, nm)} ({t['ptype']}): CONPO2 {len(t['conpo2'])} bytes = "
                        f"`{t['conpo2'].hex(' ')}`")
        if t['conpos'] is not None:
            anom.append(f"- {t['id']} {label(t, nm)} ({t['ptype']}): old CONPOS {len(t['conpos'])} bytes = "
                        f"`{t['conpos'].hex(' ')}`; OBJECTPOS sub-records {t['subs']}")
    n_c2 = sum(1 for t in gog.values() if t['conpo2'] is not None)
    n_arrows = sum(len(arrows(t)) for t in gog.values() if t['conpo2'] is not None)
    n_ent = sum(1 for t in gog.values() if t['entrance'])
    n_ent_no = [t['id'] for t in gog.values() if t['entrance'] and t['conpo2'] is None]
    anom.append('')
    anom.append(f'- {n_c2} types carry CONPO2 ({n_arrows} arrow records); {n_ent} types carry ENTRANCE; '
                f'{len(n_ent_no)} carry ENTRANCE but no CONPO2: '
                + ', '.join(f"{i} {gog[i]['ptype']}" for i in sorted(n_ent_no)))
    flagc = collections.Counter((t['F'] & 3, t['F'] & 0x7000) for t in gog.values())
    anom.append('- Flag classes over all types, (F & 3, F & 0x7000): ' +
                ', '.join(f'({a}, 0x{b:04x}) {n}' for (a, b), n in sorted(flagc.items())))
    fw = collections.Counter(t['F'] for t in gog.values())
    anom.append('- Distinct flag words: ' + ', '.join(f'0x{k:04x} {v}' for k, v in sorted(fw.items())))
    hl = collections.Counter(t['head_len'] for t in gog.values())
    anom.append('- HEAD lengths: ' + ', '.join(f'{k} B {v}' for k, v in sorted(hl.items())))
    md += anom + ['']
    # cross-check
    def find(sub):
        return [t for t in gog.values() if sub.lower() in nm.get(t['id'], '').lower()]
    xc = ['## 4. Arrow cross-check against the owner\'s HE recording', '',
          '| owner (HE recording) | id | name | PRODTYPE | WxH | CONPO2 arrows | per side -x/+y/+x/-y | raw (x3,y3,side) | match |',
          '|---|---|---|---|---|---|---|---|---|']
    want = [('forester 3', [801], 3), ('sheep farm 3', [807], 3), ('hunter 4', [805], 4),
            ('small farm 4', [809], 4), ('fisher 4', [810], 4),
            ('tanner 4', [t['id'] for t in find('tann')], 4),
            ('mine 1 front + 2 back', [t['id'] for t in gog.values() if t['ptype'] == 'MINE'], 3)]
    for lab, ids, n in want:
        for i in sorted(ids):
            t = gog.get(i)
            if not t:
                xc.append(f'| {lab} | {i} | missing | | | | | | |')
                continue
            a = arrows(t) or []
            ok = 'yes' if len(a) == n else 'NO'
            xc.append(f"| {lab} | {i} | {label(t, nm)} | {t['ptype']} | {t['w']}x{t['h']} | {len(a)} | "
                      f"{side_counts(t)} | {arrow_raw(t)} | {ok} |")
    # HE diffs
    he = ['## 3. GOG / History Edition differences', '']
    summary = {}
    for fn in (['Anlagen.addon', 'Anlagen.dat', 'Anlagen.classic', 'Anlagen.gfx'] if he_dir else []):
        p = os.path.join(he_dir, fn)
        he.append(f'### HE `{fn}`')
        he.append('')
        try:
            other = load(p)
        except Exception as ex:  # noqa: BLE001 - report and continue
            he.append(f'- not parsed: {type(ex).__name__}: {ex}')
            he.append('')
            summary[fn] = f'not parsed ({ex})'
            continue
        if other is None:
            he.append('- no ANLAGEN chunk')
            he.append('')
            summary[fn] = 'no ANLAGEN'
            continue
        if all(t['head_len'] == 0 for t in other.values()):
            he.append(f'- {len(other)} ANLAGEN entries, none with a HEAD record: graphics data, not type data; '
                      'not diffed')
            he.append('')
            summary[fn] = f'{len(other)} entry without HEAD (graphics only, not type data)'
            continue
        he.append(f'- {len(other)} types')
        lines, nd, og, oo = diff(gog, other, nm, fn)
        he += lines + ['']
        summary[fn] = f'{len(other)} types, {nd} differing, {len(og)} only GOG, {len(oo)} only HE'
        del other
        gc.collect()
    xc += ['', 'Notes (L unless stated):',
           '- The HE game loads `Anlagen.addon` (x64 AnnoGame/AnnoFrame name it; `docs/research/he-vs-gog-comparison.md` 2), '
           'which is byte-identical to GOG `Anlagen.dat` (section 3), so the recording checks the same CONPO2 bytes (P).',
           '- Forester, sheep farm, hunter, small farm, fisher (810) and tannery (1023) match the recorded counts exactly (P).',
           '- Mines: all eight MINE types are 3x4 with flag word 0x3096 (class 0x3000). 1203 (salt) and 1205 (ore) carry three '
           'arrows: one on the +y face at its middle (3,11) and one each on the -x and +x faces at y3 = 10, i.e. both in the '
           'last tile row next to the +y face. That is 3 arrows = the recorded 1 + 2. Reading the +y arrow as the recorded '
           '"front" and the two side arrows as the "back" pair depends on the screen orientation of side +y at the recorded '
           'turn, not decoded here (L). The other six mines (1206 gold, 1210 ore, 1212 gold, 1214 gem, 1216/1217 large ore) '
           'carry a single +y arrow (3,11,1): if the recorded mine was one of those, it is a mismatch. Which mine id the '
           'recording shows is not known to this inventory.',
           '- 3510 (`~Eskimo_Fischer01`, FISCHER 4x3) has ENTRANCE but no CONPO2, so no arrows in the preview; it is a '
           'scenario-range entry (3xxx), not the player fisher (P).']
    md += he + [''] + xc + ['']
    head = ['# BUILDINGS-REF-1 part A: per-type data inventory', '',
            'Generated by `cache/bref/inventory.py` (read-only; `python -I`). Do not hand-edit the tables.', '',
            '## 1. Method', '',
            f"- File: GOG `game/app/Anlagen.dat` (205,470,957 bytes), `ANNO/ANLAGEN/ENTRY` ({len(gog)} types). "
            'Names: GOG `Texte.dat` section `[ANLAGEN]` (English, first line per id).',
            '- Decoder: the repo oracle `tools/figuren.py` (`records()`, `entry()`), no second chunk reader. '
            'OBJECTPOS sub-records: 6-byte name, u16 size, data (task/UI-INFO-2b `tools/ui_layout.py` '
            '`anlagen_entrance_arrows`; AnnoGame loader 0x10009e3e..0x10009edf).',
            '- HEAD is zero-filled to 56 bytes as the original loader does (AnnoGame 0x10009351). Offsets: '
            '`[0]`/`[1]` footprint W/H, `[3]` bricks, `[4]` wood, `[5]` tools, `[6]` marble, u16 `[8]` gold, '
            '`[0x11]` action radius, u16 `[0x1c]` flag word F, u16 `[0x28]` crop object '
            '(`docs/re/production.md` 2.4; task/UI-PRODBAR-1 production.md for `[0x28]`). PROD3 u16 `[0]` '
            'upkeep while shut down, u16 `[2]` while active (production.md 2.3). WARE = outputs, ROHSTOFF = '
            'inputs, `UNUSED` dropped (production.md 2.2).',
            '- F classes: `F & 3` selects the terrain mode (1 = single-quad scenery), `F & 0x7000` the class '
            'test: 0 ordinary land (all window vertices equal, >= 32), 0x1000 mountain/relaxed (`mx - lo <= 0x10`), '
            '0x2000 coast helper, 0x3000 HIGHMAP helper (mines/quarries, unread: OQ-052), 0x4000 road/bridge '
            '(exempt) (task/FERTILITY-1a `docs/re/ship-placement.md` 3, 3a; AnnoGame 0x10008f90).',
            '- Arrows: CONPO2, 3 bytes per arrow (x3, y3, side); x3/y3 in thirds of a tile from the footprint '
            'corner at rotation 0; side 0..3 = -x, +y, +x, -y (offsets (-0.5,0), (0,0.5), (0.5,0), (0,-0.5), '
            'View.dll 0x10021828; ui.md 2.5 on task/UI-INFO-2b). Which screen side is "front" is not decoded '
            'here (L, see section 4).',
            '- `rot` = rotation blocks of sprite set 0, zoom group 0 = frames / ANIM n (`ui_layout.anlagen_preview_frame`, '
            'render::frame_index); a 1 means the type has one picture for all four turns (L). `sets` = HAEUSER sprite sets.',
            '- Groups: by PRODTYPE (my mapping, L). Houses PIONIER..ARISTOKRATEN; production workshops, mines, '
            'quarries, stonemason, cannon foundry; farms and fields PLANTAGE, WEIDETIER, SCHAFFARM, UNIVERSAL, '
            'JAGDHAUS, PELZJAEGER, FISCHER, WALFAENGER; public incl. market, stalls, decoration, castle/palace '
            '(SCHLOSS); military incl. walls, gates, fences; harbour KONTOR, WERFT, PIER; roads incl. bridges, '
            'ramps, squares (PLATZ); nature ROHSTOFF*, rocks, cliffs, ruins, rivers, sea; others = UNUSED and the rest.',
            '- Evidence letters: P parsed from the file, D code address cited in the linked doc, L likely. '
            'Unknown HEAD fields (`[0x02]`, `[0x0b]`, `[0x0c]`, `[0x12]`, `[0x20]`, `[0x24/0x25/0x2e]`, `[0x32]`) '
            'stay out of the tables: O-21 / OQ-054.',
            '', 'Types per group: ' + ', '.join(f'{g} {n}' for g, n in counts.items()) + '.', '',
            'HE summary: ' + '; '.join(f'`{k}` {v}' for k, v in summary.items()) + '.', '']
    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(head + md) + '\n')
    print('groups', counts)
    print('he', summary)


if __name__ == '__main__':
    main()
