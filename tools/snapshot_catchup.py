#!/usr/bin/env python3
"""Integer model of snapshot join and catch-up (T43, ADR 0003).

This is not a transport and not a measurement of a network. The three save
sizes, the 1188-byte fragment payload, the window of 32 and the fluid times
80.3 s / 160.6 s are the T41 figures. Everything else below is an explicit
model assumption (A1-A14 in `assumptions()`). A8 is the optimistic fluid
turn receipt. A14 is the causal per-turn clock. The fluid figures stay.

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
# T45 remeasure of save() at the same 30_000-tick age, stream 114.
# Each file is one byte above the T41 figure. These sizes do not replace
# MEASURED_SAVES. The fragment counts are unchanged.
T45_SAVE_BYTES = (
    ("S01_07_01", 11474170),
    ("endless02", 20349455),
    ("03_1024_med_256", 12906634),
)
# anno-sim-replay join, 2026-10-27. load_* is World::load of bytes already
# in memory, 20 samples after 2 warm-up loads. empty_* and script_* are one
# run_turn (10 steps of 10 game ms), 150 samples: 5 repetitions of 30 turns
# after 2 warm-up sequences. The hash is not in these times.
T45_REPLAY_SAMPLES = 150
T45_TIMING_US = {
    "S01_07_01": {
        "load_min": 24335, "load_median": 26807, "load_p95": 27702, "load_max": 27977,
        "empty_min": 232, "empty_median": 604, "empty_p95": 1473, "empty_max": 1616,
        "empty_duration": 104088,
        "script_min": 232, "script_median": 659, "script_p95": 1627, "script_max": 1966,
        "script_duration": 113867,
    },
    "endless02": {
        "load_min": 42858, "load_median": 46004, "load_p95": 51401, "load_max": 54239,
        "empty_min": 237, "empty_median": 426, "empty_p95": 976, "empty_max": 2278,
        "empty_duration": 80699,
        "script_min": 234, "script_median": 397, "script_p95": 925, "script_max": 2222,
        "script_duration": 78319,
    },
    "03_1024_med_256": {
        "load_min": 25018, "load_median": 27647, "load_p95": 31175, "load_max": 32570,
        "empty_min": 171, "empty_median": 214, "empty_p95": 288, "empty_max": 533,
        "empty_duration": 33791,
        "script_min": 171, "script_median": 217, "script_p95": 330, "script_max": 635,
        "script_duration": 35330,
    },
}
# Encoded TurnEntry bytes from the same runs. The empty command stream is 0.
# One scripted SetSpeed record is 16. The burst is 76 records, 1216 bytes.
# None of these is the ADR empty datagram, and none replaces turn_store_cap.
T45_EMPTY_PAYLOAD = 0
T45_SCRIPT_PAYLOAD = 16
T45_SCRIPT_COMMANDS = 1
T45_BURST_PAYLOAD = 1216
T45_BURST_COMMANDS = 76
T45_BURST_CAPACITY = 1228
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
        "T45 measured World::load and run_turn on one host. Those inputs are "
        "a labeled scenario and do not replace these defaults (OQ-281). The "
        "runtime refuses a joiner the allowance cannot hold.",
        "A7. Each retained turn is charged turn_store_cap 1200 bytes, the "
        "payload cap. T45 measured an empty command stream, one scripted "
        "SetSpeed per turn, and a synthetic burst. Those sizes do not replace "
        "the cap, and a real-play distribution was not measured (OQ-282). The "
        "empty-turn datagram of 69 bytes is only the concurrent-wire check.",
        "A8. Turns use channel 0 while the snapshot uses channel 1. A full "
        "channel-1 window does not delay channel 0. Receipt on this clock is "
        "the fluid bulk time of the turn datagrams, not a second one-way "
        "delay on top of the snapshot. With a 69-byte turn datagram the "
        "snapshot is slower for every measured save, so at load every turn "
        "the host has produced is in that joiner's backlog. The host pin and "
        "each joiner's backlog are both charged.",
        "A9. The acceptance case sets resend_ms to the RTT, 300. The LAN "
        "default of 50 ms stays the LAN default. At a 300 ms RTT a 50 ms "
        "resend repeats a window before its ack returns. The sender also "
        "keeps the oldest unacked sequence inside the 32-bit ack field; a "
        "larger window does not step past that field.",
        "A10. No host processing is added beyond load_ms and the replay rate. "
        "The results are not end-to-end join times.",
        "A11. tail_turns_needed is a reserved upper bound. It is not a turn "
        "the joiner has applied. Success is the first time the replayed "
        "cursor equals the host cursor, with the received cursor covering "
        "that prefix and no replay past the host.",
        "A12. loss_per_million drops data datagrams only. It does not drop "
        "acknowledgements, and on that path a duplicate arrival does not "
        "generate an acknowledgement. ack_duplicates=True generates one. "
        "ack_drop drops acknowledgements. Those two schedules are separate "
        "from the 2% rows. The 2% rows are not general datagram-loss coverage.",
        "A13. Table.advance is the shared owner's host and time step. Before "
        "the new turn is retained it restarts a running joiner whose own "
        "start is at least catchup_ms earlier, then restarts the oldest "
        "generations until the remaining pin fits in tail_turns_max and "
        "tail_bytes_max. A younger pin that still fits stays. Table.attach "
        "refuses a save past fragment_max without storing it. "
        "Table.note_backlog restarts a backlog past receive_backlog_turns. "
        "The table is not paused. This bounds the proposal's intermediate "
        "state. It is not a measured join and not original netcode.",
        "A14. The causal clock does not replace A8. Turn k after the snapshot "
        "is produced at k * turn_ms and cannot arrive before that time plus "
        "one_way_ms. Channel 1 keeps the snapshot window. Channel 0 sends one "
        "turn datagram at production and does not wait for channel 1. "
        "Serialization of that window plus one turn datagram is left off this "
        "clock only while the sum is below the one-way delay. A shared queue, "
        "jitter and the channel-2 hash flight are not modeled. Readiness is "
        "the receivable prefix, not equality with the host, when one_way_ms "
        "is at least turn_ms. That prefix is every turn whose earliest "
        "arrival is at or before t. Applied equal to the turns that happened "
        "to arrive is not readiness while a due turn is missing, and a hash "
        "multiple of that shorter prefix does not enable input. Input waits "
        "for hash_every on the due prefix. Table.advance restarts a "
        "still-running join at catchup_ms before success at that millisecond. "
        "The 2% rows still drop turn data only; an ACK drop is a separate "
        "schedule. The fluid count stays the optimistic comparison. "
        "command_delay stays 2.",
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
    # Caps joined to tail_turns_max by proposed_budgets. Separate so a test
    # can tighten one without the other.
    receive_backlog_turns: int = 4096
    max_generations: int = 2
    # ADR §7. Not a join budget. Existing rows do not read it.
    hash_every: int = 10

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
    return replace(base, tail_turns_max=cap, receive_backlog_turns=cap,
                   max_generations=base.concurrent_transfers)


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


def reserved_pin(gap, cfg):
    """Upper bound on turns a constant replay rate might have to hold.

    This is not the host cursor and not the replayed cursor. Callers that
    report progress use `simulate_catchup`.
    """
    if gap < 0:
        raise ValueError("gap")
    if gap == 0:
        return 0
    if cfg.replay_turns_per_s <= cfg.live_turns_per_s:
        return None
    den = cfg.replay_turns_per_s - cfg.live_turns_per_s
    extra = (cfg.live_turns_per_s * gap + den - 1) // den
    return gap + extra


def tail_turns_needed(fragments, cfg):
    """Reserved pin the admission rule holds for this fragment count.

    gap turns are the host turns at the estimated transfer plus load. `J` is
    `reserved_pin(gap)`, an integer upper bound under a constant replay rate
    (A6, A11). It is not a claim that the joiner applied `J` turns.
    """
    if cfg.replay_turns_per_s <= cfg.live_turns_per_s:
        return None
    t = estimate_transfer_ms(fragments, cfg) + cfg.load_ms
    return reserved_pin(t // cfg.turn_ms, cfg)


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
    acks_sent: int = 0
    acks_lost: int = 0
    duplicate_deliveries: int = 0
    sender_released: bool = False


def run_transfer(fragments, cfg, drop=None, reorder=None, policy="selective",
                 horizon_ms=None, ack_drop=None, ack_duplicates=False):
    """Discrete channel-1 transfer.

    `policy="selective"` resends a hole the ack names, a sequence that has
    fallen off the 32-bit field, and a sequence whose resend timer fired.
    `policy="skip"` advances the send window from the highest received
    sequence and does not resend. A skip that finishes the send loop with a
    hole returns status `incomplete`.

    `drop` applies to data datagrams. `ack_drop(highest, duplicate)` drops
    an acknowledgement when it returns true. A duplicate data arrival
    generates an acknowledgement only when `ack_duplicates` is true (A12).
    The 2% rows leave it false. `done_ms` is the receiver cursor completing.
    Status `complete` also requires the sender's in-flight set to drain.
    Status `unacked` means the receiver finished and the sender did not.
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
    acks_sent = 0
    acks_lost = 0
    duplicate_deliveries = 0
    receiver_done = None

    def push(when, kind, seq, attempt, bits=0, ack_sent=0):
        nonlocal serial
        heapq.heappush(queue, (when, prio[kind], serial, kind, seq, attempt, bits, ack_sent))
        serial += 1

    def finish(status, at):
        done = receiver_done if receiver_done is not None else at
        return TransferResult(status, done, fragments, cursor, sends, waited, highest,
                              max_span, acks_sent, acks_lost, duplicate_deliveries,
                              not inflight and cursor == fragments)

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

    def emit_ack(now, duplicate):
        nonlocal acks_sent, acks_lost
        if highest < 0:
            return
        if ack_drop is not None and ack_drop(highest, duplicate):
            acks_lost += 1
            return
        acks_sent += 1
        push(now + cfg.one_way_ms, "ack", highest, 0,
             ack_bits(highest, have), now)

    pump()
    while queue and not exhausted:
        now, _p, _s, kind, seq, attempt, bits, ack_sent = heapq.heappop(queue)
        if now > horizon_ms:
            return finish("timeout", now)
        if kind == "deliver":
            duplicate = seq in have
            if duplicate:
                duplicate_deliveries += 1
                # The 2% rows leave this off: a duplicate does not generate an
                # acknowledgement, and `drop` never sees an ack-only resend.
                # Recovery tests pass ack_duplicates=True.
                if ack_duplicates:
                    emit_ack(now, True)
            else:
                if seq > cursor:
                    waited = True
                have.add(seq)
                if seq > highest:
                    highest = seq
                while cursor in have:
                    cursor += 1
                emit_ack(now, False)
                if cursor == fragments and receiver_done is None:
                    receiver_done = now
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
        if cursor == fragments and not inflight:
            return finish("complete", now)
        if exhausted:
            break
        pump()

    if cursor == fragments and not inflight:
        return finish("complete", now)
    if cursor == fragments and inflight:
        return finish("unacked", now)
    if exhausted:
        return finish("exhausted", now)
    if not queue and next_seq >= fragments and not inflight:
        return finish("incomplete", now)
    return finish("timeout", now)


@dataclass(frozen=True)
class JoinResult:
    """Cursors are turn indexes. `pin_turns` is how many of them were replayed.

    `reserved_turns` is the admission upper bound. A success does not copy it
    into `replayed`.
    """

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
    host_produced: int
    received: int
    replayed: int
    reserved_turns: int
    backlog_turns: int

    def honest(self, turn_ms=100):
        if self.paused or self.replayed > self.host_produced or self.received > self.host_produced:
            return False
        if self.status != "success":
            return self.status in ("refuse", "restart")
        host_from_clock = self.snapshot_turn + self.done_ms // turn_ms
        return (self.missing == 0
                and self.replayed == self.host_produced
                and self.host_produced == host_from_clock
                and self.received >= self.replayed
                and self.pin_turns == self.replayed - self.snapshot_turn
                and self.last_turn == self.replayed
                and (self.pin_turns == 0 or self.first_turn == self.snapshot_turn + 1)
                and self.reserved_turns >= self.pin_turns)


def _fail(status, reason, snapshot_turn, transfer_ms, done_ms, missing, turn_ms=100,
          reserved=0, backlog=0, host=None):
    if host is None:
        host = snapshot_turn + done_ms // turn_ms
    replayed = snapshot_turn
    return JoinResult(status, reason, False, snapshot_turn, snapshot_turn + 1,
                      snapshot_turn, missing, transfer_ms, done_ms, 0,
                      host, replayed, replayed, reserved, backlog)


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


@dataclass(frozen=True)
class Progress:
    reached: bool
    done_ms: int
    host_produced: int
    received: int
    replayed: int
    backlog_turns: int
    future: bool


def turns_received(elapsed_ms, cfg):
    """Turns after the snapshot that channel 0 has delivered by `elapsed_ms`.

    The count is the fluid bulk time (A8) and never exceeds the turns the
    host has produced. It is not a second one-way delay.
    """
    if elapsed_ms < 0:
        raise ValueError("elapsed")
    produced = elapsed_ms // cfg.turn_ms
    if produced <= 0:
        return 0
    if turn_fluid_ms(produced, cfg) <= elapsed_ms:
        return produced
    lo = 0
    hi = produced
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if turn_fluid_ms(mid, cfg) <= elapsed_ms:
            lo = mid
        else:
            hi = mid - 1
    return lo


def load_ready_ms(transfer_ms, cfg):
    """When the snapshot and the channel-0 backlog have both arrived.

    A slower turn wire extends the wait and that extension produces more
    turns. Measured saves converge on the snapshot time at the first pass.
    """
    ready = transfer_ms
    for _ in range(8):
        wire_ms = turn_fluid_ms(ready // cfg.turn_ms, cfg)
        if wire_ms <= ready:
            return ready
        ready = wire_ms
    return ready


def simulate_catchup(load_done, cfg, snapshot_turn=0, age_turns=0):
    """Replay from `load_done` until the replayed cursor equals the host.

    Applying a turn takes `ceil(1000 / replay_turns_per_s)` ms. The joiner
    applies a turn only when the host has produced it and channel 0 has
    delivered it. The 1 188-byte acceptance case loads at 150 ms. The reserved
    bound for that gap is 2 turns at 170 ms. The host at 170 ms has produced
    1 turn, and this loop meets the host at 160 ms on that turn.
    """
    if cfg.turn_ms <= 0 or cfg.replay_turns_per_s <= 0:
        raise ValueError("rate")
    step = (1000 + cfg.replay_turns_per_s - 1) // cfg.replay_turns_per_s
    t = load_done
    replayed = snapshot_turn
    peak = 0
    limit = cfg.catchup_ms

    def host_at(tt):
        return snapshot_turn + age_turns + tt // cfg.turn_ms

    def received_at(tt):
        # `age_turns` are already in the joiner's buffer when this starts.
        # Turns produced during the wait follow the fluid channel-0 clock.
        got = snapshot_turn + age_turns + turns_received(tt, cfg)
        host = host_at(tt)
        if got > host:
            return host
        return got

    guard = 0
    while t <= limit and guard <= limit:
        guard += 1
        host = host_at(t)
        received = received_at(t)
        backlog = received - replayed
        if backlog > peak:
            peak = backlog
        if replayed > host or received > host:
            return Progress(False, t, host, min(received, host), min(replayed, host),
                            peak, True)
        if replayed == host and received >= replayed:
            return Progress(True, t, host, received, replayed, peak, False)
        if replayed < host and replayed < received:
            replayed += 1
            t += step
            continue
        nxt = (host - snapshot_turn - age_turns + 1) * cfg.turn_ms
        if nxt <= t:
            break
        t = nxt
    if t > limit:
        t = limit
    host = host_at(t)
    received = min(received_at(t), host)
    replayed = min(replayed, host)
    return Progress(False, t, host, received, replayed, peak, False)


def rolling_missing(load_done, snapshot_turn, age_turns, cfg):
    """Turns at or after `snapshot_turn + 1` that the rolling log has dropped.

    `age_turns` is the host's lead at the ask. Transfer and load add the live
    turns `load_done // turn_ms`. A positive result is a hole, not a join.
    """
    host = snapshot_turn + age_turns + load_done // cfg.turn_ms
    oldest = host - cfg.turn_buffer + 1
    first = snapshot_turn + 1
    if first < oldest:
        return oldest - first
    return 0


def _caught(snapshot_turn, transfer_ms, prog, reserved):
    pin = prog.replayed - snapshot_turn
    first = snapshot_turn + 1 if pin else snapshot_turn
    return JoinResult("success", "caught_up", False, snapshot_turn, first,
                      prog.replayed, 0, transfer_ms, prog.done_ms, pin,
                      prog.host_produced, prog.received, prog.replayed, reserved,
                      prog.backlog_turns)


def _stopped(status, reason, snapshot_turn, transfer_ms, prog, missing, reserved):
    return JoinResult(status, reason, False, snapshot_turn, snapshot_turn + 1,
                      prog.replayed, missing, transfer_ms, prog.done_ms, 0,
                      prog.host_produced, prog.received, prog.replayed, reserved,
                      prog.backlog_turns)


def evaluate_join(save_bytes, cfg, mode="pinned", transfer=None, snapshot_turn=0,
                  age_turns=0):
    """Join outcomes.

    `pinned` keeps every turn after `snapshot_turn` until the replayed cursor
    equals the host cursor, or a cap trips.
    `rolling` uses only the turn log. Transfer, load and the live turns they
    contain can drop the tail. That is a refusal, never a success with a hole.
    `periodic` reports whether the stored image is inside the log at the ask.
    The status is `available` or `unavailable`. It is not a join success.
    """
    if mode not in ("pinned", "rolling", "periodic"):
        raise ValueError(mode)
    if mode == "periodic":
        host = snapshot_turn + age_turns
        missing = age_turns - cfg.turn_buffer if age_turns > cfg.turn_buffer else 0
        status = "unavailable" if missing else "available"
        return JoinResult(status, "initial_availability", False, snapshot_turn,
                          snapshot_turn + 1, snapshot_turn, missing, 0, 0, 0,
                          host, snapshot_turn, snapshot_turn, 0, 0)

    fragments = fragment_count(save_bytes, cfg.save_per_fragment)
    if fragments > cfg.fragment_max:
        return _fail("refuse", "fragment_cap", snapshot_turn, 0, 0, 0, cfg.turn_ms)
    if transfer is None:
        transfer = run_transfer(fragments, cfg)
    if transfer.status != "complete":
        reason = "missing_fragments" if transfer.status == "incomplete" else transfer.status
        missing = transfer.fragments - transfer.cursor
        return _fail("restart", reason, snapshot_turn, transfer.done_ms,
                     transfer.done_ms, missing, cfg.turn_ms)

    ready_ms = load_ready_ms(transfer.done_ms, cfg)
    load_done = ready_ms + cfg.load_ms
    gap = load_done // cfg.turn_ms
    if mode == "rolling":
        gap += age_turns
    reserved = reserved_pin(gap, cfg)
    reserved_n = 0 if reserved is None else reserved
    if mode == "rolling":
        missing = rolling_missing(load_done, snapshot_turn, age_turns, cfg)
        if missing > 0:
            host = snapshot_turn + age_turns + load_done // cfg.turn_ms
            return _fail("refuse", "gap_not_retained", snapshot_turn, transfer.done_ms,
                         load_done, missing, cfg.turn_ms, reserved_n, host=host)
    if load_done > cfg.catchup_ms or (cfg.replay_turns_per_s <= cfg.live_turns_per_s and gap > 0):
        return _fail("restart", "cannot_outrun", snapshot_turn, transfer.done_ms,
                     cfg.catchup_ms, max(remaining_at_deadline(transfer.done_ms, cfg), 1),
                     cfg.turn_ms, reserved_n)

    prog = simulate_catchup(load_done, cfg, snapshot_turn,
                            age_turns if mode == "rolling" else 0)
    if prog.future:
        return _stopped("restart", "future_turn", snapshot_turn, transfer.done_ms, prog, 1,
                        reserved_n)
    if not prog.reached:
        reason = "cannot_outrun" if cfg.replay_turns_per_s <= cfg.live_turns_per_s else "catchup_ms"
        missing = max(prog.host_produced - prog.replayed, 1)
        return _stopped("restart", reason, snapshot_turn, transfer.done_ms, prog, missing,
                        reserved_n)
    pin = prog.replayed - snapshot_turn
    if prog.backlog_turns > cfg.receive_backlog_turns:
        return _stopped("restart", "backlog", snapshot_turn, transfer.done_ms, prog,
                        prog.backlog_turns, reserved_n)
    if pin > cfg.tail_turns_max:
        return _stopped("restart", "tail_turns", snapshot_turn, transfer.done_ms, prog, pin,
                        reserved_n)
    if pin * cfg.turn_store_cap > cfg.tail_bytes_max:
        return _stopped("restart", "tail_bytes", snapshot_turn, transfer.done_ms, prog, pin,
                        reserved_n)
    if prog.done_ms > cfg.catchup_ms:
        return _stopped("restart", "catchup_ms", snapshot_turn, transfer.done_ms, prog,
                        max(prog.host_produced - prog.replayed, 1), reserved_n)
    return _caught(snapshot_turn, transfer.done_ms, prog, reserved_n)


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
    # `gap` turns are sitting in the joiner's buffer at load, before replay.
    if gap > cfg.receive_backlog_turns:
        return "refuse", "backlog", gap
    return "admit", "within_budgets", need


def allocation_bound(cfg):
    """Worst-case bytes the proposal allows at once.

    Every part is a cap. `joiner_backlog` is each running joiner's received
    turns waiting for load and replay. `channel0_flight` is the channel-0
    window that delivers those turns. The total is the sum of the parts.
    """
    frag_bytes = cfg.fragment_max * cfg.save_per_fragment
    parts = {
        "snapshot_store": cfg.max_generations * frag_bytes,
        "reassembly": cfg.concurrent_transfers * frag_bytes,
        "send_windows": cfg.concurrent_transfers * cfg.window * cfg.payload_max,
        "channel0_flight": cfg.concurrent_transfers * cfg.window * cfg.turn_datagram_bytes,
        "pinned_turns": cfg.tail_turns_max * cfg.turn_store_cap,
        "joiner_backlog": cfg.concurrent_transfers * cfg.receive_backlog_turns * cfg.turn_store_cap,
        "rolling_turns": cfg.turn_buffer * cfg.turn_store_cap,
    }
    parts["total"] = sum(parts.values())
    return parts


def actual_allocation(generations, reassemblies, pinned_turns, joiner_backlog_turns, cfg,
                      rounded_save):
    return (generations * rounded_save
            + reassemblies * rounded_save
            + reassemblies * cfg.window * cfg.payload_max
            + reassemblies * cfg.window * cfg.turn_datagram_bytes
            + pinned_turns * cfg.turn_store_cap
            + joiner_backlog_turns * cfg.turn_store_cap
            + cfg.turn_buffer * cfg.turn_store_cap)


class Generation:
    """One snapshot image. Transfer slots and the tail live on `Table`.

    The pin is turns (S, H] and stays while any joiner on this image is running.
    """

    def __init__(self, snapshot_turn, created_ms, save_bytes):
        self.snapshot_turn = snapshot_turn
        self.created_ms = created_ms
        self.save_bytes = save_bytes
        self.joiners = {}
        self.started_ms = {}

    def rounded(self, cfg):
        return fragment_count(self.save_bytes, cfg.save_per_fragment) * cfg.save_per_fragment

    def running(self):
        return sum(1 for state in self.joiners.values() if state == "running")

    def add(self, joiner_id, now_ms):
        self.joiners[joiner_id] = "running"
        self.started_ms[joiner_id] = now_ms
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


class Table:
    """Shared owner of generations, transfer slots, pins and joiner backlogs.

    `concurrent_transfers`, `max_generations` and `tail_turns_max` are global.
    A generation does not keep its own copy of the transfer cap. Host turn
    and time move only through `advance`. That step applies deadline and
    tail caps before the new turn is retained (A13).
    """

    def __init__(self, cfg):
        self.cfg = cfg
        self.generations = []
        self.backlog = {}
        self.host_turn = 0
        self.now_ms = 0
        self.paused = False

    def running(self):
        return sum(g.running() for g in self.generations)

    def active_generations(self):
        return [g for g in self.generations if g.running() > 0]

    def _turns_held(self, gens, host_turn):
        held = set()
        for gen in gens:
            if gen.running() <= 0 or host_turn <= gen.snapshot_turn:
                continue
            for turn in range(gen.snapshot_turn + 1, host_turn + 1):
                held.add(turn)
        return held

    def distinct_turns(self):
        return self._turns_held(self.active_generations(), self.host_turn)

    def _span_count(self, gens, host_turn):
        return len(self._turns_held(gens, host_turn))

    def _tail_reason(self, count):
        if count > self.cfg.tail_turns_max:
            return "tail_turns"
        if count * self.cfg.turn_store_cap > self.cfg.tail_bytes_max:
            return "tail_bytes"
        return None

    def _active_ordered(self):
        """Oldest snapshot, then earliest save time, then open order."""
        ranked = []
        for index, gen in enumerate(self.generations):
            if gen.running() > 0:
                ranked.append((gen.snapshot_turn, gen.created_ms, index, gen))
        ranked.sort()
        return [item[3] for item in ranked]

    def tail_allows(self, snapshot_turn):
        held = set(self.distinct_turns())
        if self.host_turn > snapshot_turn:
            for turn in range(snapshot_turn + 1, self.host_turn + 1):
                held.add(turn)
        return self._tail_reason(len(held)) is None

    def advance(self, now_ms, host_turn):
        """Move the shared clock. Caps are applied before `host_turn` is kept.

        A running joiner is restarted when `now_ms - started_ms >= catchup_ms`.
        If the prospective pin still exceeds the tail count or the tail bytes,
        running joiners on the oldest generation restart, then the next oldest,
        until the remaining union fits. A younger generation whose own pin
        fits is kept. The table is not paused.
        """
        if now_ms == self.now_ms and host_turn == self.host_turn:
            return []
        if now_ms < self.now_ms or host_turn < self.host_turn:
            raise ValueError("clock")
        restarted = []
        restarted.extend(self._enforce_deadlines(now_ms))
        restarted.extend(self._enforce_tail(host_turn))
        self.now_ms = now_ms
        self.host_turn = host_turn
        return restarted

    def next_deadline_ms(self):
        """Earliest millisecond at which a running joiner meets the deadline.

        The comparison itself is `_enforce_deadlines`. This is only the
        instant a caller must not skip when it samples `advance`.
        """
        soonest = None
        for gen in self.generations:
            for joiner_id, state in gen.joiners.items():
                if state != "running":
                    continue
                at = gen.started_ms[joiner_id] + self.cfg.catchup_ms
                if soonest is None or at < soonest:
                    soonest = at
        return soonest

    def _enforce_deadlines(self, now_ms):
        restarted = []
        for gen in self.generations:
            for joiner_id, state in list(gen.joiners.items()):
                if state != "running":
                    continue
                if now_ms - gen.started_ms[joiner_id] >= self.cfg.catchup_ms:
                    self._release(joiner_id, "restart")
                    restarted.append((joiner_id, "deadline"))
        return restarted

    def _enforce_tail(self, host_turn):
        gens = self._active_ordered()
        restarted = []
        keep_from = 0
        while keep_from < len(gens):
            reason = self._tail_reason(self._span_count(gens[keep_from:], host_turn))
            if reason is None:
                break
            gen = gens[keep_from]
            for joiner_id, state in list(gen.joiners.items()):
                if state == "running":
                    self._release(joiner_id, "restart")
                    restarted.append((joiner_id, reason))
            keep_from += 1
        return restarted

    def attach(self, joiner_id, now_ms, save_bytes, snapshot_turn, host_turn):
        if fragment_count(save_bytes, self.cfg.save_per_fragment) > self.cfg.fragment_max:
            return "fragment", None
        if now_ms != self.now_ms or host_turn != self.host_turn:
            return "clock", None
        if self.running() >= self.cfg.concurrent_transfers:
            return "refuse", None
        for gen in self.active_generations():
            if (gen.save_bytes == save_bytes
                    and now_ms - gen.created_ms <= self.cfg.snapshot_age_ms):
                word = gen.add(joiner_id, now_ms)
                self.backlog[joiner_id] = 0
                return word, gen
        if len(self.active_generations()) >= self.cfg.max_generations:
            return "refuse", None
        if not self.tail_allows(snapshot_turn):
            return "tail", None
        gen = Generation(snapshot_turn, now_ms, save_bytes)
        word = gen.add(joiner_id, now_ms)
        self.generations.append(gen)
        self.backlog[joiner_id] = 0
        return word, gen

    def _release(self, joiner_id, state):
        for gen in self.generations:
            if gen.joiners.get(joiner_id) == "running":
                gen.joiners[joiner_id] = state
        self.backlog.pop(joiner_id, None)

    def cancel(self, joiner_id):
        self._release(joiner_id, "cancelled")

    def complete(self, joiner_id):
        self._release(joiner_id, "complete")

    def migrate(self):
        restarted = []
        for gen in self.generations:
            restarted.extend(gen.migrate())
        self.backlog.clear()
        return restarted

    def retained(self, turn):
        for gen in self.active_generations():
            if gen.retained(turn, self.host_turn, self.cfg):
                return True
        return False

    def note_backlog(self, joiner_id, turns):
        running = False
        for gen in self.generations:
            if gen.joiners.get(joiner_id) == "running":
                running = True
        if not running:
            return "absent"
        if turns > self.cfg.receive_backlog_turns:
            self._release(joiner_id, "restart")
            return "restart"
        self.backlog[joiner_id] = turns
        return "held"

    def memory_bytes(self):
        cfg = self.cfg
        n = self.running()
        gens = self.active_generations()
        parts = {
            "snapshot_store": sum(gen.rounded(cfg) for gen in gens),
            "reassembly": sum(gen.running() * gen.rounded(cfg) for gen in gens),
            "send_windows": n * cfg.window * cfg.payload_max,
            "channel0_flight": n * cfg.window * cfg.turn_datagram_bytes,
            "pinned_turns": len(self.distinct_turns()) * cfg.turn_store_cap,
            "joiner_backlog": sum(self.backlog.values()) * cfg.turn_store_cap,
            "rolling_turns": cfg.turn_buffer * cfg.turn_store_cap,
        }
        parts["total"] = sum(parts.values())
        return parts

    def over_cap(self):
        mem = self.memory_bytes()
        bound = allocation_bound(self.cfg)
        return [key for key in bound if mem[key] > bound[key]]


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


def t45_ceil_ms(us):
    if us < 0:
        raise ValueError("us")
    return (us + 999) // 1000


def t45_floor_per_s(us):
    if us <= 0:
        raise ValueError("us")
    return 1_000_000 // us


def t45_sum_per_s(duration_us, samples=T45_REPLAY_SAMPLES):
    if duration_us <= 0 or samples <= 0:
        raise ValueError("duration")
    return (samples * 1_000_000) // duration_us


def t45_load_ms(name):
    return t45_ceil_ms(T45_TIMING_US[name]["load_median"])


def t45_replay_rate(name, key):
    row = T45_TIMING_US[name]
    if key not in row:
        raise KeyError(key)
    if key.endswith("_duration"):
        return t45_sum_per_s(row[key])
    return t45_floor_per_s(row[key])


def t45_budgets(name, rate_key):
    """A copy of the proposed defaults with one scenario's measured load and rate.

    `proposed_budgets()` is not modified. `load_ms` is the ceil of the load
    median. A `*_duration` key is the sequence throughput, samples per second
    of the summed sample. Any other key is the floor of one turn per second
    from that sample's microseconds.
    """
    return replace(proposed_budgets(),
                   load_ms=t45_load_ms(name),
                   replay_turns_per_s=t45_replay_rate(name, rate_key))


def t45_report_lines():
    """Labeled comparison. The baseline printout above this is unchanged."""
    lines = [
        "t45 measurement",
        "t45 host cpu=AMD_Ryzen_7_9800X3D cores=8 threads=16 compiler=msvc msvc_ver=1951 ndebug=1",
        "t45 defaults_unchanged load_ms=0 replay_turns_per_s=100 turn_store_cap=1200",
        "t45 encoded empty_payload=%d script_payload=%d script_commands=%d "
        "burst_payload=%d burst_commands=%d burst_capacity=%d burst_over_cap=1 "
        "adr_empty_payload=%d not_a_packet=1"
        % (T45_EMPTY_PAYLOAD, T45_SCRIPT_PAYLOAD, T45_SCRIPT_COMMANDS,
           T45_BURST_PAYLOAD, T45_BURST_COMMANDS, T45_BURST_CAPACITY,
           EMPTY_TURN_PAYLOAD),
    ]
    base = proposed_budgets()
    t41 = dict(MEASURED_SAVES)
    rate_keys = ("script_duration", "script_median", "script_p95", "script_max",
                 "empty_duration", "empty_max")
    for name, size in T45_SAVE_BYTES:
        row = T45_TIMING_US[name]
        fragments = fragment_count(size)
        lines.append(
            "t45 save %s bytes=%d t41_bytes=%d delta=%d fragments=%d"
            % (name, size, t41[name], size - t41[name], fragments))
        lines.append(
            "t45 load %s us_min=%d us_median=%d us_p95=%d us_max=%d load_ms_ceil_median=%d"
            % (name, row["load_min"], row["load_median"], row["load_p95"],
               row["load_max"], t45_load_ms(name)))
        for workload, prefix in (("empty", "empty"), ("scripted", "script")):
            duration = row[prefix + "_duration"]
            lines.append(
                "t45 replay %s workload=%s us_min=%d us_median=%d us_p95=%d us_max=%d "
                "samples=%d duration_us=%d rate_median=%d rate_p95=%d rate_max=%d rate_sum=%d"
                % (name, workload, row[prefix + "_min"], row[prefix + "_median"],
                   row[prefix + "_p95"], row[prefix + "_max"], T45_REPLAY_SAMPLES,
                   duration, t45_floor_per_s(row[prefix + "_median"]),
                   t45_floor_per_s(row[prefix + "_p95"]),
                   t45_floor_per_s(row[prefix + "_max"]),
                   t45_sum_per_s(duration)))
        base_fragments = fragment_count(t41[name])
        base_est = estimate_transfer_ms(base_fragments, base)
        base_gap = (base_est + base.load_ms) // base.turn_ms
        base_decision, _base_reason, base_need = admit(t41[name], base)
        base_result = evaluate_join(t41[name], base)
        for key in rate_keys:
            labeled = t45_budgets(name, key)
            decision, reason, need = admit(size, labeled)
            result = evaluate_join(size, labeled)
            est = estimate_transfer_ms(fragments, labeled)
            gap = (est + labeled.load_ms) // labeled.turn_ms
            lines.append(
                "t45 compare %s rate_key=%s replay=%d load_ms=%d admit=%s reason=%s need=%d "
                "baseline_admit=%s baseline_need=%d status=%s paused=%s pin_turns=%d "
                "baseline_pin_turns=%d done_ms=%d baseline_done_ms=%d gap_turns=%d "
                "baseline_gap_turns=%d"
                % (name, key, labeled.replay_turns_per_s, labeled.load_ms,
                   decision, reason, need, base_decision, base_need,
                   result.status, str(result.paused).lower(), result.pin_turns,
                   base_result.pin_turns, result.done_ms, base_result.done_ms,
                   gap, base_gap))
        for key in ("script_duration", "script_max", "empty_max"):
            loss = replace(t45_budgets(name, key), loss_per_million=20_000, seed=1)
            result = evaluate_join(size, loss)
            lines.append(
                "t45 loss %s rate_key=%s seed=1 data_only status=%s reason=%s "
                "transfer_ms=%d done_ms=%d pin_turns=%d replayed=%d host_produced=%d paused=%s"
                % (name, key, result.status, result.reason, result.transfer_ms,
                   result.done_ms, result.pin_turns, result.replayed,
                   result.host_produced, str(result.paused).lower()))
    lines.append(
        "t45 limits A8_unchanged=1 A12_data_only=1 not_end_to_end=1 "
        "real_play_unmeasured=1 other_hosts_unmeasured=1 defaults_kept=1")
    return lines


def replay_step_ms(cfg):
    if cfg.replay_turns_per_s <= 0:
        raise ValueError("rate")
    return (1000 + cfg.replay_turns_per_s - 1) // cfg.replay_turns_per_s


def host_frontier(t_ms, snapshot_turn, cfg):
    """Host-produced and host-applied frontiers at `t_ms`.

    A10 gives them the same timestamp: the host applies turn k when it
    produces it, at k * turn_ms after the snapshot. The names stay separate
    so a later split is not hidden inside one cursor.
    """
    if t_ms < 0 or cfg.turn_ms <= 0:
        raise ValueError("frontier")
    produced = snapshot_turn + t_ms // cfg.turn_ms
    return produced, produced


def turn_produce_ms(relative_k, cfg):
    if relative_k < 1:
        raise ValueError("turn")
    return relative_k * cfg.turn_ms


def turn_arrival_ms(relative_k, cfg):
    """Earliest arrival. One-way delay is part of the arrival, not optional."""
    return turn_produce_ms(relative_k, cfg) + cfg.one_way_ms


def receivable_relative(t_ms, cfg):
    """In-order zero-loss prefix. Turn k has arrived when t >= k*turn + one_way."""
    if t_ms < 0:
        raise ValueError("elapsed")
    if t_ms < cfg.turn_ms + cfg.one_way_ms:
        return 0
    return (t_ms - cfg.one_way_ms) // cfg.turn_ms


def due_relative(t_ms, cfg):
    """Required prefix at `t_ms`: turns that have had time to arrive.

    This is not the contiguous prefix that happened to arrive. A missing or
    still-reordered turn inside this prefix blocks readiness. Checking only
    whether a later turn has arrived does not cover a missing last due turn.
    """
    return receivable_relative(t_ms, cfg)


def prefix_is_due(applied_rel, received_rel, t_ms, cfg):
    """Applied, arrived, and due prefixes are the same count."""
    due = due_relative(t_ms, cfg)
    return applied_rel == received_rel == due


def delivery_lag(t_ms, cfg):
    host = t_ms // cfg.turn_ms
    got = receivable_relative(t_ms, cfg)
    if got > host:
        raise ValueError("future")
    return host - got


def max_delivery_lag(cfg):
    """Largest host lead over the receivable prefix, in turns.

    It is ceil(one_way_ms / turn_ms). When that is at least 1, the joiner
    cannot sit on the host's current frontier: the newest produced turn is
    still in flight.
    """
    if cfg.turn_ms <= 0 or cfg.one_way_ms < 0:
        raise ValueError("lag")
    if cfg.one_way_ms == 0:
        return 0
    lag = 0
    last = cfg.turn_ms + cfg.one_way_ms + cfg.turn_ms
    for t_ms in range(0, last + 1):
        lag = max(lag, delivery_lag(t_ms, cfg))
    return lag


def equality_reachable(cfg):
    """Instant apply of an arrived turn can match the host only when the
    one-way delay is shorter than one turn. A delay of one turn or more
    moves the host frontier before that turn can be applied."""
    return 0 <= cfg.one_way_ms < cfg.turn_ms


def hash_target(applied_abs, snapshot_turn, hash_every):
    """Next ordinary hash turn at or after the applied frontier.

    The snapshot boundary itself is not a new check. A frontier that already
    lands on a later multiple uses that multiple.
    """
    if hash_every <= 0:
        raise ValueError("hash_every")
    if applied_abs > snapshot_turn and applied_abs % hash_every == 0:
        return applied_abs
    if applied_abs % hash_every == 0:
        target = applied_abs + hash_every
    else:
        target = applied_abs + (hash_every - applied_abs % hash_every)
    if target <= snapshot_turn:
        step = hash_every - (snapshot_turn % hash_every)
        target = snapshot_turn + (hash_every if step == 0 else step)
    return target


def command_input_open(applied_rel, received_rel, t_ms, cfg, snapshot_turn=0,
                       hash_matches=True):
    """Input opens only on the due prefix at a hash turn.

    A hash multiple of a shorter arrived prefix stays closed while a due
    turn is missing or still reordered.
    """
    if not hash_matches or not prefix_is_due(applied_rel, received_rel, t_ms, cfg):
        return False
    applied_abs = snapshot_turn + applied_rel
    return applied_abs >= hash_target(applied_abs, snapshot_turn, cfg.hash_every)


def channel_bandwidth_us(cfg, bits_per_s=100_000_000):
    """Serialization of one channel-1 window and one channel-0 datagram.

    The channels are independent in this model: the window does not delay
    the turn. The pair is the bandwidth assumption, not a shared queue.
    """
    if bits_per_s <= 0:
        raise ValueError("rate")
    snapshot = window_serialize_us(cfg.window, cfg.payload_max, bits_per_s)
    turn = window_serialize_us(1, cfg.turn_datagram_bytes, bits_per_s)
    return snapshot, turn, snapshot + turn, cfg.one_way_ms * 1000


@dataclass(frozen=True)
class TurnSchedule:
    arrivals: dict
    duplicates: int
    acks_lost: int
    data_drops: int
    sends: int


def build_turn_schedule(turn_count, cfg, data_drop=None, ack_drop=None, reorder=None):
    """Per-turn channel-0 sends. A custom drop replaces the LCG.

    Data loss moves that turn's first arrival to a later attempt. ACK loss
    resends and counts a duplicate; it does not move the first arrival.
    `reorder` may only add delay. Arrival stays at or after produce+one_way.
    """
    if turn_count < 0:
        raise ValueError("turns")
    rng = LCG(cfg.seed)
    arrivals = {}
    duplicates = 0
    acks_lost = 0
    data_drops = 0
    sends = 0
    for relative_k in range(1, turn_count + 1):
        send = turn_produce_ms(relative_k, cfg)
        earliest = send + cfg.one_way_ms
        for attempt in range(cfg.max_attempts):
            sends += 1
            if data_drop is None:
                dropped = rng.below(cfg.loss_per_million)
            else:
                dropped = bool(data_drop(relative_k, attempt))
            extra = 0 if reorder is None else reorder(relative_k, attempt)
            if extra < 0:
                raise ValueError("reorder")
            if dropped:
                data_drops += 1
                send += cfg.resend_ms
                continue
            arrive = send + cfg.one_way_ms + extra
            if arrive < earliest:
                raise ValueError("latency")
            if relative_k not in arrivals:
                arrivals[relative_k] = arrive
            else:
                duplicates += 1
            if ack_drop is not None and ack_drop(relative_k, attempt):
                acks_lost += 1
                send += cfg.resend_ms
                continue
            break
    return TurnSchedule(arrivals, duplicates, acks_lost, data_drops, sends)


def prefix_received(t_ms, arrivals):
    relative_k = 1
    while relative_k in arrivals and arrivals[relative_k] <= t_ms:
        relative_k += 1
    return relative_k - 1


def hole_at(t_ms, arrivals, host_relative):
    """A later turn is present while an earlier one is not."""
    prefix = prefix_received(t_ms, arrivals)
    for relative_k in range(prefix + 1, host_relative + 1):
        when = arrivals.get(relative_k)
        if when is not None and when <= t_ms:
            return True
    return False


def frontiers_at(t_ms, cfg, snapshot_turn=0, arrivals=None):
    """Frontiers after host apply and channel-0 delivery at `t_ms`.

    Joiner apply is not this function. It completes `replay_step_ms` later.
    Order at one timestamp: host produce/apply, then deliveries with
    arrival <= t, then a joiner apply only if its step has elapsed.
    """
    produced, host_applied = host_frontier(t_ms, snapshot_turn, cfg)
    host_relative = produced - snapshot_turn
    due = due_relative(t_ms, cfg)
    if arrivals is None:
        relative = min(receivable_relative(t_ms, cfg), host_relative)
        hole = False
        first = turn_arrival_ms(1, cfg) if host_relative > 0 or t_ms >= 0 else -1
    else:
        relative = prefix_received(t_ms, arrivals)
        if relative > host_relative:
            raise ValueError("future")
        hole = hole_at(t_ms, arrivals, host_relative)
        first = arrivals.get(1, -1)
    return {
        "t_ms": t_ms,
        "host_produced": produced,
        "host_applied": host_applied,
        "received": snapshot_turn + relative,
        "due": snapshot_turn + due,
        "hole": hole,
        "first_arrival_ms": first,
    }


@dataclass(frozen=True)
class CausalJoin:
    status: str
    reason: str
    paused: bool
    snapshot_turn: int
    ready_ms: int
    done_ms: int
    host_produced: int
    host_applied: int
    received: int
    applied: int
    missing: int
    first_arrival_ms: int
    input_enabled: bool
    hash_turn: int
    pin_turns: int
    host_span: int
    transfer_ms: int
    backlog_turns: int
    optimistic_status: str
    optimistic_done_ms: int
    optimistic_replayed: int
    optimistic_host: int
    duplicates: int = 0
    acks_lost: int = 0
    data_drops: int = 0

    def honest(self, cfg):
        if self.paused or (self.input_enabled and self.status != "success"):
            return False
        if self.applied > self.snapshot_turn and self.first_arrival_ms >= 0:
            if self.first_arrival_ms < cfg.turn_ms + cfg.one_way_ms:
                return False
        if self.applied > self.received or self.received > self.host_applied:
            return False
        if self.host_produced != self.host_applied:
            return False
        if self.host_produced != self.snapshot_turn + self.done_ms // cfg.turn_ms:
            return False
        if self.pin_turns != max(self.applied - self.snapshot_turn, 0):
            return False
        if self.host_span != self.host_applied - self.snapshot_turn:
            return False
        if self.status != "success":
            return self.status in ("refuse", "restart") and not self.input_enabled
        if self.missing != 0 or not self.input_enabled:
            return False
        if self.applied != self.received:
            return False
        due = self.snapshot_turn + due_relative(self.done_ms, cfg)
        if self.applied != due or self.received != due:
            return False
        if cfg.one_way_ms >= cfg.turn_ms and self.host_applied > self.snapshot_turn:
            if self.applied >= self.host_applied:
                return False
        if self.host_span > cfg.tail_turns_max:
            return False
        if self.backlog_turns > cfg.receive_backlog_turns:
            return False
        # Success at the deadline millisecond loses to Table.advance.
        return self.done_ms < cfg.catchup_ms


def _received_rel(t_ms, cfg, arrivals):
    host_relative = t_ms // cfg.turn_ms
    if arrivals is None:
        return min(receivable_relative(t_ms, cfg), host_relative)
    got = prefix_received(t_ms, arrivals)
    if got > host_relative:
        return None
    return got


def catchup_state_at(query_ms, cfg, load_done, snapshot_turn=0, arrivals=None):
    """Joiner cursors at `query_ms`, after delivery and before an unfinished apply."""
    if query_ms < 0:
        raise ValueError("query")
    if query_ms < load_done:
        produced, host_applied = host_frontier(query_ms, snapshot_turn, cfg)
        return {
            "t_ms": query_ms,
            "host_produced": produced,
            "host_applied": host_applied,
            "received": snapshot_turn,
            "applied": snapshot_turn,
            "on_frontier": False,
        }
    step = replay_step_ms(cfg)
    t_ms = load_done
    applied_rel = 0
    guard = 0
    while guard <= query_ms + 2:
        guard += 1
        rec = _received_rel(t_ms, cfg, arrivals)
        if rec is None or t_ms >= query_ms:
            break
        if applied_rel < rec:
            if t_ms + step > query_ms:
                break
            applied_rel += 1
            t_ms += step
            continue
        nxt = applied_rel + 1
        if arrivals is None:
            arrive = turn_arrival_ms(nxt, cfg)
        else:
            arrive = arrivals.get(nxt)
            if arrive is None or arrive > query_ms:
                break
        if arrive <= t_ms:
            break
        t_ms = arrive
    rec = _received_rel(query_ms, cfg, arrivals)
    if rec is None:
        rec = min(applied_rel, query_ms // cfg.turn_ms)
    produced, host_applied = host_frontier(query_ms, snapshot_turn, cfg)
    return {
        "t_ms": query_ms,
        "host_produced": produced,
        "host_applied": host_applied,
        "received": snapshot_turn + rec,
        "applied": snapshot_turn + applied_rel,
        "due": snapshot_turn + due_relative(query_ms, cfg),
        "on_frontier": applied_rel == rec,
    }


def _fail_causal(status, reason, snapshot_turn, transfer_ms, done_ms, missing, cfg,
                 optimistic, schedule=None, ready_ms=-1, applied=None, received=None,
                 backlog=0):
    if applied is None:
        applied = snapshot_turn
    if received is None:
        received = snapshot_turn
    host, host_applied = host_frontier(max(done_ms, 0), snapshot_turn, cfg)
    sched = schedule or TurnSchedule({}, 0, 0, 0, 0)
    first = -1 if schedule is None else schedule.arrivals.get(1, -1)
    if schedule is None and cfg.one_way_ms >= 0:
        first = turn_arrival_ms(1, cfg)
    return CausalJoin(
        status, reason, False, snapshot_turn, ready_ms, done_ms, host, host_applied,
        received, applied, missing, first, False, -1,
        max(applied - snapshot_turn, 0), host_applied - snapshot_turn,
        transfer_ms, backlog, optimistic.status, optimistic.done_ms,
        optimistic.replayed, optimistic.host_produced, sched.duplicates,
        sched.acks_lost, sched.data_drops)


_CAUSAL_JOINER = "causal"


def _owner_stop_reason(reason, cfg):
    """Keep the owner's restart reason.

    A chase that cannot outrun live turns still reports `cannot_outrun` when
    the deadline is what stops the clock. The comparison that fires the
    deadline stays in `Table.advance`.
    """
    if reason == "deadline" and cfg.replay_turns_per_s <= cfg.live_turns_per_s:
        return "cannot_outrun"
    return reason


def _owner_until(table, joiner_id, target_ms, snapshot_turn, cfg):
    """Move `table` to `target_ms` through `Table.advance`.

    The walk samples the owner's deadline instant and each host tick so a
    later jump cannot skip them. A restart of `joiner_id` ends the walk at
    that instant. Success is the caller's decision, and only after this
    returns no reason.
    """
    if target_ms < 0 or target_ms < table.now_ms:
        raise ValueError("clock")
    guard = 0
    limit = target_ms + 2
    while table.now_ms < target_ms:
        guard += 1
        if guard > limit:
            raise ValueError("clock")
        nxt = target_ms
        deadline = table.next_deadline_ms()
        if deadline is not None and table.now_ms < deadline < target_ms:
            nxt = deadline
        tick = (table.now_ms // cfg.turn_ms + 1) * cfg.turn_ms
        if table.now_ms < tick < nxt:
            nxt = tick
        host = snapshot_turn + nxt // cfg.turn_ms
        if host < table.host_turn:
            host = table.host_turn
        restarted = table.advance(nxt, host)
        for jid, reason in restarted:
            if jid == joiner_id:
                return reason, nxt
    host = snapshot_turn + target_ms // cfg.turn_ms
    if host < table.host_turn:
        host = table.host_turn
    if table.now_ms < target_ms or table.host_turn < host:
        restarted = table.advance(target_ms, host)
        for jid, reason in restarted:
            if jid == joiner_id:
                return reason, target_ms
    return None, table.now_ms


def evaluate_causal(save_bytes, cfg, transfer=None, snapshot_turn=0,
                    data_drop=None, ack_drop=None, reorder=None, hash_matches=True,
                    horizon_ms=None):
    """Causal join. Success is the hash on the due prefix.

    The due prefix is every turn that has had time to arrive. Equality of
    applied and the contiguous arrived prefix is not enough while a due turn
    is missing. `Table.advance` runs at the clock sample before that success
    test, so a still-running join at `elapsed >= catchup_ms` restarts instead.
    The optimistic fluid join is stored on the result and is not this test.
    The table is not paused.
    """
    optimistic_cfg = cfg
    fragments = fragment_count(save_bytes, cfg.save_per_fragment)
    if fragments > cfg.fragment_max:
        dummy = JoinResult("refuse", "fragment_cap", False, snapshot_turn,
                           snapshot_turn + 1, snapshot_turn, 0, 0, 0, 0,
                           snapshot_turn, snapshot_turn, snapshot_turn, 0, 0)
        return _fail_causal("refuse", "fragment_cap", snapshot_turn, 0, 0, 0, cfg, dummy)

    if transfer is None:
        transfer = run_transfer(fragments, cfg)
    optimistic = evaluate_join(save_bytes, optimistic_cfg, transfer=transfer,
                               snapshot_turn=snapshot_turn)
    if transfer.status != "complete":
        reason = "missing_fragments" if transfer.status == "incomplete" else transfer.status
        missing = transfer.fragments - transfer.cursor
        return _fail_causal("restart", reason, snapshot_turn, transfer.done_ms,
                            transfer.done_ms, missing, cfg, optimistic)

    load_done = transfer.done_ms + cfg.load_ms
    limit = cfg.catchup_ms if horizon_ms is None else min(cfg.catchup_ms, horizon_ms)
    needs_schedule = (cfg.loss_per_million > 0 or data_drop is not None
                      or ack_drop is not None or reorder is not None)
    schedule = None
    arrivals = None
    if needs_schedule:
        turn_count = limit // cfg.turn_ms + cfg.max_attempts + 2
        schedule = build_turn_schedule(turn_count, cfg, data_drop, ack_drop, reorder)
        arrivals = schedule.arrivals
    first_arrival = turn_arrival_ms(1, cfg) if arrivals is None else arrivals.get(1, -1)

    step = replay_step_ms(cfg)
    t_ms = load_done
    applied_rel = 0
    ready_ms = -1
    peak = 0
    guard = 0
    stop_reason = None
    table = Table(cfg)
    word, _gen = table.attach(_CAUSAL_JOINER, 0, save_bytes, snapshot_turn, 0)
    if word not in ("opened", "shared"):
        why = "fragment_cap" if word == "fragment" else "refuse"
        return _fail_causal("refuse", why, snapshot_turn, transfer.done_ms, 0, 0, cfg,
                            optimistic, schedule)

    def span_of(now):
        return now // cfg.turn_ms

    def owner_stop(target):
        reason, at = _owner_until(table, _CAUSAL_JOINER, target, snapshot_turn, cfg)
        if reason is None:
            return None, at
        return _owner_stop_reason(reason, cfg), at

    if t_ms > limit:
        stop_reason, at = owner_stop(limit)
        t_ms = at if stop_reason is not None else limit

    while stop_reason is None and t_ms <= limit and guard <= limit + 2:
        guard += 1
        stop_reason, at = owner_stop(t_ms)
        if stop_reason is not None:
            t_ms = at
            break
        rec = _received_rel(t_ms, cfg, arrivals)
        if rec is None:
            stop_reason = "future_turn"
            break
        backlog = rec - applied_rel
        if backlog > peak:
            peak = backlog
        if peak > cfg.receive_backlog_turns:
            stop_reason = "backlog"
            break
        if applied_rel > rec:
            stop_reason = "future_turn"
            break
        if prefix_is_due(applied_rel, rec, t_ms, cfg):
            if ready_ms < 0:
                ready_ms = t_ms
            applied_abs = snapshot_turn + applied_rel
            hash_at = hash_target(applied_abs, snapshot_turn, cfg.hash_every)
            if applied_abs >= hash_at:
                if not hash_matches:
                    stop_reason = "hash_mismatch"
                    break
                return _success_causal(
                    snapshot_turn, ready_ms, t_ms, applied_rel, rec, hash_at, peak,
                    transfer.done_ms, first_arrival, optimistic, schedule, cfg)
        if applied_rel < rec:
            applied_rel += 1
            t_ms += step
            continue
        nxt = applied_rel + 1
        if arrivals is None:
            arrive = turn_arrival_ms(nxt, cfg)
        else:
            arrive = arrivals.get(nxt)
            if arrive is None:
                stop_reason = "missing_prefix"
                break
        if arrive < turn_produce_ms(nxt, cfg) + cfg.one_way_ms:
            stop_reason = "future_turn"
            break
        if arrive <= t_ms:
            stop_reason = "missing_prefix"
            break
        if arrive > limit:
            stop_reason, at = owner_stop(limit)
            if stop_reason is not None:
                t_ms = at
                break
            t_ms = limit + 1
            break
        t_ms = arrive

    if stop_reason is None and table.now_ms < limit:
        stop_reason, at = owner_stop(limit)
        if stop_reason is not None:
            t_ms = at
    if stop_reason is None:
        if horizon_ms is not None and horizon_ms <= cfg.catchup_ms and t_ms > horizon_ms:
            stop_reason = "unmet_boundary"
        elif cfg.replay_turns_per_s <= cfg.live_turns_per_s:
            stop_reason = "cannot_outrun"
        elif arrivals is not None and first_arrival < 0:
            stop_reason = "missing_prefix"
        else:
            stop_reason = "catchup_ms"
    if t_ms > limit:
        t_ms = limit
    rec = _received_rel(t_ms, cfg, arrivals)
    if rec is None:
        rec = min(applied_rel, t_ms // cfg.turn_ms)
    missing = 1
    if stop_reason == "tail_turns":
        missing = span_of(t_ms)
    elif stop_reason == "backlog":
        missing = peak
    elif stop_reason == "missing_prefix":
        missing = max((t_ms // cfg.turn_ms) - applied_rel, 1)
    elif stop_reason in ("cannot_outrun", "catchup_ms", "unmet_boundary", "deadline"):
        missing = max((t_ms // cfg.turn_ms) - applied_rel, 1)
    return _fail_causal(
        "restart", stop_reason, snapshot_turn, transfer.done_ms, t_ms, missing, cfg,
        optimistic, schedule, ready_ms, snapshot_turn + applied_rel,
        snapshot_turn + rec, peak)


def _success_causal(snapshot_turn, ready_ms, done_ms, applied_rel, rec, hash_at, peak,
                    transfer_ms, first_arrival, optimistic, schedule, cfg):
    applied = snapshot_turn + applied_rel
    received = snapshot_turn + rec
    host, host_applied = host_frontier(done_ms, snapshot_turn, cfg)
    sched = schedule or TurnSchedule({}, 0, 0, 0, 0)
    return CausalJoin(
        "success", "caught_up", False, snapshot_turn, ready_ms, done_ms,
        host, host_applied, received, applied, 0, first_arrival, True, hash_at,
        applied - snapshot_turn, host_applied - snapshot_turn, transfer_ms, peak,
        optimistic.status, optimistic.done_ms, optimistic.replayed,
        optimistic.host_produced, sched.duplicates, sched.acks_lost, sched.data_drops)


def causal_report_lines(cfg=None):
    """Labeled causal rows. Fluid equality stays on `report_lines`."""
    if cfg is None:
        cfg = proposed_budgets()
    snap_us, turn_us, both_us, one_way_us = channel_bandwidth_us(cfg)
    lines = [
        "causal-join model",
        "status: proposal, not a measured network",
        "causal defaults_unchanged load_ms=%d replay_turns_per_s=%d turn_store_cap=%d "
        "one_way_ms=%d hash_every=%d command_delay=2"
        % (cfg.load_ms, cfg.replay_turns_per_s, cfg.turn_store_cap,
           cfg.one_way_ms, cfg.hash_every),
        "causal bandwidth snapshot_window_us=%d turn_datagram_us=%d sum_us=%d "
        "one_way_us=%d independent_channels=1 shared_queue=0"
        % (snap_us, turn_us, both_us, one_way_us),
        "causal lag_max=%d equality_reachable=%d"
        % (max_delivery_lag(cfg), 1 if equality_reachable(cfg) else 0),
        "causal fluid_unchanged endless02_rtt150_tenths=%d endless02_rtt300_tenths=%d"
        % (fluid_tenths(PUBLISHED_FRAGMENTS["endless02"], cfg.window, 150),
           fluid_tenths(PUBLISHED_FRAGMENTS["endless02"], cfg.window, 300)),
    ]
    one = evaluate_causal(cfg.save_per_fragment, cfg)
    lines.append(
        "causal one_fragment produce_ms=%d arrival_ms=%d ready_ms=%d done_ms=%d "
        "applied=%d received=%d host_produced=%d host_applied=%d input_enabled=%s "
        "hash_turn=%d paused=%s optimistic_done_ms=%d optimistic_replayed=%d "
        "optimistic_host=%d equal_frontier=0"
        % (turn_produce_ms(1, cfg), one.first_arrival_ms, one.ready_ms, one.done_ms,
           one.applied, one.received, one.host_produced, one.host_applied,
           str(one.input_enabled).lower(), one.hash_turn, str(one.paused).lower(),
           one.optimistic_done_ms, one.optimistic_replayed, one.optimistic_host))
    for name, size in MEASURED_SAVES:
        result = evaluate_causal(size, cfg)
        lines.append(
            "causal default %s bytes=%d status=%s reason=%s ready_ms=%d done_ms=%d "
            "applied=%d received=%d host_produced=%d host_applied=%d pin_turns=%d "
            "host_span=%d backlog_peak=%d transfer_ms=%d paused=%s "
            "optimistic_status=%s optimistic_done_ms=%d optimistic_replayed=%d "
            "input_enabled=%s"
            % (name, size, result.status, result.reason, result.ready_ms, result.done_ms,
               result.applied, result.received, result.host_produced, result.host_applied,
               result.pin_turns, result.host_span, result.backlog_turns, result.transfer_ms,
               str(result.paused).lower(), result.optimistic_status,
               result.optimistic_done_ms, result.optimistic_replayed,
               str(result.input_enabled).lower()))
    rate_keys = ("script_duration", "script_median", "script_p95", "script_max",
                 "empty_duration", "empty_max")
    for name, size in T45_SAVE_BYTES:
        for key in rate_keys:
            labeled = t45_budgets(name, key)
            result = evaluate_causal(size, labeled)
            lines.append(
                "causal t45 %s rate_key=%s replay=%d load_ms=%d status=%s reason=%s "
                "done_ms=%d applied=%d host_applied=%d pin_turns=%d host_span=%d "
                "optimistic_done_ms=%d optimistic_replayed=%d paused=%s input_enabled=%s"
                % (name, key, labeled.replay_turns_per_s, labeled.load_ms, result.status,
                   result.reason, result.done_ms, result.applied, result.host_applied,
                   result.pin_turns, result.host_span, result.optimistic_done_ms,
                   result.optimistic_replayed, str(result.paused).lower(),
                   str(result.input_enabled).lower()))
        loss = replace(t45_budgets(name, "empty_max"), loss_per_million=20_000, seed=1)
        result = evaluate_causal(size, loss)
        horizon_turns = loss.catchup_ms // loss.turn_ms + loss.max_attempts + 2
        lines.append(
            "causal t45_loss %s rate_key=empty_max seed=1 data_only status=%s reason=%s "
            "transfer_ms=%d done_ms=%d applied=%d host_applied=%d horizon_turns=%d "
            "horizon_data_drops=%d acks_lost=%d paused=%s optimistic_done_ms=%d "
            "optimistic_replayed=%d"
            % (name, result.status, result.reason, result.transfer_ms, result.done_ms,
               result.applied, result.host_applied, horizon_turns, result.data_drops,
               result.acks_lost, str(result.paused).lower(), result.optimistic_done_ms,
               result.optimistic_replayed))
    lines.append(
        "causal limits not_end_to_end=1 shared_link_unmeasured=1 hash_flight_unmeasured=1 "
        "jitter_unmeasured=1 real_play_unmeasured=1 other_hosts_unmeasured=1 "
        "defaults_kept=1 adr_proposed=1")
    return lines


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
        "joiner_backlog_bytes: %d" % bound["joiner_backlog"],
        "channel0_flight_bytes: %d" % bound["channel0_flight"],
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
                "model %s loss=2pct seed=1 data_only status=%s reason=%s "
                "transfer_ms=%d done_ms=%d pin_turns=%d host_produced=%d "
                "received=%d replayed=%d reserved_turns=%d missing=%d paused=%s"
                % (name, result.status, result.reason, result.transfer_ms,
                   result.done_ms, result.pin_turns, result.host_produced,
                   result.received, result.replayed, result.reserved_turns,
                   result.missing, result.paused))
    one = evaluate_join(cfg.save_per_fragment, cfg)
    lines.append("counterexample_1188_done_ms=%d" % one.done_ms)
    lines.append("counterexample_1188_replayed=%d" % one.replayed)
    lines.append("counterexample_1188_host=%d" % one.host_produced)
    lines.append("counterexample_1188_reserved=%d" % one.reserved_turns)
    lines.append("periodic_age_turns: %d" % periodic_age_turns(cfg))
    lines.extend("assumption: " + item for item in assumptions())
    return lines


def main():
    for line in report_lines(with_loss=True):
        print(line)
    for line in t45_report_lines():
        print(line)
    for line in causal_report_lines():
        print(line)


if __name__ == "__main__":
    main()
