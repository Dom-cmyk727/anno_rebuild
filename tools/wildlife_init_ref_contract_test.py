#!/usr/bin/env python3
"""Public CLI controls for the ordered wildlife initialization reference."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def invoke(ref, vectors):
    p = subprocess.run([sys.executable, '-I', str(ref), str(vectors)],
                       text=True, capture_output=True, timeout=10)
    output = p.stdout + p.stderr
    if 'Traceback' in output:
        raise AssertionError(output)
    return p.returncode, output


def contract(ref, vectors, root):
    rows = [json.loads(line) for line in vectors.read_text().splitlines()
            if line.strip() and not line.lstrip().startswith('#')]
    names = {row['name']: row for row in rows}
    if len(names) != 14 or len(rows) != 14:
        raise AssertionError('expected all 14 distinct hand-derived vectors')
    checks = 0

    def check(name, body, status, diagnostic='', reference=ref):
        nonlocal checks
        path = root / (name + '.jsonl')
        path.write_text(body, encoding='utf-8')
        code, output = invoke(reference, path)
        if code != status or diagnostic not in output:
            raise AssertionError(f'{name}: expected {status}/{diagnostic}, got {code}: {output}')
        checks += 1
        return output

    def row_case(name, row, code, diagnostic=''):
        return check(name, json.dumps(row) + '\n', code, diagnostic)

    check('real', vectors.read_text(), 0, '14 vectors, 0 disagreements')
    good = names['raw500']
    row_case('legal', good, 0)
    bad = copy.deepcopy(good)
    bad['expected']['records'][0]['timestamp'] = 0
    row_case('changed_expected', bad, 1, 'raw500: expected')
    check('empty', '', 2, 'zero vectors')
    check('comments', '# no executable records\n', 2, 'zero vectors')
    check('malformed', '{\n', 2, 'INVALID')
    check('unknown_row_key', json.dumps(good).replace('"input"', '"unknown"'), 2, 'object keys')
    check('duplicate_key', '{"name":"x","name":"y"}\n', 2, 'duplicate key')
    cases = []
    for name, mutate, diagnostic in [
        ('unknown_operation', lambda r: r['input'].update(operation='unread'), 'unknown operation'),
        ('unknown_input', lambda r: r['input'].update(extra=1), 'object keys'),
        ('unknown_header', lambda r: r['input']['islands'][0]['headers'][0].update(kind='unknown'), 'unknown header'),
        ('unknown_record', lambda r: r['input']['islands'][0]['headers'][0]['records'][0].update(extra=0), 'object keys'),
        ('raw_overflow', lambda r: r['input']['islands'][0].update(raw_stamp=65536), 'out of range'),
        ('negative_draw', lambda r: r['input'].update(draws=[-1]), 'out of range'),
        ('float_stamp', lambda r: r['input']['islands'][0].update(raw_stamp=500.0), 'out of range'),
        ('bool_mode', lambda r: r['input'].update(mode=True), 'out of range'),
        ('bad_expected', lambda r: r['expected'].update(consumed=-1), 'out of range'),
        ('exhausted', lambda r: r['input'].update(draws=[]), 'draw stream exhausted'),
        ('reused_island', lambda r: r['input']['islands'].append(copy.deepcopy(r['input']['islands'][0])), 'reused island'),
    ]:
        bad = copy.deepcopy(good)
        mutate(bad)
        cases.append((name, bad, diagnostic))
    for name, bad, diagnostic in cases:
        row_case(name, bad, 2, diagnostic)
    bad = copy.deepcopy(names['resource_draw_before_jitter'])
    bad['input']['draws'] = []
    row_case('intervening_exhausted', bad, 2, 'draw stream exhausted')
    bad = copy.deepcopy(names['restore_effective'])
    bad['input']['islands'][0]['headers'].insert(0, dict(kind='draw', site='load', count=1))
    row_case('restore_load_callback', bad, 2, 'restore cannot run load callbacks')
    missing = root / 'does-not-exist.jsonl'
    code, _ = invoke(ref, missing)
    if code != 2:
        raise AssertionError('missing file not refused')
    checks += 1
    source = ref.read_text()
    mutants = [
        ('wrong_order', 'enumerate(source)', 'enumerate(reversed(source))',
         'same_chunk_prepend', 'timestamp'),
        ('wrong_divisor', '// 500', '// 1000', 'raw500', 'timestamp'),
    ]
    for name, old, new, vector, field in mutants:
        if source.count(old) != 1:
            raise AssertionError(name + ': mutation must match exactly once')
        changed = source.replace(old, new)
        if changed == source or changed.count(new) != 1:
            raise AssertionError(name + ': replacement not applied')
        mutant = root / (name + '.py')
        mutant.write_text(changed, encoding='utf-8')
        output = check(name, json.dumps(names[vector]) + '\n', 1,
                       vector + ': expected', mutant)
        actual = json.loads(output.split(', got ', 1)[1].splitlines()[0])
        expected = names[vector]['expected']
        if name == 'wrong_order':
            # Same raw first draw now assigned to x 2 rather than x 1; original
            # x 2's timestamp 32 has become 2. This is not just a list spelling.
            if actual['records'][0]['x'] != 2 or actual['records'][0][field] != 2:
                raise AssertionError('wrong order did not alter the named draw assignment')
            if expected['records'][1][field] != 32:
                raise AssertionError('hand-derived assignment control changed')
        elif actual['records'][0][field] != 0 or expected['records'][0][field] != 1:
            raise AssertionError('divisor mutation did not fail raw 500 exactly')
        check(name + '_legal', json.dumps(names['raw499']) + '\n', 0,
              '1 vectors, 0 disagreements', mutant)
    print(f'wildlife initialization reference contract: {checks} checks passed; '
          '14 vectors; 2 applied source mutants killed; legal controls passed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ref', required=True, type=Path)
    parser.add_argument('--vectors', required=True, type=Path)
    args = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix='wildlife_init_ref_') as temp:
            contract(args.ref.resolve(), args.vectors.resolve(), Path(temp))
    except (AssertionError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
