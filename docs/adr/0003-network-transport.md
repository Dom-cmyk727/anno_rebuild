# ADR 0003: Network transport and sessions

- **Status:** Proposed
- **Date:** 2026-10-26. Corrected 2026-10-27 (round bq F1): the fragment cap and the window time. Retention proposal 2026-10-27 (T43). Round bs F1, 2026-10-27: progress cursors, global caps, joiner backlog, acknowledgement loss. Round bt F2, 2026-10-27: the shared owner enforces tail and deadline caps on each host advance, before the new turn is retained. T45 measurement 2026-10-27: labeled `World::load` and `run_turn` costs, and encoded turn sizes. T47 F3, 2026-10-27: channel-0 arrivals during load and replay enter the backlog through `Table.note_backlog`. T52, 2026-10-27: observable join activation, hash-report delivery and a shared-link schedule ([§8.7](#87-observable-activation-and-the-shared-link)). The model and the defaults below are unchanged. Status stays Proposed.

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
| 25 | bytes | `payload`, exactly `payload_len` bytes, and no trailing byte |

`ack` and `ack_bits` together name 33 sequences: `ack` itself and the 32 before it. That is the selective acknowledgement in [§4](#4-the-channels). The highest contiguous sequence stays on the peer. An ordered channel delivers payload only through the first missing sequence. Putting that contiguous value in `ack` would leave a packet received ahead of a hole unnamed, so the sender would not see the loss until the hole was already gone.

The header is 25 bytes. `payload_len` above the configured maximum is refused before the payload is copied. The maximum is `net.payload_max`, default 1200. Reason: an Ethernet frame of 1500 bytes minus a 20-byte IPv4 header and an 8-byte UDP header leaves 1472, and 1200 stays under that with room for options, so the LAN path does not depend on IP fragmentation. It is configuration because a later internet path may need a smaller value.

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

Cancel or disconnect frees that joiner's reassembly buffer and backlog and drops that joiner's reference. The pin remains while any join that still needs the turn is running, including a turn the rolling log has already left, so one cancel does not evict the other joiner's turns. A released slot can be taken by a later ask. When the last joiner who needs a turn finishes or leaves, that turn follows the rolling rule again. Completion means the joiner has applied through the checkpoint `H` of [§8.7](#87-observable-activation-and-the-shared-link), the hash report for that turn matches, and `SeatActive` has named `A`. The receivable prefix in [§8.6](#86-causal-readiness) is the model form of that checkpoint. The joiner then receives channel 0 with the rest of the table. The fluid equality in [§8.3](#83-budgets) is the optimistic comparison, not this completion. The reassembly buffer and the backlog are freed at that point. Turns received on channel 0 during the transfer sit in that backlog until replay, and [§13](#13-budgets) charges them.

On host migration ([§9](#9-host-migration)) an in-progress joiner is not a connected seat. The successor is not handed this generation's save. Each running join restarts on the new host, which writes its own fresh image. The §9 pause is the role change the plan already specifies. The restart does not add a pause. Peers who already have the world keep it; they use the rolling log and the resend §9 already describes.

### 8.5 Procedure

1. Admit or refuse under §8.3. A refusal ends at the ask.
2. Share an open generation when §8.4 allows it. Otherwise save at the current boundary `S` and open the pin at `S+1`.
3. Send the image on channel 1. Send turns `S+1` onward on channel 0 during that transfer. A full channel-1 window does not delay channel 0.
4. The joiner `load`s the image, then `run_turn`s from `S+1` until [§8.6](#86-causal-readiness) says the receivable prefix is applied. `load_ms` and `replay_turns_per_s` are the model's names for those two costs. The defaults stay 0 and 100. T45 measured both on one host and records them as a labeled scenario in [§13](#13-budgets). The defaults were not replaced (OQ-281). The fluid equality with the host cursor stays the optimistic comparison in [§8.3](#83-budgets).
5. The joiner sends commands only after the checkpoint in [§8.7](#87-observable-activation-and-the-shared-link): applied and received both reach the named turn `H`, the hash report for that same turn matches, and the host has named a later activation turn `A`. A mismatch or an expired report for the joiner alone restarts the joiner. `paused` stays false. A mismatch that includes a peer already in the game is [§7](#7-hash-checks).
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

Channel 1 keeps the window of 32. One window of 32 payloads of 1 200 bytes serializes in 3 072 µs at 100 Mbit/s. One channel-0 datagram of 69 bytes serializes in 6 µs. The sum, 3 078 µs, is under the 150 ms one-way delay, which is why this clock omits both. The channels are not placed on one queue. A shared link, jitter, and the flight of the channel-2 hash datagram are not in the model. [§8.7](#87-observable-activation-and-the-shared-link) is the design for those three. It does not change this clock, and OQ-320 stays open until a `netsim` case or a measured link tests it. The fluid quotients are not recomputed: `17 130 * 150 = 2 569 500`, and `2 569 500 = 32 * 80 296 + 28`, so the 150 ms-RTT figure remains 80.3 s; `17 130 * 300 = 5 139 000 = 32 * 160 593 + 24`, so the 300 ms-RTT figure remains 160.6 s.

The one-fragment row makes the two clocks visible. The snapshot fragment arrives at 150 ms. The fluid comparison then applies turn 1 at 160 ms (`counterexample_1188_done_ms=160`). On the causal clock that turn is not receivable until 250 ms, readiness with an empty prefix is 150 ms, and input is enabled at 1 160 ms on turn 10 while the host is on turn 11. Tokens: causal_one_fragment_arrival_ms=250, causal_one_fragment_done_ms=1160. Those tokens are the 600 000 ms deadline. They are not the 1 160 ms deadline above.

Admission is unchanged. `J` is still the reserved upper bound, not the applied frontier. The defaults `load_ms` 0, `replay_turns_per_s` 100 and `turn_store_cap` 1200 are unchanged. The numbers below are model output from those inputs and from the T45 labeled rates. They are not a measured join.

### 8.7 Observable activation and the shared link

[§8.6](#86-causal-readiness) is a model clock. Its due prefix and its boolean hash match are inputs of `evaluate_causal`. They are not messages. This section names the messages, the hash-report lifetime, and a bounded schedule for one shared link. `tools/snapshot_catchup.py` is unchanged. Every millisecond below that comes from that model is cited as model output. Every other millisecond is design arithmetic from the constants already in this ADR. Jitter is unmeasured, and the arithmetic rows use none (OQ-371).

**Identity.** `session_id` is the field in [§3](#3-the-packet-format). A generation is still the save at `S` plus the pin after it ([§8.4](#84-who-owns-the-bytes)). The host assigns that generation a `u32 generation_id` when it saves. The fragment head's existing `u32 id` carries this value, with `index` and `count` unchanged. A new save, including a restart and a host migration, is a new id. A message whose id is not the joiner's current generation is kept only as a stale record.

**Watermarks.** `JoinGrant`, `JoinWatermark`, `JoinCheckpoint` and `SeatActive` travel on reliable channel 0. `HashReport` travels on unreliable channel 2, and the retry below repeats it on channel 0. Nobody treats the joiner's applied turn as equal to the host's current turn.

| Message | Direction | Fields the other side learns |
|---|---|---|
| `JoinGrant` | host → joiner | `generation_id`, snapshot turn `S`, seat |
| `JoinWatermark` | joiner → host | `generation_id`, `received_through`, `applied_through` |
| `HashReport` | either way | `generation_id`, turn index, `state_hash` root |
| `JoinCheckpoint` | host → joiner | `generation_id`, the fixed hash turn `H`, the activation turn `A` |
| `SeatActive` | host → joiner | `generation_id`, seat, `A` |

`received_through` is the joiner's contiguous channel-0 prefix. `applied_through` is how far `run_turn` has gone, and it stays at or behind `received_through`. The host's produced turn is the turn index on each ordinary channel-0 turn ([§6](#6-turns)). The joiner reads that index from the turn it received. It does not subtract its own cursor from the host's to decide readiness.

**Checkpoint.** `H` is a turn the host has already applied, `H > S`, and `H - S` is a multiple of `hash_every` (default 10). The host chooses `H` and sends it in `JoinCheckpoint`. The joiner does not infer `H` from the host's current turn. `A = H + command_delay`. With the default delay of 2, `A = H + 2`. The host stamps a joiner command for `current + command_delay` and refuses a stamp below `A`. That is the same assignment rule as [§6](#6-turns). OQ-370 records that `A = H + command_delay` is this proposal, waiting with the rest of this ADR.

**When input opens.** All four are required.

1. The joiner has loaded `generation_id`.
2. `applied_through = received_through = H`, so the prefix `S+1` through `H` has no hole.
3. A `HashReport` for this `generation_id` and this same `H` matches the local root at `H`. A report for any earlier turn, including an earlier multiple of `hash_every`, does not open input.
4. `SeatActive` for this generation has named `A`, and the host has accepted the joiner's report for `H`.

The joiner may send a command after step 3. The host accepts it only after step 4, and the accepted command lands on `current + command_delay`, which is at least `A`. Turns between `H + 1` and that stamped turn carry the seat's ordinary empty or AI commands. Peers already in the game keep advancing. `paused` stays false.

**Hash lifetime.** The channel-2 payload is `generation_id` (`u32`), the turn (`u64`) and the root (`u64`): 20 bytes. Channel 2 is unreliable ([§4](#4-the-channels)). The same three fields are what the channel-0 retry carries after `hash_retry_ms` (default 500).

| Arrival | What the receiver does |
|---|---|
| Before `applied_through` reaches `H` | Store the report under (`generation_id`, turn). Compare it when apply reaches that turn. Only the stored report for `H` can open input. |
| After apply has reached `H` | Compare on arrival. |
| Duplicate of the same id, turn and root | Ignore. |
| Turn other than `H`, or an older generation | Do not open input. A shorter prefix stays a record of that turn. |
| Root mismatch for the joiner alone | Restart that joiner. The table is not paused. |
| Root mismatch for a peer already in the game | [§7](#7-hash-checks). That path may pause. |
| Still missing one `hash_every` interval after the host's first send for `H` | Restart the joiner. The interval is `hash_every * turn_ms`, 1 000 ms at the defaults. A seated peer's missing report still follows [§7](#7-hash-checks). |

A new `generation_id` retires the previous id's stored reports. They cannot activate the new join.

**Shared-link schedule.** One link carries every direction. The design rate is the 100 Mbit/s already used for the 3 072 µs window in [§8.6](#86-causal-readiness). The acceptance delay stays 150 ms one way, 300 ms round trip ([§14](#14-the-netsim-test-plan)). Loss in the worked rows is either zero or one named drop of the channel-2 report. Link overhead outside IPv4 and UDP (preamble, inter-frame gap, checksum) is outside the byte count, which is the same cut the 69-byte empty datagram already uses.

Byte count, in this order: message payload, the 25-byte header, then 20 bytes of IPv4 and 8 bytes of UDP.

| Datagram | Payload | On the wire, header included | Source of the payload size |
|---|---:|---:|---|
| Empty turn | 16 | 69 | [§13](#13-budgets): 8 + 4 + 4, plus 25, plus 28 |
| Hash report | 20 | 73 | The three fields above |
| One snapshot fragment | 1 200 | 1 253 | `payload_max` 1200, of which 12 are the fragment head |
| Synthetic 1 216-byte turn | 1 232, in two pieces | 1 253 and 109 | T45 burst is 1 216 `TurnEntry` bytes; the turn head adds 16 |

The 69-byte row is the empty turn only. The T45 burst is 1 216 bytes of commands (`t45_burst_payload=1216` in [§13](#13-budgets)). Adding the 16-byte turn head makes 1 232 payload bytes, which is above `payload_max`, so the fragmentation rule in [§4](#4-the-channels) splits it. Each piece holds at most 1 188 message bytes: 1 188 + 44. The fragment payloads are 1 200 and 56, and the datagrams are 1 253 and 109. A later real-play turn can be a third size. `turn_store_cap` stays 1 200 (OQ-282).

The host sends in this order, round-robin inside each step, and then repeats:

1. Channel 0 for every seated peer: the turn, and any hash retry that peer is owed.
2. Channel 0 for each joiner: the same turn, plus `JoinGrant`, `JoinCheckpoint` and `SeatActive` when those are due.
3. Channel 1, at most `snapshot_window` fragments in flight per joiner. The window stays 32 and does not widen `ack_bits`. A full window waits for an acknowledgement.
4. Channel 2 hash reports and keep-alives. A report for a generation that is no longer current is dropped here. The channel-0 retry for the current `H` is step 1 or 2 and is not dropped to make room.

`concurrent_transfers` stays 2. A third ask is refused before it has a queue. Two snapshot windows therefore sit behind the seated turns, and they do not share sequence numbers with channel 0. A joiner whose backlog or pin crosses [§8.3](#83-budgets) restarts alone. The seated turns in step 1 have already been sent.

**Design arithmetic at 100 Mbit/s.** These products are not measurements and do not change a default.

- One hash datagram: 73 × 8 = 584 bits, 5.84 µs.
- One snapshot datagram: 1 253 × 8 = 10 024 bits, 100.24 µs. The published 3 072 µs is 32 payloads of 1 200 bytes and leaves the 25-byte header and the 28-byte IP/UDP out. Adding those 53 bytes on each of 32 fragments is 1 696 bytes, 135.68 µs, so one full window of datagrams is 3 207.68 µs.
- Two joiners, two full windows: 6 415.36 µs of snapshot datagrams.
- The 1 216-byte burst to the other five peers of the six-peer acceptance case: each peer gets 1 362 bytes, five peers get 6 810 bytes, 54 480 bits, 544.8 µs.
- Those snapshots and that burst together are 6 960.16 µs, inside one 100 ms turn. Step 1 sends the seated copies before either window, so the two joins do not hold a seated turn for the rest of the period.
- `endless02` is 17 130 fragments ([§4](#4-the-channels)). Payload serialization is 17 130 × 1 200 × 8 = 164 448 000 bits, 1 644.48 ms. The same fragments as 1 253-byte datagrams add 53 × 17 130 = 907 890 bytes, 72.63 ms, 1 717.11 ms in all. The fluid comparisons stay 80.3 s at a 150 ms round trip and 160.6 s at 300 ms (`17 130 * 150 = 2 569 500 = 32 * 80 296 + 28`, and the 300 ms line in [§8.6](#86-causal-readiness)). Those two figures omit this serialization. 1 717 ms is the whole-snapshot link time at that rate, just under six 300 ms rounds, and it is not a new join time. One window is 3 207.68 µs, which is the part that sits inside a single round.

**Worked rows.** The model columns are `evaluate_causal` on the defaults `load_ms` 0, `replay_turns_per_s` 100 and `turn_store_cap` 1200. The test is `CausalReadiness` in `tools/test_snapshot_catchup.py`.

| Row | What is cited | What this design adds |
|---|---|---|
| One fragment, zero loss | Arrival of turn 1 at 250 ms, `ready_ms` 150, input gate 1 160 ms, applied 10, received 10, host 11, `hash_turn` 10, `input_enabled` (`test_one_fragment_turn_cannot_arrive_before_one_way`) | `H = 10`, `A = 12`. The host sends the channel-2 report when it applies turn 10, at 1 000 ms. At 150 ms one way it is stored at 1 150 ms and compared when apply reaches 10 at 1 160 ms. The joiner's report arrives at the host at 1 310 ms. Host current is then 13, so the stamp is 15, which is at least `A`, and `SeatActive` arrives at 1 460 ms. Those two times are the design exchange. They are not part of the 1 160 ms token. |
| Same fragment, channel-2 report lost | The same model row. The model itself does not drop this report. | The host retries on channel 0 at 1 000 + `hash_retry_ms` = 1 500 ms. The retry carries the root, so the joiner can match at 1 650 ms. The joiner's answer reaches the host at 1 800 ms, inside the 1 000 ms expiry that runs from 1 000 to 2 000 ms, and `SeatActive` arrives at 1 950 ms. A lost retry as well leaves the report missing at 2 000 ms and restarts the joiner. `paused` stays false. |
| Load still unfinished | `horizon_ms` 2 000, transfer complete at 3 000: `restart` / `unmet_boundary`, applied 0, received 18, host 20, backlog 18 (`test_a_horizon_before_load_applies_nothing`) | Arrivals during the load stay in the backlog. No report for `H` is compared, and input stays off. |
| `endless02`, zero loss | Causal input 179 160 ms, applied 1 790, host 1 791 ([§13](#13-budgets)). 1 790 is a multiple of `hash_every`. Fluid 80.3 s / 160.6 s as above | `H = 1790`, `A = 1792`. The 179 160 ms figure remains the model gate. The 1 717 ms serialization above is additional design arithmetic, not a replacement. |
| Two joins, one lost hash | `concurrent_transfers` 2, and a cap restarts the joiner who crossed it ([§8.3](#83-budgets)) | The joiner whose report expires restarts alone. The other joiner keeps its generation. Seated peers keep the step-1 turns. The table is not paused. |

**Decision.** The recommended contract is the messages in the table, `H` chosen by the host on a hash multiple, `A = H + command_delay`, joiner-only expiry as a restart, and the four-step send order with the existing window of 32. Status stays Proposed. Still for the owner, together with the choices already listed under [Open choices](#open-choices): approving the ADR, the UDP port (OQ-250), the budget defaults (OQ-280), `load_ms` and `replay_turns_per_s` (OQ-281), `turn_store_cap` (OQ-282), this activation lead (OQ-370), and a measured jitter or a `netsim` shared link (OQ-320, OQ-371). No default moves.

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

An outage of a few seconds is the buffer and the resend. The `netsim` cases in [§14](#14-the-netsim-test-plan) are the check that 0.5–5 s does not drop a peer.

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

An empty turn payload is 8 + 4 + 4 = 16 bytes. With the 25-byte header and 28 bytes of IPv4+UDP, one datagram is 69 bytes. That 69 is this empty datagram. A turn whose commands are the T45 1 216-byte burst is two fragments, accounted in [§8.7](#87-observable-activation-and-the-shared-link). At 10 turns per second that is 690 bytes/s from the host to each other peer. At 6 peers the host sends to 5 others: 3 450 bytes/s, plus one hash report of the same order once per second per peer. A single placement's `TurnEntry` is a few dozen bytes (`state.md` §8.5), so a command on every turn from every seat stays in the same thousands of bytes per second. The snapshot is the save measured above, and it is sent on a join or a resync, not on the one-second hash. The steady-state figure does not include it.

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

What this establishes is the boundary, the refusal rules, and that these measured inputs fit the existing caps without pausing the table. What it does not establish is a shared link, jitter, the channel-2 hash's own flight, another machine, or a real play. [§8.7](#87-observable-activation-and-the-shared-link) is the design for the link and the hash flight. OQ-320 stays open until `netsim` or a measured link tests it. No socket is added.

## 14. The `netsim` test plan

`netsim` runs N peers in one process. The link between them can delay, jitter, drop, reorder and disconnect. Each human seat has a command bot. No game files are required for the cases below; they build a world the sim can construct without `Anlagen.dat`. The scenario measurement of [§13](#13-budgets) stays a separate, manual run.

**Latency.** Latency in this plan is the one-way delay the link adds to each datagram, including each turn. A fluid bulk receipt is not this delay. It is not the round trip. The acknowledgement comes back under the same delay, and the model adds no processing time, so the RTT is twice the latency. The acceptance line "150 ms latency" is 150 ms one-way and 300 ms RTT. [§13](#13-budgets) also records the transfer bound for the other reading, 150 ms as the RTT, because the earlier text did not say which it was. The §13 fluid figures and the discrete completions are payload bounds and model times. They are not measured end-to-end join or resync times. Case 2 loads the fresh image taken after the pause ([§7](#7-hash-checks)), which has an empty tail. Case 4 uses the pin ([§8](#8-join-in-progress)). Neither case is satisfied by `fragment_max` 4096, and neither case treats the 60 s rolling log as the join tail.

The model already runs the retention cases this harness will have to reproduce once `net` exists: a 10-minute image is unavailable, not a successful join; a fresh `endless02` image whose transfer passes 60 s succeeds only with the pin; a skipped hole is a restart; a reordered prefix waits for the contiguous sequence; two joiners share a generation and one cancel leaves the other's pin; three joiners on two generations stop at the global cap of two; a synthetic save past the caps is a refusal or a restart; a replay rate below the live rate does not catch up. Those runs set `paused` false on every join outcome. The mismatch resync is the path that sets it true. `netsim` itself is not implemented here, and this model is not that harness.

From W3's acceptance (`docs/plan/lan-endless.md`):

1. 6 peers, 150 ms latency, 2 % loss, 10 000 turns. The roots match at every check.
2. One peer's world is changed under the test. The mismatch is seen within one `hash_every` interval, `diff_trees` names a path, the snapshot loads, and the roots match again.
3. A peer is dropped and rejoins inside `grace_ms`. It catches up and the roots match. The seat is the same seat.
4. A new peer joins a running game, takes a free seat, and replays until `applied_through` and `received_through` both equal the checkpoint `H` from [§8.7](#87-observable-activation-and-the-shared-link). The hash report for that `H` matches, `SeatActive` names `A = H + command_delay`, and the first accepted command is stamped `current + command_delay`. The others do not pause. Equality with the host's current turn is not the pass condition when the one-way delay is at least one turn. A lost channel-2 report retries on channel 0; a report that is still missing after one `hash_every` interval restarts that joiner only. A report for an earlier turn does not open input. Two overlapping joins use the step order in §8.7, and the seated turns go out before either snapshot window.
5. The host is killed at 20 turns chosen by the test. Each time the lowest remaining seat becomes host and the roots match afterwards.
6. The link is cut for 0.5 s, 2 s and 5 s. No seat is given to the AI (all three are inside `grace_ms`), and every peer catches up from the buffer.
7. A long run, the plan's 8-hour 6-peer session, is not part of the ordinary test. It is the W9 run. `netsim` keeps the same assertions so that run is the same harness with a longer limit.

Two real PCs on a LAN finding each other is a person test (`docs/plan/lan-endless.md` W3), not a `netsim` case.

## Open choices

- **OQ-250.** The UDP port. No original value was found in `tick-loop.md`. No default is set.
- **OQ-251.** Closed in [§13](#13-budgets): `S01_07_01` `state_hash()` is 7.9 ms median with every chunk dirty and 0.34 ms after `prepare_hash`; `save()` is 11 474 169 bytes.
- **OQ-280.** Narrowed by the T43 proposal in [§8](#8-join-in-progress) and [§13](#13-budgets), and by the round-bs correction of the cursors, the global caps and the byte total. The recommended contract is a fresh save at the ask and a pin of the turns after it. The joiner reaches the receivable-prefix boundary and matching-hash condition in §8.6; §8.7 names the checkpoint messages for that condition. Equality with the current host cursor is only the older fluid comparison. A cap trips only that joiner. The 60 s log stays the short-outage buffer. The periodic image is an availability check, not the join payload. The table is not paused for a join. Still for the owner, because this ADR is Proposed: the budget defaults (`tail_turns_max` 4096, `catchup_ms` 600000, `concurrent_transfers` 2, `snapshot_age_ms` 60000, `loss_scale` 2, acceptance `resend_ms` 300). OQ-281 and OQ-282 are narrowed by T45 and their defaults stay in that list. OQ-283 through OQ-289 were not used.
- **OQ-281.** Narrowed by the T45 measurement in [§13](#13-budgets). `World::load` and `run_turn` were timed on one host. The model defaults stay `load_ms` 0 and `replay_turns_per_s` 100. Other machines are unmeasured.
- **OQ-282.** Narrowed by the same measurement. An empty command stream is 0 bytes, one scripted `SetSpeed` is 16, and a synthetic burst is 1 216. The pin still charges `turn_store_cap` 1200. Real-play sizes are unmeasured, and 1200 is not a universal bound.
- **OQ-320.** Narrowed, still open. The causal clock gives each turn its own one-way delay and keeps channel 0 independent of channel 1. [§8.7](#87-observable-activation-and-the-shared-link) is the design for a shared link, for hash-report delivery, and for the send order that puts seated turns ahead of two snapshot windows. The arithmetic there is not a measured link, and `tools/snapshot_catchup.py` does not schedule that queue. A `netsim` case or a measured link is still required. OQ-321 through OQ-329 were not used.
- **OQ-370.** Open. The activation turn in §8.7 is proposed as `A = H + command_delay`. The owner accepts it with this ADR, which stays Proposed.
- **OQ-371.** Open. Jitter on the shared link is unmeasured. The §8.7 rows use zero jitter and, for a lost hash, the configured `hash_retry_ms` of 500. OQ-372 through OQ-379 were not used.

The command delay of 2, the 100 ms turn, the 60 s grace and the 10-minute full check are the plan's and the original's defaults. They are configuration, not new decisions.
