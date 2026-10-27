#!/usr/bin/env python3
"""Ordered join-activation protocol checker (T52 F2, ADR 0003 section 8).

This is a protocol oracle, not a game simulation and not a measurement. Hash
roots are symbolic deterministic values. Every message goes through one event
queue with integer milliseconds, per directed pair and per channel sequence
numbers, first sends and resends with an attempt number, ACK-only datagrams
with a selective 32-sequence window, and ordered delivery on reliable channel
0: the application sees sequence n only after every sequence below n has been
delivered. A loss is listed by (message, direction, sequence, attempt). The
checker does not use `catchup_state_at` or `frontiers_at` of
`tools/snapshot_catchup.py`.

Tie order inside one millisecond (lower first, then insertion order):

  0 deadline and expiry timers      30 replay step completion
  10 inbound ACK-only datagrams     40 host turn production, join asks
  20 data deliveries                50 resend checks
  25 ACK-only flush (coalesced)

`rules="F1"` reproduces the guards published at 3020900 so the round-cb
regressions can be run against them. `rules="F2"` is the proposal.

Run:

  python -I -B tools/join_activation.py [--rules F1|F2] [--trace] [row ...]
  python -I -B tools/test_join_activation.py -v
"""

from __future__ import annotations

import heapq
import sys
from collections import deque
from dataclasses import dataclass, replace

P_DEADLINE, P_ACK, P_DELIVER, P_FLUSH, P_REPLAY, P_PRODUCE, P_RESEND = (
    0, 10, 20, 25, 30, 40, 50)
HEADER = 25
IP_UDP = 28
FRAGMENT_HEAD = 12


@dataclass(frozen=True)
class Cfg:
    turn_ms: int = 100
    one_way_ms: int = 150
    resend_ms: int = 300
    max_attempts: int = 8
    hash_every: int = 10
    command_delay: int = 2
    deadline_intervals: int = 2
    replay_step_ms: int = 10
    load_ms: int = 0
    window: int = 32
    payload_max: int = 1200
    root_slots: int = 4
    wait_cap: int = 64
    catchup_ms: int = 600000
    snapshot_age_ms: int = 60000
    input_lag_max: int = 6
    rules: str = "F1"

    @property
    def legacy(self):
        return self.rules == "F1"

    def deadline_for(self, hash_turn):
        """Accept deadline: the host's first send of that root plus two intervals."""
        return hash_turn * self.turn_ms + self.deadline_intervals * self.hash_every * self.turn_ms


def root_of(turn, owners):
    """Symbolic deterministic root: the turn and the ownership table at that turn."""
    return ("root", turn, tuple(sorted(owners.items())))


class Chan:
    """One direction of one directed pair on one channel."""

    def __init__(self):
        self.next_seq = 0
        self.out = {}
        self.wait = deque()
        self.recv = {}
        self.deliv = 0
        self.highest = -1
        self.ack_pending = False
        self.acks_sent = 0
        self.dead = False


class Sim:
    def __init__(self, cfg, losses=(), delays=None, seated=1):
        self.cfg = cfg
        self.t = 0
        self.q = []
        self.n = 0
        self.trace = []
        self.losses = set(losses)
        self.delays = dict(delays or {})
        self.chans = {}
        self.stats = {}
        self.peers = {}
        self.deliveries = []
        self.violations = []
        self.host = Host(self)
        self.peers["H"] = self.host
        self.seated = []
        for i in range(seated):
            name = "P%d" % (i + 1)
            peer = Seated(self, name, i + 1)
            self.peers[name] = peer
            self.seated.append(peer)
            self.host.owners[i + 1] = "human"
        self.joiners = {}
        self.turn_extra = lambda k: 0
        self.schedule(cfg.turn_ms, P_PRODUCE, self.host.produce, 1)

    # ---- event queue ----
    def schedule(self, t, prio, fn, *args):
        self.n += 1
        heapq.heappush(self.q, (t, prio, self.n, fn, args))

    def log(self, text):
        self.trace.append("%7d %s" % (self.t, text))

    def run(self, until):
        while self.q and self.q[0][0] <= until:
            t, _, _, fn, args = heapq.heappop(self.q)
            self.t = t
            fn(*args)
        self.t = until

    def dump(self):
        return "\n".join(self.trace)

    # ---- joiners ----
    def add_join(self, name, ask_ms, seat, snapshot_ms=150, load_ms=None, attempt=1):
        joiner = self.peers.get(name) or Joiner(self, name)
        self.peers[name] = joiner
        self.joiners[name] = joiner
        self.schedule(ask_ms, P_PRODUCE + 1, self.host.admit, name, seat, snapshot_ms,
                      self.cfg.load_ms if load_ms is None else load_ms, attempt)
        return joiner

    # ---- transport ----
    def chan(self, src, dst, ch):
        key = (src, dst, ch)
        if key not in self.chans:
            self.chans[key] = Chan()
        return self.chans[key]

    def stat(self, src, dst, field, amount=1):
        entry = self.stats.setdefault((src, dst), {"data": 0, "ack": 0, "bytes": 0, "seq": 0})
        entry[field] += amount

    def send(self, src, dst, ch, kind, payload, size, **msg):
        """Queue one message; fragments over payload_max. Returns False on overflow."""
        c = self.chan(src, dst, ch)
        if c.dead:
            self.log("DROP-DEAD %s>%s ch%d %s" % (src, dst, ch, kind))
            return False
        cfg = self.cfg
        if size <= cfg.payload_max:
            sizes = [size]
        else:
            data = cfg.payload_max - FRAGMENT_HEAD
            count = -(-size // data)
            sizes = [cfg.payload_max] * (count - 1) + [size - data * (count - 1) + FRAGMENT_HEAD]
        if len(c.wait) + len(sizes) > cfg.wait_cap:
            self.log("OVERFLOW %s>%s ch%d %s waiting=%d" % (src, dst, ch, kind, len(c.wait)))
            self.stat(src, dst, "seq", 0)
            self.violations.append(("overflow", src, dst, ch, kind))
            return False
        msg["kind"] = kind
        for i, s in enumerate(sizes):
            last = i == len(sizes) - 1
            c.wait.append({"kind": kind if last else kind + "-frag", "size": s,
                           "msg": msg if last else None, "attempt": 0, "acked": False})
        self.pump(src, dst, ch)
        return True

    def window_room(self, c):
        oldest = min(c.out) if c.out else c.next_seq
        return c.next_seq - oldest < self.cfg.window

    def pump(self, src, dst, ch):
        c = self.chan(src, dst, ch)
        while c.wait and self.window_room(c) and not c.dead:
            d = c.wait.popleft()
            d["seq"] = c.next_seq
            c.next_seq += 1
            if ch != 2:
                c.out[d["seq"]] = d
            self.transmit(src, dst, ch, d)

    def key(self, kind, src, dst, seq, attempt):
        return (kind, "%s>%s" % (src, dst), seq, attempt)

    def lost(self, key):
        return key in self.losses or (key[0], key[1], None, key[3]) in self.losses

    def transmit(self, src, dst, ch, d):
        d["attempt"] += 1
        d["last"] = self.t
        key = self.key(d["kind"], src, dst, d["seq"], d["attempt"])
        wire = d["size"] + HEADER + IP_UDP
        self.stat(src, dst, "data")
        self.stat(src, dst, "bytes", wire)
        tag = "send" if d["attempt"] == 1 else "RESEND#%d" % d["attempt"]
        if self.lost(key):
            self.log("%s %s>%s ch%d seq%d %s -- LOST" % (tag, src, dst, ch, d["seq"], d["kind"]))
        else:
            arrive = self.t + self.cfg.one_way_ms + self.delays.get(
                key, self.delays.get((key[0], key[1], None, key[3]), 0))
            self.log("%s %s>%s ch%d seq%d %s -> arrives %d" % (
                tag, src, dst, ch, d["seq"], d["kind"], arrive))
            self.schedule(arrive, P_DELIVER, self.arrive, src, dst, ch, d)
        if ch != 2 and not self.cfg.legacy:
            self.schedule(self.t + self.cfg.resend_ms, P_RESEND, self.resend, src, dst, ch, d["seq"])

    def resend(self, src, dst, ch, seq):
        c = self.chan(src, dst, ch)
        d = c.out.get(seq)
        if d is None or c.dead:
            return
        if d["attempt"] >= self.cfg.max_attempts:
            c.dead = True
            self.log("ABANDON %s>%s ch%d seq%d after %d sends" % (src, dst, ch, seq, d["attempt"]))
            self.peers[src].abandoned(dst, ch)
            return
        self.transmit(src, dst, ch, d)

    def arrive(self, src, dst, ch, d):
        c = self.recv_state(src, dst, ch)
        seq = d["seq"]
        dup = seq in c.recv
        c.recv.setdefault(seq, d)
        c.highest = max(c.highest, seq)
        self.log("arrive %s>%s ch%d seq%d %s%s" % (src, dst, ch, seq, d["kind"], " (dup)" if dup else ""))
        if ch == 2 or self.cfg.legacy:
            if not dup and d["msg"] is not None:
                self.deliver(src, dst, ch, seq, d["msg"])
        else:
            while c.deliv in c.recv:
                nxt = c.recv[c.deliv]
                if nxt["msg"] is not None and not nxt.get("delivered"):
                    nxt["delivered"] = True
                    self.deliver(src, dst, ch, c.deliv, nxt["msg"])
                c.deliv += 1
        if ch != 2 and not c.ack_pending:
            c.ack_pending = True
            self.schedule(self.t, P_FLUSH, self.flush_ack, src, dst, ch)

    def recv_state(self, src, dst, ch):
        key = ("recv", src, dst, ch)
        if key not in self.chans:
            self.chans[key] = Chan()
        return self.chans[key]

    def deliver(self, src, dst, ch, seq, msg):
        self.deliveries.append((self.t, src, dst, ch, seq, msg["kind"]))
        if not self.cfg.legacy and ch == 0:
            c = self.recv_state(src, dst, ch)
            if seq > c.deliv:
                self.violations.append(("out_of_order_delivery", src, dst, seq, c.deliv))
        self.log("deliver %s>%s ch%d seq%d %s" % (src, dst, ch, seq, msg["kind"]))
        self.peers[dst].on_msg(src, msg)

    def flush_ack(self, src, dst, ch):
        c = self.recv_state(src, dst, ch)
        c.ack_pending = False
        ack = c.highest
        bits = 0
        for i in range(32):
            if ack - (i + 1) >= 0 and (ack - (i + 1)) in c.recv:
                bits |= 1 << i
        idx = c.acks_sent
        c.acks_sent += 1
        key = ("Ack", "%s>%s" % (dst, src), idx, 1)
        self.stat(dst, src, "ack")
        self.stat(dst, src, "bytes", HEADER + IP_UDP)
        if self.lost(key):
            self.log("ack-only %s>%s ch%d #%d ack=%d -- LOST" % (dst, src, ch, idx, ack))
            return
        self.log("ack-only %s>%s ch%d #%d ack=%d bits=%x" % (dst, src, ch, idx, ack, bits))
        self.schedule(self.t + self.cfg.one_way_ms, P_ACK, self.take_ack, src, dst, ch, ack, bits)

    def take_ack(self, src, dst, ch, ack, bits):
        """The sender src learns what dst received."""
        c = self.chan(src, dst, ch)
        named = {ack}
        for i in range(32):
            if bits >> i & 1:
                named.add(ack - (i + 1))
        for seq in sorted(named):
            c.out.pop(seq, None)
        self.pump(src, dst, ch)
        self.peers[src].on_acked(dst, ch)

    def outstanding(self, src, dst, ch, kind):
        c = self.chan(src, dst, ch)
        return any(d["kind"] == kind for d in c.out.values()) or any(
            d["kind"] == kind for d in c.wait)


def entry_bytes(events):
    return 12 + 2  # TurnEntry head (issuer, seq, type, len) plus seat and owner bytes


def turn_bytes(events, extra=0):
    return 16 + sum(entry_bytes(e) for e in events) + extra


class Peer:
    def __init__(self, sim, name):
        self.sim = sim
        self.name = name
        self.owners = {}
        self.hw = 0

    def apply_events(self, events):
        for ev in events:
            event_id, seat, owner = ev
            if event_id <= self.hw:
                self.sim.log("%s skips duplicate event %d" % (self.name, event_id))
                continue
            self.hw = event_id
            self.owners[seat] = owner
            self.sim.log("%s owner seat %d -> %s (event %d)" % (self.name, seat, owner, event_id))

    def on_acked(self, dst, ch):
        pass

    def abandoned(self, dst, ch):
        pass


class Seated(Peer):
    def __init__(self, sim, name, seat):
        super().__init__(sim, name)
        self.seat = seat
        self.owners = {0: "human", seat: "human"}
        self.applied = 0
        self.roots = {}
        self.pending = {}

    def on_msg(self, src, msg):
        if msg["kind"] == "Turn":
            self.pending[msg["turn"]] = msg
            while self.applied + 1 in self.pending:
                m = self.pending.pop(self.applied + 1)
                self.applied += 1
                self.apply_events(m["events"])
                if self.applied % self.sim.cfg.hash_every == 0:
                    self.roots[self.applied] = root_of(self.applied, self.owners)


class JoinHost:
    def __init__(self, name, seat, snapshot_turn, gen, attempt):
        self.name = name
        self.seat = seat
        self.S = snapshot_turn
        self.gen = gen
        self.attempt = attempt
        self.state = "waiting"
        self.H = None
        self.A = None
        self.event_id = None
        self.accept_ms = None
        self.restart_ms = None
        self.restart_reason = None
        self.watermarks = []


class Host(Peer):
    def __init__(self, sim):
        super().__init__(sim, "H")
        self.owners = {0: "human"}
        self.applied = 0
        self.roots = {}
        self.root_sent = {}
        self.pending_events = {}
        self.next_event = 1
        self.joins = {}
        self.pairs = set()
        self.gens = []
        self.log_turns = {}

    # ---- turns ----
    def produce(self, k):
        sim = self.sim
        cfg = sim.cfg
        events = self.pending_events.pop(k, [])
        self.apply_events(events)
        self.applied = k
        self.log_turns[k] = tuple(events)
        if k % cfg.hash_every == 0:
            self.roots[k] = root_of(k, self.owners)
            self.root_sent[k] = sim.t
            sim.log("host applies %d, root sent (deadline %d)" % (k, cfg.deadline_for(k)))
        size = turn_bytes(events, sim.turn_extra(k))
        for peer in sim.seated:
            sim.send("H", peer.name, 0, "Turn", None, size, turn=k, events=tuple(events))
        for name, jr in sorted(self.joins.items()):
            if jr.state in ("dead", "done") or k <= jr.S:
                continue
            sim.send("H", name, 0, "Turn", None, size, turn=k, events=tuple(events))
            if k % cfg.hash_every == 0:
                sim.send("H", name, 2, "HashRoot", None, 20, turn=k, root=self.roots[k])
        sim.schedule((k + 1) * cfg.turn_ms, P_PRODUCE, self.produce, k + 1)

    def current(self):
        return self.sim.t // self.sim.cfg.turn_ms

    # ---- admission ----
    def admit(self, name, seat, snapshot_ms, load_ms, attempt):
        sim = self.sim
        S = self.applied
        gen = len(self.gens) + 1
        self.gens.append((gen, S, sim.t))
        jr = JoinHost(name, seat, S, gen, attempt)
        self.joins[name] = jr
        sim.log("host admits %s seat %d, generation %d, snapshot turn %d" % (name, seat, gen, S))
        sim.send("H", name, 0, "JoinGrant", None, 13, gen=gen, S=S, seat=seat, attempt=attempt)
        snap = (dict(self.owners), self.hw)
        sim.schedule(sim.t + snapshot_ms, P_DELIVER, sim.peers[name].snapshot, gen, S, snap,
                     load_ms, attempt)

    # ---- messages from joiners ----
    def on_msg(self, src, msg):
        jr = self.joins.get(src)
        if jr is None or msg.get("attempt") != jr.attempt or jr.state in ("dead", "done"):
            self.sim.log("host ignores stale %s from %s" % (msg["kind"], src))
            return
        if msg["kind"] == "JoinWatermark":
            self.on_watermark(jr, msg)
        elif msg["kind"] == "HashReport":
            self.on_report(jr, msg)

    def selectable(self, h):
        cfg = self.sim.cfg
        if h <= 0 or h not in self.roots:
            return False
        newest = max(self.roots)
        if h < newest - (cfg.root_slots - 1) * cfg.hash_every:
            return False
        return self.sim.t < cfg.deadline_for(h)

    def on_watermark(self, jr, msg):
        cfg = self.sim.cfg
        jr.watermarks.append((self.sim.t, msg["received"], msg["applied"]))
        if jr.state == "accepted":
            return
        h = msg["applied"] // cfg.hash_every * cfg.hash_every
        while h > jr.S and not self.selectable(h):
            h -= cfg.hash_every
        if h <= jr.S or (jr.H is not None and h <= jr.H):
            self.sim.log("host: watermark applied=%d selects nothing" % msg["applied"])
            return
        jr.H = h
        jr.state = "checkpoint"
        self.sim.log("host selects H=%d for %s (deadline %d)" % (h, jr.name, cfg.deadline_for(h)))
        self.sim.schedule(cfg.deadline_for(h), P_DEADLINE, self.expire, jr.name, jr.attempt, h)

    def expire(self, name, attempt, h):
        jr = self.joins.get(name)
        if jr and jr.attempt == attempt and jr.H == h and jr.state == "checkpoint":
            self.restart(jr, 1)

    def on_report(self, jr, msg):
        cfg = self.sim.cfg
        if jr.state != "checkpoint" or msg["turn"] != jr.H:
            self.sim.log("host ignores report for %s H=%s (selected %s)" % (jr.name, msg["turn"], jr.H))
            return
        if self.sim.t >= cfg.deadline_for(jr.H):
            return
        if msg["root"] != self.roots[jr.H]:
            self.restart(jr, 2)
            return
        C = self.current()
        jr.A = C + cfg.command_delay
        jr.state = "accepted"
        jr.accept_ms = self.sim.t
        jr.event_id = self.next_event
        self.next_event += 1
        self.pending_events.setdefault(jr.A, []).append((jr.event_id, jr.seat, "human"))
        self.sim.log("host accepts H=%d from %s: C=%d A=%d" % (jr.H, jr.name, C, jr.A))
        self.sim.send("H", jr.name, 0, "SeatActive", None, 29, gen=jr.gen, attempt=jr.attempt,
                      H=jr.H, A=jr.A)

    def restart(self, jr, reason):
        jr.state = "dead"
        jr.restart_ms = self.sim.t
        jr.restart_reason = reason
        self.sim.log("host restarts %s, reason %d (table keeps running)" % (jr.name, reason))
        self.sim.send("H", jr.name, 0, "JoinRestart", None, 6, attempt=jr.attempt, reason=reason)

    def abandoned(self, dst, ch):
        jr = self.joins.get(dst)
        if jr and jr.state not in ("dead", "done"):
            self.restart(jr, 4)


class Joiner(Peer):
    def __init__(self, sim, name):
        super().__init__(sim, name)
        self.state = "idle"
        self.S = None
        self.turns = {}
        self.R = 0
        self.P = 0
        self.busy = False
        self.load_done = None
        self.roots = {}
        self.host_roots = {}
        self.attempt = None
        self.gen = None
        self.A = None
        self.H = None
        self.active_ms = None
        self.active_cursors = None
        self.freed = []
        self.applied_log = []
        self.sent_marks = set()

    def on_msg(self, src, msg):
        kind = msg["kind"]
        if kind == "JoinGrant":
            self.gen = msg["gen"]
            self.attempt = msg["attempt"]
            self.S = msg["S"]
            self.R = self.P = self.S
            self.state = "loading"
        elif kind == "Turn":
            if msg["turn"] > self.R:
                self.turns[msg["turn"]] = msg["events"]
            while self.R + 1 in self.turns:
                self.R += 1
            self.kick()
        elif kind == "HashRoot":
            self.host_roots[msg["turn"]] = msg["root"]
            self.trim(self.host_roots)
        elif kind == "SeatActive":
            self.on_seat_active(msg)
        elif kind == "JoinRestart":
            self.state = "restarted"
            self.sim.log("%s restarts its join (reason %d)" % (self.name, msg["reason"]))

    def trim(self, cache):
        slots = self.sim.cfg.root_slots
        while len(cache) > slots:
            del cache[min(cache)]

    def snapshot(self, gen, S, snap, load_ms, attempt):
        if self.state in ("restarted",) or attempt != self.attempt:
            return
        self.owners, self.hw = dict(snap[0]), snap[1]
        self.load_done = self.sim.t + load_ms
        self.state = "replaying"
        self.sim.log("%s loads snapshot (turn %d), replay from %d" % (self.name, S, self.load_done))
        self.sim.schedule(self.load_done, P_REPLAY, self.kick)

    def kick(self):
        if self.state not in ("replaying", "active", "owned") or self.busy:
            return
        if self.sim.t < self.load_done or self.P + 1 not in self.turns:
            return
        self.busy = True
        self.sim.schedule(self.sim.t + self.sim.cfg.replay_step_ms, P_REPLAY, self.complete)

    def complete(self):
        self.busy = False
        k = self.P + 1
        events = self.turns.pop(k)
        self.apply_events(events)
        self.P = k
        self.applied_log.append(k)
        cfg = self.sim.cfg
        if k % cfg.hash_every == 0:
            self.roots[k] = root_of(k, self.owners)
            self.trim(self.roots)
            self.on_hash_turn(k)
        self.recheck()
        self.kick()

    # F1 guards -------------------------------------------------------
    def on_hash_turn(self, k):
        cfg = self.sim.cfg
        if self.state != "replaying":
            return
        if k not in self.host_roots:
            return
        if self.R - k > cfg.hash_every:  # F1: the exchange starts when applied is near received
            return
        self.sim.send(self.name, "H", 0, "JoinWatermark", None, 20, gen=self.gen,
                      attempt=self.attempt, received=self.R, applied=self.P)
        self.sim.send(self.name, "H", 0, "HashReport", None, 20, gen=self.gen,
                      attempt=self.attempt, turn=k, root=self.roots[k])

    def on_seat_active(self, msg):
        self.A = msg["A"]
        self.H = msg["H"]
        if self.P >= self.A:
            self.sim.log("%s: applied %d >= A %d at SeatActive, restarts" % (self.name, self.P, self.A))
            self.state = "restarted"
            return
        self.recheck()

    def recheck(self):
        if self.state != "replaying" or self.A is None:
            return
        if self.A - 1 <= self.P < self.A:
            self.state = "active"
            self.active_ms = self.sim.t
            self.active_cursors = (self.P, self.R)
            unapplied = sorted(self.turns)
            self.freed = unapplied
            self.turns.clear()
            self.sim.log("%s ACTIVE: applied=%d received=%d, freed %d unapplied turns" % (
                self.name, self.P, self.R, len(unapplied)))

    def abandoned(self, dst, ch):
        self.sim.log("%s: pair to %s abandoned" % (self.name, dst))
        self.state = "restarted"


# ---- scenario helpers ----
def one_join(cfg, losses=(), delays=None, until=6000, snapshot_ms=150, seated=1):
    sim = Sim(cfg, losses, delays, seated=seated)
    sim.add_join("J1", 0, 2, snapshot_ms=snapshot_ms)
    sim.run(until)
    return sim


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    rules = "F1"
    if "--rules" in argv:
        i = argv.index("--rules")
        rules = argv[i + 1]
        del argv[i:i + 2]
    show_trace = "--trace" in argv
    if show_trace:
        argv.remove("--trace")
    cfg = Cfg(rules=rules)
    sim = one_join(cfg)
    j = sim.joiners["J1"]
    jr = sim.host.joins["J1"]
    print("rules=%s H=%s A=%s accept_ms=%s active_ms=%s cursors=%s" % (
        rules, jr.H, jr.A, jr.accept_ms, j.active_ms, j.active_cursors))
    if show_trace:
        print(sim.dump())
    return 0


if __name__ == "__main__":
    sys.exit(main())
