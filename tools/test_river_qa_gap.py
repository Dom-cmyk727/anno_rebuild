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


class Cache(unittest.TestCase):
    def test_cache_round_trip_keeps_scene_names_with_pipes_free_keys(self):
        import tempfile
        b = gap.parse_island_rows(row('D_256x256_0045_5B_g1_i2'))
        a = gap.parse_island_rows(row('D_256x256_0045_5B_g1_i2', gp=0))
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'rows.json')
            gap.save_runs(path, b, a)
            self.assertEqual(gap.load_runs(path), (b, a))


class Tool(unittest.TestCase):
    def test_river_qa_gap_patch_option_changes_the_measurement(self):
        game = os.environ.get('ANNO1503_GAME_DIR')
        if not EXE or not game or not os.path.isfile(os.path.join(game, 'Anlagen.dat')):
            self.skipTest('needs --exe and ANNO1503_GAME_DIR')
        scene = os.path.join(game, 'Scenes', 'Islands', 'Dschungel', 'D_256x256_0045_5B.scp')
        if not os.path.isfile(scene):
            self.skipTest('island not found')
        out = {}
        for patch in (0, 8):
            res = subprocess.run([EXE, game, '--groups', '2', '--per-island', '--gap-patch',
                                  str(patch), scene], capture_output=True, text=True)
            self.assertEqual(res.returncode, 0, res.stderr)
            out[patch] = gap.parse_island_rows(res.stdout)
        key = ('D_256x256_0045_5B', 2, 0)
        self.assertIn(key, out[0])
        self.assertIn(key, out[8])
        self.assertEqual(out[0][key]['o'], out[8][key]['o'])  # the original sprites do not depend on it
        self.assertNotEqual(out[0][key]['extra'], out[8][key]['extra'])
        res = subprocess.run([EXE, game, '--gap-patch', 'x', scene], capture_output=True, text=True)
        self.assertEqual(res.returncode, 2)


if __name__ == '__main__':
    if '--exe' in sys.argv:
        i = sys.argv.index('--exe')
        EXE = sys.argv[i + 1]
        del sys.argv[i:i + 2]
    unittest.main()
