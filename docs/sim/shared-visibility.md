# Five-ship start policy: peace, shared visibility, no alliances (MAP-FOUND-3 package D)

Status: implemented (see section 11); contract, revised after the independent contract review (section 10). Labels: ORIGINAL = evidenced in the original binaries/files; ADAPTATION = this policy; UNKNOWN = open. Owner decision C5 (2026-11-01): the five-player ship
start begins **at peace, with shared visibility, without alliances**. This is an **explicit adaptation of
the game, not an original-Anno behaviour**. Original loaded scenarios keep their own rules (sharing off,
the relation rows they carry).

## 1. Vocabulary: what is what

| Concept | Representation in the headless model | Written by | Meaning |
|---|---|---|---|
| Player contact | `Player::discovery` (original player property 7, as a set of players) | founding reveal, island recount | the set of players whose cities this player's knowledge reached (ORIGINAL property 7, written by the reveal and the island recount; its consumers are UNKNOWN, so "has learned they exist" is an interpretation) |
| City knowledge | `City::player_mask` | registration, reveal | which players know the city |
| Island presence | `Island::players_present` | registration | which players are on the island (original island word `+0xa6`) |
| Ore discovery | `OreObject::discovered` | founding claim stage 2 | which players discovered the ore object |
| Relations | `Player::relations` rows (TRADE, MILIT, WAR bytes, CREDIT) | scenario `DIPL`; none at the ship start (zero fill) | diplomacy; MILIT==3 marks a peer at city registration (original) |
| Fog of war / rendering | **not modelled** | - | per-client tile bit 31 and overview; a contact or mask set is **not** live fog |
| **Sharing policy (new)** | `Rules::shared_visibility`: groups of player ids | the scenario setup | revealed information is mirrored to the group |

## 2. Policy

`Rules::shared_visibility` is a list of **groups**; a group is an ascending, unique list of at least two player
ids. Groups are pairwise disjoint. Empty list = sharing off (default; every original scenario).

*Sharing* means: whenever a founding by member P records a piece of information **actually revealed by P's
action**, the same piece is recorded for every other member of P's group that exists in the world:
1. the new city's `player_mask` holds P, P's MILIT==3 peers (original) and the group;
2. `Island::players_present` gains P and the group;
3. each ore object discovered by the claim gains P and the group in `discovered`;
4. the reveal's visited city gains P and the group in its `player_mask`, and P and every group member learn
   that city's owner (`Player::discovery`).
Everything else is the existing island recount, which is not a cache: it runs over every city of the island
and records for every player in any city's mask that city's owner. Once group members sit in each other's
cities' masks, the first founding by any member therefore gives all members contact with the owners of those
cities (ORIGINAL mechanism, ADAPTATION input).

Rules of the mirror (ADAPTATION): (a) every mirror write sits in the commit block after the last fallible
step, so a refused founding writes nothing; (b) sets are written with `insert_player` (ascending, unique), no
PRNG draw, no iteration-order dependence; (c) a member that is not in the player table at the time is skipped,
as MILIT peers are; a defeated or bankrupt member still receives (membership is not state-dependent); (d) the
visit selection (`reveal_city`, its early-out and the lowest-numbered-city quirk) looks at the founder P alone;
mirroring is idempotent, and because members' per-city membership stays identical under this policy P lacking a
city means the group lacks it. Because the single visit is per founding, a member's later founding can visit a
further city of a third player earlier than it would alone: the group reveals faster than one player, which
is "revealed by P's action" and not a leak; (e) MILIT==3 peers of other members are not added (only the
founder's own, as in the original); (f) no event or message is produced or mirrored: the original's
first-discovery event (codes 0xd..0x11) stays unmodelled (C8); (g) a future consumer of `player_mask`,
`players_present` or `discovered` must not treat a group-derived entry as a right (placement, docking, attack):
sharing is information, never a permission.

It does **not**: reveal unexplored terrain, cities or ore at start; grant tiles, a territorial right or an
owner; move or equalize anything; make members co-settle; touch `Player::relations`; share with a
non-member (including a player who merely exists); or change the nearby-figure veto. No relation byte is
ever written for visibility (code: the policy code reads `Player::relations` only to check peace). The
original ties city-knowledge sharing at registration to MILIT==3 (ORIGINAL 0x10086279); this policy represents
sharing separately and never sets MILIT.

## 3. Peace

ORIGINAL facts: the hostile test is `WAR != 0` for different players (0x100a4df0, the founding veto reads it);
MILIT==3 marks a registration peer (0x10086279); a world predicate (0x100a4d20) also calls WAR==1, TRADE==3
and MILIT==3 "friendly", so no non-zero value is a safe "peace"; every shipped DIPL value is 3 and the meaning
of any other value is UNKNOWN. ADAPTATION: "at peace, no alliance, no trade arrangement" is defined as the
original zero fill, **TRADE 0, MILIT 0, WAR 0** (credit ignored), for every ordered pair of distinct group
members; an explicit all-zero row counts as peace, an absent row too. World creation with a non-empty policy
**refuses** otherwise (never rewrites). The check is made at creation and at load, not maintained: diplomacy
commands do not exist, and a later one must keep the pair at zero or leave the group. The ship-start loader
creates no `DIPL` records (zero rows exist after load), so its seats satisfy this by construction. Peace is
required among group members only; with the default group (all five seats) that is all five. Relations
towards non-members are unconstrained. ship-founding.md C5 (initial relations) now reads: the five-ship start
adopts alternative (b), all relations zero, as an explicit adaptation.

## 4. Validation (World::create; Rules::validate for the structure)

Structure (Rules::validate): each group has >= 2 members, strictly ascending; no id appears in two groups.
World (World::create **and** the load path, as a world invariant, because `Rules::validate` has no player table): every member id is an existing player below `SimConfig::player_slots`; the peace check of section 3. Any failure is a clear error and no world exists.

## 5. Identities

`Rules::digest()` and the world state hash cover the groups (stream version **121**, new). Two identities
exist and mean different things:
- **candidate geometry**: the manifest `content_hash` (islands, spawns, loadout); unchanged by the policy;
- **gameplay setup**: the world's rules digest / state hash and the replay header, which now change with the
  participant set. Same geometry with sharing on or off therefore has the same manifest hash and different
  world hashes. A replay with another participant set is refused by `rules_digest`.

Stream 121 moves **every** state hash, golden and replay header, including sharing-off worlds, because the
Rules layout grows (a count of groups, `u64 0`). That is a layout cost, not a behaviour change: sharing-off
outcomes (masks, discovery, ownership, PRNG cursor) are byte-identical to stream 120, shown by the unchanged
founding tests and a control run; the goldens move only through the version, as at 116/117. Groups are encoded
as `u64 group count` then per group `u64 member count` and `u32` members (nested `ar::seq`).

## 6. Ship-start application

`load_ship_start` takes the participants as explicit data (`ShipStartPolicy::shared_visibility`, default: the
five seat ids as one group, the owner decision). Passing an empty policy gives the earlier behaviour.
Participants are validated against the seats the loader created (a non-seat id is refused). Different
participant sets (e.g. {1,3,4}) are supported; non-default ids come from the data, not constants.

## 7. Acceptance table (tests)

| # | Behaviour | Test |
|---|---|---|
| 1 | all ordered pairs at peace: no relation row exists after load; MILIT never 3, TRADE 0, WAR 0 after founding and save/load | real loader |
| 2 | no tile owner or city appears for a non-founder; territory counts equal the founder's alone | real loader |
| 3 | P's founding: group members appear in the new city's mask, island presence, discovered ore; reveal of an older city reaches the group | synthetic + real loader |
| 4 | non-members (existing player outside the group) acquire nothing | synthetic |
| 5 | unexplored info stays unknown: before any founding nothing is set; a city beyond reach is not in any mask | real loader |
| 6 | sharing off (original scenario, empty policy): founding outcomes identical to stream 120 (existing founding tests unchanged); only the version-driven hashes move | corpus + founding tests |
| 7 | different participant set and non-default ids; absent member refused; unsorted/duplicate/overlapping groups refused; non-peace refused | unit |
| 8 | save/load and replay reproduce the hashes; a refused creation or founding leaves bytes/PRNG unchanged | unit + CLI replay |
| 9 | stream 121: a 120 save is refused with the version message; valid control | version guard |
| 10 | Q's founding legality/veto unchanged by P's policy (same results as an empty policy for the same commands); a second member founding on the same island; a MILIT==3 peer outside the group together with the group | unit |
| 11 | two disjoint groups do not leak; defeated member still receives; a save naming a missing/out-of-range member is refused on load | unit |
| 12 | X owns two cities: with sharing on, the group's visited-city sequence is as specified (c.2), off it is the original's | unit |
| 13 | a refused founding writes no mirror; 1/2/4/7-executor hash equality | unit |
| 14 | the loader's player `DISCO` (section 9): bits of existing players restored, absent/over-slot bits dropped with a warning, length != 4 refused | byte-level |

## 8. Open dependencies (not claimed)

Relation values other than 0 and 3 and the callers of the world predicate 0x100a4d20; the readers of player
property 7 (fog/UI); the first-discovery event consumers (C8); whether the 46 shipped `0xcdcdcdcd` DISCO values
survive to runtime (an uninitialised fill; read as all bits set, it names players that mostly do not exist).
Rendered fog, sailing, diplomacy commands and full-map visibility are not built and not claimed. The sharing is
headless **information state**, not fog-of-war.

## 9. Evidence: the player `DISCO` record and the relation bytes (research ledger: scratch cache/map2-found3/D-research.md)

- ORIGINAL: the player loader reads `DISCO` at AnnoWelt `0x1001d61d` (string `0x100dcbd8`) into `[player+0xac]`
  (`0x1001d641-0x1001d64d`, reader `0x100b4fd0` copies the chunk length); the saver `0x1001f1e3` writes 4 bytes
  from `[player+0xac]`. Player property 7 is exactly that dword (getter `0x1001bea0`, `0x1001bede`; setter entry
  `0x1001be2e`), and the reveal ORs `1 << owner` into it (`0x1008128c-0x1008129d`). So `DISCO` = property 7 =
  `Player::discovery` (bit i = player i).
- ORIGINAL file data: 373 `DISCO` records in the 50 shipped scenes, all length 4: 216 zero, 46 `0xcdcdcdcd`, the
  rest small masks (e.g. `0e000000` = players 1,2,3 in `0201SingleAddOn/single39.sze` at offset 1814, right after
  `BONUS`; `11020000` = bits 0,4,9 in `single36.sze` at 2032).
- Loader decision: the record must be 4 bytes (else the load is refused); bits of players that exist in the scenario
  are restored into `Player::discovery`; bits of absent players or at/over `player_slots` are dropped with one
  warning per scenario (`PlayerDiscoveryBitsDropped`). The finalisation (registration + recount) then adds to the
  restored set.
- Relation bytes: loader `0x1001d758` stores TRADE/MILIT/WAR at `[player+0x1ac+6*other+0/1/2]`; getter `0x1001bf20`;
  kind 3 = CREDIT u16; every shipped DIPL value is 3. MILIT==3 -> registration mask bit (`0x10086279`); WAR != 0 ->
  hostile (`0x100a4df0`). No relation-routed visibility sharing other than that registration marking was found
  (not exhaustively excluded).

## 10. Contract review

Independent review of the first draft: CHANGES REQUIRED (no design flaw; a false claim that the recount is a derived
cache, an unlabelled adaptation of "peace", gaps in rows and in the load-time check, the stream-hash consequence).
All items are applied above; the contract is re-reviewed together with the implementation.

## 11. Implementation record

Code: `Rules::shared_visibility` (config.hpp/cpp, state_fields.hpp, stream 121 in world.hpp), `shared_visibility_problem`
(player.cpp, called from `World::check_invariants`, so creation and load both enforce it), the mirror in
`Founding::apply` (founding.cpp, `audience`), the player `DISCO` loader (scenario_formats.cpp, warning
`PlayerDiscoveryBitsDropped`), `ShipStartPolicy` (sim_load/ship_start). Tests: `founding_test.cpp` (`shared visibility:`
cases), `founding_assets_test.cpp` (`ship start policy:`, `scenario player DISCO on the shipped scenes`),
`scenario_formats_test.cpp` (`scenario player DISCO`), version guards `{..., 120}` in ship_state/scenario_formats tests.
Mutation run (`cache/map2-found3/mutate_d.py`, scratch): 8 of 9 killed; the survivor ("audience leaks to everyone") removes
the filter that skips a group member absent from the player table, which `check_invariants` already makes unreachable
(an equivalent mutant under the invariant; the filter is defence in depth).
Goldens: re-baselined for stream 121; `tools/sim_stream.py --check` (independent transcription, Rules gain `u64 0`) agrees
for every golden it models, including the empty-world hash `cfba1445ed065e55`.
