#!/usr/bin/env python3
"""Public CLI contract and bounded negative controls; no import of the oracle."""
import argparse
from collections import Counter
from pathlib import Path
import subprocess
import sys
import tempfile


TYPES = {'replenish', 'whale_walk', 'whale_route', 'whale_hp', 'whaler_tick',
         'boat_request', 'ship_attack'}


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


def replace_once(text, old, new):
    if old == new or text.count(old) != 1:
        raise AssertionError('mutation must match exactly one distinct source span')
    changed = text.replace(old, new, 1)
    if changed == text or changed.count(new) != text.count(new) + 1:
        raise AssertionError('mutation did not apply')
    return changed


def contract(ref, vectors, root):
    text = vectors.read_text(encoding='utf-8')
    rows = [line.split('#', 1)[0].strip() for line in text.splitlines()]
    rows = [line for line in rows if line]
    counts = Counter(line.split()[0] for line in rows)
    if set(counts) != TYPES or any(n < 2 for n in counts.values()):
        raise AssertionError('bundled vectors need at least two per required type')
    labels = [tuple(line.split()[:2]) for line in rows]
    if len(set(labels)) != len(labels):
        raise AssertionError('bundled type/label pairs must be unique')
    require(ref, vectors, 0, f'{len(rows)} vectors, 0 disagreements')
    checks = 1

    def case(name, body, code, diagnostic=''):
        nonlocal checks
        path = root / (name + '.txt')
        path.write_text(body, encoding='utf-8')
        require(ref, path, code, diagnostic)
        checks += 1

    good = 'ship_attack boundary count=1 draws=4294967295 => index=0 consumed=1\n'
    case('good', good, 0, '1 vectors, 0 disagreements')
    # The changed expectation is literal, not obtained from evaluate().
    changed = replace_once(text, '=> transition=1 consumed=0', '=> transition=0 consumed=0')
    case('changed_expected', changed, 1, 'transition: expected 0, got 1')
    case('empty', '', 2, 'zero vectors')
    case('comments_only', '# no vectors\n\n', 2, 'zero vectors')
    case('malformed', 'this is not a vector\n', 2, '=>')
    case('missing_field', good.replace('count=1 ', ''), 2, 'missing=count')
    case('unknown_field', good.replace('count=1 ', 'count=1 extra=0 '), 2, 'unknown=extra')
    case('duplicate', good.replace('count=1 ', 'count=1 count=2 '), 2, 'duplicate')
    case('bad_output', good.replace('index=0', 'unknown=0'), 2, 'output keys')
    case('missing_consumed', good.replace(' consumed=1', ''), 2, 'expected consumed')
    case('bad_expected', good.replace('index=0', 'index=zero'), 2, 'integer')
    case('float_input', good.replace('count=1 ', 'count=1.0 '), 2, 'integer')
    case('float_expected', good.replace('index=0 ', 'index=0.0 '), 2, 'integer')
    case('negative_draw', good.replace('draws=4294967295', 'draws=-1'), 2, 'out of range')
    case('overflow_draw', good.replace('draws=4294967295', 'draws=4294967296'), 2, 'out of range')
    case('bad_list', good.replace('draws=4294967295', 'draws=1,,2'), 2, 'integer')
    case('draw_exhausted', good.replace('draws=4294967295', 'draws=-'), 2, 'draw stream exhausted')
    case('zero_positions', good.replace('count=1 ', 'count=0 '), 2, 'out of range')
    case('count_not_byte', good.replace('count=1 ', 'count=256 '), 2, 'out of range')
    case('unknown_type', good.replace('ship_attack ', 'unknown '), 2, 'unknown record type')
    case('missing_label', good.replace(' boundary', ''), 2, 'TYPE LABEL')
    case('two_arrows', good.rstrip() + ' =>\n', 2, '=>')
    case('mixed_malformed', good.replace('index=0', 'index=1') + 'bad\n', 2, '=>')
    for kind, oq in [('sea_legality', 'OQ-595'), ('sea_depth', 'OQ-595'),
                     ('indirect_search', 'OQ-594'), ('projectile_draws', 'OQ-594')]:
        case(kind, f'{kind} refused draws=0 => consumed=0\n', 2, oq)
    replenish = next(row for row in rows if row.startswith('replenish threshold_wrap '))
    case('bad_slot_list', replenish.replace('retries=9,255,10', 'retries=9,255'), 2, 'three slot')
    case('bad_byte', replenish.replace('retries=9,255,10', 'retries=9,256,10'), 2, 'out of range')
    walk = next(row for row in rows if row.startswith('whale_walk nearby_last '))
    case('missing_fifth', walk.replace('draws=0,4,1,0,0', 'draws=0,4,1,0'), 2, 'exhausted')
    case('float_position', walk.replace('figure_x=50', 'figure_x=50.5'), 2, 'integer')
    request = next(row for row in rows if row.startswith('boat_request negative_money '))
    case('money_underflow', request.replace('money=-1', 'money=-2147483649'), 2, 'out of range')
    tick = next(row for row in rows if row.startswith('whaler_tick before_gate '))
    case('impossible_launch_fact', tick.replace('launch_slot=-1', 'launch_slot=0'),
         2, 'successful launch_slot')
    case('zero_need', tick.replace('need=16', 'need=0'), 2, 'out of range')
    require(ref, root / 'missing.txt', 2)
    checks += 1
    encoding = root / 'encoding.txt'
    encoding.write_bytes(b'\xff')
    require(ref, encoding, 2)
    checks += 1

    # Each source mutant must demonstrably apply and disagree with the hand
    # vectors (exit1), rather than fail by syntax error or unrelated exception.
    source = ref.read_text(encoding='utf-8')
    mutants = [
        ('drop_fifth', 'r.draw()  # Fifth world draw', '0  # Fifth world draw'),
        ('skip_modulus_one', "return r.result(index=r.draw() % r['count'])",
         "return r.result(index=0) if r['count'] == 1 else r.result(index=r.draw() % r['count'])"),
        ('saturate_retry', 'retries[i] = (retries[i] + 1) & 255',
         'retries[i] = min(255, retries[i] + 1)'),
        ('clamp_output', 'output = (output + added) & 65535',
         "output = min(r['capacity'], output + added) & 65535"),
        ('free_inactive', 'enqueued = int(reason == 0)',
         "enqueued = int(reason == 0 and r['active'] != 0)"),
        ('wrong_fallback', "next=0x1c if r['failed'] else 0x10", 'next=0x10'),
        ('miss_zero_hp', "r['old'] > 0 and r['new'] <= 0 and not r['ending']",
         "r['old'] > 0 and r['new'] < 0 and not r['ending']"),
    ]
    for name, old, new in mutants:
        mutant = root / (name + '.py')
        mutant.write_text(replace_once(source, old, new), encoding='utf-8')
        require(mutant, vectors, 1, 'disagreements')
        checks += 1
    # A checker must refuse a child that always succeeds, and must be bounded.
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
    print(f'whale reference contract: {checks} checks passed; {len(rows)} bundled vectors; '
          + ', '.join(f'{k}={counts[k]}' for k in sorted(counts)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ref', required=True, type=Path)
    parser.add_argument('--vectors', required=True, type=Path)
    args = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix='whale_ref_contract_') as tmp:
            contract(args.ref.resolve(), args.vectors.resolve(), Path(tmp))
    except (AssertionError, OSError, UnicodeError) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
