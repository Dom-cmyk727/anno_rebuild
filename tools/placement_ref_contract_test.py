#!/usr/bin/env python3
"""Public-command contract of `placement_ref.py check` (stdlib only).

  placement_ref_contract_test.py --ref placement_ref.py --vectors FILE --min-vectors N

Exit 0 when every check holds. A missing, empty, malformed or too-short vector file must FAIL the reference (exit 2), a wrong
expectation must FAIL it (exit 1), the bundled file must pass with at least N cases. The same checks are run against a stand-in
that only echoes success; it must be caught (so this test cannot pass for a reference that checks nothing).
"""
import argparse
import os
import subprocess
import sys
import tempfile

GOOD = """type 1 1 1 97 0084 0
case g
  island 0 0 4 4
  v 0 3030303030
  v 1 3030303030
  v 2 3030303030
  v 3 3030303030
  place 1 1 1 0 2000000 0
  expect accept 48
"""


def run(cmd, text, extra=()):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(text)
        path = f.name
    try:
        return subprocess.run(cmd + [path] + list(extra), capture_output=True, text=True, timeout=60).returncode
    except subprocess.TimeoutExpired:
        return -1
    finally:
        os.unlink(path)


def checks(cmd, vectors, minimum):
    bad = []

    def want(name, got, w):
        if got != w:
            bad.append('%s: exit %s, wanted %s' % (name, got, w))
    want('empty file', run(cmd, '# nothing\n'), 2)
    want('malformed line', run(cmd, 'type x\n'), 2)
    want('incomplete case', run(cmd, 'type 1 1 1 97 0084 0\ncase c\n  island 0 0 4 4\n'), 2)
    want('too few cases', run(cmd, GOOD, ['--min', '2']), 2)
    want('good case', run(cmd, GOOD), 0)
    want('wrong expectation', run(cmd, GOOD.replace('accept 48', 'accept 47')), 1)
    want('wrong reason', run(cmd, GOOD.replace('expect accept 48', 'expect reject COVERAGE')), 1)
    want('missing file', subprocess.run(cmd + ['/nonexistent/vectors.txt'], capture_output=True).returncode, 2)
    want('bundled file', subprocess.run(cmd + [vectors, '--min', str(minimum)], capture_output=True).returncode, 0)
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ref', required=True)
    ap.add_argument('--vectors', required=True)
    ap.add_argument('--min-vectors', type=int, default=30)
    a = ap.parse_args()
    bad = checks([sys.executable, '-I', a.ref, 'check'], a.vectors, a.min_vectors)
    with tempfile.TemporaryDirectory() as d:
        stand = os.path.join(d, 'echo.py')
        open(stand, 'w').write('import sys\nprint("0 mismatches")\nsys.exit(0)\n')
        if not checks([sys.executable, '-I', stand], a.vectors, a.min_vectors):
            bad.append('a stand-in that always succeeds passed the contract')
    for b in bad:
        print('FAIL', b)
    print('placement_ref contract: %s' % ('FAIL' if bad else 'PASS'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
