#!/usr/bin/env python3
"""Public-CLI negative checks for the conditional ordinary geometry reference."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def invoke(ref, vectors):
    p = subprocess.run([sys.executable, '-I', str(ref), str(vectors)],
                       capture_output=True, text=True, timeout=10)
    if 'Traceback' in p.stdout + p.stderr:
        raise AssertionError('uncaught reference error: ' + p.stdout + p.stderr)
    return p.returncode, p.stdout + p.stderr


def contract(ref, vectors, root):
    text = vectors.read_text(encoding='utf-8')
    rows = json.loads(text)
    if {r['kind'] for r in rows} != {'position', 'height', 'table'}:
        raise AssertionError('missing record family')
    checks = 0
    def check(path, code, reference=ref):
        nonlocal checks
        actual, output = invoke(reference, path)
        if actual != code:
            raise AssertionError(f'{path.name}: expected {code}, got {actual}: {output}')
        checks += 1
        return output
    check(vectors, 0)
    bad = json.loads(text)
    bad[0]['expected']['x'] += 1
    p = root/'wrong-expected.json'
    p.write_text(json.dumps(bad), encoding='utf-8')
    check(p, 1)
    for name, body in [('empty', '[]'), ('malformed', '['), ('bad-schema', '[{}]')]:
        p = root/(name+'.json')
        p.write_text(body, encoding='utf-8')
        check(p, 2)
    bad = json.loads(text)
    bad[0]['inputs']['pc'] = 80
    p = root/'bad-profile.json'
    p.write_text(json.dumps(bad), encoding='utf-8')
    check(p, 2)
    bad = json.loads(text)
    bad[0]['inputs']['rc'] = 'host-default'
    p = root/'unknown-rounding.json'
    p.write_text(json.dumps(bad), encoding='utf-8')
    check(p, 2)
    check(root/'absent.json', 2)
    # Mutate actual source, proving each replacement applied before execution.
    source = ref.read_text(encoding='utf-8')
    for name, before, after in [
        ('rounded-high-shortcut', 'a.mul(F(1), offset)', 'F(1) - value(0x3f555555)'),
        ('height-table-scale', 'F(5368709, 1 << 27)', 'F(10737418, 1 << 27)'),
        ('strict-diagonal', 'if left <= x <= right:', 'if left <= x < right:'),
        ('ignore-height-rounding',
         'def height(x, z, corners, pc, rc):\n    a = Arithmetic(pc, rc)',
         "def height(x, z, corners, pc, rc):\n    a = Arithmetic(pc, 'nearest')"),
        ('ignore-height-precision',
         'def height(x, z, corners, pc, rc):\n    a = Arithmetic(pc, rc)',
         'def height(x, z, corners, pc, rc):\n    a = Arithmetic(53, rc)'),
        ('omit-left-height-spill', 'low = a.s(edge(ha, hc, t, d))',
         'low = edge(ha, hc, t, d)')]:
        if source.count(before) != 1:
            raise AssertionError(name + ': replacement did not match exactly once')
        mutant = source.replace(before, after)
        if mutant == source or mutant.count(after) != source.count(after) + 1:
            raise AssertionError(name + ': replacement not applied')
        path = root/(name+'.py')
        path.write_text(mutant, encoding='utf-8')
        output = check(vectors, 1, path)
        diagnostics = {
            'ignore-height-rounding': '25 y: expected 1056964609, got 1056964608',
            'ignore-height-precision': '26 y: expected 1056964609, got 1056964608',
            'omit-left-height-spill': '28 y: expected 1065353215, got 1065353216'}
        if name in diagnostics and (diagnostics[name] not in output or
                                    '29 vectors, 1 disagreements' not in output):
            raise AssertionError(name + ': intended numeric assertion/control coverage missing')
        print('actual source mutant rejected: ' + name)
    print(f'geometry contract: {checks} checks, 0 failures')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--ref', type=Path, required=True)
    parser.add_argument('--vectors', type=Path, required=True)
    args = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix='wildlife-geometry-contract-') as name:
            contract(args.ref.resolve(), args.vectors.resolve(), Path(name))
        return 0
    except (AssertionError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
