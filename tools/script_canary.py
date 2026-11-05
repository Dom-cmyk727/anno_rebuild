#!/usr/bin/env python3
"""SCRIPT1 complete synthetic canary: independent producer/replayer processes.

Each invocation retains its logs, encoded inputs and exact executable identity.
An ordinary in-process test is not classified as fresh-process evidence.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


def checked_child(code, output):
    if code != 0:
        raise ValueError(f'child exit {code}')
    if '[doctest] Status: SUCCESS!' not in output:
        raise ValueError('missing successful doctest terminal record')
    if not re.search(r'\[doctest\] test cases:\s+1\s+\|\s+1 passed', output):
        raise ValueError('expected exactly one executed testcase')
    if not re.search(r'\[doctest\] assertions:\s+[1-9][0-9]*\s+\|', output):
        raise ValueError('empty child assertions')


class Contracts(unittest.TestCase):
    good = '[doctest] test cases: 1 | 1 passed | 0 failed\n[doctest] assertions: 17 | 17 passed | 0 failed\n[doctest] Status: SUCCESS!\n'
    def test_legal(self):
        checked_child(0, self.good)
    def test_failure_cannot_pass(self):
        with self.assertRaises(ValueError):
            checked_child(1, self.good)
    def test_empty_cannot_pass(self):
        for text in ('', self.good.replace('17 |', '0 |'), self.good.replace('1 passed', '0 passed')):
            with self.assertRaises(ValueError):
                checked_child(0, text)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(executable, repo):
    root = repo / 'cache/script1/replays'
    root.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix='fresh-', dir=root))
    env = dict(os.environ, ANNO_SCRIPT_CANARY_DIR=str(directory))
    records = []
    manifest = dict(executable=str(executable), sha256=digest(executable), processes=records,
                    source_revision=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'], text=True).strip(),
                    verdict='INCOMPLETE', directory=str(directory))
    try:
        for role, case in [('producer','SCRIPT1 mod canary producer'),('worker','SCRIPT1 encoded canary replay worker')]:
            command = [str(executable), '--test-case='+case]
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       cwd=repo, env=env)
            output = process.communicate()[0].decode('utf-8', errors='replace')
            (directory / (role+'.log')).write_text(output)
            records.append(dict(role=role, pid=process.pid, command=command, exit=process.returncode))
            checked_child(process.returncode, output)
        if records[0]['pid'] == records[1]['pid']:
            raise ValueError('producer and worker process identities must differ')
        artifacts = {}
        for name in ('normal.log','wrong-command.log','expected.txt'):
            path = directory/name
            if not path.is_file() or path.stat().st_size == 0:
                raise ValueError('missing/nonempty artifact: '+name)
            artifacts[name] = dict(size=path.stat().st_size, sha256=digest(path))
        manifest.update(artifacts=artifacts, verdict='PASS')
        print('SCRIPT1_FRESH_PROCESS PASS '+str(directory))
    except (OSError, ValueError) as error:
        manifest['verdict'] = 'FAIL'
        manifest['error'] = str(error)
        print('SCRIPT1_FRESH_PROCESS FAIL '+str(directory)+': '+str(error))
        return 1
    finally:
        (directory/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--exe', type=Path)
    args = parser.parse_args()
    if args.self_test:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(Contracts)
        raise SystemExit(0 if unittest.TextTestRunner().run(suite).wasSuccessful() else 1)
    if not args.exe:
        parser.error('--exe is required')
    raise SystemExit(run(args.exe.resolve(), Path(__file__).resolve().parents[1]))
