# `CompLogic.dll` AI behaviour and the HE RTTI cross-reference (2026-11-07)

Read-only research write-up. Continues [`complogic.md`](complogic.md) (the
authoritative, address-numbered inventory of `CompLogic.dll`/
`CompLogicClassic.dll`, read in full before this pass started) and the
History Edition investigation started in the mine/quarry/discovery-mask
follow-up (see [`mine-deposit-followups.md`](mine-deposit-followups.md) for
the sibling write-up on that separate thread, done the same day on a
different branch). This note does **not** edit `complogic.md` itself, to
avoid touching a file another lane's task numbering (`T-AI.x`) may still be
extending; it is meant to be folded in by whoever next continues that
numbering.

**Evidence tagging, same convention as `mine-deposit-followups.md`:**
**verified** = re-disassembled/re-checked in the pass that produced this
specific claim, with the address(es) cited; **one pass** = read once by a
research agent, not independently re-derived a second time; **open** = not
resolved; **hypothesis** = a plausible inference from the evidence, stated
as such and not as fact. Nothing here was independently re-verified by a
third pass after the two research agents reported it — that distinction
matters and is kept honest throughout, the same way the mine-deposit
follow-up flagged its own open items.

GOG 2.0.0.5 binaries: `game/app/CompLogic.dll` / `CompLogicClassic.dll` /
`AnnoWelt.dll` (read-only, git-ignored, never modified). HE binaries: the
Steam "Anno 1503 - History Edition" install at
`E:\SteamLibrary\steamapps\common\Anno 1503 - History Edition\` (also
read-only, outside the repository). Toolchain for both agents: capstone +
pefile (+ unicorn for one pass) in a throwaway venv under their own
scratchpad directories — nothing installed into the project's tracked
environment, no project file touched except this one.

## Part A — the History Edition binaries: what's actually readable, and what fell out of it

**One pass.** HE's main `AnnoWelt.dll`/`CompLogic.dll`/`CompLogicClassic.dll`
are compiled for x86-64 (PE32+, machine `0x8664`, timestamps 2020-08-05),
against GOG 2.0.0.5's 32-bit x86 (2006). The sibling mine-deposit follow-up
independently re-confirmed this PE-header fact directly (**verified** there)
and additionally found that **`AnnoWeltAddOn.dll` in the HE install is still
32-bit x86** (`0x14c`, 2019-09-27, size 1,698,744, not yet compared with any
GOG module; the GOG 2.0.0.5 install has no file of that name, so which GOG
module it corresponds to is part of the question). That is a real, concrete
lead for a future pass: it may be the one HE binary actually comparable to
GOG 2.0.0.5 by the same capstone/pefile byte-level technique used throughout
this project, rather than needing an independent x64 analysis. Not pursued
in either research pass so far — **open**.

**Capability demonstrated, one pass.** `capstone.CS_MODE_64` (as opposed to
`CS_MODE_32`, used everywhere else in this project) plus the same `pefile`
API used for the 32-bit builds decode HE's x86-64 code correctly — checked
by disassembling the `CompCreateInstance` export and individual vtable-slot
functions in HE's `CompLogic.dll` and getting clean, plausible x64
instruction sequences both times. No Ghidra or `rea` MCP tool is available
in this environment and none was needed; pure Python disassembly is
sufficient on this Windows host for x64 just as it already was for x86.
**This directly answers "can the HE files be read out despite being
x86-64": yes, with the project's existing toolchain, just in 64-bit mode.**

**The actual discovery, one pass: HE ships full MSVC RTTI; GOG has none.**
`complogic.md` §1 already states, for the GOG binaries, "RTTI type
descriptors are absent. The byte sequences `.?AV`, `.?AU` and `.?AT` occur
zero times in either file." HE's `CompLogic.dll`/`CompLogicClassic.dll` are
the opposite: 24 RTTI type descriptors were found, resolving to 21 distinct
real C++ classes/interfaces (three descriptors are duplicates/padding
artifacts, not separately counted as classes):

| HE class name | notes from the one-pass read |
|---|---|
| `ICompPlayer`, `ICompWork`, `IMaxUnknown` | interfaces |
| `CBaseCompPlayer` | base player class |
| `CFullCompPlayer` | **confirmed by disassembly**, not name alone — see below |
| `CBandeCompPlayer` | present in `CompLogic.dll`, **absent from `CompLogicClassic.dll`** |
| `CNativeCompPlayer`, `CNativeCompStadt` | native-themed names |
| `CPirateCompPlayer` | pirate-themed name |
| `CHandelCompPlayer`, `CHandelCompStadt` | "Handel" = German for trade/commerce |
| `CHelpCompPlayer` | unclear role from the name alone |
| `CCompWelt`, `CCompInsel` | world/island-themed |
| `CCompStadt`, `CCityCompStadt` | city-themed, two variants |
| `CCompWork` | likely the "manager" class (see below) |
| `CBewertung` | German for "evaluation"/"rating" — plausibly the building-candidate scoring function identified as "FuncA" in the mine-placement research from earlier this session, not re-checked here |
| `CVLeaderReg`, `CCompGruppeReg` | registries, role not examined |

**Vtable sizes independently reconstructed from the x64 RTTI Complete-Object-Locator
structures, one pass, and compared against `complogic.md`'s own GOG slot
counts (§3/§6):**

- 7 `*CompPlayer` classes, **all exactly 42 slots** — matches `complogic.md`
  §3's GOG player-vtable family exactly (the base, computer/12-13, 11, 14,
  15, 16 vtables are all listed there as 42 slots too).
- 4 `*CompStadt` classes, **all exactly 32 slots** — matches §3's "Beside
  them are 32-slot runs: `0x100632e8`, `0x100633d0`, `0x10063760`,
  `0x10063968`... **Open:** the store that writes each of them," which had
  no name or confirmed purpose before this. **Hypothesis**, structural only:
  these are per-city AI state objects, one city-level `*CompStadt` instance
  per AI-owned city, paired with the `*CompPlayer` instance that owns the
  city list (GOG's `[this+0x9b8]`, see Part B).
- `CCompWork`: **exactly 13 slots** — matches §3's manager vtable
  (`0x100634f8`, constructor `0x1003d590`) size exactly.

**Disassembly cross-checks, one pass, not just name/size matching:**
- HE's `CompCreateInstance` constructs an object and stores the constant
  `0x7a120` (500,000 decimal) at `this+0x18`. GOG's manager constructor is
  documented (§4, not re-cited verbatim here) as storing the same
  `0x7a120` constant at a different but analogous offset, `this+0x10`
  (32-bit layouts differ by field padding, so the offset itself is not
  expected to match, only the magic constant).
- `CFullCompPlayer`'s vtable slot 0 reads `cmp r8d,1; jne skip;
  mov [rcx+0x1e9d],r8b` — matches the shape GOG's own player slot 0 is
  described as having ("sets `+0x1a89` to 1 when its second argument is 1,
  and to 0 otherwise" — again, different absolute offset, same conditional
  write of a one-byte flag from the second argument).
- `CCompWork`'s slot 0 increments `[this+0x14]` — matches GOG manager's
  `+0x1c` insert-counter semantics described in `complogic.md`.

These three independent, byte-level structural matches are why
`CFullCompPlayer` = GOG's computer/type-12/13 class and `CCompWork` = GOG's
manager class are stated above as **confirmed**, not merely name-plausible.

**The Classic diff, one pass, and a strong structural hypothesis for type 17.**
`CompLogicClassic.dll`'s RTTI is missing exactly one class versus
`CompLogic.dll`: `CBandeCompPlayer`. `complogic.md` §3 independently and
separately establishes, from the GOG binaries alone, that
`CompLogicClassic.dll` "has no separate type-17 constructor; type 17 takes
the failure path" and that Classic's player vtables have "41 slots, not 42"
against GOG `CompLogic.dll`'s 42 — i.e. GOG already proves type 17 is the
one class/slot Classic drops, entirely independently of the HE evidence.
Both builds therefore agree, by two unrelated methods, that there is exactly
one class present in `CompLogic.dll` and absent from `CompLogicClassic.dll`.
**Hypothesis** (strong, but not a byte-level confirmation the way the three
matches above are): `CBandeCompPlayer` = GOG's type 17, the "god party"
(`annoscene.md` §6, cited by `complogic.md` §3 as **likely**, not proven,
for that identification too). "Bande" is German for "gang/band" — a
plausible fit for a non-standard player kind, but this is name-plausibility
only, not independently confirmed by structure the way the other three
matches were.

**The remaining ambiguity: type 11 vs. type 16 ("Venetian").** `complogic.md`
§3 names GOG's type 16 "Venetian" and leaves type 11 unlabeled, noting only
that type 11's vtable slot 0 is `0x1004f050` instead of the shared
`0x10005690` every other player type uses — a distinct override, unexplained
in the existing doc. HE offers exactly two remaining unmatched classes for
these two slots: `CHandelCompPlayer` ("Handel" = trade/commerce) and
`CHelpCompPlayer`. Neither research pass resolved which maps to which.
**This note's own hypothesis**, built by combining both passes' evidence
rather than reported by either agent individually: `CHandelCompPlayer` =
type 16 "Venetian" (Venice's historical identity as a trade republic is a
direct semantic fit for a "Handel" class, and Part B below independently
finds, from the GOG side alone, that the documented type-16/Venetian slot
`0x1004cfc0` is a `COMP`-key-driven data dumper, consistent with a
commerce-/record-keeping-flavoured class), leaving `CHelpCompPlayer` = type
11, whose otherwise-unexplained distinct slot-0 override would then be
explained by it being a genuinely distinct, smaller-role class ("Help" —
an assistant/auxiliary player kind) rather than a full AI opponent. **Stated
as a hypothesis, not a finding** — nothing here traces an actual call or
byte pattern connecting "Handel"/"Help" to the numeric type 16/11, unlike
the three confirmed matches above.

**PDB reference found, file absent.** HE's `CompLogic.dll` carries a debug
directory entry referencing `complogic.pdb` with a specific GUID (one pass;
the GUID itself was read but is not reproduced here, see the research
agent's own report for it if needed). The `.pdb` file itself was searched
for and not found anywhere on this machine (**open**, S-level negative
result only — searched, not present on disk, not proof it doesn't exist
elsewhere).

**Incidental bonus, flagged for the separate mine/quarry/discovery-mask
thread, not chased further here:** scanning `AnnoWelt.dll`'s HE RTTI (155
type descriptors total, not exhaustively read) turned up `tBergwerkObject`
and `IBergwerkTab` — "Bergwerk" is German for "mine" — which is very likely
the individual ore/deposit object class that the mine-deposit follow-up
explicitly could not locate (see that note's Q4, "the ore object's own
class/vtable was never located"). Also found: `tMaskObject`, `tEnergyObject`,
`tHostObject`, `tFluesseObjectBau` ("Flüsse" = rivers, relevant to the
separate river-rendering research from earlier this session). **None of
these were traced beyond their names** — this is a one-pass S-level string
hit, not a disassembled confirmation of what any of them actually do or
whether their structure matches a GOG class the way `CFullCompPlayer`/
`CCompWork` were confirmed above. A dedicated follow-up pass against
`AnnoWelt.dll`'s ore-container class (`0x100c9b10` in GOG, per the
mine-deposit follow-up) using the same RTTI-then-disassemble method used
here would be the natural next step for that thread.

`AnnoFrame.dll`'s HE RTTI (90 type descriptors) was found to exist but not
examined in any detail — **open**, flagged as a lead for UI/cursor-related
questions (e.g. the quarry's overlay-height check, `0x1008b950`, which the
mine-deposit follow-up left as a GOG-only, UI-cursor-only finding).

## Part B — GOG `CompLogic.dll`: the computer AI's per-tick behaviour, slot by slot

**One pass**, continuing `complogic.md` §7's sub-task table (`T-AI.4`
onward) and §8's "Open" list. All addresses are `CompLogic.dll` unless a
module name is given explicitly — see the pitfall noted at the end of this
part about why that explicit tagging matters.

The computer class (`CFullCompPlayer` in HE's naming, Part A above; GOG
types 12/13, vtable `0x10063550`) is driven by nine per-tick "kind" slots,
dispatched from the already-documented per-advance helper (`complogic.md`
§4, `0x10003a20`), for every linked AI player whose activity byte is set. A
recurring field across almost every slot read, `[this+0x9b8]`, is the AI's
own linked list of the cities it owns — strong, repeated evidence (not
proven by a single read, but consistent across six of the nine slots below)
that this is the canonical "AI-owned cities" pointer.

| kind | vtable slot | address | what it does (one pass; see detail below for caveats) |
|---:|---|---|---|
| 0 | `+0x38` | `0x10041860` | top-level per-tick driver; walks the owned-city list scoring by `[city+0x120]`; applies personality/difficulty flags from `[this+0xa40]`/`[this+0xa48]`; runs an 8-state "found a new colony" expedition state machine scanning up to ~125 candidate sites |
| 1 | `+0x28` | `0x10006790` | per-city build-site scoring — **partially read**, cut off mid-function |
| 2 | `+0x34` | `0x1004a150` | per-city age-gated action/build-need driver: fixed one-time actions at city-age 1/6/10/12, steady-state build-need checks from age ≥ 14 |
| 3 | `+0x30` | `0x10006d30` | lazily initializes a per-city goal/task record (two budgets set to 127), then dispatches a 4-mode action by a stored mode byte |
| 4 | `+0x2c` | `0x100497c0` | ship "stuck too long" detector over a ring-buffer entity list (`+0xb04`/`+0xba8`/`+0xbac`) — **this exact ring mechanism is reused by the pirate class**, i.e. it is a shared base-class substrate, not computer-specific |
| 5 | `+0x3c` | `0x10043aa0` | ware-shortage/trade-need assessment against population/wealth thresholds 15000/32500/45000 |
| 6 | `+0x40` | `0x100444c0` | tiny 3-state try/wait/confirm wrapper — too short to characterize beyond "a small polling sub-check" |
| 7 | `+0x44` | `0x10008080` | 18-state settlement-founding state machine — likely the step-by-step executor of kind 0's colonization decision |
| 8 | `+0x48` | `0x10045bb0` | second ware/trade-expedition state machine, gated by a global semaphore at `0x1006a904` limiting concurrent expeditions, sharing kind 5's ware-selector field `[this+0xa34]` |

**Detail, kind 0 (`0x10041860`), one pass, full read (~500 instructions).**
In order: (1) a world-eligibility guard with an early return; (2) a
game-progress-threshold check against `[profile+0xfbfc]` (thresholds
`0x21b`/`0xc79`/`0x1943`) sending coded messages 0x153/0x791 via the shared
dispatcher `0x1003d530`; (3) the owned-city-list walk described above,
refreshing per-city caches and, only once a city's age counter
`[city+0x154]` reaches 13, calling the computer's own slot `+0x90`
(`0x1003f5b0`) — a periodic deeper per-city evaluation hook, itself unread;
(4) the personality/difficulty flag application; (5) the colonization state
machine (table `0x10041e90`), gated by sentinel `word[this+0xb00]==0xffff`
("no expedition in flight"), scanning candidates via two near-identical
helpers before committing a site via `0x1001a250`.

**The module-ambiguity pitfall, worth keeping as a general caution for
future RE passes on this project.** The research agent initially
misidentified kind 0's two site-scan helpers, `0x10027b50` and `0x10027cc0`,
as calls into `AnnoWelt.dll`'s already-documented `City::Build`/`Place`
functions, because this project's docs commonly cite bare hex addresses
with image base `0x10000000` — which both `AnnoWelt.dll` and `CompLogic.dll`
share. Re-checking the actual bytes at those addresses **in `CompLogic.dll`
itself** showed they are clean, padding-bounded, standalone functions local
to that module, unrelated to `AnnoWelt.dll`'s `Build`/`Place`. **This means
`OQ-271` ("does any kind slot call a city command slot") is not answered by
this finding and remains open** — what was found is two AI-internal
candidate-site-scan helpers, not a call into the city command path. The
general lesson: a bare address without its module name is ambiguous across
any two modules sharing the same image base, and should be double-checked
against the actual bytes in the module it's claimed to be in before being
relied on.

**Detail, kind 2 (`0x1004a150`), one pass, full read (~90 instructions).**
The clearest "what should this city build/do next" driver found. Fixed
one-time actions at `[city+0x154]` == 1, 6, 10, 12 call `0x10041720` (plus a
slot-`+0x68` call), `0x1003f160`/`0x1003f2b0`, `0x10008550`, and `0x10046630`
respectively. From age ≥ 14 it switches to checking a per-city candidate
array `[city+0x10c]` or an eligibility sub-object `[city+0x148]`, dispatching
to `0x100581b0` or `0x100582a0`. None of the four fixed-age target functions
or the two steady-state handlers were themselves entered.

**Detail, kind 3 (`0x10006d30`), one pass, short, full read.** Lazily inits
a per-city goal/task record the first time a city has no mode set
(budgets 0x7f=127); otherwise dispatches a stored mode byte (1-4) to one of
four tiny action wrappers, none of which were entered. Immediately adjacent,
a separate uncharacterized function at `0x10006e00` does a 7-criterion
weighted comparison between two records (signed per-field differences with
tolerance bands, clamped to [-7,7]) — not identified what it compares;
flagged as a lead, not resolved.

**Detail, kind 4 (`0x100497c0`), one pass, full read.** Round-robins a fixed
ring array of entities (bound `[this+0xba8]`, cursor `[this+0xbac]`, base
`[this+0xb04]`), tracking each entity's position (`+0x18`/`+0x1c`) against a
last-seen position (`+0x50`/`+0x54`); unchanged for more than 37,500 (units
not established — presumably ticks) fires recovery handler `0x10057b50`
(itself unread).

**Detail, kind 5 (`0x10043aa0`), one pass, full read (~300 instructions).**
Picks a ware type via selector `[this+0xa34]`, scans a fixed candidate table
(again via `[city+0x50]`, not `[this+0x9b8]` — this field recurs too and may
be a different, related list; not disambiguated here) collecting up to 4
matches, checks need per owned city against the three wealth-band
thresholds, then dispatches a shipment-looking action via `0x10008700` or
`0x10026f50` (neither entered).

**Detail, kind 6 (`0x100444c0`), one pass, full read, very short.** A
3-state wrapper: state 0 tries `0x10043e00` (success → state 1); state 1
tries `0x10044360` (success → state 0); state 2 tries `0x100440f0` (success
stays at 2). None of the three tried functions were entered; too small on
its own to characterize beyond "a small periodic polling check."

**Detail, kind 7 (`0x10008080`), one pass, entry + dispatch read, not every
state's target function entered.** An 18-state (0-0x11) sequential
state machine, each state calling one specific function and (mostly)
advancing by exactly one: site evaluate → reserve → several unlabeled
intermediate steps → at state 0xe, two cached fields copied from the AI's
own `+0x80`/`+0x7c` → a "has it been long enough since this record was
touched" check against interval `0x9c4`, again walking the owned-city list
→ terminal state 0x11. Read as the likely step-by-step executor of kind 0's
colonization decision, not confirmed by tracing a direct call from kind 0
into this state machine's entry.

**Detail, kind 8 (`0x10045bb0`), one pass, entry + dispatch read.** 11
states (0-0xa); one state pair gated by a global reference-count semaphore
at `0x1006a904` (incremented/decremented across calls, limiting concurrent
instances of this process), reusing kind 5's ware-selector `[this+0xa34]`.
Immediately following the dispatcher, a separate function `0x10045d80`
does per-city scoring keyed by the same selector against four lookup
tables (`0x10065e44`/`e64`/`e84`/`ea4`) — read as a second, more-committed
trade-expedition process complementing kind 5's assessment; the four lookup
tables' actual contents were not dumped or characterized.

### Pirates (2 of 12 slots, one pass)

- **`+0x24` = `0x10054710`** (`complogic.md`'s already-documented "TYP
  override" slot). Full read including its jump table: an 8-entry table
  collapsing to 4 distinct economic threshold pairs stored at
  `[this+0xaf0]`/`[this+0xaf4]`:

  | `TYP` value(s) | threshold pair |
  |---|---|
  | 0, 6 | 35000, 30000 |
  | 1, 5 | 40000, 30000 |
  | 2, 4 | 45000, 35000 |
  | 3, 7 | 50000, 40000 |

  This is a concrete, previously-undocumented numeric link from the
  scenario key `KAMPFTYP` (`complogic.md` §1's key inventory lists the key
  name and its string address; this pass does not re-derive that the key
  reader stores into `TYP`, which remains **one pass** from the earlier
  mine-placement research referenced in this session, not re-traced here)
  to actual pirate-aggression economics. A second function immediately
  after it, `0x100547b0` (not previously addressed in `complogic.md`), reads
  an AI-wide per-target table and either rolls the world PRNG (mask `0xf`,
  value 6 → 1-in-16 suppression) or sends coded status messages 0x143/0x144
  via the shared messenger `0x10026f50` — read as pirate
  aggression/tension escalation toward a specific target, gated by a dice
  roll; the messenger's own effect was not traced.
- **`+0x2c` = `0x10055a30`** (already-documented "attack walk"). Partial
  read (~190 instructions, not the full state table). Confirmed to walk the
  **same entity-ring mechanism** as the computer class's kind-4 ship-stuck
  detector — direct evidence the ring bookkeeping is a shared base-class
  substrate, not computer-specific, consistent with `CBaseCompPlayer`
  existing in HE's RTTI (Part A) as a genuine shared base. For each active
  ship, dispatches through a large state machine: a raw state byte (0-0x27,
  39 values) is translated through a byte table into a smaller jump table
  at `0x10055e60` — **not fully traced**; one branch tests
  `[this+0xa50]`/`[this+0x9b4]`/`[this+0xac6]` (read as a difficulty/activity
  gate, not confirmed) before resetting a ship to "state 22" via
  `0x10056b40`; another branch touches `[this+0xa90]`/`[this+0xa94]`, the
  **same field pair the computer class's kind-0 uses** at a different
  point in its own logic — another shared-substrate signal, not further
  explained. The full ~40-state table is flagged as a worthwhile target for
  whoever continues `T-AI.5`.

### Venetians (1 of 11 slots, one pass) — a `COMP`-key cross-reference

- **`+0x14` = `0x1004cfc0`.** Read far enough to find, and then directly
  confirm via a string dump (not inference from disassembly alone), that it
  builds `"%s=%d"`-formatted key/value lines using the literal strings
  `BAURECORDPOS`, `NATIVESTADTID`, `NATIVEANLAGEN` — three of the `COMP`
  scenario keys `complogic.md` §1 already lists by name and address, with
  no identified reader until now. This settles, at the level this pass
  actually read, that this Venetian-class function is a `COMP`-style
  serializer/dumper of native-settlement-related data (a build-record
  position, a native city id, native buildings) — i.e. **the Venetian
  class tracks references to native settlements**, which was not previously
  stated anywhere in `complogic.md`. It walks the owned-city list
  (`[this+0x9b8]`, same field as kind 0/2/5/7/8 above) reading per-city
  fields `+0xfe9c`, `+0xfe94`, `+0x120`/`+0x121`.

### Natives (1 of 6 slots, one pass) — a self-corrected misreading

- **`+0x18` = `0x10051da0`.** The research agent's own first-pass read
  guessed this was a dialogue/mood-keyword parser; checking it against the
  actual embedded strings showed that guess was wrong. The string dump
  instead shows sequential string comparisons against `TYP`, `FLAECHE`,
  `LAGER`, `HANDEL`, `TRANSPORT`, `PROZENTE`, `OPTIONAL`, `TRIBUTSPERRE`,
  each assigned a small type code — all eight are, again, `COMP` scenario
  keys already listed in `complogic.md` §1. **This is the native class's
  own `COMP` key-to-value-type reader, not a dialogue/reaction parser as
  first guessed** — recorded here specifically because the self-correction,
  not just the final answer, is worth keeping: a plausible-sounding first
  read was checked against the actual strings and overturned by them,
  exactly the discipline this project's evidence rules ask for.

## Part C — what this changes in `complogic.md`'s open-question list

Cross-referencing §8 directly (not editing it):

- **OQ-271** ("does any kind slot call a city command slot?") — **still
  open**. The one candidate this pass found (kind 0's site-scan helpers)
  turned out, on closer inspection, to be `CompLogic.dll`-internal, not a
  call into `AnnoWelt.dll`'s command path (see the module-ambiguity pitfall
  above). Next: `0x10041860`'s remaining untraced calls, or any of kind 1/3/6's
  unentered target functions.
- **OQ-272** (calls on world `+0x110`, vtable `0x100215a4`) — not touched
  by this pass, still open as stated.
- The 32-slot interface identity (§3/§8) now has a structural
  **hypothesis** from HE (per-city `*CompStadt` state, Part A) where before
  there was none — still **open** for a GOG-side confirmation (who writes
  each of the four 32-slot pointers, and into what).
- The "which slot Classic dropped" question (§8) has independent
  corroborating evidence from HE (`CBandeCompPlayer`/type 17 is the one
  class HE's `CompLogicClassic.dll` lacks too) but is **not itself answered
  at the GOG byte level** by this pass — `complogic.md`'s own suggested next
  step ("compare `0x10060490` with `0x10063550` slot by slot") was not
  carried out here.
- `T-AI.8`/`T-AI.9` (type 11, type 17) were not reached by the GOG-side
  pass at all this round.

## Open items / suggested next steps, consolidated

1. Compare `AnnoWeltAddOn.dll` (HE, still 32-bit x86, 2019) against GOG
   2.0.0.5's `AnnoWelt.dll` by the project's existing byte-level technique —
   the one HE binary that might not need an independent x64 analysis.
2. Resolve the type-11-vs-16 / `CHelpCompPlayer`-vs-`CHandelCompPlayer`
   ambiguity in Part A with an actual disassembly trace, not the
   semantic-plus-structural hypothesis offered here.
3. Finish kind-slot 1 (`0x10006790`, cut off mid-function) and kind-slot 6
   (`0x100444c0`, too short to characterize alone — its three called
   functions `0x10043e00`/`0x10044360`/`0x100440f0` are the real content).
4. Trace pirate's full ~40-state attack-walk table (`0x10055e60` onward)
   and the remaining 10 of 12 pirate slots.
5. Read the remaining 10 of 11 Venetian slots and 5 of 6 native slots.
6. Reach `T-AI.8`/`T-AI.9` (type 11, type 17) on the GOG side.
7. Follow up the `tBergwerkObject`/`IBergwerkTab` lead from HE's
   `AnnoWelt.dll` RTTI against the mine-deposit thread's still-unlocated
   ore-object class (separate thread, flagged here only as a pointer).
8. Examine `AnnoFrame.dll`'s 90 HE RTTI entries, likely relevant to cursor/
   UI questions (the quarry overlay-height check, among others).
9. Disassemble `CBewertung`'s located vtable slot and trace its callers —
   it was only name-matched against the mine-placement research's "FuncA"
   hypothesis, not confirmed.

## Orchestrator check, 2026-11-07

The Claude Code orchestrator spot-checked three claims, read-only; the rest of
this note keeps its own tags.

- **HE's CompLogic modules are x86-64 and carry RTTI; GOG's carry none.**
  Verified: HE `CompLogic.dll` and `CompLogicClassic.dll` are `pei-x86-64`
  (objdump). The `.?AV` type-name prefix occurs 21 times in HE
  `CompLogic.dll`, 20 times in HE `CompLogicClassic.dll` (one fewer, as
  Part A's Classic diff says) and 0 times in either GOG module.
- **The pirate `TYP` table.** Verified: `0x10054710` jumps through the table
  at `0x10054784`. Entries 0 and 6 store 35000/30000 into `+0xaf0`/`+0xaf4`
  (`0x10054726`), 1 and 5 store 40000/30000 (`0x10054754`), 2 and 4 store
  45000/35000 (`0x1005473d`), 3 and 7 store 50000/40000 (`0x1005476b`). This
  matches the table in Part B. That the scenario key `KAMPFTYP` feeds this
  `TYP` stays **one pass**.
- **The date in the title** was corrected from 2026-11-08 to the day of the
  pass, 2026-11-07.
