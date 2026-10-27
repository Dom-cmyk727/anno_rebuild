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
        self.assertEqual(table.attach("a", 0, size, pinned.snapshot_turn, host)[0], "opened")
        self.assertFalse(model.rolling_contains(1, 0, host, budgets))
        self.assertTrue(table.retained(1, host))

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
        self.assertEqual(table.attach("b", 1000, size, 0, 0)[0], "shared")
        self.assertEqual(table.attach("c", 1000, size, 0, 0)[0], "refuse")
        self.assertEqual(table.running(), 2)
        self.assertEqual(table.attach("b2", budgets.snapshot_age_ms, size, 0, 0)[0], "refuse")

        host = budgets.turn_buffer + 1
        self.assertFalse(model.rolling_contains(1, 0, host, budgets))
        self.assertTrue(table.retained(1, host))
        table.cancel("b")
        self.assertEqual(table.running(), 1)
        self.assertTrue(table.retained(1, host))
        both = model.evaluate_join(size, budgets)
        other = model.evaluate_join(size, budgets)
        assert_joined_clock(self, both, budgets.turn_ms)
        assert_joined_clock(self, other, budgets.turn_ms)
        self.assertFalse(both.paused or other.paused)
        table.complete("a")
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.retained(1, host))

        self.assertEqual(table.migrate(), [])
        self.assertEqual(table.attach("e", 0, size, 0, 0)[0], "opened")
        self.assertEqual(table.migrate(), ["e"])
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.retained(1, host))

        fresh = model.Table(budgets)
        self.assertEqual(fresh.attach("a", 0, size, 0, 0)[0], "opened")
        opened, gen = fresh.attach("d", budgets.snapshot_age_ms + 1, size, 40, 40)
        self.assertEqual(opened, "opened")
        self.assertEqual(gen.snapshot_turn, 40)
        self.assertEqual(fresh.running(), 2)

    def test_three_joiners_two_generations_use_one_global_cap(self):
        budgets = cfg(concurrent_transfers=2, max_generations=2)
        size = 1188 * 4
        table = model.Table(budgets)
        word, first = table.attach("a", 0, size, 0, 0)
        self.assertEqual(word, "opened")
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
        self.assertFalse(model.rolling_contains(1, 0, host, budgets))
        self.assertTrue(table.retained(1, host))
        table.cancel("b")
        self.assertEqual(table.running(), 1)
        self.assertTrue(table.retained(1, host))
        word, _third = table.attach("c", budgets.snapshot_age_ms + 1, size, 80, host)
        self.assertEqual(word, "opened")
        self.assertTrue(table.retained(1, host))
        self.assertEqual(table.running(), 2)
        table.complete("a")
        self.assertFalse(table.retained(1, host))
        self.assertEqual(table.running(), 1)
        self.assertEqual(table.migrate(), ["c"])
        self.assertEqual(table.running(), 0)
        self.assertFalse(table.retained(81, host))

    def test_a_new_generation_does_not_evict_a_tail_that_still_fits(self):
        budgets = cfg(tail_turns_max=10, concurrent_transfers=2, max_generations=2)
        table = model.Table(budgets)
        self.assertEqual(table.attach("a", 0, 1188, 100, 110)[0], "opened")
        # The first image is too old to share, so this ask would open a second
        # generation. Its span does not fit, and A's turns stay.
        self.assertEqual(
            table.attach("b", budgets.snapshot_age_ms + 1, 1188, 0, 110)[0], "tail")
        self.assertTrue(table.retained(101, 110))
        self.assertEqual(table.running(), 1)

    def test_joiner_backlog_is_capped_without_pausing_the_other(self):
        budgets = model.proposed_budgets()
        table = model.Table(budgets)
        self.assertEqual(table.attach("a", 0, 1188, 0, 0)[0], "opened")
        self.assertEqual(table.attach("b", 1000, 1188, 0, 0)[0], "shared")
        self.assertEqual(table.note_backlog("a", 10), "held")
        self.assertEqual(table.note_backlog("b", budgets.receive_backlog_turns + 1), "restart")
        self.assertEqual(table.running(), 1)
        self.assertEqual(table.backlog["a"], 10)
        mem = table.memory_bytes(0)
        self.assertEqual(mem["joiner_backlog"], 10 * budgets.turn_store_cap)
        self.assertEqual(mem["total"], sum(value for key, value in mem.items() if key != "total"))
        self.assertLessEqual(mem["total"], model.allocation_bound(budgets)["total"])
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


if __name__ == "__main__":
    unittest.main()
