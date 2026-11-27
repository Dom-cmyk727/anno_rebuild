#!/usr/bin/env python3
"""Read-only GOG ore-marker census; spec: docs/re/annoscene.md section 5.

Uses ANNO1503_GAME_DIR, or --game-dir. Without either, explicitly skips.
Exit 0 agreement/explicit skip, 1 disagreement, 2 malformed or missing files.
Only names, ids, counts and optional JSON summaries are emitted, never raw data.
"""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import figuren  # noqa: E402
import insel  # noqa: E402

# P FigurenHeader.dat; D AnnoScene 0x1003af24, kinds 0..6.
ORE_TYPES = (1661, 1561, 1562, 1563, 0, 1564, 1560)
EXPECTED = {
    1560: ('ERZSYMBOL', 'EISENERZ'), 1561: ('ERZSYMBOL', 'GOLD'),
    1562: ('ERZSYMBOL', 'SALZSTEIN'), 1563: ('ERZSYMBOL', 'EDELSTEINE'),
    1564: ('ERZSYMBOL', 'MAMORSTEIN'), 1661: ('ERZSYMBOL', 'EISENERZ'),
    1550: ('SCHATZ', 'SCHATZ'), 1551: ('SCHATZ', 'SCHATZ'),
}


def candidate(path):
    """D AnnoScene 0x10007e16..0x10007e82: filename, not directory."""
    name = path.name.upper()
    return name.endswith('.SCP') and 'PIRAT' not in name and not name.endswith('DUMMY.SCP')


def markers(payload):
    """D AnnoWelt 0x10095f63..0x10095f9d; ENTRY2's packed type."""
    offset = 0
    result = []
    while offset < len(payload):
        if len(payload) - offset < 8:
            raise ValueError('truncated occurrence header')
        tag = payload[offset:offset + 6].rstrip(b'\0')
        size, = struct.unpack_from('<H', payload, offset + 6)
        offset += 8
        if tag != b'ENTRY2' or size < 4 or size > len(payload) - offset:
            raise ValueError('malformed occurrence entry')
        word, = struct.unpack_from('<I', payload, offset)
        result.append((word >> 14) & 0xfff)
        offset += size
    return result


def census(game):
    entries, _ = figuren.parse((game / 'FigurenHeader.dat').read_bytes())
    types = {}
    for e in entries:
        if e['id'] in types:
            raise ValueError('duplicate figure id')
        records = dict(e['records'])
        types[e['id']] = tuple(figuren.cstr(records.get(k, b'')).decode('ascii')
                               for k in ('BEWOHNTYPE', 'WARE'))
    errors = []
    for key, value in EXPECTED.items():
        if types.get(key) != value:
            errors.append(f'figure {key}: expected {value}, got {types.get(key)}')
    directory = game / 'Scenes' / 'Islands'
    files = sorted(p for p in directory.rglob('*') if p.suffix.lower() == '.scp')
    if not files:
        raise ValueError('zero island templates')
    rows = []
    excluded = []
    for path in files:
        rel = path.relative_to(directory).as_posix()
        if not candidate(path):
            excluded.append(rel)
            continue
        islands = insel.islands(path.read_bytes())
        if not islands:
            raise ValueError(f'{rel}: candidate has no island')
        # D GetAnnoInselFileData: latest INFO replaces earlier, otherwise retained.
        info = bytes(12)
        for island in islands:
            for name, data in island['records']:
                if name == 'INFO':
                    if len(data) != 12:
                        raise ValueError(f'{rel}: INFO size is not 12')
                    info = data
        counts = Counter(t for island in islands for name, data in island['records']
                         if name == 'OCCUR' for t in markers(data))
        unknown = sorted(t for t in counts if t not in types)
        if unknown:
            raise ValueError(f'{rel}: unknown figure types {unknown}')
        ore = sum(n for t, n in counts.items() if types[t][0] == 'ERZSYMBOL')
        row = dict(path=rel, info4=info[4], info5=info[5], ore_markers=ore)
        rows.append(row)
        if ore != info[5]:
            errors.append(f'{rel}: ore markers {ore}, INFO[5] {info[5]}')
    return dict(files=len(files), candidates=len(rows), excluded=excluded,
                info4=dict(sorted(Counter(r['info4'] for r in rows).items())),
                agree=sum(r['ore_markers'] == r['info5'] for r in rows),
                ore_markers=sum(r['ore_markers'] for r in rows), rows=rows,
                figures={str(k): types.get(k) for k in EXPECTED}, errors=errors)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    game = args.game_dir or (Path(os.environ['ANNO1503_GAME_DIR'])
                            if os.environ.get('ANNO1503_GAME_DIR') else None)
    if game is None:
        print('SKIP: ANNO1503_GAME_DIR unset; ore-marker original-file check NOT_RUN')
        return 0
    try:
        result = census(game)
        if args.json:
            args.json.write_text(json.dumps(result, indent=2) + '\n')
    except (OSError, ValueError, struct.error, figuren.Bad, insel.Bad) as error:
        print(f'MALFORMED: {error}', file=sys.stderr)
        return 2
    for key, value in result['figures'].items():
        print(f'FIGURE {key}: {value}')
    for kind, figure in enumerate(ORE_TYPES):
        print(f'ORE KIND {kind}: figure={figure} ware={(result["figures"].get(str(figure)) or (None, None))[1]}')
    print('ORE CENSUS: files={files} candidates={candidates} agree={agree} '
          'ore_markers={ore_markers} info4={info4}'.format(**result))
    for error in result['errors']:
        print('DISAGREE:', error)
    print(f"ORE VERDICT: {'FAIL' if result['errors'] else 'PASS'}; exceptions={len(result['errors'])}")
    return 1 if result['errors'] else 0


if __name__ == '__main__':
    sys.exit(main())
