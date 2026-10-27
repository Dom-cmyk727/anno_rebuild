#!/usr/bin/env python3
"""Self-checks for tools/snapshot_catchup.py. No network and no game files."""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import snapshot_catchup as model


def cfg(**kwargs):
    return model.replace(model.proposed_budgets(), **kwargs)


class MeasuredFigures(unittest.TestCase):
    def test_fragment_counts_match_t41(self):
        for name, size in model.MEASURED_SAVES:
            self.assertEqual(model.fragment_count(size), model.PUBLISHED_FRAGMENTS[name])

    def test_fluid_tenths_match_t41_f1(self):
        for name, size in model.MEASURED_SAVES:
            fragments = model.fragment_count(size)
            for rtt, tenths in ((150, model.PUBLISHED_FLUID_TENTHS[(name, 150)]),
                                (300, model.PUBLISHED_FLUID_TENTHS[(name, 300)])):
                self.assertEqual(model.fluid_tenths(fragments, 32, rtt), tenths)

    def test_line_rate_matches_t41_f1(self):
        for name, size in model.MEASURED_SAVES:
            for bits, ms in ((100_000_000, model.PUBLISHED_LINE_MS[(name, 100_000_000)]),
                             (1_000_000_000, model.PUBLISHED_LINE_MS[(name, 1_000_000_000)])):
                self.assertEqual(model.line_rate_ms(size, bits), ms)

    def test_fragment_cap_is_the_measured_maximum_only(self):
        budgets = model.proposed_budgets()
        self.assertEqual(budgets.fragment_max, model.PUBLISHED_FRAGMENTS["endless02"])
        self.assertEqual(budgets.fragment_max * budgets.save_per_fragment, 20350440)
        self.assertEqual(4096 * 1188, 4866048)
        self.assertLess(4096 * 1188, dict(model.MEASURED_SAVES)["S01_07_01"])

    def test_discrete_milliseconds_for_the_measured_counts(self):
        self.assertEqual(model.discrete_zero_loss_ms(9659, 32, 300, 150), 90450)
        self.assertEqual(model.discrete_zero_loss_ms(17130, 32, 300, 150), 160650)
        self.assertEqual(model.discrete_zero_loss_ms(10865, 32, 300, 150), 101850)
        self.assertEqual(model.discrete_zero_loss_ms(17130, 32, 150, 75), 80325)
        self.assertEqual(model.discrete_zero_loss_ms(9659, 32, 150, 75), 45225)
        self.assertEqual(model.discrete_zero_loss_ms(10865, 32, 150, 75), 50925)

    def test_zero_loss_discrete_matches_the_closed_form(self):
        budgets = model.proposed_budgets()
        for fragments in (1, 32, 33, 64, 65):
            expected = model.discrete_zero_loss_ms(
                fragments, budgets.window, budgets.rtt_ms, budgets.one_way_ms)
            result = model.run_transfer(fragments, budgets)
            self.assertEqual(result.status, "complete")
            self.assertEqual(result.done_ms, expected)
            self.assertEqual(result.cursor, fragments)
            self.assertFalse(result.waited)


class SelectiveAck(unittest.TestCase):
    def test_ack_names_a_hole_the_contiguous_cursor_does_not_pass(self):
        have = {0, 2, 3}
        self.assertEqual(model.contiguous_cursor(have), 1)
        bits = model.ack_bits(3, have)
        named = model.sequences_named_by_ack(3, bits)
        self.assertEqual(named, {0, 2, 3})
        self.assertNotIn(1, named)

    def test_loss_is_resent_and_a_skip_is_not_success(self):
        budgets = cfg(window=4, max_attempts=4)

        def drop_first_of_one(seq, attempt):
            return seq == 1 and attempt == 0

        selective = model.run_transfer(8, budgets, drop=drop_first_of_one, policy="selective")
        self.assertEqual(selective.status, "complete")
        self.assertEqual(selective.cursor, 8)
        self.assertGreater(selective.sends, 8)
        skipped = model.run_transfer(8, budgets, drop=drop_first_of_one, policy="skip")
        self.assertNotEqual(skipped.status, "complete")
        self.assertLess(skipped.cursor, skipped.fragments)
        joined = model.evaluate_join(8 * 1188, budgets, transfer=skipped)
        self.assertEqual(joined.status, "restart")
        self.assertGreater(joined.missing, 0)
        self.assertFalse(joined.paused)
        self.assertTrue(joined.honest())

    def test_reorder_waits_for_the_contiguous_sequence(self):
        budgets = cfg()

        def delay_first(seq, attempt):
            del attempt
            return 80 if seq == 0 else 0

        result = model.run_transfer(4, budgets, reorder=delay_first)
        self.assertEqual(result.status, "complete")
        self.assertTrue(result.waited)
        self.assertEqual(result.cursor, 4)

    def test_a_larger_window_does_not_complete_when_holes_are_skipped(self):
        budgets = cfg(window=64)

        def drop_one(seq, attempt):
            return seq == 5 and attempt == 0

        wide_skip = model.run_transfer(10, budgets, drop=drop_one, policy="skip")
        self.assertNotEqual(wide_skip.status, "complete")
        wide = model.run_transfer(10, budgets, drop=drop_one, policy="selective")
        self.assertEqual(wide.status, "complete")
        self.assertLessEqual(wide.max_span, model.ACK_BITS)
        narrow = model.run_transfer(10, cfg(window=4), drop=drop_one, policy="selective")
        self.assertEqual(narrow.status, "complete")
        self.assertEqual(narrow.cursor, 10)
        self.assertLessEqual(narrow.max_span, model.ACK_BITS)

    def test_resend_faster_than_the_rtt_repeats_a_clean_window(self):
        quiet = model.run_transfer(32, cfg(resend_ms=300, one_way_ms=150))
        early = model.run_transfer(32, cfg(resend_ms=50, one_way_ms=150))
        self.assertEqual(quiet.status, "complete")
        self.assertEqual(early.status, "complete")
        self.assertEqual(quiet.sends, 32)
        self.assertGreater(early.sends, quiet.sends)

    def test_seeded_loss_is_reproducible_and_complete(self):
        budgets = cfg(loss_per_million=20_000, seed=1)
        first = model.run_transfer(200, budgets)
        second = model.run_transfer(200, budgets)
        self.assertEqual(first.status, "complete")
        self.assertEqual((first.done_ms, first.sends, first.cursor),
                         (second.done_ms, second.sends, second.cursor))
        self.assertGreater(first.sends, 200)
        self.assertEqual(first.cursor, 200)


class Retention(unittest.TestCase):
    def test_periodic_snapshot_is_refused_without_a_pause(self):
        budgets = model.proposed_budgets()
        age = model.periodic_age_turns(budgets)
        self.assertEqual(age, 6000)
        result = model.evaluate_join(0, budgets, mode="periodic", age_turns=age)
        self.assertEqual(result.status, "refuse")
        self.assertEqual(result.reason, "gap_not_retained")
        self.assertEqual(result.missing, 5400)
        self.assertFalse(result.paused)
        self.assertTrue(result.honest())

    def test_fresh_snapshot_longer_than_60s_keeps_the_tail(self):
        budgets = model.proposed_budgets()
        size = dict(model.MEASURED_SAVES)["endless02"]
        pinned = model.evaluate_join(size, budgets, mode="pinned")
        rolling = model.evaluate_join(size, budgets, mode="rolling")
        self.assertGreater(pinned.transfer_ms, 60_000)
        self.assertEqual(pinned.status, "success")
        self.assertEqual(pinned.missing, 0)
        self.assertGreater(pinned.pin_turns, budgets.turn_buffer)
        self.assertFalse(pinned.paused)
        self.assertTrue(pinned.honest())
        self.assertEqual(rolling.status, "refuse")
        self.assertEqual(rolling.reason, "gap_not_retained")
        self.assertFalse(rolling.paused)
        host = pinned.snapshot_turn + pinned.pin_turns
        generation = model.Generation(pinned.snapshot_turn, 0, size)
        self.assertEqual(generation.attach("a", 0, budgets), "opened")
        self.assertFalse(model.rolling_contains(1, 0, host, budgets))
        self.assertTrue(generation.retained(1, host, budgets))

    def test_all_three_measured_saves_at_the_acceptance_rtt(self):
        budgets = model.proposed_budgets()
        for name, size in model.MEASURED_SAVES:
            pinned = model.evaluate_join(size, budgets, mode="pinned")
            rolling = model.evaluate_join(size, budgets, mode="rolling")
            self.assertEqual(pinned.status, "success", name)
            self.assertGreater(pinned.transfer_ms, budgets.turn_buffer * budgets.turn_ms, name)
            self.assertEqual(pinned.missing, 0, name)
            self.assertTrue(pinned.honest(), name)
            self.assertEqual(rolling.status, "refuse", name)
            self.assertFalse(rolling.paused, name)
            decision, reason, _need = model.admit(size, budgets)
            self.assertEqual((decision, reason), ("admit", "within_budgets"), name)

    def test_two_percent_loss_on_the_measured_saves_stays_inside_the_pin(self):
        budgets = cfg(loss_per_million=20_000, seed=1)
        for name, size in model.MEASURED_SAVES:
            result = model.evaluate_join(size, budgets)
            self.assertEqual(result.status, "success", name)
            self.assertEqual(result.missing, 0, name)
            self.assertEqual(
                (result.transfer_ms, result.done_ms, result.pin_turns),
                {"S01_07_01": (113250, 125830, 1258),
                 "endless02": (198750, 220830, 2208),
                 "03_1024_med_256": (127050, 141170, 1412)}[name],
                name)
            self.assertLessEqual(result.pin_turns, budgets.tail_turns_max, name)
            self.assertFalse(result.paused, name)
            self.assertTrue(result.honest(), name)

    def test_catchup_can_and_cannot_outrun_live_turns(self):
        fast = model.evaluate_join(1188, cfg(replay_turns_per_s=100))
        self.assertEqual(fast.status, "success")
        self.assertEqual(fast.missing, 0)
        self.assertGreater(fast.pin_turns, 0)
        slow = model.evaluate_join(
            1188, cfg(replay_turns_per_s=5, tail_turns_max=1_000_000, catchup_ms=60_000))
        self.assertEqual(slow.status, "restart")
        self.assertEqual(slow.reason, "cannot_outrun")
        self.assertGreater(slow.missing, 0)
        self.assertFalse(slow.paused)
        self.assertTrue(slow.honest())

    def test_synthetic_sizes_trip_a_bound_and_do_not_pause(self):
        budgets = model.proposed_budgets()
        for name, size in model.SYNTHETIC_SAVES:
            decision, reason, _need = model.admit(size, budgets)
            self.assertEqual(decision, "refuse", name)
            self.assertEqual(reason, "fragment_cap", name)
            result = model.evaluate_join(size, budgets)
            self.assertEqual(result.status, "refuse", name)
            self.assertFalse(result.paused, name)
            self.assertTrue(result.honest(), name)
        doubled = dict(model.SYNTHETIC_SAVES)["synthetic_2x_endless02"]
        fragments = model.fragment_count(doubled)
        raised = cfg(fragment_max=fragments)
        decision, reason, need = model.admit(doubled, raised)
        self.assertEqual((decision, reason, need), ("refuse", "tail_turns", 7137))
        wider = cfg(fragment_max=fragments, tail_turns_max=8192)
        decision, reason, need = model.admit(doubled, wider)
        self.assertEqual((decision, reason, need), ("refuse", "catchup_ms", 7137))
        quad = dict(model.SYNTHETIC_SAVES)["synthetic_4x_endless02"]
        self.assertEqual(model.fragment_count(quad), 68517)
        self.assertEqual(model.tail_turns_needed(model.fragment_count(quad),
                                                 cfg(fragment_max=model.fragment_count(quad))),
                         14277)

    def test_tail_cap_restarts_a_permitted_join(self):
        # Admitted under the proposed cap, then the same bytes are evaluated
        # against a cap of one turn: the pin cannot be honoured, so the joiner
        # restarts. The result is not a success with a missing turn.
        budgets = cfg(tail_turns_max=1)
        result = model.evaluate_join(1188, budgets)
        self.assertEqual(result.status, "restart")
        self.assertEqual(result.reason, "tail_turns")
        self.assertFalse(result.paused)
        self.assertTrue(result.honest())
        self.assertNotEqual(result.status, "success")

    def test_window_serialization_stays_under_the_one_way_delay(self):
        budgets = model.proposed_budgets()
        lan = model.window_serialize_us(budgets.window, budgets.payload_max, 100_000_000)
        fast = model.window_serialize_us(budgets.window, budgets.payload_max, 1_000_000_000)
        self.assertEqual(lan, 3072)
        self.assertEqual(fast, 307)
        self.assertLess(lan, budgets.one_way_ms * 1000)


class Ownership(unittest.TestCase):
    def test_two_joiners_share_cancel_and_migrate_without_pausing(self):
        budgets = model.proposed_budgets()
        size = 1188 * 4
        generation = model.Generation(0, 0, size)
        self.assertEqual(generation.attach("a", 0, budgets), "opened")
        self.assertEqual(generation.attach("b", 1000, budgets), "shared")
        self.assertEqual(generation.attach("c", 1000, budgets), "refuse")
        self.assertEqual(generation.running(), 2)
        late = model.Generation(0, 0, size)
        late.attach("a", 0, budgets)
        self.assertEqual(late.attach("b", budgets.snapshot_age_ms, budgets), "shared")
        self.assertEqual(late.attach("d", budgets.snapshot_age_ms + 1, budgets), "fresh_snapshot")

        host = budgets.turn_buffer + 1
        self.assertFalse(model.rolling_contains(1, 0, host, budgets))
        self.assertTrue(generation.retained(1, host, budgets))
        generation.cancel("b")
        self.assertEqual(generation.running(), 1)
        self.assertTrue(generation.retained(1, host, budgets))
        both = model.evaluate_join(size, budgets)
        other = model.evaluate_join(size, budgets)
        self.assertEqual(both.status, "success")
        self.assertEqual(other.status, "success")
        self.assertFalse(both.paused or other.paused)
        generation.complete("a")
        self.assertEqual(generation.running(), 0)
        self.assertFalse(generation.retained(1, host, budgets))

        restarted = generation.migrate()
        self.assertEqual(restarted, [])
        generation.attach("e", 0, budgets)
        self.assertEqual(generation.migrate(), ["e"])
        self.assertEqual(generation.running(), 0)
        self.assertFalse(generation.pin_holds(1, host))

        rounded = generation.rounded(budgets)
        peak = model.actual_allocation(1, 2, host, budgets, rounded)
        self.assertLessEqual(peak, model.allocation_bound(budgets)["total"])
        self.assertGreater(model.allocation_bound(budgets)["total"],
                           budgets.fragment_max)

    def test_mismatch_resync_may_pause_and_has_an_empty_tail(self):
        resync = model.mismatch_resync(40)
        self.assertTrue(resync["paused"])
        self.assertEqual(resync["snapshot_turn"], 40)
        self.assertEqual(resync["first_retained_turn"], 41)
        self.assertEqual(resync["tail_turns"], 0)


class ProposalText(unittest.TestCase):
    def test_adr_stays_proposed_and_quotes_the_model(self):
        root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        with open(os.path.join(root, "docs", "adr", "0003-network-transport.md"),
                  encoding="utf-8") as adr_file:
            adr = adr_file.read()
        with open(os.path.join(root, "docs", "open-questions.md"),
                  encoding="utf-8") as oq_file:
            oq = oq_file.read()
        self.assertIn("**Status:** Proposed", adr)
        self.assertNotIn("**Status:** Accepted", adr)
        for token in (
                "tail_turns_max=4096",
                "tail_bytes_max=4915200",
                "allocation_bound_bytes=87113760",
                "endless02_discrete_zero_loss_ms=160650",
                "endless02_loss2_seed1_transfer_ms=198750",
                "endless02_loss2_seed1_done_ms=220830",
                "endless02_loss2_seed1_pin_turns=2208",
                "tools/snapshot_catchup.py",
                "80.3 s",
                "160.6 s",
                ):
            self.assertIn(token, adr)
        self.assertIn("narrowed (T43, 2026-10-27)", oq)
        self.assertIn("## OQ-281:", oq)
        self.assertIn("## OQ-282:", oq)
        self.assertNotIn("## OQ-283:", oq)


class Proposal(unittest.TestCase):
    def test_tail_cap_is_the_power_of_two_above_the_largest_save(self):
        budgets = model.proposed_budgets()
        needs = [model.tail_turns_needed(model.PUBLISHED_FRAGMENTS[name], budgets)
                 for name, _size in model.MEASURED_SAVES]
        self.assertEqual(needs, [2010, 3570, 2264])
        need = needs[1]
        self.assertEqual(budgets.tail_turns_max, model.next_pow2(need))
        self.assertGreaterEqual(budgets.tail_turns_max, need)
        for name, _size in model.MEASURED_SAVES:
            smaller = model.tail_turns_needed(model.PUBLISHED_FRAGMENTS[name], budgets)
            self.assertLessEqual(smaller, budgets.tail_turns_max)

    def test_turn_wire_does_not_outlast_the_measured_snapshots(self):
        budgets = model.proposed_budgets()
        for _name, size in model.MEASURED_SAVES:
            fragments = model.fragment_count(size)
            snapshot_ms = model.discrete_zero_loss_ms(
                fragments, budgets.window, budgets.rtt_ms, budgets.one_way_ms)
            turns = snapshot_ms // budgets.turn_ms
            self.assertLess(model.turn_fluid_ms(turns, budgets), snapshot_ms)

    def test_bound_covers_more_than_one_fragment_count(self):
        budgets = model.proposed_budgets()
        bound = model.allocation_bound(budgets)
        one = budgets.fragment_max * budgets.save_per_fragment
        self.assertGreater(bound["snapshot_store"], one)
        self.assertGreater(bound["reassembly"], one)
        self.assertEqual(bound["total"], sum(value for key, value in bound.items()
                                             if key != "total"))
        self.assertEqual(bound["pinned_turns"], budgets.tail_turns_max * budgets.turn_store_cap)
        self.assertEqual(bound["rolling_turns"], budgets.turn_buffer * budgets.turn_store_cap)


if __name__ == "__main__":
    unittest.main()
