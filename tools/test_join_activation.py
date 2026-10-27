#!/usr/bin/env python3
"""Self-checks for tools/join_activation.py. No network and no game files.

The regressions at the top are the round-cb counterexamples. Run them with
JOIN_RULES=F1 to see them fail against the guards published at 3020900.
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
    """A failing assertion prints the full event trace."""

    def check(self, sim, ok, text):
        if not ok:
            self.fail("%s\n--- event trace ---\n%s\n--- violations: %r" % (
                text, sim.dump(), sim.violations))


def first_seq(kind, direction, **kw):
    """Channel-0 sequence of the first `kind` message on `direction` in a loss-free run."""
    sim = ja.one_join(cfg(**kw), until=3000)
    for _, src, dst, ch, seq, k in sim.deliveries:
        if k == kind and "%s>%s" % (src, dst) == direction:
            return seq
    raise AssertionError("no %s on %s" % (kind, direction))


def in_order(sim):
    """True when each (src, dst, channel 0) pair delivered sequences in increasing order."""
    last = {}
    for _, src, dst, ch, seq, _ in sim.deliveries:
        if ch != 0:
            continue
        if seq <= last.get((src, dst), -1):
            return False
        last[(src, dst)] = seq
    return True


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
        # The first report is lost four times. The retry that gets through is sent
        # at 2360 and arrives at 2510, after the watermark for 20 was sent at 2160.
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
        sim = ja.one_join(cfg(), until=182000, snapshot_ms=160650)
        jr = sim.host.joins["J1"]
        first = jr.watermarks[0] if jr.watermarks else None
        self.check(sim, jr.H == 1770 or jr.H is not None and jr.H <= 1770,
                   "first selected H=%r, watermarks %r" % (jr.H, jr.watermarks[:3]))


if __name__ == "__main__":
    unittest.main()
