#!/usr/bin/env python3
"""Hand-derived research checks; no engine, expected-output reader or game assets."""
import importlib.util
from pathlib import Path
import sys
import unittest

spec = importlib.util.spec_from_file_location('booking_ref', Path(__file__).parents[1] / 'booking_ref.py')
ref = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = ref
spec.loader.exec_module(ref)


class BookingChecks(unittest.TestCase):
    def run_booking(self, **kw):
        args = dict(tick=2000, stamp=0, rows=[(50, 0, 300), (70, 5, 100)])
        args.update(kw)
        return ref.book(**args)

    def test_prices(self):
        # (15000 + 7500 + 128) / 256 = 88.390625 -> 88.
        self.assertEqual(self.run_booking().income, 88)
        self.assertEqual(self.run_booking(rows=[(50, 0, 300), (70, 0, 100)]).income, 86)
        self.assertEqual(self.run_booking(rows=[(50, 0, 300), (70, 10, 100)]).income, 90)

    def test_gate_control_and_exact_boundary(self):
        for tick in (0, 1, 1999):
            r = self.run_booking(tick=tick, money=99, sales=105)
            self.assertFalse(r.stepped)
            self.assertEqual((r.stamp, r.billed, r.money, r.sales), (0, (300, 100), 99, 105))
        self.assertTrue(self.run_booking().stepped)

    def test_overdue_sets_now_and_does_not_book_twice(self):
        r = self.run_booking(tick=6001)
        self.assertEqual((r.stamp, r.income, r.billed), (6001, 88, (0, 0)))
        again = self.run_booking(tick=6001, stamp=r.stamp, rows=[(50, 0, 0)])
        self.assertFalse(again.stepped)

    def test_clock_word_wrap(self):
        self.assertFalse(self.run_booking(stamp=65000, tick=1463).stepped)
        self.assertEqual(self.run_booking(stamp=65000, tick=1464).stamp, 1464)

    def test_round_after_sum_not_each_good(self):
        r = self.run_booking(rows=[(1, 0, 128), (1, 0, 128)])
        self.assertEqual(r.income, 1)  # rounding both rows first would yield 2
        self.assertEqual(self.run_booking(rows=[(1, 0, 127)]).income, 0)
        self.assertEqual(self.run_booking(rows=[(1, 0, 128)]).income, 1)

    def test_upkeep_divides_combined_cost_once(self):
        r = self.run_booking(rows=[], money=0, civil=14, military=14)
        self.assertEqual((r.upkeep, r.money), (1, -1))
        self.assertEqual(self.run_booking(rows=[], civil=14).upkeep, 0)

    def test_statistics_decay_before_add(self):
        r = self.run_booking(sales=105, trade_sales=210, trade_purchases=104)
        self.assertEqual((r.sales, r.trade_sales, r.trade_purchases), (192, 208, 103))

    def test_original_word_prices_are_zero_extended(self):
        self.assertEqual(self.run_booking(rows=[(65535, 0, 1)]).income, 256)

    def test_original_overflow_signed_division(self):
        # 65535*65535 = 4294836225; +128 -> signed -130943.
        # truncate(-130943/256) = -511; arithmetic shift alone would give -512.
        self.assertEqual(self.run_booking(rows=[(65535, 0, 65535)]).income, -511)

    def test_original_balance_wrap(self):
        self.assertEqual(self.run_booking(rows=[(1, 0, 256)], money=2147483647).money, -2147483648)

    def test_original_statistics_product_wrap(self):
        # (2^32 - 1)*104 wraps to 2^32 - 104 = 4294967192.
        # 4294967192 = 105*40904449 + 47.
        r = self.run_booking(rows=[], sales=4294967295, trade_sales=4294967295,
                             trade_purchases=4294967295)
        self.assertEqual((r.sales, r.trade_sales, r.trade_purchases),
                         (40904449, 40904449, 40904449))

    def test_original_upkeep_sum_wrap(self):
        # (2^32 - 1) + 16 wraps to 15; divide only AFTER that wrap.
        r = self.run_booking(rows=[], civil=4294967295, military=16)
        self.assertEqual((r.upkeep, r.money), (1, -1))

    def test_inputs_unchanged_and_rejected_outside_domain(self):
        rows = [[50, 0, 300]]
        self.run_booking(rows=rows)
        self.assertEqual(rows, [[50, 0, 300]])
        for rows in ([[65536, 0, 1]], [[50, 0, -1]], [[50, 0]], [[True, 0, 1]]):
            with self.assertRaises(ValueError):
                self.run_booking(rows=rows)


if __name__ == '__main__':
    unittest.main()
