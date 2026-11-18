#!/usr/bin/env python3
"""Public-command contract for the raw HIRSCH reference, using authored inputs only."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def invoke(ref, args, expected, diagnostic='', timeout=10):
    result = subprocess.run([sys.executable, '-I', str(ref), *map(str, args)],
                            capture_output=True, text=True, timeout=timeout)
    output = result.stdout + result.stderr
    if result.returncode != expected or diagnostic not in output or 'Traceback' in output:
        raise AssertionError(f'expected {expected}/{diagnostic!r}, got '
                             f'{result.returncode}: {output}')
    return result.stdout


def chunk(name, payload):
    return name.encode('ascii').ljust(12, b'\0') + len(payload).to_bytes(4, 'little') + payload


def contract(ref, root):
    invoke(ref, ['--self-test'], 0, '9 vectors, 0 disagreements')
    checks = 1
    good = ('decode distinct payload=3412cdab82f37856 => count=1 '
            'values=4660,43981,130,243,22136 encoded=3412cdab82f37856\n')

    def case(name, text, code, diagnostic=''):
        nonlocal checks
        path = root / (name + '.txt')
        path.write_text(text, encoding='utf-8')
        invoke(ref, [path], code, diagnostic)
        checks += 1

    case('legal', good, 0, '1 vectors, 0 disagreements')
    old, new = 'count=1', 'count=2'
    if good.count(old) != 1:
        raise AssertionError('expected-value control did not match once')
    changed = good.replace(old, new)
    if changed == good or changed.count(new) != 1:
        raise AssertionError('expected-value mutation was not applied')
    case('wrong_expected', changed, 1, 'count: expected 2, got 1')
    case('empty', '', 2, 'zero vectors')
    case('comments', '# no vectors\n', 2, 'zero vectors')
    case('unknown_type', good.replace('decode ', 'unread ', 1), 2, 'unknown record type')
    case('unknown_input', good.replace('payload=', 'unknown=', 1), 2, 'input keys')
    case('unknown_output', good.replace('count=', 'unknown=', 1), 2, 'output keys')
    case('duplicate_input', good.replace('payload=', 'payload=- payload=', 1), 2, 'duplicate')
    case('partial', good.replace('3412cdab82f37856', '3412cdab82f378'), 2, 'multiple of 8')
    case('bad_hex', good.replace('payload=3412', 'payload=xx12'), 2, 'hex bytes')
    case('odd_hex', good.replace('payload=3412', 'payload=412'), 2, 'hex bytes')
    case('bad_expected', good.replace('count=1', 'count=one'), 2, 'unsigned integer')
    case('overflow_field', good.replace('values=4660', 'values=65536'), 2, 'out of range')
    case('missing_arrow', good.replace('=>', ''), 2, 'one =>')
    case('two_arrows', good.rstrip() + ' =>\n', 2, 'one =>')
    case('absent', 'lookup absent payloads=absent => present=0 count=0 values=- encoded=-\n', 0)
    case('empty_first', 'lookup empty payloads=-,ffffffffffffffff => '
         'present=1 count=0 values=- encoded=-\n', 0)
    case('bad_first', 'lookup partial payloads=00,0000000000000000 => '
         'present=1 count=0 values=- encoded=-\n', 2, 'multiple of 8')
    invoke(ref, [root / 'missing.txt'], 2)
    checks += 1
    bad_encoding = root / 'encoding.txt'
    bad_encoding.write_bytes(b'\xff')
    invoke(ref, [bad_encoding], 2)
    checks += 1
    invoke(ref, ['--corpus', root / 'missing_game'], 2, 'zero corpus files')
    checks += 1
    clean_env = dict(os.environ)
    clean_env.pop('ANNO1503_GAME_DIR', None)
    skipped = subprocess.run([sys.executable, '-I', str(ref), '--check-corpus'],
                             capture_output=True, text=True, env=clean_env, timeout=10)
    if skipped.returncode != 0 or 'skipping wildlife load corpus: ANNO1503_GAME_DIR not set' not in skipped.stdout:
        raise AssertionError('unset corpus prerequisite was not an explicit skip')
    checks += 1

    # Authored one-island corpus. Distinct payload is a raw entry, not a species.
    game = root / 'game'
    scenes = game / 'Scenes'
    scenes.mkdir(parents=True)
    fixture = scenes / 'synthetic.scp'
    head = bytes.fromhex('000000000100010000000000')
    payload = bytes.fromhex('3412cdab82f37856')
    fixture.write_bytes(chunk('ANNO', chunk('INSEL5', head + chunk('HIRSCH', payload))))
    manifest = root / 'manifest.json'
    out = json.loads(invoke(ref, ['--corpus', game, '--manifest', manifest], 0))
    if out['totals'] != dict(files=1, islands=1, absent=0, empty=0, headers=1, records=1,
                             selected_records=1, malformed=0, duplicate_islands=0, jitter_entries=1):
        raise AssertionError('synthetic corpus counts differ')
    if json.loads(manifest.read_text(encoding='utf-8'))['summary'] != out:
        raise AssertionError('manifest differs from stdout')
    checks += 1
    invoke(ref, ['--corpus', game, '--manifest', game / 'refused.json'], 2, 'read-only')
    if (game / 'refused.json').exists():
        raise AssertionError('reference wrote inside original input directory')
    checks += 1
    fixture.write_bytes(chunk('ANNO', chunk('INSEL5', head + chunk('HIRSCH', payload[:-1]))))
    out = json.loads(invoke(ref, ['--corpus', game], 1))
    if out['totals']['malformed'] != 1:
        raise AssertionError('malformed corpus was not counted')
    checks += 1
    fixture.write_bytes(b'not an island')
    invoke(ref, ['--corpus', game], 2, 'island parse')
    checks += 1
    # A success-only child cannot satisfy the error-path contract.
    stand_in = root / 'stand_in.py'
    stand_in.write_text("print('zero vectors')\n", encoding='utf-8')
    try:
        invoke(stand_in, [root / 'empty.txt'], 2, 'zero vectors')
    except AssertionError:
        checks += 1
    else:
        raise AssertionError('contract accepted success-only stand-in')
    print(f'wildlife load reference contract: {checks} checks passed; 9 bundled vectors')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ref', required=True, type=Path)
    args = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix='wildlife_load_contract_') as temporary:
            contract(args.ref.resolve(), Path(temporary))
    except (AssertionError, OSError, UnicodeError, ValueError, subprocess.TimeoutExpired) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
