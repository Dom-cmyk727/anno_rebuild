#!/usr/bin/env python3
"""Self-checks for tools/join_activation.py. No network and no game files.

The round-cb regressions come first. Run the file with JOIN_RULES=F1 to see them
fail against the guards published at 3020900; the default is the F2 proposal.
A failing assertion prints the full event trace.
"""

import contextlib
import os
import sys
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import join_activation as ja

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
    """True when each (src, dst) pair delivered channel-0 sequences in increasing order."""
    last = {}
    for _, src, dst, ch, seq, _ in sim.deliveries:
        if ch != 0:
            continue
        if seq <= last.get((src, dst), -1):
            return False
        last[(src, dst)] = seq
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
        caps = ja.queue_caps(cfg())
        self.assertEqual(caps["datagram"], 1253)
        self.assertEqual(caps["ch0_in_flight_per_pair"], 40096)
        self.assertEqual(caps["ch0_waiting_per_pair"], 80192)
        self.assertEqual(caps["ch0_host"], 601440)
        self.assertEqual(caps["ch1_host"], 80192)
        self.assertEqual(caps["ch2_host"], 630)
        self.assertEqual(caps["host_total"], 682262)
        # 4 watermarks (75), 2 reports (75), 2 InputReqs (69), InputOn (59), JoinAbort (61).
        self.assertEqual(caps["joiner_reverse"], 4 * 75 + 2 * 75 + 2 * 69 + 59 + 61)
        self.assertEqual(caps["ack_kbit_per_s_ch0_and_ch1"], 848.0)

    def test_wire_sizes(self):
        self.assertEqual(ja.turn_bytes([]), 16)
        self.assertEqual(ja.turn_bytes([(1, 2, "human")]), 16 + 14)
        self.assertEqual(ja.HEADER + ja.IP_UDP, 53)


# ---- round cc (F3) regressions: written first, shown failing at 5ea1f34 ----------------------
# These use only what a test can observe without the checker's own bookkeeping: the joiner's
# input-open time and cursors, and the host turn computed from the clock (the host produces
# turn k at k * turn_ms), as the oracle.

ORACLE_LAG = 8  # host turn minus joiner applied turn at the millisecond input opens (input_lag_max 6 + 2)
DOMAIN_END = 650000
DOMAIN_STEP_MAX = 73
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
        # The domain: replay at least 1.37 times faster than the table (73 ms per 100 ms turn or
        # less), so that endless02's 1 606-turn backlog is gone inside catchup_ms. See below.
        steps = sorted(set(range(1, DOMAIN_STEP_MAX + 1)) | {15, 24, 25, 29, 30, 34, 40, 45, 50, 70})
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
        # Replay slower than 73 ms per turn cannot clear the backlog inside catchup_ms. That is the
        # join-only catch-up restart (reason 6), nothing else, and the table keeps running.
        for step in (74, 80, 100):
            sim = ja.Sim(cfg(replay_step_ms=step))
            sim.add_join("J1", 0, 2, snapshot_ms=160650)
            sim.run(DOMAIN_END)
            jr = sim.host.joins["J1"]
            self.check(sim, jr.restart_reason == 6 and jr.restart_ms == 600000, "step %d: %r" % (step, summary(sim)))
            self.check(sim, sim.host.applied == DOMAIN_END // 100, "the table stopped")

    def test_b3_the_latency_limit_is_the_window_not_the_deadline(self):
        # At 2 000 ms one way the round trip is 4 s: 40 turns per round trip want 40 sequences
        # against the window of 32, so the pair delivers 8 turns a second against 10 produced. The
        # joiner never reaches the host (no selection) and ends in the catch-up restart (reason 6);
        # the log-backed cursor keeps every turn. The deadline and input_ready_ms do not end it.
        c = cfg(one_way_ms=2000, resend_ms=4000)
        sim = ja.one_join(c, until=DOMAIN_END, snapshot_ms=2000)
        self.check(sim, sim.host.joins["J1"].restart_reason == 6, repr(summary(sim)))
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
        self.assertNotIn("iv", codes(log + [ev("outage", peer="P1")]))

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
        base = [ev("seated", peer="P1", seat=1), ev("produce", turn=1), ev("produce", turn=2)]
        self.assertEqual(codes(base), [])  # held in the host's log for the live peer
        refused = base + [ev("overflow", src="H", dst="P1", ch=0, msg="Turn", turn=2)]
        self.assertEqual(codes(refused), ["vii"])
        self.assertEqual(codes(base + [ev("abandon", src="H", dst="P1", ch=0, seq=1)]), ["vii"])
        # A dropped seat is no longer live.
        self.assertEqual(codes(refused + [ev("drop_seated", peer="P1")]), [])

    def test_ix_an_outage_must_end_or_drop(self):
        log = [ev("outage", 1000, peer="P1", src="H", dst="P1", ch=0, age=2000), ev("produce", 70000, turn=700)]
        self.assertIn("ix", codes(log))
        self.assertNotIn("ix", codes(log + [ev("outage_end", 5000, peer="P1")]))

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


if __name__ == "__main__":
    unittest.main()
