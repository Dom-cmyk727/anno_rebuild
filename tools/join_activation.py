#!/usr/bin/env python3
"""Ordered join-activation protocol checker (T52 F2/F3, ADR 0003 section 8).

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
  10 inbound ACK-only datagrams     35 input decisions (end of the delivery batch)
  20 data deliveries                40 host turn production, join asks
  25 ACK-only flush (coalesced)     50 resend checks

Every send, arrival, delivery, acknowledgement, applied turn, ownership change,
input change and admission is also appended to `Sim.events`. `validate()` re-derives
its checks from that log and the configuration alone. It does not read the `Chan`
cursors that drive delivery, and the protocol rules do not read the host's
cursor except where the host itself decides (the host knows its own turn).

`rules="F1"` reproduces the guards published at 3020900 so the round-cb
regressions can be run against them. `rules="F2"` is the proposal.

Run:

  python -I -B tools/join_activation.py [--rules F1|F2] [--trace] [row ...]
  python -I -B tools/test_join_activation.py -v
"""

from __future__ import annotations

import heapq
import sys
from collections import Counter, deque
from dataclasses import dataclass, replace

P_DEADLINE, P_ACK, P_DELIVER, P_FLUSH, P_REPLAY, P_CHECK, P_PRODUCE, P_RESEND = (
    0, 10, 20, 25, 30, 35, 40, 50)
HEADER = 25
# E1: every data datagram and every ACK-only datagram carries a u16 `epoch` field after the
# 25-byte header. The receiver compares it with its own endpoint epoch for the pair.
EPOCH_BYTES = 2
IP_UDP = 28
WIRE = HEADER + EPOCH_BYTES + IP_UDP
ASK_PAYLOAD = 5  # JoinAsk: epoch u16, seat u8, ask id u16
FRAGMENT_HEAD = 12

REASON_SEAT_BUSY = 9
REASON_SUPERSEDED = 10
REASON_NO_SLOT = 11
REASON_BACKLOG = 12
REASON_TAIL = 13
REASON_NO_GEN = 14


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
    input_host_lag_max: int = 6
    input_ready_ms: int = 20000
    max_root_pins: int = 2
    max_outstanding_wm: int = 4
    # B3: the host's observable margin. A checkpoint is offered only if the report can come
    # back before its deadline: now + rtt + select_margin_ms < deadline. The round-trip time
    # is the smoothed ACK round trip of the host-to-joiner pair; rtt_initial_ms stands in
    # until the first ACK sample (the JoinGrant's) arrives.
    rtt_initial_ms: int = 1000
    select_margin_ms: int = 100
    deadline_rtts: int = 2
    # F3 liveness limit for a seated peer without an outage record (validator iv), in turns.
    seated_lag_max: int = 20
    # F4 outages (ADR 10): a seated peer's pair is never abandoned. Without an ACK for wait_ms
    # the peer is in a logged outage; after grace_ms the seat goes to the AI and the pair closes.
    # Resends of a seated pair back off to outage_resend_ms after max_attempts sends.
    wait_ms: int = 2000
    grace_ms: int = 60000
    outage_resend_ms: int = 1000
    # F4 caps: concurrent_transfers (ADR 10), the joiner's received-but-unapplied backlog, and the
    # outstanding HashReports / InputReqs of one joiner.
    concurrent_transfers: int = 2
    receive_backlog_turns: int = 4096
    max_outstanding_reports: int = 2
    max_outstanding_req: int = 2
    # F5 retained-turn contract (ADR 8.3/8.4, 10). The rolling log keeps the last turn_buffer turns;
    # a pin keeps the turns a live join still needs; the union is capped by tail_turns_max turns and
    # tail_bytes_max bytes. Each retained turn is charged once: max(turn_store_cap, its encoded
    # bytes) (a fragmented burst turn is larger than the nominal charge and is charged what it is).
    # tail_bytes_max 0 means tail_turns_max * turn_store_cap.
    turn_buffer: int = 600
    tail_turns_max: int = 4096
    turn_store_cap: int = 1200
    tail_bytes_max: int = 0
    max_generations: int = 2
    # G2: a receiver owns the fragments of every incomplete logical message in a reassembly buffer,
    # separate from the ACK history. Per pair and channel: channel 0 (turns) holds at most
    # reassembly_fragments_max fragments (a turn of 76 fragments is 90 000 bytes of commands);
    # channel 1 (the snapshot) at most fragment_max (ADR 4, 17 130 for the largest measured save).
    # Overflow is finite: the buffer is released, a joiner aborts (reason 12), a seated peer is dropped.
    reassembly_fragments_max: int = 4096
    fragment_max: int = 17130
    # F4 seated upstream traffic: bytes of commands each seated peer sends to the host per turn
    # (0 = none) and whether it sends the section-7 hash report every hash_every turns.
    seated_cmd_bytes: int = 0
    seated_hash_reports: bool = True
    rules: str = "F2"

    @property
    def legacy(self):
        return self.rules == "F1"

    @property
    def f1(self):
        return self.rules == "F1"

    def validate(self):
        """Reject a configuration that cannot hold a single turn (G1): the caps must admit at least
        one turn of the nominal charge, and the rolling window and the reassembly buffer must exist."""
        problems = []
        if self.tail_turns_max < 1:
            problems.append("tail_turns_max < 1")
        if self.tail_bytes < self.turn_store_cap:
            problems.append("tail_bytes %d is below the nominal turn charge %d" % (self.tail_bytes, self.turn_store_cap))
        if self.turn_buffer < 1:
            problems.append("turn_buffer < 1")
        if self.reassembly_fragments_max < 1 or self.fragment_max < 1:
            problems.append("reassembly limits < 1")
        if problems:
            raise ValueError("impossible join configuration: " + "; ".join(problems))
        return self

    @property
    def tail_bytes(self):
        return self.tail_bytes_max or self.tail_turns_max * self.turn_store_cap

    def deadline_for(self, hash_turn, rtt=0):
        """Accept deadline of a root: when the host applied it plus two hash periods plus
        `deadline_rtts` round trips (the round trips are 0 in the F1 and ADR-v1 arithmetic)."""
        return (hash_turn * self.turn_ms + self.deadline_intervals * self.hash_every * self.turn_ms
                + self.deadline_rtts * rtt)


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
        self.log_next = None  # next host turn to feed from the retained log, or None
        self.frag = 0  # next fragment of that turn
        self.last_ack = 0  # millisecond of the last ACK that freed a sequence
        self.snap = None  # channel 1: the snapshot fragments still to send
        self.asm = {}  # G2: message id -> {fragment index: (bytes, sid)} of incomplete logical messages
        self.outage = False


class Sim:
    def __init__(self, cfg, losses=(), delays=None, seated=1, ai=()):
        self.cfg = cfg.validate()
        self.t = 0
        self.q = []
        self.n = 0
        self.trace = []
        self.events = []
        self.losses = set(losses)
        self.blocked_acks = set()  # directions such as "P1>H" whose ACK-only datagrams are all lost
        self.max_span = {}
        self.delays = dict(delays or {})
        self.chans = {}
        self.stats = {}
        self.peers = {}
        self.deliveries = []
        self.violations = []
        self.rtt = {}
        self.last_sid = {}  # (src, dst, ch, seq) -> sid of the datagram in the receive buffer
        self.epochs = {}  # unordered pair -> number of epoch bumps (a diagnostic count)
        self.ep = {}  # (endpoint, peer) -> the u16 epoch that endpoint stamps on and expects in packets
        self.sid = 0
        self.ack_ch = Counter()  # ACK-only datagrams by (acker, receiver, channel)
        self.max_out = {}  # (src, dst, message kind) -> most outstanding at once
        self.cuts = []  # (peer, from_ms, to_ms): everything sent to or from the peer inside is lost
        self.dropped = set()
        self.host = Host(self)
        self.peers["H"] = self.host
        self.seated = []
        initial = {0: "human"}
        initial.update({i + 1: "human" for i in range(seated)})
        initial.update({seat: "ai" for seat in ai})
        self.host.owners = dict(initial)
        self.ev("seat_init", seats=tuple(sorted(initial.items())))
        for i in range(seated):
            name = "P%d" % (i + 1)
            peer = Seated(self, name, i + 1)
            peer.owners = dict(initial)
            self.peers[name] = peer
            self.seated.append(peer)
            self.ev("seated", peer=name, seat=i + 1)
            self.chan("H", name, 0).log_next = 1
        self.joiners = {}
        self.turn_extra = lambda k: 0
        self.mid = 0
        self.reasm_frags = 0  # fragments held in all reassembly buffers right now
        self.reasm_bytes = 0
        self.peak_reasm_frags = 0
        self.peak_reasm_bytes = 0
        self.truncated = set()  # turns whose commands were rejected because one turn exceeded tail_bytes
        self.schedule(cfg.turn_ms, P_PRODUCE, self.host.produce, 1)

    # ---- event queue ----
    def schedule(self, t, prio, fn, *args):
        self.n += 1
        heapq.heappush(self.q, (t, prio, self.n, fn, args))

    def log(self, text):
        self.trace.append("%7d %s" % (self.t, text))

    def ev(self, kind, **fields):
        fields["kind"] = kind
        fields["t"] = self.t
        self.events.append(fields)

    def run(self, until):
        while self.q and self.q[0][0] <= until:
            t, _, _, fn, args = heapq.heappop(self.q)
            self.t = t
            fn(*args)
        self.t = until

    def dump(self):
        return "\n".join(self.trace)

    # ---- joiners ----
    def add_join(self, name, ask_ms, seat, snapshot_ms=None, load_ms=None, attempt=None, fragments=1):
        """`attempt` is accepted for old callers and ignored: the host assigns attempt numbers.

        The JoinGrant and `fragments` snapshot datagrams travel on channel 1 (window, ACK, loss,
        resend). `snapshot_ms` instead replaces the fragments by a LABELED INPUT: the snapshot is
        loaded that many ms after the ask (used where simulating 17 130 datagrams per run is too
        slow for a scan); the JoinGrant is still a channel-1 datagram."""
        joiner = self.peers.get(name) or Joiner(self, name)
        self.peers[name] = joiner
        self.joiners[name] = joiner
        self.schedule(ask_ms, P_PRODUCE + 1, self.host.admit, name, seat, snapshot_ms,
                      self.cfg.load_ms if load_ms is None else load_ms, fragments)
        return joiner

    def cancel(self, name, at_ms):
        """The joiner leaves (or the player cancels) at at_ms."""
        self.schedule(at_ms, P_DEADLINE, self.peers[name].cancel)

    def root_conflicts(self):
        """Turns at which two peers computed different roots, as (turn, peer, other)."""
        logs = {name: peer.root_log for name, peer in self.peers.items() if hasattr(peer, "root_log")}
        bad = []
        names = sorted(logs)
        for i, a in enumerate(names):
            for b in names[i + 1:]:
                for turn in sorted(set(logs[a]) & set(logs[b])):
                    if logs[a][turn] != logs[b][turn]:
                        bad.append((turn, a, b))
        return bad

    # ---- transport ----
    def chan(self, src, dst, ch):
        key = (src, dst, ch)
        if key not in self.chans:
            self.chans[key] = Chan()
        return self.chans[key]

    def stat(self, src, dst, field, amount=1):
        entry = self.stats.setdefault((src, dst), {"data": 0, "ack": 0, "bytes": 0, "seq": 0})
        entry[field] += amount

    @staticmethod
    def pair_key(a, b):
        return tuple(sorted((a, b)))

    def ep_of(self, endpoint, peer):
        return self.ep.get((endpoint, peer), 0)

    def reset_pair(self, a, b, why):
        """R1/E1: a new incarnation of the pair. The ask (assumed delivered, a labeled input) carries
        the next u16 epoch; both endpoints adopt it and retire the sender and receiver state of every
        channel in both directions together. Datagrams and ACK-only datagrams carry the epoch in a
        wire field; a receiver drops one whose field differs from its own epoch. Timers are local
        and carry the epoch they were created in."""
        k = self.pair_key(a, b)
        self.release_assembly(a, b, "epoch")  # explicit: no fragment survives into the new incarnation
        self.epochs[k] = self.epochs.get(k, 0) + 1
        e = self.epochs[k] % 65536
        self.ep[(a, b)] = self.ep[(b, a)] = e
        for key in list(self.chans):
            x, y = (key[1], key[2]) if key[0] == "recv" else (key[0], key[1])
            if self.pair_key(x, y) == k:
                del self.chans[key]
        self.log("transport epoch of %s<>%s is now %d (%s)" % (a, b, e, why))
        self.ev("epoch_bump", a=a, b=b, epoch=e, why=why)

    def stale(self, local, remote, epoch, what):
        """True (and logged) when `epoch`, read from a packet or a timer at `local`, is not the epoch
        `local` currently holds for `remote`. The event keeps the direction of the datagram."""
        if epoch == self.ep_of(local, remote):
            return False
        self.log("stale-epoch %s at %s from %s (field %d, now %d)" % (
            what, local, remote, epoch, self.ep_of(local, remote)))
        src, dst = (remote, local) if what in ("arrive", "flush_ack_in") else (local, remote)
        self.ev("stale_epoch", src=src, dst=dst, local=local, what=what, epoch=epoch)
        return True

    def srtt(self, a, b):
        """Smoothed round trip of the ACKs a sender a received from b, or None before a sample."""
        return self.rtt.get((a, b))

    def send(self, src, dst, ch, kind, size, **msg):
        """Queue one message; fragments over payload_max. Returns False on overflow."""
        c = self.chan(src, dst, ch)
        if c.dead:
            self.log("DROP-DEAD %s>%s ch%d %s" % (src, dst, ch, kind))
            self.ev("drop_dead", src=src, dst=dst, ch=ch, msg=kind, turn=msg.get("turn"))
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
            self.ev("overflow", src=src, dst=dst, ch=ch, msg=kind, turn=msg.get("turn"))
            return False
        msg["kind"] = kind
        self.mid += 1
        mid = ("M", self.mid)
        self.ev("enqueue", src=src, dst=dst, ch=ch, msg=kind, turn=msg.get("turn"))
        for i, s in enumerate(sizes):
            last = i == len(sizes) - 1
            c.wait.append({"kind": kind if last else kind + "-frag", "size": s,
                           "msg": msg if last else None, "attempt": 0, "acked": False,
                           "turn": msg.get("turn"), "C": msg.get("C"),
                           "mid": mid, "frag": i, "nfrag": len(sizes),
                           # control items wait until the log cursor has passed this turn
                           "barrier": self.host.applied if (src == "H" and ch == 0) else -1})
        self.pump(src, dst, ch)
        key = (src, dst, kind)
        self.max_out[key] = max(self.max_out.get(key, 0), self.outstanding(src, dst, ch, kind))
        return True

    def window_room(self, c):
        oldest = min(c.out) if c.out else c.next_seq
        return c.next_seq - oldest < self.cfg.window

    def extra(self, k):
        return 0 if k in self.truncated else self.turn_extra(k)

    def turn_sizes(self, k):
        cfg = self.cfg
        size = turn_bytes(self.host.log_turns[k], self.extra(k))
        if size <= cfg.payload_max:
            return [size]
        data = cfg.payload_max - FRAGMENT_HEAD
        count = -(-size // data)
        return [cfg.payload_max] * (count - 1) + [size - data * (count - 1) + FRAGMENT_HEAD]

    def next_datagram(self, c):
        """The next datagram of a pair: a waiting control item once the log cursor has passed
        the turn it was queued behind, else the next fragment of the next turn in the retained
        log (S1: the backlog is never copied into the queue), else nothing."""
        logged = c.log_next is not None
        if c.wait and (not logged or c.wait[0]["barrier"] < c.log_next):
            return c.wait.popleft()
        if c.snap is not None and c.snap["left"] > 0:
            # S4: the snapshot streams from the host's saved image, one fragment per window slot.
            idx = c.snap["total"] - c.snap["left"]
            c.snap["left"] -= 1
            last = c.snap["left"] == 0
            d = {"kind": "SnapshotDone" if last else "Snap-frag",
                 "size": c.snap["last_size"] if last else self.cfg.payload_max,
                 "msg": c.snap["msg"] if last else None, "attempt": 0, "acked": False,
                 "turn": None, "C": None, "mid": c.snap["mid"], "frag": idx, "nfrag": c.snap["total"]}
            return d
        if logged and c.log_next <= self.host.applied:
            k = c.log_next
            sizes = self.turn_sizes(k)
            i = c.frag
            last = i == len(sizes) - 1
            msg = {"kind": "Turn", "turn": k, "events": self.host.log_turns[k]}
            d = {"kind": "Turn" if last else "Turn-frag", "size": sizes[i], "msg": msg if last else None,
                 "attempt": 0, "acked": False, "turn": k, "C": None, "frag": i, "nfrag": len(sizes),
                 "mid": ("T", k)}
            c.frag += 1
            if c.frag == len(sizes):
                c.log_next += 1
                c.frag = 0
            return d
        return None

    def pump(self, src, dst, ch):
        c = self.chan(src, dst, ch)
        while not c.dead and self.window_room(c):
            d = self.next_datagram(c)
            if d is None:
                break
            d["seq"] = c.next_seq
            c.next_seq += 1
            self.stat(src, dst, "seq")
            d["first"] = self.t
            if ch != 2:
                c.out[d["seq"]] = d
                span = d["seq"] - min(c.out) + 1
                key = (src, dst, ch)
                self.max_span[key] = max(self.max_span.get(key, 0), span)
                if span > self.cfg.window:
                    self.violations.append(("window_exceeded", src, dst, ch, span))
            self.transmit(src, dst, ch, d)

    def feed_log(self, dst, start):
        """Give the pair host -> dst a send cursor into the retained turn log, at turn `start`."""
        c = self.chan("H", dst, 0)
        c.log_next = start
        c.frag = 0
        self.pump("H", dst, 0)

    def cut_active(self, src, dst):
        return any(self.t >= a and self.t < b and peer in (src, dst) for peer, a, b in self.cuts)

    def key(self, kind, src, dst, seq, attempt):
        return (kind, "%s>%s" % (src, dst), seq, attempt)

    def lost(self, key):
        if key[0] == "Ack" and key[1] in self.blocked_acks:
            return True
        return key in self.losses or (key[0], key[1], None, key[3]) in self.losses

    def transmit(self, src, dst, ch, d):
        d["attempt"] += 1
        d["last"] = self.t
        if "sid" not in d:
            self.sid += 1
            d["sid"] = self.sid
            d["epoch"] = self.ep_of(src, dst)  # the wire field
        key = self.key(d["kind"], src, dst, d["seq"], d["attempt"])
        wire = d["size"] + WIRE
        self.stat(src, dst, "data")
        self.stat(src, dst, "bytes", wire)
        tag = "send" if d["attempt"] == 1 else "RESEND#%d" % d["attempt"]
        lost = self.lost(key) or self.cut_active(src, dst)
        self.ev("send", src=src, dst=dst, ch=ch, seq=d["seq"], msg=d["kind"], attempt=d["attempt"],
                lost=lost, turn=d.get("turn"), C=d.get("C"), size=d["size"], sid=d["sid"], epoch=d["epoch"],
                frag=d.get("frag", 0), nfrag=d.get("nfrag", 1))
        if lost:
            self.log("%s %s>%s ch%d seq%d %s -- LOST" % (tag, src, dst, ch, d["seq"], d["kind"]))
        else:
            arrive = self.t + self.cfg.one_way_ms + self.delays.get(
                key, self.delays.get((key[0], key[1], None, key[3]), 0))
            self.log("%s %s>%s ch%d seq%d %s -> arrives %d" % (
                tag, src, dst, ch, d["seq"], d["kind"], arrive))
            self.schedule(arrive, P_DELIVER, self.arrive, src, dst, ch, d, d["epoch"])
        if ch != 2 and not self.cfg.legacy:
            gap = self.cfg.resend_ms
            if self.is_seated_pair(src, dst) and d["attempt"] >= self.cfg.max_attempts:
                gap = self.cfg.outage_resend_ms
            self.schedule(self.t + gap, P_RESEND, self.resend, src, dst, ch, d["seq"], d["epoch"])

    def is_seated_pair(self, src, dst):
        """True for a pair between the host and a peer that holds a seat (seated or finished join)."""
        other = dst if src == "H" else src
        return other in self.seated_names() and other not in self.dropped

    def seated_names(self):
        names = {p.name for p in self.seated}
        for name, jr in self.host.joins.items():
            if jr.state == "done":
                names.add(name)
        return names

    def resend(self, src, dst, ch, seq, epoch=None):
        if epoch is not None and self.stale(src, dst, epoch, "resend"):
            return
        c = self.chan(src, dst, ch)
        d = c.out.get(seq)
        if d is None or c.dead:
            return
        if self.is_seated_pair(src, dst):
            # B4/S7: a seated pair is never abandoned. Its outage is explicit and bounded.
            self.check_outage(src, dst, ch, c, d)
            if c.dead:
                return
            self.transmit(src, dst, ch, d)
            return
        if d["attempt"] >= self.cfg.max_attempts:
            c.dead = True
            self.log("ABANDON %s>%s ch%d seq%d after %d sends" % (src, dst, ch, seq, d["attempt"]))
            self.ev("abandon", src=src, dst=dst, ch=ch, seq=seq)
            self.peers[src].abandoned(dst, ch)
            return
        self.transmit(src, dst, ch, d)

    def check_outage(self, src, dst, ch, c, d):
        """An unacknowledged datagram older than wait_ms puts the peer in a logged outage; one
        older than grace_ms drops the seat to the AI and closes the pair (ADR 10)."""
        cfg = self.cfg
        peer = dst if src == "H" else src
        age = self.t - max(d["first"], c.last_ack)
        if age >= cfg.wait_ms and not c.outage:
            c.outage = True
            self.log("OUTAGE %s (no ACK for %d ms on %s>%s ch%d)" % (peer, age, src, dst, ch))
            self.ev("outage", peer=peer, src=src, dst=dst, ch=ch, age=age)
        if age >= cfg.grace_ms and peer not in self.dropped:
            c.dead = True
            self.drop_peer(peer, "grace")

    def drop_peer(self, peer, why):
        """The seat goes to the AI by an ownership event and the peer is closed: its input (if any)
        is closed first, its pairs stop, its cursor stops holding the log. A later successful
        re-ask is a new lifecycle (Host.admit clears the classification)."""
        if peer in self.dropped:
            return
        self.dropped.add(peer)
        self.release_assembly("H", peer, "drop")
        for key, c in self.chans.items():
            x, y = (key[1], key[2]) if key[0] == "recv" else (key[0], key[1])
            if peer in (x, y):
                c.dead = True
        pj = self.peers.get(peer)
        if hasattr(pj, "close_input"):
            pj.close_input("seat dropped")
            pj.state = "restarted"
        self.log("DROP %s (%s), seat goes to the AI" % (peer, why))
        self.ev("drop_seated", peer=peer, why=why)
        self.host.drop_seat(peer)

    def arrive(self, src, dst, ch, d, epoch=None):
        if self.stale(dst, src, d["epoch"], "arrive"):  # the field of the packet, not a simulator label
            return
        c = self.recv_state(src, dst, ch)
        if c.dead:  # a closed receive side (drop, overflow) takes nothing more
            return
        seq = d["seq"]
        dup = seq in c.recv
        c.recv.setdefault(seq, d)
        self.last_sid[(src, dst, ch, seq)] = c.recv[seq].get("sid")
        c.highest = max(c.highest, seq)
        self.log("arrive %s>%s ch%d seq%d %s%s" % (src, dst, ch, seq, d["kind"], " (dup)" if dup else ""))
        self.ev("arrive", src=src, dst=dst, ch=ch, seq=seq, msg=d["kind"], dup=dup, turn=d.get("turn"),
                sid=d.get("sid"), epoch=d.get("epoch"), frag=d.get("frag", 0), nfrag=d.get("nfrag", 1))
        if not dup:
            self.peers[dst].on_arrive(src, ch, d)
        if ch == 2 or self.cfg.legacy:
            if not dup and d["msg"] is not None:
                self.deliver(src, dst, ch, seq, d["msg"])
            for old in [q for q in c.recv if q < c.highest - 64]:
                del c.recv[old]
                self.last_sid.pop((src, dst, ch, old), None)
        else:
            while c.deliv in c.recv:
                nxt = c.recv[c.deliv]
                if not nxt.get("passed"):
                    nxt["passed"] = True
                    if not self.reassemble(src, dst, ch, c, nxt, c.deliv):
                        break
                c.deliv += 1
            # Only the 32 sequences below the highest are ever reported in an ACK: older entries of
            # the delivered prefix are released (bounded receive state).
            for old in [q for q in c.recv if q < min(c.deliv, c.highest) - 40]:
                del c.recv[old]
                self.last_sid.pop((src, dst, ch, old), None)
        if ch != 2 and not c.ack_pending:
            c.ack_pending = True
            self.schedule(self.t, P_FLUSH, self.flush_ack, src, dst, ch, self.ep_of(dst, src))

    def recv_state(self, src, dst, ch):
        key = ("recv", src, dst, ch)
        if key not in self.chans:
            self.chans[key] = Chan()
        return self.chans[key]

    def asm_account(self, c, frags, size):
        self.reasm_frags += frags
        self.reasm_bytes += size
        self.peak_reasm_frags = max(self.peak_reasm_frags, self.reasm_frags)
        self.peak_reasm_bytes = max(self.peak_reasm_bytes, self.reasm_bytes)

    def reassemble(self, src, dst, ch, c, d, seq):
        """A datagram passes the ordered channel in sequence. The fragments of an unfinished logical
        message move into the pair's reassembly buffer, which is the only owner of the pieces once
        the ACK history is pruned. The message is delivered when the buffer holds every fragment
        index 0..nfrag-1 of it; the last datagram alone does not deliver it. Returns False when the
        buffer overflowed (the channel stops)."""
        n = d.get("nfrag", 1)
        if n == 1:
            if d["msg"] is not None:
                self.deliver(src, dst, ch, seq, d["msg"], parts=((0, d.get("sid")),), nfrag=1)
            return True
        frags = c.asm.setdefault(d["mid"], {})
        if d["frag"] in frags:
            return True  # a duplicate never takes a second slot
        limit = self.cfg.reassembly_fragments_max if ch == 0 else self.cfg.fragment_max
        held = sum(len(f) for f in c.asm.values())
        if held + 1 > limit:
            self.log("REASSEMBLY OVERFLOW %s>%s ch%d (%d fragments held, limit %d)" % (src, dst, ch, held, limit))
            self.ev("reasm_overflow", src=src, dst=dst, ch=ch, mid=d["mid"], held=held, limit=limit)
            self.release_assembly_chan(src, dst, ch, c, "overflow")
            c.dead = True
            self.reassembly_overflow(src, dst, ch)
            return False
        frags[d["frag"]] = (d["size"], d.get("sid"))
        self.asm_account(c, 1, d["size"])
        self.ev("reasm_acquire", src=src, dst=dst, ch=ch, mid=d["mid"], turn=d.get("turn"), frag=d["frag"],
                nfrag=n, size=d["size"], sid=d.get("sid"), held=held + 1)
        if len(frags) == n and set(frags) == set(range(n)):
            parts = tuple(sorted((f, v[1]) for f, v in frags.items()))
            size = sum(v[0] for v in frags.values())
            del c.asm[d["mid"]]
            self.asm_account(c, -n, -size)
            self.ev("reasm_release", src=src, dst=dst, ch=ch, mid=d["mid"], turn=d.get("turn"),
                    frags=tuple(sorted(frags)), reason="delivered")
            self.deliver(src, dst, ch, seq, d["msg"], parts=parts, nfrag=n)
        return True

    def release_assembly_chan(self, src, dst, ch, c, reason):
        for mid, frags in list(c.asm.items()):
            size = sum(v[0] for v in frags.values())
            self.ev("reasm_release", src=src, dst=dst, ch=ch, mid=mid,
                    turn=mid[1] if mid[0] == "T" else None, frags=tuple(sorted(frags)), reason=reason)
            self.asm_account(c, -len(frags), -size)
            del c.asm[mid]

    def release_assembly(self, a, b, reason):
        """Explicit cleanup of every reassembly buffer between two endpoints (epoch, drop, attempt end)."""
        for key, c in list(self.chans.items()):
            if key[0] == "recv" and {key[1], key[2]} == {a, b}:
                self.release_assembly_chan(key[1], key[2], key[3], c, reason)

    def reassembly_overflow(self, src, dst, ch):
        """Finite overflow behavior: the receiving side gives up this stream. A joiner aborts, a
        seated peer is dropped by the host, a host-side buffer is a recorded violation."""
        peer = self.peers.get(dst)
        if dst == "H":
            self.violations.append(("reassembly_overflow", src, dst, ch))
        elif hasattr(peer, "check_backlog"):
            peer.state = "restarted"
            self.send(dst, "H", 2, "JoinAbort", 8, gen=getattr(peer, "gen", None), attempt=getattr(peer, "attempt", None),
                      reason=REASON_BACKLOG)
        else:
            self.drop_peer(dst, "reassembly cap")

    def deliver(self, src, dst, ch, seq, msg, parts=None, nfrag=1):
        self.deliveries.append((self.t, src, dst, ch, seq, msg["kind"]))
        self.ev("deliver", src=src, dst=dst, ch=ch, seq=seq, msg=msg["kind"], turn=msg.get("turn"),
                sid=self.last_sid.get((src, dst, ch, seq)), epoch=self.ep_of(dst, src),
                parts=parts if parts is not None else ((0, self.last_sid.get((src, dst, ch, seq))),), nfrag=nfrag)
        if not self.cfg.legacy and ch == 0:
            c = self.recv_state(src, dst, ch)
            if seq > c.deliv:
                self.violations.append(("out_of_order_delivery", src, dst, seq, c.deliv))
        self.log("deliver %s>%s ch%d seq%d %s" % (src, dst, ch, seq, msg["kind"]))
        self.peers[dst].on_msg(src, msg)

    def flush_ack(self, src, dst, ch, epoch=None):
        if epoch is not None and self.stale(dst, src, epoch, "flush"):  # local timer at the acker
            return
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
        self.ack_ch[(dst, src, ch)] += 1
        self.stat(dst, src, "bytes", WIRE)
        if self.lost(key) or self.cut_active(dst, src):
            self.log("ack-only %s>%s ch%d #%d ack=%d -- LOST" % (dst, src, ch, idx, ack))
            return
        self.log("ack-only %s>%s ch%d #%d ack=%d bits=%x" % (dst, src, ch, idx, ack, bits))
        # the ACK-only datagram carries the acker's epoch in its wire field
        self.schedule(self.t + self.cfg.one_way_ms, P_ACK, self.take_ack, src, dst, ch, ack, bits,
                      self.ep_of(dst, src))

    def take_ack(self, src, dst, ch, ack, bits, epoch=None):
        """The sender src learns what dst received."""
        if epoch is not None and self.stale(src, dst, epoch, "ack"):  # compared with src's own epoch
            return
        c = self.chan(src, dst, ch)
        named = {ack}
        for i in range(32):
            if bits >> i & 1:
                named.add(ack - (i + 1))
        freed = 0
        for seq in sorted(named):
            d = c.out.pop(seq, None)
            if d is not None:
                freed += 1
            if d is not None and d["attempt"] == 1:
                # Karn: only a datagram that was never resent gives a round-trip sample.
                sample = self.t - d["first"]
                old = self.rtt.get((src, dst))
                self.rtt[(src, dst)] = sample if old is None else (7 * old + sample) // 8
        self.ev("ack", src=src, dst=dst, ch=ch, seqs=tuple(sorted(named)), epoch=epoch)
        if freed:
            c.last_ack = self.t
            if c.outage:
                c.outage = False
                peer = dst if src == "H" else src
                self.log("OUTAGE over for %s" % peer)
                self.ev("outage_end", peer=peer, src=src, dst=dst, ch=ch)
        self.pump(src, dst, ch)
        self.peers[src].on_acked(dst, ch)

    def outstanding(self, src, dst, ch, kind):
        """Messages of `kind` sent or waiting on this pair and channel and not yet acknowledged."""
        c = self.chan(src, dst, ch)
        return sum(1 for d in c.out.values() if d["kind"] == kind) + sum(
            1 for d in c.wait if d["kind"] == kind)


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
            if owner is None:
                self.owners.pop(seat, None)  # an empty seat has no entry
            else:
                self.owners[seat] = owner
            self.sim.log("%s owner seat %d -> %s (event %d)" % (self.name, seat, owner, event_id))

    def log_applied(self, turn):
        self.sim.ev("apply", peer=self.name, turn=turn, owners=tuple(sorted(self.owners.items())),
                    root=root_of(turn, self.owners))

    def on_arrive(self, src, ch, d):
        pass

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
        self.root_log = {}
        self.pending = {}

    def on_msg(self, src, msg):
        if msg["kind"] == "Turn":
            self.pending[msg["turn"]] = msg
            while self.applied + 1 in self.pending:
                m = self.pending.pop(self.applied + 1)
                self.applied += 1
                self.apply_events(m["events"])
                self.log_applied(self.applied)
                cfg = self.sim.cfg
                if self.applied % cfg.hash_every == 0:
                    self.root_log[self.applied] = root_of(self.applied, self.owners)
                    if cfg.seated_hash_reports:  # section 7: the unreliable hash report to the host
                        self.sim.send(self.name, "H", 2, "SeatHash", 20, turn=self.applied)
                if cfg.seated_cmd_bytes:  # a configurable command stream, reliable and ordered
                    self.sim.send(self.name, "H", 0, "Cmd", cfg.seated_cmd_bytes, turn=None)


class JoinHost:
    def __init__(self, name, seat, snapshot_turn, gen, attempt, ask_ms):
        self.name = name
        self.seat = seat
        self.S = snapshot_turn
        self.gen = gen
        self.attempt = attempt
        self.ask_ms = ask_ms
        self.state = "waiting"  # waiting checkpoint accepted done dead
        self.H = None
        self.floor = 0  # a re-selection must be above every expired checkpoint
        self.sel_ms = None
        self.deadline = None
        self.A = None
        self.event_id = None
        self.accept_ms = None
        self.restart_ms = None
        self.restart_reason = None
        self.released_at = None
        self.done_ms = None
        self.go_ms = None
        self.prev_owner = None
        self.watermarks = []
        self.selected = []
        self.expired = []


class Host(Peer):
    def __init__(self, sim):
        super().__init__(sim, "H")
        self.owners = {0: "human"}
        self.applied = 0
        self.roots = {}
        self.root_log = {}
        self.root_sent = {}
        self.pending_events = {}
        self.next_event = 1
        self.joins = {}
        self.history = []
        self.attempts = {}
        self.gens = []
        self.gen_state = {}
        self.next_gen = 1
        self.log_turns = {}  # OPERATIONAL: the retained turns (rolling log plus pins), see retain()
        self.turn_charge = {}  # turn -> bytes charged once for it
        self.retained_bytes = 0
        self.peak_turns = 0
        self.peak_bytes = 0
        self.max_gen_state = 0
        self.max_slots = 0

    # ---- turns ----
    def produce(self, k):
        sim = self.sim
        cfg = sim.cfg
        events = self.pending_events.pop(k, [])
        events.sort(key=lambda e: e[0])
        self.apply_events(events)
        self.applied = k
        self.retain(k, tuple(events))
        sim.ev("produce", turn=k)
        self.log_applied(k)
        if k % cfg.hash_every == 0:
            self.roots[k] = root_of(k, self.owners)
            self.root_log[k] = self.roots[k]
            self.root_sent[k] = sim.t
            sim.log("host applies %d, root sent (deadline %d)" % (k, cfg.deadline_for(k)))
            self.prune_roots()
        for peer in sim.seated:
            if peer.name not in sim.dropped:
                sim.pump("H", peer.name, 0)
        for name, jr in sorted(self.joins.items()):
            if jr.state == "dead" or k <= jr.S:  # a finished join is an ordinary seated peer
                continue
            sim.pump("H", name, 0)
            if k % cfg.hash_every == 0 and cfg.f1:
                sim.send("H", name, 2, "HashRoot", 20, turn=k, root=self.roots[k])
        sim.schedule((k + 1) * cfg.turn_ms, P_PRODUCE, self.produce, k + 1)

    # ---- the retained-turn contract (R2) ----
    def charge_of(self, events, k):
        """Charged once, whoever needs the turn: the nominal turn_store_cap, or the turn's encoded
        bytes when a command burst makes it larger (a fragmented turn is charged what it is)."""
        return max(self.sim.cfg.turn_store_cap, turn_bytes(events, self.sim.extra(k)))

    def collect_gens(self):
        """Free the saved image of every generation that no live attempt owns."""
        live = {j.gen for j in self.joins.values() if j.state not in ("dead", "done")}
        for g in [g for g in self.gen_state if g not in live]:
            del self.gen_state[g]
            self.sim.ev("gen_free", gen=g)
        self.gens = [(g, gs, gt) for g, gs, gt in self.gens if g in live]

    def consumers(self):
        """Who needs old turns, as (first needed turn, kind, name): live joins (their pin) and the
        send cursors of live peers. The rolling window is not a consumer."""
        out = []
        for name, jr in sorted(self.joins.items()):
            if jr.state not in ("dead", "done"):
                out.append((jr.S + 1, "join", name))
        for key, c in self.sim.chans.items():  # insertion order is deterministic; callers sort
            if key[0] != "H" or key[2] != 0 or c.log_next is None or key[1] in self.sim.dropped:
                continue
            jr = self.joins.get(key[1])
            if jr is not None and jr.state not in ("dead", "done"):
                continue  # a live join is already counted by its pin
            out.append((c.log_next, "peer", key[1]))
        return out

    def kept_floor(self, top, extra):
        """The lowest turn kept when turn `top` (charged `extra`, not yet stored) is added: the lower
        of the consumers' need and the rolling window, which shrinks until the bytes fit. Releases
        are projected here, before deciding that a new turn does not fit."""
        cfg = self.sim.cfg
        need = [f for f, _, _ in self.consumers()]
        cf = min(need) if need else top
        rf = max(1, top - cfg.turn_buffer + 1, top - cfg.tail_turns_max + 1)  # rolling window, and the count cap
        total = extra
        low = top
        for t in range(top - 1, rf - 1, -1):
            c = self.turn_charge.get(t)
            if c is None:
                break
            if total + c > cfg.tail_bytes:
                break
            total += c
            low = t
        rf = max(rf, low)
        return min(cf, rf)

    def kept_bytes(self, floor, top, extra):
        return extra + sum(self.turn_charge.get(t, 0) for t in range(floor, top))

    def retain(self, k, events):
        """Keep turn k only inside the budget (`tail_turns_max` turns, `tail_bytes` bytes), checked
        before it is stored and after projecting every release that is already due.

        Policy when it would not fit, in this order:
          1. the rolling window shrinks (a returning peer past it rejoins through section 8);
          2. the consumer with the oldest need goes: a live join is restarted (reason 13), a seated
             peer's cursor is dropped to the AI as at the grace (`drop_seated`, why `log cap`);
             a younger consumer that fits stays;
          3. a single turn larger than `tail_bytes` is never stored: its commands are rejected and an
             empty turn takes its place (`oversized_turn`, also a recorded violation).
        A turn some live consumer still needs is never discarded silently."""
        sim, cfg = self.sim, self.sim.cfg
        charge = self.charge_of(events, k)
        if charge > cfg.tail_bytes:
            sim.log("host: turn %d is %d bytes, over tail_bytes; commands rejected" % (k, charge))
            sim.ev("oversized_turn", turn=k, bytes=charge)
            sim.violations.append(("oversized_turn", k, charge))
            sim.truncated.add(k)
            charge = self.charge_of(events, k)  # the ownership events stay; the commands are what is rejected
        while True:
            floor = self.kept_floor(k, charge)
            over = (self.kept_bytes(floor, k, charge) > cfg.tail_bytes
                    or k - floor + 1 > cfg.tail_turns_max)
            if not over:
                break
            need = sorted(self.consumers())
            if not need:
                break  # nothing but the shrunk rolling window remains
            first, kind, name = need[0]
            sim.log("host: keeping turn %d would pass the cap; %s %s (needs from %d) goes" % (k, kind, name, first))
            sim.ev("tail_cap", name=name, consumer=kind, turn=k, bytes=self.kept_bytes(floor, k, charge))
            if kind == "join":
                oldest = min(jr.S for jr in self.joins.values() if jr.state not in ("dead", "done"))
                for jn, jr in sorted(self.joins.items()):
                    if jr.state not in ("dead", "done") and jr.S == oldest:
                        self.restart(jr, REASON_TAIL)
            else:
                sim.drop_peer(name, "log cap")
        floor = self.kept_floor(k, charge)
        self.release(floor)
        self.log_turns[k] = events
        self.turn_charge[k] = charge
        self.retained_bytes += charge
        sim.ev("retain", turn=k, bytes=charge, total_bytes=self.retained_bytes, count=len(self.log_turns),
               nfrag=len(sim.turn_sizes(k)))
        self.peak_turns = max(self.peak_turns, len(self.log_turns))
        self.peak_bytes = max(self.peak_bytes, self.retained_bytes)

    def release(self, floor):
        """Drop every retained turn below `floor`."""
        for q in [q for q in self.log_turns if q < floor]:
            self.retained_bytes -= self.turn_charge.pop(q)
            del self.log_turns[q]
            self.sim.ev("release", turn=q)

    def operational(self):
        """What the host holds for the protocol right now (diagnostics excluded)."""
        sim = self.sim
        return {"reasm_fragments": sim.reasm_frags, "reasm_bytes": sim.reasm_bytes, "log_turns": len(self.log_turns), "log_bytes": self.retained_bytes,
                "images": len(self.gen_state),
                "in_flight": sum(len(c.out) for k, c in sim.chans.items() if k[0] != "recv"),
                "waiting": sum(len(c.wait) for k, c in sim.chans.items() if k[0] != "recv"),
                "reassembly": sum(len(c.recv) for k, c in sim.chans.items() if k[0] == "recv")}

    def drop_seat(self, peer):
        """Past grace_ms the seat goes to the AI by an ownership event in the turn stream (ADR 10)."""
        seat = next((p.seat for p in self.sim.seated if p.name == peer), None)
        if seat is None and peer in self.joins:
            seat = self.joins[peer].seat
        if seat is None:
            return
        r = self.applied + self.sim.cfg.command_delay
        self.pending_events.setdefault(r, []).append((self.next_event, seat, "ai"))
        self.sim.log("host queues the AI for seat %d of dropped %s in turn %d (event %d)" % (
            seat, peer, r, self.next_event))
        self.next_event += 1

    def current(self):
        """The last turn the host has produced, hence enqueued to every live peer (B2): never a
        turn that is still to be produced in this millisecond."""
        return self.applied

    def pins(self):
        return {jr.H for jr in self.joins.values()
                if jr.H is not None and jr.state in ("checkpoint", "accepted")}

    def prune_roots(self):
        cfg = self.sim.cfg
        newest = sorted(self.roots)[-cfg.root_slots:]
        keep = set(newest) | self.pins()
        for h in list(self.roots):
            if h not in keep:
                del self.roots[h]
                self.root_sent.pop(h, None)
        self.max_slots = max(self.max_slots, len(self.roots))

    def pinned_span(self):
        starts = [jr.S + 1 for jr in self.joins.values() if jr.state not in ("dead", "done")]
        return self.applied - min(starts) + 1 if starts else 0

    def rtt_for(self, name):
        sample = self.sim.srtt("H", name)
        return self.sim.cfg.rtt_initial_ms if sample is None else sample

    def deadline(self, h, name):
        """Accept deadline: the host applied root h at root_sent[h]; two hash periods and
        `deadline_rtts` measured round trips are allowed after that."""
        cfg = self.sim.cfg
        return (self.root_sent[h] + cfg.deadline_intervals * cfg.hash_every * cfg.turn_ms
                + cfg.deadline_rtts * self.rtt_for(name))

    # ---- admission ----
    def seat_holder(self, seat, name):
        """Who holds a seat: the seated human or a live attempt of another peer, else None."""
        if self.owners.get(seat) == "human":
            prev = self.joins.get(name)
            if not (prev is not None and prev.seat == seat and prev.state not in ("done",)):
                return "seated human"
        for other, jr in sorted(self.joins.items()):
            if other != name and jr.seat == seat and jr.state != "dead" and other not in self.sim.dropped:
                return "attempt of " + other
        return None

    def admit(self, name, seat, snapshot_ms, load_ms, fragments=1):
        sim = self.sim
        prev = self.joins.get(name)
        if prev is not None:
            if prev.state not in ("dead", "done"):
                self.restart(prev, REASON_SUPERSEDED)  # the same peer asks again: retire the old attempt
            self.history.append(prev)
        attempt = self.attempts.get(name, 0) + 1
        self.attempts[name] = attempt
        sim.dropped.discard(name)  # a new ask opens a new lifecycle: the old drop no longer classifies the peer
        if attempt > 1:
            # R1: the ask of a peer that was here before carries a new transport epoch, so both
            # endpoints retire the old channels together. The ask is one datagram (8-byte payload).
            sim.stat(name, "H", "data")
            sim.stat(name, "H", "bytes", ASK_PAYLOAD + WIRE)
            sim.reset_pair("H", name, "re-ask, attempt %d" % attempt)
        holder = self.seat_holder(seat, name)
        live = sum(1 for n, j in self.joins.items() if n != name and j.state not in ("dead", "done"))
        self.collect_gens()
        shareable = None
        for g, gs, gt in self.gens:
            if sim.t - gt <= sim.cfg.snapshot_age_ms:
                shareable = (g, gs)
                break
        if holder is None and live >= sim.cfg.concurrent_transfers:
            holder = "no free transfer slot"
        if holder is None and shareable is None and len(self.gens) >= sim.cfg.max_generations:
            holder = "no free generation"
        if holder is not None:
            jr = JoinHost(name, seat, 0, 0, attempt, sim.t)
            jr.state = "dead"
            jr.restart_ms = sim.t
            jr.restart_reason = {"no free transfer slot": REASON_NO_SLOT,
                                 "no free generation": REASON_NO_GEN}.get(holder, REASON_SEAT_BUSY)
            self.joins[name] = jr
            sim.log("host refuses %s seat %d (%s), attempt %d" % (name, seat, holder, attempt))
            sim.ev("refuse", name=name, attempt=attempt, seat=seat, holder=holder)
            sim.send("H", name, 0, "JoinRestart", 4, attempt=attempt, reason=jr.restart_reason)
            return
        S = self.applied
        gen = None
        if shareable is not None:
            gen, S = shareable  # a shared generation is the saved image, not a new one
        if gen is None:
            gen = self.next_gen  # identities are never reused
            self.next_gen += 1
            self.gens.append((gen, S, sim.t))
            self.gen_state[gen] = (dict(self.owners), self.hw)
            self.max_gen_state = max(self.max_gen_state, len(self.gen_state))
            sim.ev("gen_new", gen=gen, S=S)
        jr = JoinHost(name, seat, S, gen, attempt, sim.t)
        jr.prev_owner = self.owners.get(seat)  # as published in the stream: None for an empty seat
        self.joins[name] = jr
        sim.log("host admits %s seat %d, generation %d, snapshot turn %d, attempt %d" % (
            name, seat, gen, S, attempt))
        sim.ev("admit", name=name, attempt=attempt, seat=seat, gen=gen, S=S)
        # S4: the grant and the snapshot are channel-1 datagrams with window, ACK, loss and resend.
        sim.send("H", name, 1, "JoinGrant", 15, gen=gen, S=S, seat=seat, attempt=attempt)
        # S1: turns S+1 onward follow on channel 0 from the retained log, through a cursor.
        sim.feed_log(name, S + 1)
        if snapshot_ms is None:
            data = sim.cfg.payload_max - FRAGMENT_HEAD
            msg = {"kind": "SnapshotDone", "gen": gen, "S": S, "attempt": attempt,
                   "snap": self.gen_state[gen], "load_ms": load_ms}
            c1 = sim.chan("H", name, 1)
            c1.snap = {"left": fragments, "total": fragments, "msg": msg, "last_size": 100,
                       "mid": ("S", name, attempt)}
            sim.pump("H", name, 1)
        else:  # a LABELED INPUT: the snapshot is loaded snapshot_ms after the ask
            sim.schedule(sim.t + snapshot_ms, P_DELIVER, sim.peers[name].snapshot, gen, S,
                         self.gen_state[gen], load_ms, attempt)
        sim.schedule(sim.t + sim.cfg.catchup_ms, P_DEADLINE, self.catchup_expire, name, attempt)

    def catchup_expire(self, name, attempt):
        jr = self.joins.get(name)
        if jr and jr.attempt == attempt and jr.state not in ("dead", "done"):
            self.restart(jr, 6)

    # ---- messages from joiners ----
    def on_msg(self, src, msg):
        if msg["kind"] in ("Cmd", "SeatHash"):
            return  # seated upstream traffic: counted, not interpreted by the join checker
        jr = self.joins.get(src)
        if jr is None or msg.get("attempt") != jr.attempt or jr.state in ("dead", "done"):
            self.sim.log("host ignores stale %s from %s" % (msg["kind"], src))
            return
        if msg["kind"] == "JoinWatermark":
            self.on_watermark(jr, msg)
        elif msg["kind"] == "HashReport":
            self.on_report(jr, msg)
        elif msg["kind"] == "JoinAbort":
            self.restart(jr, msg["reason"])
        elif msg["kind"] == "InputReq":
            self.on_input_req(jr, msg)
        elif msg["kind"] == "InputOn":
            jr.state = "done"
            jr.done_ms = self.sim.t
            self.sim.ev("attempt_end", name=jr.name, attempt=jr.attempt, how="done")
            self.collect_gens()
            self.sim.log("host: %s has input, pin released" % jr.name)

    def selectable(self, h, jr):
        cfg = self.sim.cfg
        if h <= 0 or h not in self.roots:
            return False
        newest = max(self.roots)
        if h < newest - (cfg.root_slots - 1) * cfg.hash_every and h not in self.pins():
            return False
        if cfg.f1:
            return self.sim.t < cfg.deadline_for(h)
        # The report must be able to come back: one round trip plus a margin before the deadline.
        return self.sim.t + self.rtt_for(jr.name) + cfg.select_margin_ms < self.deadline(h, jr.name)

    def on_watermark(self, jr, msg):
        cfg = self.sim.cfg
        jr.watermarks.append((self.sim.t, msg["received"], msg["applied"]))
        if jr.state == "accepted":
            if cfg.f1:
                return
            if msg["applied"] >= jr.A:
                self.sim.send("H", jr.name, 0, "InputGrant", 14, gen=jr.gen,
                              attempt=jr.attempt, C=self.current())
            return
        if jr.state == "checkpoint" and not cfg.f1:
            # Supersede only once the report of the current checkpoint has had a round trip.
            if self.sim.t < jr.sel_ms + self.rtt_for(jr.name) + cfg.select_margin_ms:
                self.sim.log("host: watermark applied=%d waits for the report of H=%d" % (
                    msg["applied"], jr.H))
                return
        h = msg["applied"] // cfg.hash_every * cfg.hash_every
        while h > jr.S and not self.selectable(h, jr):
            h -= cfg.hash_every
        if h <= jr.S or h <= jr.floor or (jr.H is not None and h <= jr.H):
            self.sim.log("host: watermark applied=%d selects nothing" % msg["applied"])
            return
        jr.H = h
        jr.state = "checkpoint"
        jr.sel_ms = self.sim.t
        jr.deadline = cfg.deadline_for(h) if cfg.f1 else self.deadline(h, jr.name)
        jr.selected.append((self.sim.t, h))
        self.sim.log("host selects H=%d for %s (deadline %d)" % (h, jr.name, jr.deadline))
        self.sim.schedule(jr.deadline, P_DEADLINE, self.expire, jr.name, jr.attempt, h)
        if not cfg.f1:
            self.sim.send("H", jr.name, 0, "JoinCheckpoint", 22, gen=jr.gen, attempt=jr.attempt,
                          turn=h, root=self.roots[h])

    def expire(self, name, attempt, h):
        jr = self.joins.get(name)
        if jr and jr.attempt == attempt and jr.H == h and jr.state == "checkpoint":
            if self.sim.cfg.f1:
                self.restart(jr, 1)
                return
            # F3: one checkpoint expiring is not a failed join. Wait for a later watermark.
            jr.state = "waiting"
            jr.floor = max(jr.floor, h)
            jr.expired.append((self.sim.t, h))
            self.sim.log("host: checkpoint H=%d of %s expired, waits for a later watermark" % (h, name))
            self.sim.ev("checkpoint_expired", name=name, attempt=attempt, H=h)

    def on_report(self, jr, msg):
        cfg = self.sim.cfg
        if jr.state != "checkpoint" or msg["turn"] != jr.H:
            self.sim.log("host ignores report for %s H=%s (selected %s)" % (jr.name, msg["turn"], jr.H))
            return
        deadline = cfg.deadline_for(jr.H) if cfg.f1 else jr.deadline
        if self.sim.t >= deadline:
            self.sim.log("host: report for %d is at/after its deadline %d" % (jr.H, deadline))
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
        self.sim.log("host accepts H=%d from %s: C=%d A=%d (event %d in turn %d)" % (
            jr.H, jr.name, C, jr.A, jr.event_id, jr.A))
        if cfg.f1:
            self.sim.send("H", jr.name, 0, "SeatActive", 29, gen=jr.gen, attempt=jr.attempt,
                          H=jr.H, A=jr.A)
        else:
            self.sim.schedule(self.sim.t + cfg.input_ready_ms, P_DEADLINE, self.input_expire,
                              jr.name, jr.attempt)

    def on_input_req(self, jr, msg):
        """B1: the joiner asks to open input and states how far it has applied. The host knows
        its own turn, so a bound on the difference is a protocol rule here, not an oracle."""
        cfg = self.sim.cfg
        if jr.state != "accepted":
            return
        lag = self.applied - msg["applied"]
        # The request is one one-way old: allow the round trip's worth of turns on top.
        bound = cfg.input_host_lag_max + -(-self.rtt_for(jr.name) // cfg.turn_ms)
        if msg["applied"] < jr.A or lag > bound:
            self.sim.log("host: %s InputReq applied=%d refused (host %d, A=%d)" % (
                jr.name, msg["applied"], self.applied, jr.A))
            return
        jr.go_ms = self.sim.t
        self.sim.log("host: %s InputReq applied=%d accepted (host %d)" % (jr.name, msg["applied"], self.applied))
        self.sim.send("H", jr.name, 0, "InputGo", 8, gen=jr.gen, attempt=jr.attempt, nonce=msg["nonce"])

    def input_expire(self, name, attempt):
        jr = self.joins.get(name)
        if jr and jr.attempt == attempt and jr.state == "accepted":
            grace = 2 * self.rtt_for(name) + 2 * self.sim.cfg.turn_ms
            if jr.go_ms is not None and self.sim.t < jr.go_ms + grace:
                # An InputGo is in flight: let it land, the host will not restart under it.
                self.sim.schedule(jr.go_ms + grace, P_DEADLINE, self.input_expire, name, attempt)
                return
            self.restart(jr, 5)

    def restart(self, jr, reason):
        jr.state = "dead"
        jr.restart_ms = self.sim.t
        jr.restart_reason = reason
        self.sim.log("host restarts %s, reason %d (table keeps running)" % (jr.name, reason))
        self.sim.release_assembly("H", jr.name, "attempt end")  # explicit, before the lifecycle event
        self.sim.ev("attempt_end", name=jr.name, attempt=jr.attempt, how="dead")
        self.sim.chan("H", jr.name, 0).log_next = None  # a dead attempt needs no more turns
        self.collect_gens()
        if jr.A is not None and not self.sim.cfg.f1:
            # The ownership event is already in the history. Release the seat the same way,
            # to the owner the stream showed before the attempt (an empty seat stays empty).
            r = self.current() + self.sim.cfg.command_delay
            jr.released_at = r
            self.pending_events.setdefault(r, []).append((self.next_event, jr.seat, jr.prev_owner))
            self.sim.log("host queues SeatRelease event %d for seat %d in turn %d (to %s)" % (
                self.next_event, jr.seat, r, jr.prev_owner))
            self.next_event += 1
        self.sim.send("H", jr.name, 0, "JoinRestart", 4, attempt=jr.attempt, reason=reason)

    def abandoned(self, dst, ch):
        jr = self.joins.get(dst)
        if jr and jr.state not in ("dead", "done"):
            self.restart(jr, 4)


class Joiner(Peer):
    def __init__(self, sim, name):
        super().__init__(sim, name)
        self.state = "idle"  # loading replaying owned active restarted
        self.S = None
        self.turns = {}
        self.R = 0
        self.P = 0
        self.known = 0
        self.busy = False
        self.load_done = None
        self.roots = {}
        self.root_log = {}
        self.host_roots = {}
        self.pins = []
        self.attempt = None
        self.gen = None
        self.seat = None
        self.A = None
        self.H = None
        self.active_ms = None
        self.active_cursors = None
        self.closed_ms = None
        self.freed = []
        self.applied_log = []
        self.last_wm = 0
        self.grants_rejected = []
        self.max_root_slots = 0
        self.pending_grant = False
        self.pending_go = False
        self.decide_scheduled = False
        self.grant_C = None
        self.stash = {}  # turns that arrived before the JoinGrant of this attempt
        self.reqs = {}  # nonce -> millisecond the InputReq was sent
        self.go_nonce = None
        self.nonce = 0

    def reset_for(self, msg):
        """A JoinGrant opens a new attempt: everything of the previous attempt is retired."""
        self.close_input("new attempt")
        self.gen = msg["gen"]
        self.attempt = msg["attempt"]
        self.S = msg["S"]
        self.seat = msg["seat"]
        self.R = self.P = self.known = self.S
        self.turns = {k: e for k, e in self.stash.items() if k > self.S}
        self.stash = {}
        while self.R + 1 in self.turns:
            self.R += 1
        self.known = max(self.known, self.R)
        self.roots = {}
        self.host_roots = {}
        self.pins = []
        self.A = self.H = None
        self.busy = False
        self.load_done = None
        self.active_ms = None
        self.active_cursors = None
        self.closed_ms = None
        self.freed = []
        self.applied_log = []
        self.last_wm = 0
        self.grants_rejected = []
        self.pending_grant = self.pending_go = self.decide_scheduled = False
        self.grant_C = None
        self.reqs = {}
        self.go_nonce = None
        self.state = "loading"

    def close_input(self, why):
        if self.state == "active":
            self.closed_ms = self.sim.t
            self.sim.ev("input_off", peer=self.name, why=why)
            self.sim.log("%s closes input (%s)" % (self.name, why))

    def cancel(self):
        if self.state in ("restarted", "idle"):
            return
        self.sim.log("%s cancels its join" % self.name)
        self.close_input("cancel")
        self.state = "restarted"
        self.sim.send(self.name, "H", 2, "JoinAbort", 8, gen=self.gen, attempt=self.attempt, reason=8)

    def on_arrive(self, src, ch, d):
        # A datagram in the transport buffer already shows its turn number, even one that is
        # still held behind a hole. The input decision uses it (B1).
        if ch == 0 and d.get("turn") is not None and d["kind"].startswith("Turn"):
            self.known = max(self.known, d["turn"])

    def on_msg(self, src, msg):
        kind = msg["kind"]
        if kind == "JoinGrant":
            if self.attempt is not None and msg["attempt"] <= self.attempt:
                return  # an old attempt's grant, or a duplicate
            self.reset_for(msg)
            return
        if kind == "SnapshotDone":
            self.snapshot(msg["gen"], msg["S"], msg["snap"], msg["load_ms"], msg["attempt"])
            return
        if kind == "Turn" and self.state in ("idle", "restarted"):
            # Turns on channel 0 can overtake the grant on channel 1: keep them for the next attempt.
            if msg["turn"] > self.R:
                self.stash[msg["turn"]] = msg["events"]
                self.check_backlog()
            return
        if kind == "JoinRestart":
            if self.attempt is not None and msg["attempt"] < self.attempt:
                return
            self.attempt = msg["attempt"]
            self.close_input("JoinRestart")
            self.state = "restarted"
            self.sim.log("%s restarts its join (reason %d)" % (self.name, msg["reason"]))
            return
        if self.state == "restarted":
            return
        if msg.get("attempt") is not None and msg["attempt"] != self.attempt:
            return
        if kind == "Turn":
            if self.state == "idle":
                return
            if msg["turn"] > self.R:
                self.turns[msg["turn"]] = msg["events"]
            while self.R + 1 in self.turns:
                self.R += 1
            self.check_backlog()
            self.kick()
        elif kind == "HashRoot":
            self.host_roots[msg["turn"]] = msg["root"]
            self.trim(self.host_roots)
        elif kind == "SeatActive":
            self.on_seat_active(msg)
        elif kind == "JoinCheckpoint":
            self.on_checkpoint(msg)
        elif kind == "InputGrant":
            self.on_input_grant(msg)
        elif kind == "InputGo":
            self.on_input_go(msg)

    def check_backlog(self):
        """The receive-backlog cap: received but not applied turns, plus the ones stashed before the
        grant. Past it the joiner asks to be restarted; the table keeps running."""
        if len(self.turns) + len(self.stash) > self.sim.cfg.receive_backlog_turns and self.state != "restarted":
            self.sim.log("%s: backlog %d passes the cap, aborts" % (self.name, len(self.turns) + len(self.stash)))
            self.close_input("backlog")
            self.state = "restarted"
            self.stash = {}
            self.sim.send(self.name, "H", 2, "JoinAbort", 8, gen=self.gen, attempt=self.attempt,
                          reason=REASON_BACKLOG)

    def trim(self, cache):
        """Keep the newest root_slots entries plus the pinned ones."""
        slots = self.sim.cfg.root_slots
        loose = [h for h in sorted(cache) if h not in self.pins]
        for h in loose[:-slots] if len(loose) > slots else []:
            del cache[h]
        self.max_root_slots = max(self.max_root_slots, len(cache))

    def snapshot(self, gen, S, snap, load_ms, attempt):
        if self.state != "loading" or attempt != self.attempt:
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
        self.sim.schedule(self.sim.t + self.sim.cfg.replay_step_ms, P_REPLAY, self.complete, self.attempt)

    def complete(self, attempt=None):
        if attempt is not None and attempt != self.attempt:
            return
        self.busy = False
        if self.state == "restarted":
            return
        k = self.P + 1
        events = self.turns.pop(k)
        was_owner = self.owners.get(self.seat)
        self.apply_events(events)
        self.P = k
        self.applied_log.append(k)
        self.log_applied(k)
        cfg = self.sim.cfg
        if k % cfg.hash_every == 0:
            self.roots[k] = root_of(k, self.owners)
            self.root_log[k] = self.roots[k]
            self.trim(self.roots)
            self.on_hash_turn(k)
        if not cfg.f1 and self.state == "replaying" and self.owners.get(self.seat) == "human" \
                and was_owner != "human":
            self.state = "owned"
            self.sim.log("%s replays its own seat transition at turn %d, input still off" % (
                self.name, k))
            self.send_progress()
        self.recheck()
        self.kick()

    def send_progress(self, hash_k=None):
        """A watermark: the cursors now. A hash turn it offers must still be in the root cache."""
        cfg = self.sim.cfg
        if self.sim.outstanding(self.name, "H", 0, "JoinWatermark") >= cfg.max_outstanding_wm:
            return
        if hash_k is not None:
            self.last_wm = max(self.last_wm, hash_k)
            if hash_k in self.roots and hash_k not in self.pins:
                self.pins.append(hash_k)
                while len(self.pins) > cfg.max_root_pins:
                    self.pins.pop(0)
        self.sim.send(self.name, "H", 0, "JoinWatermark", 22, gen=self.gen, attempt=self.attempt,
                      received=self.R, applied=self.P)

    def on_acked(self, dst, ch):
        if self.sim.cfg.f1 or self.state not in ("replaying", "owned") or not self.roots:
            return
        # A hash turn held back by the outstanding limit goes out now.
        newest = max(self.roots)
        if newest > self.last_wm:
            self.send_progress(newest)

    # ---- hash turns ----
    def on_hash_turn(self, k):
        if self.sim.cfg.f1:
            self.f1_hash_turn(k)
        elif self.state in ("replaying", "owned"):
            self.send_progress(k)

    def f1_hash_turn(self, k):
        cfg = self.sim.cfg
        if self.state != "replaying" or k not in self.host_roots:
            return
        if self.R - k > cfg.hash_every:  # F1: the exchange starts when applied is near received
            return
        self.sim.send(self.name, "H", 0, "JoinWatermark", 20, gen=self.gen,
                      attempt=self.attempt, received=self.R, applied=self.P)
        self.sim.send(self.name, "H", 0, "HashReport", 20, gen=self.gen,
                      attempt=self.attempt, turn=k, root=self.roots[k])

    def on_checkpoint(self, msg):
        h = msg["turn"]
        if msg["attempt"] != self.attempt or self.state not in ("replaying", "owned"):
            return
        if h not in self.roots:
            self.sim.log("%s has no stored root for %d, waits for the next watermark" % (self.name, h))
            return
        if self.roots[h] != msg["root"]:
            self.sim.log("%s: root mismatch at %d" % (self.name, h))
        if self.sim.outstanding(self.name, "H", 0, "HashReport") >= self.sim.cfg.max_outstanding_reports:
            self.sim.log("%s holds back the report for %d: %d outstanding" % (
                self.name, h, self.sim.cfg.max_outstanding_reports))
            return
        self.sim.send(self.name, "H", 0, "HashReport", 22, gen=self.gen, attempt=self.attempt,
                      turn=h, root=self.roots[h])
        self.H = h

    # ---- input (B1): grant -> request -> go, each decided at the end of the delivery batch ----
    def on_input_grant(self, msg):
        if msg["attempt"] != self.attempt or self.state != "owned":
            return
        self.pending_grant = True
        self.grant_C = msg["C"]
        self.schedule_decision()

    def on_input_go(self, msg):
        if msg["attempt"] != self.attempt or self.state != "owned" or msg["nonce"] not in self.reqs:
            return
        self.pending_go = True
        self.go_nonce = msg["nonce"]
        self.schedule_decision()

    def schedule_decision(self):
        if not self.decide_scheduled:
            self.decide_scheduled = True
            self.sim.schedule(self.sim.t, P_CHECK, self.decide, self.attempt)

    def fresh(self):
        """Local freshness: the highest turn seen in the transport buffer, delivered or not,
        is at most input_lag_max beyond the applied turn."""
        return self.known - self.P <= self.sim.cfg.input_lag_max

    def decide(self, attempt):
        self.decide_scheduled = False
        if attempt != self.attempt or self.state != "owned":
            self.pending_grant = self.pending_go = False
            return
        cfg = self.sim.cfg
        if self.pending_go:
            self.pending_go = False
            srtt = self.sim.srtt(self.name, "H")
            srtt = cfg.rtt_initial_ms if srtt is None else srtt
            took = self.sim.t - self.reqs[self.go_nonce]
            limit = 3 * srtt // 2 + cfg.turn_ms
            if self.fresh() and took <= limit and self.owners.get(self.seat) == "human":
                self.state = "active"
                self.active_ms = self.sim.t
                self.active_cursors = (self.P, self.R)
                self.sim.ev("input_on", peer=self.name, applied=self.P, received=self.R,
                            known=self.known)
                self.sim.log("%s INPUT ON: applied=%d received=%d known=%d (kept %d unapplied turns)" % (
                    self.name, self.P, self.R, self.known, len(self.turns)))
                self.sim.send(self.name, "H", 0, "InputOn", 6, gen=self.gen, attempt=self.attempt)
                return
            self.grants_rejected.append((self.sim.t, self.P, self.R, "go", took))
            self.sim.log("%s keeps input off at InputGo: applied=%d known=%d took=%d limit=%d" % (
                self.name, self.P, self.known, took, limit))
        if self.pending_grant:
            self.pending_grant = False
            if self.sim.outstanding(self.name, "H", 0, "InputReq") >= cfg.max_outstanding_req:
                self.sim.log("%s holds back an InputReq: %d outstanding" % (self.name, cfg.max_outstanding_req))
            elif self.fresh() and self.R >= self.grant_C and self.owners.get(self.seat) == "human":
                self.nonce += 1
                self.reqs[self.nonce] = self.sim.t
                self.sim.send(self.name, "H", 0, "InputReq", 16, gen=self.gen, attempt=self.attempt,
                              applied=self.P, nonce=self.nonce)
            else:
                self.grants_rejected.append((self.sim.t, self.P, self.R, self.grant_C))
                self.sim.log("%s keeps input off: applied=%d received=%d known=%d grant C=%d" % (
                    self.name, self.P, self.R, self.known, self.grant_C))

    # ---- F1 guards ----
    def on_seat_active(self, msg):
        self.A = msg["A"]
        self.H = msg["H"]
        if self.P >= self.A:
            self.sim.log("%s: applied %d >= A %d at SeatActive, restarts" % (self.name, self.P, self.A))
            self.state = "restarted"
            return
        self.recheck()

    def recheck(self):
        if not self.sim.cfg.f1 or self.state != "replaying" or self.A is None:
            return
        if self.A - 1 <= self.P < self.A:
            self.state = "active"
            self.active_ms = self.sim.t
            self.active_cursors = (self.P, self.R)
            self.sim.ev("input_on", peer=self.name, applied=self.P, received=self.R, known=self.R)
            unapplied = sorted(self.turns)
            self.freed = unapplied
            self.turns.clear()
            self.sim.log("%s ACTIVE: applied=%d received=%d, freed %d unapplied turns" % (
                self.name, self.P, self.R, len(unapplied)))

    def abandoned(self, dst, ch):
        self.sim.log("%s: pair to %s abandoned" % (self.name, dst))
        if self.state != "restarted":
            self.close_input("abandoned")
            self.state = "restarted"
            self.sim.send(self.name, "H", 2, "JoinAbort", 8, gen=self.gen, attempt=self.attempt, reason=4)


# ---- independent validators ---------------------------------------------------------------
def validate(events, cfg, end_t, oracle_lag=None):
    """Re-derive the transport and protocol checks from the raw event log.

    Uses only `events` and `cfg`: no `Chan` cursor, no peer object. Returns a list of
    (check, detail) pairs; an empty list is a pass. `oracle_lag` is the bound for check (v).
    """
    if oracle_lag is None:
        # The host's own bound (input_host_lag_max plus one round trip of turns) and two turns
        # for the InputGo's trip back.
        oracle_lag = cfg.input_host_lag_max + -(-2 * cfg.one_way_ms // cfg.turn_ms) + 2
    out = []
    out += _check_order(events)
    out += _check_window(events, cfg)
    out += _check_agreement(events)
    out += _check_seated_lag(events, cfg, end_t)
    out += _check_input_lag(events, oracle_lag)
    out += _check_seats(events)
    out += _check_retained(events, end_t)
    out += _check_tail(events, cfg)
    out += _check_epochs(events)
    out += _check_grants(events)
    out += _check_outage_bounded(events, cfg, end_t)
    out += _check_generations(events, cfg)
    out += _check_input_drop(events)
    out += _check_reassembly(events, cfg)
    return out


def _check_order(events):
    """(i) Channel-0 deliveries follow the contiguous arrived prefix, in order, once each."""
    out = []
    arrived, prefix, last = {}, {}, {}
    for e in events:
        if e.get("ch") != 0:
            continue
        pair = (e.get("src"), e.get("dst"), e.get("epoch"))
        if e["kind"] == "arrive":
            seen = arrived.setdefault(pair, set())
            seen.add(e["seq"])
            n = prefix.get(pair, 0)
            while n in seen:
                n += 1
            prefix[pair] = n
        elif e["kind"] == "deliver":
            # Every sequence up to and including this one must have arrived: prefix > seq.
            if prefix.get(pair, 0) <= e["seq"]:
                seen = arrived.get(pair, set())
                missing = [s for s in range(e["seq"] + 1) if s not in seen]
                out.append(("i", "t=%d %s>%s delivered seq %d past hole %r" % (
                    e["t"], pair[0], pair[1], e["seq"], missing[:3])))
            if e["seq"] <= last.get(pair, -1):
                out.append(("i", "t=%d %s>%s delivered seq %d again or out of order" % (
                    e["t"], pair[0], pair[1], e["seq"])))
            last[pair] = max(last.get(pair, -1), e["seq"])
    return out


def _check_window(events, cfg):
    """(ii) The span from the oldest unacknowledged sequence to a new one, from send records."""
    out = []
    unacked = {}
    for e in events:
        key = (e.get("src"), e.get("dst"), e.get("ch"))
        if e["kind"] == "send" and e["ch"] != 2 and e["attempt"] == 1:
            u = unacked.setdefault(key, set())
            u.add(e["seq"])
            span = e["seq"] - min(u) + 1
            if span > cfg.window:
                out.append(("ii", "t=%d %s>%s ch%d seq %d spans %d > %d" % (
                    e["t"], key[0], key[1], key[2], e["seq"], span, cfg.window)))
        elif e["kind"] == "ack":
            unacked.get(key, set()).difference_update(e["seqs"])
    return out


def _check_agreement(events):
    """(iii) Every pair of peers that applied the same turn has the same ownership table and root."""
    out = []
    seen = {}
    for e in events:
        if e["kind"] != "apply":
            continue
        first = seen.setdefault(e["turn"], (e["peer"], e["owners"], e["root"]))
        if first[1:] != (e["owners"], e["root"]):
            out.append(("iii", "turn %d: %s has %r, %s has %r" % (
                e["turn"], first[0], first[1], e["peer"], e["owners"])))
    return out


def _live_peers(events):
    """Seated peers and the peers of attempts that were not restarted, derived chronologically from
    the log: a drop removes a peer, a later admit makes it live again as a new attempt."""
    seated = set()
    attempts = {}
    for e in events:
        k = e["kind"]
        if k == "seated":
            seated.add(e["peer"])
        elif k == "admit":
            attempts[e["name"]] = (e["attempt"], e["S"], "live")
            seated.discard(e["name"])
        elif k == "attempt_end" and e["name"] in attempts and attempts[e["name"]][0] == e["attempt"]:
            attempts[e["name"]] = (e["attempt"], attempts[e["name"]][1], e["how"])
        elif k == "drop_seated":
            seated.discard(e["peer"])
            attempts.pop(e["peer"], None)
    return seated, attempts


def _open_outages(events):
    """Chronological set of (peer, src, channel) outages open at the end. A recovery of one path
    does not close another; a drop, a new attempt or a new epoch closes every path of the peer."""
    open_ = set()
    for e in events:
        k = e["kind"]
        if k == "outage":
            open_.add((e["peer"], e["src"], e["ch"]))
        elif k == "outage_end":
            open_.discard((e["peer"], e["src"], e["ch"]))
        elif k == "drop_seated":
            open_ = {o for o in open_ if o[0] != e["peer"]}
        elif k == "admit":
            open_ = {o for o in open_ if o[0] != e["name"]}
        elif k == "epoch_bump":
            open_ = {o for o in open_ if o[0] not in (e["a"], e["b"])}
    return open_


def _check_seated_lag(events, cfg, end_t=None):
    """(iv) At the end of the run every live seated peer is within seated_lag_max of the host or has
    an outage open on the host-to-peer turn path (channel 0) at that moment. A recovered outage, an
    outage on another path or one before a drop excuses nothing. Peers whose attempt finished count
    as seated; dropped peers are not live until they are admitted again."""
    seated, attempts = _live_peers(events)
    seated = set(seated) | {n for n, (_, _, how) in attempts.items() if how == "done"}
    host = max((e["turn"] for e in events if e["kind"] == "produce"), default=0)
    applied = {}
    for e in events:
        if e["kind"] == "apply" and e["peer"] != "H":
            applied[e["peer"]] = max(applied.get(e["peer"], 0), e["turn"])
        elif e["kind"] == "admit":
            applied[e["name"]] = e["S"]  # a new attempt applies from its snapshot turn
    open_ = _open_outages(events)
    out = []
    for peer in sorted(seated):
        lag = host - applied.get(peer, 0)
        if lag > cfg.seated_lag_max and (peer, "H", 0) not in open_:
            out.append(("iv", "%s is %d turns behind the host (host %d) without an open outage on H>%s ch0" % (
                peer, lag, host, peer)))
    return out


def _check_input_lag(events, bound):
    """(v) At the instant input opens, and after every later event of that millisecond, the
    host's current turn minus the joiner's applied turn is at most `bound`."""
    out = []
    host_cur = 0
    applied = {}
    watching = {}  # peer -> millisecond in which input opened
    for e in events:
        for peer, t in list(watching.items()):
            if e["t"] != t:
                del watching[peer]
        if e["kind"] == "produce":
            host_cur = e["turn"]
        elif e["kind"] == "apply" and e["peer"] != "H":
            applied[e["peer"]] = e["turn"]
        elif e["kind"] == "input_on":
            watching[e["peer"]] = e["t"]
            applied[e["peer"]] = e["applied"]
        for peer in sorted(watching):
            lag = host_cur - applied.get(peer, 0)
            if lag > bound:
                out.append(("v", "t=%d %s: host %d - applied %d = %d > %d (after %s)" % (
                    e["t"], peer, host_cur, applied.get(peer, 0), lag, bound, e["kind"])))
    return out


def _check_seats(events):
    """(vi) A seat is held by at most one live attempt or seated human. A dropped peer's seat is free
    (it went to the AI); a dead attempt's seat is free."""
    out = []
    held = {}
    for e in events:
        k = e["kind"]
        if k == "seat_init":
            for seat, owner in e["seats"]:
                if owner == "human" and seat != 0:
                    held[seat] = ("seated", "?")
        elif k == "seated":
            held[e["seat"]] = ("seated", e["peer"])
        elif k == "admit":
            if e["seat"] in held:
                out.append(("vi", "t=%d seat %d admitted for %s while held by %s" % (
                    e["t"], e["seat"], e["name"], held[e["seat"]])))
            held[e["seat"]] = ("attempt", e["name"])
        elif k == "attempt_end":
            for seat, (kind, name) in list(held.items()):
                if kind == "attempt" and name == e["name"]:
                    if e["how"] == "dead":
                        del held[seat]
                    else:
                        held[seat] = ("seated", name)
        elif k == "drop_seated":
            for seat, (kind, name) in list(held.items()):
                if name == e["peer"]:
                    del held[seat]
    return out


def _check_retained(events, end_t=None):
    """(vii) Every logical turn a live peer still needs is delivered to it, wholly retained in the
    host log, or wholly held in flight: each of its fragments is either unacknowledged in the sender's
    window or already in the receiver's reassembly buffer, in the CURRENT incarnation. A fragment
    sent once is not ownership of the turn; an early fragment is not enough. Rebuilt from the raw
    events (`retain` with its fragment count, `release`, `produce`, `send`/`arrive` with fragment
    index and count, `ack`, `deliver`, `epoch_bump`, and the lifecycle events). Checked at every
    release and once more at the end of the observation."""
    out = []
    live = {}
    produced, nfrag, retained = [], {}, set()
    delivered = set()
    sender_held, recv_held, seq_map = {}, {}, {}

    def clear(peer):
        nonlocal delivered, sender_held, recv_held, seq_map
        delivered = {x for x in delivered if x[0] != peer}
        sender_held = {x: v for x, v in sender_held.items() if x[0] != peer}
        recv_held = {x: v for x, v in recv_held.items() if x[0] != peer}
        seq_map = {x: v for x, v in seq_map.items() if x[0] != peer}

    def safe(peer, t):
        if (peer, t) in delivered or t in retained:
            return True
        n = nfrag.get(t, 1)
        have = sender_held.get((peer, t), set()) | recv_held.get((peer, t), set())
        return all(i in have for i in range(n))

    for e in events:
        k = e["kind"]
        if k == "seated":
            live[e["peer"]] = 0
            clear(e["peer"])
        elif k == "admit":
            live[e["name"]] = e["S"]
            clear(e["name"])
        elif k == "attempt_end" and e["how"] == "dead":
            live.pop(e["name"], None)
            clear(e["name"])
        elif k == "drop_seated":
            live.pop(e["peer"], None)
            clear(e["peer"])
        elif k == "epoch_bump":
            clear(e["a"])
            clear(e["b"])
        elif k == "produce":
            produced.append(e["turn"])
        elif k == "retain":
            retained.add(e["turn"])
            nfrag[e["turn"]] = e.get("nfrag", 1)
        elif k == "send" and e["src"] == "H" and e["ch"] == 0 and str(e["msg"]).startswith("Turn") and e["attempt"] == 1:
            seq_map[(e["dst"], e["seq"])] = (e["turn"], e.get("frag", 0))
            sender_held.setdefault((e["dst"], e["turn"]), set()).add(e.get("frag", 0))
            nfrag.setdefault(e["turn"], e.get("nfrag", 1))
        elif k == "ack" and e["src"] == "H" and e["ch"] == 0:
            for q in e["seqs"]:
                tf = seq_map.get((e["dst"], q))
                if tf:
                    sender_held.get((e["dst"], tf[0]), set()).discard(tf[1])
        elif k == "arrive" and e["src"] == "H" and e["ch"] == 0 and str(e["msg"]).startswith("Turn") and not e.get("dup"):
            recv_held.setdefault((e["dst"], e["turn"]), set()).add(e.get("frag", 0))
        elif k == "reasm_release" and e["src"] == "H" and e["ch"] == 0 and e.get("turn") is not None:
            # the receiver gave these fragments up: delivered, or reset, pruned, overflowed
            recv_held.get((e["dst"], e["turn"]), set()).difference_update(e["frags"])
        elif k == "deliver" and e["msg"] == "Turn":
            delivered.add((e["dst"], e["turn"]))
            recv_held.pop((e["dst"], e["turn"]), None)
        elif k == "release":
            t = e["turn"]
            retained.discard(t)
            for peer, start in sorted(live.items()):
                if t > start and not safe(peer, t):
                    out.append(("vii", "t=%d turn %d released while live %s holds only part of it or none" % (
                        e["t"], t, peer)))
        elif k in ("overflow", "drop_dead") and e["msg"] == "Turn" and e["dst"] in live:
            out.append(("vii", "turn %s refused for live %s (%s)" % (e["turn"], e["dst"], k)))
        elif k == "abandon" and e["src"] == "H" and e["dst"] in live and e["dst"].startswith("P"):
            out.append(("vii", "t=%d the pair to seated %s was abandoned" % (e["t"], e["dst"])))
    for peer, start in sorted(live.items()):
        for t in produced:
            if t > start and not safe(peer, t):
                out.append(("vii", "turn %d is neither delivered to, wholly held for, nor retained for %s at the end" % (
                    t, peer)))
                break
    return out[:3]


def _check_reassembly(events, cfg):
    """(xiv)/(xv) A logical message of several fragments is delivered only with every fragment index
    held by the receiver's reassembly buffer, taken from acquire events and not from the last datagram;
    the parts it was built from were sent in the current incarnation; the buffer never passes its limit;
    nothing is carried across an epoch change, a drop or an attempt end without an explicit release."""
    out = []
    held = {}  # (src, dst, ch) -> {mid: set(frag)}
    sids = {}
    bumps = {}
    sent_at = {}
    for e in events:
        k = e["kind"]
        if k == "send" and e.get("sid") is not None:
            sent_at.setdefault(e["sid"], bumps.get(tuple(sorted((e["src"], e["dst"]))), 0))
        elif k == "reasm_acquire":
            key = (e["src"], e["dst"], e["ch"])
            held.setdefault(key, {}).setdefault(e["mid"], set()).add(e["frag"])
            sids[(key, e["mid"], e["frag"])] = e["sid"]
            limit = cfg.reassembly_fragments_max if e["ch"] == 0 else cfg.fragment_max
            if sum(len(v) for v in held[key].values()) > limit:
                out.append(("xv", "t=%d %s>%s ch%d holds more than %d fragments" % (e["t"], key[0], key[1], e["ch"], limit)))
        elif k == "reasm_release":
            key = (e["src"], e["dst"], e["ch"])
            held.get(key, {}).pop(e["mid"], None)
        elif k == "deliver" and e.get("nfrag", 1) > 1:
            idx = {f for f, _ in e["parts"]}
            if idx != set(range(e["nfrag"])):
                out.append(("xiv", "t=%d %s>%s delivered a %d-fragment message from fragments %r" % (
                    e["t"], e["src"], e["dst"], e["nfrag"], sorted(idx)[:6])))
            for _, sid in e["parts"]:
                now = bumps.get(tuple(sorted((e["src"], e["dst"]))), 0)
                if sid is not None and sent_at.get(sid, now) != now:
                    out.append(("xiv", "t=%d a part (sid %s) of a delivered message belongs to another incarnation" % (e["t"], sid)))
                    break
        elif k in ("epoch_bump", "drop_seated", "attempt_end"):
            who = {e["a"], e["b"]} if k == "epoch_bump" else {"H", e.get("peer") or e.get("name")}
            for key, msgs in held.items():
                if {key[0], key[1]} == who and any(msgs.values()) and (k != "attempt_end" or e["how"] == "dead"):
                    out.append(("xv", "t=%d %s leaves fragments held for %s without an explicit release" % (e["t"], k, sorted(who))))
                    msgs.clear()
        if k == "epoch_bump":
            kk = tuple(sorted((e["a"], e["b"])))
            bumps[kk] = bumps.get(kk, 0) + 1
    return out[:3]


def _check_generations(events, cfg):
    """(xiii) Generation identities are never reused, at most max_generations are live, an image is
    freed only when no live attempt owns it, and none outlives its last owner."""
    out = []
    seen, live, owners, gen_of = set(), set(), {}, {}
    for e in events:
        k = e["kind"]
        if k == "gen_new":
            if e["gen"] in seen:
                out.append(("xiii", "t=%d generation %d allocated twice" % (e["t"], e["gen"])))
            seen.add(e["gen"])
            live.add(e["gen"])
            if len(live) > cfg.max_generations:
                out.append(("xiii", "t=%d %d live generations, limit %d" % (e["t"], len(live), cfg.max_generations)))
        elif k == "admit":
            gen_of[e["name"]] = e["gen"]
            owners.setdefault(e["gen"], set()).add(e["name"])
        elif k == "attempt_end":
            g = gen_of.get(e["name"])
            if g is not None:
                owners.get(g, set()).discard(e["name"])
        elif k == "gen_free":
            if owners.get(e["gen"]):
                out.append(("xiii", "t=%d generation %d freed while owned by %r" % (
                    e["t"], e["gen"], sorted(owners[e["gen"]]))))
            live.discard(e["gen"])
    for g in sorted(live):
        if not owners.get(g):
            out.append(("xiii", "generation %d still held with no owner at the end" % g))
    return out[:3]


def _check_input_drop(events):
    """(xii) A peer that loses its seat has no input open afterwards."""
    out = []
    open_ = set()
    for e in events:
        if e["kind"] == "input_on":
            open_.add(e["peer"])
        elif e["kind"] == "input_off":
            open_.discard(e["peer"])
        elif e["kind"] == "drop_seated" and e["peer"] in open_:
            out.append(("xii", "t=%d %s keeps input open after its seat was dropped" % (e["t"], e["peer"])))
    return out


def _check_tail(events, cfg):
    """(x) The retained set, rebuilt from retain / release events, never passes tail_turns_max turns
    or tail_bytes bytes, and the span from the oldest live join's snapshot to a newly retained turn
    never passes tail_turns_max."""
    out = []
    held = {}
    live = {}
    for e in events:
        k = e["kind"]
        if k == "retain":
            held[e["turn"]] = e["bytes"]
            if len(held) > cfg.tail_turns_max or sum(held.values()) > cfg.tail_bytes:
                out.append(("x", "t=%d retained %d turns, %d bytes" % (e["t"], len(held), sum(held.values()))))
            if live:
                span = e["turn"] - min(live.values())
                if span > cfg.tail_turns_max:
                    out.append(("x", "t=%d turn %d retained under a pin of span %d" % (e["t"], e["turn"], span)))
        elif k == "release":
            held.pop(e["turn"], None)
        elif k == "admit":
            live[e["name"]] = e["S"]
        elif k == "attempt_end":
            live.pop(e["name"], None)
    return out[:3]


def _check_epochs(events):
    """(xi) A transport epoch is a number of `epoch_bump` events of the pair. Every send, arrive,
    deliver and ack that carries an epoch label carries the pair's current count, and no datagram
    is delivered or acknowledged in a later epoch than the one it was sent in (by sid)."""
    out = []
    bumps = {}
    sent_at = {}
    for e in events:
        k = e["kind"]
        if k == "epoch_bump":
            key = tuple(sorted((e["a"], e["b"])))
            bumps[key] = bumps.get(key, 0) + 1
        elif k in ("send", "arrive", "deliver", "ack") and e.get("epoch") is not None:
            key = tuple(sorted((e["src"], e["dst"])))
            now = bumps.get(key, 0)
            if e["epoch"] != now:
                out.append(("xi", "t=%d %s %s>%s carries epoch %s, the pair is at %d" % (
                    e["t"], k, e["src"], e["dst"], e["epoch"], now)))
            sid = e.get("sid")
            if k == "send" and sid is not None:
                sent_at.setdefault(sid, now)
            elif k in ("arrive", "deliver") and sid is not None and sent_at.get(sid, now) != now:
                out.append(("xi", "t=%d sid %s sent in epoch %d was accepted in epoch %d" % (
                    e["t"], sid, sent_at[sid], now)))
    return out[:3]


def _check_outage_bounded(events, cfg, end_t=None):
    """(ix) An outage is explicit and ends: an ACK on that path ends it, or the seat is dropped
    within grace_ms plus one backed-off resend. Tracked per peer and path (source, channel); the
    end of the observation is `end_t` (the last event time when omitted)."""
    out = []
    started = {}
    for e in events:
        k = e["kind"]
        if k == "outage":
            started.setdefault((e["peer"], e["src"], e["ch"]), e["t"])
        elif k == "outage_end":
            started.pop((e["peer"], e["src"], e["ch"]), None)
        elif k == "drop_seated":
            started = {x: v for x, v in started.items() if x[0] != e["peer"]}
        elif k == "admit":
            started = {x: v for x, v in started.items() if x[0] != e["name"]}
        elif k == "epoch_bump":
            started = {x: v for x, v in started.items() if x[0] not in (e["a"], e["b"])}
    end = end_t if end_t is not None else (events[-1]["t"] if events else 0)
    for (peer, src, ch), t0 in sorted(started.items()):
        if end - t0 > cfg.grace_ms + cfg.outage_resend_ms:
            out.append(("ix", "%s has been in an outage on %s ch%d since %d without end or drop" % (peer, src, ch, t0)))
    return out


def _check_grants(events):
    """(viii) An InputGrant names a turn that was already sent to that peer."""
    out = []
    sent = {}
    for e in events:
        if e["kind"] == "send" and e["src"] == "H" and e["msg"] == "Turn" and e["attempt"] == 1:
            sent.setdefault(e["dst"], set()).add(e["turn"])
        elif e["kind"] == "send" and e["msg"] == "InputGrant" and e["attempt"] == 1:
            if e["C"] not in sent.get(e["dst"], set()):
                out.append(("viii", "t=%d grant to %s names turn %s, not yet sent" % (
                    e["t"], e["dst"], e["C"])))
    return out


def validate_sim(sim, **kw):
    return validate(sim.events, sim.cfg, sim.t, **kw)


# ---- scenario helpers ----
def one_join(cfg, losses=(), delays=None, until=6000, snapshot_ms=None, seated=1):
    sim = Sim(cfg, losses, delays, seated=seated)
    sim.add_join("J1", 0, 2, snapshot_ms=snapshot_ms)
    sim.run(until)
    return sim


def first_seq(cfg, kind, direction, snapshot_ms=None):
    """Channel-0 sequence of the first `kind` on `direction` in a loss-free run."""
    sim = one_join(cfg, until=3000, snapshot_ms=snapshot_ms)
    for _, src, dst, ch, seq, k in sim.deliveries:
        if k == kind and "%s>%s" % (src, dst) == direction:
            return seq
    raise ValueError("no %s on %s" % (kind, direction))


def lose(kind, direction, attempts, seq=None):
    return {(kind, direction, seq, a) for a in attempts}


def summary(sim, name="J1"):
    jr = sim.host.joins[name]
    j = sim.joiners[name]
    return {"H": jr.H, "A": jr.A, "accept_ms": jr.accept_ms, "input_ms": j.active_ms,
            "applied_received": j.active_cursors, "restart": (jr.restart_ms, jr.restart_reason),
            "selected": [h for _, h in jr.selected]}


def row_normal(cfg):
    return one_join(cfg)


def row_lost_watermark(cfg):
    return one_join(cfg, lose("JoinWatermark", "J1>H", (1,)))


def row_lost_checkpoint(cfg):
    return one_join(cfg, lose("JoinCheckpoint", "H>J1", (1,)))


def row_report_blocks_later_watermark(cfg):
    return one_join(cfg, lose("HashReport", "J1>H", (1, 2, 3, 4)))


def row_deadline_collision(cfg, at_deadline):
    # The first report is delayed so it arrives at the deadline or one ms before. Its resends
    # are lost, so the delayed first copy is the only one that can arrive; later messages on the
    # ordered channel wait behind it. The report is first sent at 1460 and would arrive at 1610.
    # Its deadline is the root's production time plus two hash periods plus two round trips.
    seq = first_seq(cfg, "HashReport", "J1>H")
    losses = lose("HashReport", "J1>H", (2, 3, 4, 5, 6, 7, 8), seq=seq)
    deadline = cfg.deadline_for(10, 2 * cfg.one_way_ms)
    extra = deadline - 1610 - (0 if at_deadline else 1)
    return one_join(cfg, losses, delays={("HashReport", "J1>H", seq, 1): extra}, until=9000)


def row_post_h_hole(cfg):
    return one_join(cfg, lose("Turn", "H>J1", (1, 2, 3, 4), seq=12), until=9000)


def row_missing_turn_before_h(cfg):
    return one_join(cfg, lose("Turn", "H>J1", range(1, 9), seq=9), until=5000)


def row_all_control_lost(cfg):
    return one_join(replace(cfg, catchup_ms=8000), lose("JoinWatermark", "J1>H", range(1, 9)),
                    until=9000)


def row_endless02(cfg):
    return one_join(cfg, until=182000, snapshot_ms=160650)


def row_two_joins_one_generation(cfg):
    sim = Sim(cfg, seated=2)
    sim.add_join("J1", 0, 3, snapshot_ms=150)
    sim.add_join("J2", 200, 4, snapshot_ms=150)
    sim.run(6000)
    return sim


def row_two_joins_two_generations(cfg):
    sim = Sim(replace(cfg, catchup_ms=100000), seated=2)
    sim.add_join("J1", 0, 3, snapshot_ms=65000)
    sim.add_join("J2", 61000, 4, snapshot_ms=150)
    sim.run(95000)
    return sim


def row_expiry_and_restart(cfg):
    """J2 cancels before J1's seat transition and asks again as the same peer (attempt 2),
    with a snapshot before it."""
    sim = Sim(cfg, seated=2)
    sim.add_join("J1", 0, 3)
    sim.add_join("J2", 100, 4)
    sim.cancel("J2", 1000)
    sim.add_join("J2", 1200, 4)
    sim.run(6000)
    return sim


def row_burst_acks_blocked(cfg, periods=24):
    """Queue row (f): a 1216-byte burst every turn to one seated peer whose ACKs never arrive."""
    sim = Sim(replace(cfg, wait_cap=64), seated=1)
    sim.turn_extra = lambda k: 1216
    sim.blocked_acks.add("P1>H")
    sim.run(periods * cfg.turn_ms)
    return sim


def queue_caps(cfg, destinations=5, joiners=2):
    """Design bounds on bytes queued at the host, counting every datagram at its full wire size.

    The waiting queue holds datagram references. The turn bytes behind them are charged to the
    turn store, so counting them again here is a deliberate upper bound.
    """
    dgram = cfg.payload_max + EPOCH_BYTES + HEADER + IP_UDP
    ch0_pair = cfg.window * dgram
    wait_pair = cfg.wait_cap * dgram
    caps = {
        "datagram": dgram,
        "ch0_in_flight_per_pair": ch0_pair,
        "ch0_waiting_per_pair": wait_pair,
        "ch0_host": destinations * (ch0_pair + wait_pair),
        "ch1_host": joiners * ch0_pair,
        "ch2_host": destinations * (73 + 2 + 53 + 2),
    }
    caps["host_total"] = caps["ch0_host"] + caps["ch1_host"] + caps["ch2_host"]
    # One joiner's reverse queue: control messages only, each bounded by its own rule.
    wire = lambda payload: payload + WIRE
    # 4 watermarks, 2 reports, 2 InputReqs (16 B payload), InputOn (6 B), JoinAbort (8 B, channel 2).
    caps["joiner_reverse"] = (cfg.max_outstanding_wm * wire(22) + cfg.max_outstanding_reports * wire(22)
                              + cfg.max_outstanding_req * wire(16) + wire(6) + wire(8))
    # One ACK-only per channel per millisecond at most: channels 0 and 1 each 53 bytes x 1000 / s.
    caps["ack_kbit_per_s_per_channel"] = (HEADER + EPOCH_BYTES + IP_UDP) * 8 * 1000 / 1000.0
    caps["ack_kbit_per_s_ch0_and_ch1"] = 2 * caps["ack_kbit_per_s_per_channel"]
    return caps


ROWS = {
    "normal": row_normal,
    "lost_watermark": row_lost_watermark,
    "lost_checkpoint": row_lost_checkpoint,
    "report_blocks_later_watermark": row_report_blocks_later_watermark,
    "deadline_exact_ms": lambda cfg: row_deadline_collision(cfg, True),
    "deadline_minus_one_ms": lambda cfg: row_deadline_collision(cfg, False),
    "post_h_hole": row_post_h_hole,
    "missing_turn_before_h": row_missing_turn_before_h,
    "all_control_lost": row_all_control_lost,
    "endless02": row_endless02,
    "two_joins_one_generation": row_two_joins_one_generation,
    "two_joins_two_generations": row_two_joins_two_generations,
    "expiry_and_restart": row_expiry_and_restart,
}


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    rules = "F2"
    if "--rules" in argv:
        i = argv.index("--rules")
        rules = argv[i + 1]
        del argv[i:i + 2]
    show_trace = "--trace" in argv
    if show_trace:
        argv.remove("--trace")
    names = argv or list(ROWS)
    cfg = Cfg(rules=rules)
    if "queue" in names:
        names.remove("queue")
        sim = row_burst_acks_blocked(cfg)
        c = sim.chan("H", "P1", 0)
        print("queue: burst 1216 B every turn, ACKs blocked, 24 periods: turns=%d sequences_used=%d "
              "window_span=%d waiting=%d" % (sim.host.applied, c.next_seq, sim.max_span[("H", "P1", 0)],
                                             len(c.wait)))
        print("queue caps: %r" % (queue_caps(cfg),))
    for name in names:
        sim = ROWS[name](cfg)
        print("%-30s rules=%s" % (name, rules))
        for jname in sorted(sim.joiners):
            print("    %-4s %s" % (jname, summary(sim, jname)))
        print("    violations=%d root_conflicts=%d validators=%d" % (
            len(sim.violations), len(sim.root_conflicts()), len(validate_sim(sim))))
        if show_trace:
            print(sim.dump())
    return 0


if __name__ == "__main__":
    sys.exit(main())
