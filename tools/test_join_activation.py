#!/usr/bin/env python3
"""Self-checks for tools/join_activation.py. No network and no game files.

The round-cb regressions come first. Run the file with JOIN_RULES=F1 to see them
fail against the guards published at 3020900; the default is the F2 proposal.
A failing assertion prints the full event trace.
"""

import contextlib
import hashlib
import inspect
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
import multiprocessing
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import join_activation as ja
import join_jitter_study as jj

RULES = os.environ.get("JOIN_RULES", "F2")


def cfg(**kwargs):
    return ja.Cfg(rules=RULES, **kwargs)


class TraceCase(unittest.TestCase):
    def check(self, sim, ok, text):
        if not ok:
            self.fail("%s\n--- event trace ---\n%s\n--- violations: %r" % (
                text, sim.dump(), sim.violations))


def first_seq(kind, direction, **kw):
    return ja.first_seq(cfg(**kw), kind, direction)


def in_order(sim):
    """True when each (src, dst, channel, transport epoch) delivered channel-0 sequences in increasing order."""
    last = {}
    for e in sim.events:
        if e["kind"] != "deliver" or e["ch"] != 0:
            continue
        key = (e["src"], e["dst"], e["epoch"])
        if e["seq"] <= last.get(key, -1):
            return False
        last[key] = e["seq"]
    return True


def owners_everywhere(sim, seat):
    """Ownership of `seat` on every peer that is still replaying (not restarted or cancelled)."""
    return {name: p.owners.get(seat) for name, p in sorted(sim.peers.items())
            if hasattr(p, "owners") and getattr(p, "state", "") != "restarted"}


def summary(sim, name="J1"):
    return ja.summary(sim, name)


def validate_clean(sim, **kw):
    """The independent validators of join_activation.validate(), from the raw event log."""
    return ja.validate_sim(sim, **kw)


class RoundCbRegressions(TraceCase):
    def _stale_checks(self, sim):
        j = sim.joiners["J1"]
        self.check(sim, j.active_ms is not None, "joiner never became active")
        applied, received = j.active_cursors
        self.check(sim, received - applied <= sim.cfg.input_lag_max,
                   "input opened at applied=%d received=%d" % (applied, received))
        self.check(sim, j.freed == [], "unapplied turns freed: %r" % (j.freed,))
        self.check(sim, j.applied_log == list(range(1, len(j.applied_log) + 1)),
                   "applied turns are not contiguous")

    def test_a1_turn_delayed_after_h_never_opens_stale_input(self):
        # The F1 row: turn 12's first copy takes 2000 ms longer than the others.
        delays = {("Turn", "H>J1", 12, 1): 2000}
        self._stale_checks(ja.one_join(cfg(), delays=delays, until=9000))

    def test_a2_turn_lost_after_h_never_opens_stale_input(self):
        # Turn 12 (channel-0 sequence 12) is lost four times, so every later
        # message on that pair waits behind it until the fifth copy.
        losses = {("Turn", "H>J1", 12, a) for a in (1, 2, 3, 4)}
        self._stale_checks(ja.one_join(cfg(), losses=losses, until=9000))

    def test_b1_retried_report_blocks_the_later_watermark(self):
        # The first report is lost four times. The copy that gets through is sent
        # later and arrives after the watermark for turn 20 was sent at 2160. On
        # the same ordered pair and channel that watermark cannot be delivered first.
        seq = first_seq("HashReport", "J1>H")
        losses = {("HashReport", "J1>H", seq, a) for a in (1, 2, 3, 4)}
        sim = ja.one_join(cfg(), losses=losses, until=6000)
        jr = sim.host.joins["J1"]
        self.check(sim, in_order(sim), "channel 0 delivered a sequence before an earlier one")
        self.check(sim, sim.violations == [], "violations recorded")
        self.check(sim, jr.accept_ms is not None and jr.accept_ms >= 2510,
                   "accepted at %s, before the earlier report could be delivered" % jr.accept_ms)

    def test_b2_lost_watermark_is_retransmitted_on_channel_zero(self):
        seq = first_seq("JoinWatermark", "J1>H")
        losses = {("JoinWatermark", "J1>H", seq, 1)}
        sim = ja.one_join(cfg(), losses=losses, until=6000)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.watermarks and jr.watermarks[0][0] == 1610,
                   "watermark was not delivered by its channel-0 retransmission at 1610: %r"
                   % (jr.watermarks,))

    def test_c_endless02_early_candidate_is_selected(self):
        # Hash turn 1770 is applied at 178350 and its watermark arrives at 178500,
        # before the 179000 deadline of its root. It is the first selectable
        # candidate; an exchange that starts only at 1790 skips it.
        sim = ja.one_join(cfg(), until=182000, snapshot_ms=160650)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.selected and jr.selected[0] == (178500, 1770),
                   "first selection %r, watermarks near the end: %r" % (
                       jr.selected[:2], [w for w in jr.watermarks if 178000 < w[0] < 180000]))
        self.check(sim, ja.Cfg().deadline_for(1770) == 179000, "deadline arithmetic")


class OwnershipAndInput(TraceCase):
    def test_normal_row_values(self):
        sim = ja.row_normal(cfg())
        jr = sim.host.joins["J1"]
        j = sim.joiners["J1"]
        self.check(sim, (jr.H, jr.accept_ms, jr.A) == (10, 1610, 18), "accept row %r" % ((jr.H, jr.accept_ms, jr.A),))
        self.check(sim, j.active_ms == 2560 and j.active_cursors == (24, 24), "input row %r at %s" % (
            j.active_cursors, j.active_ms))
        self.check(sim, sim.violations == [] and sim.root_conflicts() == [], "violations or conflicts")

    def test_ownership_is_replayed_with_input_still_off(self):
        sim = ja.row_normal(cfg())
        j = sim.joiners["J1"]
        lines = [l for l in sim.trace if "replays its own seat" in l]
        self.check(sim, len(lines) == 1, "joiner did not replay its seat transition")
        replay_ms = int(lines[0].split()[0])
        self.check(sim, j.active_ms > replay_ms, "input opened before or at the seat transition")
        self.check(sim, set(owners_everywhere(sim, 2).values()) == {"human"}, "owners differ")

    def test_stale_grants_are_rejected_not_latched(self):
        # With a lag limit of 0 a grant that arrives while the joiner holds a backlog
        # is rejected; input opens only on a later fresh grant, if at all.
        sim = ja.one_join(cfg(input_lag_max=0), until=9000)
        j = sim.joiners["J1"]
        self.check(sim, j.active_ms is None or j.active_cursors[1] == j.active_cursors[0],
                   "input opened with a backlog: %r" % (j.active_cursors,))

    def test_unapplied_turns_survive_activation(self):
        sim = ja.row_post_h_hole(cfg())
        j = sim.joiners["J1"]
        self.check(sim, j.freed == [] and j.applied_log == list(range(1, len(j.applied_log) + 1)),
                   "backlog lost or reordered")
        # Every turn the joiner has received is either applied or still buffered.
        self.check(sim, sorted(j.turns) == list(range(j.P + 1, j.R + 1)), "buffer %r" % sorted(j.turns)[:5])

    def test_restart_after_a_releases_the_seat_everywhere(self):
        # Input never becomes fresh within input_ready_ms: restart after A was published.
        sim = ja.one_join(cfg(input_ready_ms=500), until=6000)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.restart_reason == 5 and jr.released_at is not None,
                   "expected a restart for input that never became fresh: %r" % (summary(sim),))
        self.check(sim, sim.host.owners.get(2) is None and sim.seated[0].owners.get(2) is None,
                   "seat 2 not restored to its previous (empty) owner: %r" % owners_everywhere(sim, 2))
        self.check(sim, sim.root_conflicts() == [], "roots differ")

    def test_cancel_after_a_is_published_releases_the_seat(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.cancel("J1", 1700)  # accepted at 1610, turn 18 is not yet produced
        sim.run(5000)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.accept_ms == 1610 and jr.restart_reason == 8, "state %r" % (summary(sim),))
        self.check(sim, sim.host.owners.get(2) is None and sim.seated[0].owners.get(2) is None,
                   "seat not restored to its previous (empty) owner on every peer")
        self.check(sim, sim.root_conflicts() == [], "roots differ")

    def test_late_control_for_a_dead_attempt_is_ignored(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.cancel("J1", 1700)
        sim.run(5000)
        self.check(sim, sim.host.joins["J1"].state == "dead", "attempt not dead")
        self.check(sim, sim.joiners["J1"].state == "restarted", "joiner not stopped")
        self.check(sim, sim.joiners["J1"].active_ms is None, "a dead attempt opened input")

    def test_duplicate_event_ids_are_dropped(self):
        sim = ja.Sim(cfg())
        peer = sim.seated[0]
        peer.apply_events([(1, 2, "human")])
        peer.apply_events([(1, 2, "ai")])
        self.check(sim, peer.owners[2] == "human" and peer.hw == 1, "duplicate event applied")


class OverlappingJoins(TraceCase):
    def _same_everywhere(self, sim, seats):
        for seat in seats:
            self.check(sim, set(owners_everywhere(sim, seat).values()) == {"human"},
                       "seat %d differs: %r" % (seat, owners_everywhere(sim, seat)))
        self.check(sim, sim.root_conflicts() == [], "roots differ at %r" % sim.root_conflicts()[:3])
        self.check(sim, sim.violations == [], "transport violations")

    def test_two_joins_one_generation(self):
        sim = ja.row_two_joins_one_generation(cfg())
        self.check(sim, sim.host.joins["J1"].gen == sim.host.joins["J2"].gen, "different generations")
        for name in ("J1", "J2"):
            self.check(sim, sim.joiners[name].active_ms is not None, name + " not active")
        self._same_everywhere(sim, (3, 4))
        for name, other in (("J1", 4), ("J2", 3)):
            self.check(sim, sim.joiners[name].owners.get(other) == "human", name + " lacks the other seat")

    def test_two_joins_two_generations_keep_both_pins(self):
        sim = ja.row_two_joins_two_generations(cfg())
        self.check(sim, sim.host.joins["J1"].gen != sim.host.joins["J2"].gen, "same generation")
        for name in ("J1", "J2"):
            self.check(sim, sim.joiners[name].active_ms is not None, name + " not active")
        self.check(sim, sim.host.joins["J2"].accept_ms < sim.joiners["J1"].load_done, "joins did not overlap")
        self._same_everywhere(sim, (3, 4))
        self.check(sim, sim.host.max_slots <= sim.cfg.root_slots + sim.cfg.max_root_pins + 1,
                   "host root slots %d" % sim.host.max_slots)

    def test_expiry_and_restart_from_a_snapshot_before_a(self):
        sim = ja.row_expiry_and_restart(cfg())
        j1, j2, j2b = sim.host.joins["J1"], sim.host.history[-1], sim.host.joins["J2"]
        self.check(sim, j2.attempt == 1 and j2.restart_reason == 8 and j2b.attempt == 2 and j2b.S < j1.A,
                   "the second ask did not start before A: %r" % ((j2b.S, j1.A),))
        self.check(sim, sim.joiners["J2"].active_ms is not None, "J2 not active on its second attempt")
        self._same_everywhere(sim, (3, 4))

    def test_snapshot_after_a_covers_the_event(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.add_join("J3", 3000, 3)  # snapshot at turn 30, after A=18
        sim.run(8000)
        j3 = sim.joiners["J3"]
        self.check(sim, j3.S > sim.host.joins["J1"].A and j3.owners[2] == "human", "snapshot lacks the event")
        self.check(sim, j3.hw >= 1 and sim.root_conflicts() == [], "roots differ")

    def test_all_report_losses_restart_that_joiner_only(self):
        losses = {("HashReport", "J2>H", None, a) for a in range(1, 9)}
        sim = ja.Sim(cfg(), losses=losses, seated=2)
        sim.add_join("J1", 0, 3)
        sim.add_join("J2", 0, 4)
        sim.run(9000)
        self.check(sim, sim.joiners["J1"].active_ms is not None, "J1 should be active")
        self.check(sim, sim.host.joins["J2"].restart_reason in (1, 4), "J2 not restarted: %r" % (summary(sim, "J2"),))
        self.check(sim, sim.host.applied >= 80, "the table stopped")


class Deadlines(TraceCase):
    def test_exact_deadline_does_not_accept_and_does_not_restart(self):
        # Deadline 3600 = root produced at 1000 + 2 hash periods + 2 round trips of 300 ms.
        sim = ja.row_deadline_collision(cfg(), True)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.expired[:1] == [(3600, 10)], "expired %r" % (jr.expired,))
        self.check(sim, jr.restart_ms is None and jr.H == 20 and jr.accept_ms == 3900,
                   "the join did not go on with a later checkpoint: %r" % (summary(sim),))

    def test_one_ms_earlier_accepts(self):
        sim = ja.row_deadline_collision(cfg(), False)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.accept_ms == 3599 and jr.A == 37 and jr.expired == [], repr(summary(sim)))

    def test_all_control_lost_ends_in_a_join_only_restart(self):
        sim = ja.row_all_control_lost(cfg())
        jr = sim.host.joins["J1"]
        self.check(sim, jr.restart_ms is not None and sim.host.applied >= 80, "no restart or table stopped")

    def test_missing_turn_at_or_before_h_never_selects(self):
        sim = ja.row_missing_turn_before_h(cfg())
        jr = sim.host.joins["J1"]
        self.check(sim, jr.selected == [] and jr.accept_ms is None, "selected %r" % (jr.selected,))
        self.check(sim, sim.joiners["J1"].active_ms is None, "input opened on a short prefix")


class TransportAccounting(TraceCase):
    def test_every_named_row_has_no_transport_violation(self):
        for name, fn in ja.ROWS.items():
            sim = fn(cfg())
            self.check(sim, sim.violations == [], name)
            self.check(sim, in_order(sim), name + " delivered out of order")
            self.check(sim, sim.root_conflicts() == [], name + " roots differ")
            self.check(sim, validate_clean(sim) == [], "%s: %r" % (name, validate_clean(sim)[:3]))

    def test_window_of_32_holds_with_fragmented_turns_and_blocked_acks(self):
        # A 1216-byte command burst every turn is two datagrams (1200 + 56 payload),
        # so 24 turn periods want 48 sequences. The window of 32 stops at 32.
        sim = ja.Sim(cfg(wait_cap=64), seated=1)
        sim.turn_extra = lambda k: 1216
        sim.blocked_acks.add("P1>H")
        sim.run(2400)
        key = ("H", "P1", 0)
        chan = sim.chan(*key)
        self.check(sim, sim.host.applied == 24, "turns produced %d" % sim.host.applied)
        self.check(sim, sim.max_span[key] == 32, "span %r" % sim.max_span)
        self.check(sim, chan.next_seq == 32, "sequences used %d" % chan.next_seq)
        # S1: the backlog is not copied into a queue; the pair's cursor stands at turn 17 (16 turns x
        # 2 fragments = 32 datagrams sent) and turns 17..24 are still in the host's log.
        self.check(sim, len(chan.wait) == 0 and chan.log_next == 17, "waiting %d cursor %r" % (
            len(chan.wait), chan.log_next))
        self.check(sim, sim.stats[("H", "P1")]["seq"] == 32, "seq statistic %r" % sim.stats[("H", "P1")])
        self.check(sim, sim.violations == [], "violations")
        self.check(sim, sorted(sim.host.log_turns) == list(range(1, 25)), "turn log")

    def test_overflow_restarts_the_joiner_and_keeps_the_table_running(self):
        sim = ja.Sim(cfg(wait_cap=8, catchup_ms=100000), seated=1)
        sim.turn_extra = lambda k: 1216
        sim.blocked_acks.add("J1>H")
        sim.add_join("J1", 0, 2)
        sim.run(5000)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.restart_reason in (4, 7), "restart %r" % (summary(sim),))
        self.check(sim, sim.host.applied == 50, "the table paused or stopped: turn %d" % sim.host.applied)
        self.check(sim, len(sim.host.log_turns) == 50, "authoritative turns lost")
        self.check(sim, sim.seated[0].applied >= 48, "the seated peer was starved")

    def test_ack_only_traffic_is_counted_in_both_directions(self):
        sim = ja.row_two_joins_one_generation(cfg())
        for a, b in (("H", "J1"), ("J1", "H"), ("H", "J2"), ("J2", "H"), ("H", "P1"), ("P1", "H")):
            self.check(sim, sim.stats[(a, b)]["bytes"] > 0, "no traffic %s>%s" % (a, b))
        self.check(sim, sim.stats[("P1", "H")]["ack"] > 0 and sim.stats[("J1", "H")]["ack"] > 0, "no ACK-only")

    def test_ack_only_does_not_consume_a_sequence(self):
        sim = ja.row_normal(cfg())
        c = sim.chan("J1", "H", 0)
        self.check(sim, c.next_seq <= 7 and sim.stats[("J1", "H")]["ack"] > c.next_seq, "ack %d seq %d" % (
            sim.stats[("J1", "H")]["ack"], c.next_seq))

    def test_root_caches_stay_bounded(self):
        sim = ja.row_endless02(cfg())
        cap = sim.cfg.root_slots + sim.cfg.max_root_pins
        self.check(sim, sim.host.max_slots <= cap, "host %d" % sim.host.max_slots)
        self.check(sim, sim.joiners["J1"].max_root_slots <= cap, "joiner %d" % sim.joiners["J1"].max_root_slots)

    def test_queue_caps_arithmetic(self):
        # Every datagram carries the u16 epoch field: 25 + 2 header bytes, 28 IP/UDP.
        caps = ja.queue_caps(cfg())
        self.assertEqual(caps["datagram"], 1255)
        self.assertEqual(caps["ch0_in_flight_per_pair"], 40160)
        self.assertEqual(caps["ch0_waiting_per_pair"], 80320)
        self.assertEqual(caps["ch0_host"], 602400)
        self.assertEqual(caps["ch1_host"], 80320)
        self.assertEqual(caps["ch2_host"], 650)
        self.assertEqual(caps["host_total"], 683370)
        # 4 watermarks (77), 2 reports (77), 2 InputReqs (71), InputOn (61), JoinAbort (63).
        self.assertEqual(caps["joiner_reverse"], 4 * 77 + 2 * 77 + 2 * 71 + 61 + 63)
        self.assertEqual(caps["ack_kbit_per_s_ch0_and_ch1"], 880.0)

    def test_wire_sizes(self):
        self.assertEqual(ja.turn_bytes([]), 16)
        self.assertEqual(ja.turn_bytes([(1, 2, "human")]), 16 + 14)
        self.assertEqual(ja.HEADER + ja.EPOCH_BYTES + ja.IP_UDP, 55)
        self.assertEqual(ja.WIRE, 55)


# ---- round cc (F3) regressions: written first, shown failing at 5ea1f34 ----------------------
# These use only what a test can observe without the checker's own bookkeeping: the joiner's
# input-open time and cursors, and the host turn computed from the clock (the host produces
# turn k at k * turn_ms), as the oracle.

ORACLE_LAG = 8  # host turn minus joiner applied turn at the millisecond input opens (input_lag_max 6 + 2)
DOMAIN_END = 650000
DOMAIN_STEP_MAX = 60
LATENCY_MAX = 1500  # one-way ms; the window of 32 datagrams per round trip ends it, see the next test
ADR_RESTART_SNAPSHOT_TURN = 0  # the restarted J2 shares generation 1, whose snapshot is turn 0


def host_lag_at_open(sim, name="J1"):
    j = sim.joiners[name]
    if j.active_ms is None:
        return None
    return j.active_ms // sim.cfg.turn_ms - j.active_cursors[0]


class RoundCcJoinSide(TraceCase):
    def _opens_fresh(self, sim, text):
        lag = host_lag_at_open(sim)
        self.check(sim, lag is not None, text + ": input never opened")
        self.check(sim, lag <= ORACLE_LAG, text + ": input opened with host - applied = %d" % lag)
        self.check(sim, validate_clean(sim, oracle_lag=ORACLE_LAG) == [],
                   text + ": validators %r" % (validate_clean(sim, oracle_lag=ORACLE_LAG)[:3],))

    # A joiner that replays slowly still has a backlog when the hole fills, so the freshness
    # gates are what keeps input shut; at 10 ms per turn the round trip alone hides the defect.
    REPLAY_STEPS = (10, 40, 60, 80)

    def test_b1_input_grant_lost_seven_times_cannot_open_stale_input(self):
        for step in self.REPLAY_STEPS:
            seq = ja.first_seq(cfg(replay_step_ms=step), "InputGrant", "H>J1")
            losses = {("InputGrant", "H>J1", seq, a) for a in range(1, 8)}
            self._opens_fresh(ja.one_join(cfg(replay_step_ms=step), losses=losses, until=20000),
                              "grant lost 1-7, replay %d ms" % step)

    def test_b1_input_grant_lost_four_times_cannot_open_stale_input(self):
        for step in self.REPLAY_STEPS:
            seq = ja.first_seq(cfg(replay_step_ms=step), "InputGrant", "H>J1")
            losses = {("InputGrant", "H>J1", seq, a) for a in range(1, 5)}
            self._opens_fresh(ja.one_join(cfg(replay_step_ms=step), losses=losses, until=20000),
                              "grant lost 1-4, replay %d ms" % step)

    def test_b1_turn_before_the_grant_lost_seven_times_cannot_open_stale_input(self):
        for step in self.REPLAY_STEPS:
            seq = ja.first_seq(cfg(replay_step_ms=step), "InputGrant", "H>J1") - 1
            losses = {("Turn", "H>J1", seq, a) for a in range(1, 8)}
            self._opens_fresh(ja.one_join(cfg(replay_step_ms=step), losses=losses, until=20000),
                              "turn %d lost 1-7, replay %d ms" % (seq, step))

    def test_b2_grant_names_a_turn_already_sent(self):
        # Boundary configurations of the round-cc scan (watermark delivered on a turn boundary).
        for one_way, step in ((145, 10), (140, 20), (195, 10), (190, 20)):
            c = cfg(one_way_ms=one_way, replay_step_ms=step, resend_ms=2 * one_way)
            sim = ja.one_join(c, until=25000, snapshot_ms=one_way)
            jr = sim.host.joins["J1"]
            self.check(sim, sim.joiners["J1"].active_ms is not None and jr.restart_ms is None,
                       "one-way %d, replay step %d: %r" % (one_way, step, summary(sim)))
            self.check(sim, validate_clean(sim) == [], "validators %r" % (validate_clean(sim)[:3],))

    def test_b2_loss_free_latency_by_replay_step_scan(self):
        bad = []
        for one_way in range(100, 201, 5):
            for step in (10, 20):
                c = cfg(one_way_ms=one_way, replay_step_ms=step, resend_ms=2 * one_way)
                sim = ja.one_join(c, until=25000, snapshot_ms=one_way)
                if sim.joiners["J1"].active_ms is None or sim.host.joins["J1"].restart_ms is not None \
                        or validate_clean(sim):
                    bad.append((one_way, step, summary(sim)["restart"], validate_clean(sim)[:1]))
        self.assertEqual(bad, [])

    def test_b3_endless02_replay_steps_do_not_restart(self):
        # The domain: replay at least 1.64 times faster than the table (60 ms per 100 ms turn or
        # less), so that endless02's 1 606-turn backlog is gone before the pin passes tail_turns_max
        # (turn 4096, 409 600 ms). See below.
        steps = sorted(set(range(1, DOMAIN_STEP_MAX + 1)) | {15, 24, 25, 29, 30, 34, 40, 45, 50})
        bad = []
        for step in steps:
            sim = ja.Sim(cfg(replay_step_ms=step))
            sim.add_join("J1", 0, 2, snapshot_ms=160650)
            sim.run(DOMAIN_END)
            jr = sim.host.joins["J1"]
            if jr.restart_ms is not None or sim.joiners["J1"].active_ms is None or validate_clean(sim):
                bad.append((step, jr.restart_ms, jr.restart_reason, validate_clean(sim)[:1]))
        self.assertEqual(bad, [])

    def test_b3_endless02_beyond_the_domain_ends_in_the_catchup_limit_only(self):
        # Replay slower than 60 ms per turn cannot clear the backlog before the pin passes
        # tail_turns_max: the oldest pin is restarted (reason 13) when turn 4097 would be kept, and
        # the table keeps running.
        for step in (61, 74, 80, 100):
            sim = ja.Sim(cfg(replay_step_ms=step))
            sim.add_join("J1", 0, 2, snapshot_ms=160650)
            sim.run(DOMAIN_END)
            jr = sim.host.joins["J1"]
            self.check(sim, jr.restart_reason == ja.REASON_TAIL and jr.restart_ms == 409700, "step %d: %r" % (step, summary(sim)))
            self.check(sim, sim.host.applied == DOMAIN_END // 100, "the table stopped")

    def test_b3_the_latency_limit_is_the_window_not_the_deadline(self):
        # At 2 000 ms one way the round trip is 4 s: 40 turns per round trip want 40 sequences
        # against the window of 32, so the pair delivers 8 turns a second against 10 produced. The
        # joiner never reaches the host (no selection) and ends in the catch-up restart (reason 6);
        # the log-backed cursor keeps every turn. The deadline and input_ready_ms do not end it.
        c = cfg(one_way_ms=2000, resend_ms=4000)
        sim = ja.one_join(c, until=DOMAIN_END, snapshot_ms=2000)
        self.check(sim, sim.host.joins["J1"].restart_reason == ja.REASON_TAIL, repr(summary(sim)))
        self.check(sim, sim.host.applied == DOMAIN_END // 100, "the table stopped")

    def test_b3_expiry_of_one_checkpoint_reselects_instead_of_restarting(self):
        # The first report is lost nine times (a limit of twenty sends keeps the pair alive), so
        # everything behind it on the ordered channel waits. Its checkpoint expires; when the hole
        # fills, a later watermark selects a later hash turn and the join completes. No restart.
        seq = first_seq("HashReport", "J1>H")
        losses = {("HashReport", "J1>H", seq, a) for a in range(1, 10)}
        sim = ja.one_join(cfg(max_attempts=20), losses=losses, until=20000)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.expired and jr.restart_ms is None and sim.joiners["J1"].active_ms is not None,
                   "expired %r, restart or no input: %r" % (jr.expired, summary(sim),))
        self.check(sim, len(jr.selected) >= 2 and jr.H > jr.expired[0][1],
                   "no re-selection above the expired checkpoint: %r" % (jr.selected,))
        self.check(sim, validate_clean(sim) == [], "validators")

    def test_b3_normal_row_across_latencies(self):
        bad = []
        for one_way in list(range(0, 451, 25)) + [500, 600, 900, 1200, LATENCY_MAX]:
            c = cfg(one_way_ms=one_way, resend_ms=max(2 * one_way, 100))
            sim = ja.one_join(c, until=60000, snapshot_ms=max(one_way, 1))
            if sim.joiners["J1"].active_ms is None or sim.host.joins["J1"].restart_ms is not None \
                    or validate_clean(sim):
                bad.append((one_way, summary(sim)["restart"], validate_clean(sim)[:1]))
        self.assertEqual(bad, [])


class RoundCcSeats(TraceCase):
    def _no_conflicts(self, sim):
        self.check(sim, sim.root_conflicts() == [], "roots differ")

    def test_p10a_two_joins_for_one_seat_only_one_is_admitted(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.add_join("J2", 0, 2)
        sim.run(6000)
        active = [n for n in ("J1", "J2") if sim.joiners[n].active_ms is not None]
        self.check(sim, len(active) == 1, "joiners with input on seat 2: %r" % (active,))
        self.check(sim, sim.host.owners.get(2) == "human", "seat 2 owner %r" % sim.host.owners.get(2))
        self.check(sim, validate_clean(sim) == [], "validators %r" % (validate_clean(sim)[:3],))
        self._no_conflicts(sim)

    def test_p10b_a_refused_joiner_cancelling_does_not_release_the_other_seat(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.add_join("J2", 0, 2)
        sim.cancel("J2", 2300)
        sim.run(6000)
        self.check(sim, sim.joiners["J1"].state == "active" and sim.host.owners.get(2) == "human",
                   "J1 %s, seat 2 %r" % (sim.joiners["J1"].state, sim.host.owners.get(2)))
        self.check(sim, sim.seated[0].owners.get(2) == "human", "seated peer sees %r" % sim.seated[0].owners.get(2))
        self._no_conflicts(sim)

    def test_p10c_a_join_on_a_seated_humans_seat_is_refused(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 1)  # seat 1 is P1's
        sim.run(25000)
        self.check(sim, sim.joiners["J1"].active_ms is None and sim.host.owners.get(1) == "human",
                   "join on P1's seat: %r, owner %r" % (summary(sim), sim.host.owners.get(1)))
        self.check(sim, not [l for l in sim.trace if "owner seat 1 -> ai" in l], "the seat was given to the AI")
        self.check(sim, validate_clean(sim) == [], "validators %r" % (validate_clean(sim)[:3],))

    def test_a_failed_join_on_an_empty_seat_does_not_create_an_ai(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.cancel("J1", 1700)  # after A was published, before turn A
        sim.run(5000)
        self.check(sim, sim.host.owners.get(2) in (None, "empty") and sim.seated[0].owners.get(2) in (None, "empty"),
                   "seat 2 after the cancel: host %r seated %r" % (sim.host.owners.get(2), sim.seated[0].owners.get(2)))

    def test_a_failed_join_on_an_ai_seat_returns_it_to_the_ai(self):
        sim = ja.Sim(cfg(), seated=1)
        for p in sim.peers.values():
            p.owners[3] = "ai"
        sim.add_join("J1", 0, 3)
        sim.cancel("J1", 1700)
        sim.run(5000)
        self.check(sim, sim.host.owners.get(3) == "ai" and sim.seated[0].owners.get(3) == "ai",
                   "seat 3 after the cancel: host %r seated %r" % (sim.host.owners.get(3), sim.seated[0].owners.get(3)))


class RoundCcSamePeer(TraceCase):
    def test_s3_the_same_peer_can_ask_again_after_a_restart(self):
        sim = ja.Sim(cfg(), seated=2)
        sim.add_join("J1", 0, 3)
        sim.add_join("J2", 100, 4)
        sim.cancel("J2", 1000)
        sim.add_join("J2", 1200, 4)
        sim.run(30000)
        j2 = sim.joiners["J2"]
        self.check(sim, j2.active_ms is not None and sim.host.owners.get(4) == "human",
                   "J2 did not join on its second ask: %r" % (summary(sim, "J2"),))
        self.check(sim, sim.joiners["J1"].active_ms is not None, "J1 not active")
        self.check(sim, sim.root_conflicts() == [], "roots differ")

    def test_s3_the_restart_row_snapshot_turn_matches_the_adr(self):
        sim = ja.row_expiry_and_restart(cfg())
        jr = sim.host.joins["J2"]
        self.check(sim, jr.attempt == 2, "attempt %r" % (jr.attempt,))
        self.check(sim, jr.S == ADR_RESTART_SNAPSHOT_TURN, "snapshot turn %r" % (jr.S,))

    def test_grant_race_input_never_opens_after_the_host_restarted_the_attempt(self):
        for ready in range(640, 661):
            sim = ja.one_join(cfg(input_ready_ms=ready), until=8000)
            j, jr = sim.joiners["J1"], sim.host.joins["J1"]
            late = j.active_ms is not None and jr.restart_ms is not None and j.active_ms > jr.restart_ms
            self.check(sim, not late, "input_ready_ms=%d: input on at %s after the restart at %s" % (
                ready, j.active_ms, jr.restart_ms))
            if jr.restart_ms is not None:
                self.check(sim, j.state == "restarted", "input_ready_ms=%d: joiner state %s" % (ready, j.state))


# ---- the independent validators, on hand-written logs and under mutants (F3 step 1) -----------
def ev(kind, t=0, **kw):
    kw.update(kind=kind, t=t)
    return kw


def codes(events, **kw):
    return sorted({c for c, _ in ja.validate(events, ja.Cfg(), 0, **kw)})


class ValidatorsOnHandWrittenLogs(unittest.TestCase):
    """Each validator fires on a log written by hand: none of them reads a peer or a Chan."""

    def arrive(self, seq, t=1):
        return ev("arrive", t, src="H", dst="P1", ch=0, seq=seq, msg="Turn", dup=False, turn=seq)

    def deliver(self, seq, t=2):
        return ev("deliver", t, src="H", dst="P1", ch=0, seq=seq, msg="Turn", turn=seq)

    def test_i_delivery_past_a_hole(self):
        log = [self.arrive(0), self.arrive(2), self.deliver(2)]
        self.assertEqual(codes(log), ["i"])
        self.assertEqual(codes([self.arrive(0), self.arrive(1), self.deliver(0), self.deliver(1)]), [])

    def test_i_delivery_twice(self):
        log = [self.arrive(0), self.deliver(0), self.deliver(0)]
        self.assertEqual(codes(log), ["i"])

    def test_ii_window_span_from_send_records(self):
        sends = [ev("send", i, src="H", dst="P1", ch=0, seq=i, msg="Turn", attempt=1, lost=False,
                    turn=i, C=None, size=10) for i in range(33)]
        self.assertEqual(codes(sends), ["ii"])
        acked = sends[:1] + [ev("ack", 1, src="H", dst="P1", ch=0, seqs=(0,))] + sends[1:]
        self.assertEqual(codes(acked), [])

    def test_iii_tables_must_agree_at_every_applied_turn(self):
        a = ev("apply", peer="H", turn=3, owners=((0, "human"),), root="r")
        b = ev("apply", peer="P1", turn=3, owners=((0, "human"), (2, "ai")), root="r2")
        self.assertEqual(codes([a, b]), ["iii"])
        self.assertEqual(codes([a, dict(b, owners=a["owners"], root="r")]), [])

    def test_iv_a_silent_stall_and_a_logged_outage(self):
        log = [ev("seated", peer="P1", seat=1)] + [ev("produce", turn=k) for k in range(1, 41)]
        log += [ev("apply", peer="P1", turn=3, owners=(), root="r")]
        self.assertIn("iv", codes(log))
        open_ = ev("outage", peer="P1", src="H", dst="P1", ch=0, age=2000)
        self.assertNotIn("iv", codes(log + [open_]))
        # An outage on another path (channel 1, or the reverse direction) excuses nothing.
        other = ev("outage", peer="P1", src="H", dst="P1", ch=1, age=2000)
        self.assertIn("iv", codes(log + [other]))
        reverse = ev("outage", peer="P1", src="P1", dst="H", ch=0, age=2000)
        self.assertIn("iv", codes(log + [reverse]))

    def test_v_input_lag_at_the_instant_and_later_in_the_millisecond(self):
        log = [ev("produce", turn=k, t=k) for k in range(1, 31)]
        opened = [ev("input_on", 30, peer="J1", applied=20, received=20, known=20)]
        self.assertEqual(codes(log + opened, oracle_lag=8), ["v"])
        self.assertEqual(codes(log + opened, oracle_lag=10), [])
        # Within the bound at the instant, over it after a later event of the same millisecond.
        ok = [ev("produce", turn=k, t=k) for k in range(1, 29)]
        late = ok + [ev("input_on", 28, peer="J1", applied=20, received=20, known=20),
                     ev("produce", 28, turn=29)]
        self.assertEqual(codes(late, oracle_lag=8), ["v"])

    def test_vi_two_attempts_or_a_seated_human_hold_a_seat(self):
        init = ev("seat_init", seats=((0, "human"), (1, "human")))
        self.assertEqual(codes([init, ev("admit", name="J1", attempt=1, seat=1, gen=1, S=0)]), ["vi"])
        two = [init, ev("admit", name="J1", attempt=1, seat=2, gen=1, S=0),
               ev("admit", name="J2", attempt=1, seat=2, gen=1, S=0)]
        self.assertEqual(codes(two), ["vi"])
        freed = [init, ev("admit", name="J1", attempt=1, seat=2, gen=1, S=0),
                 ev("attempt_end", name="J1", attempt=1, how="dead"),
                 ev("admit", name="J2", attempt=1, seat=2, gen=1, S=0)]
        self.assertEqual(codes(freed), [])

    def test_vii_a_produced_turn_must_be_delivered_or_retained(self):
        # Nothing delivered, nothing retained: the absence of an error event is not evidence.
        bare = [ev("seated", peer="P1", seat=1), ev("produce", turn=1), ev("produce", turn=2)]
        self.assertEqual(codes(bare), ["vii"])
        # Retained but not yet sent (a healthy backlog behind the window): clean.
        kept = [ev("seated", peer="P1", seat=1), ev("retain", turn=1, bytes=1200, total_bytes=1200, count=1),
                ev("produce", turn=1), ev("retain", turn=2, bytes=1200, total_bytes=2400, count=2),
                ev("produce", turn=2)]
        self.assertEqual(codes(kept), [])
        refused = kept + [ev("overflow", src="H", dst="P1", ch=0, msg="Turn", turn=2)]
        self.assertEqual(codes(refused), ["vii"])
        self.assertEqual(codes(kept + [ev("abandon", src="H", dst="P1", ch=0, seq=1)]), ["vii"])
        # Pruned too early: released while the live peer has neither it nor a send of it.
        self.assertEqual(codes(kept + [ev("release", turn=1)]), ["vii"])
        arrived = ev("arrive", src="H", dst="P1", ch=0, seq=0, msg="Turn", dup=False, turn=1)
        delivered = ev("deliver", src="H", dst="P1", ch=0, seq=0, msg="Turn", turn=1)
        self.assertEqual(codes(kept + [arrived, delivered, ev("release", turn=1)]), [])
        # A dropped seat is no longer live; a rejoin makes it live again from its snapshot turn.
        dropped = kept + [ev("drop_seated", peer="P1")]
        self.assertEqual(codes(dropped), [])
        rejoin = dropped + [ev("admit", name="P1", attempt=2, seat=1, gen=1, S=1), ev("release", turn=2)]
        self.assertEqual(codes(rejoin), ["vii"])
        retained_again = dropped + [ev("admit", name="P1", attempt=2, seat=1, gen=1, S=1)]
        self.assertEqual(codes(retained_again), [])

    def test_iv_a_recovered_outage_does_not_excuse_a_later_silent_stall(self):
        log = [ev("seated", peer="P1", seat=1), ev("outage", 1000, peer="P1", src="H", dst="P1", ch=0, age=2000),
               ev("outage_end", 4000, peer="P1", src="H", dst="P1", ch=0)] + [ev("produce", 100000, turn=1000)]
        self.assertIn("iv", codes(log))
        # The same stall with the outage still open at the end is explicit.
        open_ = [ev("seated", peer="P1", seat=1), ev("outage", 1000, peer="P1", src="H", dst="P1", ch=0, age=2000),
                 ev("produce", 1500, turn=1000)]
        self.assertNotIn("iv", codes(open_))

    def test_x_the_retained_set_is_bounded_from_retain_and_release_events(self):
        cfg = ja.Cfg(tail_turns_max=3)
        log = [ev("retain", turn=k, bytes=1200, total_bytes=0, count=0) for k in range(1, 5)]
        self.assertEqual(sorted({c for c, _ in ja.validate(log, cfg, 0)}), ["x"])
        log2 = log[:3] + [ev("release", turn=1)] + log[3:]
        self.assertNotIn("x", {c for c, _ in ja.validate(log2, cfg, 0)})
        spanned = [ev("admit", name="J1", attempt=1, seat=2, gen=1, S=0), ev("retain", turn=4, bytes=1200, total_bytes=0, count=0)]
        self.assertIn("x", {c for c, _ in ja.validate(spanned, cfg, 0)})

    def test_xi_epoch_labels_and_stale_datagrams(self):
        bump = ev("epoch_bump", a="H", b="J1", epoch=1, why="re-ask")
        send0 = ev("send", src="H", dst="J1", ch=0, seq=0, msg="Turn", attempt=1, lost=False, turn=1, C=None, size=1, sid=1, epoch=0)
        late = ev("arrive", src="H", dst="J1", ch=0, seq=0, msg="Turn", dup=False, turn=1, sid=1, epoch=0)
        self.assertIn("xi", {c for c, _ in ja.validate([send0, bump, late], ja.Cfg(), 0)})
        # A reused label: the bump happened but the datagram still carries the old number.
        relabeled = dict(late, epoch=1)
        self.assertIn("xi", {c for c, _ in ja.validate([send0, bump, relabeled], ja.Cfg(), 0)})
        fresh = dict(send0, sid=2, epoch=1)
        ok = [bump, fresh, dict(late, sid=2, epoch=1)]
        self.assertNotIn("xi", {c for c, _ in ja.validate(ok, ja.Cfg(), 0)})

    def test_ix_an_outage_must_end_or_drop(self):
        log = [ev("outage", 1000, peer="P1", src="H", dst="P1", ch=0, age=2000)]
        self.assertIn("ix", {c for c, _ in ja.validate(log, ja.Cfg(), 70000)})
        ended = log + [ev("outage_end", 5000, peer="P1", src="H", dst="P1", ch=0)]
        self.assertNotIn("ix", {c for c, _ in ja.validate(ended, ja.Cfg(), 70000)})
        # Another path recovering does not close this one.
        wrong = log + [ev("outage_end", 5000, peer="P1", src="H", dst="P1", ch=1)]
        self.assertIn("ix", {c for c, _ in ja.validate(wrong, ja.Cfg(), 70000)})

    def test_viii_a_grant_must_name_a_turn_already_sent(self):
        grant = ev("send", 5, src="H", dst="J1", ch=0, seq=9, msg="InputGrant", attempt=1, lost=False,
                   turn=None, C=7, size=14)
        sent = ev("send", 4, src="H", dst="J1", ch=0, seq=8, msg="Turn", attempt=1, lost=False,
                  turn=7, C=None, size=14)
        self.assertEqual(codes([grant]), ["viii"])
        self.assertEqual(codes([sent, grant]), [])


MUTANT_TABLE = []  # (name, patch factory, tests that must fail under it)


def _patch_deliver_past_holes():
    def arrive(self, src, dst, ch, d):
        c = self.recv_state(src, dst, ch)
        seq = d["seq"]
        dup = seq in c.recv
        c.recv.setdefault(seq, d)
        c.highest = max(c.highest, seq)
        self.ev("arrive", src=src, dst=dst, ch=ch, seq=seq, msg=d["kind"], dup=dup, turn=d.get("turn"))
        if not dup:
            self.peers[dst].on_arrive(src, ch, d)
        if ch == 2:
            if not dup and d["msg"] is not None:
                self.deliver(src, dst, ch, seq, d["msg"])
        else:
            while c.deliv <= c.highest:
                nxt = c.recv.get(c.deliv)
                if nxt is not None and nxt["msg"] is not None and not nxt.get("delivered"):
                    nxt["delivered"] = True
                    self.deliver(src, dst, ch, c.deliv, nxt["msg"])
                c.deliv += 1
        if ch != 2 and not c.ack_pending:
            c.ack_pending = True
            self.schedule(self.t, ja.P_FLUSH, self.flush_ack, src, dst, ch)
    return mock.patch.object(ja.Sim, "arrive", arrive)


def _patch_window_plus_one():
    def room(self, c):
        oldest = min(c.out) if c.out else c.next_seq
        return c.next_seq - oldest < self.cfg.window + 1
    return mock.patch.object(ja.Sim, "window_room", room)


def _patch_no_freshness_gate():
    def req(self, jr, msg):
        if jr.state == "accepted" and msg["applied"] >= jr.A:
            jr.go_ms = self.sim.t
            self.sim.send("H", jr.name, 0, "InputGo", 14, gen=jr.gen, attempt=jr.attempt, nonce=msg["nonce"])
    stack = contextlib.ExitStack()
    stack.enter_context(mock.patch.object(ja.Host, "on_input_req", req))
    stack.enter_context(mock.patch.object(ja.Joiner, "fresh", lambda self: True))
    return stack


def _patch_release_to_ai():
    orig = ja.Host.admit

    def admit(self, name, seat, snapshot_ms, load_ms):
        orig(self, name, seat, snapshot_ms, load_ms)
        jr = self.joins.get(name)
        if jr is not None and jr.state != "dead":
            jr.prev_owner = "ai"
    return mock.patch.object(ja.Host, "admit", admit)


def _patch_c_one_ahead():
    return mock.patch.object(ja.Host, "current", lambda self: self.applied + 1)


def _patch_no_seat_exclusivity():
    return mock.patch.object(ja.Host, "seat_holder", lambda self, seat, name: None)


MUTANTS = {
    "deliver past holes": (_patch_deliver_past_holes, [
        "TransportAccounting.test_every_named_row_has_no_transport_violation",
        "OwnershipAndInput.test_unapplied_turns_survive_activation"]),
    "window + 1": (_patch_window_plus_one, [
        "TransportAccounting.test_window_of_32_holds_with_fragmented_turns_and_blocked_acks"]),
    "grant freshness gate removed": (_patch_no_freshness_gate, [
        "RoundCcJoinSide.test_b1_input_grant_lost_seven_times_cannot_open_stale_input",
        "RoundCcJoinSide.test_b1_input_grant_lost_four_times_cannot_open_stale_input",
        "RoundCcJoinSide.test_b1_turn_before_the_grant_lost_seven_times_cannot_open_stale_input"]),
    "release always to ai": (_patch_release_to_ai, [
        "RoundCcSeats.test_a_failed_join_on_an_empty_seat_does_not_create_an_ai",
        "OwnershipAndInput.test_restart_after_a_releases_the_seat_everywhere"]),
    "C one turn ahead": (_patch_c_one_ahead, [
        "RoundCcJoinSide.test_b2_grant_names_a_turn_already_sent",
        "OwnershipAndInput.test_normal_row_values"]),
    "no seat exclusivity": (_patch_no_seat_exclusivity, [
        "RoundCcSeats.test_p10a_two_joins_for_one_seat_only_one_is_admitted",
        "RoundCcSeats.test_p10c_a_join_on_a_seated_humans_seat_is_refused"]),
}


def failing_under(patch, names):
    """Names (among `names`) of the tests that fail while `patch` is applied."""
    loader = unittest.TestLoader()
    module = sys.modules[__name__]
    failed = []
    with patch():
        for name in names:
            suite = loader.loadTestsFromName(name, module)
            result = unittest.TestResult()
            suite.run(result)
            if result.failures or result.errors:
                failed.append(name)
    return failed


class MutantTable(unittest.TestCase):
    """Every mutant of the F3 list is caught by the named tests of the suite (the report's table)."""

    def test_every_mutant_is_caught_by_each_named_test(self):
        for mutant, (patch, names) in MUTANTS.items():
            failed = failing_under(patch, names)
            self.assertEqual(failed, names, "mutant %r: caught only by %r" % (mutant, failed))

    def test_the_named_tests_pass_without_a_mutant(self):
        loader = unittest.TestLoader()
        for _, (_, names) in MUTANTS.items():
            for name in names:
                result = unittest.TestResult()
                loader.loadTestsFromName(name, sys.modules[__name__]).run(result)
                self.assertTrue(result.wasSuccessful(), name)


# ---- round cc (F4) regressions ---------------------------------------------------------------
def delivered_turns(sim, dst):
    return {e["turn"] for e in sim.events if e["kind"] == "deliver" and e["dst"] == dst and e["msg"] == "Turn"}


class RoundCcOutages(TraceCase):
    def _caught_up(self, sim, peer, slack=4):
        self.check(sim, sim.host.applied - peer.applied <= slack,
                   "%s applied %d, host %d" % (peer.name, peer.applied, sim.host.applied))

    def test_b4_a_three_second_two_way_cut_does_not_drop_the_peer_and_it_catches_up(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.cuts.append(("P1", 1000, 4000))
        sim.run(12000)
        p1 = sim.seated[0]
        self.check(sim, not [e for e in sim.events if e["kind"] == "abandon"], "the pair was abandoned")
        self.check(sim, [e for e in sim.events if e["kind"] == "outage"], "no logged outage for a 3 s cut")
        self.check(sim, [e for e in sim.events if e["kind"] == "outage_end"], "the outage never ended")
        self._caught_up(sim, p1)
        self.check(sim, set(range(1, p1.applied + 1)) <= delivered_turns(sim, "P1"), "a turn was not delivered")
        self.check(sim, validate_clean(sim) == [] and sim.root_conflicts() == [], repr(validate_clean(sim)[:3]))

    def test_b4_a_600_ms_cut_with_a_50_ms_resend_does_not_abandon(self):
        sim = ja.Sim(cfg(resend_ms=50), seated=1)
        sim.cuts.append(("P1", 1000, 1600))
        sim.run(8000)
        self.check(sim, not [e for e in sim.events if e["kind"] == "abandon"], "abandoned")
        self._caught_up(sim, sim.seated[0])
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_b4_losing_only_the_acks_never_abandons_and_ends_in_a_logged_outage(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.blocked_acks.add("P1>H")
        sim.schedule(9000, 0, lambda: sim.blocked_acks.discard("P1>H"))
        sim.run(16000)
        self.check(sim, not [e for e in sim.events if e["kind"] == "abandon"], "abandoned on lost ACKs")
        self.check(sim, [e for e in sim.events if e["kind"] == "outage"], "the stall was silent")
        self._caught_up(sim, sim.seated[0])
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_b4_turn_a_lost_eight_times_to_a_seated_peer_ends_with_equal_ownership(self):
        base = ja.Sim(cfg(), seated=2)
        base.add_join("J1", 0, 3)
        base.run(6000)
        A = base.host.joins["J1"].A
        seq = next(e["seq"] for e in base.events if e["kind"] == "send" and e["dst"] == "P2" and e["msg"] == "Turn"
                   and e["turn"] == A and e["attempt"] == 1)
        sim = ja.Sim(cfg(), losses={("Turn", "H>P2", seq, a) for a in range(1, 9)}, seated=2)
        sim.add_join("J1", 0, 3)
        sim.run(12000)
        p2 = sim.seated[1]
        self.check(sim, p2.owners.get(3) == "human" and sim.host.owners.get(3) == "human",
                   "P2 sees seat 3 as %r, host %r, P2 applied %d" % (p2.owners.get(3), sim.host.owners.get(3), p2.applied))
        self._caught_up(sim, p2)
        self.check(sim, validate_clean(sim) == [] and sim.root_conflicts() == [], repr(validate_clean(sim)[:3]))

    def test_s7_a_small_wait_cap_never_loses_a_turn_of_a_seated_peer(self):
        sim = ja.Sim(cfg(wait_cap=8), seated=1)
        sim.turn_extra = lambda k: 1216
        sim.blocked_acks.add("P1>H")
        sim.schedule(4000, 0, lambda: sim.blocked_acks.discard("P1>H"))
        sim.run(12000)
        self.check(sim, sim.violations == [], "queue violations %r" % (sim.violations[:2],))
        self._caught_up(sim, sim.seated[0])
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_the_burst_row_runs_past_2500_ms_without_an_abandonment(self):
        sim = ja.row_burst_acks_blocked(cfg(), periods=60)
        self.check(sim, not [e for e in sim.events if e["kind"] == "abandon"], "abandoned")
        outage = [e for e in sim.events if e["kind"] == "outage"]
        self.check(sim, outage and 2000 <= outage[0]["t"] <= 2400, "outage record %r" % (outage[:1],))
        self.check(sim, sim.max_span[("H", "P1", 0)] == 32, "span %r" % sim.max_span)

    def test_beyond_the_grace_the_seat_goes_to_the_ai_on_every_peer(self):
        sim = ja.Sim(cfg(), seated=2)
        sim.cuts.append(("P1", 1000, 90000))
        sim.run(80000)
        drops = [e for e in sim.events if e["kind"] == "drop_seated"]
        self.check(sim, len(drops) == 1 and drops[0]["peer"] == "P1", "drops %r" % (drops,))
        self.check(sim, 61000 <= drops[0]["t"] <= 63000, "dropped at %d" % drops[0]["t"])
        self.check(sim, sim.host.owners.get(1) == "ai" and sim.seated[1].owners.get(1) == "ai",
                   "seat 1: host %r, P2 %r" % (sim.host.owners.get(1), sim.seated[1].owners.get(1)))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))


class RoundCcSnapshotChannel(TraceCase):
    def test_s1_a_joiner_sharing_a_generation_completes_wherever_it_asks(self):
        for ask in (9500, 9700, 20000, 50000):
            sim = ja.Sim(cfg(), seated=2)
            sim.add_join("J1", 0, 3, snapshot_ms=65000)
            sim.add_join("J2", ask, 4)
            sim.run(ask + 40000)
            j2, jr2 = sim.joiners["J2"], sim.host.joins["J2"]
            self.check(sim, jr2.gen == sim.host.joins["J1"].gen and jr2.S == 0, "ask %d: not shared %r" % (ask, summary(sim, "J2")))
            self.check(sim, j2.active_ms is not None and jr2.restart_ms is None, "ask %d: %r" % (ask, summary(sim, "J2")))
            self.check(sim, sim.violations == [] and validate_clean(sim) == [], "ask %d: %r" % (ask, validate_clean(sim)[:3]))

    def test_s4_a_lost_joingrant_is_resent_on_channel_1(self):
        sim = ja.one_join(cfg(), losses=ja.lose("JoinGrant", "H>J1", (1, 2)), until=9000)
        self.check(sim, sim.joiners["J1"].active_ms is not None, repr(summary(sim)))
        grants = [e for e in sim.events if e["kind"] == "send" and e["msg"] == "JoinGrant"]
        self.check(sim, [g["attempt"] for g in grants] == [1, 2, 3] and all(g["ch"] == 1 for g in grants), repr(grants))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_s4_snapshot_fragments_are_datagrams_with_loss_and_resend(self):
        base = ja.Sim(cfg(), seated=1)
        base.add_join("J1", 0, 2, fragments=5)
        base.run(9000)
        sim = ja.Sim(cfg(), losses=ja.lose("Snap-frag", "H>J1", (1,)), seated=1)
        sim.add_join("J1", 0, 2, fragments=5)
        sim.run(9000)
        loaded = lambda x: next(e["t"] for e in x.events if e["kind"] == "deliver" and e["msg"] == "SnapshotDone")
        self.check(sim, loaded(sim) > loaded(base), "the lost fragment did not delay the load: %d vs %d" % (loaded(sim), loaded(base)))
        self.check(sim, sim.joiners["J1"].active_ms is not None, repr(summary(sim)))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_s4_endless02_is_simulated_per_datagram(self):
        # 17 130 fragments through the window of 32 at a 300 ms round trip: 535.3 windows.
        sim = ja.Sim(cfg())
        sim.add_join("J1", 0, 2, fragments=17130)
        sim.run(DOMAIN_END)
        j = sim.joiners["J1"]
        done = next(e["t"] for e in sim.events if e["kind"] == "deliver" and e["msg"] == "SnapshotDone")
        self.check(sim, 160000 <= done <= 162000, "snapshot delivered at %d ms" % done)
        self.check(sim, j.active_ms is not None and sim.host.joins["J1"].restart_ms is None, repr(summary(sim)))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_s4_the_third_concurrent_transfer_is_refused_before_anything_is_reserved(self):
        sim = ja.Sim(cfg(), seated=3)
        for i, name in enumerate(("J1", "J2", "J3")):
            sim.add_join(name, 0, 4 + i)
        sim.run(8000)
        jr3 = sim.host.joins["J3"]
        self.check(sim, jr3.restart_reason == ja.REASON_NO_SLOT and jr3.gen == 0, repr(summary(sim, "J3")))
        self.check(sim, all(sim.joiners[n].active_ms is not None for n in ("J1", "J2")), "J1/J2 did not complete")
        self.check(sim, sim.host.owners.get(6) is None, "the refused seat was published")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_the_receive_backlog_cap_restarts_that_joiner_only(self):
        sim = ja.Sim(cfg(receive_backlog_turns=5), seated=1)
        sim.add_join("J1", 0, 2, snapshot_ms=3000)
        sim.run(8000)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.restart_reason == ja.REASON_BACKLOG, repr(summary(sim)))
        self.check(sim, sim.host.applied == 80 and sim.seated[0].applied >= 76, "the table was disturbed")


class RoundCcAccounting(TraceCase):
    def test_s6_seated_upstream_traffic_and_both_channels_acks_are_counted(self):
        sim = ja.Sim(cfg(seated_cmd_bytes=40), seated=1)
        sim.add_join("J1", 0, 2)
        sim.run(6000)
        st = sim.stats
        self.check(sim, st[("P1", "H")]["data"] > 50, "seated -> host data %r" % st[("P1", "H")])
        self.check(sim, st[("H", "P1")]["ack"] > 50, "host -> seated ACK-only %r" % st[("H", "P1")])
        self.check(sim, any(e["kind"] == "send" and e["msg"] == "SeatHash" and e["ch"] == 2 for e in sim.events),
                   "no section-7 hash report from the seated peer")
        self.check(sim, sim.ack_ch[("J1", "H", 1)] > 0 and sim.ack_ch[("H", "J1", 0)] > 0 and sim.ack_ch[("H", "P1", 0)] > 0,
                   "ACKs per channel %r" % (dict(sim.ack_ch),))

    def test_control_messages_have_bounded_outstanding_counts(self):
        seq = first_seq("HashReport", "J1>H")
        sim = ja.one_join(cfg(max_attempts=20), losses={("HashReport", "J1>H", seq, a) for a in range(1, 10)}, until=20000)
        self.check(sim, sim.max_out.get(("J1", "H", "HashReport"), 0) <= sim.cfg.max_outstanding_reports, repr(sim.max_out))
        self.check(sim, sim.max_out.get(("J1", "H", "InputReq"), 0) <= sim.cfg.max_outstanding_req, repr(sim.max_out))
        self.check(sim, sim.max_out.get(("J1", "H", "JoinWatermark"), 0) <= sim.cfg.max_outstanding_wm, repr(sim.max_out))

    def test_two_generations_hold_four_recent_roots_plus_one_pin_per_attempt(self):
        sim = ja.row_two_joins_two_generations(cfg())
        self.check(sim, sim.host.max_slots <= sim.cfg.root_slots + 2, "host slots %d" % sim.host.max_slots)

    def test_join_abort_and_restart_carry_a_reason(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.cancel("J1", 1700)
        sim.run(5000)
        sizes = {e["msg"]: e["size"] for e in sim.events if e["kind"] == "send" and e["msg"] in ("JoinAbort", "JoinRestart")}
        self.assertEqual(sizes, {"JoinAbort": 8, "JoinRestart": 4})


# ---- round cd (F5) regressions: written first, shown failing at d9a58ae -----------------------
def reask_after_losses(losses, ask=4000, clear=3500, until=30000, seated=1, **kw):
    sim = ja.Sim(cfg(**kw), losses=losses, seated=seated)
    sim.add_join("J1", 0, 2)
    sim.schedule(clear, 0, sim.losses.clear)
    sim.add_join("J1", ask, 2)
    sim.run(until)
    return sim


class RoundCdReliableRecovery(TraceCase):
    def _recovered(self, sim, why):
        jr, j = sim.host.joins["J1"], sim.joiners["J1"]
        self.check(sim, jr.attempt == 2 and j.attempt == 2, "%s: attempts host %d joiner %r" % (why, jr.attempt, j.attempt))
        self.check(sim, j.active_ms is not None and jr.restart_ms is None, "%s: %r" % (why, summary(sim)))
        self.check(sim, sim.seated[0].applied >= sim.host.applied - 4, "%s: an unrelated peer stalled" % why)
        self.check(sim, validate_clean(sim) == [] and sim.root_conflicts() == [], "%s: %r" % (why, validate_clean(sim)[:3]))
        self.check(sim, sim.host.owners.get(2) == "human" and sim.seated[0].owners.get(2) == "human", "%s: seat 2" % why)

    def test_r1_abandoned_joingrant_channel_one_host_to_joiner(self):
        sim = reask_after_losses(ja.lose("JoinGrant", "H>J1", range(1, 9)))
        self.check(sim, [e for e in sim.events if e["kind"] == "abandon" and e["ch"] == 1], "channel 1 was not abandoned")
        self._recovered(sim, "ch1 H>J1")

    def test_r1_abandoned_turn_stream_channel_zero_host_to_joiner(self):
        sim = reask_after_losses(ja.lose("Turn", "H>J1", range(1, 9)))
        self.check(sim, [e for e in sim.events if e["kind"] == "abandon" and e["ch"] == 0 and e["dst"] == "J1"],
                   "channel 0 H>J1 was not abandoned")
        self._recovered(sim, "ch0 H>J1")

    def test_r1_abandoned_joiner_to_host_channel_zero(self):
        sim = reask_after_losses(ja.lose("JoinWatermark", "J1>H", range(1, 9)))
        self.check(sim, [e for e in sim.events if e["kind"] == "abandon" and e["src"] == "J1"], "J1>H was not abandoned")
        self._recovered(sim, "ch0 J1>H")

    def test_r1_old_datagrams_acks_and_timers_cannot_enter_the_new_epoch(self):
        # Attempt 1's first turn is delayed 6 s; the peer cancels, asks again, and the old datagram
        # lands inside attempt 2's stream with sequence 0 of its own.
        sim = ja.Sim(cfg(), delays={("Turn", "H>J1", 0, 1): 6000}, seated=1)
        sim.add_join("J1", 0, 2)
        sim.add_join("J1", 1000, 2)  # the same peer asks again while attempt 1 is still running
        sim.run(20000)
        stale = [e for e in sim.events if e["kind"] == "stale_epoch"]
        self.check(sim, any(e["what"] == "arrive" for e in stale), "no stale arrival was rejected: %r" % (stale[:3],))
        self.check(sim, any(e["what"] in ("ack", "resend", "flush") for e in stale), "no stale ACK or timer: %r" % (stale[:3],))
        self._recovered(sim, "late old traffic")
        bumps = [e for e in sim.events if e["kind"] == "epoch_bump"]
        self.check(sim, len(bumps) == 1 and bumps[0]["epoch"] == 1, repr(bumps))

    def test_r1_the_ask_is_one_counted_datagram(self):
        sim = reask_after_losses(ja.lose("JoinGrant", "H>J1", range(1, 9)))
        self.check(sim, sim.stats[("J1", "H")]["bytes"] >= 8 + ja.HEADER + ja.IP_UDP, "the ask is not in the accounting")


class RoundCdRetention(TraceCase):
    SLOW = 10 ** 9  # a labeled snapshot that never arrives: the join only pins

    def test_r2_the_pin_cap_restarts_the_old_join_before_turn_4097_is_kept(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2, snapshot_ms=self.SLOW)
        sim.run(409600)  # turn 4096 is produced: span 4096 still fits
        self.check(sim, sim.host.joins["J1"].restart_ms is None and len(sim.host.log_turns) == 4096,
                   "restarted early or retained %d" % len(sim.host.log_turns))
        sim.run(409700)  # keeping turn 4097 would make the span 4097
        jr = sim.host.joins["J1"]
        self.check(sim, jr.restart_ms == 409700 and jr.restart_reason == ja.REASON_TAIL, repr(summary(sim)))
        self.check(sim, 4097 not in sim.host.log_turns or len(sim.host.log_turns) <= 4096, "turn 4097 kept over the cap")
        self.check(sim, sim.host.peak_turns <= sim.cfg.tail_turns_max, "peak %d" % sim.host.peak_turns)
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_r2_only_the_oldest_generation_is_restarted(self):
        sim = ja.Sim(cfg(), seated=2)
        sim.add_join("J1", 0, 3, snapshot_ms=self.SLOW)
        sim.add_join("J2", 62000, 4, snapshot_ms=self.SLOW)  # past snapshot_age_ms: its own generation, S = 619
        sim.run(410000)
        j1, j2 = sim.host.joins["J1"], sim.host.joins["J2"]
        self.check(sim, j1.gen != j2.gen, "same generation")
        self.check(sim, j1.restart_reason == ja.REASON_TAIL and j2.restart_ms is None,
                   "J1 %r J2 %r" % (j1.restart_reason, j2.restart_ms))
        self.check(sim, len(sim.host.log_turns) <= sim.cfg.tail_turns_max, "retained %d" % len(sim.host.log_turns))
        sim.run(480000)
        self.check(sim, sim.host.joins["J2"].restart_reason == ja.REASON_TAIL, "J2 not restarted at its own cap")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_r2_a_shared_generation_keeps_its_pin_when_one_joiner_cancels(self):
        sim = ja.Sim(cfg(), seated=2)
        sim.add_join("J1", 0, 3, snapshot_ms=self.SLOW)
        sim.add_join("J2", 200, 4, snapshot_ms=self.SLOW)
        sim.cancel("J1", 100000)
        sim.run(200000)
        self.check(sim, sim.host.joins["J2"].S == 0 and sim.host.joins["J1"].S == 0, "not shared")
        self.check(sim, len(sim.host.log_turns) == 2000, "the pin of the remaining joiner was released: %d turns" % len(sim.host.log_turns))
        sim.run(410000)
        self.check(sim, sim.host.joins["J2"].restart_reason == ja.REASON_TAIL, repr(sim.host.joins["J2"].restart_reason))

    def test_r2_a_long_healthy_run_retains_only_the_rolling_log(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.run(700000)
        host = sim.host
        self.check(sim, len(host.log_turns) == sim.cfg.turn_buffer and sorted(host.log_turns)[0] == 7000 - 599,
                   "retained %d from %d" % (len(host.log_turns), min(host.log_turns)))
        self.check(sim, host.peak_turns <= sim.cfg.turn_buffer + 1 and host.peak_bytes <= (sim.cfg.turn_buffer + 1) * 1200,
                   "peak %d turns %d bytes" % (host.peak_turns, host.peak_bytes))
        self.check(sim, sim.seated[0].applied >= 6996, "the seated peer fell behind")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_r2_the_byte_cap_and_the_per_turn_charge(self):
        # A 1 216-byte burst makes each turn 1 232 bytes, above the nominal 1 200: charged what it is.
        c = cfg(tail_bytes_max=1000 * 1232)
        sim = ja.Sim(c, seated=1)
        sim.turn_extra = lambda k: 1216
        sim.add_join("J1", 0, 2, snapshot_ms=self.SLOW)
        sim.run(100100)
        self.check(sim, set(sim.host.turn_charge.values()) == {1232}, repr(set(sim.host.turn_charge.values())))
        jr = sim.host.joins["J1"]
        self.check(sim, jr.restart_ms == 100100 and jr.restart_reason == ja.REASON_TAIL, repr(summary(sim)))
        self.check(sim, sim.host.peak_bytes <= c.tail_bytes, "peak %d" % sim.host.peak_bytes)
        plain = ja.Sim(cfg(), seated=1)
        plain.run(1000)
        self.check(sim, set(plain.host.turn_charge.values()) == {1200}, "nominal charge")

    def test_r2_receive_and_generation_state_is_bounded_and_history_is_separate(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.run(60000)
        biggest = max(len(c.recv) for c in sim.chans.values())
        self.check(sim, biggest <= 80, "receive state holds %d sequences" % biggest)
        self.check(sim, len(sim.host.gen_state) <= sim.cfg.max_generations, "generation images %d" % len(sim.host.gen_state))
        self.check(sim, len(sim.host.root_sent) <= sim.cfg.root_slots + sim.cfg.max_root_pins + 2,
                   "root bookkeeping %d" % len(sim.host.root_sent))


class RoundCdValidatorsOnRealRuns(TraceCase):
    def _scenario_cut(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.cuts.append(("P1", 1000, 9000))  # 80 turns: more than the window of 32 can hold in flight
        sim.run(16000)
        return sim

    def _scenario_open_outage(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.cuts.append(("P1", 1000, 40000))
        sim.run(40000)
        return sim

    def _scenario_reask(self):
        return reask_after_losses(ja.lose("JoinGrant", "H>J1", range(1, 9)))

    def test_retention_holds_on_a_cut_and_a_long_run(self):
        sim = self._scenario_cut()
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_the_open_outage_is_the_explicit_state(self):
        sim = self._scenario_open_outage()
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_epochs_hold_across_a_reask(self):
        sim = self._scenario_reask()
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))


def _patch_skip_a_turn():
    orig = ja.Sim.next_datagram

    def next_datagram(self, c):
        if c.log_next == 50 and c.frag == 0 and c is self.chans.get(("H", "P1", 0)):
            c.log_next = 51  # the turn is never sent to P1
        return orig(self, c)
    return mock.patch.object(ja.Sim, "next_datagram", next_datagram)


def _patch_prune_too_early():
    def release(self, floor):
        for q in [q for q in self.log_turns if q < self.applied - 5]:
            self.sim.ev("release", turn=q)  # evidence says released; data kept so the run goes on
            self.log_turns_released = getattr(self, "log_turns_released", set()) | {q}
    return mock.patch.object(ja.Host, "release", release)


def _patch_suppress_outage():
    def check_outage(self, src, dst, ch, c, d):
        return None
    return mock.patch.object(ja.Sim, "check_outage", check_outage)


def _patch_reuse_epoch():
    def reset_pair(self, a, b, why):
        for key in list(self.chans):
            x, y = (key[1], key[2]) if key[0] == "recv" else (key[0], key[1])
            if self.pair_key(x, y) == self.pair_key(a, b):
                del self.chans[key]
        self.ev("epoch_bump", a=a, b=b, epoch=self.epoch_of(a, b), why=why)  # the old number again
    return mock.patch.object(ja.Sim, "reset_pair", reset_pair)


MUTANTS.update({
    "skip a required turn": (_patch_skip_a_turn, [
        "RoundCdValidatorsOnRealRuns.test_retention_holds_on_a_cut_and_a_long_run"]),
    "prune one too early": (_patch_prune_too_early, [
        "RoundCdValidatorsOnRealRuns.test_retention_holds_on_a_cut_and_a_long_run"]),
    "suppress a current outage": (_patch_suppress_outage, [
        "RoundCdValidatorsOnRealRuns.test_the_open_outage_is_the_explicit_state"]),
    "reuse an old transport epoch": (_patch_reuse_epoch, [
        "RoundCdValidatorsOnRealRuns.test_epochs_hold_across_a_reask",
        "RoundCdReliableRecovery.test_r1_old_datagrams_acks_and_timers_cannot_enter_the_new_epoch"]),
})


# ---- round ce (F6) regressions: written first, shown failing at 8e2493a ------------------------
def _pair_after_reask(**kw):
    """J1 joins at 0 and asks again at 1000 (epoch 1); returns the sim run to 3000 ms."""
    sim = ja.Sim(cfg(**kw), seated=1)
    sim.add_join("J1", 0, 2)
    sim.add_join("J1", 1000, 2)
    sim.run(3000)
    return sim


def _old_datagram(**extra):
    d = {"kind": "Turn", "size": 16, "msg": {"kind": "Turn", "turn": 7, "events": ()}, "attempt": 1, "acked": False,
         "turn": 7, "C": None, "seq": 0, "first": 0, "sid": 99999, "epoch": 0, "frag": 0, "nfrag": 1}
    d.update(extra)
    return d


class RoundCeWireEpoch(TraceCase):
    def test_e1_identical_sequence_numbers_differ_in_the_packet_field(self):
        sim = _pair_after_reask()
        zero = [e for e in sim.events if e["kind"] == "send" and e["src"] == "H" and e["dst"] == "J1"
                and e["ch"] == 0 and e["seq"] == 0]
        self.check(sim, {e["epoch"] for e in zero} >= {0, 1}, "sequence 0 only seen in epochs %r" % ({e["epoch"] for e in zero},))
        self.check(sim, ja.EPOCH_BYTES == 2 and ja.WIRE == ja.HEADER + 2 + ja.IP_UDP, "the field is not counted")

    def test_e1_a_stale_data_packet_is_rejected_by_its_field_alone(self):
        sim = _pair_after_reask()
        before = len(sim.events)
        sim.arrive("H", "J1", 0, _old_datagram(epoch=0))  # no timer epoch is passed: only the packet's field
        new = sim.events[before:]
        self.check(sim, [e for e in new if e["kind"] == "stale_epoch" and e["what"] == "arrive"], "not rejected")
        self.check(sim, not [e for e in new if e["kind"] in ("arrive", "deliver")], "the old packet was accepted")

    def test_e1_a_stale_ack_is_rejected_by_its_field_alone(self):
        sim = _pair_after_reask()
        c = sim.chan("H", "J1", 0)
        c.out[500] = {"seq": 500, "attempt": 1, "first": sim.t, "kind": "Turn", "size": 1}
        sim.take_ack("H", "J1", 0, 500, 0, 0)  # an ACK-only datagram whose field says epoch 0
        self.check(sim, 500 in c.out, "a stale ACK freed a sequence of the new stream")
        self.check(sim, [e for e in sim.events if e["kind"] == "stale_epoch" and e["what"] == "ack"], "not logged")
        sim.take_ack("H", "J1", 0, 500, 0, sim.ep_of("J1", "H"))  # the current epoch is accepted
        self.check(sim, 500 not in c.out, "the current ACK was not accepted")

    def test_e1_stale_timers_are_local_and_rejected_separately(self):
        sim = _pair_after_reask()
        c = sim.chan("H", "J1", 0)
        c.out[501] = {"seq": 501, "attempt": 1, "first": sim.t, "kind": "Turn", "size": 1, "msg": None, "turn": None,
                      "C": None, "sid": 5, "epoch": 1}
        sent = len([e for e in sim.events if e["kind"] == "send"])
        sim.resend("H", "J1", 0, 501, 0)  # a resend timer of epoch 0
        sim.flush_ack("H", "J1", 0, 0)  # a flush timer of epoch 0
        whats = {e["what"] for e in sim.events if e["kind"] == "stale_epoch"}
        self.check(sim, {"resend", "flush"} <= whats, repr(whats))
        self.check(sim, len([e for e in sim.events if e["kind"] == "send"]) == sent, "a stale timer sent a datagram")

    def test_e1_bytes_of_the_epoch_field_and_the_ask_are_counted(self):
        sim = _pair_after_reask()
        for a, b in (("H", "J1"), ("J1", "H"), ("H", "P1"), ("P1", "H")):
            data = sum(e["size"] + ja.WIRE for e in sim.events if e["kind"] == "send" and e["src"] == a and e["dst"] == b)
            acks = sim.ack_ch_total(a, b) if hasattr(sim, "ack_ch_total") else sum(
                n for (x, y, ch), n in sim.ack_ch.items() if x == a and y == b)
            ask = (ja.ASK_PAYLOAD + ja.WIRE) if (a, b) == ("J1", "H") else 0
            self.check(sim, sim.stats[(a, b)]["bytes"] == data + acks * ja.WIRE + ask,
                       "%s>%s bytes %d != %d" % (a, b, sim.stats[(a, b)]["bytes"], data + acks * ja.WIRE + ask))

    def test_e1_a_late_old_packet_never_enters_the_new_stream_in_a_run(self):
        sim = ja.Sim(cfg(), delays={("Turn", "H>J1", 0, 1): 6000}, seated=1)
        sim.add_join("J1", 0, 2)
        sim.add_join("J1", 1000, 2)
        sim.run(20000)
        self.check(sim, [e for e in sim.events if e["kind"] == "stale_epoch" and e["what"] == "arrive"], "no stale arrival")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))


def _timeline_e2(**kw):
    sim = ja.Sim(cfg(**kw), seated=1)
    sim.add_join("J1", 0, 2)
    sim.cuts.append(("J1", 6000, 80000))
    sim.cuts.append(("J1", 92000, 95000))
    sim.add_join("J1", 85000, 2)
    sim.run(110000)
    return sim


class RoundCeDropRejoin(TraceCase):
    def test_e2_the_rejoined_peer_survives_the_next_short_cut(self):
        sim = _timeline_e2()
        j1 = sim.joiners["J1"]
        self.check(sim, [e for e in sim.events if e["kind"] == "drop_seated" and e["peer"] == "J1"], "never dropped")
        self.check(sim, "J1" not in sim.dropped, "the old drop still classifies the peer")
        self.check(sim, j1.active_ms is not None and sim.host.joins["J1"].attempt == 2, repr(summary(sim)))
        self.check(sim, not [e for e in sim.events if e["kind"] == "abandon" and e["t"] > 85000], "abandoned after the rejoin")
        self.check(sim, sim.host.applied - j1.P <= 4, "J1 is %d turns behind" % (sim.host.applied - j1.P))
        self.check(sim, [e for e in sim.events if e["kind"] == "outage" and e["t"] > 92000], "no outage for the 3 s cut")
        self.check(sim, [e for e in sim.events if e["kind"] == "outage_end" and e["t"] > 92000], "the outage did not end")
        self.check(sim, sim.host.owners.get(2) == "human" and sim.seated[0].owners.get(2) == "human", "seat 2 differs")
        self.check(sim, validate_clean(sim) == [] and sim.root_conflicts() == [], repr(validate_clean(sim)[:3]))

    def test_e2_input_is_closed_when_the_seat_is_dropped(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.cuts.append(("J1", 6000, 80000))
        sim.run(80000)
        drop = next(e for e in sim.events if e["kind"] == "drop_seated")
        off = [e for e in sim.events if e["kind"] == "input_off" and e["peer"] == "J1" and e["t"] <= drop["t"]]
        self.check(sim, off and sim.joiners["J1"].state == "restarted", "input stayed open: %r" % (off,))
        self.check(sim, "xii" not in {c for c, _ in validate_clean(sim)}, repr(validate_clean(sim)[:3]))

    def test_e2_a_second_grace_expiry_drops_the_rejoined_peer_again(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.cuts.append(("J1", 6000, 80000))
        sim.add_join("J1", 85000, 2)
        sim.cuts.append(("J1", 100000, 400000))
        sim.run(200000)
        drops = [e for e in sim.events if e["kind"] == "drop_seated" and e["peer"] == "J1"]
        self.check(sim, len(drops) == 2 and drops[1]["t"] > 100000, repr(drops))
        self.check(sim, sim.host.owners.get(2) == "ai" and sim.seated[0].owners.get(2) == "ai", "seat 2 is not the AI")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_e2_another_peer_can_take_the_vacated_seat(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.cuts.append(("J1", 6000, 400000))
        sim.add_join("J2", 70000, 2)
        sim.run(90000)
        self.check(sim, [e for e in sim.events if e["kind"] == "drop_seated" and e["peer"] == "J1"], "J1 not dropped")
        self.check(sim, sim.joiners["J2"].active_ms is not None, repr(summary(sim, "J2")))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_e2_late_old_traffic_after_the_rejoin_is_rejected(self):
        sim = ja.Sim(cfg(), delays={("Turn", "H>J1", 3, 1): 90000}, seated=1)
        sim.add_join("J1", 0, 2)
        sim.cuts.append(("J1", 6000, 80000))
        sim.add_join("J1", 85000, 2)
        sim.run(100000)
        self.check(sim, [e for e in sim.events if e["kind"] == "stale_epoch"], "nothing stale arrived")
        self.check(sim, sim.joiners["J1"].active_ms is not None and validate_clean(sim) == [], repr(validate_clean(sim)[:3]))


class RoundCeBudgets(TraceCase):
    SLOW = 10 ** 9

    def test_e3_seven_fragment_turns_never_pass_the_byte_cap(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.turn_extra = lambda k: 8300
        sim.run(70000)
        host = sim.host
        self.check(sim, host.peak_bytes <= sim.cfg.tail_bytes, "peak %d > %d" % (host.peak_bytes, sim.cfg.tail_bytes))
        self.check(sim, host.retained_bytes <= sim.cfg.tail_bytes and len(host.log_turns) < sim.cfg.turn_buffer,
                   "the rolling window did not shrink: %d turns" % len(host.log_turns))
        self.check(sim, set(host.turn_charge.values()) == {8316}, repr(set(host.turn_charge.values())))
        self.check(sim, not [e for e in sim.events if e["kind"] == "drop_seated"], "a healthy peer was dropped")
        self.check(sim, sim.seated[0].applied >= host.applied - 4, "the seated peer fell behind")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_e3_byte_boundary_exactly_n_turns(self):
        sim = ja.Sim(cfg(tail_bytes_max=10 * 1200), seated=1)
        sim.run(5000)
        self.check(sim, len(sim.host.log_turns) == 10 and sim.host.retained_bytes == 12000,
                   "%d turns %d bytes" % (len(sim.host.log_turns), sim.host.retained_bytes))

    def test_e3_a_stalled_seated_cursor_is_dropped_at_the_cap_not_discarded(self):
        sim = ja.Sim(cfg(tail_bytes_max=50 * 1200), seated=2)
        sim.cuts.append(("P1", 1000, 40000))
        sim.run(12000)
        drops = [e for e in sim.events if e["kind"] == "drop_seated"]
        self.check(sim, len(drops) == 1 and drops[0]["peer"] == "P1" and drops[0]["why"] == "log cap", repr(drops))
        self.check(sim, drops[0]["t"] < 1000 + sim.cfg.grace_ms, "dropped at the grace, not at the cap")
        self.check(sim, sim.host.peak_bytes <= sim.cfg.tail_bytes, "peak %d" % sim.host.peak_bytes)
        self.check(sim, sim.host.owners.get(1) == "ai" and sim.seated[1].owners.get(1) == "ai", "seat 1 is not the AI")
        self.check(sim, sim.seated[1].applied >= sim.host.applied - 4, "the other peer fell behind")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_e3_an_oversized_single_turn_rejects_its_commands(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.turn_extra = lambda k: 6000000 if k == 5 else 0
        sim.run(3000)
        self.check(sim, [e for e in sim.events if e["kind"] == "oversized_turn" and e["turn"] == 5], "not detected")
        self.check(sim, ("oversized_turn", 5) == sim.violations[0][:2], repr(sim.violations))
        self.check(sim, sim.host.peak_bytes <= sim.cfg.tail_bytes and sim.seated[0].applied >= sim.host.applied - 4, "cap or peer")
        self.check(sim, 5 in sim.host.log_turns and sim.host.turn_charge[5] == 1200, "the empty turn was not kept")

    def test_e3_unneeded_rolling_entries_do_not_cause_a_restart(self):
        # Cap of 100 turns. A join asking at turn 300 pins from 301: it fits until turn 400 and is
        # restarted when turn 401 would be kept, not earlier because of turns below 301.
        sim = ja.Sim(cfg(tail_bytes_max=100 * 1200, tail_turns_max=100), seated=1)
        sim.add_join("J1", 30000, 2, snapshot_ms=self.SLOW)
        sim.run(40000)
        self.check(sim, sim.host.joins["J1"].restart_ms is None, "restarted early: %r" % (summary(sim),))
        sim.run(40100)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.restart_ms == 40100 and jr.restart_reason == ja.REASON_TAIL, repr(summary(sim)))

    def test_e3_max_generations_is_enforced_and_a_shareable_one_is_kept(self):
        sim = ja.Sim(cfg(max_generations=1), seated=2)
        sim.add_join("J1", 0, 3, snapshot_ms=self.SLOW)
        sim.add_join("J2", 30000, 4, snapshot_ms=self.SLOW)  # shareable: allowed although the table is full
        sim.run(31000)
        self.check(sim, sim.host.joins["J2"].gen == sim.host.joins["J1"].gen == 1, "not shared")
        sim = ja.Sim(cfg(max_generations=1), seated=2)
        sim.add_join("J1", 0, 3, snapshot_ms=self.SLOW)
        sim.add_join("J2", 62000, 4, snapshot_ms=self.SLOW)  # past snapshot_age_ms: needs a second generation
        sim.run(63000)
        jr2 = sim.host.joins["J2"]
        self.check(sim, jr2.restart_reason == ja.REASON_NO_GEN and jr2.gen == 0 and len(sim.host.gen_state) == 1, repr(summary(sim, "J2")))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_e3_generation_identities_are_never_reused(self):
        sim = ja.Sim(cfg(), seated=3)
        sim.add_join("J1", 0, 4, snapshot_ms=self.SLOW)
        sim.add_join("J2", 62000, 5, snapshot_ms=self.SLOW)
        sim.cancel("J1", 63000)
        sim.add_join("J3", 124000, 6, snapshot_ms=self.SLOW)
        sim.run(125000)
        gens = {n: sim.host.joins[n].gen for n in ("J1", "J2", "J3")}
        self.check(sim, len(set(gens.values())) == 3, "reused: %r" % (gens,))
        self.check(sim, sorted(sim.host.gen_state) == [gens["J2"], gens["J3"]], "images %r" % (sorted(sim.host.gen_state),))
        self.check(sim, sim.host.gen_state[gens["J2"]] != sim.host.gen_state[gens["J3"]] or
                   sim.host.joins["J2"].S != sim.host.joins["J3"].S, "same snapshot state")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_e3_the_last_owner_frees_the_image_and_the_operational_state_is_bounded(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.run(30000)
        op = sim.host.operational()
        self.check(sim, op["images"] == 0, "an image outlived its last owner: %r" % (op,))
        self.check(sim, op["log_bytes"] <= sim.cfg.tail_bytes and op["in_flight"] <= 3 * 2 * sim.cfg.window, repr(op))
        self.check(sim, op["reassembly"] <= 80 * 6 and op["waiting"] <= sim.cfg.wait_cap * 6, repr(op))


def _hand(*events):
    return list(events)


class RoundCeFragments(TraceCase):
    def _base(self, nfrag=2):
        return [ev("seated", peer="P1", seat=1), ev("retain", turn=1, bytes=2432, total_bytes=2432, count=1, nfrag=nfrag),
                ev("produce", turn=1)]

    def _send(self, frag, seq, nfrag=2, attempt=1):
        return ev("send", src="H", dst="P1", ch=0, seq=seq, msg="Turn" if frag == nfrag - 1 else "Turn-frag", attempt=attempt,
                  lost=False, turn=1, C=None, size=1200, sid=seq + 1, epoch=0, frag=frag, nfrag=nfrag)

    def _arrive(self, frag, seq, nfrag=2):
        return ev("arrive", src="H", dst="P1", ch=0, seq=seq, msg="Turn", dup=False, turn=1, sid=seq + 1, epoch=0,
                  frag=frag, nfrag=nfrag)

    def test_e4_an_early_fragment_is_not_retention_of_the_turn(self):
        log = self._base() + [self._send(0, 0), ev("release", turn=1)]
        self.assertEqual(codes(log), ["vii"])

    def test_e4_all_fragments_in_the_window_are_wholly_held(self):
        log = self._base() + [self._send(0, 0), self._send(1, 1), ev("release", turn=1)]
        self.assertEqual(codes(log), [])

    def test_e4_fragments_split_between_the_window_and_the_receiver_are_wholly_held(self):
        log = self._base() + [self._send(0, 0), self._send(1, 1), self._arrive(0, 0),
                              ev("ack", src="H", dst="P1", ch=0, seqs=(0,), epoch=0), ev("release", turn=1)]
        self.assertEqual(codes(log), [])

    def test_e4_an_acknowledged_fragment_nobody_holds_is_lost(self):
        # ACKed but never recorded as arrived: the ACK alone does not make the receiver the owner.
        log = self._base() + [self._send(0, 0), self._send(1, 1), ev("ack", src="H", dst="P1", ch=0, seqs=(0,), epoch=0),
                              ev("release", turn=1)]
        self.assertEqual(codes(log), ["vii"])

    def test_e4_holdings_do_not_survive_an_epoch_change_or_a_drop(self):
        held = self._base() + [self._send(0, 0), self._send(1, 1)]
        bump = held + [ev("epoch_bump", a="H", b="P1", epoch=1, why="re-ask"), ev("release", turn=1)]
        self.assertIn("vii", codes(bump))
        drop = held + [ev("drop_seated", peer="P1"), ev("admit", name="P1", attempt=2, seat=1, gen=1, S=0), ev("release", turn=1)]
        self.assertIn("vii", codes(drop))

    def test_e4_the_end_of_the_observation_is_checked(self):
        # Released earlier with full coverage is fine; a turn not retained and only half held fails at the end.
        log = self._base() + [self._send(0, 0), ev("release", turn=1)]
        self.assertEqual(codes(log), ["vii"])
        self.assertEqual(codes(self._base() + [self._send(0, 0)]), [])  # still retained by the host at the end

    def test_e4_a_fragmented_run_with_a_cut_holds_every_fragment(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.turn_extra = lambda k: 1216 if k <= 60 else 0
        sim.cuts.append(("P1", 2000, 6000))
        sim.run(70000)
        self.check(sim, any(e.get("nfrag", 1) == 2 for e in sim.events if e["kind"] == "retain"), "no fragmented turns")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))


def _patch_drop_a_fragment():
    orig = ja.Sim.next_datagram

    def next_datagram(self, c):
        if c is self.chans.get(("H", "P1", 0)) and c.log_next == 20 and c.frag == 1:
            c.log_next, c.frag = 21, 0  # the second fragment of turn 20 is never sent
        return orig(self, c)
    return mock.patch.object(ja.Sim, "next_datagram", next_datagram)


def _patch_free_generations_early():
    def collect(self):
        for g in list(self.gen_state):
            del self.gen_state[g]
            self.sim.ev("gen_free", gen=g)
        self.gens = []
    return mock.patch.object(ja.Host, "collect_gens", collect)


def _patch_reuse_generation_ids():
    orig = ja.Host.collect_gens

    def collect(self):
        orig(self)
        self.next_gen = 1 + len(self.gens)
    return mock.patch.object(ja.Host, "collect_gens", collect)


def _patch_accept_stale_incarnations():
    return mock.patch.object(ja.Sim, "stale", lambda self, local, remote, epoch, what: False)


MUTANTS.update({
    "drop one fragment": (_patch_drop_a_fragment, [
        "RoundCeFragments.test_e4_a_fragmented_run_with_a_cut_holds_every_fragment"]),
    "release a generation too early": (_patch_free_generations_early, [
        "RoundCeBudgets.test_e3_generation_identities_are_never_reused"]),
    "reuse generation identities": (_patch_reuse_generation_ids, [
        "RoundCeBudgets.test_e3_generation_identities_are_never_reused"]),
    "carry holdings into a new incarnation": (_patch_accept_stale_incarnations, [
        "RoundCeWireEpoch.test_e1_a_late_old_packet_never_enters_the_new_stream_in_a_run"]),
})


# ---- round cf (F7) regressions: written first, shown failing at 604107c --------------------------
class RoundCfCaps(TraceCase):
    def _no_drop(self, sim):
        drops = [e for e in sim.events if e["kind"] == "drop_seated"]
        self.check(sim, not drops, "a healthy peer was dropped: %r" % (drops,))

    def test_g1_count_cap_probe_keeps_the_healthy_peer(self):
        sim = ja.Sim(cfg(tail_turns_max=100, tail_bytes_max=4915200), seated=1)
        sim.run(12000)
        self._no_drop(sim)
        self.check(sim, len(sim.host.log_turns) <= 100 and sim.host.peak_turns <= 100,
                   "retained %d, peak %d" % (len(sim.host.log_turns), sim.host.peak_turns))
        self.check(sim, sim.seated[0].applied >= sim.host.applied - 4, "the peer fell behind")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_g1_no_consumers_at_all(self):
        sim = ja.Sim(cfg(tail_turns_max=50), seated=0)
        sim.run(12000)
        self.check(sim, len(sim.host.log_turns) == 50 and sim.host.peak_turns <= 50,
                   "retained %d peak %d" % (len(sim.host.log_turns), sim.host.peak_turns))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_g1_exact_boundaries_and_a_cap_below_the_rolling_window(self):
        for turns in (1, 5, 10):
            sim = ja.Sim(cfg(tail_turns_max=turns), seated=1)
            sim.run(5000)
            self.check(sim, len(sim.host.log_turns) == turns, "cap %d keeps %d" % (turns, len(sim.host.log_turns)))
            self._no_drop(sim)
        # byte-limited below the count cap, and count-limited below the byte cap
        sim = ja.Sim(cfg(tail_bytes_max=7 * 1200, tail_turns_max=100), seated=1)
        sim.run(5000)
        self.check(sim, len(sim.host.log_turns) == 7 and sim.host.retained_bytes == 8400, "byte-limited")
        sim = ja.Sim(cfg(tail_bytes_max=100 * 1200, tail_turns_max=9), seated=1)
        sim.run(5000)
        self.check(sim, len(sim.host.log_turns) == 9, "count-limited")

    def test_g1_a_genuinely_stalled_consumer_is_dropped_the_healthy_one_is_not(self):
        sim = ja.Sim(cfg(tail_turns_max=50), seated=2)
        sim.cuts.append(("P1", 1000, 40000))
        sim.run(12000)
        drops = [e for e in sim.events if e["kind"] == "drop_seated"]
        self.check(sim, [(d["peer"], d["why"]) for d in drops] == [("P1", "log cap")], repr(drops))
        self.check(sim, len(sim.host.log_turns) <= 50 and sim.seated[1].applied >= sim.host.applied - 4, "P2 or the cap")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_g1_impossible_configurations_are_rejected(self):
        for kw in ({"tail_turns_max": 0}, {"tail_bytes_max": 100}, {"turn_buffer": 0}, {"reassembly_fragments_max": 0}):
            with self.assertRaises(ValueError):
                ja.Sim(cfg(**kw))


def _asm_probe(**kw):
    sim = ja.Sim(cfg(turn_buffer=1, **kw), seated=1)
    sim.turn_extra = lambda k: 90000 if k == 1 else 0
    return sim


class RoundCfReassembly(TraceCase):
    def _delivered_turn1(self, sim):
        return [e for e in sim.events if e["kind"] == "deliver" and e["dst"] == "P1" and e["msg"] == "Turn" and e["turn"] == 1]

    def test_g2_the_76_fragment_turn_is_delivered_with_full_coverage(self):
        sim = _asm_probe()
        sim.run(800)
        asm = sim.chans[("recv", "H", "P1", 0)].asm
        held = len(asm.get(("T", 1), {}))
        self.check(sim, 20 <= held < 76 and 1 not in sim.host.log_turns and not self._delivered_turn1(sim),
                   "mid-flight: reassembly holds %d, log has turn 1: %r" % (held, 1 in sim.host.log_turns))
        self.check(sim, validate_clean(sim) == [], "mid-flight: %r" % (validate_clean(sim)[:3],))
        sim.run(3000)
        d = self._delivered_turn1(sim)
        self.check(sim, len(d) == 1 and d[0]["nfrag"] == 76 and sorted(f for f, _ in d[0]["parts"]) == list(range(76)), repr(d))
        self.check(sim, sim.seated[0].applied >= sim.host.applied - 4, "the peer fell behind")
        self.check(sim, sim.peak_reasm_frags >= 44 and sim.reasm_frags == 0, "peak %d, held at the end %d" % (
            sim.peak_reasm_frags, sim.reasm_frags))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_g2_a_lost_early_or_late_fragment_is_retried_and_duplicates_take_no_slot(self):
        for kind, seq in (("Turn-frag", 3), ("Turn", 75)):
            sim = _asm_probe()
            sim.losses = ja.lose(kind, "H>P1", (1, 2), seq=seq)
            sim.run(6000)
            d = self._delivered_turn1(sim)
            self.check(sim, len(d) == 1 and len(d[0]["parts"]) == 76, "%s %d: %r" % (kind, seq, d))
            acq = [(e["mid"], e["frag"]) for e in sim.events if e["kind"] == "reasm_acquire" and e["dst"] == "P1" and e["mid"] == ("T", 1)]
            self.check(sim, len(acq) == len(set(acq)) == 76, "acquired %d distinct %d" % (len(acq), len(set(acq))))
            self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_g2_the_buffer_has_a_limit_and_overflow_is_finite(self):
        sim = _asm_probe(reassembly_fragments_max=30)
        sim.run(4000)
        self.check(sim, [e for e in sim.events if e["kind"] == "reasm_overflow"], "no overflow")
        drops = [e for e in sim.events if e["kind"] == "drop_seated"]
        self.check(sim, drops and drops[0]["why"] == "reassembly cap", repr(drops))
        self.check(sim, sim.reasm_frags == 0 and sim.peak_reasm_frags <= 30, "peak %d held %d" % (sim.peak_reasm_frags, sim.reasm_frags))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_g2_a_snapshot_over_fragment_max_aborts_that_joiner_only(self):
        sim = ja.Sim(cfg(fragment_max=50), seated=1)
        sim.add_join("J1", 0, 2, fragments=100)
        sim.run(8000)
        self.check(sim, sim.host.joins["J1"].restart_reason == ja.REASON_BACKLOG, repr(summary(sim)))
        self.check(sim, sim.seated[0].applied >= sim.host.applied - 4 and validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_g2_epoch_reset_releases_a_partial_assembly(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.turn_extra = lambda k: 90000 if k == 13 else 0
        sim.add_join("J1", 0, 2)
        sim.add_join("J1", 1900, 2)  # the same peer asks again while the 76 fragments are on their way
        sim.run(30000)
        rel = [e for e in sim.events if e["kind"] == "reasm_release" and e["reason"] in ("epoch", "attempt end") and e["dst"] == "J1"]
        self.check(sim, rel, "no explicit release when the old attempt ended at the re-ask")
        self.check(sim, sim.joiners["J1"].active_ms is not None and validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_g2_a_dropped_peer_releases_its_assembly(self):
        sim = _asm_probe()
        sim.cuts.append(("P1", 300, 400000))
        sim.run(70000)
        rel = [e for e in sim.events if e["kind"] == "reasm_release" and e["reason"] == "drop"]
        self.check(sim, [e for e in sim.events if e["kind"] == "drop_seated"] and validate_clean(sim) == [], repr(validate_clean(sim)[:3]))
        self.check(sim, sim.reasm_frags == 0, "fragments still held: %d" % sim.reasm_frags)


class RoundCfValidators(TraceCase):
    def _log(self):
        return [ev("seated", peer="P1", seat=1), ev("retain", turn=1, bytes=2432, total_bytes=2432, count=1, nfrag=2),
                ev("produce", turn=1)]

    def _send(self, frag, seq):
        return ev("send", src="H", dst="P1", ch=0, seq=seq, msg="Turn" if frag else "Turn-frag", attempt=1, lost=False, turn=1,
                  C=None, size=1200, sid=seq + 1, epoch=0, frag=frag, nfrag=2)

    def _arrive(self, frag, seq):
        return ev("arrive", src="H", dst="P1", ch=0, seq=seq, msg="Turn", dup=False, turn=1, sid=seq + 1, epoch=0, frag=frag, nfrag=2)

    def _acq(self, frag, seq):
        return ev("reasm_acquire", src="H", dst="P1", ch=0, mid=("T", 1), turn=1, frag=frag, nfrag=2, size=1200, sid=seq + 1, held=1)

    def test_early_fragment_moved_to_the_reassembly_owner_passes(self):
        log = self._log() + [self._send(0, 0), self._send(1, 1), self._arrive(0, 0), self._acq(0, 0),
                             ev("ack", src="H", dst="P1", ch=0, seqs=(0,), epoch=0), ev("release", turn=1)]
        self.assertEqual(codes(log), [])

    def test_pruning_an_incomplete_fragment_fails(self):
        log = self._log() + [self._send(0, 0), self._send(1, 1), self._arrive(0, 0), self._acq(0, 0),
                             ev("ack", src="H", dst="P1", ch=0, seqs=(0,), epoch=0),
                             ev("reasm_release", src="H", dst="P1", ch=0, mid=("T", 1), turn=1, frags=(0,), reason="prune"),
                             ev("release", turn=1)]
        self.assertEqual(codes(log), ["vii"])

    def test_delivery_needs_every_fragment_index(self):
        part = ev("deliver", src="H", dst="P1", ch=0, seq=1, msg="Turn", turn=1, sid=2, epoch=0, parts=((1, 2),), nfrag=2)
        full = dict(part, parts=((0, 1), (1, 2)))
        self.assertIn("xiv", codes(self._log() + [part]))
        self.assertIn("xiv", codes(self._log() + [full]))
        evidence = [self._send(0, 0), self._send(1, 1), self._arrive(0, 0), self._acq(0, 0),
                    self._arrive(1, 1), self._acq(1, 1)]
        self.assertNotIn("xiv", codes(self._log() + evidence + [full]))

    def test_assembly_may_not_survive_an_epoch_change_without_release(self):
        carry = self._log() + [self._acq(0, 0), ev("epoch_bump", a="H", b="P1", epoch=1, why="re-ask")]
        self.assertIn("xv", codes(carry))
        clean = self._log() + [self._acq(0, 0), ev("reasm_release", src="H", dst="P1", ch=0, mid=("T", 1), turn=1, frags=(0,),
                                                 reason="epoch"), ev("epoch_bump", a="H", b="P1", epoch=1, why="re-ask")]
        self.assertNotIn("xv", codes(clean))

    def test_the_reassembly_buffer_limit_is_checked_from_acquire_events(self):
        log = []
        for i in range(5):
            common = dict(src="H", dst="P1", ch=0, mid=("T", 1), turn=1, frag=i, nfrag=99, size=1, sid=i, epoch=0)
            log.extend([ev("send", **common, seq=i, msg="Turn-frag", attempt=1),
                        ev("arrive", **common, seq=i, msg="Turn-frag", dup=False),
                        ev("reasm_acquire", **common, held=i + 1)])
        self.assertIn("xv", {c for c, _ in ja.validate(log, ja.Cfg(reassembly_fragments_max=3), 0)})


def _patch_prune_an_incomplete_fragment():
    orig = ja.Sim.reassemble

    def reassemble(self, src, dst, ch, c, d, seq):
        ok = orig(self, src, dst, ch, c, d, seq)
        frags = c.asm.get(d.get("mid"))
        if ch == 0 and d.get("nfrag", 1) > 1 and d.get("frag") == 5 and frags and 5 in frags:
            size = frags.pop(5)[0]
            self.asm_account(c, -1, -size)
            self.ev("reasm_release", src=src, dst=dst, ch=ch, mid=d["mid"], turn=d.get("turn"), frags=(5,), reason="prune")
        return ok
    return mock.patch.object(ja.Sim, "reassemble", reassemble)


def _patch_deliver_from_the_final_fragment_alone():
    def reassemble(self, src, dst, ch, c, d, seq):
        if d["msg"] is not None:
            self.deliver(src, dst, ch, seq, d["msg"], parts=((d.get("frag", 0), d.get("sid")),), nfrag=d.get("nfrag", 1))
        return True
    return mock.patch.object(ja.Sim, "reassemble", reassemble)


def _patch_skip_the_count_cap():
    def kept_floor(self, top, extra):
        cfg_ = self.sim.cfg
        need = [f for f, _, _ in self.consumers()]
        cf = min(need) if need else top
        rf = max(1, top - cfg_.turn_buffer + 1)
        total, low = extra, top
        for t in range(top - 1, rf - 1, -1):
            c = self.turn_charge.get(t)
            if c is None or total + c > cfg_.tail_bytes:
                break
            total += c
            low = t
        return min(cf, max(rf, low))
    return mock.patch.object(ja.Host, "kept_floor", kept_floor)


def _patch_forget_assembly_at_an_epoch():
    orig = ja.Sim.release_assembly

    def release_assembly(self, a, b, reason):
        if reason in ("epoch", "attempt end"):
            return None  # the buffers go with the old channels, but nothing is released explicitly
        return orig(self, a, b, reason)
    return mock.patch.object(ja.Sim, "release_assembly", release_assembly)


MUTANTS.update({
    "prune an incomplete fragment": (_patch_prune_an_incomplete_fragment, [
        "RoundCfReassembly.test_g2_the_76_fragment_turn_is_delivered_with_full_coverage"]),
    "deliver from the final fragment alone": (_patch_deliver_from_the_final_fragment_alone, [
        "RoundCfReassembly.test_g2_the_76_fragment_turn_is_delivered_with_full_coverage"]),
    "skip the count-cap pruning": (_patch_skip_the_count_cap, [
        "RoundCfCaps.test_g1_count_cap_probe_keeps_the_healthy_peer"]),
    "retain assembly across an attempt end or epoch": (_patch_forget_assembly_at_an_epoch, [
        "RoundCfReassembly.test_g2_epoch_reset_releases_a_partial_assembly"]),
})


class RoundCfCompletion(unittest.TestCase):
    def test_complete_parts_without_acquisition_do_not_prove_delivery(self):
        log = [ev("deliver", src="H", dst="P1", ch=0, seq=1, msg="Turn", turn=1,
                  sid=2, epoch=0, mid=("T", 1), parts=((0, 1), (1, 2)), nfrag=2)]
        self.assertIn("xiv", {c for c, _ in ja._check_reassembly(log, ja.Cfg())})

    def test_released_parts_cannot_be_reused_for_delivery(self):
        base = ja.Sim(ja.Cfg(), seated=1)
        base.turn_extra = lambda k: 1216 if k == 1 else 0
        base.run(1000)
        log = list(base.events)
        d = next(e for e in log if e["kind"] == "deliver" and e.get("nfrag", 1) > 1)
        log.append(dict(d, t=1001))
        self.assertIn("xiv", {c for c, _ in ja._check_reassembly(log, ja.Cfg())})

    def test_irreducible_ownership_turn_is_rejected_before_application(self):
        sim = ja.Sim(ja.Cfg(turn_store_cap=16, tail_bytes_max=16), seated=0)
        sim.host.pending_events[1] = [(1, 1, "human")]
        with self.assertRaises(ValueError):
            sim.run(100)
        self.assertEqual(sim.host.applied, 0)
        self.assertNotIn(1, sim.host.owners)
        self.assertEqual(sim.host.retained_bytes, 0)

    def test_byte_cap_must_hold_the_empty_turn_even_with_a_small_nominal_charge(self):
        with self.assertRaises(ValueError):
            ja.Sim(ja.Cfg(turn_store_cap=1, tail_bytes_max=1))

    def test_an_active_joiner_overflow_closes_input_and_drops_its_seat(self):
        sim = ja.Sim(ja.Cfg(reassembly_fragments_max=30), seated=1)
        sim.add_join("J1", 0, 2)
        sim.turn_extra = lambda k: 90000 if k == 50 else 0
        sim.run(8000)
        self.assertIsNotNone(sim.joiners["J1"].active_ms)
        self.assertIn("J1", sim.dropped)
        self.assertTrue(any(e["kind"] == "input_off" and e["peer"] == "J1" for e in sim.events))
        self.assertEqual(sim.host.owners.get(2), "ai")

    def test_dead_attempt_does_not_reacquire_inflight_snapshot_fragments(self):
        sim = ja.Sim(ja.Cfg(), seated=1)
        sim.add_join("J1", 0, 2, fragments=100)
        sim.cancel("J1", 350)
        sim.run(8000)
        end = next(e["t"] for e in sim.events if e["kind"] == "attempt_end" and e["how"] == "dead")
        self.assertFalse([e for e in sim.events if e["kind"] == "reasm_acquire" and e["dst"] == "J1" and e["t"] > end])
        self.assertEqual(sim.reasm_frags, 0)


def _patch_hide_acquisition_evidence():
    original = ja.Sim.ev

    def event(self, kind, **fields):
        if kind == "reasm_acquire":
            return
        original(self, kind, **fields)
    return mock.patch.object(ja.Sim, "ev", event)


MUTANTS["claim delivery without acquisition evidence"] = (_patch_hide_acquisition_evidence, [
    "RoundCfReassembly.test_g2_the_76_fragment_turn_is_delivered_with_full_coverage"])


# ---- round cg (T58): the opt-in shared link, written before the code ----------------------------
def lcfg(bps=100_000_000, queue=131072, **kw):
    return ja.Cfg(link_bps=bps, link_queue_bytes=queue, rules=RULES, **kw)


def bare_link(**kw):
    """A simulator used only to drive link_plan directly at chosen instants (ms)."""
    return ja.Sim(lcfg(**kw), seated=1)


def plan(sim, t_ms, src, dst, wire, kind="Turn", lost=False, attempt=1, sid=None):
    sim.t = t_ms
    return sim.link_plan(src, dst, wire, kind, 0, sid if sid is not None else sim.lid + 1000, attempt, lost, 0)


def link_events(sim, kind, lid=None):
    return [e for e in sim.events if e["kind"] == kind and (lid is None or e["lid"] == lid)]


class LinkContract(TraceCase):
    def test_service_time_is_integer_nanoseconds_rounded_up(self):
        self.assertEqual(ja.service_ns(1255, 100_000_000), 100400)
        self.assertEqual(32 * ja.service_ns(1255, 100_000_000), 3212800)
        self.assertEqual(ja.service_ns(55, 100_000_000), 4400)  # a short ACK-only datagram
        self.assertEqual(ja.service_ns(1255, 3_000_000), 3346667)  # 3 346 666.67 ns, never early
        self.assertEqual(ja.arrival_ms(1_000_000), 1)
        self.assertEqual(ja.arrival_ms(1_000_001), 2)

    def test_the_default_is_the_unserialized_comparison_mode(self):
        c = ja.Cfg()
        self.assertEqual((c.link_bps, c.link_queue_bytes), (0, 0))
        sim = ja.row_normal(cfg())
        self.assertEqual((sim.host.joins["J1"].accept_ms, sim.joiners["J1"].active_ms), (1610, 2560))
        self.assertFalse([e for e in sim.events if e["kind"].startswith("link_")])

    def test_link_parameters_are_validated(self):
        for kw in ({"link_bps": 100_000_000, "link_queue_bytes": 0}, {"link_bps": 100_000_000, "link_queue_bytes": 1000},
                   {"link_bps": -1}):
            with self.assertRaises(ValueError):
                ja.Sim(ja.Cfg(**kw))

    def test_same_instant_enqueues_serve_in_call_order_without_preemption(self):
        sim = bare_link()
        out = [plan(sim, 10, "H", "P1", 1255) for _ in range(3)]
        starts = [e["t_ns"] for e in sim.events if e["kind"] == "link_start"]
        fins = [e["t_ns"] for e in sim.events if e["kind"] == "link_finish"]
        self.check(sim, starts == [10_000_000, 10_100_400, 10_200_800], repr(starts))
        self.check(sim, fins == [10_100_400, 10_200_800, 10_301_200], repr(fins))
        # arrival = finish + 150 ms, rounded up to the next millisecond callback
        self.check(sim, [a for _, a in out] == [161, 161, 161], repr(out))

    def test_a_short_ack_takes_its_own_service_time_and_delays_what_follows(self):
        sim = bare_link()
        plan(sim, 0, "P1", "H", 55, kind="ack")
        plan(sim, 0, "P1", "H", 1255)
        starts = [e["t_ns"] for e in sim.events if e["kind"] == "link_start"]
        self.check(sim, starts == [0, 4400], repr(starts))

    def test_the_two_directions_are_independent(self):
        sim = bare_link()
        plan(sim, 5, "H", "P1", 1255)
        plan(sim, 5, "P1", "H", 1255)
        starts = {e["link"]: e["t_ns"] for e in sim.events if e["kind"] == "link_start"}
        self.check(sim, starts == {"egress": 5_000_000, "ingress": 5_000_000}, repr(starts))

    def test_overflow_is_rejected_at_enqueue_and_costs_no_transmission_time(self):
        sim = bare_link(queue=2600)  # two full datagrams (2 x 1255) fit, a third does not
        a = plan(sim, 0, "H", "P1", 1255)
        b = plan(sim, 0, "H", "P1", 1255)
        c = plan(sim, 0, "H", "P1", 1255)
        self.check(sim, a[1] is not None and b[1] is not None and c[1] is None, repr((a, b, c)))
        self.check(sim, [e["reason"] for e in link_events(sim, "link_drop")] == ["queue"], "drop events")
        self.check(sim, len(link_events(sim, "link_start")) == 2, "the rejected datagram was served")
        later = plan(sim, 1, "H", "P1", 1255)  # 1 ms later the two admitted ones are done
        self.check(sim, later[1] is not None and link_events(sim, "link_start")[-1]["t_ns"] == 1_000_000, "the queue did not free")
        for kind in ("link_start", "link_finish"):  # exactly the three admitted datagrams and the later one are served
            self.check(sim, len(link_events(sim, kind)) == 3, "%s: %d" % (kind, len(link_events(sim, kind))))
        self.check(sim, sim.link_stats["egress"]["drop_queue"] == 1 and sim.link_stats["egress"]["peak_bytes"] <= 2600,
                   repr(dict(sim.link_stats["egress"])))
        said = [m for _, m in ja._check_link(sim.events, sim.cfg)]
        self.check(sim, said and all("is enqueued without a transmission record" in m for m in said),
                   "direct link_plan calls have no transmission records; the only error allowed is that one: %r" % said)

    def test_loss_is_sampled_after_transmission_and_still_costs_full_service(self):
        sim = bare_link()
        lost = plan(sim, 0, "H", "P1", 1255, lost=True)
        nxt = plan(sim, 0, "H", "P1", 1255)
        self.check(sim, lost[1] is None and [e["reason"] for e in link_events(sim, "link_drop")] == ["loss"], "loss")
        self.check(sim, [e["t_ns"] for e in link_events(sim, "link_start")] == [0, 100400], "the lost datagram cost no time")
        self.check(sim, sim.link_stats["egress"]["bytes"] == 2510 and nxt[1] is not None, repr(dict(sim.link_stats["egress"])))

    def test_retransmissions_and_acks_are_counted_exactly(self):
        sim = ja.one_join(lcfg(), losses=ja.lose("JoinGrant", "H>J1", (1, 2)), until=9000)
        evs = [e for e in sim.events if e["kind"] == "link_enqueue"]
        for d in ("egress", "ingress"):
            admitted = sum(e["wire"] for e in evs if e["link"] == d) - sim.link_stats[d]["drop_queue_bytes"]
            self.check(sim, admitted == sim.link_stats[d]["bytes"], "%s %d != %d" % (d, admitted, sim.link_stats[d]["bytes"]))
            st = sim.link_stats[d]
            self.check(sim, st["first_bytes"] + st["retry_bytes"] + st["ack_bytes"] == st["bytes"], repr(dict(st)))
        grants = [e for e in sim.events if e["kind"] == "link_enqueue" and e["msg"] == "JoinGrant"]
        self.check(sim, [g["attempt"] for g in grants][:3] == [1, 2, 3] and len({g["wire"] for g in grants}) == 1 and grants[0]["wire"] == 15 + ja.WIRE, repr(grants))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_the_ask_is_charged_separately_and_old_epoch_packets_keep_their_service(self):
        sim = ja.Sim(lcfg(), delays={("Turn", "H>J1", 0, 1): 6000}, seated=1)
        sim.add_join("J1", 0, 2)
        sim.add_join("J1", 1000, 2)
        sim.run(20000)
        ingress_j1 = sum(e["wire"] for e in sim.events if e["kind"] == "link_enqueue" and e["src"] == "J1")
        ingress_j1 -= sim.link_stats["ingress"]["drop_queue_bytes"]
        self.check(sim, sim.stats[("J1", "H")]["bytes"] - ingress_j1 == ja.ASK_PAYLOAD + ja.WIRE,
                   "the ask bytes are not the labelled external charge: %d vs %d" % (sim.stats[("J1", "H")]["bytes"], ingress_j1))
        self.check(sim, not [e for e in sim.events if e["kind"] == "link_enqueue" and "ask" in str(e["msg"]).lower()], "ask on the link")
        stale = [e for e in sim.events if e["kind"] == "stale_epoch" and e["what"] == "arrive"]
        self.check(sim, stale, "the late old packet did not reach the receiver")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))


def _signature(sim):
    return (ja.summary(sim) if sim.joiners else None, sim.host.applied, dict(sim.link_stats["egress"]),
            dict(sim.link_stats["ingress"]), len(sim.events))


class LinkScenarios(TraceCase):
    def test_every_row_at_100M_is_valid_and_reproducible(self):
        for name, fn in ja.LINK_ROWS.items():
            if name == "endless02":
                continue  # its own test
            a = fn("100M")
            self.check(a, validate_clean(a) == [], "%s: %r" % (name, validate_clean(a)[:3]))
            if name not in ("two_joins_two_generations", "seated_during_transfer", "outage_and_rejoin"):  # the long ones run once
                self.check(a, _signature(a) == _signature(fn("100M")), "%s is not reproducible" % name)
            self.check(a, sum(sim_.link_stats["egress"]["drop_queue"] for sim_ in (a,)) == 0, "%s overflowed the queue" % name)
            self.check(a, max([a.host.applied - p.applied for p in a.seated] or [0]) <= 4, "%s: a seated peer fell behind" % name)

    def test_one_fragment_join_and_the_burst_at_100M(self):
        one = ja.LINK_ROWS["one_fragment"]("100M")
        self.check(one, one.joiners["J1"].active_ms is not None and one.host.joins["J1"].restart_ms is None, repr(summary(one)))
        burst = ja.LINK_ROWS["burst_fragmented_turn"]("100M")
        self.check(burst, burst.joiners["J1"].active_ms is not None and burst.link_stats["egress"]["peak_bytes"] > 60000,
                   repr(dict(burst.link_stats["egress"])))

    def test_endless02_at_100M_completes_without_pausing_the_table(self):
        sim = ja.LINK_ROWS["endless02"]("100M")
        j, jr = sim.joiners["J1"], sim.host.joins["J1"]
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))
        self.check(sim, sim.host.applied == 2500 and max(sim.host.applied - p.applied for p in sim.seated) <= 4, "the table paused")
        self.check(sim, j.active_ms is not None and jr.restart_ms is None, repr(summary(sim)))
        self.check(sim, sim.link_stats["egress"]["peak_bytes"] <= sim.cfg.link_queue_bytes, "queue peak")

    def test_the_slow_stress_rate_is_reported_not_forced(self):
        sim = ja.LINK_ROWS["one_fragment"]("1M")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))
        self.check(sim, sim.link_stats["egress"]["busy_ns"] > 100 * sim.link_stats["egress"]["packets"] * 1000,
                   "the slow link did not serialize")

    def test_a_tight_queue_drops_at_enqueue_and_stays_valid(self):
        sim = ja.Sim(lcfg(queue=20000), seated=1)
        sim.turn_extra = lambda k: 90000 if k == 5 else 0
        sim.add_join("J1", 0, 2)
        sim.run(30000)
        st = sim.link_stats["egress"]
        self.check(sim, st["drop_queue"] > 0 and st["peak_bytes"] <= 20000, repr(dict(st)))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))


# ---- round ch (T58 F1): independent link validation, exercised loss, sensitivity ----------------
def _lk(t_ns=0, lid=1, sid=1, attempt=1, src="H", dst="P1", ch=0, epoch=0, size=16, extra_ms=0, link=None, fin_ns=None,
        lost=False, with_service=True, arr_ns=None):
    """One data datagram in the order in which the simulator emits it: enqueue, start, finish, arrival (or loss),
    then the send record. The delivery event is separate (_dv)."""
    wire = size + ja.WIRE
    link = link or ("egress" if src == "H" else "ingress")
    fin = t_ns + ja.service_ns(wire, 100_000_000) if fin_ns is None else fin_ns
    out = [ev("link_enqueue", t_ns // 1_000_000, link=link, lid=lid, t_ns=t_ns, wire=wire, msg="Turn", ch=ch, src=src,
              dst=dst, sid=sid, attempt=attempt, epoch=epoch, idx=None, extra_ms=extra_ms)]
    if with_service:
        out += [ev("link_start", t_ns // 1_000_000, link=link, lid=lid, t_ns=t_ns),
                ev("link_finish", t_ns // 1_000_000, link=link, lid=lid, t_ns=fin)]
    if lost:
        out.append(ev("link_drop", t_ns // 1_000_000, link=link, lid=lid, t_ns=fin, reason="loss", wire=wire))
    else:
        arrival = fin + (150 + extra_ms) * 1_000_000 if arr_ns is None else arr_ns
        out.append(ev("link_arrive", t_ns // 1_000_000, link=link, lid=lid, arrival_ns=arrival, sched_ms=-(-arrival // 1_000_000),
                      sid=sid, msg="Turn"))
    out.append(ev("send", t_ns // 1_000_000, src=src, dst=dst, ch=ch, seq=0, msg="Turn", attempt=attempt, lost=lost,
                  turn=1, C=None, size=size, sid=sid, epoch=epoch, frag=0, nfrag=1, mid=None, lid=lid))
    return out


def _dv(lk, **kw):
    """The delivery event of the datagram built by _lk (default: its own lid and attempt)."""
    snd = [e for e in lk if e["kind"] == "send"][0]
    arr = [e for e in lk if e["kind"] == "link_arrive"][0]
    e = ev("arrive", arr["sched_ms"], src=snd["src"], dst=snd["dst"], ch=snd["ch"], seq=0, msg="Turn", dup=False,
           turn=1, sid=snd["sid"], epoch=snd["epoch"], frag=0, nfrag=1, mid=None, size=snd["size"], lid=snd["lid"],
           attempt=snd["attempt"])
    e.update(kw)
    return e


def _ak(t_ns=0, lid=9, idx=0, acker="P1", to="H", ch=0, epoch=0, extra_ms=0, link=None, lost=False):
    """One ACK-only datagram from `acker` to `to` in the simulator's order, then the ack event the sender processes."""
    link = link or ("egress" if acker == "H" else "ingress")
    fin = t_ns + ja.service_ns(ja.WIRE, 100_000_000)
    out = [ev("link_enqueue", t_ns // 1_000_000, link=link, lid=lid, t_ns=t_ns, wire=ja.WIRE, msg="ack", ch=ch, src=acker,
              dst=to, sid=None, attempt=1, epoch=epoch, idx=idx, extra_ms=0),
           ev("link_start", t_ns // 1_000_000, link=link, lid=lid, t_ns=t_ns),
           ev("link_finish", t_ns // 1_000_000, link=link, lid=lid, t_ns=fin)]
    arrival = fin + 150 * 1_000_000
    if lost:
        out.append(ev("link_drop", t_ns // 1_000_000, link=link, lid=lid, t_ns=fin, reason="loss", wire=ja.WIRE))
    else:
        out.append(ev("link_arrive", t_ns // 1_000_000, link=link, lid=lid, arrival_ns=arrival,
                      sched_ms=-(-arrival // 1_000_000), sid=None, msg="ack"))
    out.append(ev("ack_send", t_ns // 1_000_000, src=acker, dst=to, ch=ch, idx=idx, wire=ja.WIRE, epoch=epoch, lid=lid, lost=lost))
    return out


def _ack_event(lk, **kw):
    arr = [e for e in lk if e["kind"] == "link_arrive"][0]
    snd = [e for e in lk if e["kind"] == "ack_send"][0]
    e = ev("ack", arr["sched_ms"], src=snd["dst"], dst=snd["src"], ch=snd["ch"], seqs=(0,), epoch=snd["epoch"],
           lid=snd["lid"], idx=snd["idx"])
    e.update(kw)
    return e


def _bump(t=150, a="H", b="P1", epoch=1):
    """A new incarnation of the pair (Sim.reset_pair writes this record)."""
    return ev("epoch_bump", t, a=a, b=b, epoch=epoch, why="ask")


def _closed(kind="drop_seated", t=150, **kw):
    """A record that closes a receive side: the seat drop of its non-host endpoint, or the reassembly overflow
    of that (src, dst, channel)."""
    if kind == "drop_seated":
        return ev("drop_seated", t, peer=kw.get("peer", "P1"), why="grace")
    return ev("reasm_overflow", t, src=kw.get("src", "H"), dst=kw.get("dst", "P1"), ch=kw.get("ch", 0), mid=("M", 1), held=4,
              limit=4)


def _held(n, ch=0, src="H", dst="P1", t=100):
    """n fragments of one logical message acquired on the receive side, as Sim.reassemble records them before an
    overflow (the matching sends and arrivals are not needed by the xvi/xvii assertions made with them)."""
    return [ev("reasm_acquire", t, src=src, dst=dst, ch=ch, mid=("M", 1), turn=None, frag=i, nfrag=99, size=1, sid=100 + i,
               held=i + 1) for i in range(n)]


def _refused(**kw):
    e = ev("arrive_refused", 151, src="H", dst="P1", ch=0, seq=0, msg="Turn", lid=1, attempt=1, sid=1, epoch=0, size=16, frag=0,
           nfrag=1, mid=None, why="closed")
    e.update(kw)
    return e


def _stale(**kw):
    e = ev("stale_epoch", 151, src="H", dst="P1", local="P1", what="arrive", epoch=0, lid=1)
    e.update(kw)
    return e


class OverflowRecordsOnHandWrittenLogs(unittest.TestCase):
    """V7 (F4): a reasm_overflow record is justified by the fragments the validator rebuilt on that receive side in the
    current epoch and by the cap in cfg, in the order in which Sim.reassemble emits them (acquisitions, then the overflow).
    The caps here are reassembly_fragments_max 4 (channel 0) and fragment_max 3 (channel 1)."""

    CFG = dict(reassembly_fragments_max=4, fragment_max=3)

    def acq(self, n, ch=0, mid=("M", 1)):
        out = []
        for i in range(n):
            common = dict(src="H", dst="P1", ch=ch, mid=mid, turn=None, frag=i, nfrag=99, size=1, sid=100 + 10 * ch + i, epoch=0)
            out += [ev("send", 1, **common, seq=i, msg="Snap-frag", attempt=1),
                    ev("arrive", 2, **common, seq=i, msg="Snap-frag", dup=False),
                    ev("reasm_acquire", 2, **common, held=i + 1)]
        return out

    def over(self, held, limit, ch=0, **kw):
        return ev("reasm_overflow", 3, src="H", dst="P1", ch=ch, mid=("M", 1), held=held, limit=limit, **kw)

    def msgs(self, log):
        return [m for c, m in ja.validate(log, ja.Cfg(**self.CFG), 0) if c == "xv"]

    def says(self, log, needle):
        return any(needle in m for m in self.msgs(log))

    def test_v7_a_justified_overflow_is_clean_on_both_channels(self):
        self.assertEqual(self.msgs(self.acq(4) + [self.over(4, 4)]), [])  # channel 0: 4 held, the 5th exceeds 4
        self.assertEqual(self.msgs(self.acq(3, ch=1) + [self.over(3, 3, ch=1)]), [])  # channel 1 takes fragment_max (3)

    def test_v7_an_overflow_whose_count_plus_one_does_not_exceed_the_cap_is_rejected(self):
        log = self.acq(3) + [self.over(3, 4)]  # 3 held + 1 = 4 does not exceed 4
        self.assertTrue(self.says(log, "is not justified"), self.msgs(log))
        self.assertFalse(self.says(log, "records held") or self.says(log, "records limit"), self.msgs(log))
        self.assertTrue(self.says([self.over(0, 4)], "is not justified"), "forged, nothing held")
        carry = self.acq(4) + [ev("reasm_release", 3, src="H", dst="P1", ch=0, mid=("M", 1), turn=None, frags=(0, 1, 2, 3),
                                  reason="epoch"), ev("epoch_bump", 3, a="H", b="P1", epoch=1, why="ask"), self.over(4, 4)]
        self.assertTrue(self.says(carry, "is not justified"), "fragments of the old epoch are not held in the new one")

    def test_v7_an_overflow_whose_limit_differs_from_the_configured_cap_is_rejected(self):
        log = self.acq(4) + [self.over(4, 5)]  # justified by the count, but the record names another cap
        self.assertTrue(self.says(log, "records limit 5, the configured cap is 4"), self.msgs(log))
        self.assertFalse(self.says(log, "is not justified") or self.says(log, "records held"), self.msgs(log))
        wrong_channel = self.acq(3, ch=1) + [self.over(3, 4, ch=1)]  # channel 1 uses fragment_max, not the channel-0 cap
        self.assertTrue(self.says(wrong_channel, "records limit 4, the configured cap is 3"), self.msgs(wrong_channel))

    def test_v7_an_overflow_whose_held_differs_from_the_rebuilt_count_is_rejected(self):
        log = self.acq(4) + [self.over(5, 4)]
        self.assertTrue(self.says(log, "records held 5, 4 fragments are held"), self.msgs(log))
        self.assertFalse(self.says(log, "is not justified") or self.says(log, "records limit"), self.msgs(log))


class LinkValidatorsOnHandWrittenLogs(TraceCase):
    """xvi on logs written by hand, in the order in which the simulator emits them (the link records of a
    datagram come before its send record). Complete positive traces pass; each invalid one is rejected by xvi."""

    def _run_xvi(self, log, queue, end, delays):
        kw = {} if delays is None else {"delays": delays}
        cfg_ = ja.Cfg(link_bps=100_000_000, link_queue_bytes=queue, reassembly_fragments_max=4, fragment_max=4)
        return [(c, m) for c, m in ja.validate(log, cfg_, end, **kw) if c in ("xvi", "xvii")]

    def x(self, log, queue=131072, end=0, delays=None):
        return sorted({c for c, _ in self._run_xvi(log, queue, end, delays)})

    def msgs(self, log, queue=131072, end=0, delays=None):
        return [m for _, m in self._run_xvi(log, queue, end, delays)]

    def says(self, log, needle, **kw):
        return any(needle in m for m in self.msgs(log, **kw))

    def good(self):
        d = _lk()
        a = _ak(t_ns=200_000_000, lid=2)
        return d, a

    def test_complete_data_and_ack_traces_pass(self):
        d, a = self.good()
        self.assertEqual(self.x(d + [_dv(d)] + a + [_ack_event(a)]), [])
        self.assertEqual(self.x(d), [])  # an arrival after the horizon needs no delivery event

    def test_a_lost_datagram_with_full_service_passes(self):
        d = _lk(lost=True)
        self.assertEqual(self.x(d), [])

    def test_a_queue_rejection_without_service_passes(self):
        d1 = _lk(size=1200, lid=1, sid=1)
        d2 = _lk(size=1200, lid=2, sid=2, with_service=False)
        d2 = [e for e in d2 if e["kind"] != "link_arrive"]
        d2.insert(1, ev("link_drop", 0, link="egress", lid=2, t_ns=0, reason="queue", wire=1255))
        d2[-1] = dict(d2[-1], lost=True)
        self.assertEqual(self.x(d1 + d2, queue=1300), [])

    def test_bypassing_the_shared_queue_fails(self):
        d = _lk()
        self.assertEqual(self.x([e for e in d if not e["kind"].startswith("link_")]), ["xvi"])

    def test_wrong_wire_bytes_fail_whatever_the_emission_order(self):
        d = _lk()
        d[0] = dict(d[0], wire=16)
        self.assertEqual(self.x(d), ["xvi"])
        self.assertEqual(self.x(d[-1:] + d[:-1]), ["xvi"])  # the send record first: same verdict

    def test_a_one_byte_retry_charge_fails(self):
        first, retry = _lk(lid=1, attempt=1), _lk(lid=2, attempt=2, t_ns=300_000_000)
        retry[0] = dict(retry[0], wire=1)
        self.assertEqual(self.x(first + retry), ["xvi"])

    def test_an_ack_with_the_wrong_size_fails(self):
        a = _ak()
        a[0] = dict(a[0], wire=1)
        self.assertEqual(self.x(a), ["xvi"])

    def test_an_ack_without_a_link_record_or_lid_fails(self):
        a = _ak()
        self.assertEqual(self.x([e for e in a if not e["kind"].startswith("link_")]), ["xvi"])
        self.assertEqual(self.x(a + [_ack_event(a, lid=None)]), ["xvi"])

    def test_invented_lane_names_fail(self):
        self.assertEqual(self.x(_lk(link="egress-P1")), ["xvi"])
        self.assertEqual(self.x(_lk(src="P1", dst="H", link="egress")), ["xvi"])  # wrong direction for the endpoints

    def test_simultaneous_service_from_the_host_on_two_labels_fails(self):
        one, two = _lk(lid=1, sid=1, dst="P1"), _lk(lid=2, sid=2, dst="P2", link="egress-P2")
        self.assertEqual(self.x(one + two), ["xvi"])
        two = _lk(lid=2, sid=2, dst="P2")  # the right label but service that starts while lid 1 still sends
        two = [dict(e, t_ns=100) if e["kind"] in ("link_enqueue", "link_start") else e for e in two]
        self.assertEqual(self.x(one + two), ["xvi"])

    def test_missing_service_records_fail(self):
        d = _lk()
        self.assertEqual(self.x([e for e in d if e["kind"] not in ("link_start", "link_finish")]), ["xvi"])
        self.assertEqual(self.x([e for e in d if e["kind"] != "link_finish"]), ["xvi"])

    def test_orphan_duplicate_and_unserved_records_fail(self):
        d = _lk()
        self.assertEqual(self.x(d + [dict(d[3], lid=77)]), ["xvi"])  # an arrival with no enqueue
        self.assertEqual(self.x(d + [d[3]]), ["xvi"])  # the arrival twice
        self.assertEqual(self.x(d + [d[-1]]), ["xvi"])  # the send twice
        self.assertEqual(self.x(d + [d[1]]), ["xvi"])  # service started twice

    def test_a_rejected_datagram_that_is_served_fails(self):
        d = _lk()
        self.assertEqual(self.x(d[:1] + [ev("link_drop", 0, link="egress", lid=1, t_ns=0, reason="queue", wire=71)] + d[1:]), ["xvi"])

    def test_a_delivery_must_be_its_own_transmission(self):
        first, retry = _lk(lid=1, attempt=1), _lk(lid=2, attempt=2, t_ns=300_000_000)
        base = first + retry
        self.assertEqual(self.x(base + [_dv(first), _dv(retry)]), [])
        self.assertEqual(self.x(base + [_dv(first, attempt=2)]), ["xvi"])  # attempt 1's lid, attempt 2's identity
        self.assertEqual(self.x(base + [_dv(retry, lid=1)]), ["xvi"])  # another transmission's lid
        self.assertEqual(self.x(base + [_dv(first, dst="P2")]), ["xvi"])  # another endpoint
        self.assertEqual(self.x(base + [_dv(first, lid=None)]), ["xvi"])  # no identity
        self.assertEqual(self.x(base + [_dv(first), _dv(first)]), ["xvi"])  # one arrival used twice
        self.assertEqual(self.x(base + [_dv(first, t=first[3]["sched_ms"] + 1)]), ["xvi"])  # not at the callback

    def test_an_ack_must_be_its_own_transmission(self):
        a, b = _ak(lid=3, idx=0), _ak(lid=4, idx=1, t_ns=1_000_000)
        self.assertEqual(self.x(a + b + [_ack_event(a), _ack_event(b)]), [])
        self.assertEqual(self.x(a + b + [_ack_event(a, idx=1)]), ["xvi"])
        self.assertEqual(self.x(a + b + [_ack_event(a, lid=4)]), ["xvi"])
        self.assertEqual(self.x(a + b + [_ack_event(a, src="P2")]), ["xvi"])

    def test_the_lost_flag_must_match_the_link(self):
        d = _lk()
        d[-1] = dict(d[-1], lost=True)
        self.assertEqual(self.x(d), ["xvi"])

    def test_an_end_time_shorter_than_the_service_fails(self):
        self.assertEqual(self.x(_lk(fin_ns=1000)), ["xvi"])

    def test_early_rounding_and_early_arrival_fail(self):
        d = _lk()
        d[3] = dict(d[3], sched_ms=150)
        self.assertEqual(self.x(d), ["xvi"])
        d = _lk()
        d[3] = dict(d[3], arrival_ns=100_000_000, sched_ms=100)
        self.assertEqual(self.x(d), ["xvi"])

    def test_admission_over_the_queue_cap_and_a_missed_rejection_fail(self):
        a, b = _lk(lid=1, sid=1, size=1200), _lk(lid=2, sid=2, size=1200, t_ns=1000)
        self.assertEqual(self.x(a + b, queue=1300), ["xvi"])  # the second does not fit and is served
        c = _lk(lid=2, sid=2, size=100, t_ns=1000, with_service=False)
        c = [e for e in c if e["kind"] != "link_arrive"]
        c.insert(1, ev("link_drop", 0, link="egress", lid=2, t_ns=1000, reason="queue", wire=155))
        c[-1] = dict(c[-1], lost=True)
        self.assertEqual(self.x(a + c, queue=1500), ["xvi"])  # 1255 + 155 fits 1500: the rejection was wrong
        self.assertEqual(self.x(a + c, queue=1300), [])  # it did not fit 1300: the rejection is right

    # ---- round ci (T58 F2): V1 early arrival, V2 payload identity, V3 arrival consumption ----
    def test_v1_an_arrival_at_the_finish_fails_whatever_the_label_says(self):
        d = _lk(arr_ns=5680)  # label 0, but zero propagation: the physical floor is finish + 150 ms
        self.assertEqual(self.x(d), ["xvi"])
        self.assertTrue(self.says(d, "physical floor"), self.msgs(d))

    def test_v1_a_negative_label_fails(self):
        d = _lk(extra_ms=-150)  # the label and the arrival agree with each other; both are impossible
        self.assertTrue(self.says(d, "negative labelled extra delay"), self.msgs(d))
        self.assertTrue(self.says(d, "physical floor"), self.msgs(d))

    def test_v1_the_label_must_equal_the_declared_delay(self):
        d = _lk(extra_ms=40)
        self.assertTrue(self.says(d, "differs from the declared"), self.msgs(d))  # nothing declared: 0
        self.assertEqual(self.x(d, delays={("Turn", "H>P1", None, 1): 40}), [])  # declared by kind, direction, any seq
        self.assertEqual(self.x(d, delays={("Turn", "H>P1", 0, 1): 40}), [])  # declared by the exact sequence
        self.assertTrue(self.says(d, "differs from the declared", delays={("Turn", "H>P1", 0, 2): 40}), "wrong attempt")
        self.assertEqual(self.x(_lk(), delays={}), [])

    def test_v2_attempts_of_one_sid_carry_one_payload(self):
        first, retry = _lk(lid=1, attempt=1, size=1200), _lk(lid=2, attempt=2, t_ns=300_000_000, size=1)  # consistently charged 1 byte
        self.assertTrue(self.says(first + retry, "records size"), self.msgs(first + retry))
        same = _lk(lid=2, attempt=2, t_ns=300_000_000, size=1200)
        self.assertEqual(self.x(first + same), [])
        other = _lk(lid=2, attempt=2, t_ns=300_000_000, size=1200)
        other[-1] = dict(other[-1], seq=9)
        self.assertTrue(self.says(first + other, "identity"), self.msgs(first + other))

    def test_v2_a_delivery_carries_the_size_and_identity_of_its_transmission(self):
        d = _lk(size=1)
        self.assertTrue(self.says(d + [_dv(d, size=1200)], "delivery"), self.msgs(d + [_dv(d, size=1200)]))
        d = _lk(size=1200)
        self.assertEqual(self.x(d + [_dv(d)], end=200), [])
        self.assertTrue(self.says(d + [_dv(d, mid=("T", 1))], "delivery"), "message identity")
        self.assertTrue(self.says(d + [_dv(d, frag=3)], "delivery"), "fragment identity")

    def test_v3_a_served_arrival_without_a_recorded_outcome_fails_at_the_horizon(self):
        d = _lk()
        self.assertTrue(self.says(d, "consumed by no recorded outcome", end=200), self.msgs(d, end=200))
        self.assertEqual(self.x(d, end=100), [])  # the callback is at 151 ms, after the horizon: a reservation

    def test_v3_each_recorded_outcome_consumes_the_arrival_exactly_once(self):
        d = _lk()
        for outcome in ([_dv(d)], [_bump(), _stale()], [_closed(), _refused()]):
            self.assertEqual(self.x(d + outcome, end=200), [], outcome[-1]["kind"])
        ready = d + [_bump(), _closed()]  # an epoch change, then a closure: both outcomes are valid on their own
        for pair in ((_dv(d), _stale()), (_dv(d), _dv(d, dup=True)), (_stale(), _refused())):
            self.assertTrue(self.says(ready + list(pair), "more than one", end=200), pair[1]["kind"])
        wrong = _stale(src="P1", dst="H")  # a stale record of the other direction is not this datagram's
        self.assertTrue(self.says(d + [_bump(), wrong], "endpoints", end=200), self.msgs(d + [_bump(), wrong], end=200))
        late = _stale(t=152)
        self.assertTrue(self.says(d + [_bump(), late], "callback", end=200), self.msgs(d + [_bump(), late], end=200))

    def test_v3_an_ack_arrival_is_consumed_by_an_ack_or_a_stale_record(self):
        a = _ak(lid=9)
        self.assertTrue(self.says(a, "consumed by no recorded outcome", end=200), self.msgs(a, end=200))
        self.assertEqual(self.x(a + [_ack_event(a)], end=200), [])
        stale = _stale(local="H", what="ack", lid=9)
        self.assertEqual(self.x(a + [_bump(), stale], end=200), [])
        self.assertTrue(self.says(a + [_bump(), _ack_event(a), stale], "more than one", end=200), "twice")

    def test_v4_a_refusal_needs_a_closure_record_of_its_receive_side(self):
        d = _lk()
        self.assertTrue(self.says(d + [_refused()], "no closure record", end=200), self.msgs(d + [_refused()], end=200))
        self.assertEqual(self.x(d + [_closed(), _refused()], end=200), [])  # the seat drop of the non-host endpoint
        self.assertEqual(self.x(d + _held(4) + [_closed("reasm_overflow"), _refused()], end=200), [])  # the overflow of the same channel
        for other in (_closed(peer="P2"), _closed("reasm_overflow", ch=1), _closed("reasm_overflow", src="P1", dst="H")):
            self.assertTrue(self.says(d + [other, _refused()], "no closure record", end=200), other)
        self.assertTrue(self.says(d + [_closed(), _bump(), _refused()], "no closure record", end=200), "closure before the bump")
        self.assertTrue(self.says(d + [_refused(), _closed()], "no closure record", end=200), "closure after the refusal")
        self.assertEqual(self.x(d + [_bump(), _closed(), _refused()], end=200), [], "closure after the bump")

    def test_v8_a_closed_receive_side_takes_no_arrival(self):
        over = _closed("reasm_overflow")  # H>P1 ch0
        d = _lk()  # a data datagram H>P1 ch0, epoch 0
        needle = "after its receive side was closed"
        self.assertTrue(self.says(d + [over, _dv(d)], needle, end=200), self.msgs(d + [over, _dv(d)], end=200))
        self.assertTrue(self.says(d + [over, _dv(d, dup=True)], "(duplicate) on H>P1 ch0 " + needle, end=200), "duplicate")
        up = _lk(src="P1", dst="H")  # P1>H ch0
        self.assertTrue(self.says(up + [_closed(), _dv(up)], "on P1>H ch0 " + needle, end=200), "seat drop, other direction")
        self.assertEqual(self.x(d + [_closed(), _refused()], end=200), [])  # a refusal is the closed side's outcome
        # not closed: another channel, the reverse direction of an overflow, the epoch after the next bump
        other = _lk(ch=1)
        self.assertEqual(self.x(other + [over, _dv(other)], end=200), [], "H>P1 ch1 after the ch0 overflow")
        self.assertEqual(self.x(up + [over, _dv(up)], end=200), [], "P1>H ch0 after the H>P1 ch0 overflow")
        later = _lk(epoch=1)
        self.assertEqual(self.x(later[:0] + [over, _bump()] + later + [_dv(later)], end=200), [], "after the next bump")
        self.assertEqual(self.x(d + [_closed(peer="P2"), _dv(d)], end=200), [], "the seat drop of another peer")
        self.assertEqual(self.x(d + [_dv(d)], end=200), [], "no closure")

    def test_v5_a_stale_record_must_be_stale_and_the_transmissions_own(self):
        d = _lk()
        self.assertTrue(self.says(d + [_stale()], "is not stale", end=200), self.msgs(d + [_stale()], end=200))  # epoch 0 is current
        self.assertEqual(self.x(d + [_bump(), _stale()], end=200), [])
        self.assertTrue(self.says(d + [_bump(), _stale(what="ack")], "but its transmission is", end=200), "type")
        self.assertTrue(self.says(d + [_bump(), _stale(epoch=3)], "its transmission carries epoch", end=200), "epoch")
        a = _ak(lid=9)
        self.assertTrue(self.says(a + [_bump(), _stale(local="H", what="arrive", lid=9)], "but its transmission is", end=200), "ACK")
        self.assertTrue(self.says(a + [_stale(local="H", what="ack", lid=9)], "is not stale", end=200), "ACK, no bump")
        wrapped = d + [_bump(epoch=e % 65536) for e in range(1, 65537)]  # the epoch is the count of bumps mod 65536
        self.assertTrue(self.says(wrapped + [_stale()], "is not stale", end=200), "65536 bumps")
        self.assertEqual(self.x(wrapped + [_bump(epoch=1), _stale()], end=200), [], "65537 bumps")

    def test_v6_a_non_last_fragment_carries_the_full_payload(self):
        def frag(size, i, n):
            log = _lk(size=size)
            log[-1] = dict(log[-1], frag=i, nfrag=n)
            return log
        self.assertEqual([m for m in self.msgs(frag(1200, 0, 3)) if "non-last" in m or "payload_max" in m], [])
        self.assertTrue(self.says(frag(1199, 0, 3), "non-last fragment"), self.msgs(frag(1199, 0, 3)))
        self.assertTrue(self.says(frag(1, 1, 3), "non-last fragment"), "middle fragment")
        self.assertFalse(self.says(frag(100, 2, 3), "non-last fragment"), "the last fragment's size is the message model's")
        self.assertFalse(self.says(frag(1, 0, 1), "non-last fragment"), "a single datagram is its own last fragment")
        self.assertTrue(self.says(frag(1201, 2, 3), "exceeds payload_max"), "no payload exceeds the cap")
        self.assertTrue(self.says(frag(1201, 0, 1), "exceeds payload_max"), "not even a single datagram")

def _wrap_link_plan(fn):
    orig = ja.Sim.link_plan

    def link_plan(self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw):
        return fn(orig, self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw)
    return mock.patch.object(ja.Sim, "link_plan", link_plan)


def _patch_bypass_the_shared_queue():
    def fn(orig, self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw):
        self.lid += 1
        return self.lid, (None if lost_key or self.cut_active(src, dst) else self.t + self.cfg.one_way_ms + extra_ms)
    return _wrap_link_plan(fn)


def _patch_one_byte_retries():
    def fn(orig, self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw):
        return orig(self, src, dst, 1 if attempt > 1 and kind != "ack" else wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw)
    return _wrap_link_plan(fn)


def _patch_one_byte_first_sends():
    def fn(orig, self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw):
        return orig(self, src, dst, 1 if attempt == 1 and kind != "ack" else wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw)
    return _wrap_link_plan(fn)


def _patch_acks_bypass_the_link():
    def fn(orig, self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw):
        if kind == "ack":
            return None, self.t + self.cfg.one_way_ms
        return orig(self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw)
    return _wrap_link_plan(fn)


def _patch_one_byte_acks():
    def fn(orig, self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw):
        return orig(self, src, dst, 1 if kind == "ack" else wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw)
    return _wrap_link_plan(fn)


def _patch_no_service_records():
    orig = ja.Sim.ev

    def ev_(self, kind, **kw):
        if kind in ("link_start", "link_finish"):
            return None
        return orig(self, kind, **kw)
    return mock.patch.object(ja.Sim, "ev", ev_)


def _patch_round_the_arrival_down():
    return mock.patch.object(ja, "arrival_ms", lambda ns: ns // ja.NS_PER_MS)


def _patch_breach_the_queue_cap():
    return mock.patch.object(ja.Sim, "link_cap", lambda self: 10 ** 12)


class LinkValidation(TraceCase):
    """Each runtime mutant of the link accounting is caught by its own test, on the specific xvi message that
    mutant produces (a mutant caught by an unrelated check does not count). Every test passes unmutated."""

    def _run(self):
        return ja.one_join(lcfg(), losses=ja.lose("JoinGrant", "H>J1", (1,)), until=12000)

    def _no_message(self, sim, needle, code="xvi"):
        hit = [m for c, m in validate_clean(sim) if c == code and needle in m]
        self.check(sim, not hit, "%s says %r" % (code, hit[:2]))

    def _refusal_sim(self):
        """A real closure: the snapshot may hold three fragments, so the fourth overflows the reassembly and closes the
        receive side; datagrams already in flight are served on the closed side and the simulator refuses them."""
        return ja.fragment_loss_sim(lcfg(fragment_max=3))

    def _two_ask_sim(self):
        sim = ja.Sim(lcfg(), delays={("Turn", "H>J1", 0, 1): 6000}, seated=1)
        sim.add_join("J1", 0, 2)
        sim.add_join("J1", 1000, 2)
        sim.run(20000)
        return sim

    def test_every_data_attempt_is_charged_its_wire_size(self):
        sim = self._run()
        self.check(sim, [e for e in sim.events if e["kind"] == "link_enqueue" and e["attempt"] == 1], "no first transmission")
        self._no_message(sim, "(data attempt 1) is")

    def test_retransmissions_are_charged_their_wire_size(self):
        sim = self._run()
        self.check(sim, [e for e in sim.events if e["kind"] == "link_enqueue" and e["attempt"] > 1], "the run has no retransmission")
        self._no_message(sim, "(data attempt 2) is")

    def test_acks_pass_through_the_shared_link(self):
        sim = self._run()
        self.check(sim, [e for e in sim.events if e["kind"] == "ack_send"], "the run has no ACK-only datagram")
        self._no_message(sim, "transmission ('a'")

    def test_acks_are_charged_their_wire_size(self):
        self._no_message(self._run(), "(ACK-only) is")

    def test_service_records_are_required(self):
        self._no_message(self._run(), "without a complete service record")

    def test_the_shared_capacity_is_enforced(self):
        self._no_message(self._run(), "transmission ('d'")

    def test_the_arrival_is_never_early(self):
        self._no_message(self._run(), "called back at")

    def test_v1_no_link_arrival_is_before_finish_plus_one_way_delay(self):
        sim = self._run()
        self._no_message(sim, "negative labelled extra delay")
        self._no_message(sim, "physical floor")

    def test_v1_the_labelled_extra_delay_is_the_declared_one(self):
        sim = ja.Sim(lcfg(), delays={("Turn", "H>J1", None, 1): 40}, seated=1)
        sim.add_join("J1", 0, 2)
        sim.run(6000)
        self._no_message(sim, "differs from the declared")

    def test_v2_every_attempt_of_a_sid_records_one_payload(self):
        sim = self._run()
        self.check(sim, [e for e in sim.events if e["kind"] == "send" and e["attempt"] > 1], "no retransmission")
        self._no_message(sim, "records size")

    def test_v3_every_served_arrival_has_a_recorded_outcome(self):
        sim = ja.fragment_loss_sim(lcfg())
        self.check(sim, [e for e in sim.events if e["kind"] == "link_arrive"], "no arrival")
        self._no_message(sim, "consumed by no recorded outcome")

    def test_a_closed_receiver_leaves_a_refusal_record_with_the_lid(self):
        sim = ja.Sim(lcfg(), seated=1)
        sim.recv_state("H", "P1", 0).dead = True
        sim.arrive("H", "P1", 0, _old_datagram(epoch=0, sid=77, attempt=1), 0, 5, 1)
        refused = [e for e in sim.events if e["kind"] == "arrive_refused"]
        self.assertEqual([(e["lid"], e["attempt"], e["why"]) for e in refused], [(5, 1, "closed")])

    def test_a_stale_epoch_arrival_and_ack_record_the_lid(self):
        sim = self._two_ask_sim()
        stale = [e for e in sim.events if e["kind"] == "stale_epoch" and e["what"] in ("arrive", "ack")]
        self.assertTrue(stale and all(e.get("lid") is not None for e in stale), stale[:2])
        self.assertEqual({e["what"] for e in stale}, {"arrive", "ack"})
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_v4_every_refusal_follows_a_closure_of_its_receive_side(self):
        for sim in (ja.fragment_loss_sim(lcfg()), self._refusal_sim()):
            self._no_message(sim, "no closure record")

    def test_v5_every_stale_record_with_a_lid_is_stale(self):
        for sim in (ja.fragment_loss_sim(lcfg()), self._two_ask_sim()):
            self._no_message(sim, "is not stale")
            self._no_message(sim, "but its transmission is")
            self._no_message(sim, "its transmission carries epoch")

    def test_v6_every_non_last_fragment_is_a_full_payload(self):
        sim = ja.fragment_loss_sim(lcfg())
        self.check(sim, [e for e in sim.events if e["kind"] == "send" and e["msg"] == "Snap-frag"], "no snapshot fragment")
        self._no_message(sim, "non-last fragment", "xvii")
        self._no_message(sim, "exceeds payload_max", "xvii")

    def test_v8_an_overflowed_receive_side_takes_no_arrival(self):
        sim = self._refusal_sim()  # a real overflow of H>J1 ch1, then a real refusal
        self.check(sim, [e for e in sim.events if e["kind"] == "reasm_overflow"], "no overflow record")
        self._no_message(sim, "after its receive side was closed")

    def test_v7_a_forged_overflow_record_is_not_a_closure(self):
        sim = self._refusal_sim()  # a real overflow (held 3, limit 3) closes the receive side
        self.check(sim, [e for e in sim.events if e["kind"] == "reasm_overflow"], "no overflow record")
        self._no_message(sim, "is not justified", "xv")
        self._no_message(sim, "records held", "xv")

    def test_v7_an_overflow_exceeds_the_configured_cap_not_one_fragment_less(self):
        sim = ja.fragment_loss_sim(lcfg(fragment_max=4))
        self.check(sim, [e for e in sim.events if e["kind"] == "reasm_overflow"], "no overflow record")
        self._no_message(sim, "records limit", "xv")
        self._no_message(sim, "is not justified", "xv")

    def test_a_real_closure_is_followed_by_a_refusal_and_a_valid_run(self):
        sim = self._refusal_sim()
        over = [e for e in sim.events if e["kind"] == "reasm_overflow"]
        refused = [e for e in sim.events if e["kind"] == "arrive_refused"]
        self.check(sim, len(over) == 1 and refused, "overflow %r, refusals %r" % (over, refused))
        o = over[0]
        self.check(sim, all((e["src"], e["dst"], e["ch"]) == (o["src"], o["dst"], o["ch"]) and e["t"] >= o["t"] for e in refused),
                   "a refusal is not on the closed receive side after the overflow: %r" % refused)
        served = [e for e in sim.events if e["kind"] == "link_arrive" and e["lid"] in {r["lid"] for r in refused}]
        self.check(sim, len(served) == len(refused), "every refusal consumes a served link arrival")
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))


def _patch_negative_extra_delay():
    def fn(orig, self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw):
        return orig(self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms - self.cfg.one_way_ms, *a, **kw)
    return _wrap_link_plan(fn)


@contextlib.contextmanager
def _patch_small_retry_payload():
    """Every retry is recorded as a 1-byte payload on its send record and charged 1 + WIRE on the link."""
    orig_plan, orig_ev = ja.Sim.link_plan, ja.Sim.ev

    def link_plan(self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw):
        if kind != "ack" and attempt > 1:
            wire = 1 + ja.WIRE
        return orig_plan(self, src, dst, wire, kind, ch, sid, attempt, lost_key, extra_ms, *a, **kw)

    def ev_(self, kind, **kw):
        if kind == "send" and kw.get("attempt", 1) > 1 and kw.get("lid") is not None:
            kw["size"] = 1
        return orig_ev(self, kind, **kw)
    with mock.patch.object(ja.Sim, "link_plan", link_plan), mock.patch.object(ja.Sim, "ev", ev_):
        yield


def _patch_silent_snapshot_discard():
    orig = ja.Sim.arrive

    def arrive(self, src, dst, ch, d, epoch=None, lid=None, attempt=None):
        if d["kind"] == "Snap-frag" and attempt == 1:
            return None
        return orig(self, src, dst, ch, d, epoch, lid, attempt)
    return mock.patch.object(ja.Sim, "arrive", arrive)


def _patch_refuse_on_a_live_receiver():
    """The first Snap-frag arrival is refused by a receiver that was never closed, with the F2 closed-receiver record."""
    orig = ja.Sim.arrive

    def arrive(self, src, dst, ch, d, epoch=None, lid=None, attempt=None):
        if d["kind"] == "Snap-frag" and attempt == 1 and d["epoch"] == self.ep_of(dst, src):
            self.ev("arrive_refused", src=src, dst=dst, ch=ch, seq=d["seq"], msg=d["kind"], lid=lid, attempt=attempt,
                    sid=d.get("sid"), epoch=d.get("epoch"), size=d["size"], frag=d.get("frag", 0),
                    nfrag=d.get("nfrag", 1), mid=d.get("mid"), why="closed")
            return None
        return orig(self, src, dst, ch, d, epoch, lid, attempt)
    return mock.patch.object(ja.Sim, "arrive", arrive)


def _patch_stale_record_for_a_current_epoch():
    """The first Snap-frag arrival of the current epoch is dropped with a stale_epoch record that carries its lid."""
    orig = ja.Sim.arrive

    def arrive(self, src, dst, ch, d, epoch=None, lid=None, attempt=None):
        if d["kind"] == "Snap-frag" and attempt == 1 and d["epoch"] == self.ep_of(dst, src):
            self.ev("stale_epoch", src=src, dst=dst, local=dst, what="arrive", epoch=d["epoch"], lid=lid)
            return None
        return orig(self, src, dst, ch, d, epoch, lid, attempt)
    return mock.patch.object(ja.Sim, "arrive", arrive)


def _patch_forged_overflow_then_refusals():
    """P5: on a live receiver the first Snap-frag arrival writes an unjustified reasm_overflow record (held 0) and is
    refused; the later ones are refused too, so the F3 closure check is satisfied by the forged record."""
    orig = ja.Sim.arrive

    def arrive(self, src, dst, ch, d, epoch=None, lid=None, attempt=None):
        if d["kind"] == "Snap-frag" and attempt == 1 and d["epoch"] == self.ep_of(dst, src):
            if not getattr(self, "_forged", False):
                self._forged = True
                self.ev("reasm_overflow", src=src, dst=dst, ch=ch, mid=d["mid"], held=0, limit=self.cfg.fragment_max)
            self.ev("arrive_refused", src=src, dst=dst, ch=ch, seq=d["seq"], msg=d["kind"], lid=lid, attempt=attempt,
                    sid=d.get("sid"), epoch=d.get("epoch"), size=d["size"], frag=d.get("frag", 0),
                    nfrag=d.get("nfrag", 1), mid=d.get("mid"), why="closed")
            return None
        return orig(self, src, dst, ch, d, epoch, lid, attempt)
    return mock.patch.object(ja.Sim, "arrive", arrive)


def _patch_overflow_leaves_the_side_open():
    """P8: the overflow is recorded and the buffer released, but the receive side is set open again."""
    orig = ja.Sim.reassemble

    def reassemble(self, src, dst, ch, c, d, seq):
        ok = orig(self, src, dst, ch, c, d, seq)
        if not ok:
            c.dead = False
        return ok
    return mock.patch.object(ja.Sim, "reassemble", reassemble)


def _patch_overflow_one_fragment_early():
    """P7: the snapshot cap is lowered by one only while Sim.reassemble runs, so the reassembly overflows one
    fragment early and the record says held = limit = fragment_max - 1."""
    orig = ja.Sim.reassemble

    def reassemble(self, src, dst, ch, c, d, seq):
        if ch == 0:
            return orig(self, src, dst, ch, c, d, seq)
        object.__setattr__(self.cfg, "fragment_max", self.cfg.fragment_max - 1)
        try:
            return orig(self, src, dst, ch, c, d, seq)
        finally:
            object.__setattr__(self.cfg, "fragment_max", self.cfg.fragment_max + 1)
    return mock.patch.object(ja.Sim, "reassemble", reassemble)


def _patch_one_byte_snapshot_fragments():
    """Every snapshot fragment is shrunk to a 1-byte payload before its first record, so all records agree."""
    orig = ja.Sim.transmit

    def transmit(self, src, dst, ch, d):
        if "sid" not in d and d["kind"] == "Snap-frag":
            d["size"] = 1
        return orig(self, src, dst, ch, d)
    return mock.patch.object(ja.Sim, "transmit", transmit)


class LinkRows(TraceCase):
    """The fragment-loss row exercises the loss it names; the grant-loss row stays identifiable."""

    def _row(self, rate):
        return ja.LINK_ROWS["fragment_loss_and_retry"](rate)

    def _check_fragment_loss(self, rate):
        sim = self._row(rate)
        seq = ja.FRAGMENT_LOSS_SEQ
        sends = [e for e in sim.events if e["kind"] == "send" and e["msg"] == "Snap-frag" and e["seq"] == seq]
        self.check(sim, [e["attempt"] for e in sends][:3] == [1, 2, 3], "the named fragment was sent %r" % [e["attempt"] for e in sends])
        self.check(sim, [e["lost"] for e in sends][:3] == [True, True, False], "lost flags %r" % [e["lost"] for e in sends])
        for e in sends[:2]:
            lid = e["lid"]
            self.check(sim, any(x["kind"] == "link_drop" and x["lid"] == lid and x["reason"] == "loss" for x in sim.events), "no loss record")
            fin = [x for x in sim.events if x["kind"] == "link_finish" and x["lid"] == lid]
            self.check(sim, len(fin) == 1, "a lost datagram must be served in full")
        arrive = [e for e in sim.events if e["kind"] == "arrive" and e["msg"] == "Snap-frag" and e["seq"] == seq and not e["dup"]]
        self.check(sim, [e["attempt"] for e in arrive] == [3] and arrive[0]["lid"] == sends[2]["lid"], repr(arrive))
        done = [e for e in sim.events if e["kind"] == "deliver" and e["msg"] == "SnapshotDone" and e["dst"] == "J1"]
        self.check(sim, len(done) == 1 and done[0]["t"] == arrive[0]["t"], "the snapshot message was not delivered by the retry arrival")
        rel = [e for e in sim.events if e["kind"] == "reasm_release" and e["dst"] == "J1" and e["mid"] == done[0]["mid"]]
        self.check(sim, len(rel) == 1 and rel[0]["t"] >= done[0]["t"], "the reassembly was not completed and released")
        j = sim.joiners["J1"]
        self.check(sim, j.active_ms is not None and sim.host.joins["J1"].restart_ms is None, repr(ja.summary(sim)))
        self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))
        return sim

    def test_the_named_snapshot_fragment_is_lost_twice_and_recovered_at_100M(self):
        self._check_fragment_loss("100M")

    def test_the_named_snapshot_fragment_is_lost_twice_and_recovered_at_1M(self):
        self._check_fragment_loss("1M")

    def test_grant_loss_is_a_separate_identifiable_row(self):
        sim = ja.LINK_ROWS["grant_loss_and_retry"]("100M")
        sends = [e for e in sim.events if e["kind"] == "send" and e["msg"] == "JoinGrant"]
        self.check(sim, [e["lost"] for e in sends][:2] == [True, False], repr([(e["attempt"], e["lost"]) for e in sends]))
        self.check(sim, validate_clean(sim) == [], "")


# ---- T61: deterministic propagation jitter, written before the implementation -----------------
class JoinJitter(TraceCase):
    def _sim(self, link=False, losses=()):
        c = cfg(link_bps=100_000_000, link_queue_bytes=131072,
                jitter_min_ms=0, jitter_max_ms=25, jitter_seed=6101) if link else cfg(
                    jitter_min_ms=0, jitter_max_ms=25, jitter_seed=6101)
        return ja.one_join(c, losses=losses, until=9000)

    def test_defaults_are_zero_and_invalid_jitter_is_rejected(self):
        self.assertEqual((ja.Cfg().jitter_min_ms, ja.Cfg().jitter_max_ms, ja.Cfg().jitter_seed), (0, 0, 0))
        for kw in ({"jitter_min_ms": -1}, {"jitter_min_ms": 9, "jitter_max_ms": 8},
                   {"jitter_seed": -1}, {"jitter_script": (("bad", 1),)}):
            with self.assertRaises(ValueError):
                ja.Sim(cfg(**kw))

    def test_samples_are_keyed_by_immutable_data_and_ack_identities(self):
        data = ja.jitter_identity("data", "H", "J1", 1, 3, 7, 2)
        retry = ja.jitter_identity("data", "H", "J1", 1, 3, 7, 3)
        ack = ja.jitter_identity("ack", "J1", "H", 1, 3, 4)
        c = cfg(jitter_min_ms=0, jitter_max_ms=25, jitter_seed=6101)
        self.assertNotEqual(data, retry)
        self.assertNotEqual(data, ack)
        self.assertEqual(ja.jitter_delay(c, data), ja.jitter_delay(c, data))
        self.assertTrue(0 <= ja.jitter_delay(c, data) <= 25)

    def test_every_data_attempt_and_ack_has_a_sample_in_both_modes(self):
        for link in (False, True):
            sim = self._sim(link, losses=ja.lose("JoinGrant", "H>J1", (1,)))
            tx = [e for e in sim.events if e["kind"] in ("send", "ack_send")]
            self.check(sim, tx and all("jitter_ms" in e and "jitter_id" in e for e in tx), repr(tx[:2]))
            retries = [e for e in tx if e["kind"] == "send" and e["attempt"] > 1]
            self.check(sim, retries and len({tuple(e["jitter_id"]) for e in retries}) == len(retries), repr(retries))
            self.check(sim, validate_clean(sim) == [], repr(validate_clean(sim)[:3]))

    def test_zero_jitter_preserves_the_legacy_semantic_projection(self):
        old = ja.one_join(cfg(), until=6000)
        zero = ja.one_join(cfg(jitter_min_ms=0, jitter_max_ms=0, jitter_seed=999), until=6000)
        ignored = {"jitter_ms", "jitter_id", "jitter_source"}
        def project(events):
            return [{k: v for k, v in e.items() if k not in ignored} for e in events]
        self.assertEqual(project(old.events), project(zero.events))

    def test_mixed_zero_jitter_evidence_is_not_a_legacy_fixture(self):
        sim = ja.one_join(cfg(link_bps=100_000_000, link_queue_bytes=131072), until=6000)
        log = [dict(e) for e in sim.events]
        enqueue = next(e for e in log if e["kind"] == "link_enqueue")
        enqueue.pop("jitter_ms")
        enqueue.pop("jitter_id")
        codes = {code for code, _ in ja.validate(log, sim.cfg, sim.t, delays=sim.delays)}
        self.assertTrue({"xvi", "xviii"} & codes, codes)

    def test_validator_rebuilds_arrival_and_rejects_a_consistent_forgery(self):
        sim = self._sim(True)
        clean = validate_clean(sim)
        self.assertEqual(clean, [], clean[:3])
        forged = [dict(e) for e in sim.events]
        sent = next(e for e in forged if e["kind"] == "send" and not e["lost"])
        sent["jitter_ms"] += 1
        lid = sent["lid"]
        for e in forged:
            if e.get("lid") == lid and e["kind"] in ("link_enqueue", "link_arrive", "arrive"):
                if e["kind"] == "link_arrive":
                    e["arrival_ns"] += ja.NS_PER_MS
                    e["sched_ms"] += 1
                elif e["kind"] == "arrive":
                    e["t"] += 1
                else:
                    e["jitter_ms"] += 1
        self.assertIn("xviii", {code for code, _ in ja.validate(forged, sim.cfg, sim.t, delays=sim.delays)})

    def test_j1_through_j8_timing_mutants_are_each_caught_independently(self):
        def fresh(link=True, losses=()):
            return self._sim(link, losses)

        def codes(sim, log):
            return {c for c, _ in ja.validate(log, sim.cfg, sim.t, delays=sim.delays)}

        # J1: ACK propagation silently ignores the profile.
        sim = fresh()
        log = [dict(e) for e in sim.events]
        next(e for e in log if e["kind"] == "ack_send")["jitter_ms"] = 0
        self.assertIn("xviii", codes(sim, log))
        # J2: a retry reuses attempt one's sample and identity.
        sim = fresh(losses=ja.lose("JoinGrant", "H>J1", (1,)))
        log = [dict(e) for e in sim.events]
        retry = next(e for e in log if e["kind"] == "send" and e["attempt"] == 2)
        retry["jitter_id"] = tuple(retry["jitter_id"][:-1]) + (1,)
        retry["jitter_ms"] = ja.jitter_delay(sim.cfg, tuple(retry["jitter_id"]))
        self.assertIn("xviii", codes(sim, log))
        # J3: channel/epoch are omitted from a sample identity.
        sim = fresh()
        log = [dict(e) for e in sim.events]
        data = next(e for e in log if e["kind"] == "send")
        data["jitter_id"] = ("data", data["src"], data["dst"], data["seq"], data["attempt"])
        self.assertIn("xviii", codes(sim, log))
        # J4: the un-serialized path zeros all propagation jitter.
        sim = fresh(False)
        log = [dict(e) for e in sim.events]
        next(e for e in log if e["kind"] == "send")["jitter_ms"] = 0
        self.assertIn("xviii", codes(sim, log))
        # J5: a serialized callback is rounded down, together with its consumer.
        sim = fresh()
        log = [dict(e) for e in sim.events]
        send = next(e for e in log if e["kind"] == "send" and not e["lost"])
        for e in log:
            if e.get("lid") == send["lid"] and e["kind"] == "link_arrive":
                e["sched_ms"] -= 1
            elif e.get("lid") == send["lid"] and e["kind"] == "arrive":
                e["t"] -= 1
        self.assertIn("xviii", codes(sim, log))
        # J6: a logged sample and arrival are forged consistently (covered above, independently repeated).
        sim = fresh()
        log = [dict(e) for e in sim.events]
        next(e for e in log if e["kind"] == "send")["jitter_ms"] += 1
        self.assertIn("xviii", codes(sim, log))
        # J7: two attempts exchange sample identities.
        sim = fresh(losses=ja.lose("JoinGrant", "H>J1", (1,)))
        log = [dict(e) for e in sim.events]
        a, b = [e for e in log if e["kind"] == "send" and e["msg"] == "JoinGrant"][:2]
        a["jitter_id"], b["jitter_id"] = b["jitter_id"], a["jitter_id"]
        self.assertIn("xviii", codes(sim, log))
        # J8: an in-horizon served packet has its outcome erased.
        sim = fresh(False)
        send = next(e for e in sim.events if e["kind"] == "send" and not e["lost"])
        log = [dict(e) for e in sim.events if not (e["kind"] == "arrive" and e.get("sid") == send["sid"] and e.get("attempt") == send["attempt"])]
        self.assertIn("xviii", codes(sim, log))


MUTANTS.update({
    "charge every retry one byte": (_patch_one_byte_retries, [
        "LinkValidation.test_retransmissions_are_charged_their_wire_size"]),
    "charge every first transmission one byte": (_patch_one_byte_first_sends, [
        "LinkValidation.test_every_data_attempt_is_charged_its_wire_size"]),
    "bypass the link for all ACKs": (_patch_acks_bypass_the_link, [
        "LinkValidation.test_acks_pass_through_the_shared_link"]),
    "charge every ACK one byte": (_patch_one_byte_acks, [
        "LinkValidation.test_acks_are_charged_their_wire_size"]),
    "omit the service records": (_patch_no_service_records, [
        "LinkValidation.test_service_records_are_required"]),
    "bypass the shared queue": (_patch_bypass_the_shared_queue, [
        "LinkValidation.test_the_shared_capacity_is_enforced",
        "LinkScenarios.test_every_row_at_100M_is_valid_and_reproducible"]),
    "round the arrival down (early)": (_patch_round_the_arrival_down, [
        "LinkValidation.test_the_arrival_is_never_early",
        "LinkScenarios.test_every_row_at_100M_is_valid_and_reproducible"]),
    "breach the queue cap": (_patch_breach_the_queue_cap, [
        "LinkScenarios.test_a_tight_queue_drops_at_enqueue_and_stays_valid"]),
    "V1: negative extra delay, zero propagation": (_patch_negative_extra_delay, [
        "LinkValidation.test_v1_no_link_arrival_is_before_finish_plus_one_way_delay"]),
    "V2: retry recorded and charged as a 1-byte payload": (_patch_small_retry_payload, [
        "LinkValidation.test_v2_every_attempt_of_a_sid_records_one_payload"]),
    "V3: the receiver silently discards served Snap-frag arrivals": (_patch_silent_snapshot_discard, [
        "LinkValidation.test_v3_every_served_arrival_has_a_recorded_outcome"]),
    "V4: a live receiver refuses a Snap-frag with the closed-receiver record": (_patch_refuse_on_a_live_receiver, [
        "LinkValidation.test_v4_every_refusal_follows_a_closure_of_its_receive_side"]),
    "V5: a stale_epoch record for a datagram of the current epoch": (_patch_stale_record_for_a_current_epoch, [
        "LinkValidation.test_v5_every_stale_record_with_a_lid_is_stale"]),
    "V6: every snapshot fragment shrunk to a 1-byte payload": (_patch_one_byte_snapshot_fragments, [
        "LinkValidation.test_v6_every_non_last_fragment_is_a_full_payload"]),
    "V7/P5: a forged reasm_overflow record on a live receiver, then refusals": (_patch_forged_overflow_then_refusals, [
        "LinkValidation.test_v7_a_forged_overflow_record_is_not_a_closure"]),
    "V7/P7: the reassembly overflows one fragment early": (_patch_overflow_one_fragment_early, [
        "LinkValidation.test_v7_an_overflow_exceeds_the_configured_cap_not_one_fragment_less"]),
    "V8/P8: the overflow is recorded but the receive side stays open": (_patch_overflow_leaves_the_side_open, [
        "LinkValidation.test_v8_an_overflowed_receive_side_takes_no_arrival"]),
})


# ---- round dn: H1-H4 and N1-N4 regressions (failing on the defective source) ----
def _repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _git_show(spec):
    return subprocess.check_output(["git", "show", spec], cwd=_repo_root())


def _blob_sha256(name, head="HEAD"):
    return hashlib.sha256(_git_show("%s:tools/%s" % (head, name))).hexdigest()


def _jitter_codes(events, end_t=1000, cfg=None):
    return {code for code, _ in ja.validate(events, cfg or ja.Cfg(), end_t)}


def _served_send(lost=False):
    """One un-serialized data transmission whose derived arrival is t=150."""
    ident = ("data", "H", "J1", 0, 1, 4, 1)
    # JoinGrant, not a Turn: the retention checker only maps Turn sends, and this fixture
    # is about transmission identity rather than the retained-turn log.
    send = ev("send", 0, src="H", dst="J1", ch=0, seq=4, msg="JoinGrant", attempt=1, lost=lost,
              sid=7, epoch=1, size=10, jitter_ms=0, jitter_id=ident, jitter_source="seed")
    arrive = ev("arrive", 150, src="H", dst="J1", ch=0, seq=4, msg="JoinGrant", sid=7, epoch=1,
                attempt=1, dup=False)
    return ident, send, arrive


class RoundDnH1Conservation(unittest.TestCase):
    """Round-dh H1. These pass on the fixed checker and fail on the 30964c2 archive."""

    def test_h1_control_one_matching_arrival_is_accepted(self):
        _, send, arrive = _served_send()
        self.assertNotIn("xviii", _jitter_codes([send, arrive]))

    def test_h1a_wrong_time_extra_arrival_is_rejected(self):
        _, send, arrive = _served_send()
        extra = ev("arrive", 200, src="H", dst="J1", ch=0, seq=4, msg="JoinGrant", sid=7, epoch=1,
                   attempt=1, dup=False)
        self.assertIn("xviii", _jitter_codes([send, arrive, extra]))

    def test_h1b_orphan_arrival_with_a_changed_sid_is_rejected(self):
        _, send, arrive = _served_send()
        orphan = ev("arrive", 150, src="H", dst="J1", ch=0, seq=4, msg="JoinGrant", sid=99, epoch=1,
                    attempt=1, dup=False)
        self.assertIn("xviii", _jitter_codes([send, arrive, orphan]))

    def test_h1c_outcome_of_a_lost_transmission_is_rejected(self):
        _, send, arrive = _served_send(lost=True)
        self.assertIn("xviii", _jitter_codes([send, arrive]))

    def test_h1d_boolean_attempt_script_is_rejected(self):
        ident = ("data", "H", "J1", 0, 0, 0, True)
        with self.assertRaises(ValueError):
            ja.Cfg(jitter_script=((ident, 5),)).validate()

    def test_h1e_negative_epoch_script_is_rejected(self):
        ident = ("data", "H", "J1", 0, -1, 0, 1)
        with self.assertRaises(ValueError):
            ja.Cfg(jitter_script=((ident, 5),)).validate()

    def test_h1f_empty_endpoint_script_is_rejected(self):
        ident = ("data", "", "J1", 0, 0, 0, 1)
        with self.assertRaises(ValueError):
            ja.Cfg(jitter_script=((ident, 5),)).validate()


class _Events:
    def __init__(self, events):
        self.events = events


class RoundDnH4Metrics(unittest.TestCase):
    """Round-dh H4 and the F2.4 evidence metrics."""

    def test_h4_input_on_samples_maximum_lag_without_a_later_apply(self):
        sim = _Events([
            {"kind": "apply", "peer": "H", "turn": 100, "t": 0},
            {"kind": "input_on", "peer": "J1", "applied": 98, "t": 1},
        ])
        self.assertEqual(jj.maximum_lag(sim), 2)

    def test_h4_a_loading_joiner_does_not_increase_seated_lag(self):
        # Seated P1 is 2 turns behind. Loading J1 has applied turn 2 and is not seated
        # and has not opened input, so its 100-turn gap is not the seated maximum.
        sim = _Events([
            {"kind": "seated", "peer": "P1", "t": 0},
            {"kind": "apply", "peer": "P1", "turn": 100, "t": 1},
            {"kind": "apply", "peer": "H", "turn": 102, "t": 2},
            {"kind": "admit", "name": "J1", "t": 3},
            {"kind": "apply", "peer": "J1", "turn": 2, "t": 4},
        ])
        self.assertEqual(jj.maximum_lag(sim), 2)

    def test_n6_peak_retained_turns_and_bytes_match_a_hand_vector(self):
        sim = _Events([
            {"kind": "retain", "turn": 1, "bytes": 10},
            {"kind": "retain", "turn": 2, "bytes": 25},
            {"kind": "release", "turn": 1},
            {"kind": "retain", "turn": 3, "bytes": 5},
        ])
        # Two turns and 35 bytes are held together, then turn 1 leaves, then turn 3 joins
        # the surviving 25 bytes. The peaks are 2 turns and 35 bytes.
        self.assertEqual(jj.peak_retained(sim), (2, 35))

    def test_n6_queued_network_traffic_is_not_every_scheduler_event(self):
        sim = ja.one_join(ja.link_cfg("100M"), until=2000)

        def not_network():
            return None

        sim.q.append((sim.t, 0, 0, not_network, ()))
        got = jj.metrics(sim)
        network = sum(1 for item in sim.q if item[3].__name__ in ("arrive", "take_ack"))
        self.assertEqual(got["pending_scheduler_events"], len(sim.q))
        self.assertEqual(got["pending_network_callbacks"], network)
        self.assertLess(got["pending_network_callbacks"], got["pending_scheduler_events"])
        self.assertEqual(got["pending_wire_bytes"], sum(link["occ"] for link in sim.links.values()))

    def test_n6_reassembly_ownership_is_read_from_events(self):
        sim = ja.one_join(ja.link_cfg("100M"), until=2000)
        sim.peak_reasm_frags = -1
        held = [event["held"] for event in sim.events if event["kind"] == "reasm_acquire"]
        self.assertEqual(jj.metrics(sim)["peak_reassembly_fragments"], max(held or [0]))


def _manifest(diagnostics):
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=_repo_root(), text=True).strip()
    return {
        "kind": "manifest", "run_id": "round-dn-fixture",
        "provenance": {
            "code_head": head,
            "source_hashes": {
                "join_activation.py": _blob_sha256("join_activation.py"),
                "join_jitter_study.py": _blob_sha256("join_jitter_study.py"),
            },
            "python": "test", "platform": "test",
        },
        "row_timeout_seconds": 300, "study_timeout_seconds": 5400, "diagnostics": list(diagnostics),
    }


def _write_study(path, rows, diagnostics):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(json.dumps(_manifest(diagnostics), sort_keys=True) + "\n")
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True, default=list) + "\n")


def _production_validate(path, full_matrix=False, extra_args=()):
    """Run the production validator. A fixture strips only the 72-row cardinality."""
    source = inspect.getsource(jj.main)
    if not full_matrix:
        source = source.replace("len(primary) != 72", "len(primary) != len(expected)")
        source = source.replace('len({row["config_hash"] for row in primary}) != 72',
                                'len({row["config_hash"] for row in primary}) != len(expected)')
    saved = jj.main
    exec(compile(source, jj.__file__, "exec"), jj.__dict__)
    try:
        try:
            code = jj.main(["--validate", "--output", path, *extra_args])
        except SystemExit as exc:
            return (exc.code if isinstance(exc.code, int) else 1), str(exc.code or exc)
        return (0 if code in (None, 0) else code), ""
    finally:
        jj.main = saved


@contextlib.contextmanager
def _fixture_matrix():
    saved = (jj.RATES, jj.TIMERS, jj.ROWS, jj.PROFILES)
    jj.RATES = ("100M",)
    jj.TIMERS = (300,)
    jj.ROWS = ("one_fragment",)
    jj.PROFILES = {name: saved[3][name] for name in ("zero", "uniform25-seed6101")}
    try:
        yield
    finally:
        jj.RATES, jj.TIMERS, jj.ROWS, jj.PROFILES = saved


def _finished(profile, script=(), diagnostic=None):
    row = jj.run("100M", 300, "one_fragment", profile, script, diagnostic)
    row["wall_seconds"] = 1.0
    return row


class RoundDnStudyEvidence(unittest.TestCase):
    """N1-N4 and the round-dh H2/H3 tamper and diagnostic evidence. One small matrix, never 72 rows."""

    @classmethod
    def setUpClass(cls):
        cls._dir = tempfile.mkdtemp(prefix="t61-dn-", dir=os.path.join(_repo_root(), "cache", "t61-f2", "dn"))
        script = ((("data", "H", "J1", 1, 0, 0, 1), 100),)
        zero = _finished("zero")
        keyed = _finished("uniform25-seed6101")
        repeat = dict(keyed)
        repeat["kind"] = "diagnostic"
        repeat["diagnostic"] = "repeat"
        asymmetric = _finished("zero", script, "asymmetric-data-delay")
        cls.rows = [zero, keyed, repeat, asymmetric]
        cls.script = script
        cls.path = os.path.join(cls._dir, "clean.jsonl")
        _write_study(cls.path, cls.rows, ["repeat", "asymmetric-data-delay"])

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls._dir, ignore_errors=True)

    def _copy(self, name, mutate, diagnostics=None):
        path = os.path.join(self._dir, name)
        rows = json.loads(json.dumps(self.rows, default=list))
        # json round-trip above used the in-memory rows; rewrite from the originals via dumps.
        rows = [json.loads(json.dumps(row, sort_keys=True, default=list)) for row in self.rows]
        mutate(rows)
        names = ["repeat", "asymmetric-data-delay"] if diagnostics is None else diagnostics
        _write_study(path, rows, names)
        return path

    def _check(self, path, extra_args=()):
        with _fixture_matrix():
            return _production_validate(path, full_matrix=False, extra_args=extra_args)

    def test_clean_fixture_is_accepted(self):
        code, text = self._check(self.path)
        self.assertEqual(code, 0, text)

    def test_h2_invented_result_hash_is_rejected(self):
        def edit(rows):
            rows[0]["result_hash"] = "0" * 64
        code, text = self._check(self._copy("hash.jsonl", edit))
        self.assertNotEqual(code, 0, text)

    def test_h2_invented_semantic_trace_hash_is_rejected(self):
        def edit(rows):
            rows[0]["semantic_trace_hash"] = "1" * 64
        code, text = self._check(self._copy("trace.jsonl", edit))
        self.assertNotEqual(code, 0, text)

    def test_h2_metrics_reduced_to_validators_are_rejected(self):
        def edit(rows):
            rows[0]["metrics"] = {"validators": []}
        code, text = self._check(self._copy("metrics-dropped.jsonl", edit))
        self.assertNotEqual(code, 0, text)
        self.assertIn("metrics", text)

    def test_h2_edited_ack_bytes_are_rejected(self):
        def edit(rows):
            rows[0]["metrics"]["ack_bytes"] += 1000
        code, text = self._check(self._copy("ack.jsonl", edit))
        self.assertNotEqual(code, 0, text)
        self.assertIn("metrics", text)

    def test_h2_edited_config_rate_is_rejected(self):
        def edit(rows):
            rows[0]["config"]["rate_bps"] += 1
        code, text = self._check(self._copy("rate.jsonl", edit))
        self.assertNotEqual(code, 0, text)
        self.assertIn("config", text)

    def test_h2_edited_config_jitter_is_rejected(self):
        def edit(rows):
            rows[0]["config"]["jitter_max_ms"] += 1
        code, text = self._check(self._copy("jitter.jsonl", edit))
        self.assertNotEqual(code, 0, text)
        self.assertIn("config", text)

    def test_h2_nan_wall_seconds_are_rejected(self):
        def edit(rows):
            rows[0]["wall_seconds"] = float("nan")
        code, text = self._check(self._copy("nan.jsonl", edit))
        self.assertNotEqual(code, 0, text)

    def test_h2_wall_seconds_past_the_row_bound_are_rejected(self):
        def edit(rows):
            rows[0]["wall_seconds"] = 301
        code, text = self._check(self._copy("wall.jsonl", edit))
        self.assertNotEqual(code, 0, text)

    def test_h2_removing_both_diagnostics_is_rejected(self):
        path = os.path.join(self._dir, "no-diag.jsonl")
        primaries = [row for row in self.rows if row["kind"] == "primary"]
        _write_study(path, primaries, [])
        code, text = self._check(path)
        self.assertNotEqual(code, 0, text)
        self.assertIn("diagnostic", text)

    def test_h2_non_ancestor_head_is_rejected(self):
        path = os.path.join(self._dir, "ancestor.jsonl")
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=_repo_root(), text=True).strip()
        body = open(self.path, encoding="utf-8").read().replace('"code_head": "%s"' % head, '"code_head": "%s"' % ("0" * 40), 1)
        open(path, "w", encoding="utf-8", newline="\n").write(body)
        code, detail = self._check(path)
        self.assertNotEqual(code, 0, detail)

    def test_h2_changed_source_hash_is_rejected(self):
        path = os.path.join(self._dir, "source.jsonl")
        body = open(self.path, encoding="utf-8").read()
        current = _blob_sha256("join_activation.py")
        open(path, "w", encoding="utf-8", newline="\n").write(body.replace(current, "a" * 64, 1))
        code, detail = self._check(path)
        self.assertNotEqual(code, 0, detail)

    def test_n1_provenance_hashes_the_committed_blob(self):
        recorded = jj.provenance()["source_hashes"]
        for name in ("join_activation.py", "join_jitter_study.py"):
            blob = _git_show("HEAD:tools/%s" % name)
            raw = open(os.path.join(_repo_root(), "tools", name), "rb").read()
            self.assertNotEqual(raw, blob)
            self.assertEqual(recorded[name], hashlib.sha256(blob).hexdigest())
            self.assertNotEqual(recorded[name], hashlib.sha256(raw).hexdigest())

    def test_n1_crlf_and_lf_checkouts_validate_the_same_blob(self):
        tmp = os.path.join(self._dir, "eol-repo")
        os.makedirs(os.path.join(tmp, "tools"))
        blobs = {}
        for name in ("join_activation.py", "join_jitter_study.py"):
            blobs[name] = _git_show("HEAD:tools/%s" % name)
            open(os.path.join(tmp, "tools", name), "wb").write(blobs[name])
        git = ["git", "-c", "user.name=t61", "-c", "user.email=t61@example.invalid", "-c", "core.autocrlf=false"]
        subprocess.check_call(["git", "init"], cwd=tmp)
        subprocess.check_call(git + ["add", "tools"], cwd=tmp)
        subprocess.check_call(git + ["commit", "-m", "lf blob"], cwd=tmp)
        probe = os.path.join(tmp, "tools", "_prov.py")
        open(probe, "w", encoding="utf-8", newline="\n").write(
            "import os, sys\n"
            "sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))\n"
            "import join_jitter_study as jj\n"
            "print(jj.provenance()['source_hashes']['join_activation.py'])\n"
            "print(jj.provenance()['source_hashes']['join_jitter_study.py'])\n")

        def recorded(mode):
            # git checkout -c core.autocrlf left this host's worktree as LF, so the two
            # encodings are written explicitly from the same committed blob.
            for name, blob in blobs.items():
                data = blob if mode == "lf" else blob.replace(b"\n", b"\r\n")
                open(os.path.join(tmp, "tools", name), "wb").write(data)
            raw = open(os.path.join(tmp, "tools", "join_activation.py"), "rb").read()
            if mode == "crlf":
                self.assertIn(b"\r\n", raw)
                self.assertNotEqual(raw, blobs["join_activation.py"])
            else:
                self.assertEqual(raw, blobs["join_activation.py"])
            proc = subprocess.run([sys.executable, "-I", "-B", "_prov.py"], cwd=os.path.join(tmp, "tools"),
                                  capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            return proc.stdout.split()

        crlf = recorded("crlf")
        lf = recorded("lf")
        self.assertEqual(crlf, lf)
        self.assertEqual(crlf[0], hashlib.sha256(blobs["join_activation.py"]).hexdigest())

    def test_n3_a_short_deadline_reaps_the_owned_worker(self):
        before = {process.pid for process in multiprocessing.active_children()}
        started = time.monotonic()
        result = jj.controlled_run(("100M", 300, "one_fragment", "zero", (), None), 0.001)
        self.assertEqual(result["status"], "timeout")
        self.assertLess(time.monotonic() - started, 5)
        left = [process for process in multiprocessing.active_children()
                if process.pid not in before and process.is_alive()]
        self.assertEqual(left, [])

    def test_n3_validate_replay_obeys_the_row_deadline(self):
        started = time.monotonic()
        code, text = self._check(self.path, extra_args=("--row-timeout", "0.001"))
        self.assertNotEqual(code, 0, text)
        self.assertLess(time.monotonic() - started, 5)
        self.assertIn("timeout", text)
        self.assertEqual([process for process in multiprocessing.active_children() if process.is_alive()], [])

    def test_h3_repeat_matches_its_primary_projection(self):
        primary = _finished("uniform25-seed6101")
        repeat = _finished("uniform25-seed6101", (), "repeat")
        self.assertEqual(repeat["result_hash"], primary["result_hash"])
        self.assertEqual(repeat["semantic_trace_hash"], primary["semantic_trace_hash"])

    def test_h3_asymmetric_delay_is_the_scripted_100ms(self):
        ident = self.script[0][0]
        zero = _drive_one_fragment("zero", ())
        shifted = _drive_one_fragment("zero", self.script)
        def arrival(sim):
            sent = next(event for event in sim.events
                        if event["kind"] == "send" and tuple(event.get("jitter_id", ())) == ident)
            self.assertEqual(sent["jitter_source"], "script" if sim is shifted else "seed")
            arrived = next(event for event in sim.events
                           if event["kind"] == "arrive" and event.get("sid") == sent["sid"]
                           and event.get("attempt") == sent["attempt"] and event.get("src") == sent["src"])
            return sent, arrived
        zero_send, zero_arrive = arrival(zero)
        shift_send, shift_arrive = arrival(shifted)
        self.assertEqual(shift_send["jitter_ms"], 100)
        self.assertEqual(zero_send["jitter_ms"], 0)
        self.assertEqual(shift_arrive["t"] - zero_arrive["t"], 100)
        used = {tuple(event["jitter_id"]) for event in shifted.events
                if event["kind"] in ("send", "ack_send") and event.get("jitter_source") == "script"}
        self.assertEqual(used, {ident})


def _drive_one_fragment(profile, script):
    lo, hi, seed, configured = jj.PROFILES[profile]
    original = ja.link_cfg

    def config(selected_rate, **kw):
        return original(selected_rate, jitter_min_ms=lo, jitter_max_ms=hi, jitter_seed=seed,
                        jitter_script=tuple(configured) + tuple(script), **kw)

    ja.link_cfg = config
    try:
        return ja.SENS_ROWS["one_fragment"][0]("100M", 300)
    finally:
        ja.link_cfg = original


if __name__ == "__main__":
    multiprocessing.freeze_support()
    unittest.main()

