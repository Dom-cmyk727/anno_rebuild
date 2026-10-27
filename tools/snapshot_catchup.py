#!/usr/bin/env python3
"""Integer model of snapshot join and catch-up (T43, ADR 0003).

This is not a transport and not a measurement of a network. The three save
sizes, the 1188-byte fragment payload, the window of 32 and the fluid times
80.3 s / 160.6 s are the T41 figures. Everything else below is an explicit
model assumption (A1-A13 in `assumptions()`).

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


if __name__ == "__main__":
    main()
