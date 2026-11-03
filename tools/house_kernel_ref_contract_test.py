#!/usr/bin/env python3
"""Public-command contract of tools/house_kernel_ref.py (stdlib only).

  house_kernel_ref_contract_test.py --ref REF.py --vectors VECTORS [--timeout 20]

Every check runs the reference as a bounded child process. Exit 0 when all checks hold, 1 otherwise.
  empty vector file            must FAIL (exit 2: zero vectors is never success)
  malformed vector             must FAIL (exit 2)
  missing file                 must FAIL (exit 2)
  correct vector               exit 0
  wrong expectation            exit 1 naming the key (a reference that echoes its input fails here)
  evaluated without mood/res/e exit 2 (a nominal-success expectation is refused)
  bundled vector file          exit 0 and the vector count is at least --min-vectors
The module also exposes `check(ref_cmd)` so a test can run these checks against a stand-in reference
that merely reports success; --self-test does exactly that and requires the checks to FAIL.
"""
import argparse
import os
import subprocess
import sys
import tempfile

HEAD = ("table T 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n"
        "group G wohnmax=8 nodown=0 vermehr=-16,-8,0,8,16 nahrung=T bedarf=T infra=T needs=26:0:16\n")
GOOD = "vec V group=G now=2500 last=0 period=2500 mask=1 e=128 res=64 stock=0 prev=8 => "


def run(cmd, text, timeout):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(text)
        path = f.name
    try:
        p = subprocess.run(cmd + [path], capture_output=True, text=True, timeout=timeout)
        return p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        return -1, 'timeout'
    finally:
        os.unlink(path)


def check(cmd, vectors=None, timeout=20.0, min_vectors=40):
    """-> list of failure messages (empty = the reference honours the contract)."""
    bad = []

    def expect(name, got, want):
        if got != want:
            bad.append('%s: exit %s, wanted %s' % (name, got, want))
    expect('empty file', run(cmd, '# nothing\n', timeout)[0], 2)
    expect('malformed vector', run(cmd, HEAD + 'vec V group=G now=zzz => status=skipped\n', timeout)[0], 2)
    expect('missing file', subprocess.run(cmd + ['/nonexistent/vectors.txt'], capture_output=True,
                                          timeout=timeout).returncode, 2)
    # mood 0 tables: mood = trunc((0+0+0+1)/3) = 0, so a zero-stock body evaluates.
    code, out = run(cmd, HEAD + GOOD + 'status=evaluated last=2500 e=128 mood=0 res=64\n', timeout)
    expect('correct vector', code, 0)
    code, out = run(cmd, HEAD + GOOD + 'status=evaluated last=2500 e=999 mood=0 res=64\n', timeout)
    expect('wrong expectation', code, 1)
    if 'expected 999' not in out:
        bad.append('wrong expectation: the output does not name the expected value')
    expect('evaluated without result keys',
           run(cmd, HEAD + GOOD + 'status=evaluated last=2500\n', timeout)[0], 2)
    if vectors:
        code, out = run_file(cmd, vectors, timeout)
        expect('bundled vectors', code, 0)
        n = int(out.split()[0]) if out.split() and out.split()[0].isdigit() else 0
        if n < min_vectors:
            bad.append('bundled vectors: %d executed, at least %d wanted' % (n, min_vectors))
    return bad


def run_file(cmd, path, timeout):
    p = subprocess.run(cmd + [path], capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ref', required=True)
    ap.add_argument('--vectors')
    ap.add_argument('--timeout', type=float, default=20.0)
    ap.add_argument('--self-test', action='store_true')
    a = ap.parse_args()
    real = [sys.executable, '-I', a.ref]
    bad = check(real, a.vectors, a.timeout)
    for m in bad:
        print('FAIL', m)
    if bad:
        return 1
    # A reference that only reports success (echoes) must fail the same checks.
    echo = [sys.executable, '-I', '-c', 'import sys; print("1 vectors, 0 disagreements"); sys.exit(0)']
    echoed = check(echo, a.vectors, a.timeout)
    if not echoed:
        print('FAIL the echo stand-in passed the contract')
        return 1
    print('contract ok (%d checks reject the echo stand-in)' % len(echoed))
    return 0


if __name__ == '__main__':
    sys.exit(main())
