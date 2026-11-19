# Owned normal end and spawn release

WILDLIFE-RELEASE-EFFECTS-1, 2026-11-19. Preimplementation boundary.
Builds on [creation effects](wildlife-create-effects.md), [creation closure](wildlife-create.md),
[prefix](wildlife-prefix.md), [wildlife §6](../re/wildlife.md#6-spawn-release-oq-573)
and [hunter](../re/hunter.md). Review disposition pending orchestrator.

| Requirement | Positive proof / hand expectation | Wrong sibling / scope |
|---|---|---|
| Selected New end +0x110 | kinds 49/51, configuration 4, no followers/motion/route | unsupported live state refuses |
| Configuration query 12 | numeric mask 2, not 4; configuration 4 skips manager follower lookup | configuration 2 refuses, never silently ignored |
| Home lookup | original unsigned home words, island origin, chunk then category 0 then packed local x/z | moved current position has another record, remains unchanged |
| Exact release | packed 0xc3 ->0x83, id/time/goods retained | clearing extra fields or checking saved ID must fail |
| Detached repeat | no island release on second end, even with replacement record | release-again mutant must fail |
| Direct deletion | no normal-end release; occupancy preserved | no runtime reachability assertion, OQ-579 open |
| Ownership | returned copies survive input destruction; input immutable | no pointers, external frees or actual registry mutation |

## Primary closure before code

D: AnnoWelt New table 0x100c66fc +0x110 selects 0x10052b40–0x10052c01.
The configuration low byte numeric mask 1 guards its follower loop;
empty head skips all follower property calls/free at 0x10052b43–0x10052bbd.
Slot +0x168 selects 0x10051a90–0x10051afa: current action 25/26 would call
0x100330b0; current 0 skips it. With argument 1 this clears current/requested,
flag word mask 0x0800, motion cursor, and copies position to second position.
Motion count word +0x54 = 0 skips the entire world/manager release branch
in 0x10030030–0x1003008d. Null route +0x20 skips its deleting destructor.
These are explicit admission facts, not assumptions about arbitrary live cleanup.

D: island table 0x100cac2c +0x148 selects 0x1008bc80–0x1008bde5.
New property +0x48 selects 0x10052b00–0x10052b3a: query 0x2f zero-extends
u16 home +0x68/+0x6a into x/z and returns a one-cell rectangle. Type getter
+0x78 = 0x100a3cb0 and Game kind getter +0x50 = 0x10010a40 are the already
closed creation receivers. Kind 54 has a separate arm; normal family selection
at 0x1008bd05–0x1008bd0d admits 49..64. This API supports only New 49/51.
Query 12 in New +0x8c = 0x10052570 uses table 0x100525c0 entry 5 ->0x100525a2,
returning configuration dword +0xa0. 0x1008bd21 tests AL with numeric mask 2.
Configuration 4 skips 0x1008bd25–0x1008bd61 entirely, independently of an empty
follower list: it makes no optional manager lookup/detach call. Other configurations
are outside this initial profile and refuse rather than claiming zero callbacks.

D: 0x1008bd64–0x1008bdce reads home x/z dwords from that rectangle; subtracts
zero-extended island origin words +4/+6; signed division by 8 truncates toward
zero, z chunk times island chunk-row stride +0x8c plus x chunk. Local coordinates
use signed remainder of ABSOLUTE home x/z by 8, not origin-relative remainder.
Supported home is nonnegative u16, so the local remainders are 0..7.
Category-0 lookup 0x1007c630–0x1007c691 first tests category bit 0 in chunk word
+8, then walks list order; 0x1007bc00/0x1007bc40 select category before matching
packed x bits 0..2 and z bits 3..5. Spawn table 0x100cac04 slots 0/+4 select
0x100952b0 (category byte +10) /0x100afa40 (next link +4). Both are plain reads.
Only the first matching record is returned. 0x1008bdd9 clears only byte +8 mask
0x40, without testing occupancy or comparing saved ID. No timestamp/id/goods write.

D: after island release New clears its island pointer at 0x10052bf8. Repeated end
still does empty local cleanup but skips island callback at 0x10052beb–0x10052bff.
Manager direct Remove(figure,1), 0x100974b0–0x10097553, takes its deleting path
without +0x110: registry slot cleared then figure destructor. That path does not
itself invoke island release. It is a contrast only, not a modeled manager or a
claim that normal gameplay directly deletes an unended attached animal.

Direct selected cleanup bodies contain no world +0xb4 draw. Transitive selected
callbacks are closed above only for empty profile and simple category/next readers;
nonempty motion/jobs/followers, allocation/deallocation, other kinds/configuration,
first actions, destruction and full OQ-578 remain unproved/out of scope.

## Owned API and supported domain

`project_wildlife_end(descriptor, end_state, island_snapshot, operation)` returns
owned `WildlifeEndEffects`: copied state/island plus ordered projection stages.
It reuses sealed kind 49/51 creation facts and requires matching widened type ID.
`WildlifeEndState` separately carries attachment/ended bookkeeping and explicit
follower count, motion count/cursor, route presence. Configuration must equal 4,
current action 0 and first action not started. Every nonempty/live profile refuses.
This is a composed creation-to-end projection with an authored changed current
position, not a claim to support executed movement. Ending is callable without
simulating HP damage. `ended` is API bookkeeping, not a new original field/flag.

Snapshot geometry has widened origin/columns/rows and sparse unique chunks; origin
must fit original u16. Each chunk supplies category availability and ordered typed
spawn records. Category 0's first packed-local match is changed; no saved-ID test.
Absent sparse chunk or category gives no write. Out-of-grid home refuses rather
than following an invalid native pointer. Negative origin-relative differences
use signed truncation by 8, matching the read; absolute-home remainder is retained.
This traverses owned facts, not original lists or actual spatial memory, and does
not certify that callers supplied a genuine original snapshot. No trust token.

Normal end clears current/requested, flag 0x0800, copies current position words to
second position, then releases/detaches if attached. Action, HP, quantity, cargo,
yield, home, IDs and time are preserved. Ended+attached is inconsistent/refused.
Detached repeated normal end does local reset only. DirectRemovalContrast returns
unchanged state/snapshot and one contrast stage: no deletion/free/detachment is
executed. Geometry/profile checks are adapter admission even for detached/contrast
inputs, not a statement that the original direct-remove path performs them.

Hand expectations: create x3/z0 ->occupied 0xc3; move supplied current to 8/8,
then end ->home 0x83, current record 0xc0 unchanged. Saved id 65535 versus animal
9 still releases. Wrong category and absent category bit do not. Duplicate local
matches change only the first category-0 one. Home8/origin9 yields chunk0 and
local0 by signed truncation and absolute remainder. A detached second end leaves
new id444/packed0xc3 occupied; an old attached animal still clears it. These are
literal synthetic expectations, not an original runtime or corpus oracle.
