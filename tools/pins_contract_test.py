#!/usr/bin/env python3
"""Pins CLI/orchestration contracts; recording uses private self-authored files only."""
import ast
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('pins', ROOT / 'tools/pins.py')
pins = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pins)


def oracle_collisions(rows, tools):
    values = {node.value for path in tools.glob('*.py')
              for node in ast.walk(ast.parse(path.read_text(encoding='utf-8')))
              if isinstance(node, ast.Constant) and type(node.value) is int}
    return sorted(name for name, (_, _, value) in rows.items()
                  if not pins.oracle_owned(name) and value in values)


class Contract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='pins-contract-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.current = self.root / 'fixtures/synthetic/sim/current'
        self.current.mkdir(parents=True)
        for name in pins.RECORDINGS:
            (self.current / name).write_text('# self-authored\n')
        self.path = self.current / 'goldens.txt'
        self.path.write_text('self PIN 138 0000000000000001\n')
        self.args = type('Args', (), dict(command='check', jobs=None, log=None,
                                        build_dir=self.root / 'build', config='',
                                        select=pins.SELECT, timeout=30))()

    def run_private(self, execute):
        out = io.StringIO()
        with patch.object(pins, 'ROOT', self.root), patch.dict(os.environ, {}, clear=True):
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
                code = pins.run(self.args, execute)
        return code, out.getvalue()

    def test_check_routes_through_canonical_gate_without_update(self):
        def execute(command, **kwargs):
            self.assertIn('run_selected.py', command[2])
            self.assertIn('--require-test', command)
            self.assertIn('pins_contract', command)
            self.assertNotIn('--record-fixture', command)
            self.assertNotIn('ANNO_UPDATE_GOLDEN', kwargs['env'])
            return subprocess.CompletedProcess(command, 0)
        self.assertEqual(self.run_private(execute)[0], 0)

    def test_regen_summary_and_recording_paths(self):
        self.args.command = 'regen'
        def execute(command, **kwargs):
            self.assertEqual(kwargs['env']['ANNO_UPDATE_GOLDEN'], '1')
            self.assertEqual(command.count('--record-fixture'), len(pins.RECORDINGS))
            self.path.write_text('self PIN 138 0000000000000002\n')
            return subprocess.CompletedProcess(command, 0)
        code, out = self.run_private(execute)
        self.assertEqual(code, 0)
        self.assertIn('pin self: PIN 138 0000000000000001 -> PIN 138 0000000000000002', out)
        self.assertEqual(out.count('pin self:'), 1)

    def test_version_only_move_is_visible(self):
        self.args.command = 'regen'
        def execute(command, **kwargs):
            self.path.write_text('self PIN 139 0000000000000001\n')
            return subprocess.CompletedProcess(command, 0)
        code, out = self.run_private(execute)
        self.assertEqual(code, 0)
        self.assertIn('PIN 138 0000000000000001 -> PIN 139 0000000000000001', out)

    def test_regen_excludes_only_the_oracle_test_entries(self):
        self.args.command = 'regen'
        def execute(command, **kwargs):
            selection = command[command.index('--select') + 1]
            self.assertEqual(selection, pins.REGEN_SELECT)
            self.assertNotIn('CMAKE_BUILD_PARALLEL_LEVEL', kwargs['env'])
            return subprocess.CompletedProcess(command, 0)
        self.assertEqual(self.run_private(execute)[0], 0)

    def test_real_oracle_numeric_ownership_and_original_counterexample(self):
        rows = pins.read_pins(ROOT / 'fixtures/synthetic/sim/current/goldens.txt')
        self.assertEqual(oracle_collisions(rows, ROOT / 'tools'), [])
        wrong = dict(rows)
        for name in ('registry_highmap_absent', 'registry_highmap_present'):
            wrong[name] = wrong.pop('oracle_' + name)
        self.assertEqual(oracle_collisions(wrong, ROOT / 'tools'),
                         ['registry_highmap_absent', 'registry_highmap_present'])

    def test_gate_failure_is_not_success(self):
        code, out = self.run_private(lambda *a, **k: subprocess.CompletedProcess(a[0], 7))
        self.assertEqual(code, 7)
        self.assertIn('PINS check: FAIL', out)

    def test_check_cannot_mutate(self):
        def execute(command, **kwargs):
            self.path.write_text('self PIN 138 0000000000000002\n')
            return subprocess.CompletedProcess(command, 0)
        self.assertEqual(self.run_private(execute)[0], 1)

    def test_oracle_regeneration_cannot_be_accepted(self):
        self.args.command = 'regen'
        self.path.write_text('oracle_self PIN 138 0000000000000001\n')
        def execute(command, **kwargs):
            self.path.write_text('oracle_self PIN 138 0000000000000002\n')
            return subprocess.CompletedProcess(command, 0)
        self.assertEqual(self.run_private(execute)[0], 1)

    def test_cli_refuses_parallel_regen(self):
        result = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/pins.py'),
                                 'regen', '--jobs', '2'], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertIn('writers are serialized', result.stderr)

    def test_cli_refuses_ci_regen(self):
        env = dict(os.environ, CI='1')
        result = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/pins.py'), 'regen'],
                                capture_output=True, text=True, env=env, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertIn('refused under CI', result.stderr)

    def test_cli_refuses_unknown_area(self):
        result = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/pins.py'), 'check', '--area', 'bad'],
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2)

    def test_malformed_duplicate_pin_is_refused(self):
        self.path.write_text('self PIN 138 0000000000000001\n' * 2)
        with self.assertRaises(ValueError):
            pins.read_pins(self.path)


if __name__ == '__main__':
    unittest.main()
