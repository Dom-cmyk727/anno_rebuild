#!/usr/bin/env python3
"""Bounded public CLI contract; all source fixtures are authored."""
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
    assert len(rows) == 13
    checks = 0

    def run(args, code, text):
        nonlocal checks
        p = subprocess.run([sys.executable, '-I', '-B', str(ref), *map(str, args)],
                           capture_output=True, text=True, timeout=15)
        assert p.returncode == code, (args, p.returncode, p.stdout, p.stderr)
        assert text in p.stdout+p.stderr, (args, text, p.stdout, p.stderr)
        checks += 1

    run([vectors], 0, '13 vectors, 0 disagreements')
    # Absolute authored path lets private manifests keep their source identity.
    good = rows[0].replace('source=growth/', 'source='+str(vectors.parent/'growth')+'/')+'\n'

    def vector(name, body, code, text):
        p = root/(name+'.txt')
        p.write_text(body, encoding='utf-8')
        run([p], code, text)

    vector('legal', good, 0, '1 vectors, 0 disagreements')
    before, after = 'bytes=64:1,', 'bytes=64:2,'
    assert good.count(before) == 1
    changed = good.replace(before, after)
    assert changed != good and after in changed
    vector('wrong', changed, 1, 'lookup/stats differ')
    vector('empty', '', 2, 'zero vectors')
    vector('comments', '# nothing\n', 2, 'zero vectors')
    vector('arrow', good.replace('=>', ''), 2, 'one =>')
    vector('duplicate', good+good, 2, 'duplicate name')
    vector('count', good.replace('ids=1', 'ids=bad'), 2, 'unsigned expected count')
    run([root/'missing'], 2, 'No such file')

    src = b'[WACHSTUM]\nSize: 1,1\nId: 1\nProz: 20\n[END]\n'
    output = root/'lookup'

    def source(name, data, code, text):
        path = root/(name+'.dat')
        path.write_bytes(data)
        output.write_bytes(b'unchanged')
        run(['--source', path, '--write-lookup', output], code, text)
        assert path.read_bytes() == data, 'source modified'
        if code:
            assert output.read_bytes() == b'unchanged', 'partial output published'
        else:
            expected = bytearray(16384)
            expected[64] = 20
            assert output.read_bytes() == expected

    source('source_legal', src, 0, '"records": 1')
    for name, tail in [('tail_max', b'2147483647'), ('tail_min', b'-2147483648'),
                       ('tail_multi_legal', b'1,-2147483648,2147483647')]:
        source(name, src.replace(b'Proz: 20', b'Proz: 20 '+tail), 0, '"records": 1')
    for name, tail in [('tail_over', b'2147483648'), ('tail_under', b'-2147483649'),
                       ('tail_later_over', b'1,2147483648'),
                       ('tail_later_under', b'1,-2147483649')]:
        source(name, src.replace(b'Proz: 20', b'Proz: 20 '+tail), 2, 'out of range')
    edits = [
        ('zero', b'Size: 1,1', b'Size: 0,1', 'staging domain'),
        ('large', b'Size: 1,1', b'Size: 257,1', 'staging domain'),
        ('overflow', b'Proz: 20', b'Proz: 2147483648', 'out of range'),
        ('hex', b'Proz: 20', b'Proz: 0x14', 'numeric syntax'),
        ('octal', b'Proz: 20', b'Proz: 020', 'numeric syntax'),
        ('trailing', b'Proz: 20', b'Proz: 20,', 'integer required'),
        ('extra_value', b'Proz: 20', b'Proz: 20,1', 'too many'),
        ('extra_row', b'Proz: 20', b'Proz: 20\nProz: 1', 'extra Proz'),
        ('incomplete', b'Size: 1,1', b'Size: 1,2', 'incomplete record'),
        ('size_repeat', b'Id: 1', b'Size: 1,1\nId: 1', 'Size only once'),
        ('size_late', b'Proz: 20', b'Size: 1,1\nProz: 20', 'Size only once'),
        ('missing_id', b'Id: 1\n', b'', 'Id required'),
        ('unknown', b'Proz:', b'Other:', 'unknown key'),
        ('section', b'[WACHSTUM]', b'[OTHER]', 'WACHSTUM required'),
        ('no_end', b'[END]\n', b'', 'END and'),
        ('tail', b'Proz: 20', b'Proz: 20 bad', 'numeric tail'),
        ('nul', b'Proz: 20', b'Proz: 20\0', 'without NUL'),
        ('comment', b'Id: 1', b'; [ comment\nId: 1', 'bracket in comment'),
        ('quote', b'Proz: 20', b'Proz: "20"', 'quoted code'),
        ('klima', b'[END]', b'Klima: "POLAR"\n[END]', 'quoted code'),
    ]
    for name, old, new, text in edits:
        assert src.count(old) == 1 and old != new
        changed = src.replace(old, new)
        assert changed != src
        source(name, changed, 2, text)
    source('newline', src[:-1], 2, 'newline-terminated')
    path = root/'same.dat'
    path.write_bytes(src)
    run(['--source', path, '--write-lookup', path], 2, 'differ from source')
    assert path.read_bytes() == src
    print(f'{checks} CLI controls passed; 13 hand-derived full-map vectors')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--ref', type=Path, required=True)
    parser.add_argument('--vectors', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='wildlife-growth-contract-') as directory:
        contract(args.ref.resolve(), args.vectors.resolve(), Path(directory))


if __name__ == '__main__':
    try:
        main()
    except (AssertionError, OSError, ValueError, UnicodeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
