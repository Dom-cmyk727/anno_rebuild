# Wildlife initialization — WILDLIFE-INIT-1, 2026-11-18

This is a research/reference contract, **not a World implementation**. The
fresh-load profile below is a candidate for the faithful port. Its local HIRSCH
semantics are established; **whole-profile original RNG alignment is
NOT_ESTABLISHED**. A concrete load-resource precursor must be completed before
claiming that alignment. No divergent initialization phase is approved here.

Evidence: **D** matching original instructions/data; **P** parsed original files;
**C** current rebuild code; **A** explicit rebuild contract; **U** unproved.
Image base is `0x10000000`. GOG AnnoWelt.dll SHA256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`;
AnnoScene.dll SHA256
`5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38`.
Only paraphrases and compact measurements are published; raw reads stay ignored.

The raw formats boundary is read at decoder `f1e245d2`, and the reviewed / 500
correction at `533ee3fc`, by pinned ref only. Neither implementation is taken
into this branch. The relevant existing notes are [wildlife sections 1–3 and
11](../re/wildlife.md), [island initialization](../re/islands.md),
[world loading](../re/annowelt.md#world-save-and-load-the-accumulators-t21-2026-10-23)
and [scene loading](../re/annoscene.md#1-entry-points-and-classes-annoscenedll).
The / 1000 load statement in the older baseline wildlife note is superseded by
the addressed correction at 533ee3fc, not reused as an oracle here.

## 1. First profile and its observable boundaries

**D/C — candidate:** a new scene/world instance, explicit nonzero original seed,
no advance during loading, embedded INSEL5 islands with unique slots, and no
INSELLNK placement. HIRSCH payload lengths must be divisible by 8 and local
coordinates valid for the island; the reference models successful allocations,
not malformed-file or allocation-failure recovery. A new world has an empty island table, so each first slot is
fresh. Scene flag `+0x164 == 0` selects construction; a nonzero flag calls the
existing world's Init instead (AnnoScene `0x10002e44`–`0x10002ea6`). A nonzero
seed is passed as world property 8 (`0x10002ec3`–`0x10002ed4`); it is not silently
inferred from a raw island stamp. This is an input precondition, not a default flip.

**D — exclusions:** an already populated world/table or repeated island slot can
reuse an island (`AnnoWelt 0x100a2f53`–`0x100a2f5c`); links take a different
placement path (AnnoScene `0x1000a016`–`0x1000a05b`, `0x10008490`). Original
savegames/reused instances and linked templates are separate profiles. Their
exclusion does not mean their draws are zero. Rebuild native reload is a separate
restoration operation (§5), not a replay of original scene callbacks.

**P — representative:** `Tutorial/01Discover01.sze` contains one embedded island,
slot 11, no INSELLNK and no cities. The first player headers have kinds 8 and 2;
neither satisfies the unnamed-computer kind-12 gate. A later player block has
one actor; BIGBROTHER is present. The island's HIRSCH is child ordinal 71,
156 records, all with byte-4 bit 0 clear. It follows HEIGHT/GFXID/GELAENDE and
other headers. MAIN is present: its seconds offset and bit 1 are not the island's
sampled tick or wildlife mode bit. This is a structural inventory, not a runtime
canary or a claim that every callback has been closed.

## 2. Mode, stamp and receiver identity

| Evidence | Established rule |
|---|---|
| D — world table `0x100ccdd8`, inherited entries `+0x70`, `+0x74`, `+0x10c` | SetProperty `0x100a3230`, GetProperty `0x100a3380`, mode query/set `0x100a3790`; the receiver is the whole world object, no subtracting a monopoly subobject offset |
| D — AnnoWelt `0x100a3790`–`0x100a37bb` | Argument -1 queries bit 0 at world `+0x69cc` without writing. Other arguments replace that bit from argument bit 0, preserving the other bits; return the resulting 0/1 |
| D — constructor `0x100af3e0` → base `0x100a3980`, `0x100a3a4b`–`0x100a3a69`, `0x100a3acd` | Bit 2 is cleared, tick dwords including `+0x10c` are zeroed, and bits 0/1 are cleared. Derived constructor installs `0x100ccdd8` at `0x100af442` |
| D — Clear `0x100af24b`–`0x100af27e` | Clears bit 1 and resets tick/cursor fields; this is not a clearing of wildlife bit 0 or constructor-equivalent proof for every reused load |
| D — MAIN `0x100ae30b`–`0x100ae32a` | Restores bit 1 from its saved Boolean and seconds offset at `+0x69d4`; preserves bits 0 and 2; does not restore tick `+0x10c` |
| D — getter dispatch table `0x100a345c`, entry 4 → `0x100a33c5`–`0x100a33cb` | World property 4 returns the u32 at `+0x10c`, distinct from property 7's divided value plus seconds offset |
| D — island `0x1008c090`, `0x1008c0de`, `0x1008c2dc`–`0x1008c2e6` | Stores the passed world pointer at island `+0x20`; once during initialization calls its `+0x74(4)`, storing only low 16 bits at island `+0x9a` |
| D — loader `0x100946e9`–`0x10094705` | Samples that existing u16 before the header's record loop and divides by 500. No physical unit follows solely from this divisor |

For the fresh, non-advancing construction edge the initial tick and sampled raw
stamp are 0. MAIN's nonzero saved seconds would not change that. This does not
extend to existing islands whose stamp was sampled earlier. The header's mode
query uses island `+0x20`, not its chunk or the 16-byte spawn object
(`0x100947d3`–`0x100947d8`). Its bit-0 meaning remains U beyond the established
goods override; it is not equated with an engine difficulty or wildlife switch.
The reference accepts explicit raw stamps and query results so these distinct
inputs remain testable, rather than baking a fresh-world assumption into every load.

## 3. Ordered headers, allocation and list identity

**D — global file dispatch:** AnnoScene `0x100026db`–`0x10002720` reads SCENE
children sequentially and calls scene `+0x5c`; its ordinary fallback forwards to
world `+0x78` (`0x1000a375`–`0x1000a396`). The derived world handler delegates
INSEL5 to `0x100a2ea0` (existing world-load read). That routine first obtains or
creates the slot, then reads child headers sequentially and calls island `+0xdc`
(`0x100a2f8c`–`0x100a2fbc`). No numeric-slot sort or header-name deduplication
occurs in this loop. Post-header world callbacks at `0x100a2fc8`/`0x100a2fd7`
remain separate transitive edges, not assumed to consume zero draws.

**D — local dispatch:** HIRSCH receiver `0x100946c0` compares the supplied header;
a different tag delegates to `0x10089130` (`0x10094848`). Absent means no HIRSCH
call; present-empty means a successful call with count 0. Repeated headers are
separate calls. The format accessor exposing a first header is not the original
caller policy. New code must walk the ordered record list, not only that accessor.

**D — draws and traversal:** each successfully allocated record is prepended to
its chunk list (`0x10094770`–`0x10094797`, `0x1007bd50`). Only after that does it
query mode and, if raw byte-4 bit 0 is clear, consume a world `+0xb4` draw at
`0x10094808`, add its low 5 bits to the divided base, and store the timestamp
byte. Set bit 0 consumes none. Mode 1 substitutes goods 255; mode 0 preserves raw
goods (`0x100947d3`–`0x10094816`). Same-chunk records A then B get draws in A/B
order and later traverse B/A. Different chunks do not reverse each other's draw
assignment. This packet proves no new cross-chunk sweep schedule.

**U — allocation:** a failed record allocation branches past initialization
(`0x1009477a`–`0x1009477c`). The reference models successful allocation only;
record counts alone do not establish total original jitter draws under failure.
No allocation-failure policy is invented for production here.

## 4. Concrete missing precursor; finite next read

**D — original path:** the non-HIRSCH GELAENDE branch in `0x10089130` compares
that supplied tag at `0x10089301`–`0x1008930f`, resolves each type through the
catalogue pointer at island `+0x1c` (`0x1008938c`–`0x10089398`), admits the
placement through island `+0x6c` and commits it through `+0x78`
(`0x100893f3`, `0x10089417`). Type slot `+0x5c` is grouped by 32; group 225
(types 225–256) then reaches **one world `+0xb4` draw** through island `+0x20`
at `0x10089455`–`0x10089468`. Catalogue `+0x88(type, island byte +0xc0, draw)`
selects a value at `0x10089486`; its low nibble replaces the high nibble of
a **separate per-tile byte plane at island `+0x18`**, preserving the low nibble
(`0x1008944f`–`0x1008949d`). This is not the loaded growth phase or spare bits
in the base tile record. The initializer separately allocates/zeros that W × H
byte plane (`0x1008c1e4`–`0x1008c217`). Catalogue `+0x88` is not the world
creation slot of the same numeric offset. Its remaining closure is not invented. World `+0x18` getter `0x100a37d0`
returns world `+0x110`, the catalogue source stored at island `+0x1c` by
`0x1008c0c9`–`0x1008c0db`. In matching AnnoGame.dll (SHA256
`ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`), four candidate
tables `0x100213d0`, `0x100214b0`, `0x100215a4`, `0x10021694` share the known
`+0x70` lookup `0x100037e0` and `+0x88 = 0x10001b30`. **D/U:** those table
entries are verified; confirming the dynamic table and reading `0x10001b30` is
the minimal next edge, not a claim that its transitive draws are already closed.

**P/D — smallest representative witness:** the first nonempty GELAENDE header
is ordinal 34. Its first resource placement has type 70, kind 226, 1 × 1 footprint,
local (59,29), flag word 0x29, and four HEIGHT corners 48. Earlier terrain blocks
contain no object. The constructor initializes object ids to 0
(`0x1008c10a`–`0x1008c11b`). The existing addressed placement predicate
`0x1007c790` accepts this empty land cell: bounds hold, the quad mean is 48 and
there is no conflicting object ([placement reference](../../tools/placement_ref.py)).
This is one demonstrated resource-load draw before HIRSCH, not an inference from
the mere presence of a type. Parsed resource candidates before HIRSCH total 4,589;
**that is not an actual-draw count**. The scene has no links or cities to excuse
this particular predecessor.

**C — missing in the rebuild:** `island_from_insel` copies scenery facts and
`build_island` applies them without this original resource-variant draw or its
separate-plane result. Copying the existing growth phase is not a substitute. The
current manifest includes missing computer/link/native/city-name draws but does
not describe this GELAENDE precursor. No counterclaim of alignment can come from
feeding only 156 jitter draws to the wildlife helpers.

**Technical prerequisite OPEN, not a renewed permission request:** finish a
bounded faithful GELAENDE-load read/contract: caller admission/order, the
catalogue `+0x88` target and any draws it makes, then a per-record resolved draw
schedule over this representative. Also close the finite surrounding calls:
original **import** admission `+0x6c` and island placement `+0x78` at `0x10089417`, post-header world `+0x1d4`/`+0x1d0`,
and the original actor load path reached by the later player block. Player/city
creation and BIGBROTHER callbacks must be classified if the supported profile is
widened past this load prefix; no whole-module/lifecycle read is requested.

Unnamed computer names occur only after file reading and only for empty-name
kind 12 players (AnnoScene `0x1000291f`–`0x10002964`); this representative's header
kinds exclude them. With no INSELLNK, the placement loop's empty-list arm returns
without ore/native/treasure placement (`0x10009562`–`0x10009576` selection early return and `0x1000849f`–`0x100084b8` placement loop, existing
[link read](../re/annoscene.md#5-island-selection-0x10009550-annoscenedll)). With
no STADT/STADT2 it has no city-name-load call (existing
[city creation E15](city-creation.md)). Other maps do not inherit these exclusions.

Full-profile original RNG alignment is **NOT_ESTABLISHED**, not declared impossible
for every profile. Continuing the authorized faithful port through this bounded
prerequisite needs no new owner policy. Choosing a different, post-import rebuild
RNG phase would change ordering and would require explicit owner approval. This
packet neither recommends nor implements that alternative as a shortcut.

## 5. Independent executable examples

[Reference](../../tools/wildlife_init_ref.py), [vectors](../../fixtures/synthetic/sim/wildlife_init_vectors.jsonl)
and [public-command contract](../../tools/wildlife_init_ref_contract_test.py) use
JSON records, one per non-comment line: `name`, `input`, `expected`. Arrays make
island/header/record order explicit without quoting an original script or inventing
a compact file format. Every expected output is hand-derived, not generated by
executing the model. Exit 0 agrees, 1 disagrees, 2 refuses malformed, unknown,
missing, empty or exhausted input. Unused trailing raw u32 draws are allowed.

Each island supplies its slot, raw u16 stamp and an ordered list of HIRSCH or
resolved intervening-draw events. Duplicate/reused island slots refuse in this
fresh-load model. Unknown headers refuse; the model never treats an unread
callback as zero draws. A draw event supplies its resolved count and site label;
**the caller must prove that count and position separately**. Ordinals in output
are positions in the supplied model arrays, not guessed offsets in an original
file. The later adapter must retain the actual raw source ordinals independently.

14 authored vectors (12 load, 2 restore) cover out-of-numeric-order islands; two same-chunk versus
two-chunk records; absent, empty and repeated headers; mixed/unknown flag bits
and raw byte 5; modes 0/1 with only goods changing; raw 499/500/65535 and maximum
jitter 31; one resolved, evidenced resource-load draw before jitter; and two
**A — rebuild persistence** examples. The resource example's values/count are
authored: it illustrates the proved admitted-placement path, not the full map
schedule. Chunk rows use a canonical report ordering, not an original sweep claim.

For rebuild reload, persisted effective goods/timestamps/source chronology are
validated and restored without consuming draws, converting stamps again or
reapplying mode. Source chronology reconstructs the initial per-chunk prepend
lists. This is a proposed rebuild contract, not evidence of original saved-runtime
wildlife state. It models no occupied/live animal, saved figure id or later insert.

The contract proves an applied reversed-record mutation fails the exact timestamp
assignment while a legal control passes, and applied / 1000 fails raw 500 while
raw 499 still passes. No generic mutation campaign or world generator is added.

## 6. Proposed import/persistence boundary; no numeric tags

**C — current code:** `load_scenario` first collects embedded islands in file
order, then players/cities/actors; link resolution later transfers its cursor to
`ScenarioSpec::seed` when links exist (`scenario_formats.cpp`,
`resolve_scenario_links`; `sim_load::load_scenario_with_links`). World::create
constructs the RNG from that seed and parallelizes `build_island`, then places
results in its slot table. Script INIT runs after creation in normal session paths.
This reordered rebuild pipeline is not the original scene/header dispatch loop.

**Proposed code placement, conditional on §4:** a serial, evidenced initialization
schedule must consume draws in the original supported profile's order and carry
its final cursor into World::create. It cannot blindly append wildlife after link
resolution, and cannot draw a shared RNG in parallel `build_island`. Parallel
jobs receive already resolved immutable spawn facts only. The resource prerequisite
must apply the original variant result to its separate plane, not merely burn a
dummy draw or overwrite growth phase/auxiliary tile bits. The persistence/data
representation of that plane remains a prerequisite boundary; the spawn payload
below does not silently represent it.
With the feature off, bypass this new schedule entirely: keep the existing link
cursor transfer, object phases, World bytes and defaults unchanged. No new precursor
or wildlife draw is made on that path; this preserves the current rebuild baseline,
not a claim that its omitted original draws were already reproduced. Where a profile has
links/player/actor/script draws, their placement in this schedule must be proved
or that profile refused explicitly; no silent fallback/projection/suppression.

The eventual jitter site is load-phase and feature-local under the coordinated
registration. It must not reuse `DrawSite::WildlifeSweep`, which is an advance
site. The resource precursor needs its own truthful load-site identity; charging
its draws to wildlife jitter would hide the demonstrated ordering dependency.
No base DrawSite enum, PRNG timeline or manifest is changed by this document.

Semantic payload proposal under [ADR 0006](../adr/0006-save-sections.md) and
[state sections 9.7–9.8](state.md#97-section-versions-and-extensions-2026-11-14-save-sections-1a):

- **Header / wildlife_rules:** explicit u8 enabled (default 0), and explicit u8
  original import-mode bit if the approved profile needs it. It is not a mirror
  Rules field; use ExtensionContext. Invalid Boolean values or nondefault feature
  state while disabled refuse. A feature-off world emits no added entries.
- **Islands / wildlife_spawn_records:** explicit u8 initialized (default 0), u64
  source-island ordinal, u16 raw sampled stamp, then u64 HIRSCH-header count.
  Per header: u64 actual source-header ordinal, u64 record count. Per record in
  source order: u64 source-entry ordinal, widened u32 local x/y, u8 raw flags,
  u8 raw byte 5, widened u32 raw goods and effective goods, u8 effective timestamp.
  These are proposed field widths/order, not a struct dump or original wire format.
- `initialized = 1` distinguishes enabled import even with absent HIRSCH. Present
  empty HIRSCH retains an explicit zero-record header. Repeated headers remain
  distinct; source header/entry order strictly increases. Per-chunk indices are
  derived by source-order prepend, not duplicated mutable saved lists.
- Validate initialized/domain values, source ordinal uniqueness/order, coordinates
  against actual island geometry, goods against the caller's registry/sentinel
  contract, header/record counts against bytes, enabled/payload consistency and
  absence/default rules. Byte-4 unknown bits and byte 5 remain raw provenance.
  No original pointer, vtable, uninitialized figure id or live occupied state is
  imported. Allocation failure policy remains explicit, not a silent skipped draw.
- Native loading restores effective fields and PRNG cursor transactionally; it
  never runs original-file initialization again. Header and islands extensions
  omit at defaults. No numeric tag is allocated and no field/payload is written here.

## 7. Next-code checklist and stopping boundary

1. First finish the bounded faithful resource-load prerequisite of §4 and its
   resolved schedule. Hand-derive a first-placement vector including its separate-plane write, and distinguish one
   skipped precursor draw from the correct first HIRSCH timestamp/cursor.
   Use the original import-admission contract, not BuildCmd `placement_rule`:
   that engine command deliberately rejects scenery and must not be widened to
   make an import pass. Confirm selector/admission callees, define a distinct
   representation of the variation plane and its native continuation, and do not
   alias growth phase or borrow unrelated tile bits.
2. Coordinate semantic feature tags with the board owner. Only after that:
   formats' explicit decoder → ordered ScenarioSpec spawn inputs with actual source
   ordinals, repeated/empty/absent checks; retain raw data separately from effects.
3. `scenario_formats.cpp` / `sim_load`: serial scheduling and cursor transfer for
   one approved fresh profile; reject unsupported reuse/links/unclosed callbacks.
   Compare actual source order against a numeric-slot-sort negative control.
4. `scenario.cpp`: apply precomputed effective records in parallel without RNG
   access; validate geometry/registry and per-chunk prepend identity. No animal
   creation. A tiny synthetic scenario alone cannot prove the real-file profile.
5. ExtensionContext / state field functions: explicit header/islands payloads,
   feature-off byte equality, enabled native round-trip and identical continuation,
   malformed count/domain/order refusal and no reroll on reload. No new state number.
6. Public normal-loader representative test after prerequisite closure: prove
   precursor and jitter counts/assignment, source identities, deterministic repeat
   and save/reload; 0 pins moved while disabled. Retain only affected gates.

**Stop:** no World/ScenarioSpec code, tag reservation, fauna sweep, species creation,
materialization, hunting or default change in this packet. Original/HE runtime is
NOT_RUN. The local model does not certify the complete original world draw cursor.
The orchestrator reviews this bounded contract and chooses the next faithful
prerequisite; an owner question is needed only for a proposed policy divergence.
