# ADR 0003: Network transport and sessions

- **Status:** Proposed
- **Date:** 2026-10-26

ADR 0002 is reserved for the float walk (T34) and is not this document.

This is the design of the `net` library. It does not add code. The owner approves it before W3 step 2. Numbers are either taken from a decision or from the original, or they are named as unmeasured in [§13](#13-budgets). Nothing here is a guess stated as a measurement.

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
| 15 | `u32` | `ack`, the highest contiguous sequence the sender has received on this channel |
| 19 | `u32` | `ack_bits`, bit `i` set means `ack - (i+1)` was also received |
| 23 | `u16` | `payload_len` |
| 25 | bytes | `payload`, exactly `payload_len` bytes, and no trailing byte |

The header is 25 bytes. `payload_len` above the configured maximum is refused before the payload is copied. The maximum is `net.payload_max`, default 1200. Reason: an Ethernet frame of 1500 bytes minus a 20-byte IPv4 header and an 8-byte UDP header leaves 1472, and 1200 stays under that with room for options, so the LAN path does not depend on IP fragmentation. It is configuration because a later internet path may need a smaller value.

`protocol_version` other than 1 is refused at the first packet. `session_id` other than the one from the handshake is dropped.

## 4. The channels

| id | reliability | what travels |
|---:|---|---|
| 0 | reliable, ordered | turns, client commands, handshake messages after discovery |
| 1 | reliable, ordered, own sequence | snapshot fragments |
| 2 | unreliable | keep-alive, ping, hash report |

Channel 1 is separate from channel 0 so a snapshot cannot sit in front of a turn. The plan's "without stopping the others" on a join (`docs/plan/lan-endless.md` §4) is the reason. One ordered stream for both would stall the players who are already in the game.

**Selective acknowledgement.** `ack` and `ack_bits` name the last 33 packets the sender has seen on that channel. A packet not covered is resent. The resend wait is `net.resend_ms`, default 50. Reason: the turn period is 100 ms, and a resend at 50 ms still lands inside the next period on a LAN. It is configuration because an internet path will want a larger value.

**Keep-alive.** On channel 2, each peer sends one every `net.keepalive_ms`, default 1000. Reason: the original's in-game keep-alive is 1000 ms (`tick-loop.md` §5). A peer that has sent any packet in the interval skips the extra one.

**Fragmentation.** A payload larger than `payload_max` is not one datagram. The sender splits it into pieces of at most `payload_max` minus a 12-byte fragment head (`u32 id`, `u32 index`, `u32 count`), on the channel that owns the message. The receiver refuses `count` above `net.fragment_max`, default 4096, before allocating. Reason: 4096 times 1200 bytes is about 4.9 MB, above any save this sim has described, and the cap is what stops a hostile `count` from allocating. The pieces are delivered upward only when every index is present.

**Snapshot flow control.** At most `net.snapshot_window` fragments of channel 1 are in flight, default 32. Reason: 32 times 1200 bytes is 38 400 bytes outstanding, which is a bound on the host's buffer while a joiner catches up, and turns on channel 0 are not part of that window. When the window is full the host waits for acks. It does not drop turns to make room.

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

**The full check.** Every `net.full_check_min` minutes of real time, default 10, the host writes `World::save()` and each peer checks that its own `save()` bytes match the hash it already trusts, and exchanges the root again. Reason for 10: the original's single-player autosave interval is 10 minutes of real time (`tick-loop.md` fixture F9). The snapshot from that save is the one a later resync loads. Pairing them is the plan's rule (`docs/plan/lan-endless.md` §4, "together with the host's autosave snapshot").

**On a mismatch.**

1. The host pauses.
2. The peers that differ record a `HashTree`, starting at depth 0 (the sections). They exchange only the nodes that differ and walk down those branches, up to depth 4 (buildings, the depth `state.md` §9.4 names). `diff_trees` names the path. A matching branch is not sent.
3. The host sends its latest snapshot on channel 1. Every peer `World::load`s it. `load` already refuses a bad version, a bad checksum and a hostile length before it allocates (`state.md` §9.5).
4. The host resumes at the turn after the snapshot's tick. Turns between the snapshot and that point are replayed from the host's buffer, through `run_turn`, so the joiner and the resumed peers land on the same hash before anyone plays again.

The cost of step 2's tree is paid only on a mismatch, not every check.

## 8. Join-in-progress

A new player asks the host, naming a free seat or a seat the AI is holding. The host:

1. does the handshake of [§5](#5-the-session-states) against the running session's setup (the joiner does not get to change it);
2. sends the latest snapshot on channel 1, without pausing the other peers;
3. sends every turn after that snapshot's turn, from the buffer, on channel 0.

The joiner `load`s the snapshot and `run_turn`s until its turn index equals the host's latest, then compares `state_hash()` with the next ordinary check. On a match it may send commands. On a mismatch it is dropped from the join and the others are not paused: a bad join is the joiner's failure, not a desync of the table, unless the check says an existing peer disagrees too.

This is the plan's OpenTTD-style join (`docs/plan/lan-endless.md` §4). The snapshot is `World::save()` / `World::load`, not a second format.

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

If the successor also vanishes before step 6, the next-lowest seat repeats the same steps. The game ends only when no human seat is connected ([§10](#10-drops-and-outages)).

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
| `fragment_max` | 4096 | [§4](#4-the-channels) |
| `snapshot_window` | 32 | [§4](#4-the-channels) |

A peer silent past `wait_ms` raises the wait dialog on the others and the host keeps sending turns into the buffer. The game is not ended. Past `grace_ms` the seat is marked computer: the AI seam on every peer produces that seat's commands (`state.md` §8.2 item 4), and nothing for that seat is required on the wire (D9). A rejoin inside or after the grace follows [§8](#8-join-in-progress) and the seat is returned to the human.

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

Measured with the existing `anno-sim-replay.exe` in the main checkout's build (`E:\anno1503\build\msvc\src\tools\sim_replay\Release\anno-sim-replay.exe`), not rebuilt in this worktree. The worktree is at the same commit as `main` (`6ace6c8`). The command was `scenario` on `Scenes\0100Kampagne01\S01_07_01.sze` with `--ticks 0` and `ANNO1503_GAME_DIR` set. Output, counts only:

- 24 islands, 0 links, 8 players, 14 cities, 662 buildings, 113 017 tile objects;
- `hash after create` `4ff82ec96d856be3`;
- `time load 47 ms, create 13 ms, run 5 ms`.

That timer does **not** include `state_hash()`. In `src/tools/sim_replay/main.cpp` the create timestamp is taken before the hash is printed, and the run timestamp is taken before the second hash. So 13 ms is the world build, not the root-hash cost. The snapshot size was not printed: the tool has no `save()` call.

**OQ-251.** The two budgets the plan asks to measure before this ADR is accepted are still open: the wall time of one `state_hash()` on this world, and `save().size()`. The next measurement is those two calls after the same `scenario` create, repeated enough times that the first call is not the only sample. Until they are in, this ADR stays Proposed and does not claim a millisecond or a byte size.

What can be calculated without them is the steady-state traffic, because an empty turn does not depend on the save.

An empty turn payload is 8 + 4 + 4 = 16 bytes. With the 25-byte header and 28 bytes of IPv4+UDP, one datagram is 69 bytes. At 10 turns per second that is 690 bytes/s from the host to each other peer. At 6 peers the host sends to 5 others: 3 450 bytes/s, plus one hash report of the same order once per second per peer. A single placement's `TurnEntry` is a few dozen bytes (`state.md` §8.5), so a command on every turn from every seat stays in the same thousands of bytes per second. The snapshot is the burst, and its size is OQ-251. The acceptance of the ADR waits on that number; the steady-state figure above does not.

## 14. The `netsim` test plan

`netsim` runs N peers in one process. The link between them can delay, jitter, drop, reorder and disconnect. Each human seat has a command bot. No game files are required for the cases below; they build a world the sim can construct without `Anlagen.dat`. The scenario measurement of [§13](#13-budgets) stays a separate, manual run.

From W3's acceptance (`docs/plan/lan-endless.md`):

1. 6 peers, 150 ms latency, 2 % loss, 10 000 turns. The roots match at every check.
2. One peer's world is changed under the test. The mismatch is seen within one `hash_every` interval, `diff_trees` names a path, the snapshot loads, and the roots match again.
3. A peer is dropped and rejoins inside `grace_ms`. It catches up and the roots match. The seat is the same seat.
4. A new peer joins a running game, takes a free seat, replays, and the roots match. The others do not pause.
5. The host is killed at 20 turns chosen by the test. Each time the lowest remaining seat becomes host and the roots match afterwards.
6. The link is cut for 0.5 s, 2 s and 5 s. No seat is given to the AI (all three are inside `grace_ms`), and every peer catches up from the buffer.
7. A long run, the plan's 8-hour 6-peer session, is not part of the ordinary test. It is the W9 run. `netsim` keeps the same assertions so that run is the same harness with a longer limit.

Two real PCs on a LAN finding each other is a person test (`docs/plan/lan-endless.md` W3), not a `netsim` case.

## Open choices

- **OQ-250.** The UDP port. No original value was found in `tick-loop.md`. No default is set.
- **OQ-251.** Root-hash time and snapshot size on `S01_07_01`, [§13](#13-budgets).

The command delay of 2, the 100 ms turn, the 60 s grace and the 10-minute full check are the plan's and the original's defaults. They are configuration, not new decisions.
