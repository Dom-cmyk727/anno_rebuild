#!/usr/bin/env python3
"""Integer model of snapshot join and catch-up (T43, ADR 0003).

This is not a transport and not a measurement of a network. The three save
sizes, the 1188-byte fragment payload, the window of 32 and the fluid times
80.3 s / 160.6 s are the T41 figures. Everything else below is an explicit
model assumption (A1-A10 in `assumptions()`).

Run:

  python -I tools/snapshot_catchup.py
  python -I tools/test_snapshot_catchup.py
"""

from __future__ import annotations

import heapq
from dataclasses import dataclass, replace

# T41 `anno-sim-replay measure`, 30 000 ticks, copied from ADR 0003 §13.
MEASURED_SAVES = (
    ("S01_07_01", 11474169),
    ("endless02", 20349454),
    ("03_1024_med_256", 12906633),
)
# Labeled multiples of the measured endless02 save. Not measured worlds.
SYNTHETIC_SAVES = (
    ("synthetic_2x_endless02", 2 * 20349454),
    ("synthetic_4x_endless02", 4 * 20349454),
)
PUBLISHED_FRAGMENTS = {
    "S01_07_01": 9659,
    "endless02": 17130,
    "03_1024_med_256": 10865,
}
# T41 F1 fluid quotient, rounded half up to 0.1 s, as tenths of a second.
# Keys are (scenario, rtt_ms).
PUBLISHED_FLUID_TENTHS = {
    ("S01_07_01", 150): 453,
    ("S01_07_01", 300): 906,
    ("endless02", 150): 803,
    ("endless02", 300): 1606,
    ("03_1024_med_256", 150): 509,
    ("03_1024_med_256", 300): 1019,
}
PUBLISHED_LINE_MS = {
    ("S01_07_01", 100_000_000): 918,
    ("S01_07_01", 1_000_000_000): 92,
    ("endless02", 100_000_000): 1628,
    ("endless02", 1_000_000_000): 163,
    ("03_1024_med_256", 100_000_000): 1033,
    ("03_1024_med_256", 1_000_000_000): 103,
}

FULL_CHECK_MIN = 10
HEADER_BYTES = 25
IPV4_UDP_BYTES = 28
EMPTY_TURN_PAYLOAD = 16
# ack_bits covers this many sequences before the highest one received.
# The sender must not run further ahead than this, or a hole can no longer
# be named and a later ack cannot confirm the resend either.
ACK_BITS = 32


def assumptions():
    """Every non-measured input the model uses, in one place."""
    return (
        "A1. Measured inputs are the three T41 save sizes, 1188 save bytes "
        "per fragment, window 32, turn 100 ms, rolling buffer 600. The fluid "
        "tenths are the T41 F1 quotients. They are not replaced by this model.",
        "A2. The acceptance latency is 150 ms one way, so the RTT is 300 ms "
        "(ADR §14). The 150 ms RTT column is the other reading of the same "
        "sentence, reproduced and not used as the acceptance case.",
        "A3. Datagram serialization time is not on the event clock. One window "
        "of 32 payloads of 1200 bytes is 3.072 ms at 100 Mbit/s and 0.307 ms "
        "at 1 Gbit/s, both under a 150 ms one-way delay. The clock is invalid "
        "for a window whose serialization exceeds the one-way delay.",
        "A4. Loss is an independent integer draw per send attempt, 20 000 per "
        "1 000 000 for 2 percent, fixed LCG seed. It is not a measured path.",
        "A5. Admission multiplies the zero-loss discrete completion by 2. "
        "That factor is a budget allowance, not a measured loss multiplier.",
        "A6. load_ms defaults to 0 and replay_turns_per_s defaults to 100. "
        "Neither was measured (OQ-281). The runtime refuses a joiner the "
        "allowance cannot hold.",
        "A7. Each retained turn is charged turn_store_cap 1200 bytes, the "
        "payload cap. That is not a measured command size (OQ-282). The "
        "empty-turn datagram of 69 bytes is only the concurrent-wire check.",
        "A8. Turns use channel 0 while the snapshot uses channel 1. The join "
        "waits for the slower of the two. With a 69-byte turn datagram the "
        "snapshot is slower for every measured save.",
        "A9. The acceptance case sets resend_ms to the RTT, 300. The LAN "
        "default of 50 ms stays the LAN default. At a 300 ms RTT a 50 ms "
        "resend repeats a window before its ack returns. The sender also "
        "keeps the oldest unacked sequence inside the 32-bit ack field; a "
        "larger window does not step past that field.",
        "A10. No host processing is added beyond load_ms and the replay rate. "
        "The results are not end-to-end join times.",
    )


@dataclass(frozen=True)
class Budgets:
    payload_max: int = 1200
    fragment_head: int = 12
    save_per_fragment: int = 1188
    fragment_max: int = 17130
    window: int = 32
    one_way_ms: int = 150
    resend_ms: int = 300
    turn_ms: int = 100
    live_turns_per_s: int = 10
    turn_buffer: int = 600
    load_ms: int = 0
    replay_turns_per_s: int = 100
    catchup_ms: int = 600_000
    snapshot_age_ms: int = 60_000
    concurrent_transfers: int = 2
    turn_store_cap: int = 1200
    turn_datagram_bytes: int = HEADER_BYTES + EMPTY_TURN_PAYLOAD + IPV4_UDP_BYTES
    loss_scale_num: int = 2
    loss_scale_den: int = 1
    max_attempts: int = 8
    tail_turns_max: int = 4096
    loss_per_million: int = 0
    seed: int = 1

    @property
    def rtt_ms(self):
        return self.one_way_ms * 2

    @property
    def tail_bytes_max(self):
        return self.tail_turns_max * self.turn_store_cap


def proposed_budgets():
    """Acceptance-case budgets. `tail_turns_max` is the power of two at or
    above the admission pin for the measured largest save."""
    base = Budgets()
    need = tail_turns_needed(fragment_count(20349454, base.save_per_fragment), base)
    cap = next_pow2(need)
    return replace(base, tail_turns_max=cap)


def next_pow2(n):
    if n < 1:
        return 1
    p = 1
    while p < n:
        p *= 2
    return p


def fragment_count(save_bytes, save_per_fragment=1188):
    if save_bytes < 0:
        raise ValueError("save_bytes")
    if save_bytes == 0:
        return 0
    return (save_bytes + save_per_fragment - 1) // save_per_fragment


def round_div(num, den):
    """Round half up for num/den, both non-negative integers."""
    if den <= 0 or num < 0:
        raise ValueError("round_div")
    return (2 * num + den) // (2 * den)


def fluid_tenths(fragments, window, rtt_ms):
    """T41 F1 quotient fragments/window*rtt, in tenths of a second."""
    if window <= 0 or rtt_ms < 0 or fragments < 0:
        raise ValueError("fluid_tenths")
    if fragments == 0 or rtt_ms == 0:
        return 0
    return round_div(fragments * rtt_ms, window * 100)


def fluid_ms_num_den(fragments, window, rtt_ms):
    """Exact fluid duration as the rational (fragments * rtt_ms) / window."""
    return fragments * rtt_ms, window


def line_rate_ms(save_bytes, bits_per_s):
    """Save bytes on an idle line, headers ignored, rounded half up to ms."""
    return round_div(save_bytes * 8 * 1000, bits_per_s)


def window_serialize_us(window, payload, bits_per_s):
    """Serialization of one full window, integer microseconds, round half up."""
    return round_div(window * payload * 8 * 1_000_000, bits_per_s)


def discrete_zero_loss_ms(fragments, window, rtt_ms, one_way_ms):
    """Ack-clocked burst model, no loss, no serialization delay.

    Fragment i is sent at (i // window) * rtt. It arrives one_way later.
    Completion is the arrival of fragment n-1.
    """
    if fragments <= 0:
        return 0
    if window <= 0:
        raise ValueError("window")
    return ((fragments - 1) // window) * rtt_ms + one_way_ms


def estimate_transfer_ms(fragments, cfg):
    base = discrete_zero_loss_ms(fragments, cfg.window, cfg.rtt_ms, cfg.one_way_ms)
    return (base * cfg.loss_scale_num + cfg.loss_scale_den - 1) // cfg.loss_scale_den


def tail_turns_needed(fragments, cfg):
    """Pin length the admission rule holds for this fragment count.

    gap turns arise during the estimated transfer and load. The chase
    `J = gap + ceil(live * gap / (replay - live))` is the model's catch-up
    rule (A6), an integer upper bound under a constant replay rate.
    """
    if cfg.replay_turns_per_s <= cfg.live_turns_per_s:
        return None
    t = estimate_transfer_ms(fragments, cfg) + cfg.load_ms
    gap = t // cfg.turn_ms
    den = cfg.replay_turns_per_s - cfg.live_turns_per_s
    extra = (cfg.live_turns_per_s * gap + den - 1) // den
    return gap + extra


def catchup_ms_after_load(gap, cfg):
    if gap < 0:
        raise ValueError("gap")
    if gap == 0:
        return 0
    if cfg.replay_turns_per_s <= cfg.live_turns_per_s:
        return None
    den = cfg.replay_turns_per_s - cfg.live_turns_per_s
    extra = (cfg.live_turns_per_s * gap + den - 1) // den
    j = gap + extra
    return (j * 1000 + cfg.replay_turns_per_s - 1) // cfg.replay_turns_per_s


def turn_fluid_ms(turn_count, cfg):
    """Same fluid quotient as a snapshot, applied to turn datagrams."""
    if turn_count <= 0:
        return 0
    num, den = fluid_ms_num_den(turn_count, cfg.window, cfg.rtt_ms)
    return round_div(num, den)


class LCG:
    """Numerical Recipes LCG, masked to 31 bits. Rejection sampling keeps
    `below` uniform on 0 .. modulus-1."""

    def __init__(self, seed):
        self.state = seed & 0x7FFFFFFF
        if self.state == 0:
            self.state = 1

    def next_u31(self):
        self.state = (1103515245 * self.state + 12345) & 0x7FFFFFFF
        return self.state

    def below(self, n, modulus=1_000_000):
        if n <= 0:
            return False
        if n >= modulus:
            return True
        span = 0x80000000
        limit = (span // modulus) * modulus
        while True:
            draw = self.next_u31()
            if draw < limit:
                return (draw % modulus) < n


def ack_bits(highest, have):
    """Bit i set means sequence highest-(i+1) is in `have`."""
    bits = 0
    if highest < 0:
        return 0
    for i in range(32):
        seq = highest - (i + 1)
        if seq >= 0 and seq in have:
            bits |= 1 << i
    return bits


def sequences_named_by_ack(ack, bits):
    """The 33-packet selective ack. `ack` itself counts as received."""
    if ack < 0:
        return set()
    named = {ack}
    for i in range(32):
        if bits & (1 << i):
            seq = ack - (i + 1)
            if seq >= 0:
                named.add(seq)
    return named


def contiguous_cursor(have):
    cursor = 0
    while cursor in have:
        cursor += 1
    return cursor


@dataclass(frozen=True)
class TransferResult:
    status: str
    done_ms: int
    fragments: int
    cursor: int
    sends: int
    waited: bool
    highest_ack: int
    max_span: int


def run_transfer(fragments, cfg, drop=None, reorder=None, policy="selective",
                 horizon_ms=None):
    """Discrete channel-1 transfer.

    `policy="selective"` resends a hole the ack names, a sequence that has
    fallen off the 32-bit field, and a sequence whose resend timer fired.
    `policy="skip"` advances the send window from the highest received
    sequence and does not resend. A skip that finishes the send loop with a
    hole returns status `incomplete`.
    """
    if policy not in ("selective", "skip"):
        raise ValueError(policy)
    if drop is None:
        rng = LCG(cfg.seed)

        def drop(seq, attempt, _rng=rng, _n=cfg.loss_per_million):
            del seq, attempt
            return _rng.below(_n)

    if reorder is None:
        def reorder(seq, attempt):
            del seq, attempt
            return 0

    if horizon_ms is None:
        horizon_ms = max(cfg.catchup_ms, estimate_transfer_ms(max(fragments, 1), cfg) * 8 + 1)

    # kind priorities at one timestamp: ack, deliver, resend.
    prio = {"ack": 0, "deliver": 1, "resend": 2}
    queue = []
    serial = 0
    now = 0
    next_seq = 0
    inflight = {}
    have = set()
    highest = -1
    cursor = 0
    sends = 0
    waited = False
    exhausted = False
    max_span = 0

    def push(when, kind, seq, attempt, bits=0, ack_sent=0):
        nonlocal serial
        heapq.heappush(queue, (when, prio[kind], serial, kind, seq, attempt, bits, ack_sent))
        serial += 1

    def start(seq, attempt, when):
        nonlocal sends, exhausted, max_span
        if attempt >= cfg.max_attempts:
            exhausted = True
            return
        inflight[seq] = (attempt, when)
        if len(inflight) >= 2:
            span = max(inflight) - min(inflight)
            if span > max_span:
                max_span = span
        sends += 1
        if policy == "selective":
            push(when + cfg.resend_ms, "resend", seq, attempt)
        if not drop(seq, attempt):
            extra = reorder(seq, attempt)
            push(when + cfg.one_way_ms + extra, "deliver", seq, attempt)

    def pump():
        nonlocal next_seq
        while len(inflight) < cfg.window and next_seq < fragments and not exhausted:
            if inflight and next_seq > min(inflight) + ACK_BITS:
                break
            seq = next_seq
            next_seq += 1
            start(seq, 0, now)

    def resend_now(seq, when, ack_sent):
        if seq not in inflight:
            return
        attempt, sent_at = inflight[seq]
        if sent_at > ack_sent - cfg.one_way_ms:
            return
        start(seq, attempt + 1, when)

    pump()
    while queue and cursor < fragments and not exhausted:
        now, _p, _s, kind, seq, attempt, bits, ack_sent = heapq.heappop(queue)
        if now > horizon_ms:
            return TransferResult("timeout", now, fragments, cursor, sends, waited, highest,
                                  max_span)
        if kind == "deliver":
            if seq in have:
                continue
            if seq > cursor:
                waited = True
            have.add(seq)
            if seq > highest:
                highest = seq
            while cursor in have:
                cursor += 1
            if cursor < fragments and highest >= 0:
                push(now + cfg.one_way_ms, "ack", highest, 0,
                     ack_bits(highest, have), now)
        elif kind == "ack":
            if policy == "skip":
                for old in list(inflight):
                    if old <= seq:
                        del inflight[old]
            else:
                named = sequences_named_by_ack(seq, bits)
                for got in named:
                    inflight.pop(got, None)
                holes = []
                for pending in inflight:
                    if pending <= seq - 32 or (pending < seq and pending not in named):
                        holes.append(pending)
                for pending in holes:
                    resend_now(pending, now, ack_sent)
        elif kind == "resend":
            if policy != "selective":
                continue
            current = inflight.get(seq)
            if current is None or current[0] != attempt:
                continue
            start(seq, attempt + 1, now)
        if exhausted:
            break
        pump()

    if exhausted:
        return TransferResult("exhausted", now, fragments, cursor, sends, waited, highest,
                              max_span)
    if cursor == fragments:
        return TransferResult("complete", now, fragments, cursor, sends, waited, highest,
                              max_span)
    if not queue and next_seq >= fragments and not inflight:
        return TransferResult("incomplete", now, fragments, cursor, sends, waited, highest,
                              max_span)
    return TransferResult("timeout", now, fragments, cursor, sends, waited, highest, max_span)


@dataclass(frozen=True)
class JoinResult:
    status: str
    reason: str
    paused: bool
    snapshot_turn: int
    first_turn: int
    last_turn: int
    missing: int
    transfer_ms: int
    done_ms: int
    pin_turns: int

    def honest(self):
        if self.paused:
            return False
        if self.status == "success":
            return self.missing == 0 and self.last_turn == self.snapshot_turn + self.pin_turns and (
                self.pin_turns == 0 or self.first_turn == self.snapshot_turn + 1
            )
        return self.status in ("refuse", "restart")


def _fail(status, reason, snapshot_turn, transfer_ms, done_ms, missing):
    return JoinResult(status, reason, False, snapshot_turn, snapshot_turn + 1,
                      snapshot_turn, missing, transfer_ms, done_ms, 0)


def remaining_at_deadline(transfer_ms, cfg):
    """Turns the joiner still has not applied when `catchup_ms` fires.
    Negative means the integer rate would already have passed the host."""
    start = transfer_ms + cfg.load_ms
    if start >= cfg.catchup_ms:
        return cfg.catchup_ms // cfg.turn_ms
    applied = (cfg.catchup_ms - start) * cfg.replay_turns_per_s // 1000
    host = cfg.catchup_ms // cfg.turn_ms
    return host - applied


def rolling_contains(turn, snapshot_turn, host_turn, cfg):
    if turn <= snapshot_turn or turn > host_turn:
        return False
    oldest = host_turn - cfg.turn_buffer + 1
    return turn >= oldest


def evaluate_join(save_bytes, cfg, mode="pinned", transfer=None, snapshot_turn=0,
                  age_turns=0):
    """Join outcomes.

    `pinned` is the proposed contract: a save at `snapshot_turn` and a pin of
    every later turn until catch-up or a budget trip.
    `rolling` uses only the 600-turn log. A gap that log no longer holds is
    a refusal, never a success with a hole.
    `periodic` is a stored snapshot `age_turns` behind the host with no pin.
    """
    if mode == "periodic":
        missing = age_turns - cfg.turn_buffer if age_turns > cfg.turn_buffer else 0
        if missing > 0:
            return _fail("refuse", "gap_not_retained", snapshot_turn, 0, 0, missing)
        return JoinResult("success", "within_rolling_buffer", False, snapshot_turn,
                          snapshot_turn + 1, snapshot_turn + age_turns, 0, 0, 0, age_turns)

    fragments = fragment_count(save_bytes, cfg.save_per_fragment)
    if fragments > cfg.fragment_max:
        return _fail("refuse", "fragment_cap", snapshot_turn, 0, 0, 0)
    if transfer is None:
        transfer = run_transfer(fragments, cfg)
    if transfer.status != "complete":
        reason = "missing_fragments" if transfer.status == "incomplete" else transfer.status
        missing = transfer.fragments - transfer.cursor
        return _fail("restart", reason, snapshot_turn, transfer.done_ms, transfer.done_ms, missing)

    # A8: channel 0 carries the turns that appear while the snapshot is in
    # flight. If that wire is slower, the load waits for it, and the extra
    # wait can itself create turns. Two passes close the integer fixed point
    # for the rates in this model (turn datagrams are one window slot each).
    ready_ms = transfer.done_ms
    for _ in range(2):
        wire_ms = turn_fluid_ms(ready_ms // cfg.turn_ms, cfg)
        if wire_ms > ready_ms:
            ready_ms = wire_ms
    load_done = ready_ms + cfg.load_ms
    gap = load_done // cfg.turn_ms
    if load_done > cfg.catchup_ms or (
            cfg.replay_turns_per_s <= cfg.live_turns_per_s and gap > 0):
        return _fail("restart", "cannot_outrun", snapshot_turn, transfer.done_ms,
                     cfg.catchup_ms, max(remaining_at_deadline(transfer.done_ms, cfg), 1))

    extra_ms = catchup_ms_after_load(gap, cfg)
    if extra_ms is None:
        return _fail("restart", "cannot_outrun", snapshot_turn, transfer.done_ms,
                     cfg.catchup_ms, max(remaining_at_deadline(transfer.done_ms, cfg), 1))
    done_ms = load_done + extra_ms
    den = cfg.replay_turns_per_s - cfg.live_turns_per_s
    extra_turns = 0 if gap == 0 else (cfg.live_turns_per_s * gap + den - 1) // den
    pin = gap + extra_turns
    if mode == "rolling" and pin > cfg.turn_buffer:
        return _fail("refuse", "gap_not_retained", snapshot_turn, transfer.done_ms,
                     done_ms, pin - cfg.turn_buffer)
    if done_ms > cfg.catchup_ms:
        return _fail("restart", "catchup_ms", snapshot_turn, transfer.done_ms,
                     cfg.catchup_ms, max(remaining_at_deadline(transfer.done_ms, cfg), 1))
    if pin > cfg.tail_turns_max:
        return _fail("restart", "tail_turns", snapshot_turn, transfer.done_ms, done_ms, pin)
    if pin * cfg.turn_store_cap > cfg.tail_bytes_max:
        return _fail("restart", "tail_bytes", snapshot_turn, transfer.done_ms, done_ms, pin)
    return JoinResult("success", "caught_up", False, snapshot_turn, snapshot_turn + 1,
                      snapshot_turn + pin, 0, transfer.done_ms, done_ms, pin)


def admit(save_bytes, cfg, active_transfers=0):
    """Admission, before any datagram. A refusal pauses nobody."""
    fragments = fragment_count(save_bytes, cfg.save_per_fragment)
    if fragments > cfg.fragment_max:
        return "refuse", "fragment_cap", fragments
    if active_transfers >= cfg.concurrent_transfers:
        return "refuse", "concurrent", fragments
    if cfg.replay_turns_per_s <= cfg.live_turns_per_s:
        return "refuse", "cannot_outrun", fragments
    need = tail_turns_needed(fragments, cfg)
    if need > cfg.tail_turns_max:
        return "refuse", "tail_turns", need
    est = estimate_transfer_ms(fragments, cfg) + cfg.load_ms
    gap = est // cfg.turn_ms
    chase = catchup_ms_after_load(gap, cfg)
    if chase is None or est + chase > cfg.catchup_ms:
        return "refuse", "catchup_ms", need
    return "admit", "within_budgets", need


def allocation_bound(cfg):
    """Worst-case bytes the proposal allows at once. Caps, not one save."""
    frag_bytes = cfg.fragment_max * cfg.save_per_fragment
    parts = {
        "snapshot_store": cfg.concurrent_transfers * frag_bytes,
        "reassembly": cfg.concurrent_transfers * frag_bytes,
        "send_windows": cfg.concurrent_transfers * cfg.window * cfg.payload_max,
        "pinned_turns": cfg.tail_turns_max * cfg.turn_store_cap,
        "rolling_turns": cfg.turn_buffer * cfg.turn_store_cap,
    }
    parts["total"] = sum(parts.values())
    return parts


def actual_allocation(generations, reassemblies, pinned_turns, cfg, rounded_save):
    return (generations * rounded_save
            + reassemblies * rounded_save
            + reassemblies * cfg.window * cfg.payload_max
            + pinned_turns * cfg.turn_store_cap
            + cfg.turn_buffer * cfg.turn_store_cap)


class Generation:
    """One snapshot image and the joiners allowed to use it.

    A joiner who asks at time t shares this image when
    `t - created_ms <= snapshot_age_ms` and a transfer slot is free.
    The pin is turns (S, H] and stays while any joiner is running.
    """

    def __init__(self, snapshot_turn, created_ms, save_bytes):
        self.snapshot_turn = snapshot_turn
        self.created_ms = created_ms
        self.save_bytes = save_bytes
        self.joiners = {}

    def rounded(self, cfg):
        return fragment_count(self.save_bytes, cfg.save_per_fragment) * cfg.save_per_fragment

    def running(self):
        return sum(1 for state in self.joiners.values() if state == "running")

    def attach(self, joiner_id, now_ms, cfg):
        if now_ms - self.created_ms > cfg.snapshot_age_ms:
            return "fresh_snapshot"
        if self.running() >= cfg.concurrent_transfers:
            return "refuse"
        self.joiners[joiner_id] = "running"
        return "shared" if self.running() > 1 else "opened"

    def cancel(self, joiner_id):
        if self.joiners.get(joiner_id) == "running":
            self.joiners[joiner_id] = "cancelled"

    def complete(self, joiner_id):
        if self.joiners.get(joiner_id) == "running":
            self.joiners[joiner_id] = "complete"

    def migrate(self):
        """Proposed host hand-over: an in-progress join restarts. The
        successor is not given this image. The join does not pause the table;
        §9's handover pause is a different flag and is not set here."""
        restarted = []
        for joiner_id, state in list(self.joiners.items()):
            if state == "running":
                self.joiners[joiner_id] = "restart"
                restarted.append(joiner_id)
        return restarted

    def pin_holds(self, turn, host_turn):
        if turn <= self.snapshot_turn or turn > host_turn:
            return False
        return self.running() > 0

    def retained(self, turn, host_turn, cfg):
        if self.pin_holds(turn, host_turn):
            return True
        return rolling_contains(turn, self.snapshot_turn, host_turn, cfg)


def mismatch_resync(host_turn):
    """Pause is allowed. The snapshot is taken after the pause, at the last
    applied turn, so the replay tail is empty."""
    return {
        "paused": True,
        "snapshot_turn": host_turn,
        "first_retained_turn": host_turn + 1,
        "last_applied_turn": host_turn,
        "tail_turns": 0,
    }


def periodic_age_turns(cfg):
    return FULL_CHECK_MIN * 60 * 1000 // cfg.turn_ms


def _scenario_rows(cfg):
    rows = []
    for name, size in MEASURED_SAVES:
        fragments = fragment_count(size, cfg.save_per_fragment)
        rows.append((name, size, fragments, False))
    for name, size in SYNTHETIC_SAVES:
        fragments = fragment_count(size, cfg.save_per_fragment)
        rows.append((name, size, fragments, True))
    return rows


def report_lines(cfg=None, with_loss=False):
    if cfg is None:
        cfg = proposed_budgets()
    bound = allocation_bound(cfg)
    lines = [
        "snapshot-catchup model",
        "status: proposal, not a measured network",
        "tail_turns_max: %d" % cfg.tail_turns_max,
        "tail_bytes_max: %d" % cfg.tail_bytes_max,
        "allocation_bound_bytes: %d" % bound["total"],
        "snapshot_store_bytes: %d" % bound["snapshot_store"],
        "reassembly_bytes: %d" % bound["reassembly"],
        "send_window_bytes: %d" % bound["send_windows"],
        "pinned_turn_bytes: %d" % bound["pinned_turns"],
        "rolling_turn_bytes: %d" % bound["rolling_turns"],
        "one_way_ms: %d" % cfg.one_way_ms,
        "rtt_ms: %d" % cfg.rtt_ms,
        "resend_ms: %d" % cfg.resend_ms,
        "load_ms: %d" % cfg.load_ms,
        "replay_turns_per_s: %d" % cfg.replay_turns_per_s,
        "loss_scale: %d/%d" % (cfg.loss_scale_num, cfg.loss_scale_den),
    ]
    for name, size, fragments, synthetic in _scenario_rows(cfg):
        kind = "synthetic" if synthetic else "measured"
        decision, reason, need = admit(size, cfg)
        fluid_150 = fluid_tenths(fragments, cfg.window, 150)
        fluid_300 = fluid_tenths(fragments, cfg.window, 300)
        zero = discrete_zero_loss_ms(fragments, cfg.window, cfg.rtt_ms, cfg.one_way_ms)
        lines.append(
            "%s %s bytes=%d fragments=%d admit=%s reason=%s detail=%s "
            "fluid_150ms_rtt_tenths=%d fluid_300ms_rtt_tenths=%d "
            "discrete_zero_loss_ms=%d"
            % (kind, name, size, fragments, decision, reason, need,
               fluid_150, fluid_300, zero))
        if with_loss and not synthetic and decision == "admit":
            loss_cfg = replace(cfg, loss_per_million=20_000, seed=1)
            result = evaluate_join(size, loss_cfg)
            lines.append(
                "model %s loss=2pct seed=1 status=%s reason=%s transfer_ms=%d "
                "done_ms=%d pin_turns=%d missing=%d paused=%s"
                % (name, result.status, result.reason, result.transfer_ms,
                   result.done_ms, result.pin_turns, result.missing, result.paused))
    lines.append("periodic_age_turns: %d" % periodic_age_turns(cfg))
    lines.extend("assumption: " + item for item in assumptions())
    return lines


def main():
    for line in report_lines(with_loss=True):
        print(line)


if __name__ == "__main__":
    main()
