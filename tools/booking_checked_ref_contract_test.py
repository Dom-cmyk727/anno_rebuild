#!/usr/bin/env python3
"""Exercise the public checked-booking CLI, including plausible wrong models."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('--ref', type=Path, required=True)
parser.add_argument('--vectors', type=Path, required=True)
args, unittest_args = parser.parse_known_args()
document = json.loads(args.vectors.read_text())


class PublicContract(unittest.TestCase):
    def execute(self, value, reference=None):
        with tempfile.TemporaryDirectory(prefix='econbook-ref-') as tmp:
            path = Path(tmp) / 'input.json'
            path.write_text(json.dumps(value))
            return subprocess.run([sys.executable, '-I', str(reference or args.ref), str(path)],
                                  text=True, capture_output=True, timeout=10)

    def one(self, id):
        return dict(version=1, cases=[copy.deepcopy(next(c for c in document['cases'] if c['id'] == id))])

    def test_nonempty_complete_execution_with_actual_results(self):
        child = self.execute(document)
        self.assertEqual(child.returncode, 0, child.stderr)
        result = json.loads(child.stdout)
        self.assertGreaterEqual(result['executed'], 91)
        self.assertEqual(result['executed'], len(document['cases']))
        self.assertEqual([r['id'] for r in result['results']], [c['id'] for c in document['cases']])
        self.assertEqual([r['actual'] for r in result['results']], [c['expected'] for c in document['cases']])

    def test_bad_expected_answer_is_computed_not_echoed(self):
        value = self.one('ordinary-22500-plus128-div256-is88')
        value['cases'][0]['expected']['income'] = 89
        child = self.execute(value)
        self.assertEqual(child.returncode, 1, child.stderr)
        self.assertEqual(json.loads(child.stderr)['actual']['income'], 88)

    def test_partial_expected_cannot_hide_missing_execution(self):
        value = self.one('ordinary-22500-plus128-div256-is88')
        value['cases'][0]['expected'] = {'stepped': True}
        self.assertEqual(self.execute(value).returncode, 1)

    def test_expected_result_json_types_are_exact(self):
        for key, wrong in (('income', 88.0), ('upkeep', False), ('stepped', 1)):
            value = self.one('due-2000')
            value['cases'][0]['expected'][key] = wrong
            self.assertEqual(self.execute(value).returncode, 1)

    def test_invalid_documents_refused(self):
        one = self.one('due-2000')['cases'][0]
        for value in ([], {}, dict(version=1, cases=[]), dict(version=2, cases=[one]),
                      dict(version=1, cases=[one, one]),
                      dict(version=1, cases=[dict(id='type-only', kind='book')])):
            with self.subTest(value=value):
                self.assertEqual(self.execute(value).returncode, 2)

    def test_unknown_kind_and_empty_expected_refused(self):
        for key, value in (('kind', 'unknown'), ('expected', {}), ('input', [])):
            case = self.one('due-2000')
            case['cases'][0][key] = value
            self.assertEqual(self.execute(case).returncode, 2)

    def test_missing_file_and_malformed_json_refused(self):
        with tempfile.TemporaryDirectory(prefix='econbook-ref-bad-') as tmp:
            path = Path(tmp) / 'bad.json'
            child = subprocess.run([sys.executable, '-I', str(args.ref), str(path)], capture_output=True)
            self.assertEqual(child.returncode, 2)
            path.write_text('{broken')
            child = subprocess.run([sys.executable, '-I', str(args.ref), str(path)], capture_output=True)
            self.assertEqual(child.returncode, 2)

    def test_original_wrap_is_explicitly_distinct(self):
        # Independent signed dword calculation: 65535^2+128 = 2^32-130943;
        # trunc(-130943/256) = -511. Checked modern result is positive16776704.
        case = self.one('original-signed-wrap-is-not-modern-policy')
        self.assertEqual(case['cases'][0]['expected']['income'], 16776704)
        case['cases'][0]['expected']['income'] = -511
        self.assertEqual(self.execute(case).returncode, 1)

    def test_intended_mutants_fail_decisive_case_after_legal_control(self):
        # Each mutation compiles/runs and executes a nonempty legal control first.
        # No compilation error/timeout/asset refusal is accepted as a kill.
        mutations = [
            ('width', "U64, 'product'", "U32, 'product'", 'u32-product-high-control'),
            ('round-each-row', 'income = unsigned(total + 128, U64, \'rounding\') // 256',
             'income = sum(((p+s)*b+128)//256 for p,s,b in rows)', 'round-after-sum'),
            ('net-before-gross', "credited = bounded(money + income, I64_MIN, I64_MAX, 'credit')",
             'credited = money + income', 'credit-overflow-even-when-net-fits'),
            ('reset-before-fault', 'return dict(unchanged, fault=str(exc))',
             'return dict(unchanged, bills=[0]*len(rows), fault=str(exc))', 'u64-product-overflow'),
            ('double-book', 'if (tick - stamp) % (U16 + 1) < period:', 'if False:',
             'repeat-does-not-rebook'),
            ('house-rescan', "actor['cost'] = event['cost'][:]",
             "actor['cost'] = event['cost'][:]; costs[bucket] = contribution(actor)",
             'house-type-history-not-reconstructed'),
            ('weapons-raw-or', "any(on and eligible for on, eligible in zip(actor['on'], actor['eligible']))",
             "any(actor['on'])", 'weapons-selector-not-raw-or'),
            ('blanket-final-fault', 'stopped, bookings = False, []',
             "stopped, bookings = any(e['kind']=='fault' for e in events), []",
             'book-then-fault-preserves-booking'),
            ('unsigned-price-source', "bounded(args[0], I64_MIN, I64_MAX, 'Preis')",
             "unsigned(args[0], U32, 'Preis')", 'signed-preis-recovers'),
            ('unchecked-signed-accumulator', "bounded(value, I64_MIN, I64_MAX, 'price_accumulator')",
             'value', 'signed-accumulator-overflow-before-reset'),
            ('cost-values-instead-of-selectors', 'if was_active != active(actor):',
             'if old != new:', 'equal-cost-selector-still-preflights-history-underflow'),
            ('unbounded-malus-negation', "if kind == 'Malus' and amount == I64_MIN:",
             'if False:', 'signed-min-malus-negation-refused'),
        ]
        source = args.ref.read_text()
        control = self.one('ordinary-22500-plus128-div256-is88')
        for name, old, new, decisive in mutations:
            with self.subTest(mutant=name), tempfile.TemporaryDirectory(prefix='econbook-mutant-') as tmp:
                self.assertEqual(source.count(old), 1, 'mutation site changed: ' + name)
                reference = Path(tmp) / 'ref.py'
                reference.write_text(source.replace(old, new))
                child = self.execute(control, reference)
                self.assertEqual(child.returncode, 0, child.stderr)
                child = self.execute(self.one(decisive), reference)
                self.assertEqual(child.returncode, 1, child.stderr)
                self.assertEqual(json.loads(child.stderr)['id'], decisive)

    def test_echo_expected_mutant_is_rejected_by_bad_answer_control(self):
        source = args.ref.read_text()
        with tempfile.TemporaryDirectory(prefix='econbook-echo-') as tmp:
            reference = Path(tmp) / 'ref.py'
            reference.write_text(source.replace('actual = evaluate(case)', "actual = case['expected']"))
            legal = self.one('due-2000')
            self.assertEqual(self.execute(legal, reference).returncode, 0)
            legal['cases'][0]['expected']['income'] = 89
            # This demonstrates why the real contract above must require exit1.
            self.assertEqual(self.execute(legal, reference).returncode, 0)


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0], *unittest_args])
