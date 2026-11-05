#!/usr/bin/env python3
"""Independent hand-vector oracle for SCRIPT1 (not the pure branch helper).

AST tuples below deliberately spell the original right grouping. This oracle
counts reads, evaluates left once, and uses Python unbounded arithmetic with an
explicit signed wrap. It is not an implementation-generated golden.
"""
import unittest


def signed(x):
    return (x + 2**31) % 2**32 - 2**31


def schedule(arg, count):
    n = signed(signed(signed(arg) * count) + 125)
    q = abs(n) // 250 * (-1 if n < 0 else 1)
    return count if q < 0 or q > count else q


def evaluate(node, values, reads):
    if isinstance(node, str):
        reads.append(node)
        return bool(values[node]), False
    op, left, right = node
    a, _ = evaluate(left, values, reads)
    if op == 'and' and not a:
        return False, True
    b, _ = evaluate(right, values, reads)
    return (a and b if op == 'and' else a or b), False


def branch(node, values, reads):
    truth, suppressed = evaluate(node, values, reads)
    return None if suppressed else ('then' if truth else 'else')


class Contracts(unittest.TestCase):
    def test_schedule_hand_vectors(self):
        self.assertEqual([schedule(t, 4) for t in (0, 1, 31, 32, 94, 157, 219, 250)],
                         [0, 0, 0, 1, 2, 3, 4, 4])
        self.assertEqual(schedule(-126, 1), 0)
        self.assertEqual(schedule(-375, 1), 1)
        self.assertEqual(schedule(1073741793, 4), 0)

    def test_grouping_and_suppression(self):
        reads = []
        self.assertIsNone(branch(('and', 'A', ('or', 'B', 'C')),
                                 dict(A=False, B=True, C=True), reads))
        self.assertEqual(reads, ['A'])
        # C precedence would select THEN. A false primary selects ELSE.
        self.assertEqual(branch('A', dict(A=False), []), 'else')
        # An inner suppression is a false value, not root suppression.
        self.assertEqual(branch(('or', 'A', ('and', 'B', 'C')),
                                dict(A=False, B=False, C=True), []), 'else')

    def test_or_reads_right_and_and_reads_left_once(self):
        for op in ('or', 'and'):
            reads = []
            self.assertEqual(branch((op, 'A', 'B'), dict(A=True, B=True), reads), 'then')
            self.assertEqual(reads, ['A', 'B'])

    def test_native_cursor_and_init_barrier(self):
        # Hand transcription: INIT sets x=true; IF0 clears it; IF1 reads it.
        state = dict(started=False, cursor=0, x=False, seen=None)
        def visit(arg):
            if not state['started']:
                state.update(started=True, x=True)
            for _ in range(schedule(arg, 2)):
                if state['cursor'] == 0:
                    state['x'] = False
                else:
                    state['seen'] = state['x']
                state['cursor'] = (state['cursor'] + 1) % 2
        visit(0)
        self.assertTrue(state['x'])
        visit(63)  # (63*2+125)/250 = 1
        saved = dict(state)
        self.assertEqual(saved['cursor'], 1)
        visit(63)
        self.assertFalse(state['seen'])
        self.assertEqual(state['cursor'], 0)
        self.assertFalse(state['x'])  # no repeated INIT
        resumed = dict(saved)
        resumed.update(seen=resumed['x'], cursor=0)
        self.assertEqual(state, resumed)


if __name__ == '__main__':
    unittest.main()
