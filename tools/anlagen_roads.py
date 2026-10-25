"""tools/anlagen_roads.py

Prints, for every entry of Anlagen.dat in the game directory, one line with:
  id, ANLAGEN name (from `anno-dump texte` section ANLAGEN), PRODTYPE string,
  HEAD flags_word in hex (u16 at HEAD offset 0x1c), WALKMASK byte count.

Nothing from the game is written: the script only reads Anlagen.dat and Texte.dat
(via anno-dump) and prints to stdout. No file is created or modified.

Usage:
  python tools/anlagen_roads.py [--game-dir DIR] [--anno-dump EXE]
  python tools/anlagen_roads.py --game-dir DIR --walk-predictions 2527,2520 --rotation 0

Walk predictions describe the object placement before terrain additions. Evidence:
AnnoGame 0x100091f0 / 0x10007dc0; AnnoWelt 0x100825b7 / 0x100825d9.
The existing layer_blocks.py reader supplies layer bytes; no second block reader.

The game directory comes from --game-dir or the ANNO1503_GAME_DIR environment
variable. The anno-dump executable defaults to
build/msvc/src/tools/anno_dump/Release/anno-dump.exe relative to the repository
and can be overridden with --anno-dump.
"""

import argparse
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'tools'))
import figuren  # the Python oracle; parses Anlagen.dat via records()/entry()


def anlagen_names(game_dir, anno_dump):
    """Return {id: name} from `anno-dump texte <game dir>/Texte.dat`, section ANLAGEN."""
    r = subprocess.run(
        [str(anno_dump), 'texte', str(game_dir / 'Texte.dat')],
        capture_output=True, text=True, encoding='utf-8', errors='replace',
    )
    if r.returncode != 0:
        raise SystemExit(f'anno-dump texte failed: {r.stderr}')
    names = {}
    in_section = False
    for line in r.stdout.splitlines():
        if 'SECTION' in line and 'name="ANLAGEN"' in line:
            in_section = True
            continue
        if in_section and line.startswith('SECTION '):
            break
        if in_section:
            m = re.match(r'\s*ID id=(\d+) text="(.*)"', line)
            if m:
                names[int(m.group(1))] = m.group(2)
    return names


def parse_anlagen(game_dir):
    buf = (game_dir / 'Anlagen.dat').read_bytes()
    top = figuren.records(buf, 16, 16 + struct.unpack_from('<I', buf, 12)[0])
    anl = next((p for n, p in top if n == 'ANLAGEN'), None)
    entries = []
    for n, p in figuren.records(anl, 0, len(anl)):
        if n == 'ENTRY':
            entries.append(figuren.entry(p))
    return entries


def record_of(entry, name):
    for rn, rp in entry['records']:
        if rn == name:
            return rp
    return b''


def walk_code(head, rotation):
    """AnnoGame 0x1000940b..0x1000942b, 0x100091f0, 0x10007dc0..0x10007ea2."""
    head = head.ljust(56, b'\0')  # original HEAD reader zero-fills 56 bytes
    first, shape = head[0x2f], head[0x30]
    code = 4  # constructor 0x1000b555, retained only when both HEAD bytes are zero
    if first or shape:
        code = first
        if first >= 8:
            code = ((first - 8) | 0x80) & 0xff
            if shape <= 4:
                code |= (0x50, 0x48, 0x58, 0x78, 0x10)[shape]
    # Getter rotates only these four canonical connection patterns; others fall through.
    group = (code & 0x78) >> 3
    if code & 0x80 and group in (2, 9, 10, 11):
        for _ in range(rotation):
            group = ((group << 1) | (group >> 3)) & 0xf
        code = 0x80 | (group << 3) | (code & 7)
    return code


def footprint_mask(walkmask, width, height, rotation, dx, dy):
    """Object-only mask, AnnoWelt 0x1007b5e0..0x1007b837 (no terrain additions).

    WALKMASK has three u32 subcell rows per original footprint tile row, not
    one u32 per tile. The original constructor zero-fills its mask storage.
    """
    rows = list(struct.unpack('<' + 'I' * (len(walkmask) // 4), walkmask))
    def bit(x, y):
        return (rows[y] >> x) & 1 if 0 <= y < len(rows) and x >= 0 else 0
    result = 0
    for v in range(3):
        for u in range(3):
            x, y = dx * 3 + u, dy * 3 + v
            if rotation == 1:
                x, y = width * 3 - 1 - y, x
            elif rotation == 2:
                x, y = width * 3 - 1 - x, height * 3 - 1 - y
            elif rotation == 3:
                x, y = y, height * 3 - 1 - x
            result |= bit(x, y) << (v * 3 + u)
    return result


def walk_predictions(game_dir, ids, rotation):
    """Derived file predictions; never writes extracted records."""
    import pefile
    from orig.layer_blocks import disk_block
    dll = game_dir / 'AnnoWelt.dll'
    pe = pefile.PE(str(dll), fast_load=True)
    mapper = struct.unpack('<512H', pe.get_data(0x100c1af8 - pe.OPTIONAL_HEADER.ImageBase, 1024))
    layers = {layer: disk_block(str(dll), layer) for layer in (0, 3)}
    selected = [e for e in parse_anlagen(game_dir) if e['id'] in ids]
    print(f'walk prediction entries={len(selected)} rotation={rotation}')
    print('object placement only; terrain additions and later overlays are excluded')
    for entry in selected:
        head = record_of(entry, 'HEAD').ljust(56, b'\0')
        width, height = head[:2]
        wm = record_of(entry, 'WALKMASK')
        code = walk_code(head, rotation)
        print(f'type={entry["id"]} size={width}x{height} code={code} '
              f'codes_by_rotation={tuple(walk_code(head, r) for r in range(4))} '
              f'flags=0x{struct.unpack_from("<H", head, 0x1c)[0]:04x}')
        rw, rh = (height, width) if rotation & 1 else (width, height)
        for dy in range(rh):
            for dx in range(rw):
                mask = footprint_mask(wm, width, height, rotation, dx, dy)
                detail = []
                for layer, block in layers.items():
                    costs = tuple(block[0x400 + code * 8 + d] for d in (6, 2, 0, 4))
                    detail.append(f'L{layer}={block[code]} costs_UDLR={costs}')
                print(f'  tile={dx},{dy} mask=0x{mask:03x} low=0x{mapper[mask]:03x} '
                      + ' '.join(detail))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game-dir', help='game directory (default: ANNO1503_GAME_DIR)')
    ap.add_argument('--anno-dump', default=None,
                    help='path to anno-dump.exe (default: build/msvc/src/tools/'
                         'anno_dump/Release/anno-dump.exe relative to the repo)')
    ap.add_argument('--walk-predictions', metavar='IDS',
                    help='comma-separated type ids: object-only walk codes/masks and layer entries')
    ap.add_argument('--rotation', type=int, choices=range(4), default=0,
                    help='original placed-item rotation (0..3) for --walk-predictions')
    args = ap.parse_args()

    game_dir = Path(args.game_dir or os.environ.get('ANNO1503_GAME_DIR', ''))
    if not game_dir or not game_dir.is_dir():
        raise SystemExit(
            'no game directory: pass --game-dir or set ANNO1503_GAME_DIR')

    if args.walk_predictions:
        walk_predictions(game_dir, {int(v) for v in args.walk_predictions.split(',')}, args.rotation)
        return

    repo_root = Path(__file__).resolve().parent.parent
    anno_dump = Path(args.anno_dump or repo_root / 'build' / 'msvc' / 'src'
                     / 'tools' / 'anno_dump' / 'Release' / 'anno-dump.exe')

    names = anlagen_names(game_dir, anno_dump)
    entries = parse_anlagen(game_dir)

    rows = []
    for e in entries:
        eid = e['id']
        head = record_of(e, 'HEAD')
        wm = record_of(e, 'WALKMASK')
        pt = record_of(e, 'PRODTYPE')
        flags = struct.unpack_from('<H', head, 0x1c)[0] if len(head) >= 0x1e else 0
        prodtype = pt.split(b'\x00', 1)[0].decode('ascii', 'replace') if pt else '?'
        name = names.get(eid, '')
        rows.append((eid, name, prodtype, flags, wm))

    road_keywords = re.compile(r'\b(road|street|bridge|pier)\b', re.IGNORECASE)
    named = [r for r in rows if r[1] and road_keywords.search(r[1])]
    named_kroad = [r for r in named if r[3] & 0x4000]
    named_0x1000 = [r for r in named if r[3] & 0x1000]

    print(f'Entries parsed: {len(rows)}')
    print(f'Entries with HEAD flags_word 0x4000 (kRoadFlag): '
          f'{sum(1 for r in rows if r[3] & 0x4000)}')
    print(f'Named roads (ANLAGEN name matches road/street/bridge/pier): {len(named)}')
    print(f'  of which flags 0x4000: {len(named_kroad)}, flags 0x1000: {len(named_0x1000)}')
    print()
    print(f'{"id":>5} {"name":32s} {"PRODTYPE":10s} {"flags":>7} WALKMASK_bytes')
    for eid, nm, pt, fl, wm in sorted(rows):
        print(f'{eid:5d} {nm[:32]:32s} {pt:10s} 0x{fl:04x}   {len(wm)}')


if __name__ == '__main__':
    main()
