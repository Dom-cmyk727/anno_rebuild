# ADR 0003: Network transport and sessions

- **Status:** Proposed
- **Date:** 2026-10-26. Corrected 2026-10-27 (round bq F1): the fragment cap and the window time. Retention proposal 2026-10-27 (T43). Round bs F1, 2026-10-27: progress cursors, global caps, joiner backlog, acknowledgement loss. Round bt F2, 2026-10-27: the shared owner enforces tail and deadline caps on each host advance, before the new turn is retained. T45 measurement 2026-10-27: labeled `World::load` and `run_turn` costs, and encoded turn sizes. T47 F3, 2026-10-27: channel-0 arrivals during load and replay enter the backlog through `Table.note_backlog`. T52, 2026-10-27: observable join activation, hash-report delivery and a shared-link schedule ([§8.7](#87-ordered-join-protocol-ownership-events-and-queue-bounds-t52-f2)). T52 F1, 2026-10-27: the hash match is not the seat transition, the activation turn is chosen after the host has the report, and hash retention and the shared queues are bounded. The model and the defaults below are unchanged. T52 F2, 2026-10-27: the protocol is checked event by event in `tools/join_activation.py` with ordered channel-0 delivery; ownership events ride in the turn stream, input needs a fresh grant, and the queues are bounded with overflow rules. The F1 rows and the F1 "all MET" claims are superseded. Status stays Proposed.

ADR 0002 is reserved for the float walk (T34) and is not this document.

This is the design of the `net` library. It does not add code. The owner approves it before W3 step 2. Numbers are either taken from a decision or from the original, or they are the measurements in [§13](#13-budgets). Nothing here is a guess stated as a measurement.

## 1. Context and the owner's requirements

The game is lockstep. Every peer runs the same deterministic `World`. Only commands travel, plus a snapshot when someone must catch up (`docs/plan/lan-endless.md` §4).

The owner required (R14, D3, D9, D10, `docs/plan/lan-endless.md` §4 and §7):

- our own UDP layer, stability and performance first, flexibility so internet play can be added later;
- joining a game that is already running;
- the host leaving, and another player becoming host;
- a short drop must not end the game for everyone;
- a hash check that stays cheap over a long session;
- a dropped human seat goes to the AI after 60 s and comes back on rejoin (D9);
- in multiplayer the host sets the speed, and 400 % is refused (D10).

The original (`docs/re/tick-loop.md` §4–§5) is the model being replaced, not the byte format. It sends a turn every 100 ms, keeps turns for resend, keep-alives every 1 s in game and drops a client after four missed ones, and checks an MD5 of a full save every 60 s. There is no fixed command lead, no join-in-progress, and a desync reloads the last agreed snapshot or ends the game. Our wire format is the sim's (`docs/sim/state.md` §8.5), which is explicitly not byte-compatible with the original.

## 2. Decision

**Own UDP.** Session rules, turns and hashes live in `net`. They do not depend on a third-party reliability library.

Rejected:

- **ENet.** It is a reliable UDP library. It would still leave join, host migration, the turn log and the hash check to us, which is the bulk of W3. D3 already chose our own layer, and the plan records that the co-developer's own UDP protocol had already been run through joins, migration and drops. Taking ENet would add a dependency for the part we are not short of.
- **GameNetworkingSockets.** It is a large session stack aimed at internet play and relay. v1 is LAN. The same objection as ENet: the library does not know our turn, our snapshot or our hash tree, so the hard code remains, and the dependency arrives before the internet transport exists.

The transport is an interface ([§11](#11-the-transport-interface)). LAN UDP is the first implementation.

## 3. The packet format

Every datagram is explicit little-endian fields. No struct cast, no padding. A short datagram is refused before any later field is read.

Bytes 0..3 are `41 33 4E 54` (`A3NT`). The following fields:

| offset | type | name |
|---:|---|---|
| 4 | `u16` | `protocol_version`, 1 for this ADR |
| 6 | `u32` | `session_id`, chosen by the host when the session is created |
| 10 | `u8` | `channel` ([§4](#4-the-channels)) |
| 11 | `u32` | `sequence`, per sender, per channel, starting at 0 |
| 15 | `u32` | `ack`, the highest sequence the sender has received on this channel |
| 19 | `u32` | `ack_bits`, bit `i` set means sequence `ack - (i+1)` was also received |
| 23 | `u16` | `payload_len` |
| 25 | `u16` | `epoch`, the sender's incarnation of this endpoint pair (T52 F6, below) |
| 27 | bytes | `payload`, exactly `payload_len` bytes, and no trailing byte |

`ack` and `ack_bits` together name 33 sequences: `ack` itself and the 32 before it. That is the selective acknowledgement in [§4](#4-the-channels). The highest contiguous sequence stays on the peer. An ordered channel delivers payload only through the first missing sequence. Putting that contiguous value in `ack` would leave a packet received ahead of a hole unnamed, so the sender would not see the loss until the hole was already gone.

The header is **27 bytes** (25 plus the `u16 epoch`; T52 F6 added the field, and every figure of §4, §8.7 and the T41/T45 budgets that was computed with a 25-byte header gains 2 bytes per datagram: a full datagram is 1 255 bytes on the wire instead of 1 253, an empty turn 71 instead of 69, an ACK-only 55 instead of 53, at most 0.16 % of a full datagram). `payload_len` above the configured maximum is refused before the payload is copied. The maximum is `net.payload_max`, default 1200. Reason: an Ethernet frame of 1500 bytes minus a 20-byte IPv4 header and an 8-byte UDP header leaves 1472, and 1200 stays under that with room for options, so the LAN path does not depend on IP fragmentation. It is configuration because a later internet path may need a smaller value.

**The epoch (incarnation).** Each endpoint holds one `u16` epoch per peer, 0 at the start. It stamps that value into every data datagram and every ACK-only datagram it sends to that peer, and it drops, before any sequence, `ack` or `ack_bits` is read, a datagram from that peer whose `epoch` differs from its own. Sequence spaces of all channels in both directions restart at 0 whenever the epoch changes, so sequence 0 of two incarnations is told apart only by this field. The epoch changes only through the join ask: a peer that asks again after having been in the session sends `JoinAsk` (`epoch` u16, `seat` u8, `ask id` u16: 5 bytes, 60 on the wire) carrying the previous epoch plus one modulo 65 536; both endpoints adopt it, retire every channel's sender window, waiting items, cursors and receive state for that peer, and answer only to the new value. Timers (resend, ACK flush) are local to an endpoint and carry the epoch they were created in; they do nothing after a change. The epoch wraps after 65 536 asks; reuse is safe only if no datagram of a 65 536-ask-old incarnation can still be in flight, which needs the maximum datagram lifetime to be shorter than 65 536 asks of one peer (asks are at least `resend_ms` apart; `max_attempts × resend_ms` bounds a datagram's life at the sender). **Labeled boundary:** the ask itself is assumed delivered (its loss, retry, and the first datagrams racing it, are not modelled), as are jitter and link serialization (OQ-320, OQ-371).

`protocol_version` other than 1 is refused at the first packet. `session_id` other than the one from the handshake is dropped.

## 4. The channels

| id | reliability | what travels |
|---:|---|---|
| 0 | reliable, ordered | turns, client commands, handshake messages after discovery |
| 1 | reliable, ordered, own sequence | snapshot fragments |
| 2 | unreliable | keep-alive, ping, hash report |

Channel 1 is separate from channel 0 so a snapshot cannot sit in front of a turn. The plan's "without stopping the others" on a join (`docs/plan/lan-endless.md` §4) is the reason. One ordered stream for both would stall the players who are already in the game.

**Selective acknowledgement.** `ack` is the highest sequence received on that channel, and `ack_bits` names the 32 sequences before it ([§3](#3-the-packet-format)). A clear bit in that range is a hole and is resent. A sequence that is still unacked at or below `ack - 32` is resent too, because the field can no longer name it. The sender does not transmit a new sequence more than 32 ahead of its oldest unacked sequence. `snapshot_window` limits how many fragments are in flight. It does not widen the bitfield. A sender that raises the window and slides it across a hole delivers a short snapshot; the model records that as a failed transfer (`tools/snapshot_catchup.py`), and the join is restarted.

The resend wait is `net.resend_ms`, default 50. Reason: the turn period is 100 ms, and a resend at 50 ms still lands inside the next period on a LAN whose round trip is under 50 ms. It is configuration because a longer round trip needs a longer wait. The acceptance case in [§14](#14-the-netsim-test-plan) is 150 ms one way, a 300 ms round trip. There the model sets `resend_ms` to 300, so the acknowledgement can return before a clean window is repeated. The same model with `resend_ms` 50 on that path repeats the window. The LAN default in [§10](#10-drops-and-outages) stays 50.

**Keep-alive.** On channel 2, each peer sends one every `net.keepalive_ms`, default 1000. Reason: the original's in-game keep-alive is 1000 ms (`tick-loop.md` §5). A peer that has sent any packet in the interval skips the extra one.

**Fragmentation.** A payload larger than `payload_max` is not one datagram. The sender splits it into pieces of at most `payload_max` minus a 12-byte fragment head (`u32 id`, `u32 index`, `u32 count`), on the channel that owns the message. Each fragment therefore carries at most 1200 - 12 = 1188 save bytes at the default `payload_max`. The receiver refuses `count` above `net.fragment_max` before allocating. The cap is what stops a hostile `count` from allocating the reassembled save.

The previous default, 4096, permits 4096 × 1188 = 4 866 048 save bytes. The three measured saves are larger. [§13](#13-budgets) needs 9 659 fragments for `S01_07_01` (11 474 169 bytes), 17 130 for `endless02` (20 349 454 bytes) and 10 865 for `03_1024_med_256` (12 906 633 bytes). None of those snapshots is transferable at 4096. The old reason, "4096 times 1200 bytes is about 4.9 MB, above any save", counted the fragment head as save bytes and compared the product with saves this section had not measured.

Proposed default: 17 130. It is the smallest count that admits the largest measured save, `ceil(20 349 454 / 1188)`. The allocation that count allows is 17 130 × 1188 = 20 350 440 bytes, the `endless02` save rounded up to whole fragments. A hostile `count` can force that allocation and no larger one. There is no spare fragment: a bigger save is refused until the config is raised. This default is part of the proposal. The pieces are delivered upward only when every index is present.

**Snapshot flow control.** At most `net.snapshot_window` fragments of channel 1 are in flight, default 32. Those 32 fragments carry 32 × 1188 = 38 016 save bytes. Their datagram payloads are 32 × 1200 = 38 400 bytes, and that payload total is the bound on the host's outstanding channel-1 buffer. The 384 bytes of fragment heads are not save bytes. Turns on channel 0 are not part of that window. When the window is full the host waits for acks. It does not drop turns to make room. The time that window takes is [§13](#13-budgets), not this paragraph. The window is the flight limit. The turn log is the pin in [§8](#8-join-in-progress). A larger window does not remove that pin, and it does not let the sender run more than 32 sequences ahead of a hole.

## 5. The session states

`Discovery`, `Handshake`, `Lobby`, `Game`, `Pause`.

**Discovery.** The host listens on `net.port` and also sends a UDP broadcast on that port every `net.discover_ms`, default 1000, carrying the session id, the player count and the host's name. A client may instead be given an IP and the same port. The port number is **not chosen here** (OQ-250): `tick-loop.md` does not record the original's listen port, and a made-up default would be a guess. Until it is set, the host refuses to open a socket.

**Handshake.** Before tick 0, both sides compare, and a mismatch refuses the peer with a named reason (`docs/plan/lan-endless.md` §4):

- content hash of `Anlagen.dat`, `FigurenHeader.dat`, the island templates and the walk tables;
- build version and `kStateHashVersion`;
- `kCommandWireVersion` (`command.hpp`, currently 1);
- the rules-preset digest;
- the generator version, its parameters and the seed;
- `GameSetup`: seats, colours, world options (W1; the type is T36's, and this ADR names the fields the plan already lists).

Then every peer builds the world and exchanges `World::state_hash()` before tick 0. A difference refuses the session. No turn is sent until the hashes match.

**Lobby.** Seats are assigned. The host is a playing peer, not a dedicated server. Starting the game moves every peer to `Game` together, on a turn the host numbers.

**Game.** Turns run ([§6](#6-turns)).

**Pause.** The host sets speed 0 through `SetSpeedCmd`, which the sim already restricts to the host (`docs/sim/state.md` §8.2). Pause is used for a hash mismatch, for the host hand-over, and when the wait in [§10](#10-drops-and-outages) has expired and people are still missing. Leaving pause is a host `SetSpeed` back to the previous percent. 400 % is refused in multiplayer (D10); the sim's `NotHost` and `BadSpeed` results are the refusals, identical on every peer.

## 6. Turns

The period is `net.turn_ms`, default 100. Reason: the original emits one turn per 100 ms (`tick-loop.md` §5, `0x100555a0(100)`), and the plan keeps that default.

A command is not applied in the turn that is already running. The sender gives it a `u32` sequence number per seat. The host stamps the issuer (the connection's seat; the client does not choose the issuer, `state.md` §8.2) and assigns the target turn `current + net.command_delay`. The default delay is 2. Reason: the plan's default (`docs/plan/lan-endless.md` §4), so a command lands on a turn every peer still has ahead of it. The original has no fixed lead (`tick-loop.md` §5); the delay is the plan's change, and it stays configuration. A sequence the host has already accepted is ignored, which is what makes a resend after migration harmless.

One turn on channel 0, after the packet header:

| field | type |
|---|---|
| turn index | `u64` |
| `step_ms` | `u32` |
| command count | `u32` |
| commands | that many `TurnEntry` values |

`TurnEntry` is the host form in `state.md` §8.5: `u32 issuer`, `u32 seq`, then `u16 type`, `u16 len`, `payload[len]`. The decoder is the sim's `decode_turn`: a length past the remaining bytes, or a count that would allocate before the bytes exist, is a refusal, and the turn is not applied. The same refusal on every peer is a session error, not a per-peer skip, because a different skip would desync. The host must not emit a turn the decoder refuses; a peer that nonetheless sees one pauses and reports it.

Applying a turn is `run_turn` (`replay.hpp`): the commands in order, then `Timing::mp_steps_per_turn` advances (10, `clock.hpp`) of `step_ms`. `step_ms` comes from `multiplayer_step_ms` of the current speed (`state.md` §8.6). Single-player uses `run_frame` through the same apply path, so the two modes do not grow different rules.

The host sends the turn to every other seat. A seat with no commands still gets the turn, so the index stays contiguous. Empty turns are how a peer knows the host is alive between clicks.

## 7. Hash checks

Two checks, both of `World::state_hash()`, which is the root of the hierarchical hash (`hash_tree.hpp`, `state.md` §9.4). The hot path does not build a tree.

**Often.** Every `net.hash_every` turns, default 10. Reason: the plan's default, one second at the 100 ms period, against the original's 60 s MD5 (`tick-loop.md` §5). `state.md` §9.4 says peers exchange the root every turn. The plan narrows that to every K turns. This ADR follows the plan; the every-turn sentence in the spec stays until the owner accepts this document. Each peer sends on channel 2 the turn index (`u64`) and the root (`u64`). The host compares. A lost report is asked for again on channel 0 after `net.hash_retry_ms`, default 500. Reason: the original's server asks for a missing digest every 500 ms (`0x10057470`). If it is still missing after one check interval, the host pauses. A mismatch also pauses.

**The full check.** Every `net.full_check_min` minutes of real time, default 10, the host writes `World::save()` and each peer checks that its own `save()` bytes match the hash it already trusts, and exchanges the root again. Reason for 10: the original's single-player autosave interval is 10 minutes of real time (`tick-loop.md` fixture F9). Writing the save stores the snapshot. It is not a broadcast, and this check does not send those bytes. A send is the mismatch path below, or a join ([§8](#8-join-in-progress)). The stored image can be 10 minutes old. The rolling turn log holds 60 s ([§10](#10-drops-and-outages)). The mismatch path therefore writes a new image at the pause, and a join writes a new image at the ask. Neither sends the periodic image.

**On a mismatch.**

1. The host pauses.
2. The peers that differ record a `HashTree`, starting at depth 0 (the sections). They exchange only the nodes that differ and walk down those branches, up to depth 4 (buildings, the depth `state.md` §9.4 names). `diff_trees` names the path. A matching branch is not sent.
3. The host writes a fresh `World::save()` after the pause, at the last applied turn `S`, before turn `S+1`. That image is what channel 1 sends. Every peer `World::load`s it. `load` already refuses a bad version, a bad checksum and a hostile length before it allocates (`state.md` §9.5).
4. The replay tail is empty: the pause stopped new turns before the save. The host resumes at turn `S+1`. Peers who load that image are on the same boundary, and the next hash is taken after that resume.

The cost of step 2's tree is paid only on a mismatch, not every check.

## 8. Join-in-progress

A new player asks the host, naming a free seat or a seat the AI is holding. The host does the handshake of [§5](#5-the-session-states) against the running session's setup. The joiner does not change it. The peers already in the game keep playing. Pause remains the mismatch path ([§7](#7-hash-checks)) and the host hand-over ([§9](#9-host-migration)).

### 8.1 Snapshot tick and retained turns

The host calls `World::save()` only at a turn boundary: `run_turn(S)` has returned, and `run_turn(S+1)` has not started. The snapshot's turn index is `S`. Its tick is `WorldClock::tick()` inside that save (`clock.hpp`: one tick is 2 game ms). At 100% a turn is 10 steps of 10 game ms, so the clock advances 50 ticks a turn while the speed stays there. A speed change breaks that multiple. Peers do not recompute the tick from `S`. The save already holds the clock. Retention uses `S`.

The first retained turn is `S+1`. Turns at or before `S` are inside the snapshot and are not replayed. The save does not choose a last index. While a pin is held, the host retains every index `k` with `S+1 <= k <= H`, where `H` is the latest turn it has applied. The fluid comparison calls the joiner caught up when it has applied that same `H`. [§8.6](#86-causal-readiness) is the readiness contract. When the one-way delay is at least one turn, `H` is still in flight at the moment the host applies it, so equality with `H` is not that boundary. The host still retains every index through `H` while the join runs. That index is the last retained turn. Turn `H+1` is ordinary live play.

The rolling log is a different range. It holds `turn_buffer` indexes ending at `H`, from `H - turn_buffer + 1` through `H`. Turn `S+1` leaves that range when `H >= S + turn_buffer + 1`. At the defaults that is 60.1 s after the save, 601 turns. A live pin keeps `S+1` anyway. Freeing a pinned index is not how the host makes room.

### 8.2 Fresh image and pin, and the rolling log

Both rules below were scored against the same requirement: the peers already playing are not paused. The save sizes are the three T41 measurements ([§13](#13-budgets)). The acceptance round trip is 300 ms.

| Rule | What the joiner receives | `endless02` at a 300 ms round trip | When the budget trips |
|---|---|---|---|
| Fresh save at the ask, and a pin of the turns after `S` | The bytes of `S`, then every turn `S+1` through `H` | Admitted. The zero-loss discrete completion is 160 650 ms, 1 606 turns before the chase, which is past the 600-turn log. | This joiner is refused or restarted. The pin is released. The table is not paused. |
| Rolling log only (`turn_buffer` 600) | The turns after `S` that are still inside the last 60 s | Refused. The transfer outlasts the log. A load that skips the missing turns is a failure, not a caught-up peer. | The same refusal. The log is the whole memory for this rule. |

The periodic save ([§7](#7-hash-checks)) was the third image. It is up to 10 minutes old, 6 000 turns at a 100 ms period when the host does not pause. The rolling log has already dropped 5 400 of them. Asking about that image returns `unavailable`. That is an availability check: `transfer_ms` is 0 and the status is not `success`. A save that is still inside the log at the ask is `available` on the same terms. `endless02`, 20 349 454 bytes and 599 turns old, is `available` and is not a join. The rolling join then counts the transfer, the load and the turns the host produces while those run. That image's zero-loss transfer is 160 650 ms, the log has dropped 1 605 of the needed turns by load, and the join is `refuse` / `gap_not_retained`. The full check still writes the save, and it still does not broadcast it.

Pausing the table for the whole transfer would hold `H` still, and a 60 s log would then cover a tail that is not growing. That pause is the thing the plan rules out for a join. This proposal does not offer it. Mismatch resync may pause, and the image it sends is taken after that pause, so its tail is empty.

The proposed join rule is the fresh save plus the pin. The rolling log stays the short-outage path: a cut of 0.5–5 s, and a peer who already has the world and whose missed turns are still in the log. A new joiner, and a rejoin whose gap is longer than the log, use the pin.

### 8.3 Budgets

The numbers are configuration. The defaults are the proposal in [§13](#13-budgets). The model states each one as an assumption. They are not measured transfer times.

| Name | Proposed default | What trips a refusal or a restart |
|---|---:|---|
| `fragment_max` | 17 130 | The save needs more fragments than the measured largest save |
| `snapshot_window` | 32 | Flight limit on channel 1. The ack field stays 32 bits |
| `snapshot_age_ms` | 60 000 | A later ask does not attach to this image |
| `concurrent_transfers` | 2 | A third joiner is refused |
| `tail_turns_max` | 4 096 | The pin would grow past this many turns |
| `turn_store_cap` | 1 200 | Bytes charged for one retained turn, the payload cap. T45 did not replace it (OQ-282) |
| `catchup_ms` | 600 000 | The joiner has not reached the host |
| `load_ms` | 0 | Default. Time added after the bytes arrive, before replay. T45 measured 27–47 ms on one host and left the default (OQ-281) |
| `replay_turns_per_s` | 100 | Default chase rate. The live rate is 10. T45 measured faster turns on one host and left the default (OQ-281) |
| `loss_scale` | 2 | Admission multiplies the zero-loss discrete completion by this |

Admission takes the zero-loss discrete completion, multiplies by `loss_scale`, adds `load_ms`, and calls that duration's turn count `gap`. When `replay_turns_per_s > live_turns_per_s`, the reserved pin length is `J = gap + ceil(live * gap / (replay - live))`. When the replay rate is at or below the live rate, admission refuses: the gap does not close. `J` above `tail_turns_max`, a `gap` above `receive_backlog_turns`, or an estimate above `catchup_ms`, is also a refusal. `J` is an upper bound held for admission. It is not the turn the joiner has applied.

The fluid model keeps three cursors: turns the host has produced, the contiguous turns the joiner has received, and the turns the joiner has replayed. On that clock a success is the first time the replayed cursor equals the host cursor, the received cursor covers that prefix, and the replayed cursor has not passed the host. One fragment is 1 188 save bytes. Its reserved bound is 2 turns, which the old clock placed at 170 ms. The cursors meet at 160 ms on turn 1. At 170 ms the host has produced one turn, because `170 // 100` is 1. Counterexample tokens: counterexample_1188_done_ms=160, counterexample_1188_replayed=1. That meeting is assumption A8. It is not the readiness contract. Turn 1 is produced at 100 ms and, with a 150 ms one-way delay, cannot arrive before 250 ms. [§8.6](#86-causal-readiness) names the boundary that can actually be reached. The 160 ms row stays as the optimistic comparison.

A joiner who passed admission and then crosses a cap is restarted. The partial reassembly and that joiner's backlog are discarded. The next ask takes a new `S`. The restarted join does not apply a prefix of the turns and report success. `paused` is false on every join outcome here.

The model applies that rule in `Table.advance`, before the new host turn is part of the retained span. A joiner's deadline is that joiner's own start time plus `catchup_ms`. At that millisecond a joiner who is still running is restarted; one millisecond earlier is not. `complete` before the deadline leaves the joiner complete. When the union of open pins would pass `tail_turns_max` or `tail_bytes_max`, the owner restarts every running joiner on the oldest generation, then the next oldest, until the remaining union fits. Order is snapshot turn, then the time the generation was saved, then the order the generations were opened. Joiners on one generation share its pin and restart together. A younger generation whose own pin still fits is kept, including a turn the rolling log has already dropped. Opening a generation, and the terminal result of `evaluate_join`, do not by themselves keep this intermediate span inside the cap. Asking what a later host turn would retain is not an advance.

The byte bound is the sum of every cap in [§13](#13-budgets): the snapshot store, reassembly, the channel-1 windows, the channel-0 flight window, the host pin, each joiner's received-turn backlog, and the rolling log.

### 8.4 Who owns the bytes

A generation is the immutable save at `S` and the pin of turns after it. The host stores the save. Each joiner has a reassembly buffer of at most `fragment_max × 1188` bytes. The host's outstanding channel-1 buffer is `snapshot_window × payload_max` bytes per transfer.

Concurrency, the number of overlapping generations and the retained-turn span are global. A generation does not keep a private copy of `concurrent_transfers`. A second joiner who asks while `t - t_save <= snapshot_age_ms`, and while the table still has a free transfer slot, shares that generation. The pin is the union of the turns those joiners still need. An image older than `snapshot_age_ms` is not shared; the running join on it is not evicted by age. The next ask opens another generation when `max_generations` and the tail span allow it. Three joiners on two generations, with a cap of two, refuse the third even though each generation holds only one joiner. An ask that finds the global slots taken is refused. The joins already running stay as they are. A new generation whose span would pass `tail_turns_max` is refused, and turns already pinned stay.

Cancel or disconnect frees that joiner's reassembly buffer and backlog and drops that joiner's reference. The pin remains while any join that still needs the turn is running, including a turn the rolling log has already left, so one cancel does not evict the other joiner's turns. A released slot can be taken by a later ask. When the last joiner who needs a turn finishes or leaves, that turn follows the rolling rule again. Completion means the host has accepted a report for the selected checkpoint `H` compared at that same turn, the ownership event for the joiner's seat is in turn `A` (a turn still unsent at that acceptance), the joiner has replayed turn `A`, and the joiner has opened input after the `InputGrant` / `InputReq` / `InputGo` exchange, so the host holds its `InputOn` ([§8.7](#87-ordered-join-protocol-ownership-events-and-queue-bounds-t52-f2)). Ownership at `A` and input are separate: every peer replays `A`, and the joiner's input stays off while the highest turn in its transport buffer, delivered or not, is more than `input_lag_max` ahead of its applied cursor. Received and applied keep moving during the flights. The receivable-prefix gate in [§8.6](#86-causal-readiness) is the model comparison, not this completion. The joiner then receives channel 0 with the rest of the table. The fluid equality in [§8.3](#83-budgets) is the optimistic comparison, not this completion. The reassembly buffer is freed at completion; the turns received but not yet applied are not. They stay in the joiner's live buffer and are applied in order exactly once, and the host's pin for that joiner ends when `InputOn` arrives, or on restart. Turns received on channel 0 during the transfer sit in that backlog until replay, and [§13](#13-budgets) charges them.

On host migration ([§9](#9-host-migration)) an in-progress joiner is not a connected seat. The successor is not handed this generation's save. Each running join restarts on the new host, which writes its own fresh image. The §9 pause is the role change the plan already specifies. The restart does not add a pause. Peers who already have the world keep it; they use the rolling log and the resend §9 already describes.

### 8.5 Procedure

1. Admit or refuse under §8.3. A refusal ends at the ask.
2. Share an open generation when §8.4 allows it. Otherwise save at the current boundary `S` and open the pin at `S+1`.
3. Send the image on channel 1. Send turns `S+1` onward on channel 0 during that transfer. A full channel-1 window does not delay channel 0.
4. The joiner `load`s the image, then `run_turn`s from `S+1`. `load_ms` and `replay_turns_per_s` are the model's names for those two costs. The defaults stay 0 and 100. T45 measured both on one host and records them as a labeled scenario in [§13](#13-budgets). The defaults were not replaced (OQ-281). The fluid equality with the host cursor stays the optimistic comparison in [§8.3](#83-budgets). The receivable-prefix gate in [§8.6](#86-causal-readiness) is a model input. It is not the seat message.
5. The joiner sends commands only after input is opened by the grant / request / go exchange ([§8.7](#87-ordered-join-protocol-ownership-events-and-queue-bounds-t52-f2)). A stored hash match at `H` does not open input, the seat transition at `A` does not open input, and input does not wait until received and applied sit on `H` again. The host names `A` only after it has accepted the report for that same `H`, and `A` is a turn the host has not sent yet; the ownership change is an event in turn `A`. A mismatch, a cancel, an abandoned pair, a cap or an input that never becomes fresh restarts that joiner (and restores its seat's previous owner by a second event if `A` was published). An expired checkpoint alone does not: the host waits for a later watermark and selects a later hash turn. `paused` stays false. A mismatch that includes a peer already in the game is [§7](#7-hash-checks).
6. A cap that trips during the transfer or the chase restarts this joiner only. Turns still named by another running pin stay. The table is not paused. `Table.advance` does this before it retains the new host turn. An older generation is selected first when the union overflows. A younger pin that still fits stays. `Table.attach` refuses a save past `fragment_max` before a generation is stored, and it refuses an ask whose clock is not the owner's clock. `Table.note_backlog` restarts a backlog past `receive_backlog_turns`. Those calls are the path for the parts in the byte bound. A caller does not retain a later turn by querying the span.

This is the plan's OpenTTD-style join (`docs/plan/lan-endless.md` §4). The snapshot is `World::save()` / `World::load`, not a second format.

### 8.6 Causal readiness

Four frontiers share one integer clock, measured in milliseconds from the snapshot boundary `S`. Under assumption A10 the host applies a turn at the same boundary that produces it, so the first two frontiers match and stay separately named:

| Frontier | At time `t` |
|---|---|
| Host-produced | `S + t // turn_ms` |
| Host-applied | the same value, A10 |
| Joiner-received | the contiguous prefix of turns whose arrival time is `<= t` |
| Joiner-applied | turns the joiner has `run_turn`ed, never past the received prefix and never past the host |

Turn `S+k` (`k >= 1`) is produced at `k * turn_ms` and is sent on channel 0 at that instant. Channel 1 does not delay it. The earliest arrival is `k * turn_ms + one_way_ms`. A reorder may add delay. It may not remove the one-way delay. A hole, including a later turn that arrives while an earlier one has not, does not advance the received prefix. That later turn still takes one backlog slot. A duplicate delivery does not. `Table.note_backlog` runs at the arrival, while the snapshot is loading and while a replay step is unfinished. At one timestamp the order is deadline, then tail, then that delivery, then a joiner apply only after `replay_step_ms` has elapsed. The joiner therefore has not applied a turn at the millisecond it is produced.

The acceptance latency is 150 ms one way and the turn is 100 ms. Turn 1 is produced at 100 ms and arrives at 250 ms. The largest host lead over the receivable prefix is 2 turns. For every `t` at which the host has applied a turn after `S`, the joiner's received prefix is strictly behind the host. No replay rate closes that gap: the turn that would close it has not arrived. Matching the current produced frontier would require erasing the delay or pausing peers who are already playing. This proposal does neither. `command_delay` stays 2. The host still stamps `current + command_delay`. That lead is the plan's existing default, not a new barrier.

Readiness is the first time, after the snapshot has loaded, at which joiner-applied equals joiner-received and that prefix is exactly the set of turns that have had time to arrive. That set is the due prefix: every relative turn `k >= 1` with `k * turn_ms + one_way_ms <= t`. The contiguous prefix that happened to arrive is a different count. Applied equal to the arrived prefix is not readiness while a due turn is missing or still reordered. Input stays disabled there. It is enabled only when the joiner has stayed on the due prefix through the next ordinary hash turn (`hash_every`, default 10) and that hash matches. A hash multiple of a shorter arrived prefix does not enable input. The snapshot boundary is not itself that check. A hash miss of the joiner alone restarts the joiner. `paused` stays false. A miss that includes a peer already in the game remains [§7](#7-hash-checks).

A snapshot that finishes loading at 3 000 ms, with turn 11 never delivered and later turns delivered on time, is the counterexample. At 3 100 ms the arrived and applied prefixes are both 10, the host is on turn 31, and the due prefix is 29. Later turns have arrived, so the schedule has a hole, and 10 is a hash multiple. That is not success and it does not enable input. The same block holds when the missing turn is the last due turn and nothing later has arrived: there is then no hole, and a check that only looks for a later arrival would accept the short prefix. Delaying turn 11 by 2 000 ms on that same late snapshot keeps input off at 3 100 ms. The due prefix is first applied at 3 480 ms, which is not a hash turn, and input opens at 4 160 ms on turn 40 while the host is on turn 41.

A replay rate at or below the live 10 turns/s does not reach the boundary; the result is `restart` / `cannot_outrun`. A missing prefix, a reorder that has not been repaired, a tail or backlog cap, and a deadline are the same kind of result. None of them is `success`. The deadline and the tail are `Table.advance`, in that order, before a success at the same millisecond. A joiner who is still running at `elapsed >= catchup_ms` restarts; the chase does not get a separate terminal check that can succeed on that millisecond. A younger pin that fits stays, and the host turn keeps moving. With `catchup_ms` set to 1 160, the one-fragment chase is `restart` / `deadline` at 1 160 ms. One millisecond earlier the joiner is still running and input is not yet enabled. One millisecond later the joiner stays restarted. The published input time below is the same chase under the 600 000 ms deadline, which has not fired.

Joiner-applied counts a turn only after its `replay_step_ms` interval has elapsed. The shared owner walks that interval before the cursor moves. A deadline, a tail cap, or a horizon that stops the walk keeps the last completed turn. A walk that ends on `catchup_ms` is that stop: the owner restarts and the turn is not counted, so input stays disabled. On the one-fragment clock the step for turn 10 runs from 1 150 ms to 1 160 ms. `catchup_ms` of 1 155 or 1 159 therefore restarts with applied 9, the same applied cursor as `catchup_state_at` at that instant. At 1 160 ms that helper reports applied 10, because it has no owner; the deadline restart at that millisecond reports applied 9. A horizon of 1 155 under the 600 000 ms deadline stops the same unfinished step with applied 9. The 600 000 ms deadline still enables input at 1 160 ms on turn 10, because that step finishes while the joiner is still running. A horizon or deadline before `transfer.done_ms + load_ms` does not start replay. Channel 0 still delivers during that interval. A snapshot that completes at 3 000 ms, read with `horizon_ms` 2 000, stops at 2 000 ms with applied 0, received 18 and backlog 18. Input stays disabled. A nonzero `load_ms` that leaves load completion after the horizon keeps those cursors. `receive_backlog_turns` 5 restarts that joiner at the sixth arrival, 750 ms, with applied 0. A deadline on an arrival's millisecond restarts before that arrival is stored. `catchup_state_at` has no owner and still counts the arrival. `pin_turns` is the applied cursor, and `host_span` is the host frontier at the stop time.

Channel 1 keeps the window of 32. One window of 32 payloads of 1 200 bytes serializes in 3 072 µs at 100 Mbit/s. One channel-0 datagram of 69 bytes serializes in 6 µs. The sum, 3 078 µs, is under the 150 ms one-way delay, which is why this clock omits both. The channels are not placed on one queue. A shared link, jitter, and the flight of the channel-2 hash datagram are not in the model. [§8.7](#87-ordered-join-protocol-ownership-events-and-queue-bounds-t52-f2) is the design for those three. It does not change this clock, and OQ-320 stays open until a `netsim` case or a measured link tests it. The fluid quotients are not recomputed: `17 130 * 150 = 2 569 500`, and `2 569 500 = 32 * 80 296 + 28`, so the 150 ms-RTT figure remains 80.3 s; `17 130 * 300 = 5 139 000 = 32 * 160 593 + 24`, so the 300 ms-RTT figure remains 160.6 s.

The one-fragment row makes the two clocks visible. The snapshot fragment arrives at 150 ms. The fluid comparison then applies turn 1 at 160 ms (`counterexample_1188_done_ms=160`). On the causal clock that turn is not receivable until 250 ms, readiness with an empty prefix is 150 ms, and input is enabled at 1 160 ms on turn 10 while the host is on turn 11. Tokens: causal_one_fragment_arrival_ms=250, causal_one_fragment_done_ms=1160. Those tokens are the 600 000 ms deadline. They are not the 1 160 ms deadline above. They are also not the seat transition in [§8.7](#87-ordered-join-protocol-ownership-events-and-queue-bounds-t52-f2). At 1 160 ms this model has applied 10, received 10 and host 11. The host learns that only from a later watermark, and the cursors have moved by the time the watermark arrives.

Admission is unchanged. `J` is still the reserved upper bound, not the applied frontier. The defaults `load_ms` 0, `replay_turns_per_s` 100 and `turn_store_cap` 1200 are unchanged. The numbers below are model output from those inputs and from the T45 labeled rates. They are not a measured join.

### 8.7 Ordered join protocol, ownership events and queue bounds (T52 F7)

[§8.6](#86-causal-readiness) is a model clock. Its due prefix and its boolean hash match are inputs of `evaluate_causal`; they are not messages. This section names the messages and the guards, and every row below is produced by one executable checker, `tools/join_activation.py` (tests: `tools/test_join_activation.py`). Every run also writes a raw event log (sends, arrivals, deliveries, acknowledgements, applied turns, ownership, input, admissions), and `validate()` re-derives fifteen checks from that log and the configuration alone, without reading a `Chan` cursor or a peer object: (i) channel-0 deliveries follow the contiguous arrived prefix; (ii) the window span, from the send records; (iii) every pair of peers that applied the same turn has the same ownership table and root; (iv) at the end every seated peer is within `seated_lag_max` (20 turns) of the host or has a logged outage; (v) at the millisecond input opens, and after every later event in that millisecond, host turn minus joiner applied turn is within a stated bound; (vi) a seat is held by at most one live attempt or seated human; (vii) every produced turn is delivered to, or still retained for, every live peer; (viii) a grant names a turn already sent to that peer; (ix) an outage is bounded: it ends with an ACK or with the seat's drop within `grace_ms`; (x) the retained set rebuilt from `retain` / `release` events never passes `tail_turns_max` turns or `tail_bytes` bytes and no turn is retained under a pin wider than `tail_turns_max`; (xi) transport epochs: every send, arrival, delivery and ACK carries the pair's current epoch (the count of `epoch_bump` events, modulo 65 536), and no datagram is accepted in a later epoch than it was sent in; (xii) a peer whose seat was dropped has no input open; (xiii) generation identities are never reused, at most `max_generations` are live, an image is freed only when no live attempt owns it and none outlives its last owner. (xiv) a logical message of several fragments is delivered only when every claimed part is still owned by this receiver for this message: matching send, current-epoch arrival and acquisition records are required; partial releases remove only named parts, and already delivered parts cannot be consumed twice. A fabricated complete parts list is rejected; (xv) a reassembly buffer never holds more fragments than its limit, and an epoch change, a drop or the end of an attempt leaves no fragment held without an explicit `reasm_release`. Check (vii) is rebuilt around **complete logical turns**: a produced turn a live peer still needs must be delivered, wholly retained by the host log, or wholly held in flight (every fragment is unacknowledged in the sender's window or already in the receiver's reassembly buffer, in the current incarnation); a fragment sent once is not ownership, and the receiver's holdings come from arrival events and are given up only by explicit `reasm_release` events (delivered, reset, pruned, overflow, epoch, drop, attempt end), never by the passage of time; they are discarded at an epoch change, a drop or a dead attempt; it is evaluated at every host-log release, nonterminal assembly release and at `end_t`. Checks (iv) and (ix) track outages per peer, source and channel and are closed by a drop, a new attempt or an epoch change; only an open outage on the host-to-peer turn path (channel 0) excuses a lagging seated peer. Check (iv) now looks at the outage that is open **at the end**, not at any outage that ever happened, and check (vii) is rebuilt from retention evidence (below). Validators read events and the configuration only. The validators may use the host's cursor as an oracle; the protocol rules may not (only the host's own turn is read, by the host). The checker is a protocol oracle: roots are symbolic values (`("root", turn, ownership table)`), and it is not a game simulation, not a measurement and not production networking. `tools/snapshot_catchup.py` is unchanged and is not used as the protocol's delivered cursor. Where this section cites a millisecond from `catchup_state_at` or `evaluate_causal`, it says so; all other milliseconds come from the checker and are design arithmetic. **No old fixed timestamp is an acceptance target.** The link is 100 Mbit/s each way, 150 ms one way, 300 ms round trip, zero jitter (OQ-371); serialization time is not scheduled by the checker (the largest datagram is 100.24 µs).

**What F1 got wrong (superseded).** The T52 F1 text, and the "all MET" acceptance in its report, chose arrival times for each message independently. That is not a protocol. Six consequences were found in round cb and are now regressions in the checker (`RoundCbRegressions`; they fail under `JOIN_RULES=F1`, see the report). Round cc found further defects in F2 (a stale grant opens input, a grant names a turn not yet sent, late selection and expiry discard the transfer, seats are not exclusive, a restart cannot be the same peer); they are the `RoundCc*` regressions, written before the fixes and shown failing at `5ea1f34`:

| Round-cb finding | What the F1 text claimed | What ordered transport gives |
|---|---|---|
| (a) stale activation | the post-H hole row installs at 3 380 ms with applied 14, received 32, host 33 | with `rules="F1"` the checker opens input at applied 14, received 32. F2/F3: input opens only after the grant / request / go exchange (below). |
| (b) channel-0 order | accept at 2 310 ms, `A = 25`, with the old report's retry missing until 2 410 ms | a watermark sent after a stuck report on the same pair and channel is delivered after it, never before: with `rules="F1"` (independent arrivals, no resend) the checker accepts at 2 310 ms, which ordered delivery forbids; the F2 flow accepts at 2 810 ms. A lost watermark is resent at `resend_ms`, not dropped. |
| (c) endless02 | the exchange starts at H = 1 790 | H = 1 770 is applied at 178 350 ms, its watermark arrives at 178 500 ms, before its 179 000 ms deadline. It is the first selection. |
| (d) ownership events | `SeatActive` goes to four destinations | the seat change is an event in turn `A`; every peer that replays turn `A` applies it, including the other joiner. |
| (e) generations | a new `generation_id` drops the old slots | roots are keyed by turn, not generation; attempts and generations keep their own pins. |
| (f) queues | 24 turn periods bound the window | at a 1 216-byte burst every turn, 24 periods want 48 sequences (two datagrams per turn); the 32-sequence window, not time, is the bound. |

#### Transport model (checked)

- **Event queue.** Integer milliseconds. Tie order inside one millisecond: deadline and expiry timers (0), inbound ACK-only datagrams (10), data deliveries (20), the coalesced ACK-only flush (25), replay-step completion (30), the joiner's input decisions at the end of the delivery batch (35), host turn production and join asks (40), then resend checks (50). Insertion order breaks the remaining ties.
- **Sequences.** Per directed pair and per channel, from 0. Channel 0 is reliable and ordered: the application sees sequence `n` only after every sequence below `n` was delivered. Channel 2 is unreliable and consumes a sequence but is never acknowledged. A fragmented message uses one sequence per datagram and is delivered when its last fragment is.
- **Loss, delay.** A loss is listed by (message, direction, sequence, attempt); `Ack` is the ACK-only datagram and its sequence is its index on that direction. An extra delay is listed the same way. A reorder on channel 0 is repaired by ordered delivery.
- **Resends.** A reliable datagram not acknowledged `resend_ms` after its last send is sent again with the next attempt number. For a **joiner's** pair, after `max_attempts` (8) sends and one more `resend_ms` the pair is abandoned (2 400 ms after the first send) and that join restarts (reason 4). **A seated peer's pair is never abandoned** (F4): after `max_attempts` its resends back off to `outage_resend_ms` (1 000 ms, one keep-alive), and the rules of §10 apply: no ACK for `wait_ms` (2 000 ms) is a logged *outage* state of that peer (validator iv excuses the lag only with that record), an ACK ends it, and after `grace_ms` (60 000 ms) the host publishes the AI for that seat in turn `current + command_delay`, closes the pair and the peer is no longer live (`drop_seated`). Losing only a peer's ACKs therefore never abandons its pair: the window stalls, the outage is logged, and the stalled peer catches up from the log when an ACK gets through. Selective-hole fast resend is not modelled; the timer is the only resend trigger (a conservative design assumption).
- **Reconnect after abandonment: the transport epoch (R1).** A pair carries an *epoch* number (0 at the start), **a `u16` field of every data and ACK-only datagram** (§3). When a peer that was here before asks again (attempt 2 or later), the ask is one datagram with a 5-byte payload (60 bytes on the wire, counted in the statistics) that carries the next epoch. Both endpoints then retire the sender and receiver state of **every channel in both directions together** (sequences restart at 0, windows, waiting items, cursors, ACK state). A receiver compares the packet's field with its own epoch before it looks at the sequence, and the sender compares an ACK-only's field with its own; resend and flush timers are local and carry the epoch they were created in. Data, ACK and timer rejection are separate checks (`stale_epoch` with `what` = `arrive`, `ack`, `resend`, `flush`). A delayed datagram of attempt 1 therefore cannot be delivered, or acknowledged, as sequence 0 of attempt 2. An abandoned channel (eight sends lost, one more `resend_ms`) restarts the join (reason 4), and the re-ask after the losses stop completes: tested for the grant on channel 1 (host to joiner), the turn stream on channel 0 (host to joiner) and the joiner's watermarks on channel 0 (joiner to host); the joiner sends no reliable data on channel 1, so that direction does not exist. Unrelated peers keep their pairs and their epochs.
- **ACK-only datagrams (design assumption).** An ACK-only datagram has `payload_len` 0. It names `ack` and the 32 `ack_bits`, consumes no sequence number, is unreliable, is never acknowledged and never resent. Its loss is repaired by the next ACK-only or by the sender's resend. It is sent when a reliable datagram arrived on that channel, flushed after the data deliveries of that millisecond and before replay-step completion (priority 25), so arrivals in one millisecond coalesce into one ACK-only. A 53-byte ACK-only (25 + 28) is outside the sequence window, so a full window never blocks it. In the scheduler it goes first, before data; it is never queued behind a snapshot fragment. This repeats `sequence` as "next sequence to send" on the wire; the receiver ignores it. Whether the wire should instead reserve a flag is OQ-372.
- **Window.** A sender transmits a new sequence only while it is fewer than 32 ahead of its oldest unacknowledged sequence. The checker records every pair's maximum span and fails if it exceeds 32.

#### Messages and encoding (design assumptions)

| Message | Channel, direction | Payload fields | Payload | Wire |
|---|---|---|---:|---:|
| `Turn` | 0, host → every peer | head 16, then 14 bytes per ownership entry | 16 + 14·n | 69 + 14·n |
| `JoinGrant` | **1**, host → joiner | `gen` u32, snapshot turn u64, seat u8, `attempt` u16 | 15 | 68 |
| snapshot fragments, the last `SnapshotDone` | **1**, host → joiner | 1 188 save bytes each after a 12-byte fragment head (§4) | 1 200 (last: remainder) | 1 253 (last: remainder + 53) |
| `Cmd` | 0, seated peer → host | the peer's commands (a configurable stream, `seated_cmd_bytes`) | configurable | payload + 53 |
| `SeatHash` | 2, seated peer → host | the §7 hash report, once per `hash_every` | 20 | 73 |
| `JoinWatermark` | 0, joiner → host | `gen`, `attempt`, `received` u64, `applied` u64 | 22 | 75 |
| `JoinCheckpoint` | 0, host → joiner | `gen`, `attempt`, `H` u64, root u64 | 22 | 75 |
| `HashReport` | 0, joiner → host | `gen`, `attempt`, `H` u64, root u64 | 22 | 75 |
| `InputGrant` | 0, host → joiner | `gen`, `attempt`, `C` u64 | 14 | 67 |
| `JoinAsk` | before channel 0, joiner → host | the next transport `epoch` u16, `seat` u8, `ask` id u16 (a re-ask only) | 5 | 60 |
| `InputReq` | 0, joiner → host | `gen`, `attempt`, `applied` u64, `nonce` u16 | 16 | 69 |
| `InputGo` | 0, host → joiner | `gen` u32, `attempt` u16, `nonce` u16 | 8 | 61 |
| `InputOn` | 0, joiner → host | `gen`, `attempt` | 6 | 59 |
| `JoinRestart` | 0, host → joiner | `attempt`, reason u16 (1 expiry, F1 only; 2 mismatch, 4 attempt cap, 5 input never fresh, 6 catch-up deadline, 7 queue overflow, 8 cancel, 9 seat busy, 10 superseded by a new ask of the same peer, 11 no free transfer slot, 12 receive backlog, 13 tail cap) | 4 | 57 |
| `JoinAbort` | 2, joiner → host | `gen` u32, `attempt` u16, `reason` u16 (4 pair abandoned, 8 cancel, 12 receive backlog) | 8 | 61 |
| ACK-only | either | none | 0 | 53 |

The "Wire" column of this table is payload + 53 (header 25 + IP/UDP 28); with the u16 `epoch` of §3 it is payload + 55, which is what the checker counts. The join identity is (`gen`, `attempt`), and **the host assigns the attempt number**: 1 for a peer's first ask, then one more for each ask of the same peer. A joiner that receives a `JoinGrant` with a higher attempt than its own retires everything of the old attempt (turns, roots, pins, cursors, input) and starts again from the new snapshot; a grant at or below its attempt is a duplicate and is ignored.  a session-wide seat event has its own `event_id` (below) and never carries either. An ownership entry is a `TurnEntry` (§6): `u32 issuer`, `u32 seq` (the `event_id`), `u16 type`, `u16 len`, then seat `u8` and owner `u8`, 14 bytes. These are design assumptions, not wire commitments.

#### Ownership events live in the turn stream

The host does not send a separate seat message. When it accepts a report it appends the event `(event_id, seat, human)` to turn `A` before that turn is produced. Because `A = C + command_delay` and `C` is the host turn at the accept millisecond, `A` is two turns past the last produced turn and has not been sent. Every peer, seated or replaying, applies the event when it applies turn `A`. This includes the other joiner of an overlapping join and a joiner that restarted from a snapshot taken before `A`: the turn is part of the retained history (the pin) and arrives on channel 0 like any other.

- **Identity.** `event_id` is a host-assigned, session-wide, strictly increasing `u32`. It is not a `gen`, an `attempt` or a snapshot id. Events are ordered by `event_id` inside a turn. Because `A` never decreases with `event_id`, a high-water mark is a complete duplicate filter: a peer skips an entry whose `event_id` is at or below its applied high-water mark.
- **Snapshot coverage.** A snapshot at turn `S` contains the ownership table and the high-water mark. It covers every event with `A <= S`. A joiner that loads it replays only turns after `S`, so it applies exactly the events the snapshot does not cover. Test: a joiner whose snapshot is at turn 30 holds the event of `A = 18`; a joiner whose snapshot is at turn 12 replays it.
- **Release.** A restart or cancel after `A` was published cannot unsend turn `A`. The host appends `(event_id, seat, previous)` to turn `current + command_delay`, where `previous` is the owner the stream showed for that seat when the attempt was admitted: an empty seat stays empty, an AI seat returns to the AI, so every peer restores the seat at the same turn. A restart before the accept publishes nothing, so a failed join on an empty seat never creates an AI.
- **Late messages.** Any message carrying an `attempt` other than the host's current one for that joiner, or arriving after the attempt is dead, is dropped and logged. A joiner in `Restarted` ignores everything except `JoinRestart` and a newer `JoinGrant`. A joiner that had opened input closes it on `JoinRestart`.

#### Joiner and host states

Joiner: `Loading` (grant received) → `Replaying` (snapshot loaded) → `Owned` (it replayed its own seat's transition; input is still off) → `Active` (input on), or `Restarted` from any state. Host, per attempt: `Waiting` → `Checkpoint` (an `H` is selected) → `Accepted` (event queued) → `Done` (`InputOn` received; the pin is released, the joiner stays an ordinary peer), or `Dead`.

**Seats are exclusive.** At admission the host refuses an ask, before it reserves a pin, a generation share or a queue entry, when the seat is owned by a seated human or reserved by another live attempt. It answers with `JoinRestart` reason 9 and creates no attempt state. The same peer asking again while its old attempt is alive first retires that attempt (reason 10, with the release of its seat if `A` was published) and then gets the next attempt number.

**Watermarks.** The joiner sends a `JoinWatermark` each time it applies a hash turn whose root it holds, while it has fewer than `max_outstanding_wm` (design limit 4) watermarks unacknowledged. A hash turn held back by that limit goes out when an acknowledgement frees a slot. A watermark that offers a hash turn pins that root in the joiner's cache; at most `max_root_pins` (design limit 2) roots stay pinned besides the 4 recent slots, and the older pins are released first. With 4 watermarks outstanding, 2 offered roots are pinned at any time, not 4. One watermark per hash turn is why an early candidate on the `endless02` chase is offered at all: a limit of one outstanding watermark samples every 30th hash turn and skips 1 770.

**Selecting H.** On a delivered watermark the host takes the greatest multiple of `hash_every` at most the watermark's `applied`, for which it still holds the root (4 recent slots plus pins), which is above every checkpoint of this attempt that has expired, and which the report can still reach in time: `now + rtt + select_margin_ms < deadline`. `rtt` is the host's smoothed round trip to that joiner, measured from the ACK-only datagrams of datagrams that were never resent (the joiner's `JoinGrant` gives the first sample; `rtt_initial_ms` 1 000 stands in before it), and `select_margin_ms` is 100. A candidate not greater than the current `H` selects nothing. A greater one supersedes only after the current checkpoint has had one round trip plus the margin (`sel_ms + rtt + select_margin_ms`); a watermark that arrives earlier waits, because the report it would discard may still be on its way. The deadline of a hash turn is the millisecond the host produced that turn (it does not send the root to the joiner; the checkpoint carries it later), plus `2 · hash_every · turn_ms` (2 000 ms), plus `deadline_rtts` (2) measured round trips. At the deadline millisecond the deadline is ordered first: a report arriving then does not accept. **Expiry does not restart the join.** The host marks that checkpoint expired, returns to `Waiting`, and selects again from the next delivered watermark that offers a later hash turn. A restart happens only for a root mismatch (reason 2), `catchup_ms` (6), a queue cap (7), an abandoned pair (4), a cancel (8), an input that never becomes fresh (5) or a seat conflict (9, 10).

**Liveness of the supersede rule and the stated domain.** (1) The rule cannot livelock at any round trip: a checkpoint is replaced only after its own report had a round trip, and the deadline grows by two round trips, so a window in which the report fits always exists once the joiner is near the host. (2) The joiner offers a hash turn every `hash_every` turns, so a report that is lost ends at its deadline and the next offer is at most one hash period later. (3) The root cache keeps the newest 4 hash turns, so the joiner must be within about 40 turns of the host before it can be selected at all; a replay that does not catch up never is. The checker's scans (loss-free, `tools/test_join_activation.py`): `endless02` (snapshot done at 160 650 ms, 1 606 turns behind) at replay steps 1–60 ms per 100 ms turn, including 15, 24, 25, 29, 30, 34, 40, 45 and 50, completes without a restart (F3 stated 1–73; the pin cap that F5 enforces shortens it, see the retained-turn contract). The limit is the catch-up arithmetic against the pin cap: the backlog closes at `1 / step - 1 / 100` turns per ms, and the join must finish before turn 4097 would be retained (409 700 ms), so 1 606 turns need at most 248 950 ms, i.e. 60.8 ms per turn. Steps 61–100 (70 included) end in the join-only tail-cap restart (reason 13) at 409 700 ms and the table keeps running; without the cap they would have run to the 600 000 ms `catchup_ms` (F3's 73 ms limit). The normal row at one-way delays of 0 to 450 ms in 25 ms steps, and 500, 600, 900, 1 200 and 1 500 ms, completes without a restart. At 2 000 ms one way the 32-datagram window (32 sequences per round trip against 40 turns per 4 s round trip) delivers 8 turns a second against 10, the joiner never reaches the host, and the oldest pin is restarted when it reaches the tail cap (reason 13): that, not a deadline, is the latency ceiling of the model, about 3.2 s round trip at 10 turns per second.

**Report.** On `JoinCheckpoint` the joiner looks up its own stored root for that same `H` (never its current root) and sends `HashReport` on channel 0. Equal roots for the selected `H` before its deadline accept; unequal roots restart that joiner (reason 2). A report for another turn, or for an attempt that is dead, is ignored. Because the host root arrives on channel 0 inside the checkpoint, the unreliable channel-2 root is not needed by the joiner in F2 (the host still sends it to seated peers under §7).

**Accept.** `C` is the last turn the host has produced and enqueued (not `t // turn_ms`: a watermark delivered on a turn boundary is handled before that turn is produced), `A = C + command_delay`, event queued in turn `A`. The host arms `input_ready_ms` (design limit 20 000 ms); if the joiner is not `Done` by then the host restarts it (reason 5) and restores the seat. The host does not restart under an `InputGo` it has just sent: it waits `2 · rtt + 2 · turn_ms` for the `InputOn`, so a joiner never opens input after the host restarted its attempt (an `InputOn` from a dead attempt is dropped, and a joiner that did open input closes it on `JoinRestart`).

#### Input is separate from ownership

At turn `A` every peer has the same ownership; the joiner has only replayed it. Input needs a second, observable fact about **current progress**, and the check does not use the host's cursor, an ideal due frontier, or equality of any two cursors:

1. After the joiner replays its own seat transition (state `Owned`), and at every later hash turn, it sends a watermark. The host answers any watermark from an accepted attempt with `applied >= A` by an `InputGrant` that names `C`, the last turn it has already enqueued to that joiner. A grant never names a turn that has not been sent (validator viii).
2. **Joiner side.** At the end of the delivery batch of the millisecond in which a grant is delivered (priority 35, after every delivery and replay step of that millisecond), the joiner computes `known - applied`, where `known` is the highest turn number in its transport buffer from the host, delivered or still held behind a hole. If it is at most `input_lag_max` (design limit 6) and `received >= C`, the joiner sends `InputReq(applied, nonce)`; otherwise it counts the grant as rejected and waits for the next hash-turn watermark. "Ordered delivery proves freshness" is withdrawn: a grant held back by a lost datagram is delivered in a batch with older turns, and the turns the host produced since are still in flight.
3. **Host side.** On `InputReq` the host checks its own turn: `host_turn - applied <= input_host_lag_max + ceil(rtt / turn_ms)` (6 plus the turns of one round trip) and `applied >= A`. Only then it sends `InputGo(nonce)`. This is a bound, not equality, and it uses the host's own cursor, which the host knows.
4. **Joiner again.** On `InputGo` the joiner measures the time since it sent that request with its own clock (no synchronisation): if it took more than `1.5 · srtt + turn_ms`, the exchange is stale and input stays off. Otherwise, at the end of that millisecond's delivery batch, it repeats the lag check and opens input (`Active`), then sends `InputOn`. A delayed grant or go therefore cannot open input on a prefix that the host has left behind: the host's check bounds the lag at the time it answers, and the elapsed-time check bounds how long the answer was on its way.
5. The checker shows this for `InputGrant` lost 7 and 4 times and for the turn before it lost 7 times, at replay steps of 10, 40, 60 and 80 ms per turn: input opens with the host at most 6 turns ahead of the joiner's applied turn, and the mutant that removes both gates opens it 9 to 23 turns behind at the 40 to 80 ms steps (at 10 ms per turn the round trip alone lets the joiner catch up, which hides the defect). F1's `A - 1 <= applied < A` guard opens input at exactly applied 14, received 32.
6. If input is never fresh within `input_ready_ms`, the host restarts the joiner and restores the seat as above.
7. **No turn is lost at completion.** Delivered turns that are not yet applied stay in the joiner's buffer (the live buffer) and are applied in order exactly once. F1's "free the backlog" step is removed. The test asserts the buffer equals the received-but-unapplied turns and the applied sequence is contiguous.
8. **Pin and cache lifetimes.** The host's turn pin for a joiner starts at its snapshot turn plus one and ends when the host receives `InputOn`, or on restart. A turn already delivered is the joiner's responsibility from then on; one not yet acknowledged is still held by the pair's resend window. A host root slot is pinned from selection to accept, restart or expiry. The host never holds more than the 4 recent slots plus one pin per live attempt; the joiner never more than 4 plus `max_root_pins`. Tests assert both bounds on the `endless02` chase.

#### Overlapping joins and generations

Roots are keyed by turn. Only pins are per attempt. A new generation therefore drops nothing: the older attempt keeps its selected root and its attempt identity. The global caps are unchanged (§8.4: `concurrent_transfers`, `max_generations`, `tail_turns_max`); the root caches add at most one pin per live attempt. Cases in the checker, each with identical ownership and identical roots at every matching turn on the host, the seated peers and every replaying joiner:

- **Two joins, one generation.** Both ask within `snapshot_age_ms`, share snapshot turn 0, both accept at 1 610 ms with `A = 18`, events 1 and 2 in turn 18, and both open input at 2 560 ms (applied 24). Each joiner replays the other's seat.
- **Two joins, two generations.** The first joiner's transfer is slow (snapshot done at 65 000 ms); the second asks at 61 000 ms, past `snapshot_age_ms`, and gets a new generation. The second (snapshot turn 610) is accepted first (62 610 ms, `A = 628`) while the first still loads; the first later replays turn 628 and its own, `A = 726` (accepted at 72 450 ms). Neither pin is dropped.
- **One cancel, restart from a snapshot before `A`.** The second joiner cancels at 1 000 ms (a best-effort `JoinAbort`; the host restarts it at 1 150 ms, reason 8). **The same peer** asks again at 1 200 ms, the host numbers it attempt 2, the old attempt is retired, and it shares generation 1, so its snapshot is turn 0 (the earlier text said turn 12; the checker's rule is that a live generation inside `snapshot_age_ms` is shared). That is before the first joiner's `A = 18`, so it replays that turn, is accepted at 1 900 ms (`A = 20`) and opens input at 2 760 ms.

#### Bounded transport accounting

Everything below is design arithmetic from the checker's `queue_caps` and message sizes; limits are configurable design limits with the reasons given, and no engine default is adopted.

- **Where produced turns and events wait (S1).** A turn is produced once and kept in the retained turn log (§8.1, charged by `turn_store_cap` and `tail_turns_max`). **Each pair keeps a send cursor into that log**; the window of 32 and `wait_cap` (design limit 64) cover only the datagrams in flight and the waiting *control* items, and the backlog is never copied into a queue. A control message is queued behind the turns produced before it (it carries the turn it must follow) and is sent when the cursor has passed that turn, so the order on the ordered channel is the production order. **The log is charged once**, by `turn_store_cap` and `tail_turns_max`; the byte caps below count only datagrams in flight and waiting control items, so the earlier statement that the waiting references double-count the turn store is withdrawn. A joiner that shares a generation is admitted anywhere inside `snapshot_age_ms` and completes: asks at 9 500, 9 700, 20 000 and 50 000 ms on a generation whose own transfer ends at 65 000 ms all complete (F3 restarted them with reason 7).
- **The retained-turn contract and its policy when nothing can be restarted (R2, E3).** The host's `log_turns` is operational state and obeys the §8.3/§8.4/§10 policy. Each retained turn is **charged once**, whoever needs it, at `max(turn_store_cap, encoded bytes)`: 1 200 for an ordinary turn, 1 232 for the 1 216-byte burst, 8 316 for a seven-fragment turn. The budget is `tail_turns_max` (4 096) turns and `tail_bytes` (`tail_turns_max × turn_store_cap`, 4 915 200) bytes. **Consumers** are the live joins (their pin, from `S + 1`) and the send cursors of live peers (the next turn not yet sent); the rolling window (`turn_buffer`, 600) is not a consumer, it only serves a returning peer. Before turn k is kept the host projects every release that is already due and computes the lowest turn it would keep: the lower of the oldest consumer's need and the rolling floor, and **the rolling floor shrinks until both independent caps hold** (the count `tail_turns_max` as well as the bytes `tail_bytes`; in F6 only the bytes shrank it, so a count cap below the rolling window dropped a healthy peer at turn 101 although it needed none of the old entries). A configuration that cannot hold one turn is rejected when the simulation is created (`Cfg.validate`: `tail_turns_max` < 1, `tail_bytes` below the nominal turn charge, `turn_buffer` < 1, a reassembly limit < 1); nothing is silently retained over a cap. If the kept set still passes a cap, the consumer with the oldest need goes: a live join is restarted (reason 13, oldest generation first, a younger one that fits stays), a seated peer's cursor is dropped as at the grace (`drop_seated`, why `log cap`: its seat goes to the AI and it may rejoin through §8). A turn some live consumer still needs is never discarded silently. A single turn larger than `tail_bytes` is never stored: its commands are rejected, an empty turn takes its place, `oversized_turn` is logged and recorded as a violation. Results: the seven-fragment probe (8 316 bytes per turn, one seated peer, no join) shrinks the rolling window to 591 turns and peaks at 4 914 756 bytes, under the cap, without dropping the healthy peer (F5 allocated 4 989 600); a cap of exactly 10 turns keeps 10; with `tail_turns_max` 100 and the default rolling window one healthy seated peer keeps 100 turns (peak 100, 120 000 bytes) and is never dropped (F6 dropped it at 10 100 ms); caps of 1, 5 and 9 turns, a byte cap of 7 turns under a count cap of 100, and no consumer at all hold exactly; a cut seated peer with a cap of 50 turns is dropped at the cap, not at the grace; a join asking at turn 300 with a cap of 100 turns is restarted when turn 401 would be kept, not earlier. Receive and reassembly state is bounded as before (the last 40 sequences of an ordered channel, 64 of the unreliable one, `receive_backlog_turns`); the host's operational state is published by `Host.operational()` (log turns and bytes, live images, datagrams in flight, waiting control items, reassembly entries) and the diagnostic histories (`trace`, the event log, `root_log`, `history`, `applied_log`, `deliveries`) stay separate and unread by protocol logic.
- **Impossible retained-turn inputs (G1).** Configuration must hold at least the encoded empty turn even if `turn_store_cap` is smaller than 16. If ownership events alone exceed `tail_bytes`, the checker rejects that scenario with `ValueError` before consuming the pending events, applying ownership, advancing the host cursor or retaining the turn. It does not discard ownership events or adopt a new command policy. Rejecting extra commands remains the separate OQ-373 proposal.
- **Reassembly ownership (G2).** A receiver keeps the fragments of every unfinished logical message in a per-pair, per-channel reassembly buffer that is separate from the ACK history. A datagram passing the ordered channel in sequence moves its fragment into the buffer (`reasm_acquire`); the message is delivered when the buffer holds every index 0..n-1 (the last datagram alone does not deliver it), and the fragments are released (`reasm_release`, `delivered`). Pruning the ACK history (the 40 sequences below the highest) therefore cannot release the only copy of a half-received turn. Limits are explicit and configurable: channel 0 holds at most `reassembly_fragments_max` (design limit 4 096) fragments, channel 1 `fragment_max` (17 130, §4). Overflow is finite: the buffer is released (`overflow`), a seated peer is dropped as `reassembly cap`, a joiner aborts (reason 12), an overflow of a seated pair in either direction drops that peer, including a joiner whose attempt is already done; input closes before the drop. A still-joining peer aborts with reason 12. An epoch change, a drop and the end of an attempt release the buffers explicitly (`epoch`, `drop`, `attempt end`). A retired pair cannot acquire late fragments from its old attempt; only the next epoch reopens assembly. This retirement is part of the existing modeled pair lifecycle, not a claim that ask delivery is reliable. Checker results: the 90 000-byte command burst of the round-cf probe (a 76-fragment turn through a window of 32, `turn_buffer` 1) is delivered with all 76 fragments, with a peak of 76 fragments and 90 928 bytes in the buffer; a lost early or a lost late fragment is retried and duplicates take no slot; the `endless02` snapshot peaks at 17 130 fragments and 20 554 900 bytes in the joiner's buffer. The reassembly peaks are published with the other operational state in `Host.operational()` (`reasm_fragments`, `reasm_bytes`) and `Sim.peak_reasm_frags` / `peak_reasm_bytes`.
- **Generations (E3).** `max_generations` (2) is enforced on its own, independently of `concurrent_transfers`: an ask that can share a live generation inside `snapshot_age_ms` always does; an ask that needs a new one while the limit is reached is refused before anything is reserved (`JoinRestart`, reason 14). Identities come from a counter and are never reused; an image is freed (`gen_free`) when its last owner ends, whether by restart, cancel or `InputOn`. The 0 / 62 000 / cancel J1 at 63 000 / 124 000 sequence now allocates generations 1, 2 and 3 with distinct snapshot turns (F5 gave J2 and J3 both generation 2).
- **A dropped peer that rejoins (E2).** A drop (grace or log cap) closes the peer's input, stops its pairs and gives its seat to the AI by an ownership event; the seat is free for another peer. A later ask by the same peer is a new lifecycle: the drop classification is cleared, a new epoch retires the dead pairs, the seat is taken again chronologically, and the F4 outage rules (2 s outage state, 60 s grace) apply to the new attempt. The timeline join 0 / cut 6 000–80 000 / re-ask 85 000 / cut 92 000–95 000 now ends with the peer 3 turns behind or better, an outage logged and ended for the second cut, no abandonment, and clean seat validators; a second grace expiry drops it again; another peer can take the vacated seat; a late datagram of the old incarnation is rejected.
- **Overflow and outages.** A control item that would pass `wait_cap` is refused; for a joiner's pair the host restarts that joiner (reason 7), releases its pin and restores its seat if `A` was published, and keeps producing. A turn is never refused or lost: it stays in the log, for a seated peer as for a joiner (S7: with `wait_cap` 8 and blocked ACKs a seated peer loses no turn and is back at the host's turn within four turns once the ACKs return). A joiner's dead pair (eight sends, one more `resend_ms`) is abandoned and it restarts (reason 4) with a best-effort `JoinAbort` on channel 2; `catchup_ms` is the backstop. A seated peer's pair follows the outage rules in §8.7 "Resends" and §10.
- **The 32-sequence window with fragmented turns.** A 1 216-byte burst every turn is two datagrams (1 200 and 56 payload bytes; 1 253 and 109 on the wire). Twenty-four periods (2 400 ms) want 48 sequences. With every ACK blocked the checker sends 32 (16 turns), leaves turns 17 to 24 in the log behind the cursor, and has produced 24 turns. The pair is **not** abandoned at 2 500 ms any more: the peer is in a logged outage from about 2 100 ms (the first datagram, sent at 100 ms, has waited `wait_ms`), the first datagram is resent every 1 000 ms after the eighth send, and the row now runs 6 000 ms with the window span at 32 throughout. The 24 periods alone are not the bound; the window is.
- **Snapshot wire arithmetic (unchanged from F1, exact).** `endless02` is 17 130 fragments; the last carries `20 349 454 - 17 129 × 1 188 = 202` save bytes, a 214-byte payload and a 267-byte datagram. The exact wire total is `17 129 × 1 253 + 267 = 21 462 904` bytes, 1 717.032 ms at 100 Mbit/s. Counting the last fragment as another 1 253-byte datagram gives 21 463 890 bytes, 1 717.111 ms, 986 bytes higher; that padded figure is an upper bound only. The fluid 80.3 s and 160.6 s stay the labeled comparisons. A 1 216-byte burst splits into 1 253 and 109 byte datagrams; five peers receive `5 × (1 253 + 109) = 6 810` bytes per turn.
- **ACK-only and seated upstream traffic (S6).** Each received reliable datagram on channel 0 or 1 causes at most one ACK-only per pair, channel and millisecond. In the checker's two-joiner row, over 6 000 ms, a joiner sends 63 ACK-only on channel 0 and one on channel 1 (3 889 bytes in total with its 7 data messages), a seated peer 58 ACK-only plus 5 `SeatHash` reports on channel 2 (3 439 bytes), and the host sends each joiner 7 ACK-only (on channel 0, for the joiner's control messages) and a seated peer none: with no `Cmd` stream a seated peer sends only unreliable `SeatHash`, which is not acknowledged. With a 40-byte `Cmd` per turn per seated peer the same 6 000 ms count is 63 data datagrams and 58 ACK-only from the peer and 57 ACK-only from the host (7 175 bytes host to peer, 8 833 peer to host). Statistics are kept per direction and ACKs per channel. A snapshot fragment burst is acknowledged at most once per millisecond (53 bytes) on channel 1 and the turn stream at most once per millisecond on channel 0: **440 kbit/s per channel, 880 kbit/s for both** with the epoch field (424 / 848 before it), the corrected figure (the earlier 424 kbit/s counted channel 1 only).
- **Host queue cap (six peers: the host, three seated peers and two joiners, so five destinations).** Channel 0: 5 destinations × 32 datagrams in flight × 1 255 bytes = 200 800 bytes, plus at most `wait_cap` waiting control items per pair (control items are 53 to 90 bytes on the wire, so the 80 320 bytes per pair that the full-size arithmetic gives is an upper bound, kept in `queue_caps` as `ch0_waiting_per_pair`). Channel 1: 2 joiners × 32 × 1 255 = 80 320. Channel 2: 5 × (75 + 55) = 650. The conservative total of `queue_caps` is 683 370 bytes (with the full-size waiting references), 54.67 ms at 100 Mbit/s, inside one 100 ms turn. One joiner's reverse queue is control only and bounded by its own rules: 4 watermarks (77), 2 reports (77, `max_outstanding_reports`), 2 `InputReq` (71, `max_outstanding_req`), one `InputOn` (61) and one `JoinAbort` (63, channel 2): 4·77 + 2·77 + 2·71 + 61 + 63 = **728 bytes** (the earlier 568 omitted the `InputReq`s and left the reports unbounded and counted a `JoinAbort` on channel 0), plus the ACK-only datagrams above. The joiner's received-but-unapplied turns are capped by `receive_backlog_turns` (design limit 4 096, `tail_turns_max`): past it the joiner asks to be restarted (reason 12) and the table keeps running.
- **Scheduler order.** ACK-only (end of millisecond, outside the window) → seated channel 0 → joiner channel 0 → channel 1 (one new fragment per joiner per pass, never more than 32 in flight) → channel 2. A fragment already started is finished: the nonpreemptible unit is 1 253 bytes. A join control message never waits behind snapshot fragments (separate channel, own sequence).
- **Join refusal without a pause (S4).** `concurrent_transfers` (2) live attempts hold the slots: a third ask is refused (`JoinRestart`, reason 11) before a pin, a generation share or a queue entry is reserved, and the running joins continue. The `JoinGrant` and the snapshot fragments are channel-1 datagrams with the window of 32, ACK, loss and resend: a lost grant is resent after `resend_ms`; the channel is ordered, so the snapshot waits behind it. Every fragment carries the 2-byte epoch: the exact `endless02` wire total is `17 129 × 1 255 + 269 = 21 497 164` bytes (the last fragment is a 214-byte payload plus 55), 1 719.8 ms at 100 Mbit/s, 34 260 bytes more than the 25-byte-header figure. A snapshot of one to five fragments is simulated per datagram (a lost fragment is resent and delays the load), and `endless02` is simulated with all 17 130 fragments: the last is delivered at about 160 600 ms, matching the fluid 160.6 s of §8.6. Scans that run `endless02` many times at different replay steps still use the labeled input `snapshot_ms` 160 650 for the transfer (the JoinGrant is still a channel-1 datagram); every part of those rows after the snapshot load is checker output, and the transfer duration is the only input. Any cap, deadline or abandonment restarts one joiner; the table, the seated peers and the other joiner continue.

#### Rows from the checker

Run `python -I -B tools/join_activation.py` (add a row name for one row, `--trace` for events, `--rules F1` for the F1 guards, `queue` for the burst row). `C` is the host turn at the accept millisecond. Input is the millisecond the joiner's input turned on, with its applied and received cursors.

| Row | Losses and delays (message, direction, sequence, attempt) | H | Accept ms (C) | `A` | Input ms (applied, received) |
|---|---|---:|---:|---:|---|
| normal, one fragment | none | 10 | 1 610 (16) | 18 | 2 560 (24, 24) |
| lost watermark | `JoinWatermark`, J1→H, first send lost; resent 1 460, delivered 1 610 | 10 | 1 910 (19) | 21 | 3 160 (30, 30) |
| lost checkpoint | `JoinCheckpoint`, H→J1, attempt 1 lost | 10 | 1 910 (19) | 21 | 2 860 (27, 27) |
| report blocks later watermark | `HashReport`, J1→H, attempts 1–4 lost | 10 | 2 810 (28) | 30 | 3 760 (36, 36) |
| deadline, exact | first report delayed to arrive at its 3 600 ms deadline, copies 2–8 lost | 10 expired at 3 600, then 20 | 3 900 (38) | 40 | 4 760 (46, 46) |
| deadline, one ms earlier | the same, arrives at 3 599 | 10 | 3 599 (35) | 37 | 4 460 (43, 43) |
| turn lost after H | `Turn` channel-0 sequence 12 (turn 13, because the grant now takes sequence 0 of channel 1), H→J1, attempts 1–4 lost | 10 | 2 800 (27) | 29 | 3 660 (35, 35) |
| turn missing at or before H | `Turn` sequence 9 (turn 10), H→J1, all 8 attempts lost | none | none | — | restart 3 400, reason 4 |
| all control lost | `JoinWatermark`, J1→H, all 8 attempts lost, `catchup_ms` 8 000 | none | none | — | restart 3 710, reason 4 |
| `endless02`, slow transfer | none; snapshot done at 160 650 ms | 1 770 at 178 500 | 178 800 (1 787) | 1 789 | 179 660 (1 795, 1 795) |
| two joins, one generation | none | 10 / 10 | 1 610 / 1 610 | 18 / 18 | 2 560 / 2 560 |
| two joins, two generations | slow first transfer | 700 / 620 | 72 450 / 62 610 | 726 / 628 | 73 360 / 63 560 |
| cancel and restart before `A` | J2 cancels at 1 000, the same peer asks again at 1 200 (attempt 2, snapshot turn 0) | 10 / 10 | 1 610 / 1 900 | 18 / 20 | 2 560 / 2 760 |

The rows above are unchanged by F5 (no epoch bump, no cap reached). Input opens three flights after the seat transition because of the grant / request / go exchange; the 600 ms it adds to the F2 rows (2 260 → 2 560 in the normal row) is the price of the freshness bound.

On `endless02` the first selectable candidate, H = 1 770, is selected at 178 500 ms (its watermark arrives before its deadline of 179 600 ms) and its report is accepted at 178 800 ms; the margin rule keeps the host from selecting an H whose report could not return in time (at 30 ms per replayed turn F2 selected H = 2 270 at 228 900 ms with 100 ms to its deadline and threw the transfer away; now the join completes). The 1 160 ms and 179 160 ms of §8.6, and the older F1 rows (1 310 / 1 460 / 1 560 ms and the like), are **model comparisons** from `catchup_state_at`; they are not rows of this protocol and are not acceptance targets.

#### Remaining uncertainties

Serialization and message sizes are design assumptions. The loss and jitter model is the checker's. The `endless02` row uses the checker's replay step (10 ms), not the T45 measured rate. The ACK-only wire form (OQ-372) and the design limits `input_lag_max`, `input_host_lag_max`, `max_outstanding_wm`, `max_root_pins`, `wait_cap`, `input_ready_ms`, `select_margin_ms`, `deadline_rtts`, `rtt_initial_ms` and `seated_lag_max` (OQ-373), and the replay-speed and latency domains above, need the owner or a `netsim` run. F4 models seated-peer outages, send cursors into the retained log, the snapshot and `JoinGrant` as channel-1 datagrams and seated upstream traffic; its limits `wait_ms`, `grace_ms`, `outage_resend_ms`, `concurrent_transfers`, `receive_backlog_turns`, `max_outstanding_reports`, `max_outstanding_req` and `seated_cmd_bytes` are in OQ-373. F5 added the transport epoch, the retained-turn contract and retention/outage/epoch validators; F7 made both retention caps independent and reassembly an owner of its own; F6 made the epoch a wire field, restored the lifecycle of a dropped peer that rejoins, gave every operational owner a cap with a finite policy, enforced `max_generations` with unique identities and made the retention check fragment-aware. What stays unmodelled is jitter, link serialization, how the ask datagram itself is lost or retried (it is assumed delivered), and the real encoded size of retained turns. A seated peer's reliable pair at a round trip above about 3.2 s cannot keep up with 10 turns per second through a window of 32; it stays in the pair as a growing lag that validator (iv) flags. Jitter, link serialization and a measured join remain OQ-371 and OQ-320. OQ-320 and OQ-371 stay open. ADR 0003 stays **Proposed**.

## 9. Host migration

Every peer already has the world, so only the role moves: who numbers turns and who collects commands.

The successor is the lowest seat index that is still connected. Every peer can compute that from the same connected-set, so there is no vote. "Connected" means a packet was received within `net.grace_ms` ([§10](#10-drops-and-outages)).

On the way:

1. peers pause locally as soon as the host has been silent for `net.handover_ms`, default 1000. Reason: one keep-alive interval, long enough not to treat a single lost packet as a dead host, short enough that the table does not play on without a turn source;
2. the successor sends the last turns it has, on channel 0;
3. a peer missing a turn asks the successor for it, by index;
4. each seat resends commands whose sequence the old host had not acked. The sequence rule in [§6](#6-turns) drops duplicates;
5. the successor's first turn is the next index, not a restart at 0;
6. the table resumes when every connected peer has acked that turn and the roots match.

If the successor also vanishes before step 6, the next-lowest seat repeats the same steps. The game ends only when no human seat is connected ([§10](#10-drops-and-outages)). A join that was still receiving a snapshot restarts on the successor, as [§8](#8-join-in-progress) defines. That restart is not a further pause.

## 10. Drops and outages

Every number in this section is configuration. The defaults and the reasons:

| name | default | reason |
|---|---:|---|
| `turn_ms` | 100 | the original's turn period, [§6](#6-turns) |
| `command_delay` | 2 | the plan's default lead |
| `keepalive_ms` | 1000 | the original's in-game keep-alive |
| `resend_ms` | 50 | half a turn, [§4](#4-the-channels) |
| `wait_ms` | 2000 | the original shows the waiting dialog after 2 s without turns (`tick-loop.md` F9) |
| `grace_ms` | 60000 | D9, the seat goes to the AI after 60 s |
| `handover_ms` | 1000 | one keep-alive, [§9](#9-host-migration) |
| `turn_buffer` | 600 | `grace_ms / turn_ms`, so a peer that returns inside the grace can replay from the buffer when the gap fits |
| `hash_every` | 10 | the plan's one-second check |
| `hash_retry_ms` | 500 | the original's missing-digest poll |
| `full_check_min` | 10 | the original's 10-minute autosave |
| `payload_max` | 1200 | [§3](#3-the-packet-format) |
| `fragment_max` | 17130 | the largest measured save, [§4](#4-the-channels). A larger save is refused until this is raised |
| `snapshot_window` | 32 | [§4](#4-the-channels). Does not widen the 32-bit ack field |
| `snapshot_age_ms` | 60000 | a second joiner shares the image for one rolling-log interval, then the next ask gets a new save ([§8](#8-join-in-progress)) |
| `concurrent_transfers` | 2 | allocation is linear in this count; the third joiner is refused on the whole table, including when the running joins sit on two generations |
| `tail_turns_max` | 4096 | power of two at or above the admission pin for `endless02` ([§13](#13-budgets)) |
| `turn_store_cap` | 1200 | bytes charged per retained turn: `payload_max`, a cap. T45 did not replace it (OQ-282) |
| `catchup_ms` | 600000 | 10 minutes, the same real-time interval as `full_check_min`. A join still behind the host is restarted |
| `load_ms` | 0 | default `World::load` allowance. T45 measured 27–47 ms on one host and left this default (OQ-281) |
| `replay_turns_per_s` | 100 | default admission chase rate, 10× the live 10 turns/s. T45 measured faster turns on one host and left this default (OQ-281) |
| `loss_scale` | 2 | admission multiplies the zero-loss discrete completion by this. An allowance, not a measured multiplier |

A peer silent past `wait_ms` raises the wait dialog on the others and the host keeps sending turns into the buffer. The game is not ended. Past `grace_ms` the seat is marked computer: the AI seam on every peer produces that seat's commands (`state.md` §8.2 item 4), and nothing for that seat is required on the wire (D9). A rejoin inside or after the grace follows [§8](#8-join-in-progress) and the seat is returned to the human. If the missed turns are still in the rolling log, that log is enough. If they are not, the rejoin is a fresh save and a pin, and a cap that trips restarts the rejoining seat only.

`turn_buffer` is that rolling log. It is not the join tail. A pin keeps the turns a running join still needs after the rolling range has moved on, up to `tail_turns_max`. The host does not evict those turns to free the log, and it does not pause the table so the log will stop growing. `resend_ms` 50 in the table is the LAN setting. The 300 ms round-trip case uses 300, as [§4](#4-the-channels) says.

The original dropped a client after four unanswered 1 s keep-alives (`tick-loop.md` §5). That rule is not used. R14 says a short drop must not take the table down, and a 4 s silence is inside `grace_ms`.

An outage of a few seconds is the buffer and the resend. The join checker (§8.7, T52 F4) models it: a seated peer's reliable pair is never abandoned; no ACK for `wait_ms` is a logged outage state of that peer; the host keeps the turns in its log behind a per-pair send cursor and resends at `outage_resend_ms`; when an ACK arrives the peer catches up from the log; after `grace_ms` the seat goes to the AI by an ownership event and the pair closes. A 3 s two-way cut, a 600 ms cut at `resend_ms` 50, and the loss of the ACKs alone are regressions: none drops the peer. The `netsim` cases in [§14](#14-the-netsim-test-plan) are the check that 0.5–5 s does not drop a peer.

The session ends when the last human seat has been gone for `grace_ms`. One human leaving while others remain does not end it.

## 11. The transport interface

`net` talks to a `Transport`. LAN UDP is the first one. A later internet transport (relay or NAT traversal) is a second one. Sessions, turns and hashes call the interface and do not see sockets.

The interface is:

- `bind(port) -> result`
- `send(address, datagram)`
- `poll() ->` the datagrams received since the last call
- `broadcast(port, datagram)` for discovery, which the internet transport may implement as "send to the relay" or refuse with a named error

`address` is opaque bytes owned by the transport. The session stores it per seat and does not parse it. That is the seam: internet play adds an implementation, not a change to [§5](#5-the-session-states)–[§10](#10-drops-and-outages).

## 12. Security for LAN v1

v1 does not authenticate. Anyone on the LAN who can see the broadcast can ask to join, and the host's player list is the only acceptance. That is a stated limit, not an oversight: the plan's v1 is a trusted LAN.

What v1 does check:

- `protocol_version`, `kCommandWireVersion`, `kStateHashVersion`, the content hash and the setup digest, at the handshake;
- every length and count is refused before a buffer is sized to it: the packet header ([§3](#3-the-packet-format)), fragment `count` ([§4](#4-the-channels)), and the turn decoder (`state.md` §8.5, "checked before any allocation");
- `World::load` of a snapshot runs the save checks before it builds containers (`state.md` §9.5);
- the issuer of a command is the host's stamp, not a field the client is trusted to set.

Internet play adds, later, authentication of the seat and encryption of the datagrams. Those sit in the transport implementation. The session rules do not change. What they look like is not designed in this ADR.

## 13. Budgets

Measured with this worktree's `anno-sim-replay measure`, after 30 000 ticks (600 advances of 100 ms), 50 repetitions, `ANNO1503_GAME_DIR=E:\anno1503\game\app`. The clock is the tool's `steady_clock`. `src/sim` has none. Create marks every chunk dirty and the tick never calls `prepare_hash`, and `state_hash()` is const so it recomputes a dirty chunk on every call and does not clear the bit (`world.hpp`). `dirty_chunks_unprepared` equals the chunk count on each file, so that series is a full recompute. `prepare_hash` then fills the cache: the clean series only reads it. The incremental series is one `set_object` of the same id on each island's origin tile after that, so one dirty chunk per island (`dirty_chunks` below). Median of an even count is the integer average of the two middle samples. p95 is the nearest rank, `ceil(0.95 * 50)` = the 48th sample.

| Scenario | Unprepared µs min / median / p95 / max | Clean µs | Incremental µs | `save()` bytes | Tiles | Chunks | Dirty after one tile each |
|---|---|---|---|---|---|---|---|
| `0100Kampagne01/S01_07_01.sze` | 7778 / 7920 / 8591 / 8795 | 332 / 342 / 438 / 450 | 361 / 373 / 517 / 764 | 11 474 169 | 414 720 | 6 480 | 24 |
| `0011SingleEndlosAddon/endless02.sze` | 13678 / 14213 / 15109 / 15861 | 196 / 199 / 248 / 259 | 231 / 234 / 285 / 365 | 20 349 454 | 750 336 | 11 724 | 30 |
| `0010SingleEndlos/03_1024_med_256.sze` | 8624 / 8788 / 9404 / 9981 | 88 / 89 / 108 / 145 | 119 / 119 / 143 / 154 | 12 906 633 | 477 696 | 7 464 | 27 |

**Hash check.** `hash_every` is 10 turns and the turn is 100 ms, so the root is checked once per second. The cost of one check is the unprepared column when the caller has not run `prepare_hash` since the world was created: median 7.9 ms on `S01_07_01`, 14.2 ms on `endless02` (the largest file), 8.8 ms on the 1024-tile endless world. Per second that is the same number, once. After `prepare_hash` the check is the clean column, under 0.5 ms median on all three. One dirty chunk per island adds a few tens of microseconds on these files (the incremental column minus the clean column).

The cadence holds. The slowest sample is 15.9 ms (`endless02` unprepared max), which is 1.6 % of the one-second interval. A check does not have to wait for the snapshot: channel 1 is separate from channel 0 (§4).

**Snapshot.** `save().size()` is the byte column. At line rate, counting save bytes only and ignoring headers and the window: `S01_07_01` is 0.918 s at 100 Mbit/s and 0.092 s at 1 Gbit/s; `endless02` is 1.628 s and 0.163 s; `03_1024_med_256` is 1.033 s and 0.103 s. Those figures are payload bounds. They are not measured transfer times.

A fragment carries 1188 save bytes (§4). The three saves need 9 659, 17 130 and 10 865 fragments. `fragment_max` 4096 admits 4 866 048 bytes and refuses all three, so none of these snapshots is transferable under that cap. The times below assume the proposed cap of 17 130, which does admit them.

The in-flight window is 32 fragments, 38 016 save bytes (§4). The fluid bound is ideal and full-window: the window stays full, nothing is lost or resent, and one window completes per RTT. It counts save bytes only. It is not a measured join or resync. The discrete clock later in this section is a separate model result.

[§14](#14-the-netsim-test-plan) defines latency as one-way delay, so the plan's "150 ms latency" is 150 ms each way and a 300 ms RTT. `endless02` then takes 17 130 / 32 × 0.300 s = 160.6 s. If that 150 ms is instead the RTT, it takes 17 130 / 32 × 0.150 s = 80.3 s. The same model gives 90.6 s and 101.9 s for the other two saves at 300 ms RTT, and 45.3 s and 50.9 s at 150 ms RTT (`S01_07_01`, then `03_1024_med_256`).

`turn_buffer` is 600 turns, 60 s at the 100 ms period (§10). A live join does not pause the other peers (§8), so turns keep being issued during the send. At 160.6 s that is about 1 606 turns, and at 80.3 s about 803 turns. Either is more than 600, so for `endless02` the rolling buffer drops the earliest turns of the snapshot's interval before the joiner has the snapshot. At a 150 ms RTT the other two ideal snapshot times are under 60 s (about 453 and 509 turns). At 150 ms one-way they are not (about 906 and 1 019 turns). A time under 60 s is only the snapshot. The turns the joiner must still receive, the headers, loss, `World::load` and `run_turn` are outside the bound.

§7 stores a snapshot every 10 minutes and does not send it. A join or a resync sends a save taken at the moment [§8](#8-join-in-progress) and [§7](#7-hash-checks) name, plus the turns after that boundary. The 10-minute image is older than the 60 s log, so it is not the payload.

**T43 model.** `tools/snapshot_catchup.py` is an integer discrete-event model of that proposal. It is not a socket, and it is not a measured network. `python -I tools/snapshot_catchup.py` prints the rows below; `python -I tools/test_snapshot_catchup.py` checks them. Assumptions A1–A13 are in the tool. A8 is still a fluid channel-0 receipt with no second one-way delay. A12 still drops data only on the 2% rows. A13 is the owner's runtime cap on `Table.advance`. None of those is an end-to-end measured time.

The fluid tenths above stay the T41 F1 quotients: 45.3 s, 80.3 s and 50.9 s at a 150 ms round trip, and 90.6 s, 160.6 s and 101.9 s at a 300 ms round trip, for `S01_07_01`, `endless02` and `03_1024_med_256`. The discrete clock is a different quantity. With a burst of 32, no serialization delay and no loss, fragment `n-1` arrives at `((n-1)//32) × RTT + one_way`:

| Scenario | 150 ms round trip | 300 ms round trip |
|---|---:|---:|
| `S01_07_01` | 45 225 ms | 90 450 ms |
| `endless02` | 80 325 ms | 160 650 ms |
| `03_1024_med_256` | 50 925 ms | 101 850 ms |

One window of 32 payloads of 1 200 bytes serializes in 3 072 µs at 100 Mbit/s and 307 µs at 1 Gbit/s. Both sit under a 150 ms one-way delay, which is why the event clock omits serialization (assumption A3). The line-rate save-byte times earlier in this section are the other bound, and on these sizes the window's round trip is the slower one.

Admission multiplies the 300 ms zero-loss completion by `loss_scale` 2, with `load_ms` 0 and `replay_turns_per_s` 100. The reserved pin lengths are 2 010, 3 570 and 2 264 turns. `tail_turns_max` is the power of two at or above the largest, 4 096. At `turn_store_cap` 1 200 that pin is 4 915 200 bytes. The rolling log at the same cap is 600 × 1 200 = 720 000 bytes. The T45 labeled rates are a later subsection. They do not replace this row.

The allocation bound, with `concurrent_transfers` 2, `max_generations` 2 and `fragment_max` 17 130, is the sum of the caps. Each joiner's received-turn backlog is `receive_backlog_turns` × 1 200, and the channel-0 window that delivers those turns is `snapshot_window` × 69 bytes per joiner. The snapshot store, reassembly, channel-1 windows, host pin and rolling log sum to 87 113 760 bytes. That figure omits the backlog and the channel-0 flight. The total includes both.

| Part | Bytes |
|---|---:|
| Snapshot store, two generations at the fragment cap | 40 700 880 |
| Reassembly, two joiners at the fragment cap | 40 700 880 |
| Channel-1 windows, 2 × 32 × 1 200 | 76 800 |
| Channel-0 flight, 2 × 32 × 69 | 4 416 |
| Pinned turns, one shared host pin | 4 915 200 |
| Joiner backlog, 2 × 4 096 × 1 200 | 9 830 400 |
| Rolling log | 720 000 |
| Total | 96 948 576 |

T43 model constants, acceptance case: tail_turns_max=4096, tail_bytes_max=4915200, allocation_bound_bytes=96948576, joiner_backlog_bytes=9830400, channel0_flight_bytes=4416, endless02_discrete_zero_loss_ms=160650, counterexample_1188_done_ms=160, counterexample_1188_replayed=1.

Seed 1 at 2% loss (20 000 per 1 000 000) drops data datagrams only. Acknowledgements are not dropped on this row, and a duplicate arrival does not generate an acknowledgement here. Acknowledgement loss, and a duplicate that does generate an acknowledgement, are separate schedules in `tools/snapshot_catchup.py`. This row is a model result for that seed. It is not a measured join, and it is not general datagram-loss coverage.

The caught-up column is the replayed cursor. It equals the host cursor at that millisecond. `S01_07_01` and `endless02` already met that test at the times below. `03_1024_med_256` meets it at 141 160 ms on turn 1 411.

| Scenario | Transfer ms | Caught up ms | Replayed turns | Host turns |
|---|---:|---:|---:|---:|
| `S01_07_01` | 113 250 | 125 830 | 1 258 | 1 258 |
| `endless02` | 198 750 | 220 830 | 2 208 | 2 208 |
| `03_1024_med_256` | 127 050 | 141 160 | 1 411 | 1 411 |

T43 model constants, seed 1: endless02_loss2_seed1_transfer_ms=198750, endless02_loss2_seed1_done_ms=220830, endless02_loss2_seed1_pin_turns=2208, 03_1024_loss2_seed1_done_ms=141160, 03_1024_loss2_seed1_replayed=1411.

The seed-1 pins sit under the ×2 admission allowance. A worse draw that passes `tail_turns_max`, `catchup_ms` or `max_attempts` (8) restarts that joiner. Two labeled synthetic sizes, `2 × 20 349 454 = 40 698 908` bytes (34 259 fragments) and `4 × 20 349 454 = 81 397 816` bytes (68 517 fragments), are not measured worlds. At `fragment_max` 17 130 both are refused before a transfer. Raising the fragment cap to 34 259 and leaving the other defaults refuses the double on `tail_turns` (the allowance asks for 7 137 turns). Raising `tail_turns_max` to 8 192 as well still refuses it, because the ×2 completion is 642 300 ms and `catchup_ms` is 600 000. The quadruple allowance is 14 277 turns and fails the same two caps. Growing a world past the measured save is a configuration change of more than one knob, and the joiner is the seat that is refused.

The one-second hash is not this transfer. Channel 1 is separate from channel 0.

**OQ-251 is closed** by the `S01_07_01` row: one `state_hash()` is 7.9 ms median with every chunk dirty and 0.34 ms median after `prepare_hash`, and `save()` is 11 474 169 bytes. The status of this ADR stays Proposed until the owner approves it.

An empty turn payload is 8 + 4 + 4 = 16 bytes. With the 25-byte header and 28 bytes of IPv4+UDP, one datagram is 69 bytes. That 69 is this empty datagram. A turn whose commands are the T45 1 216-byte burst is two fragments, accounted in [§8.7](#87-ordered-join-protocol-ownership-events-and-queue-bounds-t52-f2). At 10 turns per second that is 690 bytes/s from the host to each other peer. At 6 peers the host sends to 5 others: 3 450 bytes/s, plus one hash report of the same order once per second per peer. A single placement's `TurnEntry` is a few dozen bytes (`state.md` §8.5), so a command on every turn from every seat stays in the same thousands of bytes per second. The snapshot is the save measured above, and it is sent on a join or a resync, not on the one-second hash. The steady-state figure does not include it.

### T45 measurement (2026-10-27)

This subsection does not replace the T41 table. The unprepared `state_hash` medians stay 7.9 / 14.2 / 8.8 ms. The T41 save sizes stay 11 474 169, 20 349 454 and 12 906 633 bytes. `anno-sim-replay join` remeasured `save()` on stream 114 at the same 30 000-tick age. Each file is one byte larger: 11 474 170, 20 349 455 and 12 906 634. The fragment counts stay 9 659, 17 130 and 10 865, so `fragment_max` 17 130 still admits the remeasured `endless02` save. The extra byte was not attributed to a field.

The host is an AMD Ryzen 7 9800X3D, 8 cores and 16 logical processors, Windows, MSVC 19.51 Release (`msvc_ver` 1951, `NDEBUG`). The tool reported `threads=16`, `create_executor=threads`, `load_executor=none`, `clock=steady_clock` and `timing_includes_hash=0`. Warm-up was 2. Load used 20 repetitions. Replay used 5 repetitions of 30 turns, 150 samples. These times are this host only. They are not a weaker machine and not a network.

A tick is 2 game ms. The age is 30 000 ticks from tick 0, reached by 600 calls to `World::advance(100)`. One multiplayer turn in the tool is `run_turn` with `step_ms` 10 and `steps` 10: 100 game ms, 50 ticks. The live rate in the model stays 10 such turns per second. `World::advance` only ages the measured world. The timed replay is `run_turn`.

`World::load` below is the load of bytes already in memory. File I/O, the registry, templates, scenario parsing, world creation and `save()` were timed as separate one-sample phases and are not this distribution. `docs/perf/README.md` lists those phases. Each timed load's hash matched the source. Each timed replay matched an untimed `run_turn` reference, and that reference matched apply-then-advance. A failed check prints `join validation=fail` and no timing line. All three runs printed `validation=ok`.

| Scenario | Path under `game/app` | `World::load` µs min / median / p95 / max | Ceil of median |
|---|---|---:|---:|
| `S01_07_01` | `Scenes/0100Kampagne01/S01_07_01.sze` | 24 335 / 26 807 / 27 702 / 27 977 | 27 ms |
| `endless02` | `Scenes/0011SingleEndlosAddon/endless02.sze` | 42 858 / 46 004 / 51 401 / 54 239 | 47 ms |
| `03_1024_med_256` | `Scenes/0010SingleEndlos/03_1024_med_256.sze` | 25 018 / 27 647 / 31 175 / 32 570 | 28 ms |

The empty workload is `run_turn` with no commands. Accepted 0 is that empty script. Rejected was 0. The scripted workload is one `SetSpeed` per turn from host player 0, percent cycling through 50, 100 and 200. All 30 were accepted and `speed_percent` matched. The burst is one synthetic turn: `SetSpeed` records grown until the encoded stream passes 1 200 bytes. 75 records are 1 200 bytes and are not over the cap. 76 records are 1 216 bytes. All 76 were accepted. It is not a play distribution.

| Scenario | Workload | µs min / median / p95 / max | Duration µs | Turns/s, median floor | Turns/s, sum | Turns/s, p95 floor | Turns/s, max floor |
|---|---|---:|---:|---:|---:|---:|---:|
| `S01_07_01` | empty | 232 / 604 / 1 473 / 1 616 | 104 088 | 1 655 | 1 441 | 678 | 618 |
| `S01_07_01` | scripted | 232 / 659 / 1 627 / 1 966 | 113 867 | 1 517 | 1 317 | 614 | 508 |
| `endless02` | empty | 237 / 426 / 976 / 2 278 | 80 699 | 2 347 | 1 858 | 1 024 | 438 |
| `endless02` | scripted | 234 / 397 / 925 / 2 222 | 78 319 | 2 518 | 1 915 | 1 081 | 450 |
| `03_1024_med_256` | empty | 171 / 214 / 288 / 533 | 33 791 | 4 672 | 4 439 | 3 472 | 1 876 |
| `03_1024_med_256` | scripted | 171 / 217 / 330 / 635 | 35 330 | 4 608 | 4 245 | 3 030 | 1 574 |

Encoded sizes are `TurnEntry` bytes only. The same three numbers came out of every scenario. The empty command stream is 0 bytes. One scripted record is 16 bytes: issuer `u32`, sequence `u32`, type `u16`, length `u16`, percent `u32`. The burst is 76 commands and 1 216 bytes. The `std::vector` capacity on this MSVC build was 0, 16 and 1 228 bytes. That capacity is allocator storage, not a budget. The tool also printed the ADR header of 25 bytes, IPv4+UDP of 28 bytes and the ADR empty payload of 16 bytes, marked `packet_overhead=adr_proposal` and `not_a_packet=1`. Those bytes were not added to the payload, and the tool does not write a packet. The scripted 16-byte record is a different quantity from the 16-byte empty datagram defined above.

`proposed_budgets()` stays `load_ms` 0, `replay_turns_per_s` 100 and `turn_store_cap` 1 200. `tools/snapshot_catchup.py` feeds each scenario through `replace()`: the ceil of the load median, and one measured rate at a time. Every labeled row admits with `within_budgets`. The admission turn gap is unchanged, because 27, 47 and 28 ms do not cross a 100 ms boundary on these estimates: 1 809, 3 213 and 2 037 turns. The reserved pin shrinks. The scripted sequence rates ask for 1 823, 3 230 and 2 042 turns, against the baseline 2 010, 3 570 and 2 264. The slowest measured turn is the empty-workload maximum on `endless02`, 2 278 µs, 438 turns/s. That row asks for 3 289 turns. `tail_turns_max` 4 096 still holds it. Zero-loss catch-up still succeeds, and that slowest row's pin is 1 656 turns against the baseline 1 784. Seed 1 at 2% data loss keeps the published transfer times 113 250, 198 750 and 127 050 ms, because the fragment counts did not change. The replayed cursor still meets the host. On `endless02` that meeting is 200 805 ms on turn 2 008 at the scripted sequence rate, and 204 944 ms on turn 2 049 at 438 turns/s, against the baseline 220 830 ms on turn 2 208.

A8 and A12 are unchanged. Assumption A10 still applies: no host processing is added beyond `load_ms` and the replay rate. This is not an end-to-end join, not a real network, and not a claim about another machine. Real-play turn sizes were not measured.

Headroom, with the defaults left as they are:

- Keep `replay_turns_per_s` at 100. It is slower than every measured summary on this host, including 438 turns/s, so it reserves the larger pin.
- Keep `load_ms` at 0. The slowest ceil-of-median load is 47 ms and it adds no admission turn on these three saves. An allowance of 50 ms would cover that median on this host. It is not adopted.
- Keep `turn_store_cap` at 1 200. The measured empty stream is 0 bytes and the scripted turn is 16. The synthetic burst of 1 216 bytes shows that 1 200 is not a size every encoded turn stays under. The cap is not lowered, and it is not a universal bound.

T45 model tokens: t45_load_ms_ceil_median=27/47/28, t45_endless02_empty_max_replay=438, t45_endless02_empty_max_need=3289, t45_gap_turns_unchanged=1809/3213/2037, t45_burst_payload=1216, t45_empty_payload=0, t45_script_payload=16. OQ-300 through OQ-309 were not used.

### T47 causal readiness (2026-10-27)

`evaluate_causal` in `tools/snapshot_catchup.py` is the [§8.6](#86-causal-readiness) clock. `evaluate_join` is unchanged and remains the optimistic fluid comparison. `proposed_budgets()` is unchanged. A14 does not replace A8 or A12. The 2% rows still drop data only. An ACK drop is a separate schedule: on a 12-turn probe it leaves turn 1's arrival at 250 ms, and the one-fragment join still enables input at 1 160 ms. Recomputed after the due-prefix check, the zero-loss rows below are unchanged. A snapshot that loads at 3 000 ms with turn 11 missing is not success at 3 100 ms (arrived and applied 10, host 31, due prefix 29). The same snapshot with turn 11 delayed by 2 000 ms enables input at 4 160 ms on turn 40.

Default rate, T41 save bytes, zero loss. The host column is the causal host-applied frontier. The fluid column is the optimistic equality cursor from the same snapshot transfer.

| Scenario | Snapshot ms | Ready ms | Input ms | Applied | Host | Fluid ms | Fluid turns |
|---|---:|---:|---:|---:|---:|---:|---:|
| `S01_07_01` | 90 450 | 100 480 | 101 160 | 1 010 | 1 011 | 100 490 | 1 004 |
| `endless02` | 160 650 | 178 480 | 179 160 | 1 790 | 1 791 | 178 490 | 1 784 |
| `03_1024_med_256` | 101 850 | 113 140 | 113 160 | 1 130 | 1 131 | 113 160 | 1 131 |

`03_1024_med_256` is the split that shows the two clocks are not the same test. Both name 113 160 ms. The fluid cursor is turn 1 131 and the causal cursor is turn 1 130, because turn 1 131 was produced at 113 100 ms and does not arrive until 113 250 ms. Tokens: causal_endless02_done_ms=179160, causal_endless02_applied=1790, causal_endless02_host=1791, causal_03_same_ms_applied=1130, causal_03_same_ms_fluid=1131.

Every host span above is under `tail_turns_max` 4 096, and every input time is under `catchup_ms` 600 000. The peak received-but-unapplied backlog is 903, 1 605 and 1 017 turns, each under `receive_backlog_turns` 4 096. The reserved admission pins 2 010, 3 570 and 2 264 are not these spans. They stay the upper bound.

The T45 rates are `replace()` copies. The slowest row is still `endless02` empty-max, 438 turns/s, load 47 ms. Admission still asks for 3 289 turns. The gap is still 3 213. The fluid pin on that row is still 1 656, against the default-rate fluid pin 1 784. The causal boundary on that same row enables input at 166 153 ms, applied turn 1 660, host turn 1 661. Token: t45_endless02_empty_max_causal_applied=1660, t45_endless02_empty_max_fluid_pin=1656. The other labeled rates also reach the boundary with applied one turn behind the host. None of them changes a default.

Seed 1 at 2% data loss keeps the snapshot transfer times 113 250, 198 750 and 127 050 ms. Channel 0 draws its own LCG from the same seed over the 6 010-turn catch-up horizon (`catchup_ms / turn_ms + max_attempts + 2`) and drops 128 data sends on that horizon. It does not drop acknowledgements. `endless02` at 438 turns/s then enables input at 205 153 ms, applied turn 2 050, host turn 2 051. The fluid comparison on that transfer still meets at 204 944 ms on turn 2 049. The host span 2 051 stays inside 4 096.

What this establishes is the boundary, the refusal rules, and that these measured inputs fit the existing caps without pausing the table. What it does not establish is a shared link, jitter, the channel-2 hash's own flight, another machine, or a real play. [§8.7](#87-ordered-join-protocol-ownership-events-and-queue-bounds-t52-f2) is the design for the link and the hash flight. OQ-320 stays open until `netsim` or a measured link tests it. No socket is added.

T52 F2 does not recompute this table and does not change `load_ms`, `replay_turns_per_s` or `turn_store_cap`. The input column is the model gate. 1 160 ms and 179 160 ms are model comparisons from `catchup_state_at`; they are not rows of the §8.7 protocol (whose `endless02` row opens input at 179 560 ms on the checker's own 10 ms replay step). The exact `endless02` datagram total is in §8.7. The 96 948 576 byte figure above is memory. It is not the transmit-queue cap in §8.7.

## 14. The `netsim` test plan

`netsim` runs N peers in one process. The link between them can delay, jitter, drop, reorder and disconnect. Each human seat has a command bot. No game files are required for the cases below; they build a world the sim can construct without `Anlagen.dat`. The scenario measurement of [§13](#13-budgets) stays a separate, manual run.

**Latency.** Latency in this plan is the one-way delay the link adds to each datagram, including each turn. A fluid bulk receipt is not this delay. It is not the round trip. The acknowledgement comes back under the same delay, and the model adds no processing time, so the RTT is twice the latency. The acceptance line "150 ms latency" is 150 ms one-way and 300 ms RTT. [§13](#13-budgets) also records the transfer bound for the other reading, 150 ms as the RTT, because the earlier text did not say which it was. The §13 fluid figures and the discrete completions are payload bounds and model times. They are not measured end-to-end join or resync times. Case 2 loads the fresh image taken after the pause ([§7](#7-hash-checks)), which has an empty tail. Case 4 uses the pin ([§8](#8-join-in-progress)). Neither case is satisfied by `fragment_max` 4096, and neither case treats the 60 s rolling log as the join tail.

The model already runs the retention cases this harness will have to reproduce once `net` exists: a 10-minute image is unavailable, not a successful join; a fresh `endless02` image whose transfer passes 60 s succeeds only with the pin; a skipped hole is a restart; a reordered prefix waits for the contiguous sequence; two joiners share a generation and one cancel leaves the other's pin; three joiners on two generations stop at the global cap of two; a synthetic save past the caps is a refusal or a restart; a replay rate below the live rate does not catch up. Those runs set `paused` false on every join outcome. The mismatch resync is the path that sets it true. `netsim` itself is not implemented here, and this model is not that harness.

From W3's acceptance (`docs/plan/lan-endless.md`):

1. 6 peers, 150 ms latency, 2 % loss, 10 000 turns. The roots match at every check.
2. One peer's world is changed under the test. The mismatch is seen within one `hash_every` interval, `diff_trees` names a path, the snapshot loads, and the roots match again.
3. A peer is dropped and rejoins inside `grace_ms`. It catches up and the roots match. The seat is the same seat.
4. A new peer joins a running game and takes a free seat. The pass condition is the protocol of [§8.7](#87-ordered-join-protocol-ownership-events-and-queue-bounds-t52-f2), run through the ordered transport: the host has accepted a report for the selected `H` compared at that same turn; the ownership event is in turn `A = C + command_delay` and every peer, including a second joiner and a restarted joiner, replays it with identical roots at matching turns; and the joiner opens input only on a delivered `InputGrant` with received minus applied at most `input_lag_max`. Delivered but unapplied turns are kept. The first accepted command is stamped `current + command_delay`. The others do not pause. Cases: a lost watermark, checkpoint or report is repaired by the channel-0 resend and later messages on that pair wait behind it; a checkpoint at its two-interval deadline restarts that joiner only (the exact-deadline millisecond does not accept, one millisecond earlier does); a report for a turn other than the selected `H` does not accept; a turn lost after `H` does not open input stale; two overlapping joins on one generation and on two generations keep both attempts; a cancel after `A` releases the seat on every peer; and a 1 216-byte burst every turn with ACKs blocked stays inside the 32-sequence window.
5. The host is killed at 20 turns chosen by the test. Each time the lowest remaining seat becomes host and the roots match afterwards.
6. The link is cut for 0.5 s, 2 s and 5 s. No seat is given to the AI (all three are inside `grace_ms`), and every peer catches up from the buffer.
7. A long run, the plan's 8-hour 6-peer session, is not part of the ordinary test. It is the W9 run. `netsim` keeps the same assertions so that run is the same harness with a longer limit.

Two real PCs on a LAN finding each other is a person test (`docs/plan/lan-endless.md` W3), not a `netsim` case.

## Open choices

- **OQ-250.** The UDP port. No original value was found in `tick-loop.md`. No default is set.
- **OQ-251.** Closed in [§13](#13-budgets): `S01_07_01` `state_hash()` is 7.9 ms median with every chunk dirty and 0.34 ms after `prepare_hash`; `save()` is 11 474 169 bytes.
- **OQ-280.** Narrowed by the T43 proposal in [§8](#8-join-in-progress) and [§13](#13-budgets), and by the round-bs correction of the cursors, the global caps and the byte total. The recommended contract is a fresh save at the ask and a pin of the turns after it. The joiner reaches the receivable-prefix boundary in §8.6; that boundary is the model gate. §8.7 installs the seat later, at `A = C + command_delay`, and does not require the cursors to sit on `H` at that instant. Equality with the current host cursor is only the older fluid comparison. A cap trips only that joiner. The 60 s log stays the short-outage buffer. The periodic image is an availability check, not the join payload. The table is not paused for a join. Still for the owner, because this ADR is Proposed: the budget defaults (`tail_turns_max` 4096, `catchup_ms` 600000, `concurrent_transfers` 2, `snapshot_age_ms` 60000, `loss_scale` 2, acceptance `resend_ms` 300). OQ-281 and OQ-282 are narrowed by T45 and their defaults stay in that list. OQ-283 through OQ-289 were not used.
- **OQ-281.** Narrowed by the T45 measurement in [§13](#13-budgets). `World::load` and `run_turn` were timed on one host. The model defaults stay `load_ms` 0 and `replay_turns_per_s` 100. Other machines are unmeasured.
- **OQ-282.** Narrowed by the same measurement. An empty command stream is 0 bytes, one scripted `SetSpeed` is 16, and a synthetic burst is 1 216. The pin still charges `turn_store_cap` 1200. Real-play sizes are unmeasured, and 1200 is not a universal bound.
- **OQ-320.** Narrowed, still open. T52 F2 checks the join protocol event by event (`tools/join_activation.py`): ordered channel-0 delivery with resends and ACK-only datagrams, ownership events in the turn stream, input behind a fresh grant, and bounded queues. It is a protocol oracle with symbolic roots and a fixed one-way delay. A `netsim` case with real jitter, shared-link serialization and loss, or a measured link, is still required. The F1 rows and F1 "all MET" claims are superseded. OQ-321 through OQ-329 were not used.
- **OQ-370.** Narrowed by T52 F2, still open as part of approving this Proposed ADR. `A = H + command_delay` stays withdrawn. The rule is `A = C + command_delay` with `C` the host turn at the accept millisecond; `command_delay` stays 2. The new part is that ownership at `A` is an event in turn `A`, and input is a separate step behind `InputGrant` and `input_lag_max`. The owner accepts the future-turn rule, the event-in-turn design and the release event by approving the ADR.
- **OQ-371.** Open. Jitter and serialization on the shared link are unmeasured. The §8.7 rows use zero jitter and a fixed 150 ms one-way delay; serialization is not scheduled in the checker. `resend_ms` 300 is the acceptance value. OQ-372 and OQ-373 are new; OQ-374 through OQ-379 were not used.

The command delay of 2, the 100 ms turn, the 60 s grace and the 10-minute full check are the plan's and the original's defaults. They are configuration, not new decisions.

- **OQ-372.** Open. The wire form of an ACK-only datagram (design assumption: payload length 0, no sequence consumed, the `sequence` field repeats the next sequence) and the encoded size of the T52 messages and of an ownership `TurnEntry` (14 bytes). These are design assumptions in §8.7, not wire commitments.
- **OQ-373.** Open. The design limits `input_lag_max` 6, `input_host_lag_max` 6 (plus one round trip of turns), `max_outstanding_wm` 4, `max_root_pins` 2, `wait_cap` 64 datagrams, `input_ready_ms` 20 000, `select_margin_ms` 100, `deadline_rtts` 2, `rtt_initial_ms` 1 000 and `seated_lag_max` 20 turns, with the reasons in §8.7, and two domains the checker found (T52 F3): replay at most 73 ms per 100 ms turn on `endless02` (the catch-up arithmetic against `catchup_ms` 600 000 ms), and a one-way delay up to 1 500 ms in the normal row (the 32-datagram window at 10 turns per second ends it at about a 3.2 s round trip). None is adopted as an engine default.