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
import runpy
import io
from contextlib import redirect_stdout
import subprocess
import tempfile
import unittest


# Share the public gate's sanitizer-report policy; do not hide reports in child files.
SANITIZER = runpy.run_path(str(Path(__file__).resolve().parent / 'test/ctest_verdict.py'))['SANITIZER']


def checked_child(code, output):
    if SANITIZER.search(output):
        raise ValueError('sanitizer report in child output')
    if code != 0:
        raise ValueError(f'child exit {code}')
    status = re.findall(r'^\[doctest\] Status:\s*(SUCCESS!|FAILURE!)\s*$', output, re.M)
    cases = re.findall(r'^\[doctest\] test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed(?:\s*\|\s*\d+ skipped)?\s*\|?\s*$', output, re.M)
    assertions = re.findall(r'^\[doctest\] assertions:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed\s*\|?\s*$', output, re.M)
    if status != ['SUCCESS!'] or len(cases) != 1 or tuple(map(int,cases[0])) != (1,1,0):
        raise ValueError('require one consistent successful executed testcase')
    if len(assertions) != 1:
        raise ValueError('missing/ambiguous assertion terminal record')
    total, passed, failed = map(int,assertions[0])
    if total <= 0 or total != passed + failed or passed != total or failed != 0:
        raise ValueError('empty/contradictory child assertion counts')


class Contracts(unittest.TestCase):
    good = '[doctest] test cases: 1 | 1 passed | 0 failed\n[doctest] assertions: 17 | 17 passed | 0 failed\n[doctest] Status: SUCCESS!\n'
    def test_legal(self):
        checked_child(0, self.good)
    def test_contradictory_summaries_cannot_pass(self):
        for cases, assertions in [('1 | 1 passed | 1 failed','17 | 17 passed | 0 failed'),
                                  ('1 | 1 passed | 0 failed','17 | 16 passed | 1 failed'),
                                  ('1 | 1 passed | 0 failed','17 | 1 passed | 0 failed'),
                                  ('2 | 2 passed | 0 failed','17 | 17 passed | 0 failed')]:
            wrong='[doctest] test cases: '+cases+'\n[doctest] assertions: '+assertions+'\n[doctest] Status: SUCCESS!\n'
            with self.assertRaises(ValueError): checked_child(0,wrong)
        with self.assertRaises(ValueError): checked_child(0,self.good+self.good)
        with self.assertRaises(ValueError): checked_child(0,self.good+'[doctest] Status: FAILURE!\n')

    def test_failure_cannot_pass(self):
        with self.assertRaises(ValueError):
            checked_child(1, self.good)
    def test_empty_cannot_pass(self):
        for text in ('', self.good.replace('17 |', '0 |'), self.good.replace('1 passed', '0 passed')):
            with self.assertRaises(ValueError):
                checked_child(0, text)

    def test_sanitizer_cannot_pass(self):
        for report in ('AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
                       'runtime error:', 'UndefinedBehaviorSanitizer'):
            with self.assertRaisesRegex(ValueError, 'sanitizer'):
                checked_child(0, self.good + report)
    def test_manifest_failure_has_no_success_marker(self):
        with tempfile.TemporaryDirectory() as temp:
            output = io.StringIO()
            with redirect_stdout(output), self.assertRaises(OSError):
                emit_success(Path(temp)/'absent', dict(verdict='PASS'))
            self.assertNotIn('PASS', output.getvalue())
    def test_manifest_is_committed_before_success(self):
        with tempfile.TemporaryDirectory() as temp:
            output = io.StringIO()
            with redirect_stdout(output):
                emit_success(Path(temp), dict(verdict='PASS'))
            self.assertEqual(json.loads((Path(temp)/'manifest.json').read_text()), dict(verdict='PASS'))
            self.assertIn('SCRIPT1_FRESH_PROCESS PASS', output.getvalue())


def commit_manifest(directory, manifest):
    path = directory/'manifest.json'
    path.write_text(json.dumps(manifest, indent=2)+'\n')
    if json.loads(path.read_text()) != manifest:
        raise ValueError('written manifest failed verification')


def emit_success(directory, manifest):
    commit_manifest(directory, manifest)
    print('SCRIPT1_FRESH_PROCESS PASS '+str(directory))


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
            try:
                checked_child(process.returncode, output)
            except ValueError:
                print(output)
                raise
        if records[0]['pid'] == records[1]['pid']:
            raise ValueError('producer and worker process identities must differ')
        artifacts = {}
        for name in ('normal.log','wrong-command.log','expected.txt'):
            path = directory/name
            if not path.is_file() or path.stat().st_size == 0:
                raise ValueError('missing/nonempty artifact: '+name)
            artifacts[name] = dict(size=path.stat().st_size, sha256=digest(path))
        manifest.update(artifacts=artifacts, verdict='PASS')
        emit_success(directory, manifest)
    except (OSError, ValueError) as error:
        manifest['verdict'] = 'FAIL'
        manifest['error'] = str(error)
        try:
            commit_manifest(directory, manifest)
        except (OSError, ValueError) as artifact_error:
            print('manifest unavailable: '+str(artifact_error))
        print('SCRIPT1_FRESH_PROCESS FAIL '+str(directory)+': '+str(error))
        return 1
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
