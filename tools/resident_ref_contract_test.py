#!/usr/bin/env python3
"""Public-command contract of tools/resident_ref.py (stdlib only).

  resident_ref_contract_test.py --ref REF.py --vectors VECTORS [--timeout 20]

Every check runs the reference as a bounded child process. Exit 0 when all checks hold, 1 otherwise.
  empty vector file       must FAIL (exit 2: zero vectors is never success)
  malformed vector        must FAIL (exit 2)
  missing file            must FAIL (exit 2)
  correct vector          exit 0
  wrong expectation       exit 1 naming the key (a reference that echoes its input fails here)
  bundled vector file     exit 0 and the vector count is at least --min-vectors
The same checks are run against a stand-in that merely reports success (an echo); they must FAIL.
"""
import argparse
import os
import subprocess
import sys
import tempfile

HEAD = ("goods G count=96\n"
        "house H needs=26:16,27:16 lookup=26:0,27:1\n")
GOOD = "choice V goods=G house=H e=128 res=0 stock=0,0 out=26,0 avail=300,0 => "


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


def run_file(cmd, path, timeout):
    p = subprocess.run(cmd + [path], capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def check(cmd, vectors=None, timeout=20.0, min_vectors=100):
    """-> list of failure messages (empty = the reference honours the contract)."""
    bad = []

    def expect(name, got, want):
        if got != want:
            bad.append('%s: exit %s, wanted %s' % (name, got, want))
    expect('empty file', run(cmd, '# nothing\n', timeout)[0], 2)
    expect('malformed vector', run(cmd, HEAD + 'choice V goods=G house=H e=zzz => status=none\n', timeout)[0], 2)
    expect('missing file', subprocess.run(cmd + ['/nonexistent/vectors.txt'], capture_output=True,
                                          timeout=timeout).returncode, 2)
    code, _ = run(cmd, HEAD + GOOD + 'status=chosen score=128 goods=26\n', timeout)
    expect('correct vector', code, 0)
    code, out = run(cmd, HEAD + GOOD + 'status=chosen score=127 goods=26\n', timeout)
    expect('wrong expectation', code, 1)
    if 'expected 127' not in out:
        bad.append('wrong expectation: the output does not name the expected value')
    expect('choice without status', run(cmd, HEAD + GOOD + 'score=128\n', timeout)[0], 2)
    if vectors:
        code, out = run_file(cmd, vectors, timeout)
        expect('bundled vectors', code, 0)
        n = int(out.split()[0]) if out.split() and out.split()[0].isdigit() else 0
        if n < min_vectors:
            bad.append('bundled vectors: %d executed, at least %d wanted' % (n, min_vectors))
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ref', required=True)
    ap.add_argument('--vectors')
    ap.add_argument('--timeout', type=float, default=20.0)
    a = ap.parse_args()
    real = [sys.executable, '-I', a.ref]
    bad = check(real, a.vectors, a.timeout)
    for m in bad:
        print('FAIL', m)
    if bad:
        return 1
    echo = [sys.executable, '-I', '-c', 'import sys; print("1 vectors, 0 disagreements"); sys.exit(0)']
    echoed = check(echo, a.vectors, a.timeout)
    if not echoed:
        print('FAIL the echo stand-in passed the contract')
        return 1
    print('contract ok (%d checks reject the echo stand-in)' % len(echoed))
    return 0


if __name__ == '__main__':
    sys.exit(main())
