#!/usr/bin/env python3
"""Tests of the gap patch survey (tools/river_qa_gap.py).

  python tools/test_river_qa_gap.py [--exe path/to/river_qa]

The pure functions are always tested. With --exe and ANNO1503_GAME_DIR set, river_qa itself is
run on one river island with --gap-patch 0 and 8 (skipped with a message otherwise).
"""
import os
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import river_qa_gap as gap  # noqa: E402

EXE = None


def read_text(path):
    with open(path, encoding='utf-8') as f:
        return f.read()

HEADER = ('| island | O px | covered | edge px | gap px | extra px | O comps | E comps | split | '
          'lost | gap comps |\n|---|---|---|---|---|---|---|---|---|---|---|\n')


def row(tag, o=1000, covered=990, edge=5, gp=5, extra=100, oc=3, ec=4, split=0, lost=0, gc=1):
    return '| %s (s=2) | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d |\n' % (
        tag, o, covered, edge, gp, extra, oc, ec, split, lost, gc)


class Parse(unittest.TestCase):
    def test_parses_island_rows_and_ignores_other_lines(self):
        text = HEADER + row('A_B_g1_i0', ec=7) + '| scene | group | islands |\n| A_B | 1 | 1 | 5 |\n'
        rows = gap.parse_island_rows(text)
        self.assertEqual(list(rows), [('A_B', 1, 0)])
        r = rows[('A_B', 1, 0)]
        self.assertEqual((r['o'], r['e_comps'], r['s']), (1000, 7, 2))

    def test_scene_names_with_underscores_and_group_digits(self):
        rows = gap.parse_island_rows(row('D_256x256_0045_5B_g2_i13'))
        self.assertEqual(list(rows), [('D_256x256_0045_5B', 2, 13)])

    def test_wrong_column_count_is_an_error(self):
        with self.assertRaises(ValueError):
            gap.parse_island_rows('| X_g0_i0 (s=1) | 1 | 2 |\n')


class Rules(unittest.TestCase):
    def rows(self, **kw):
        return gap.parse_island_rows(row('X_g0_i0', **kw))[('X', 0, 0)]

    def test_no_regression_when_better(self):
        self.assertEqual(gap.regressions(self.rows(ec=16, gp=9), self.rows(ec=7, gp=0)), [])

    def test_more_components_is_a_regression(self):
        self.assertEqual(gap.regressions(self.rows(ec=4), self.rows(ec=5)), ['components 4 -> 5'])

    def test_split_lost_and_gap_are_regressions(self):
        r = gap.regressions(self.rows(), self.rows(split=1, lost=2, gp=6))
        self.assertEqual(r, ['split 0 -> 1', 'lost 0 -> 2', 'gap px 5 -> 6'])

    def test_extra_water_threshold_is_a_share_of_the_original_pixels(self):
        before = self.rows(o=1000, extra=100)
        self.assertEqual(gap.regressions(before, self.rows(o=1000, extra=350)), [])
        self.assertEqual(len(gap.regressions(before, self.rows(o=1000, extra=351))), 1)

    def test_zero_original_pixels_does_not_divide_by_zero(self):
        self.assertEqual(len(gap.regressions(self.rows(o=0, extra=0), self.rows(o=0, extra=1))), 1)


class Join(unittest.TestCase):
    def runs(self):
        b = gap.parse_island_rows(row('A_g0_i0', gp=5) + row('A_g1_i0', gp=7) + row('B_g1_i0', gp=1))
        a = gap.parse_island_rows(row('A_g0_i0', gp=0) + row('A_g1_i0', gp=9, ec=6) +
                                  row('C_g1_i0'))
        return b, a

    def test_join_keeps_keys_present_in_both(self):
        j = gap.join_runs(*self.runs())
        self.assertEqual(sorted(j), [('A', 0)])
        self.assertEqual(sorted(j[('A', 0)]), [0, 1])

    def test_badness_ranks_regressions_before_remaining_gaps(self):
        b, a = self.runs()
        worse = gap.join_runs(b, a)[('A', 0)]
        quiet = gap.join_runs(gap.parse_island_rows(row('Q_g0_i0', gp=50)),
                              gap.parse_island_rows(row('Q_g0_i0', gp=40)))[('Q', 0)]
        self.assertGreater(gap.badness(worse), gap.badness(quiet))

    def test_table_and_summary(self):
        j = gap.join_runs(*self.runs())
        table = gap.survey_table(j).splitlines()
        self.assertEqual(len(table), 3)
        self.assertIn('A i0', table[2])
        self.assertIn('5 -> 0', table[2])
        self.assertIn('g1 components 4 -> 6; gap px 7 -> 9', table[2])
        summary = gap.survey_summary(j, '8')
        self.assertIn('islands with a regression (patch 8 against 0): 1', summary)
        self.assertIn('g1 gap px total: 7 -> 9', summary)
        self.assertIn('g0 O px total: 1000 -> 1000', summary)


class Worst(unittest.TestCase):
    def test_identical_variants_count_once_and_order_is_by_badness(self):
        b = gap.parse_island_rows(row('A_g0_i0', ec=9) + row('A2_g0_i0', ec=9) + row('B_g0_i0', ec=6))
        a = gap.parse_island_rows(row('A_g0_i0', ec=9) + row('A2_g0_i0', ec=9) + row('B_g0_i0', ec=3))
        j = gap.join_runs(b, a)
        self.assertEqual(gap.worst_islands(j, 2), [('A', 0), ('B', 0)])
        self.assertEqual(gap.worst_islands(j, 5), [('A', 0), ('B', 0)])
        self.assertEqual(gap.worst_islands(j, 1), [('A', 0)])

    def pair(self, tag, before, after):
        return gap.join_runs(gap.parse_island_rows(row(tag, **before)),
                             gap.parse_island_rows(row(tag, **after)))

    def test_ranking_is_regressions_then_remaining_gaps_then_growth_then_surplus(self):
        # extra growth: A +20 % of the original px, B +5 %; C has a gap pixel more, D a surplus
        a = self.pair('A_g0_i0', {}, dict(extra=300))[("A", 0)]
        b = self.pair('B_g0_i0', {}, dict(extra=150))[('B', 0)]
        c = self.pair('C_g0_i0', {}, dict(extra=100, gp=6))[('C', 0)]
        d = self.pair('D_g0_i0', dict(ec=4, oc=3), dict(extra=100, ec=4, oc=3))[('D', 0)]
        e = self.pair('E_g0_i0', dict(ec=4, oc=3), dict(extra=100, ec=2, oc=3))[('E', 0)]
        self.assertEqual(gap.badness(a), (0, 5, 0.2, 1))
        self.assertEqual(gap.badness(b), (0, 5, 0.05, 1))
        self.assertEqual(gap.badness(d), (0, 5, 0.0, 1))
        self.assertEqual(gap.badness(e), (0, 5, 0.0, 0))  # fewer components than O: clamped at 0
        self.assertGreater(gap.badness(c), gap.badness(a))  # a regression comes first
        self.assertGreater(gap.badness(a), gap.badness(b))
        self.assertGreater(gap.badness(b), gap.badness(d))
        self.assertGreater(gap.badness(d), gap.badness(e))

    def test_same_counts_in_another_group_or_other_after_values_are_not_duplicates(self):
        b = gap.parse_island_rows(row('A_g0_i0') + row('B_g1_i0') + row('C_g0_i0') + row('D_g0_i0'))
        a = gap.parse_island_rows(row('A_g0_i0') + row('B_g1_i0') + row('C_g0_i0', ec=1) +
                                  row('D_g0_i0', extra=900))
        j = gap.join_runs(b, a)
        self.assertEqual(len(gap.worst_islands(j, 9)), 4)

    def test_summary_lists_coverage_growth_split_lost_and_edge(self):
        b = gap.parse_island_rows(row('A_g0_i0', split=1, lost=2, edge=40, extra=100))
        a = gap.parse_island_rows(row('A_g0_i0', split=0, lost=1, edge=30, extra=300))
        text = gap.survey_summary(gap.join_runs(b, a), '8',
                                  {'tried': 5, 'loaded': 4, 'failed': 1, 'with-river': 2})
        for line in ('g0 split total: 1 -> 0', 'g0 lost total: 2 -> 1',
                     'g0 edge px total: 40 -> 30',
                     'scenes: tried 5, loaded 4, failed 1, with river pieces 2',
                     'g0 water outside the painted bed, share of original river px: 10.0 % -> '
                     '30.0 % (largest growth 20.0 %, A i0)'):
            self.assertIn(line, text)

    def test_table_separator_matches_the_header_and_shows_growth(self):
        j = gap.join_runs(gap.parse_island_rows(row('A_g0_i0') + row('A_g1_i0')),
                          gap.parse_island_rows(row('A_g0_i0', extra=200) + row('A_g1_i0')))
        lines = gap.survey_table(j).splitlines()
        self.assertEqual(lines[0].count('|'), lines[1].count('|'))
        self.assertIn('extra growth g0', lines[0])
        self.assertIn('10.0 %', lines[2])


class Coverage(unittest.TestCase):
    def test_parse_coverage(self):
        err = 'x group 0: scene bounds\nscenes: tried 444 loaded 443 failed 1 with-river 72\n'
        self.assertEqual(gap.parse_coverage(err),
                         {'tried': 444, 'loaded': 443, 'failed': 1, 'with-river': 72})
        with self.assertRaises(ValueError):
            gap.parse_coverage('nothing')


class Cache(unittest.TestCase):
    def test_cache_round_trip_keeps_scene_names_with_pipes_free_keys(self):
        import tempfile
        b = gap.parse_island_rows(row('D_256x256_0045_5B_g1_i2'))
        a = gap.parse_island_rows(row('D_256x256_0045_5B_g1_i2', gp=0))
        cov = {'tried': 3, 'loaded': 3, 'failed': 0, 'with-river': 1}
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'rows.json')
            gap.save_runs(path, b, a, cov)
            self.assertEqual(gap.load_runs(path), (b, a, cov))
            gap.save_runs(path, b, a)
            self.assertEqual(gap.load_runs(path), (b, a, None))


FAKE_EXE = r"""
import sys
patch = sys.argv[sys.argv.index('--gap-patch') + 1]
assert '--per-island' in sys.argv and sys.argv[-1].startswith('@')
files = open(sys.argv[-1][1:]).read().split()
extra = 100 if patch == '0' else 400
for f in files:
    print('| %s_g1_i0 (s=2) | 1000 | 990 | 7 | 0 | %d | 3 | 3 | 0 | 0 | 0 |' % (f, extra))
sys.stderr.write('scenes: tried %d loaded %d failed 0 with-river %d\n' % (len(files), len(files), len(files)))
"""


class Glue(unittest.TestCase):
    """run_per_island and gap_patch against a fake river_qa (a Python script run by the
    interpreter: argv[1] of the fake is the 'game dir' slot, which is the script path here)."""

    def setUp(self):
        import tempfile
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.fake = os.path.join(self.tmp.name, 'fake_river_qa.py')
        with open(self.fake, 'w') as f:
            f.write(FAKE_EXE)

    def quiet(self, args, files):
        import contextlib
        import io
        with contextlib.redirect_stdout(io.StringIO()):
            return gap.gap_patch(args, files, lambda d: ['S1', 'S2'])

    def test_run_per_island_passes_the_gap_patch_and_the_file_list(self):
        rows0, cov = gap.run_per_island(sys.executable, self.fake, ['S1', 'S2'], 0, '1')
        rows8, _ = gap.run_per_island(sys.executable, self.fake, ['S1', 'S2'], 8, '1')
        self.assertEqual(sorted(rows0), [('S1', 1, 0), ('S2', 1, 0)])
        self.assertEqual(rows0[('S1', 1, 0)]['extra'], 100)
        self.assertEqual(rows8[('S1', 1, 0)]['extra'], 400)
        self.assertEqual(cov, {'tried': 2, 'loaded': 2, 'failed': 0, 'with-river': 2})

    def test_gap_patch_writes_the_table_from_a_cache_and_from_a_run(self):
        import types
        table = os.path.join(self.tmp.name, 'table.md')
        cache = os.path.join(self.tmp.name, 'rows.json')
        args = types.SimpleNamespace(exe=sys.executable, game_dir=self.fake, patch='8', groups='1',
                                     climates='', table=table, sheets=None, worst=5, cache=cache)
        files = lambda d, c: []  # noqa: E731
        self.assertEqual(self.quiet(args, files), 0)
        self.assertTrue(os.path.isfile(cache))
        first = read_text(table)
        self.assertIn('scenes: tried 2, loaded 2, failed 0, with river pieces 2', first)
        self.assertIn('g1 water outside the painted bed, share of original river px: 10.0 % -> '
                      '40.0 %', first)
        os.unlink(self.fake)  # the second call must come from the cache
        self.assertEqual(self.quiet(args, files), 0)
        self.assertEqual(read_text(table), first)


class Tool(unittest.TestCase):
    def setUp(self):
        self.game = os.environ.get('ANNO1503_GAME_DIR')
        if not EXE or not self.game or not os.path.isfile(os.path.join(self.game, 'Anlagen.dat')):
            self.skipTest('needs --exe and ANNO1503_GAME_DIR')
        self.scene = os.path.join(self.game, 'Scenes', 'Islands', 'Dschungel',
                                  'D_256x256_0045_5B.scp')
        if not os.path.isfile(self.scene):
            self.skipTest('island not found')

    def run_qa(self, *args):
        return subprocess.run([EXE, self.game, *args, self.scene], capture_output=True, text=True)

    def test_river_qa_gap_patch_option_changes_the_measurement(self):
        out = {}
        for patch in (0, 8):
            res = self.run_qa('--groups', '2', '--per-island', '--gap-patch', str(patch))
            self.assertEqual(res.returncode, 0, res.stderr)
            out[patch] = gap.parse_island_rows(res.stdout)
            self.assertEqual(gap.parse_coverage(res.stderr),
                             {'tried': 1, 'loaded': 1, 'failed': 0, 'with-river': 1})
            lines = res.stdout.splitlines()
            self.assertEqual(lines[0].count('|'), lines[1].count('|'))  # header and separator
        key = ('D_256x256_0045_5B', 2, 0)
        self.assertIn(key, out[0])
        self.assertIn(key, out[8])
        self.assertEqual(out[0][key]['o'], out[8][key]['o'])  # the original sprites do not depend on it
        self.assertNotEqual(out[0][key]['extra'], out[8][key]['extra'])
        # edge px (the gap pixels with water close by) is the raw gap count minus the gap px: it
        # is not the gap px column, and the patch does not add any
        self.assertGreater(out[0][key]['edge'], 0)
        self.assertLessEqual(out[8][key]['edge'], out[0][key]['edge'])

    def test_gap_patch_values_that_are_not_numbers_ge_zero_are_refused(self):
        for bad in ('x', '-1', '8abc', ''):
            res = self.run_qa('--gap-patch', bad)
            self.assertEqual(res.returncode, 2, bad)

    def test_a_scene_that_fails_to_load_is_counted(self):
        res = subprocess.run([EXE, self.game, '--per-island', self.scene,
                              os.path.join(self.game, 'Scenes', 'Islands', 'nothere.scp')],
                             capture_output=True, text=True)
        self.assertEqual(gap.parse_coverage(res.stderr)['failed'], 1, res.stderr)


if __name__ == '__main__':
    if '--exe' in sys.argv:
        i = sys.argv.index('--exe')
        EXE = os.path.abspath(sys.argv[i + 1])
        del sys.argv[i:i + 2]
    unittest.main()
