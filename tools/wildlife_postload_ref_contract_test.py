#!/usr/bin/env python3
"""Public CLI contract for the isolated graphics leaf; authored synthetic data only."""
import argparse
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
    if len(rows) < 12:
        raise AssertionError('fewer than 12 hand vectors')
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
