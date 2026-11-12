#!/usr/bin/env python3
"""Bounded public-command checks for wildlife_ref.py; no game files or imports of it."""
import argparse
from collections import Counter
from pathlib import Path
import subprocess
import sys
import tempfile


TYPES = {'density', 'habitat', 'replenish', 'create', 'load', 'animal', 'walk',
         'route_old', 'wolf', 'prey'}


def invoke(ref, path, timeout=5):
    try:
        result = subprocess.run([sys.executable, '-I', str(ref), str(path)],
                                capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        raise AssertionError('reference timed out') from error
    return result.returncode, result.stdout + result.stderr


def require(ref, path, code, diagnostic=''):
    actual, output = invoke(ref, path)
    if actual != code or diagnostic not in output or 'Traceback' in output:
        raise AssertionError(f'{path.name}: expected exit {code} / {diagnostic!r}; '
                             f'got {actual}: {output}')


def contract(ref, vectors, root):
    text = vectors.read_text(encoding='utf-8')
    rows = [line.split('#', 1)[0].strip() for line in text.splitlines()]
    rows = [line for line in rows if line]
    counts = Counter(line.split()[0] for line in rows)
    if set(counts) != TYPES or any(count < 2 for count in counts.values()):
        raise AssertionError('bundled vectors need at least two per required type')
    require(ref, vectors, 0, f'{len(rows)} vectors, 0 disagreements')
    checks = 1

    def case(name, body, code, diagnostic=''):
        nonlocal checks
        path = root / (name + '.txt')
        path.write_text(body, encoding='utf-8')
        require(ref, path, code, diagnostic)
        checks += 1

    good = 'density boundary n=13 draws=- => desired=4 consumed=0\n'
    case('good', good, 0, '1 vectors, 0 disagreements')
    # Prove the mutation actually changed one expectation, before executing it.
    old, new = '=> desired=4 consumed=0', '=> desired=5 consumed=0'
    if text.count(old) != 1:
        raise AssertionError('expected-value mutation did not match exactly one bundled vector')
    mutated = text.replace(old, new, 1)
    if mutated == text or mutated.count(new) != text.count(new) + 1:
        raise AssertionError('expected-value mutation was not applied')
    case('changed_expected', mutated, 1, 'desired: expected 5, got 4')
    case('empty', '', 2, 'zero vectors')
    case('comments_only', '# no selected vectors\n\n', 2, 'zero vectors')
    case('malformed', 'this is not a vector\n', 2, '=>')
    case('missing_input', good.replace('n=13 ', ''), 2, 'missing=n')
    case('unknown_input', good.replace('n=13', 'n=13 extra=1'), 2, 'unknown=extra')
    case('duplicate', good.replace('n=13', 'n=13 n=14'), 2, 'duplicate')
    case('unknown_output', good.replace('desired=4', 'species=4'), 2, 'output keys')
    case('missing_consumed', good.replace(' consumed=0', ''), 2, 'expected consumed')
    case('malformed_expected', good.replace('desired=4', 'desired=four'), 2, 'unsigned integer')
    case('unknown_type', good.replace('density ', 'unread '), 2, 'unknown record type')
    case('float_input', good.replace('n=13', 'n=13.0'), 2, 'unsigned integer')
    case('negative_draw', good.replace('draws=-', 'draws=-1'), 2, 'unsigned integer')
    case('overflow_draw', good.replace('draws=-', 'draws=4294967296'), 2, 'out of range')
    case('bad_list', good.replace('draws=-', 'draws=1,,2'), 2, 'unsigned integer')
    case('out_of_scope_density', good.replace('n=13', 'n=17'), 2, '16-cell')
    case('missing_label', good.replace(' boundary', ''), 2, 'TYPE LABEL')
    case('two_arrows', good.rstrip() + ' =>\n', 2, '=>')
    case('mixed_bad', good.replace('desired=4', 'desired=5') + 'bad\n', 2, '=>')
    case('draw_underflow', 'route_old empty quantity=0 draws=- => next=7 consumed=1\n',
         2, 'draw stream exhausted')
    case('candidate_exhaustion',
         'replenish exhausted candidates=1 deficit=1 occupied=1 draws=0 => chosen=- consumed=1\n',
         2, 'OQ-572')
    create = next(line for line in rows if line.startswith('create cave_wrap '))
    case('cave_no_entrance', create.replace('entrances=2', 'entrances=0') + '\n', 2, 'OQ-574')
    case('bad_table', create.replace('types=100,101,', 'types=') + '\n', 2, '32 u16')
    case('zero_hp', 'prey zero_hp predator=116 prey=130 kind=49 draws=- => admitted=1 consumed=0\n',
         2, 'OQ-585')
    case('idle_zero_variants',
         'animal bad event=idle state=28 success=1 quantity=0 capacity=9 variants=0 draws=0 '
         '=> next=28 consumed=1\n', 2, 'variants=1..3')
    require(ref, root / 'missing.txt', 2)
    checks += 1
    invalid_encoding = root / 'invalid_encoding.txt'
    invalid_encoding.write_bytes(b'\xff')
    require(ref, invalid_encoding, 2)
    checks += 1
    # Negative controls for this checker: a success-only child and a hanging child
    # cannot masquerade as a correct reference. No recursive self-testing.
    echo = root / 'success_only.py'
    echo.write_text("print('1 vectors, 0 disagreements')\n", encoding='utf-8')
    try:
        require(echo, root / 'empty.txt', 2, 'zero vectors')
    except AssertionError:
        checks += 1
    else:
        raise AssertionError('contract accepted success-only stand-in')
    hang = root / 'hang.py'
    hang.write_text('import time\ntime.sleep(60)\n', encoding='utf-8')
    try:
        invoke(hang, vectors, timeout=0.25)
    except AssertionError as error:
        if 'timed out' not in str(error):
            raise
        checks += 1
    else:
        raise AssertionError('contract accepted hanging stand-in')
    print(f'wildlife reference contract: {checks} checks passed; '
          f'{len(rows)} bundled vectors; ' + ', '.join(f'{k}={counts[k]}' for k in sorted(counts)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ref', required=True, type=Path)
    parser.add_argument('--vectors', required=True, type=Path)
    args = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix='wildlife_ref_contract_') as tmp:
            contract(args.ref.resolve(), args.vectors.resolve(), Path(tmp))
    except (AssertionError, OSError, UnicodeError) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
