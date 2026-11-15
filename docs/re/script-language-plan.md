# S01_02_01 script-language dependencies and proposed steps (SL-0, 2026-11-15)

Status: **READY FOR REVIEW**. This is a read and proposed plan, not authorization
for SL-1 or a claim that the unchanged campaign script executes.

Evidence: **D** original instruction/table read; **S** original name catalogue;
**P** parsed original file; **A** derived consequence; **T** bounded project-code
probe; **U** unproved. All addresses below are virtual addresses in the explicitly
named GOG 2.0.0.5 module. Raw listings and analysis inputs remain in ignored cache.
No original script text, bytes or listings are reproduced here.

Reference identities: AnnoScene.dll SHA-256
`5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38`;
AnnoWelt.dll `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
Other read modules and full hashes are recorded in `cache/sl-0/input-identities.json`.
The scenario is `Scenes/0100Kampagne01/S01_02_01.sze`, SHA-256
`23a99f002e422df6580781746f4cf3ed6766595540f67acd867d88b6ddf17630`.

Reuse [BIGBROTHER](../formats/bigbrother.md), [city objects](script-objects.md),
[fisher §8](fisher.md#8-script-object-inventory-and-proposed-next-packet-2026-11-15),
[script scheduling/save](script-runtime.md), [existing event bridge](script-runtime2.md),
[AI parameter evidence](annoscene.md#11-ai-profiles-compprofil2-the-opponent-setup-and-the-hand-over-to-complogicdll)
and [CompLogic](complogic.md). Existing reads are cited as such, not presented as new closure.

## 1. Inventory and the refusal ladder

**P.** `tools/bigbrother.py::payload/parse/walk` confirms **3 distinct PLAYER
query names in 5 uses**, and **11 distinct action names in 29 calls**. There are
**14 remaining names**, not 16 distinct names. The earlier five-query count was
an occurrence count. Payload offsets below identify the statement/primary starts.
The two city-object queries are already supported by accepted SO-1b and excluded.

**P/D.** The single missing action semicolon is SET_PROFILE at 5856. The existing
read-on model reaches offset 6093 and loses **0 statements**; these counts therefore
have no swallowed action names. Reuse the addressed read-on proof in BIGBROTHER
§3.4 / `script-runtime.md`; this packet does not change the parser.

| Kind / name | Uses | All payload offsets |
|---|---:|---|
| action QUESTLOG_SHOW | 12 | 1339, 1437, 1528, 4824, 4880, 5215, 5271, 5729, 6582, 6638, 6751, 6807 |
| action SETWARELOG | 1 | 1634 |
| action SET_FOR_BUY | 1 | 1715 |
| action SET_DIPLOMACY | 4 | 1827, 1875, 1924, 1968 |
| action ATTACK_AREA | 1 | 2055 |
| action BEAMDOWN | 1 | 2097 |
| action GOTO_SCREEN | 1 | 2169 |
| action SET_PROFILE | 2 | 3592, 5856 |
| action SHOWTEXT | 3 | 3783, 4005, 6865 |
| action LOST | 2 | 3832, 4052 |
| action WON | 1 | 6907 |
| query INHABITANTCOUNT | 2 | 3501, 4677 |
| query DEAD | 2 | 3750, 3974 |
| query BOUGHT | 1 | 5046 |

**T — compile-order measurement, not a gameplay test.** Main `cc7af72c` does not
yet contain SO-1b. A cache-only probe links the already-built accepted SO-1b
compiler/library with its matching headers. Starting from the complete payload,
it measures the first refusal, then substitutes all occurrences of that one name
with an existing inert timer statement or signal primary, preserving every payload
offset and the missing-semicolon shape. Only these private analysis copies are
compiled; none is executed or committed. This is a diagnostic technique, not a
proposed bypass for a session. The first unmodified refusal is exactly
`script offset 1339: unsupported action: QUESTLOG_SHOW`.

| Pass | Newly exposed name | Payload offset |
|---:|---|---:|
| 0 | QUESTLOG_SHOW | 1339 |
| 1 | SETWARELOG | 1634 |
| 2 | SET_FOR_BUY | 1715 |
| 3 | SET_DIPLOMACY | 1827 |
| 4 | ATTACK_AREA | 2055 |
| 5 | BEAMDOWN | 2097 |
| 6 | GOTO_SCREEN | 2169 |
| 7 | INHABITANTCOUNT | 3501 |
| 8 | SET_PROFILE | 3592 |
| 9 | DEAD | 3750 |
| 10 | SHOWTEXT | 3783 |
| 11 | LOST | 3832 |
| 12 | BOUGHT | 5046 |
| 13 | WON | 6907 |

After those **14 diagnostic substitutions**, the transformed input compiles.
That does not establish support for any of the replaced semantics. Names, counts,
read-on data and diagnostics are retained in `cache/sl-0/` for review.

## 2. Handler and class catalogue

**S/D.** Strings are in AnnoScene.dll. The action reader is `0x100170d0`–
`0x1001c5e4`; each action table's execution entry is slot +4, with
`this = action +0`. The PLAYER reader is `0x1001d650`; it constructs a player-query
receiver with table `0x1003ba90` at `0x1001d688`, resolved player at +4.
That receiver's +4/+8 are numeric `0x100128d0` / Boolean `0x10012280`.
Expression wrappers and their adjustments are identified in §3.

Classes below are **2 sim**, **3 presentation**, **9 needs a subsystem**.
A found Scene leaf does not prove the full transitive draw or save path. `U` in
those columns is a specific remaining read, not permission to implement a no-op.

| Name | String / reader reference (Scene) | Execution handler / discriminator (Scene) | Class; state read/write | Draw and persistence boundary |
|---|---|---|---|---|
| QUESTLOG_SHOW | `0x1003f428` / `0x100180f5` | table `0x1003b8fc` -> `0x10015680`–`0x100156de` | **presentation**: reads resolved player id, text reference and show flag; sends quest-display command through framework manager, no direct world store | No direct draw; UI receiver/save closure U1. Definition remains source text. |
| SETWARELOG | `0x1003f624` / `0x10017779` | table `0x1003b914` -> `0x10014100`–`0x1001415b` | **needs a subsystem: AI trade watcher/history**. Reads two mapper bindings and ware selectors; sends AI command 19; changes watched goods/peer and resets counters on the paths in §4 | Scene leaf and watcher setter/getter make no draw; actual trade producer/units U2. LOGWARE is saved by AI writer (§4). |
| SET_FOR_BUY | `0x1003f630` / `0x1001775c` | table `0x1003b7d4` -> `0x10014160`–`0x100141eb` | **needs a subsystem: AI purchase offers**. Reads player kind and AI pointer; kinds 8/11 skip; others receive command 16 with ware/quantity | No direct Scene draw; offer/world-callee closure and exact EKVKWARE save mapping U3. |
| SET_DIPLOMACY | `0x1003eb58` / `0x10019ff9` | table `0x1003b804` -> `0x10015d90`–`0x10015de2`, command 11 | **needs a subsystem: AI diplomatic intent**. Reads two players; changes the AI's peer intent/history, not a direct assignment to the rebuild's MILIT/TRADE/WAR arrays | No direct Scene draw; bounded AI reset/write is draw-free (§4); complete subsequent AI effects and save coverage U4. |
| ATTACK_AREA | `0x1003f28c` / `0x1001882a` | table `0x1003b8d4` -> `0x10015380`–`0x10015425`, command 21 | **needs a subsystem: AI military orders/combat**. Reads area centre, island there and player AI; submits target/width request; AI mutates military-task records | No direct Scene draw; nested military draws, order lifetime/save U5. |
| BEAMDOWN | `0x1003f338` / `0x100185ae` | shared parser `0x1001c16d`; table `0x1003b7dc` -> `0x10014440`–`0x10014a39` | **needs a subsystem: captured clipboard input and world paste**. Reads area origin and OS clipboard text; parses external records and invokes world/object mutation (§5) | No direct world-draw call in this leaf; object-creation draws/save and external-input/replay contract U6. |
| GOTO_SCREEN | `0x1003f7b8` / `0x10017116` | numeric form table `0x1003b92c` -> `0x10013fa0`–`0x10013ff9`; named form `0x1003b934` -> `0x10013e00`–`0x10013f97` | **presentation**: map uses the numeric three-argument form; sends camera/scale changes through framework +0x5c | No direct draw; Scene camera save is already in `script-runtime.md`; host camera transition timing U1. |
| SET_PROFILE | `0x1003ed54` / `0x10019a0b` | table `0x1003b874` -> `0x10015e80`–`0x10015eda` | **needs a subsystem: AI military policy**. Both uses select COMPCAPS_KAMPFTYP, index 18. Writes masked AI policy and can clear existing military-task records (§4) | No direct Scene draw; task-clear callee and full military/save closure U5. |
| SHOWTEXT | `0x1003f130` / `0x10018e0d`, `0x1001bf90` | shared parser `0x1001be90`; table `0x1003b7fc` -> `0x10015600`–`0x10015642` | **presentation**: reads text reference, requests event interface class `0x3800`, enqueues event code 4 for SHOWTEXT (3 for shared SHOWMESSAGE) | No direct draw; existing Frame event enqueue evidence reused; UI display/save policy U1. |
| LOST | `0x1003f13c` / `0x10018df0` | parser `0x1001bfc3`; table `0x1003b7ec` -> `0x100155d0`–`0x100155fd` | **needs a subsystem: mission outcome/termination**. Calls framework +0xc0 with player and low-word reason plus outcome code 4; sets Scene stop latch | No direct draw; Scene latch has no own save entry; framework/profile/world outcome closure U7. |
| WON | `0x1003f144` / `0x10018dd3`, `0x1001c0e7` | shared parser `0x1001bfc3`; table `0x1003b7f4` -> `0x100155a0`–`0x100155cd` | **needs a subsystem: mission outcome/termination**. Same shape as LOST, outcome code 5; sets same stop latch | No direct draw; same U7 persistence/termination boundary. |
| INHABITANTCOUNT | `0x1003fd80` / `0x1001dc4e` | `0x100128d0`, selectors `0xc`–`0x11` -> `0x10012ca0`; city selection helper `0x100120f0` | **sim**: live player city/population read; selected-city and cumulative-level semantics, not an assumed global sum (§3); no direct authoritative state write | No direct draw; query holds no result latch. Existing city/population data is the source; getter/import closure U8. |
| DEAD | `0x1003fba0` / `0x1001edf4` | `0x10012280`, selector 6 -> `0x1001268c` | **sim**: reads player state selector 5; true for states 2/3/4/6 and unsigned values above 7; false for 0/1/5/7; no write | Getter and query arm draw-free. No own query state to save; existing rebuild Player::state is saved. Original broader player-save coverage U8. |
| BOUGHT | `0x1003fc90` / `0x1001e577`, `0x1001fd51` | receiver `0x100128d0`, selector `0x33` -> `0x10012b8b`; wrapper table `0x1003b970` -> `0x100109a0` | **needs a subsystem: AI trade history**. Reads selected AI LOGWARE counter through getter 19; returns 0 if no AI / no matching log; shares SETWARELOG state, not ship-hold cargo (§4) | Getter draw-free; no query-result latch. AI LOGWARE saved; producer units/wrap and transaction closure U2. |

## 3. Live query details and current-engine fit

**D.** INHABITANTCOUNT constructs an expression with table `0x1003b750`
(`AnnoScene 0x1001dcb9`), sets the level selector at `0x1001dce3`–`0x1001dd85`,
and calls the numeric receiver through expression +0x14 at `0x10010499`–`0x100104a7`.
The highest-development city helper `0x100120f0`–`0x10012273` enumerates the
player's city list (player +0x84, selector 2). It finds the highest occupied
population group 0–4, then selects a city maximal in that group's count;
the `jle`/`jl` decisions at `0x10012155`–`0x1001215d` and
`0x1001220b`–`0x10012211` make an equal count replace the earlier candidate.
This is **not** a proved sum over every city. The selected city's population
interface is used at `0x10012caf`–`0x10012cd2`; the SIEDLER branch sums groups
1–4 (`0x10012d28`–`0x10012d60`), not just group 1. Empty-city-list/zero-count
fallback and all getter semantics need explicit oracle vectors before a port (U8).
The engine already has island city rings and population state; inspect
`src/sim/include/anno/sim/city.hpp` and existing population specs, rather than
introducing a new saved aggregate.

**D.** DEAD's Boolean wrapper is an adjusted subobject: parser stores receiver
at complete expression +0x14, discriminator 6 at +0x18 (`0x1001ee52`–`0x1001ee66`);
table `0x1003b77c` +4 -> adjustor `0x1001c920` -> `0x100108a0`, which calls receiver
+8 with the proper arguments. The receiver switch table `0x100127dc` sends
selector 6 to `0x1001268c`; state dispatch `0x100128ac` has the truth set in §2.
Player table `AnnoWelt 0x100c2238`, +0xc4 = `0x1001bea0`; key 5 returns byte
+0x294 at `0x1001bec8`–`0x1001bed0`. The rebuild has PlayerState Alive=0,
Bankrupt=2, Defeated=3 (`player.hpp`), so a non-Alive shortcut happens to agree
for its current legal values but does not specify the original truth table.
Save `Player::state` already exists; no new query latch is justified.

**D.** BOUGHT's descriptor selector is `0x33`, set through compare/narrowing
`AnnoScene 0x1001fd81`–`0x1001fd9f`; it is **not** selector `0x1a` (a different
count query). Its wrapper `0x100109a0` passes a local containing the resolved peer
and ware to the receiver (`0x100109ac`–`0x100109c9`). Receiver
`0x10012b8b`–`0x10012c09` obtains player property 44, calls AI getter 19, and
selects returned message +0x18 for BOUGHT (+0x14 for SOLD). This is a live read;
there is no stored Boolean or RNG call in this getter path.

## 4. CompLogic is a real dependency

**D, reused receiver evidence.** [AnnoScene §11.4/11.5](annoscene.md#114-hand-over-at-game-start-annoframedll-0x10028d90)
proves player property 44 (`0x2c`) returns `[player+0xc]`: AnnoWelt getter
`0x10020ab9`, set at `0x10020f27`. It is a per-player AI object.
CompLogic table `0x10063550` +4/+8 dispatch to `0x10004550` / `0x10004a70`;
`this = AI +0`, not its world-access subobject at +4. The rebuild lacks this
object and its trade/military intent state (PLAN M5 and current `Player` fields).
Existing MILIT/TRADE/WAR arrays and ship cargo do not provide these AI records.

**P.** This map's merged SPIELER header/state series have kinds 8, 12, 12, 16,
2 at slots 0, 1, 2, 3, 31. Slots 1, 2, 3 have COMP records; slots 0/31 do not.
SETWARELOG targets slot 1, SET_FOR_BUY's mapped receiver is slot 1, ATTACK_AREA
and both KAMPFTYP changes target slot 2. SET_DIPLOMACY targets slots 1/2.
These are computer receivers with COMP data, not the Scene leaf's null-AI or
human-kind skip controls. Raw parsing merged both SPIELER series; a later
state record lacking HEAD must not overwrite the earlier kind with absence.

**D — watcher and BOUGHT.** Command 19 dispatches at CompLogic
`0x100048ad`–`0x100048bd` to `0x10005e50`–`0x10005eee`. Two 8-byte log records
at AI +0xa00 / +0xa08 contain a ware word, peer word, and two counter words.
Changing the watched ware or clearing it resets counters; a same-ware path
keeps them (`0x10005e68`–`0x10005ea3`, `0x10005ead`–`0x10005ee7`).
Getter 19 (`0x10004cb6`–`0x10004cc1`) calls `0x10005f00`–`0x10005f67`;
it matches the ware, chooses the first matching log, writes the bought counter
to message +0x18 and the sold counter to +0x14. It does not itself filter by
message peer; peer matching occurs in the billing producer, not this getter.
Producer `0x10005f70`–`0x10006007` tests peer/ware and direction, then adds a
word quantity to the appropriate word counter. Units, all producer callers,
overflow compatibility and the rest of the producer remain U2.

**D — save.** AnnoWelt player writer `0x1001fa54`–`0x1001fa6f` calls AI slot
+0x14 when the AI exists. On table `0x10063550`, that is CompLogic
`0x10041090`; `0x100410bb` calls base writer `0x10003b30`. The base writer packs
the two log records at `0x10004008`–`0x100040a4` under the LOGWARE key
(`0x10065538`). Thus these counters/watchers are saved state, not only script text.
Their full reader round-trip and other AI fields' save coverage remain U2–U5.

**D — purchase offer.** SET_FOR_BUY uses AI command 16, dispatched at
`CompLogic 0x1000489a`–`0x100048aa` to `0x100061f0`–`0x10006309`.
The nonzero-quantity arm writes a per-ware six-byte record at +0x1c24,
active flags and quantity, then calls `0x1000c3d0`, `0x100204b0`, `0x10028100`
and marks city ware flags. The zero arm clears the offer and calls
`0x100282b0` along an AI city chain. Scene's human-kind exclusion is
`0x10014189`–`0x10014195`. This requires a real offer/trade backend (U3),
not treating it as a city stock injection or an accepted empty instruction.

**D — diplomacy.** SET_DIPLOMACY sends command 11 (`AnnoScene 0x1001be6f`–
`0x1001be7f`). CompLogic `0x10004861`–`0x10004871` enters
`0x10008690`–`0x100086f9`, validating the peer and touching that peer's AI
record through `0x1003e830` (clears 32 intent entries) and `0x1003e680`
(inserts/updates a stamped entry, alters aggregate score with clamping).
Those bounded helpers contain no calls to another function or PRNG.
The complete relationship evolution/world publication/save path is U4.

**D — military policy/order.** Both SET_PROFILE arguments name
COMPCAPS_KAMPFTYP (P). Index 18, CompLogic `0x100046af`–`0x100046cd`, stores
`value & 15` at +0xa38 and calls `0x10004df0`–`0x10004e58`. That helper can
clear matching entries in the military-task array via `0x10059710`.
ATTACK_AREA sends command 21; CompLogic `0x100048d3`–`0x100048df` calls
`0x10005940`, whose entry inspects/matches military-task records and may clear
one at `0x100059bc`. Further arms call military planners such as
`0x10022d40`, `0x10020780`, `0x10020970`, `0x10037440` (U5).
A boolean attack flag or an event without authoritative orders is not this rule.

## 5. External input and session outcomes

**D — BEAMDOWN.** The parser at AnnoScene `0x100185be` selects the shared area
argument reader `0x1001c16d`; it creates table `0x1003b7dc` at `0x1001c2c2`.
The execution body `0x10014440`–`0x10014a39` tests clipboard format 1, opens
the clipboard and gets format 1 data (`0x1001444c`–`0x10014494`). Import-table
resolution proves these are USER32 IsClipboardFormatAvailable, OpenClipboard,
GetClipboardData, CloseClipboard and KERNEL32 GlobalLock, not a sim RNG API.
A failed guard returns without a world change. The populated path reads external
text, parses records and invokes world/figure/building operations, including
manager creation at `0x1001475b`–`0x1001479f` and later object operations through
`0x100149ae`. The target is the declared area's origin, not a declared city object.
No original clipboard contents were executed or copied to git. Clipboard contents,
record interpretation, transitive creation draws and replay capture are U6.
Assuming an empty clipboard as a default would introduce an unreviewed input contract.

**D — outcomes.** WON/LOST execution passes the player's resolved pointer and
`(reason & 0xffff) | (outcome << 16)` to framework slot +0xc0, then sets
Scene byte `0x10043438` at `0x100155c5` / `0x100155f5`. Scene constructor
`0x1001611f` clears it; script step reads it at `0x10016b7d`
(reused `script-runtime.md`). This changes future script execution, so a UI-only
notification loses a sim-relevant effect. The bounded Scene writer
`0x10016470`–`0x1001650f` saves source/signals/camera, not this global latch.
The framework's player outcome, campaign/profile persistence and session end
behaviour have not been followed here (U7). A per-world, deterministic outcome
contract must precede a port; no new state or extension is allocated by SL-0.

**D/A — presentation boundary.** QUESTLOG_SHOW's leaf packs player id and show
flag and calls framework manager +0x9c with operation 15
(`0x10015699`–`0x100156d3`). GOTO_SCREEN's numeric form sends scale/position
through framework +0x5c (`0x10013fa0`–`0x10013ff4`); the named form has additional
world lookups but is not used by this map. SHOWTEXT uses event class `0x3800`
and its +0x58 enqueue (`0x10015600`–`0x10015642`), broadcasting to `0xaa`.
The existing Frame event copy/audience evidence at `0x10061010` is reused from
`script-runtime2.md`; exact event-4 presentation handling and quest-log storage
are U1. Proposed ports emit typed, transactional UI requests without changing
world money, goods, ownership, timers or PRNG; actual UI display remains a UI-lane
acceptance item, and restoration/redelivery policy needs a documented contract.

## 6. Proposed small-step order — owner review required

Each row is a proposed packet of **about 2 h**, one concern, with its own oracle,
negative controls, affected gate, pins check and orchestrator handover. Read packets
run no local test suite. Backend availability is a release condition, not work
hidden inside a two-hour frontend packet. The full AI, trading and combat systems
are **not sized here**; their contracts need the prerequisite reads below.
No SL code packet is released by this plan. Each stops for review.

Next-refusal cells assume earlier code rows have actually landed with their
prerequisites. They are predictions from the measured ladder, not successful
compilations of the unchanged source. Independent query/presentation work can
proceed while a backend question is held, but the first refusal may stay unchanged.

| Proposed step | Scope / prerequisite | Fails before; passes after | Next unchanged-script refusal |
|---|---|---|---|
| SL-1 | Presentation requests: QUESTLOG_SHOW, numeric GOTO_SCREEN, SHOWTEXT. First close U1's message payload/restore policy; reuse PLAYWAVE transaction/event machinery | Three unsupported action forms -> exact typed events, audience/ordering/repetition, rollback on later failure, native-cut event policy; no world/RNG mutation | SETWARELOG, 1634 |
| SL-2 | Live player reads: DEAD and INHABITANTCOUNT. Close U8 with hand-derived multi-city/tier/empty vectors before implementation | Unsupported queries -> original state truth table, selected-city/tie/cumulative-level vectors and live changes across reload; no added saved aggregate | SETWARELOG, 1634 (independent progress) |
| SL-3 | Read + oracle/contract for AI trade watch, BOUGHT and purchase offers (U2/U3); choose backend ownership/save disposition in Q1 | Incomplete producer/units/offer/save knowledge -> addressed contract and vectors; no engine frontend admitted yet | SETWARELOG, 1634 |
| SL-4 | Trade frontend: SETWARELOG, SET_FOR_BUY, BOUGHT, **only after an accepted real offer/transaction/history backend**, including original COMP import, producers and persistence | Three refusals -> actual watcher reset/billing, real offer mutations and live bought counters, a real transaction feeding the query, save/replay and rollback; no constant counter fallback | SET_DIPLOMACY, 1827 |
| SL-5 | Read + oracle/contract for AI diplomacy, KAMPFTYP and attack orders (U4/U5); size the missing military backend separately (Q1) | Incomplete AI intent/order lifetime/draw/save knowledge -> exact contracts and a separately reviewed backend sequence; no stub | SET_DIPLOMACY, 1827 |
| SL-6 | SET_DIPLOMACY and SET_PROFILE frontend, after accepted AI intent/military policy backend | Refusals -> real peer intent reset/update and KAMPFTYP task effects; save/reload, existing order controls and exact draws | ATTACK_AREA, 2055 |
| SL-7 | ATTACK_AREA frontend, after accepted military order/combat backend and SL-5 contract | Refusal -> original area target/order accepted and consumed by real units; invalid target and missing subsystem refuse; order persistence/draw proof | BEAMDOWN, 2097 |
| SL-8 | Read + policy for BEAMDOWN's captured clipboard/world paste (U6, Q2) | Unspecified external input -> deterministic captured-input/replay contract and decoded paste rules, including authentic absent-format guard; no code change | BEAMDOWN, 2097 |
| SL-9 | BEAMDOWN frontend, after accepted external-input and paste backend | Refusal -> actual captured input executes the original mutations, or authentic absent-format input takes its proved empty path; populated unsupported input refuses; native/replay/draw proof | LOST, 3832 (GOTO_SCREEN, both live queries and BOUGHT already handled above) |
| SL-10 | Read + oracle/contract for WON/LOST framework/session stop and persistence (U7, Q3) | Unread outcome callback -> accepted per-world result/termination/save contract, no outcome stub | LOST, 3832 |
| SL-11 | WON/LOST frontend, after accepted outcome backend/contract | Refusals -> authoritative outcome plus script-stop semantics, repeated/combined actions, rollback and native/replay; every map name covered | None from this inventory; fresh complete compile/startup must then be measured |

Only after that fresh unchanged-source check may FISHER-1e's real canary proceed.
Script compilation alone proves neither AI progression nor the fisher's return
through closed entrances (FISHER-RE-3 remains separately queued).

## 7. What remains U and the decisions to review

All **14 handlers are located and classed**. Remaining U items concern transitive
contracts, not invented replacement behaviour:

- **U1:** quest receiver/storage and SHOWTEXT event-4 consumer; camera interpolation;
  exact UI restore/redelivery policy. Tried: Scene reader/vtables/leaves and existing
  Frame event bridge. These support the presentation classification, not a claim of
  live UI completion or fully decoded consumer persistence.
- **U2:** LOGWARE producer callers, quantity units/wrap and full loader round-trip.
  Tried: Scene BOUGHT wrapper/receiver, CompLogic command/getter, billing and writer.
- **U3:** offer helper closure and exact EKVKWARE save mapping. Tried: complete
  Scene leaf, CompLogic `0x100061f0`–`0x10006309`, named helper call sites and existing AI notes.
- **U4:** subsequent diplomacy evolution/world publication and full save fields.
  Tried: command 11 receiver and bounded intent reset/update helpers.
- **U5:** military planners/task clear, their RNG draws, consumers and save lifetime.
  Tried: ATTACK_AREA/KAMPFTYP dispatch, entry arms and existing CompLogic inventory.
- **U6:** clipboard record grammar, complete mutation/draw closure and deterministic
  external-input capture. Tried: full Scene leaf and resolved OS imports.
- **U7:** framework outcome callback, campaign persistence and native result/stop
  contract. Tried: both leaves and existing script-step/save read; framework +0xc0
  itself remains unread. No assertion that outcome is merely presentational.
- **U8:** complete city-stat getter closure and empty/zero fallback vectors;
  original player-state save detail. Tried: PLAYER parser, both receivers,
  selected-city helper and original player state getter. Sim query class is settled.

**QUESTION Q1 — subsystem ownership/scope.** Which separately reviewed backend
packets should supply AI trade/history/offers and diplomatic/military intent before
SL-4/6/7? Recommend the bounded SL-3/5 reads first, then explicit backend assignments;
a two-hour script wrapper cannot replace a missing AI/trade/combat producer.

**QUESTION Q2 — clipboard contract.** Shall BEAMDOWN receive recorded/synchronized
external input (including explicitly captured absent-format input), or remain a typed
refusal until that subsystem exists? Recommend a read/capture contract before SL-9.
Do not silently substitute an empty host clipboard in a normal-session canary.

**QUESTION Q3 — mission result contract.** Who owns the authoritative per-world
outcome/script-stop and persistence contract for WON/LOST? Recommend SL-10 before
its code packet and a separately reviewed extension decision if saved state is
needed; an emitted UI notification alone loses the original stop latch.

History Edition and original live execution are NOT_RUN. SL-0 changes no engine,
fixture, pin, state version or default. The F1 island-number writer proof is appended
to [script-objects.md §8](script-objects.md#8-so-1b-review-f1--original-island-number-writer-sl-0-2026-11-15).
