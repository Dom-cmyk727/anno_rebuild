#!/usr/bin/env python3
"""Public-command controls for the supplied-fact ground reference."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


def contract(ref, vectors, root):
    rows = []
    previous = ''
    for line in vectors.read_text(encoding='utf-8').splitlines():
        if line.strip() and not line.startswith('#'):
            assert previous.startswith('#'), 'hand derivation required'
            rows.append(line)
        if line.strip():
            previous = line
    assert len(rows) >= 24, 'at least 24 vectors required'
    checks = 0

    def run(path, code, text):
        nonlocal checks
        p = subprocess.run([sys.executable, '-I', '-B', str(ref), str(path)],
                           capture_output=True, text=True, timeout=15)
        assert p.returncode == code, (path.name, p.returncode, p.stdout, p.stderr)
        assert text in p.stdout+p.stderr, (path.name, text, p.stdout, p.stderr)
        checks += 1

    run(vectors, 0, '24 vectors, 0 disagreements')
    good = rows[0]+'\n'

    def case(name, body, code, text):
        path = root/(name+'.txt')
        path.write_text(body, encoding='utf-8')
        run(path, code, text)

    def alter(name, before, after, code, text):
        assert before != after and good.count(before) == 1, 'replacement must match once'
        changed = good.replace(before, after)
        assert changed != good and after in changed, 'replacement not applied'
        case(name, changed, code, text)

    case('legal', good, 0, '1 vectors, 0 disagreements')
    alter('wrong', '=> ground=167', '=> ground=166', 1, 'expected')
    for name, body, text in [('empty', '', 'zero vectors'),
                             ('comments', '# nothing\n', 'zero vectors'),
                             ('arrow', good.replace('=>', ''), 'one =>'),
                             ('label', good+good, 'duplicate name')]:
        case(name, body, 2, text)
    alter('duplicate', 'w=1', 'w=1 w=1', 2, 'duplicate key')
    alter('unknown', 'w=1', 'other=1', 2, 'missing or unknown key')
    alter('zero', 'w=1', 'w=0', 2, 'out of range')
    alter('wide', 'w=1', 'w=4294967296', 2, 'out of range')
    alter('shape', 'w=1', 'w=2', 2, 'plane length')
    alter('lookup', 'lookup=0 ', 'lookup=- ', 2, 'lookup coverage')
    alter('signed_byte', 'lookup=0 ', 'lookup=-1 ', 2, 'out of range')
    alter('wide_byte', 'lookup=0 ', 'lookup=256 ', 2, 'out of range')
    alter('missing_fact', 'facts=0,N,0,0', 'facts=-', 2, 'missing object fact')
    alter('duplicate_fact', 'facts=0,N,0,0', 'facts=0,N,0,0;0,N,0,0', 2, 'duplicate object fact')
    alter('null_kind', 'facts=0,N,0,0', 'facts=0,N,225,0', 2, 'malformed object fact')
    alter('failed_value', 'facts=0,N,0,0', 'facts=0,F,225,1', 2, 'malformed object fact')
    alter('unknown_state', 'facts=0,N,0,0', 'facts=0,X,0,0', 2, 'malformed object fact')
    alter('origin', 'origin=0,0', 'origin=0', 2, 'origin shape')
    alter('end', 'origin=0,0', 'origin=2147483647,0', 0, '1 vectors')
    case('overflow', good.replace('origin=0,0', 'origin=2147483647,0').replace('w=1', 'w=2'),
         2, 'geometry exceeds')
    kernel = next(t for t in good.split() if t.startswith('kernel='))
    alter('kernel', kernel, 'kernel=0', 2, 'kernel needs 25 bytes')
    run(root/'missing', 2, 'No such file')
    print(f'{checks} CLI controls passed; {len(rows)} hand-derived vectors')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--ref', type=Path, required=True)
    parser.add_argument('--vectors', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='wildlife-ground-contract-') as directory:
        contract(args.ref.resolve(), args.vectors.resolve(), Path(directory))


if __name__ == '__main__':
    try:
        main()
    except (AssertionError, ValueError, OSError, UnicodeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
