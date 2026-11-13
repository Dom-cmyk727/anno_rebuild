#!/usr/bin/env python3
"""Fisher CLI contract and applied source mutants, through subprocesses only."""
import argparse
from collections import Counter
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


TYPES = {'farm_tick', 'converted', 'search_rect', 'sea_cell', 'trip', 'cut_match', 'corner_grid'}


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
    rows = []
    previous = ''
    for line in text.splitlines():
        stripped = line.split('#', 1)[0].strip()
        if stripped:
            if not previous.startswith('#'):
                raise AssertionError('each bundled vector needs a derivation comment above it')
            rows.append(stripped)
        if line.strip():
            previous = line.strip()
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

    good = 'sea_cell control heights=31,31,31,31 draws=- => open=1 free=511 consumed=0\n'
    case('good', good, 0, '1 vectors, 0 disagreements')
    changed = replace_once(text, '=> need=69 rate=17 consumed=0',
                           '=> need=68 rate=17 consumed=0')
    case('changed_expected', changed, 1, 'need: expected 68, got 69')
    case('empty', '', 2, 'zero vectors')
    case('comments_only', '# no vectors\n\n', 2, 'zero vectors')
    case('malformed', 'not a vector\n', 2, '=>')
    case('missing_field', good.replace('draws=- ', ''), 2, 'missing=draws')
    case('unknown_field', good.replace('draws=- ', 'draws=- extra=0 '), 2, 'unknown=extra')
    case('duplicate', good.replace('draws=- ', 'draws=- draws=1 '), 2, 'duplicate')
    case('unknown_output', good.replace('open=1', 'unknown=1'), 2, 'output keys')
    case('missing_consumed', good.replace(' consumed=0', ''), 2, 'including consumed')
    case('vacuous_expected', good.split('=>')[0] + '=> consumed=0\n', 2, 'all output keys')
    case('bad_expected', good.replace('open=1', 'open=one'), 2, 'integer')
    case('float_expected', good.replace('open=1', 'open=1.0'), 2, 'integer')
    case('negative_draw', good.replace('draws=-', 'draws=-1'), 2, 'out of range')
    case('overflow_draw', good.replace('draws=-', 'draws=4294967296'), 2, 'out of range')
    case('bad_list', good.replace('draws=-', 'draws=1,,2'), 2, 'integer')
    case('extra_draws_unused', good.replace('draws=-', 'draws=0,4294967295'), 0)
    case('unknown_type', good.replace('sea_cell ', 'unknown '), 2, 'unknown record type')
    case('missing_label', good.replace(' control', ''), 2, 'TYPE LABEL')
    case('two_arrows', good.rstrip() + ' =>\n', 2, '=>')
    case('mixed_malformed', good.replace('open=1', 'open=0') + 'bad\n', 2, '=>')
    for kind, oq in [('float_position', 'OQ-603'), ('continuation', 'OQ-603'),
                     ('transitive_draws', 'OQ-604')]:
        case(kind, f'{kind} refused draws=- => consumed=0\n', 2, oq)
    case('bad_height_count', good.replace('31,31,31,31', '31,31,31'), 2, 'four corner')
    case('bad_height_byte', good.replace('31,31,31,31', '31,31,31,256'), 2, 'out of range')
    case('float_height', good.replace('31,31,31,31', '31,31,31,31.5'), 2, 'integer')
    tick = next(row for row in rows if row.startswith('farm_tick at_gate '))
    case('draw_exhausted', tick.replace('draws=4294967295', 'draws=-'), 2, 'exhausted')
    case('zero_need', tick.replace('need=128', 'need=0'), 2, 'out of range')
    case('zero_rate', tick.replace('rate=51', 'rate=0'), 2, 'out of range')
    case('bad_switch', tick.replace('switch=1', 'switch=2'), 2, 'out of range')
    case('bad_flags', tick.replace('flags=0', 'flags=256'), 2, 'out of range')
    case('bad_workers', tick.replace('workers=0', 'workers=6'), 2, 'out of range')
    case('elapsed_overflow', tick.replace('elapsed=500', 'elapsed=65536'), 2, 'out of range')
    case('inconsistent_made', tick.replace('made=0', 'made=1'), 2, 'made fact')
    conversion = next(row for row in rows if row.startswith('converted farm3978 '))
    case('zero_denominator', conversion.replace('t0=5/1', 't0=5/0'), 2, 'out of range')
    case('zero_time', conversion.replace('t0=5/1', 't0=0/1'), 2, 't0 must be positive')
    case('float_fraction', conversion.replace('t0=5/1', 't0=5.0/1'), 2, 'integer')
    case('bare_fraction', conversion.replace('t0=5/1', 't0=5'), 2, 'exact fraction')
    case('converted_overflow', conversion.replace('i0=5/2', 'i0=4294967295/1'),
         2, 'u16 domain')
    rectangle = next(row for row in rows if row.startswith('search_rect normal '))
    case('zero_width', rectangle.replace('width=3', 'width=0'), 2, 'out of range')
    case('float_coordinate', rectangle.replace('x=100', 'x=100.5'), 2, 'OQ-603')
    case('word_coordinate', rectangle.replace('x=100', 'x=65536'), 2, 'out of range')
    case('byte_radius', rectangle.replace('radius=7', 'radius=256'), 2, 'out of range')
    require(ref, root / 'missing.txt', 2)
    checks += 1
    encoding = root / 'encoding.txt'
    encoding.write_bytes(b'\xff')
    require(ref, encoding, 2)
    checks += 1

    grid = next(row for row in rows if row.startswith('corner_grid square '))
    case('grid_count', grid.replace('heights=1,2,3,4', 'heights=1,2,3'), 2, 'sample count')
    case('grid_byte', grid.replace('heights=1,2,3,4', 'heights=1,2,3,256'), 2, 'out of range')
    case('grid_width', grid.replace('width=2', 'width=0'), 2, 'out of range')
    case('grid_expected', grid.replace('grid=1,2,0', 'grid=one,2,0'), 2, 'integer')
    case('grid_missing', grid.replace(' grid=1,2,0,3,4,0,0,0,0', ''), 2, 'all output keys')

    # Tiny self-authored chunk files check the real CLI boundary without assets.
    def chunk(name, data):
        return name.encode().ljust(12, b'\0') + struct.pack('<I', len(data)) + data

    corpus = root / 'corpus'
    scenes = corpus / 'Scenes'
    scenes.mkdir(parents=True)
    sample = scenes / 'mini.sze'

    def mini(height, fish=True, with_height=True):
        block = struct.pack('<HHBB', 0, 0, 1, 1)
        data = struct.pack('<HHHHI', 0, 0, 1, 1, 7)
        if with_height:
            data += chunk('HEIGHT', block + bytes([height]))
        data += chunk('GELAENDE', block + (struct.pack('<HHbb', 11, 0, 0, 0) if fish else b''))
        sample.write_bytes(chunk('ANNO', chunk('SCENE', chunk('INSEL5', data))))

    def corpus_case(code, diagnostic, counts=True):
        nonlocal checks
        argv = [sys.executable, '-I', str(ref), '--check-fish-water', str(corpus)]
        if counts:
            argv += ['--expected-scenarios', '1', '--expected-placements', '1']
        result = subprocess.run(argv, capture_output=True, text=True, timeout=5)
        output = result.stdout + result.stderr
        if result.returncode != code or diagnostic not in output or 'Traceback' in output:
            raise AssertionError(f'corpus: expected {code}/{diagnostic}; got '
                                 f'{result.returncode}: {output}')
        checks += 1
        return output

    mini(0)
    corpus_case(0, 'water=1 failed=0')
    corpus_case(2, 'expected 50 scenarios', counts=False)
    mini(128)
    output = corpus_case(1, 'water=0 failed=1')
    if output.count('EXCEPTION ') != 1 or 'corners=128,0,0,0 mean=32' not in output:
        raise AssertionError('real-file negative control must list the exact exception')
    mini(0, fish=False)
    corpus_case(2, 'expected 1 fish placements, found 0')
    mini(0, with_height=False)
    corpus_case(2, 'tile not covered')
    sample.unlink()
    corpus_case(2, 'expected 1 scenarios, found 0')

    # Every replacement is proved applied once before running the mutant. A
    # syntax error, exception or malformed-input exit2 is not a killed mutant.
    source = ref.read_text(encoding='utf-8')
    mutants = [
        ('replicate_right_border', '    return grid',
         '    for y in range(height):\n'
         '        grid[y * stride + width] = grid[y * stride + width - 1]\n'
         '    return grid'),
        ('replicate_bottom_border', '    return grid',
         '    grid[height * stride:] = grid[(height - 1) * stride:height * stride]\n'
         '    return grid'),
        ('no_full_output_draw', "and r['workers'] < r['maximum']",
         "and r['workers'] < r['maximum'] and output < r['capacity']"),
        ('clamp_output', 'output = (output + added) & 65535',
         "output = min(r['capacity'], output + added) & 65535"),
        ('sea_inclusive', "sum(r['heights']) // 4 < 32", "sum(r['heights']) // 4 <= 32"),
        ('drop_dispatch_draw', '                r.draw()', '                pass'),
        ('repeat_at_capacity', "r['cargo'] < r['capacity']", "r['cargo'] <= r['capacity']"),
        ('raw_before_debit', "384 // r['rate'] >= raw // r['need']",
         "384 // r['rate'] >= r['raw'] // r['need']"),
        ('rounded_input', 'need = 256 * td * inn // (tn * ind)', 'need = rate * inn // ind'),
        ('unwrapped_origin', "x = (r['x'] + (r['width'] - 1) // 2 - r['radius']) & 65535",
         "x = r['x'] + (r['width'] - 1) // 2 - r['radius']"),
        ('saturate_activity', 'activity = (activity + 16 * added) & 65535',
         'activity = min(65535, activity + 16 * added)'),
    ]
    for name, old, new in mutants:
        mutant = root / (name + '.py')
        mutant.write_text(replace_once(source, old, new), encoding='utf-8')
        require(mutant, vectors, 1, 'disagreements')
        checks += 1
        print('mutant ' + name + ': replacement applied once; exit1 disagreement')
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
    print(f'fisher reference contract: {checks} checks passed; {len(rows)} bundled vectors; '
          + ', '.join(f'{k}={counts[k]}' for k in sorted(counts)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ref', required=True, type=Path)
    parser.add_argument('--vectors', required=True, type=Path)
    args = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix='fisher_ref_contract_') as tmp:
            contract(args.ref.resolve(), args.vectors.resolve(), Path(tmp))
    except (AssertionError, OSError, UnicodeError) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
