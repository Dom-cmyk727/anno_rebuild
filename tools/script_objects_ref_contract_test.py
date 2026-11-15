#!/usr/bin/env python3
"""SO-1a public-command contract and verified source mutants; no game files."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


def invoke(ref, path):
    r = subprocess.run([sys.executable, '-I', str(ref), str(path)],
                       capture_output=True, text=True, timeout=10)
    return r.returncode, r.stdout + r.stderr


def require(ref, path, code, message=''):
    actual, output = invoke(ref, path)
    if actual != code or message not in output or 'Traceback' in output:
        raise AssertionError(f'{path.name}: expected {code}/{message!r}, got {actual}: {output}')


def replace_once(text, old, new):
    if old == new or text.count(old) != 1:
        raise AssertionError('mutant must match exactly once')
    changed = text.replace(old, new, 1)
    if changed == text:
        raise AssertionError('mutant did not apply')
    return changed


def contract(ref, vectors, root):
    text = vectors.read_text(encoding='utf-8')
    rows = {}
    previous = ''
    for line in text.splitlines():
        if line.strip() and not line.lstrip().startswith('#'):
            if not previous.startswith('#'):
                raise AssertionError('each vector requires a hand-derivation comment')
            label = line.split()[1]
            if label in rows:
                raise AssertionError('duplicate vector label')
            rows[label] = line
        if line.strip():
            previous = line.strip()
    needed = {'bound_alive', 'bound_destroyed', 'absent_at_parse', 'appears_later',
              'recreated', 'owner_and_empty', 'other_city', 'zero', 'low_word',
              'negative_word', 'duplicate_first', 'bad_name', 'full_name'}
    if set(rows) != needed:
        raise AssertionError('missing required separating vectors')
    require(ref, vectors, 0, f'{len(rows)} vectors, 0 disagreements')
    checks = 1

    def case(label, body, code, message=''):
        nonlocal checks
        path = root / (label + '.txt')
        path.write_text(body, encoding='utf-8')
        require(ref, path, code, message)
        checks += 1

    alive = rows['bound_alive'] + '\n'
    case('changed_expected', replace_once(alive, 'killed=0 ', 'killed=1 '), 1,
         'killed: expected 1, got 0')
    case('empty', '', 2, 'zero vectors')
    case('comments', '# comment only\n', 2, 'zero vectors')
    case('malformed', 'query bad\n', 2, '=>')
    case('unknown_type', alive.replace('query ', 'other ', 1), 2, 'invalid/duplicate')
    case('duplicate_label', alive + alive, 2, 'duplicate')
    case('duplicate_field', alive.replace('ref=alpha ', 'ref=alpha ref=alpha '), 2, 'duplicate')
    case('missing_input', alive.replace('city_slots=16 ', ''), 2, 'input keys')
    case('unknown_input', alive.replace('city_slots=16 ', 'city_slots=16 extra=0 '), 2, 'unknown=extra')
    case('missing_output', alive.replace(' writes=0', ''), 2, 'output keys')
    case('bad_expected', alive.replace('killed=0 ', 'killed=2 '), 2, 'Boolean sequence')
    case('bad_expected_count', alive.replace('writes=0', 'writes=no'), 2, 'integer')
    case('bad_snapshot', alive.replace('frames=2:1:3:1', 'frames=2:1:3'), 2, 'city needs')
    case('duplicate_city', alive.replace('frames=2:1:3:1', 'frames=2:1:3:1,2:1:8:0'), 2, 'duplicate city')
    case('duplicate_island', alive.replace('islands=1,2', 'islands=1,2,2'), 2, 'duplicate island')
    case('missing_island', alive.replace('objects=alpha:258', 'objects=alpha:259'), 2, 'null island')
    case('bad_city_domain', alive.replace('objects=alpha:258', 'objects=alpha:4098'), 2,
         'readable city-slot domain')
    case('empty_objects', rows['bad_name'].replace('objects=alpha:258', 'objects=-'), 0,
         '1 vectors, 0 disagreements')
    case('non_ascii', alive.replace('ref=alpha ', 'ref=ä '), 2, 'ASCII')
    case('wide_operand', alive.replace('objects=alpha:258', 'objects=alpha:2147483648'), 2, 'out of range')
    case('float_operand', alive.replace('objects=alpha:258', 'objects=alpha:258.0'), 2, 'integer')
    require(ref, root / 'does-not-exist', 2, 'ERROR:')
    checks += 1
    source = ref.read_text(encoding='utf-8')
    mutants = [
        ('swapped_bytes', 'island, city = packed & 0xff, (packed >> 8) & 0xff',
         'island, city = (packed >> 8) & 0xff, packed & 0xff', 'bound_alive'),
        ('cached_at_parse', 'present = target in frame', 'present = target in initial', 'appears_later'),
        ('empty_buildings', 'present = target in frame',
         'present = target in frame and frame[target][1] != 0', 'owner_and_empty'),
        ('owner_latch', 'present = target in frame',
         'present = target in frame and frame[target][0] == initial.get(target, (-1, 0))[0]',
         'owner_and_empty'),
        ('sticky_killed', "killed.append('0' if present else '1')",
         "killed.append('1' if '1' in killed or not present else '0')", 'recreated'),
        ('last_duplicate', 'for symbol, n in objects if', 'for symbol, n in reversed(objects) if',
         'duplicate_first'),
        ('name_prefix', 'ref = name(given[\'ref\'])',
         'ref = name(given[\'ref\']).split(\'@\', 1)[0]', 'full_name'),
    ]
    for label, old, new, separating in mutants:
        mutant = root / (label + '.py')
        mutant.write_text(replace_once(source, old, new), encoding='utf-8')
        sample = root / (label + '-vector.txt')
        sample.write_text(rows[separating] + '\n', encoding='utf-8')
        require(mutant, sample, 1, 'disagreements')
        # This legal control keeps island == city, stable owner, and positive building count.
        control = root / (label + '-legal.txt')
        control.write_text(rows['zero'] + '\n', encoding='utf-8')
        require(mutant, control, 0, '1 vectors, 0 disagreements')
        checks += 2
        print(f'mutant {label}: replacement applied once; separating exit 1; legal exit 0')
    print(f'script objects reference contract: {checks} checks passed; {len(rows)} vectors')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ref', required=True, type=Path)
    ap.add_argument('--vectors', required=True, type=Path)
    args = ap.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix='anno-script-objects-') as tmp:
            contract(args.ref.resolve(), args.vectors.resolve(), Path(tmp))
    except (AssertionError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
