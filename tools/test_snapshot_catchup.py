#!/usr/bin/env python3
"""Self-checks for tools/snapshot_catchup.py. No network and no game files."""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import snapshot_catchup as model


def cfg(**kwargs):
    return model.replace(model.proposed_budgets(), **kwargs)


def assert_within_caps(test, table):
    mem = table.memory_bytes()
    bound = model.allocation_bound(table.cfg)
    test.assertEqual(mem["total"], sum(value for key, value in mem.items() if key != "total"))
    test.assertEqual(table.over_cap(), [])
    for key, cap in bound.items():
        test.assertLessEqual(mem[key], cap, key)
    test.assertLessEqual(len(table.distinct_turns()), table.cfg.tail_turns_max)
    test.assertLessEqual(mem["pinned_turns"], table.cfg.tail_bytes_max)
    test.assertFalse(table.paused)


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
        self.assertEqual(first.acks_lost, 0)

    def test_ack_loss_recovers_from_a_duplicate_and_refuses_when_it_cannot(self):
        budgets = cfg(window=4, max_attempts=4, resend_ms=300)

        def drop_first(highest, duplicate):
            del highest
            return not duplicate

        recovered = model.run_transfer(
            4, budgets, ack_drop=drop_first, ack_duplicates=True)
        self.assertEqual(recovered.status, "complete")
        self.assertTrue(recovered.sender_released)
        self.assertGreater(recovered.acks_lost, 0)
        self.assertGreater(recovered.duplicate_deliveries, 0)
        self.assertGreater(recovered.acks_sent, 0)

        def drop_all(highest, duplicate):
            del highest, duplicate
            return True

        lost = model.run_transfer(4, budgets, ack_drop=drop_all, ack_duplicates=True)
        self.assertEqual(lost.status, "unacked")
        self.assertFalse(lost.sender_released)
        self.assertGreater(lost.acks_lost, 0)
        self.assertEqual(lost.cursor, lost.fragments)
        joined = model.evaluate_join(4 * 1188, budgets, transfer=lost)
        self.assertEqual(joined.status, "restart")
        self.assertEqual(joined.reason, "unacked")
        self.assertFalse(joined.paused)
        self.assertNotEqual(joined.status, "success")

    def test_the_two_percent_path_drops_data_only(self):
        text = " ".join(model.assumptions())
        self.assertIn("drops data datagrams only", text)
        self.assertIn("does not drop acknowledgements", text)
        self.assertIn("duplicate arrival does not generate", text)
        quiet = model.run_transfer(8, cfg(resend_ms=50, one_way_ms=150))
        self.assertGreater(quiet.duplicate_deliveries, 0)
        self.assertEqual(quiet.acks_lost, 0)
        named = model.run_transfer(
            8, cfg(resend_ms=50, one_way_ms=150), ack_duplicates=True)
        self.assertGreater(named.acks_sent, quiet.acks_sent)


def assert_joined_clock(test, result, turn_ms):
    """Host cursor from the completion clock, not from the reserved bound."""
    test.assertEqual(result.status, "success")
    test.assertEqual(result.missing, 0)
    test.assertFalse(result.paused)
    test.assertEqual(result.host_produced, result.snapshot_turn + result.done_ms // turn_ms)
    test.assertEqual(result.replayed, result.host_produced)
    test.assertGreaterEqual(result.received, result.replayed)
    test.assertLessEqual(result.replayed, result.host_produced)
    test.assertEqual(result.pin_turns, result.replayed - result.snapshot_turn)
    test.assertGreaterEqual(result.reserved_turns, result.pin_turns)
    test.assertTrue(result.honest(turn_ms))


class Retention(unittest.TestCase):
    def test_periodic_snapshot_is_not_a_join(self):
        budgets = model.proposed_budgets()
        age = model.periodic_age_turns(budgets)
        self.assertEqual(age, 6000)
        result = model.evaluate_join(0, budgets, mode="periodic", age_turns=age)
        self.assertEqual(result.status, "unavailable")
        self.assertEqual(result.reason, "initial_availability")
        self.assertNotEqual(result.status, "success")
        self.assertEqual(result.missing, 5400)
        self.assertEqual(result.transfer_ms, 0)
        self.assertEqual(result.replayed, result.snapshot_turn)
        self.assertFalse(result.paused)
        self.assertFalse(result.honest())

    def test_fresh_snapshot_longer_than_60s_keeps_the_tail(self):
        budgets = model.proposed_budgets()
        size = dict(model.MEASURED_SAVES)["endless02"]
        pinned = model.evaluate_join(size, budgets, mode="pinned")
        rolling = model.evaluate_join(size, budgets, mode="rolling")
        self.assertGreater(pinned.transfer_ms, 60_000)
        assert_joined_clock(self, pinned, budgets.turn_ms)
        self.assertGreater(pinned.pin_turns, budgets.turn_buffer)
        self.assertEqual(pinned.backlog_turns, pinned.transfer_ms // budgets.turn_ms)
        self.assertEqual(rolling.status, "refuse")
        self.assertEqual(rolling.reason, "gap_not_retained")
        self.assertGreater(rolling.transfer_ms, 0)
        self.assertGreater(rolling.missing, 0)
        self.assertFalse(rolling.paused)
        self.assertTrue(rolling.honest(budgets.turn_ms))
        host = pinned.host_produced
        table = model.Table(budgets)
        self.assertEqual(table.advance(0, host), [])
        self.assertEqual(table.attach("a", 0, size, pinned.snapshot_turn, host)[0], "opened")
        self.assertFalse(model.rolling_contains(1, 0, host, budgets))
        self.assertTrue(table.retained(1))
        assert_within_caps(self, table)

    def test_all_three_measured_saves_at_the_acceptance_rtt(self):
        budgets = model.proposed_budgets()
        for name, size in model.MEASURED_SAVES:
            pinned = model.evaluate_join(size, budgets, mode="pinned")
            rolling = model.evaluate_join(size, budgets, mode="rolling")
            assert_joined_clock(self, pinned, budgets.turn_ms)
            self.assertGreater(pinned.transfer_ms, budgets.turn_buffer * budgets.turn_ms, name)
            self.assertEqual(rolling.status, "refuse", name)
            self.assertFalse(rolling.paused, name)
            decision, reason, _need = model.admit(size, budgets)
            self.assertEqual((decision, reason), ("admit", "within_budgets"), name)

    def test_two_percent_loss_on_the_measured_saves_stays_inside_the_pin(self):
        budgets = cfg(loss_per_million=20_000, seed=1)
        for name, size in model.MEASURED_SAVES:
            result = model.evaluate_join(size, budgets)
            assert_joined_clock(self, result, budgets.turn_ms)
            self.assertEqual(
                (result.transfer_ms, result.done_ms, result.pin_turns),
                {"S01_07_01": (113250, 125830, 1258),
                 "endless02": (198750, 220830, 2208),
                 "03_1024_med_256": (127050, 141160, 1411)}[name],
                name)
            self.assertEqual(result.replayed, result.done_ms // budgets.turn_ms, name)
            self.assertLessEqual(result.pin_turns, budgets.tail_turns_max, name)
            self.assertLessEqual(result.backlog_turns, budgets.receive_backlog_turns, name)
        # The published loss row for 03_1024 applied turn 1412 at 141170 ms.
        # 141170 // 100 is 1411, so that turn was ahead of the host.
        self.assertEqual(141170 // budgets.turn_ms, 1411)

    def test_catchup_can_and_cannot_outrun_live_turns(self):
        fast = model.evaluate_join(1188, cfg(replay_turns_per_s=100))
        assert_joined_clock(self, fast, 100)
        self.assertGreater(fast.pin_turns, 0)
        # The rejected reading applied turn 2 at 170 ms. The host has produced
        # one turn by then: 170 // 100 == 1.
        self.assertEqual(170 // 100, 1)
        self.assertEqual((fast.done_ms, fast.replayed, fast.host_produced), (160, 1, 1))
        self.assertEqual(fast.reserved_turns, 2)
        self.assertNotEqual((fast.done_ms, fast.replayed), (170, 2))
        slow = model.evaluate_join(
            1188, cfg(replay_turns_per_s=5, tail_turns_max=1_000_000, catchup_ms=60_000))
        self.assertEqual(slow.status, "restart")
        self.assertEqual(slow.reason, "cannot_outrun")
        self.assertGreater(slow.missing, 0)
        self.assertFalse(slow.paused)
        self.assertTrue(slow.honest())
        self.assertLessEqual(slow.replayed, slow.host_produced)

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
        # The one-fragment join replays one turn. A cap of one holds it.
        # A cap of zero cannot, so that joiner restarts. The table is not paused.
        held = model.evaluate_join(1188, cfg(tail_turns_max=1))
        assert_joined_clock(self, held, 100)
        self.assertEqual(held.pin_turns, 1)
        budgets = cfg(tail_turns_max=0)
        result = model.evaluate_join(1188, budgets)
        self.assertEqual(result.status, "restart")
        self.assertEqual(result.reason, "tail_turns")
        self.assertFalse(result.paused)
        self.assertTrue(result.honest())
        self.assertLessEqual(result.replayed, result.host_produced)
        self.assertNotEqual(result.status, "success")

    def test_a_599_turn_old_save_is_not_a_join_and_expires_during_transfer(self):
        budgets = model.proposed_budgets()
        size = dict(model.MEASURED_SAVES)["endless02"]
        self.assertEqual(size, 20349454)
        avail = model.evaluate_join(size, budgets, mode="periodic", age_turns=599)
        self.assertEqual(avail.status, "available")
        self.assertEqual(avail.reason, "initial_availability")
        self.assertNotEqual(avail.status, "success")
        self.assertEqual(avail.transfer_ms, 0)
        self.assertEqual(avail.done_ms, 0)
        self.assertEqual(avail.replayed, avail.snapshot_turn)
        rolled = model.evaluate_join(size, budgets, mode="rolling", age_turns=599)
        self.assertEqual(rolled.status, "refuse")
        self.assertEqual(rolled.reason, "gap_not_retained")
        self.assertEqual(rolled.transfer_ms, 160650)
        self.assertEqual(rolled.missing, 1605)
        self.assertEqual(rolled.host_produced, 599 + 160650 // budgets.turn_ms)
        self.assertFalse(rolled.paused)
        self.assertTrue(rolled.honest(budgets.turn_ms))

    def test_rolling_tail_expires_when_load_runs_past_the_log(self):
        budgets = cfg(load_ms=2000)
        result = model.evaluate_join(1188, budgets, mode="rolling", age_turns=590)
        self.assertEqual(result.status, "refuse")
        self.assertEqual(result.reason, "gap_not_retained")
        self.assertEqual(result.transfer_ms, 150)
        self.assertGreater(result.done_ms, result.transfer_ms)
        self.assertGreater(result.missing, 0)
        self.assertFalse(result.paused)

    def test_tick_boundaries_match_the_host_clock(self):
        budgets = model.proposed_budgets()

        def at(ms):
            transfer = model.TransferResult("complete", ms, 1, 1, 1, False, 0, 0)
            return model.evaluate_join(1188, budgets, transfer=transfer)

        before = at(99)
        assert_joined_clock(self, before, budgets.turn_ms)
        self.assertEqual((before.done_ms, before.replayed), (99, 0))
        opened = at(100)
        assert_joined_clock(self, opened, budgets.turn_ms)
        self.assertEqual((opened.done_ms, opened.replayed), (110, 1))
        boundary = at(200)
        assert_joined_clock(self, boundary, budgets.turn_ms)
        self.assertEqual((boundary.done_ms, boundary.replayed), (220, 2))
        self.assertEqual(169 // budgets.turn_ms, 1)

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
        table = model.Table(budgets)
        self.assertEqual(table.attach("a", 0, size, 0, 0)[0], "opened")
        self.assertEqual(table.advance(1000, 0), [])
        self.assertEqual(table.attach("b", 1000, size, 0, 0)[0], "shared")
        self.assertEqual(table.attach("c", 1000, size, 0, 0)[0], "refuse")
        self.assertEqual(table.running(), 2)
        self.assertEqual(table.advance(budgets.snapshot_age_ms, 0), [])
        self.assertEqual(table.attach("b2", budgets.snapshot_age_ms, size, 0, 0)[0], "refuse")

        host = budgets.turn_buffer + 1
        self.assertEqual(table.advance(budgets.snapshot_age_ms, host), [])
        self.assertFalse(model.rolling_contains(1, 0, host, budgets))
        self.assertTrue(table.retained(1))
        table.cancel("b")
        self.assertEqual(table.running(), 1)
        self.assertTrue(table.retained(1))
        both = model.evaluate_join(size, budgets)
        other = model.evaluate_join(size, budgets)
        assert_joined_clock(self, both, budgets.turn_ms)
        assert_joined_clock(self, other, budgets.turn_ms)
        self.assertFalse(both.paused or other.paused)
        table.complete("a")
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.retained(1))

        self.assertEqual(table.migrate(), [])
        self.assertEqual(table.attach("e", table.now_ms, size, 0, table.host_turn)[0], "opened")
        self.assertEqual(table.migrate(), ["e"])
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.retained(1))
        assert_within_caps(self, table)

        fresh = model.Table(budgets)
        self.assertEqual(fresh.attach("a", 0, size, 0, 0)[0], "opened")
        self.assertEqual(fresh.advance(budgets.snapshot_age_ms + 1, 40), [])
        opened, gen = fresh.attach("d", budgets.snapshot_age_ms + 1, size, 40, 40)
        self.assertEqual(opened, "opened")
        self.assertEqual(gen.snapshot_turn, 40)
        self.assertEqual(fresh.running(), 2)
        assert_within_caps(self, fresh)

    def test_three_joiners_two_generations_use_one_global_cap(self):
        budgets = cfg(concurrent_transfers=2, max_generations=2)
        size = 1188 * 4
        table = model.Table(budgets)
        word, first = table.attach("a", 0, size, 0, 0)
        self.assertEqual(word, "opened")
        self.assertEqual(table.advance(budgets.snapshot_age_ms + 1, 0), [])
        word, second = table.attach("b", budgets.snapshot_age_ms + 1, size, 0, 0)
        self.assertEqual(word, "opened")
        self.assertIsNot(first, second)
        self.assertEqual(first.running(), 1)
        self.assertEqual(second.running(), 1)
        # Each generation has a free local slot. The global cap of two refuses.
        self.assertEqual(table.attach("c", budgets.snapshot_age_ms + 1, size, 0, 0)[0],
                         "refuse")
        self.assertEqual(table.running(), 2)
        host = budgets.turn_buffer + 40
        self.assertEqual(table.advance(budgets.snapshot_age_ms + 1, host), [])
        self.assertFalse(model.rolling_contains(1, 0, host, budgets))
        self.assertTrue(table.retained(1))
        table.cancel("b")
        self.assertEqual(table.running(), 1)
        self.assertTrue(table.retained(1))
        word, _third = table.attach("c", budgets.snapshot_age_ms + 1, size, 80, host)
        self.assertEqual(word, "opened")
        self.assertTrue(table.retained(1))
        self.assertEqual(table.running(), 2)
        table.complete("a")
        self.assertFalse(table.retained(1))
        self.assertEqual(table.running(), 1)
        self.assertEqual(table.migrate(), ["c"])
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.retained(81))
        assert_within_caps(self, table)

    def test_a_new_generation_does_not_evict_a_tail_that_still_fits(self):
        budgets = cfg(tail_turns_max=10, concurrent_transfers=2, max_generations=2)
        table = model.Table(budgets)
        self.assertEqual(table.advance(0, 110), [])
        self.assertEqual(table.attach("a", 0, 1188, 100, 110)[0], "opened")
        # The first image is too old to share, so this ask would open a second
        # generation. Its span does not fit, and A's turns stay.
        self.assertEqual(table.advance(budgets.snapshot_age_ms + 1, 110), [])
        self.assertEqual(
            table.attach("b", budgets.snapshot_age_ms + 1, 1188, 0, 110)[0], "tail")
        self.assertTrue(table.retained(101))
        self.assertEqual(table.running(), 1)
        assert_within_caps(self, table)

    def test_joiner_backlog_is_capped_without_pausing_the_other(self):
        budgets = model.proposed_budgets()
        table = model.Table(budgets)
        self.assertEqual(table.attach("a", 0, 1188, 0, 0)[0], "opened")
        self.assertEqual(table.advance(1000, 0), [])
        self.assertEqual(table.attach("b", 1000, 1188, 0, 0)[0], "shared")
        self.assertEqual(table.note_backlog("a", 10), "held")
        self.assertEqual(table.note_backlog("b", budgets.receive_backlog_turns + 1), "restart")
        self.assertEqual(table.running(), 1)
        self.assertEqual(table.backlog["a"], 10)
        mem = table.memory_bytes()
        self.assertEqual(mem["joiner_backlog"], 10 * budgets.turn_store_cap)
        self.assertEqual(mem["total"], sum(value for key, value in mem.items() if key != "total"))
        self.assertLessEqual(mem["total"], model.allocation_bound(budgets)["total"])
        assert_within_caps(self, table)
        over = model.evaluate_join(1188, cfg(receive_backlog_turns=0))
        self.assertEqual(over.status, "restart")
        self.assertEqual(over.reason, "backlog")
        self.assertFalse(over.paused)
        self.assertNotEqual(over.status, "success")
        decision, reason, detail = model.admit(
            dict(model.MEASURED_SAVES)["endless02"], cfg(receive_backlog_turns=1000))
        self.assertEqual((decision, reason), ("refuse", "backlog"))
        self.assertGreater(detail, 1000)

    def test_mismatch_resync_may_pause_and_has_an_empty_tail(self):
        resync = model.mismatch_resync(40)
        self.assertTrue(resync["paused"])
        self.assertEqual(resync["snapshot_turn"], 40)
        self.assertEqual(resync["first_retained_turn"], 41)
        self.assertEqual(resync["tail_turns"], 0)

    def test_one_join_holds_4096_and_restarts_before_4097(self):
        budgets = model.proposed_budgets()
        self.assertEqual(budgets.tail_turns_max, 4096)
        self.assertEqual(budgets.tail_bytes_max, 4_915_200)
        self.assertEqual(4097 * budgets.turn_store_cap, 4_916_400)
        table = model.Table(budgets)
        self.assertEqual(table.attach("a", 0, 1188, 0, 0)[0], "opened")
        self.assertEqual(table.advance(0, 4096), [])
        self.assertEqual(table.running(), 1)
        self.assertTrue(table.retained(1))
        self.assertTrue(table.retained(4096))
        self.assertFalse(table.retained(4097))
        self.assertEqual(table.memory_bytes()["pinned_turns"], budgets.tail_bytes_max)
        assert_within_caps(self, table)
        restarted = table.advance(0, 4097)
        self.assertEqual(restarted, [("a", "tail_turns")])
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.paused)
        self.assertFalse(table.retained(1))
        self.assertEqual(table.host_turn, 4097)
        self.assertEqual(table.memory_bytes()["pinned_turns"], 0)
        assert_within_caps(self, table)
        self.assertEqual(table.attach("b", 0, 1188, 4097, 4097)[0], "opened")
        self.assertEqual(table.running(), 1)
        self.assertFalse(table.retained(1))
        assert_within_caps(self, table)

    def test_older_generation_restarts_and_the_younger_pin_stays(self):
        budgets = cfg(concurrent_transfers=3)
        table = model.Table(budgets)
        self.assertEqual(table.attach("a", 0, 1188, 0, 0)[0], "opened")
        opened_at = budgets.snapshot_age_ms + 1
        self.assertEqual(table.advance(opened_at, 601), [])
        self.assertEqual(table.running(), 1)
        self.assertTrue(table.retained(1))
        word, younger = table.attach("b", opened_at, 1188, 601, 601)
        self.assertEqual(word, "opened")
        self.assertEqual(younger.snapshot_turn, 601)
        self.assertEqual(table.attach("c", opened_at, 1188, 601, 601)[0], "shared")
        self.assertEqual(table.advance(opened_at, 4096), [])
        self.assertEqual(table.running(), 3)
        self.assertTrue(table.retained(1))
        self.assertEqual(len(table.distinct_turns()), 4096)
        self.assertEqual(table.memory_bytes()["pinned_turns"], budgets.tail_bytes_max)
        assert_within_caps(self, table)
        restarted = table.advance(opened_at, 4097)
        self.assertEqual(restarted, [("a", "tail_turns")])
        self.assertEqual(table.running(), 2)
        self.assertFalse(table.paused)
        self.assertFalse(table.retained(1))
        self.assertFalse(model.rolling_contains(602, 601, 4097, budgets))
        self.assertTrue(table.retained(602))
        self.assertTrue(table.retained(4097))
        self.assertEqual(len(table.distinct_turns()), 4097 - 601)
        self.assertEqual(table.memory_bytes()["pinned_turns"],
                         (4097 - 601) * budgets.turn_store_cap)
        assert_within_caps(self, table)
        table.cancel("c")
        self.assertEqual(table.running(), 1)
        self.assertTrue(table.retained(602))
        assert_within_caps(self, table)
        table.complete("b")
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.retained(602))
        self.assertEqual(table.attach("d", opened_at, 1188, 4097, 4097)[0], "opened")
        self.assertEqual(table.running(), 1)
        assert_within_caps(self, table)

    def test_deadline_boundary_restarts_one_joiner_and_keeps_the_later_one(self):
        budgets = model.proposed_budgets()
        table = model.Table(budgets)
        self.assertEqual(table.attach("a", 0, 1188, 0, 0)[0], "opened")
        self.assertEqual(table.advance(1000, 4), [])
        self.assertEqual(table.attach("b", 1000, 1188, 0, 4)[0], "shared")
        self.assertEqual(table.advance(budgets.catchup_ms - 1, 4), [])
        self.assertEqual(table.running(), 2)
        self.assertTrue(table.retained(1))
        assert_within_caps(self, table)
        restarted = table.advance(budgets.catchup_ms, 4)
        self.assertEqual(restarted, [("a", "deadline")])
        self.assertEqual(table.running(), 1)
        self.assertFalse(table.paused)
        self.assertTrue(table.retained(1))
        assert_within_caps(self, table)
        self.assertEqual(table.advance(1000 + budgets.catchup_ms - 1, 4), [])
        self.assertEqual(table.running(), 1)
        restarted = table.advance(1000 + budgets.catchup_ms, 4)
        self.assertEqual(restarted, [("b", "deadline")])
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.retained(1))
        assert_within_caps(self, table)
        self.assertEqual(table.attach("c", table.now_ms, 1188, 4, 4)[0], "opened")
        assert_within_caps(self, table)

    def test_deadline_wins_when_the_same_step_would_also_overflow_the_tail(self):
        budgets = model.proposed_budgets()
        table = model.Table(budgets)
        self.assertEqual(table.attach("a", 0, 1188, 0, 0)[0], "opened")
        self.assertEqual(table.advance(budgets.catchup_ms - 1, 4096), [])
        self.assertEqual(table.running(), 1)
        self.assertEqual(table.memory_bytes()["pinned_turns"], budgets.tail_bytes_max)
        restarted = table.advance(budgets.catchup_ms, 4097)
        self.assertEqual(restarted, [("a", "deadline")])
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.retained(1))
        self.assertEqual(table.memory_bytes()["pinned_turns"], 0)
        assert_within_caps(self, table)

    def test_completion_before_the_deadline_is_not_restarted(self):
        budgets = model.proposed_budgets()
        table = model.Table(budgets)
        self.assertEqual(table.attach("a", 0, 1188, 0, 0)[0], "opened")
        self.assertEqual(table.advance(budgets.catchup_ms - 1, 3), [])
        table.complete("a")
        self.assertEqual(table.advance(budgets.catchup_ms, 3), [])
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.paused)
        assert_within_caps(self, table)
        with self.assertRaises(ValueError):
            table.advance(budgets.catchup_ms - 1, 3)
        self.assertEqual(table.host_turn, 3)
        self.assertEqual(table.now_ms, budgets.catchup_ms)

    def test_fragment_and_backlog_cannot_bypass_the_owner_cap(self):
        budgets = model.proposed_budgets()
        table = model.Table(budgets)
        self.assertEqual(table.attach("a", 0, 1188, 0, 0)[0], "opened")
        huge = dict(model.SYNTHETIC_SAVES)["synthetic_2x_endless02"]
        self.assertGreater(model.fragment_count(huge), budgets.fragment_max)
        self.assertEqual(table.attach("z", 0, huge, 0, 0), ("fragment", None))
        self.assertEqual(table.running(), 1)
        self.assertEqual(table.host_turn, 0)
        self.assertEqual(table.attach("b", 0, 1188, 0, 4097), ("clock", None))
        self.assertEqual(table.running(), 1)
        self.assertEqual(table.host_turn, 0)
        self.assertEqual(table.memory_bytes()["pinned_turns"], 0)
        self.assertEqual(table.note_backlog("a", budgets.receive_backlog_turns), "held")
        self.assertEqual(table.memory_bytes()["joiner_backlog"],
                         budgets.receive_backlog_turns * budgets.turn_store_cap)
        assert_within_caps(self, table)
        self.assertEqual(table.advance(1000, 0), [])
        self.assertEqual(table.attach("b", 1000, 1188, 0, 0)[0], "shared")
        self.assertEqual(table.note_backlog("b", budgets.receive_backlog_turns), "held")
        assert_within_caps(self, table)
        self.assertEqual(table.note_backlog("b", budgets.receive_backlog_turns + 1), "restart")
        self.assertEqual(table.running(), 1)
        self.assertEqual(table.backlog["a"], budgets.receive_backlog_turns)
        self.assertNotIn("b", table.backlog)
        assert_within_caps(self, table)
        self.assertFalse(table.paused)


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
                "allocation_bound_bytes=96948576",
                "joiner_backlog_bytes=9830400",
                "channel0_flight_bytes=4416",
                "endless02_discrete_zero_loss_ms=160650",
                "endless02_loss2_seed1_transfer_ms=198750",
                "endless02_loss2_seed1_done_ms=220830",
                "endless02_loss2_seed1_pin_turns=2208",
                "03_1024_loss2_seed1_done_ms=141160",
                "03_1024_loss2_seed1_replayed=1411",
                "counterexample_1188_done_ms=160",
                "counterexample_1188_replayed=1",
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
        self.assertEqual(bound["joiner_backlog"], 9830400)
        self.assertEqual(bound["channel0_flight"], 4416)
        self.assertEqual(bound["total"], 96948576)
        without_backlog = bound["total"] - bound["joiner_backlog"] - bound["channel0_flight"]
        self.assertEqual(without_backlog, 87113760)
        self.assertGreater(bound["total"], without_backlog)


def _fields(line):
    found = {}
    for part in line.split():
        if "=" in part:
            key, value = part.split("=", 1)
            found[key] = value
    return found


class T45Measurements(unittest.TestCase):
    def test_defaults_stay(self):
        budgets = model.proposed_budgets()
        self.assertEqual((budgets.load_ms, budgets.replay_turns_per_s, budgets.turn_store_cap),
                         (0, 100, 1200))
        self.assertEqual(model.MEASURED_SAVES, (
            ("S01_07_01", 11474169),
            ("endless02", 20349454),
            ("03_1024_med_256", 12906633),
        ))
        labeled = model.t45_budgets("endless02", "empty_max")
        again = model.proposed_budgets()
        self.assertEqual(again.load_ms, 0)
        self.assertEqual(again.replay_turns_per_s, 100)
        self.assertEqual(labeled.turn_store_cap, 1200)
        self.assertEqual(labeled.tail_turns_max, budgets.tail_turns_max)
        self.assertEqual(labeled.fragment_max, budgets.fragment_max)
        base = "\n".join(model.report_lines())
        self.assertIn("load_ms: 0", base)
        self.assertIn("replay_turns_per_s: 100", base)
        self.assertIn("bytes=20349454", base)
        self.assertNotIn("bytes=20349455", base)

    def test_remeasured_saves_keep_the_fragment_counts(self):
        previous = dict(model.MEASURED_SAVES)
        self.assertEqual(model.T45_TIMING_US["S01_07_01"]["load_median"], 26807)
        self.assertEqual(model.T45_TIMING_US["endless02"]["load_median"], 46004)
        self.assertEqual(model.T45_TIMING_US["03_1024_med_256"]["load_median"], 27647)
        self.assertEqual(model.t45_load_ms("S01_07_01"), 27)
        self.assertEqual(model.t45_load_ms("endless02"), 47)
        self.assertEqual(model.t45_load_ms("03_1024_med_256"), 28)
        self.assertEqual(model.t45_replay_rate("S01_07_01", "script_duration"), 1317)
        self.assertEqual(model.t45_replay_rate("S01_07_01", "script_p95"), 614)
        self.assertEqual(model.t45_replay_rate("endless02", "script_duration"), 1915)
        self.assertEqual(model.t45_replay_rate("endless02", "empty_max"), 438)
        self.assertEqual(model.t45_replay_rate("03_1024_med_256", "script_duration"), 4245)
        for name, size in model.T45_SAVE_BYTES:
            self.assertEqual(size, previous[name] + 1)
            self.assertEqual(model.fragment_count(size), model.PUBLISHED_FRAGMENTS[name])

    def test_encoded_workloads(self):
        budgets = model.proposed_budgets()
        self.assertEqual(model.T45_EMPTY_PAYLOAD, 0)
        self.assertEqual(model.T45_SCRIPT_PAYLOAD, 16)
        self.assertEqual(model.T45_SCRIPT_COMMANDS, 1)
        self.assertEqual(model.T45_BURST_COMMANDS, 76)
        self.assertEqual(model.T45_BURST_PAYLOAD, 1216)
        self.assertEqual(model.T45_BURST_CAPACITY, 1228)
        self.assertNotEqual(model.T45_EMPTY_PAYLOAD, model.EMPTY_TURN_PAYLOAD)
        self.assertLess(model.T45_SCRIPT_PAYLOAD, budgets.payload_max)
        self.assertGreater(model.T45_BURST_PAYLOAD, budgets.turn_store_cap)
        self.assertEqual(76 * 16, 1216)

    def test_labeled_rows_admit_and_catch_up(self):
        lines = model.t45_report_lines()
        text = "\n".join(lines)
        self.assertIn("defaults_unchanged load_ms=0 replay_turns_per_s=100 turn_store_cap=1200", text)
        self.assertIn("not_end_to_end=1", text)
        self.assertIn("real_play_unmeasured=1", text)
        self.assertIn("other_hosts_unmeasured=1", text)
        compares = [line for line in lines if line.startswith("t45 compare ")]
        losses = [line for line in lines if line.startswith("t45 loss ")]
        self.assertEqual(len(compares), 18)
        self.assertEqual(len(losses), 9)
        for line in compares:
            row = _fields(line)
            self.assertEqual(row["admit"], "admit")
            self.assertEqual(row["reason"], "within_budgets")
            self.assertEqual(row["baseline_admit"], "admit")
            self.assertEqual(row["status"], "success")
            self.assertEqual(row["paused"], "false")
            self.assertLessEqual(int(row["need"]), int(row["baseline_need"]))
            self.assertLessEqual(int(row["need"]), 4096)
            self.assertLessEqual(int(row["pin_turns"]), int(row["baseline_pin_turns"]))
            self.assertLessEqual(int(row["done_ms"]), int(row["baseline_done_ms"]))
            self.assertEqual(row["gap_turns"], row["baseline_gap_turns"])
        binding = _fields(next(line for line in compares
                               if "endless02" in line and "rate_key=empty_max" in line))
        self.assertEqual(binding["replay"], "438")
        self.assertEqual(binding["load_ms"], "47")
        self.assertEqual(binding["need"], "3289")
        self.assertEqual(binding["baseline_need"], "3570")
        self.assertEqual(binding["gap_turns"], "3213")
        self.assertEqual(binding["pin_turns"], "1656")
        self.assertEqual(binding["baseline_pin_turns"], "1784")
        published_transfer = {
            "S01_07_01": "113250",
            "endless02": "198750",
            "03_1024_med_256": "127050",
        }
        for line in losses:
            row = _fields(line)
            self.assertEqual(row["status"], "success")
            self.assertEqual(row["paused"], "false")
            self.assertEqual(row["replayed"], row["host_produced"])
            self.assertEqual(row["pin_turns"], row["replayed"])
            for name, transfer in published_transfer.items():
                if line.startswith("t45 loss " + name + " "):
                    self.assertEqual(row["transfer_ms"], transfer)
        slow = _fields(next(line for line in losses
                            if line.startswith("t45 loss endless02 ")
                            and "rate_key=empty_max" in line))
        self.assertEqual(slow["done_ms"], "204944")
        self.assertEqual(slow["pin_turns"], "2049")
        self.assertLess(int(slow["pin_turns"]), 2208)

    def test_assumptions_keep_the_defaults(self):
        text = " ".join(model.assumptions())
        self.assertIn("load_ms defaults to 0 and replay_turns_per_s defaults to 100", text)
        self.assertIn("do not replace these defaults", text)
        self.assertIn("real-play distribution was not measured", text)
        self.assertIn("drops data datagrams only", text)

    def test_adr_records_the_labeled_result(self):
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
                "t45_load_ms_ceil_median=27/47/28",
                "t45_endless02_empty_max_replay=438",
                "t45_endless02_empty_max_need=3289",
                "t45_gap_turns_unchanged=1809/3213/2037",
                "t45_burst_payload=1216",
                "t45_empty_payload=0",
                "t45_script_payload=16",
                "11 474 169",
                "11 474 170",
                ):
            self.assertIn(token, adr)
        self.assertIn("narrowed (T45, 2026-10-27)", oq)
        self.assertIn("## OQ-281:", oq)
        self.assertIn("## OQ-282:", oq)
        self.assertNotIn("## OQ-300:", oq)


def _complete(done_ms, fragments=1):
    return model.TransferResult(
        "complete", done_ms, fragments, fragments, fragments, False, 0, 0)


class CausalReadiness(unittest.TestCase):
    def test_one_fragment_turn_cannot_arrive_before_one_way(self):
        budgets = model.proposed_budgets()
        self.assertEqual(model.turn_produce_ms(1, budgets), 100)
        self.assertEqual(model.turn_arrival_ms(1, budgets), 250)
        self.assertEqual(model.max_delivery_lag(budgets), 2)
        self.assertFalse(model.equality_reachable(budgets))
        wire = model.frontiers_at(160, budgets)
        self.assertEqual(wire["host_produced"], 1)
        self.assertEqual(wire["host_applied"], 1)
        self.assertEqual(wire["received"], 0)
        self.assertFalse(wire["hole"])
        joiner = model.catchup_state_at(160, budgets, 150)
        self.assertEqual(joiner["applied"], 0)
        self.assertEqual(joiner["received"], 0)
        arrived = model.frontiers_at(250, budgets)
        self.assertEqual(arrived["received"], 1)
        self.assertEqual(arrived["host_produced"], 2)
        result = model.evaluate_causal(1188, budgets)
        self.assertEqual(result.status, "success")
        self.assertEqual(result.reason, "caught_up")
        self.assertFalse(result.paused)
        self.assertTrue(result.input_enabled)
        self.assertEqual(result.first_arrival_ms, 250)
        self.assertEqual(result.ready_ms, 150)
        self.assertEqual(
            (result.done_ms, result.applied, result.received,
             result.host_produced, result.host_applied, result.hash_turn),
            (1160, 10, 10, 11, 11, 10))
        self.assertLess(result.applied, result.host_applied)
        self.assertEqual(
            (result.optimistic_done_ms, result.optimistic_replayed, result.optimistic_host),
            (160, 1, 1))
        self.assertNotEqual(result.done_ms, result.optimistic_done_ms)
        self.assertTrue(result.honest(budgets))
        fluid = model.evaluate_join(1188, budgets)
        self.assertEqual((fluid.done_ms, fluid.replayed, fluid.host_produced), (160, 1, 1))

    def test_tick_order_is_produce_then_deliver_then_apply(self):
        budgets = model.replace(model.proposed_budgets(), one_way_ms=0)
        at_produce = model.catchup_state_at(100, budgets, 0)
        self.assertEqual(at_produce["host_produced"], 1)
        self.assertEqual(at_produce["host_applied"], 1)
        self.assertEqual(at_produce["received"], 1)
        self.assertEqual(at_produce["applied"], 0)
        self.assertLess(at_produce["applied"], at_produce["received"])
        at_apply = model.catchup_state_at(110, budgets, 0)
        self.assertEqual(at_apply["applied"], 1)
        self.assertEqual(at_apply["received"], 1)
        self.assertLessEqual(at_apply["applied"], at_apply["received"])
        self.assertLessEqual(at_apply["received"], at_apply["host_produced"])
        delayed = model.proposed_budgets()
        before = model.catchup_state_at(100, delayed, 0)
        self.assertEqual(before["host_produced"], 1)
        self.assertEqual(before["received"], 0)
        self.assertEqual(before["applied"], 0)

    def test_zero_delay_can_match_and_a_full_turn_of_delay_cannot(self):
        zero = model.replace(model.proposed_budgets(), one_way_ms=0)
        self.assertTrue(model.equality_reachable(zero))
        matched = model.evaluate_causal(1188, zero)
        self.assertEqual(matched.status, "success")
        self.assertEqual(matched.applied, matched.host_applied)
        self.assertEqual((matched.done_ms, matched.applied), (1010, 10))
        self.assertFalse(matched.paused)
        short = model.replace(model.proposed_budgets(), one_way_ms=50)
        self.assertTrue(model.equality_reachable(short))
        early = model.evaluate_causal(1188, short)
        self.assertEqual(early.status, "success")
        self.assertEqual(early.first_arrival_ms, 150)
        self.assertEqual((early.done_ms, early.applied, early.host_applied), (1060, 10, 10))
        long = model.proposed_budgets()
        self.assertFalse(model.equality_reachable(long))
        behind = model.evaluate_causal(1188, long)
        self.assertLess(behind.applied, behind.host_applied)
        self.assertGreaterEqual(long.one_way_ms, long.turn_ms)

    def test_catchup_faster_than_live_reaches_the_boundary(self):
        budgets = model.proposed_budgets()
        late = _complete(5000)
        fast = model.evaluate_causal(1188, budgets, transfer=late)
        self.assertEqual(fast.status, "success")
        self.assertFalse(fast.paused)
        self.assertTrue(fast.input_enabled)
        self.assertEqual(fast.missing, 0)
        self.assertEqual(fast.applied, fast.received)
        self.assertLess(fast.applied, fast.host_applied)
        self.assertEqual((fast.done_ms, fast.applied, fast.host_applied), (6160, 60, 61))
        self.assertTrue(fast.honest(budgets))
        slow_cfg = model.replace(budgets, replay_turns_per_s=5, catchup_ms=60_000)
        slow = model.evaluate_causal(1188, slow_cfg, transfer=late)
        self.assertEqual(slow.status, "restart")
        self.assertEqual(slow.reason, "cannot_outrun")
        self.assertFalse(slow.paused)
        self.assertFalse(slow.input_enabled)
        self.assertNotEqual(slow.status, "success")
        self.assertGreater(slow.missing, 0)
        self.assertLessEqual(slow.applied, slow.host_applied)
        self.assertTrue(slow.honest(slow_cfg))

    def test_missing_or_reordered_prefix_is_not_success(self):
        budgets = model.proposed_budgets()

        def drop_turn_one(relative_k, attempt):
            del attempt
            return relative_k == 1

        missing = model.evaluate_causal(
            1188, budgets, data_drop=drop_turn_one, horizon_ms=2000)
        self.assertEqual(missing.status, "restart")
        self.assertEqual(missing.reason, "missing_prefix")
        self.assertFalse(missing.input_enabled)
        self.assertFalse(missing.paused)
        self.assertNotEqual(missing.status, "success")

        def delay_turn_one(relative_k, attempt):
            del attempt
            return 400 if relative_k == 1 else 0

        schedule = model.build_turn_schedule(4, budgets, reorder=delay_turn_one)
        self.assertEqual(schedule.arrivals[1], 650)
        self.assertEqual(schedule.arrivals[2], 350)
        probed = model.frontiers_at(400, budgets, arrivals=schedule.arrivals)
        self.assertTrue(probed["hole"])
        self.assertEqual(probed["received"], 0)
        self.assertGreater(probed["host_produced"], 1)
        early = model.evaluate_causal(
            1188, budgets, reorder=delay_turn_one, horizon_ms=600)
        self.assertNotEqual(early.status, "success")
        self.assertEqual(early.reason, "unmet_boundary")
        self.assertFalse(early.paused)
        filled = model.evaluate_causal(1188, budgets, reorder=delay_turn_one)
        self.assertEqual(filled.status, "success")
        self.assertEqual(filled.first_arrival_ms, 650)
        self.assertGreaterEqual(filled.first_arrival_ms, 250)
        self.assertTrue(filled.honest(budgets))

    def test_data_loss_delays_arrival_and_ack_loss_does_not(self):
        budgets = model.proposed_budgets()

        def drop_first(relative_k, attempt):
            return relative_k == 1 and attempt == 0

        delayed = model.build_turn_schedule(3, budgets, data_drop=drop_first)
        self.assertEqual(delayed.arrivals[1], 550)
        self.assertGreaterEqual(delayed.arrivals[1], 250)
        self.assertEqual(delayed.data_drops, 1)
        self.assertEqual(delayed.acks_lost, 0)
        hole = model.frontiers_at(400, budgets, arrivals=delayed.arrivals)
        self.assertTrue(hole["hole"])
        self.assertEqual(hole["received"], 0)
        self.assertLessEqual(delayed.arrivals[2], 400)

        def drop_acks(relative_k, attempt):
            del relative_k, attempt
            return True

        quiet = model.build_turn_schedule(12, budgets, ack_drop=drop_acks)
        self.assertEqual(quiet.arrivals[1], 250)
        self.assertEqual(quiet.data_drops, 0)
        self.assertGreater(quiet.acks_lost, 0)
        self.assertGreater(quiet.duplicates, 0)
        joined = model.evaluate_causal(1188, budgets, ack_drop=drop_acks)
        plain = model.evaluate_causal(1188, budgets)
        self.assertEqual(joined.status, "success")
        self.assertEqual(joined.done_ms, plain.done_ms)
        self.assertEqual(joined.first_arrival_ms, 250)
        self.assertEqual(joined.data_drops, 0)
        self.assertGreater(joined.acks_lost, 0)
        self.assertFalse(joined.paused)
        self.assertTrue(joined.honest(budgets))

    def test_cap_restarts_one_joiner_and_the_host_keeps_playing(self):
        budgets = model.replace(
            model.proposed_budgets(), tail_turns_max=4, snapshot_age_ms=0)
        stopped = model.evaluate_causal(1188, budgets)
        self.assertEqual(stopped.status, "restart")
        self.assertEqual(stopped.reason, "tail_turns")
        self.assertFalse(stopped.paused)
        self.assertFalse(stopped.input_enabled)
        self.assertNotEqual(stopped.status, "success")
        self.assertEqual(stopped.done_ms, 500)
        self.assertEqual(stopped.host_span, 5)
        table = model.Table(budgets)
        self.assertEqual(table.attach("old", 0, 1188, 0, 0)[0], "opened")
        self.assertEqual(table.advance(400, 4), [])
        self.assertEqual(table.attach("young", 400, 1188, 4, 4)[0], "opened")
        produced, host_applied = model.host_frontier(500, 0, budgets)
        self.assertEqual((produced, host_applied), (5, 5))
        restarted = table.advance(500, produced)
        self.assertEqual(restarted, [("old", "tail_turns")])
        self.assertEqual(table.running(), 1)
        self.assertFalse(table.paused)
        self.assertEqual(table.host_turn, 5)
        self.assertEqual(table.now_ms, 500)
        self.assertFalse(table.retained(1))
        self.assertTrue(table.retained(5))
        self.assertGreater(table.host_turn, 4)
        mismatch = model.evaluate_causal(1188, model.proposed_budgets(), hash_matches=False)
        self.assertEqual(mismatch.status, "restart")
        self.assertEqual(mismatch.reason, "hash_mismatch")
        self.assertFalse(mismatch.paused)
        self.assertFalse(mismatch.input_enabled)

    def test_measured_saves_meet_the_boundary_inside_the_caps(self):
        budgets = model.proposed_budgets()
        self.assertEqual((budgets.load_ms, budgets.replay_turns_per_s, budgets.turn_store_cap),
                         (0, 100, 1200))
        expected = {
            "S01_07_01": (90450, 100480, 101160, 1010, 1011, 903, 100490, 1004),
            "endless02": (160650, 178480, 179160, 1790, 1791, 1605, 178490, 1784),
            "03_1024_med_256": (101850, 113140, 113160, 1130, 1131, 1017, 113160, 1131),
        }
        for name, size in model.MEASURED_SAVES:
            result = model.evaluate_causal(size, budgets)
            self.assertEqual(result.status, "success", name)
            self.assertFalse(result.paused, name)
            self.assertTrue(result.input_enabled, name)
            self.assertEqual(result.missing, 0, name)
            self.assertLess(result.applied, result.host_applied, name)
            self.assertEqual(result.applied, result.received, name)
            self.assertLessEqual(result.host_span, budgets.tail_turns_max, name)
            self.assertLessEqual(result.backlog_turns, budgets.receive_backlog_turns, name)
            self.assertLessEqual(result.done_ms, budgets.catchup_ms, name)
            self.assertTrue(result.honest(budgets), name)
            (transfer, ready, done, applied, host, peak, opt_done, opt_replayed) = expected[name]
            self.assertEqual(
                (result.transfer_ms, result.ready_ms, result.done_ms, result.applied,
                 result.host_applied, result.backlog_turns, result.optimistic_done_ms,
                 result.optimistic_replayed),
                (transfer, ready, done, applied, host, peak, opt_done, opt_replayed),
                name)
        # Same millisecond, different cursor: the fluid row names the host turn.
        split = model.evaluate_causal(dict(model.MEASURED_SAVES)["03_1024_med_256"], budgets)
        self.assertEqual(split.done_ms, split.optimistic_done_ms)
        self.assertNotEqual(split.applied, split.optimistic_replayed)
        binding = model.t45_budgets("endless02", "empty_max")
        self.assertEqual(binding.replay_turns_per_s, 438)
        self.assertEqual(binding.load_ms, 47)
        self.assertEqual(binding.turn_store_cap, 1200)
        decision, reason, need = model.admit(dict(model.T45_SAVE_BYTES)["endless02"], binding)
        self.assertEqual((decision, reason, need), ("admit", "within_budgets", 3289))
        caught = model.evaluate_causal(dict(model.T45_SAVE_BYTES)["endless02"], binding)
        self.assertEqual(caught.status, "success")
        self.assertEqual(
            (caught.done_ms, caught.applied, caught.host_applied, caught.pin_turns,
             caught.host_span, caught.optimistic_done_ms, caught.optimistic_replayed),
            (166153, 1660, 1661, 1660, 1661, 165665, 1656))
        self.assertLess(caught.host_span, binding.tail_turns_max)
        self.assertLess(caught.optimistic_replayed, 1784)
        loss = model.replace(binding, loss_per_million=20_000, seed=1)
        lost = model.evaluate_causal(dict(model.T45_SAVE_BYTES)["endless02"], loss)
        self.assertEqual(lost.status, "success")
        self.assertEqual(lost.acks_lost, 0)
        self.assertEqual(
            (lost.transfer_ms, lost.done_ms, lost.applied, lost.host_applied,
             lost.optimistic_done_ms, lost.optimistic_replayed),
            (198750, 205153, 2050, 2051, 204944, 2049))
        self.assertFalse(lost.paused)
        self.assertLess(lost.applied, lost.host_applied)
        self.assertTrue(lost.honest(loss))

    def test_channels_stay_independent_and_the_fluid_quotient_stays(self):
        budgets = model.proposed_budgets()
        snapshot_us, turn_us, both_us, one_way_us = model.channel_bandwidth_us(budgets)
        self.assertEqual((snapshot_us, turn_us, both_us, one_way_us), (3072, 6, 3078, 150000))
        self.assertLess(both_us, one_way_us)
        self.assertEqual(model.fluid_tenths(17130, 32, 150), 803)
        self.assertEqual(model.fluid_tenths(17130, 32, 300), 1606)
        # 17130 * 150 = 2569500; 2569500 = 32 * 80296 + 28, and 28/32 is 0.875 ms.
        # 17130 * 300 = 5139000; 5139000 = 32 * 160593 + 24, and 24/32 is 0.75 ms.
        self.assertEqual(17130 * 150, 2569500)
        self.assertEqual(divmod(2569500, 32), (80296, 28))
        self.assertEqual(17130 * 300, 5139000)
        self.assertEqual(divmod(5139000, 32), (160593, 24))
        lines = model.causal_report_lines()
        text = "\n".join(lines)
        self.assertIn("defaults_unchanged load_ms=0 replay_turns_per_s=100 turn_store_cap=1200",
                      text)
        self.assertIn("command_delay=2", text)
        self.assertIn("independent_channels=1 shared_queue=0", text)
        self.assertIn("arrival_ms=250", text)
        self.assertIn("optimistic_done_ms=160", text)
        self.assertIn("equal_frontier=0", text)
        self.assertIn("fluid_unchanged endless02_rtt150_tenths=803 endless02_rtt300_tenths=1606",
                      text)
        self.assertIn("not_end_to_end=1", text)
        self.assertIn("shared_link_unmeasured=1", text)
        fluid = "\n".join(model.report_lines())
        self.assertIn("counterexample_1188_done_ms=160", fluid)
        self.assertNotIn("causal one_fragment", fluid)
        again = model.proposed_budgets()
        self.assertEqual((again.load_ms, again.replay_turns_per_s, again.turn_store_cap),
                         (0, 100, 1200))

    def test_a_missing_due_turn_is_not_hidden_by_the_arrived_prefix(self):
        budgets = model.proposed_budgets()
        late = _complete(3000)

        def drop_turn_11(relative_k, attempt):
            del attempt
            return relative_k == 11

        schedule = model.build_turn_schedule(40, budgets, data_drop=drop_turn_11)
        self.assertNotIn(11, schedule.arrivals)
        self.assertLessEqual(schedule.arrivals[12], 3100)
        probed = model.frontiers_at(3100, budgets, arrivals=schedule.arrivals)
        self.assertEqual(probed["received"], 10)
        self.assertEqual(probed["due"], 29)
        self.assertEqual(probed["host_produced"], 31)
        self.assertTrue(probed["hole"])
        self.assertFalse(model.prefix_is_due(10, 10, 3100, budgets))
        self.assertFalse(model.command_input_open(10, 10, 3100, budgets))
        state = model.catchup_state_at(3100, budgets, 3000, arrivals=schedule.arrivals)
        self.assertEqual((state["applied"], state["received"], state["due"]), (10, 10, 29))
        result = model.evaluate_causal(
            1188, budgets, transfer=late, data_drop=drop_turn_11)
        self.assertEqual(result.status, "restart")
        self.assertEqual(result.reason, "missing_prefix")
        self.assertEqual(
            (result.done_ms, result.applied, result.received,
             result.host_applied, result.host_produced),
            (3100, 10, 10, 31, 31))
        self.assertFalse(result.input_enabled)
        self.assertGreater(result.missing, 0)
        self.assertFalse(result.paused)
        self.assertTrue(result.honest(budgets))
        forged = model.replace(
            result, status="success", reason="caught_up", input_enabled=True,
            missing=0, hash_turn=10)
        self.assertFalse(forged.honest(budgets))

    def test_input_stays_off_until_the_due_prefix_and_the_hash(self):
        budgets = model.proposed_budgets()
        held = model.catchup_state_at(960, budgets, 150)
        self.assertEqual((held["applied"], held["received"], held["due"]), (8, 8, 8))
        self.assertTrue(model.prefix_is_due(8, 8, 960, budgets))
        self.assertFalse(model.command_input_open(8, 8, 960, budgets))
        opened = model.command_input_open(10, 10, 1160, budgets)
        self.assertTrue(opened)

        def delay_turn_11(relative_k, attempt):
            del attempt
            return 2000 if relative_k == 11 else 0

        schedule = model.build_turn_schedule(40, budgets, reorder=delay_turn_11)
        self.assertEqual(schedule.arrivals[11], 3250)
        self.assertLessEqual(schedule.arrivals[12], 3100)
        hole = model.frontiers_at(3100, budgets, arrivals=schedule.arrivals)
        self.assertTrue(hole["hole"])
        self.assertEqual((hole["received"], hole["due"], hole["host_produced"]), (10, 29, 31))
        self.assertFalse(model.command_input_open(10, 10, 3100, budgets))
        waiting = model.catchup_state_at(
            3100, budgets, 3000, arrivals=schedule.arrivals)
        self.assertEqual((waiting["applied"], waiting["received"]), (10, 10))
        self.assertFalse(model.command_input_open(
            waiting["applied"], waiting["received"], 3100, budgets))
        filled = model.evaluate_causal(
            1188, budgets, transfer=_complete(3000), reorder=delay_turn_11)
        self.assertEqual(filled.status, "success")
        self.assertTrue(filled.input_enabled)
        self.assertFalse(filled.paused)
        self.assertEqual(
            (filled.ready_ms, filled.done_ms, filled.applied, filled.received,
             filled.host_applied, filled.hash_turn),
            (3480, 4160, 40, 40, 41, 40))
        self.assertEqual(
            filled.applied,
            filled.snapshot_turn + model.due_relative(filled.done_ms, budgets))
        self.assertTrue(model.command_input_open(
            filled.applied, filled.received, filled.done_ms, budgets))
        self.assertTrue(filled.honest(budgets))
        before = model.catchup_state_at(
            filled.done_ms - budgets.turn_ms, budgets, 3000, arrivals=schedule.arrivals)
        self.assertFalse(model.command_input_open(
            before["applied"], before["received"], filled.done_ms - budgets.turn_ms, budgets))

    def test_a_missing_last_due_turn_is_not_a_hole(self):
        budgets = model.proposed_budgets()

        def drop_turn_11(relative_k, attempt):
            del attempt
            return relative_k == 11

        schedule = model.build_turn_schedule(20, budgets, data_drop=drop_turn_11)
        probed = model.frontiers_at(1300, budgets, arrivals=schedule.arrivals)
        self.assertEqual(probed["received"], 10)
        self.assertEqual(probed["due"], 11)
        self.assertFalse(probed["hole"])
        self.assertLess(probed["received"], probed["due"])
        self.assertFalse(model.command_input_open(10, 10, 1300, budgets))
        result = model.evaluate_causal(
            1188, budgets, transfer=_complete(1200), data_drop=drop_turn_11)
        self.assertEqual(result.status, "restart")
        self.assertEqual(result.reason, "missing_prefix")
        self.assertEqual((result.done_ms, result.applied, result.received), (1300, 10, 10))
        self.assertEqual(result.host_applied, 13)
        self.assertFalse(result.input_enabled)
        self.assertFalse(result.paused)
        self.assertTrue(result.honest(budgets))

    def test_deadline_runs_before_one_fragment_success(self):
        budgets = model.proposed_budgets()
        tight = model.replace(budgets, catchup_ms=1160)
        table = model.Table(tight)
        self.assertEqual(table.attach("old", 0, 1188, 0, 0)[0], "opened")
        self.assertEqual(table.advance(1159, 11), [])
        self.assertEqual(table.running(), 1)
        self.assertEqual(table.advance(1160, 11), [("old", "deadline")])
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.paused)
        self.assertEqual(table.advance(1161, 11), [])
        self.assertEqual(table.running(), 0)
        early = model.evaluate_causal(1188, tight, horizon_ms=1159)
        self.assertNotEqual(early.status, "success")
        self.assertFalse(early.input_enabled)
        self.assertFalse(early.paused)
        at_deadline = model.evaluate_causal(1188, tight)
        self.assertEqual(at_deadline.status, "restart")
        self.assertEqual(at_deadline.reason, "deadline")
        self.assertEqual(at_deadline.done_ms, 1160)
        self.assertFalse(at_deadline.input_enabled)
        self.assertFalse(at_deadline.paused)
        self.assertTrue(at_deadline.honest(tight))
        forged = model.replace(
            at_deadline, status="success", reason="caught_up", input_enabled=True,
            missing=0, hash_turn=10)
        self.assertFalse(forged.honest(tight))
        plain = model.evaluate_causal(1188, budgets)
        self.assertEqual((plain.status, plain.done_ms, plain.applied, plain.host_applied),
                         ("success", 1160, 10, 11))
        self.assertTrue(plain.honest(budgets))

    def test_deadline_beats_the_tail_and_a_younger_join_stays(self):
        collision = model.replace(
            model.proposed_budgets(), catchup_ms=1000, tail_turns_max=9)
        table = model.Table(collision)
        self.assertEqual(table.attach("a", 0, 1188, 0, 0)[0], "opened")
        self.assertEqual(table.advance(999, 9), [])
        self.assertEqual(table.running(), 1)
        self.assertEqual(table.advance(1000, 10), [("a", "deadline")])
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.paused)
        caught = model.evaluate_causal(1188, collision)
        self.assertEqual(caught.status, "restart")
        self.assertEqual(caught.reason, "deadline")
        self.assertEqual(caught.done_ms, 1000)
        self.assertFalse(caught.input_enabled)
        self.assertFalse(caught.paused)
        self.assertTrue(caught.honest(collision))

        tight = model.replace(model.proposed_budgets(), catchup_ms=1160)
        shared = model.Table(tight)
        self.assertEqual(shared.attach("old", 0, 1188, 0, 0)[0], "opened")
        self.assertEqual(shared.advance(1000, 10), [])
        self.assertEqual(shared.attach("young", 1000, 1188, 0, 10)[0], "shared")
        self.assertEqual(shared.advance(1159, 11), [])
        self.assertEqual(shared.running(), 2)
        self.assertEqual(shared.advance(1160, 11), [("old", "deadline")])
        self.assertEqual(shared.running(), 1)
        self.assertFalse(shared.paused)
        self.assertEqual(shared.advance(1161, 11), [])
        self.assertEqual(shared.running(), 1)
        self.assertEqual(shared.advance(2159, 21), [])
        self.assertEqual(shared.running(), 1)
        self.assertEqual(shared.advance(2160, 21), [("young", "deadline")])
        self.assertEqual(shared.running(), 0)
        self.assertFalse(shared.paused)

    def test_a_horizon_before_load_applies_nothing(self):
        budgets = model.proposed_budgets()
        late = _complete(3000)
        early = model.evaluate_causal(1188, budgets, transfer=late, horizon_ms=2000)
        state = model.catchup_state_at(2000, budgets, 3000)
        self.assertEqual(early.status, "restart")
        self.assertEqual(early.reason, "unmet_boundary")
        self.assertEqual(early.done_ms, 2000)
        self.assertEqual((early.applied, early.received), (0, 0))
        self.assertEqual((state["applied"], state["received"]), (0, 0))
        self.assertEqual(early.pin_turns, 0)
        self.assertEqual(early.backlog_turns, 0)
        self.assertEqual(early.host_applied, 20)
        self.assertEqual(early.host_span, early.host_applied - early.snapshot_turn)
        self.assertFalse(early.input_enabled)
        self.assertFalse(early.paused)
        self.assertTrue(early.honest(budgets))

        loaded = model.replace(budgets, load_ms=500)
        during = model.evaluate_causal(
            1188, loaded, transfer=late, horizon_ms=2000)
        self.assertEqual(during.done_ms, 2000)
        self.assertEqual((during.applied, during.received, during.pin_turns), (0, 0, 0))
        self.assertEqual(during.backlog_turns, 0)
        self.assertEqual(during.host_span, 20)
        self.assertFalse(during.input_enabled)
        self.assertFalse(during.paused)
        self.assertTrue(during.honest(loaded))

        # Transfer has finished; load has not. The horizon still cannot replay.
        mid_load = model.evaluate_causal(
            1188, model.replace(budgets, load_ms=1000),
            transfer=_complete(1500), horizon_ms=2000)
        self.assertEqual(mid_load.done_ms, 2000)
        self.assertEqual(mid_load.applied, 0)
        self.assertEqual(mid_load.pin_turns, 0)
        self.assertFalse(mid_load.input_enabled)
        self.assertTrue(mid_load.honest(model.replace(budgets, load_ms=1000)))

    def test_an_unfinished_replay_step_is_not_applied(self):
        budgets = model.proposed_budgets()
        self.assertEqual(model.replay_step_ms(budgets), 10)
        for ms in (1155, 1159):
            tight = model.replace(budgets, catchup_ms=ms)
            stopped = model.evaluate_causal(1188, tight)
            state = model.catchup_state_at(ms, tight, 150)
            self.assertEqual(stopped.status, "restart")
            self.assertEqual(stopped.reason, "deadline")
            self.assertEqual(stopped.done_ms, ms)
            self.assertEqual(stopped.applied, 9)
            self.assertNotEqual(stopped.applied, 10)
            self.assertEqual(stopped.applied, state["applied"])
            self.assertEqual(stopped.received, state["received"])
            self.assertEqual(stopped.pin_turns, stopped.applied - stopped.snapshot_turn)
            self.assertEqual(
                stopped.host_applied,
                stopped.snapshot_turn + stopped.done_ms // budgets.turn_ms)
            self.assertEqual(stopped.host_span, stopped.host_applied - stopped.snapshot_turn)
            self.assertEqual(stopped.backlog_turns, state["received"] - state["applied"])
            self.assertFalse(stopped.input_enabled)
            self.assertFalse(stopped.paused)
            self.assertTrue(stopped.honest(tight))

        cut = model.evaluate_causal(1188, budgets, horizon_ms=1155)
        state = model.catchup_state_at(1155, budgets, 150)
        self.assertEqual(cut.status, "restart")
        self.assertEqual(cut.reason, "unmet_boundary")
        self.assertEqual(cut.done_ms, 1155)
        self.assertEqual(cut.applied, 9)
        self.assertEqual(cut.applied, state["applied"])
        self.assertEqual(cut.received, state["received"])
        self.assertEqual(cut.pin_turns, 9)
        self.assertEqual(cut.host_span, cut.host_applied)
        self.assertEqual(cut.host_applied, 11)
        self.assertEqual(cut.backlog_turns, 1)
        self.assertFalse(cut.input_enabled)
        self.assertFalse(cut.paused)
        self.assertTrue(cut.honest(budgets))

        plain = model.evaluate_causal(1188, budgets)
        finished = model.catchup_state_at(1160, budgets, 150)
        self.assertEqual(
            (plain.status, plain.done_ms, plain.applied, plain.received, plain.host_applied),
            ("success", 1160, 10, 10, 11))
        self.assertEqual(finished["applied"], 10)
        self.assertTrue(plain.input_enabled)
        self.assertTrue(plain.honest(budgets))
        tight = model.replace(budgets, catchup_ms=1160)
        at_deadline = model.evaluate_causal(1188, tight)
        self.assertEqual(at_deadline.status, "restart")
        self.assertEqual(at_deadline.reason, "deadline")
        self.assertEqual(at_deadline.done_ms, 1160)
        self.assertEqual(at_deadline.applied, 9)
        self.assertEqual(at_deadline.pin_turns, 9)
        self.assertEqual(at_deadline.host_span, 11)
        self.assertEqual(at_deadline.backlog_turns, 1)
        self.assertFalse(at_deadline.input_enabled)
        self.assertFalse(at_deadline.paused)
        self.assertTrue(at_deadline.honest(tight))

    def test_adr_names_the_causal_boundary_and_stays_proposed(self):
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
                "causal_one_fragment_arrival_ms=250",
                "causal_one_fragment_done_ms=1160",
                "causal_endless02_done_ms=179160",
                "causal_endless02_applied=1790",
                "causal_endless02_host=1791",
                "causal_03_same_ms_applied=1130",
                "causal_03_same_ms_fluid=1131",
                "t45_endless02_empty_max_causal_applied=1660",
                "t45_endless02_empty_max_fluid_pin=1656",
                "OQ-320",
                ):
            self.assertIn(token, adr)
        self.assertIn("## OQ-320:", oq)
        self.assertIn("OQ-321..329 were not used", oq)
        self.assertNotIn("## OQ-321:", oq)


if __name__ == "__main__":
    unittest.main()
