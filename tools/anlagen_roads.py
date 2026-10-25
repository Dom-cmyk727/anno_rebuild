"""tools/anlagen_roads.py

Prints, for every entry of Anlagen.dat in the game directory, one line with:
  id, ANLAGEN name (from `anno-dump texte` section ANLAGEN), PRODTYPE string,
  HEAD flags_word in hex (u16 at HEAD offset 0x1c), WALKMASK byte count.

Nothing from the game is written: the script only reads Anlagen.dat and Texte.dat
(via anno-dump) and prints to stdout. No file is created or modified.

Usage:
  python tools/anlagen_roads.py [--game-dir DIR] [--anno-dump EXE]

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


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game-dir', help='game directory (default: ANNO1503_GAME_DIR)')
    ap.add_argument('--anno-dump', default=None,
                    help='path to anno-dump.exe (default: build/msvc/src/tools/'
                         'anno_dump/Release/anno-dump.exe relative to the repo)')
    args = ap.parse_args()

    game_dir = Path(args.game_dir or os.environ.get('ANNO1503_GAME_DIR', ''))
    if not game_dir or not game_dir.is_dir():
        raise SystemExit(
            'no game directory: pass --game-dir or set ANNO1503_GAME_DIR')

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
