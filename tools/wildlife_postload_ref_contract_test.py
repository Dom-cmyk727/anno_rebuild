#!/usr/bin/env python3
"""Public CLI contract for the isolated graphics leaf; authored synthetic data only."""
import argparse
from collections import Counter
from pathlib import Path
import subprocess
import sys
import tempfile


def require(ref, path, code, diagnostic):
    result = subprocess.run([sys.executable, '-I', '-B', str(ref), str(path)],
                            capture_output=True, text=True, timeout=10)
    output = result.stdout + result.stderr
    if result.returncode != code or diagnostic not in output or 'Traceback' in output:
        raise AssertionError(f'{path.name}: expected {code}/{diagnostic!r}; '
                             f'got {result.returncode}: {output}')


def contract(ref, vectors, root):
    text = vectors.read_text(encoding='utf-8')
    previous = ''
    rows = []
    for line in text.splitlines():
        if line.strip() and not line.lstrip().startswith('#'):
            if not previous.startswith('#'):
                raise AssertionError('every vector needs a hand derivation')
            rows.append(line)
        if line.strip():
            previous = line.strip()
    counts = Counter(row.split()[0] for row in rows)
    if set(counts) != {'gfx', 'fill'} or counts['gfx'] < 12 or counts['fill'] < 19:
        raise AssertionError('need 12 graphics and 19 fill hand vectors')
    require(ref, vectors, 0, f'{len(rows)} vectors, 0 disagreements')
    checks = 1
    good = 'gfx control w=1 h=1 height=0 gfx=8363 => gfx=0 cleared=1\n'

    def case(name, body, code, diagnostic):
        nonlocal checks
        path = root / (name + '.txt')
        path.write_text(body, encoding='utf-8')
        require(ref, path, code, diagnostic)
        checks += 1

    case('legal', good, 0, '1 vectors, 0 disagreements')
    old, new = '=> gfx=0', '=> gfx=1'
    if good.count(old) != 1 or old == new:
        raise AssertionError('expected-value mutation did not match once')
    changed = good.replace(old, new)
    if changed == good or changed.count(new) != 1:
        raise AssertionError('expected-value mutation not applied')
    case('wrong_expected', changed, 1, 'gfx: expected 1, got 0')
    case('empty', '', 2, 'zero vectors')
    case('comments', '# empty\n', 2, 'zero vectors')
    case('unknown', good.replace('gfx control', 'other control'), 2, 'unknown record')
    case('missing_arrow', good.replace('=>', ''), 2, 'one =>')
    case('two_arrows', good.rstrip() + ' =>\n', 2, 'one =>')
    case('duplicate', good.replace('w=1', 'w=1 w=1'), 2, 'duplicate key')
    case('unknown_key', good.replace('w=1', 'other=1'), 2, 'unexpected or missing keys')
    case('unknown_output', good.replace('cleared=1', 'other=1'), 2, 'unexpected or missing keys')
    case('signed', good.replace('height=0', 'height=-1'), 2, 'unsigned decimal')
    case('wide_height', good.replace('height=0', 'height=256'), 2, 'out of range')
    case('wide_gfx', good.replace('gfx=8363', 'gfx=65536'), 2, 'out of range')
    case('wide_dimension', good.replace('w=1', 'w=4294967296'), 2, 'out of range')
    case('zero_dimension', good.replace('w=1', 'w=0'), 2, 'positive geometry')
    case('short', good.replace('w=1', 'w=2'), 2, 'plane length')
    case('extra_padding', good.replace('height=0', 'height=0,1'), 2, 'plane length')
    case('empty_plane', good.replace('height=0', 'height=-'), 2, 'plane length')
    case('wrong_count', good.replace('cleared=1', 'cleared=0'), 1, 'cleared: expected 0, got 1')
    case('duplicate_label', good + good, 2, 'duplicate label')
    require(ref, root / 'missing', 2, 'No such file')
    checks += 1
    path = root / 'bad_encoding'
    path.write_bytes(b'\xff')
    require(ref, path, 2, 'decode')
    checks += 1
    valid_fill = next(row for row in rows if row.startswith('fill fish15 ')) + '\n'
    case('fill_legal', valid_fill, 0, '1 vectors, 0 disagreements')

    def alter(name, old, new, code, diagnostic):
        if old == new or valid_fill.count(old) != 1:
            raise AssertionError(f'{name}: replacement must match exactly once')
        changed = valid_fill.replace(old, new)
        if changed == valid_fill:
            raise AssertionError('replacement not applied')
        case(name, changed, code, diagnostic)

    alter('fill_wrong_count', 'fish=1 grass=0', 'fish=0 grass=0', 1, 'fish: expected 0, got 1')
    alter('fill_wrong_cell', '=> cells=11,', '=> cells=12,', 1, 'cells: expected')
    alter('fill_wrong_draw_count', 'consumed=0', 'consumed=1', 1, 'consumed: expected 1, got 0')
    alter('fill_incomplete', 'complete=1', 'complete=0', 2, 'completed local headers')
    alter('fill_absent_ground', 'ground_done=1', 'ground_done=0', 2, 'precomputed ground')
    alter('fill_ground_mismatch', 'raw=253,0,0,0 cells=', 'raw=252,0,0,0 cells=', 2, 'low nibble mismatch')
    alter('fill_short_height', 'rect=10,20,1,1 height=15,15,15,15 gfx=', 'rect=10,20,1,1 height=15 gfx=', 2, 'plane length')
    alter('fill_bad_grass', 'grass=2,41,2,3', 'grass=2,40,2,3', 2, 'unsupported mean profile')
    alter('fill_bad_fish', 'fish=11,41,2,3', 'fish=11,40,2,3', 2, 'unsupported mean profile')
    alter('fill_same_roles', 'fish=11,41,2,3', 'fish=2,41,2,3', 2, 'distinct')
    alter('fill_zero_role', 'grass=2,41,2,3', 'grass=0,41,2,3', 2, 'zero role id')
    alter('fill_bad_origin', 'origin=10,20', 'origin=10', 2, 'origin/rectangle shape')
    alter('fill_bad_rect', 'rect=10,20,1,1', 'rect=10,20,1', 2, 'origin/rectangle shape')
    alter('fill_negative_width', 'rect=10,20,1,1', 'rect=10,20,-1,1', 2, 'unsigned decimal')
    alter('fill_wide_origin', 'origin=10,20', 'origin=2147483648,20', 2, 'signed integer out of range')
    alter('fill_bad_cell', 'cells=0,127,7,', 'cells=0,256,7,', 2, 'integer out of range')
    alter('fill_missing_key', 'complete=1 ', '', 2, 'missing keys')
    alter('fill_unknown_output', 'consumed=0', 'other=0', 2, 'missing keys')
    print(f'{checks} CLI controls passed; {len(rows)} vectors executed')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--ref', type=Path, required=True)
    parser.add_argument('--vectors', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='wildlife-postload-contract-') as directory:
        contract(args.ref.resolve(), args.vectors.resolve(), Path(directory))


if __name__ == '__main__':
    try:
        main()
    except (AssertionError, OSError, UnicodeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
