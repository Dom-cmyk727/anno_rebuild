#!/usr/bin/env python3
"""Self-checks for tools/join_activation.py. No network and no game files.

The round-cb regressions come first. Run the file with JOIN_RULES=F1 to see them
fail against the guards published at 3020900; the default is the F2 proposal.
A failing assertion prints the full event trace.
"""

import os
import sys
import unittest

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
        self.check(sim, j.active_ms == 2260 and j.active_cursors == (20, 21), "input row %r" % (j.active_cursors,))
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
        self.check(sim, sim.host.owners[2] == "ai" and sim.seated[0].owners[2] == "ai",
                   "seat not released: %r" % owners_everywhere(sim, 2))
        self.check(sim, sim.root_conflicts() == [], "roots differ")

    def test_cancel_after_a_is_published_releases_the_seat(self):
        sim = ja.Sim(cfg(), seated=1)
        sim.add_join("J1", 0, 2)
        sim.cancel("J1", 1700)  # accepted at 1610, turn 18 is not yet produced
        sim.run(5000)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.accept_ms == 1610 and jr.restart_reason == 8, "state %r" % (summary(sim),))
        self.check(sim, sim.host.owners[2] == "ai" and sim.seated[0].owners[2] == "ai",
                   "seat not released on every peer")
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
        j1, j2, j2b = (sim.host.joins[n] for n in ("J1", "J2", "J2b"))
        self.check(sim, j2.restart_reason == 8 and j2b.S < j1.A, "J2b did not start before A: %r" % ((j2b.S, j1.A),))
        self.check(sim, sim.joiners["J2b"].active_ms is not None, "J2b not active")
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
    def test_exact_deadline_does_not_accept(self):
        sim = ja.row_deadline_collision(cfg(), True)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.accept_ms is None and jr.restart_ms == 3000 and jr.restart_reason == 1, repr(summary(sim)))

    def test_one_ms_earlier_accepts(self):
        sim = ja.row_deadline_collision(cfg(), False)
        jr = sim.host.joins["J1"]
        self.check(sim, jr.accept_ms == 2999 and jr.A == 31, repr(summary(sim)))

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
        self.check(sim, len(chan.wait) == 16, "waiting datagrams %d" % len(chan.wait))
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
        self.check(sim, c.next_seq <= 6 and sim.stats[("J1", "H")]["ack"] > c.next_seq, "ack %d seq %d" % (
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
        self.assertEqual(caps["joiner_reverse"], 4 * 75 + 2 * 75 + 59 + 59)

    def test_wire_sizes(self):
        self.assertEqual(ja.turn_bytes([]), 16)
        self.assertEqual(ja.turn_bytes([(1, 2, "human")]), 16 + 14)
        self.assertEqual(ja.HEADER + ja.IP_UDP, 53)


if __name__ == "__main__":
    unittest.main()
