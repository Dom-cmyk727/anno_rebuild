# Wildlife load boundary — WILDLIFE-LOAD-1, 2026-11-18

This packet adds a raw **formats accessor only**. It changes no ScenarioSpec,
World, tag, Rules, save, registry, default or draw site. KBEWOHN and the pure
wildlife helper candidate are independent branches, not source dependencies here.

Evidence: **D** original instructions/data, **P** parsed original content,
**C** current rebuild code, **U** unresolved. GOG AnnoWelt.dll SHA256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
The [raw format](../formats/insel.md#hirsch-raw-spawn-records-2026-11-18-wildlife-load-1)
and [existing addressed reads](../re/wildlife.md#3-hirsch-loader-and-thirschobject)
remain separate from a persistent-state implementation contract.

## What the bounded read establishes

- **D, `0x100946dc`, `0x100b4b20`–`0x100b4b6c`:** the HIRSCH handler compares
  the supplied chunk header with the tag. It does not search the island for a
  first HIRSCH. A nonmatching header delegates to `0x10089130` at `0x10094848`.
  Absence of that header and a supplied empty HIRSCH are different inputs; neither
  requires a fabricated spawn record. Empty HIRSCH has count 0 and returns success.
- **D, `0x100a2ea0`–`0x100a2fbc`:** the INSEL5 path reads its 12-byte head,
  obtains or creates the island (`0x100a2f53`–`0x100a2f83`), then reads and
  dispatches child headers in stream order through island slot `+0xdc` at
  `0x100a2fb0`. The wildlife vtable `0x100cac2c`, installed at `0x10094c68`,
  holds `0x100946c0` at slot `+0xdc` (`0x100cad08`). This local loop does not
  deduplicate names. The new first-record convenience accessor follows existing
  C++ record() policy, not a claim that the original discards later duplicates.
- **D, `0x1008c0de`, `0x1008c2dc`–`0x1008c2e6`:** island initialization
  stores its world pointer at `+0x20`, then takes world property 4 through slot
  `+0x74` and stores its low 16 bits at island `+0x9a`. The HIRSCH handler
  reads the receiver's existing `u16 +0x9a` before entry iteration
  (`0x100946e9`–`0x10094705`). **Confirmed factual correction:** the inherited
  `/1000` and base range 0–65 are withdrawn. The multiplier `0x10624dd3`
  and total shift 37 compute `floor(u16_time / 500)`, with base range **0–131**.
  The smallest distinguishing input is 500: result 1 rather than 0; input
  65535 gives 131. Maximum base plus jitter 31 is 162. An integer check covers
  all 65,536 inputs, and the Mac orchestrator independently rechecked the
  matching DLL on 2026-11-18. Original clock units are not inferred. No time
  conversion is implemented here; reuse/template initialization ordering still
  requires its own contract. The preserved helper/oracle correction is separate.
- **D, `0x100947d3`–`0x100947ef`, getter `0x100a3790`–`0x100a37bb`:**
  world slot `+0x10c(-1)` reads bit 0 of world `+0x69cc` without changing it.
  If set, effective spawn goods become `0x00ff`; otherwise retain file goods.
  The raw decoder applies neither branch. The mode's player-facing meaning
  remains unresolved (OQ-576); engine settings must not be guessed equivalent.
- **D, `0x100947f3`–`0x10094816`:** after a successful allocation, a clear
  entry byte 4 bit 0 consumes one world slot `+0xb4` draw, takes `draw & 31`
  and adds it to the base, storing a byte. A set bit uses the base without a
  draw. These are **load-time** draws, not first-advance draws. Other raw flag
  bits and byte 5 are not read in this body and are retained by the decoder.
- **D, `0x10094744`–`0x10094797`, `0x1007bd50`–`0x1007bd60`:** local
  coordinates select the chunk; each new spawn record is prepended to chunk
  `+0xc`. For same-chunk file entries A then B, runtime traversal is B then A.
  File order for decoding/jitter and chunk traversal order for later sweeps
  must not be conflated. Cross-chunk sweep order is a separate caller rule.
- **D, `0x10094786`–`0x10094822`:** the 16-byte runtime object is not the
  eight-byte file record. It gets category 0, effective goods and timestamp;
  occupied bit `0x40` is cleared. Its saved figure id at `+0xc` is not initialized
  by this loader. Do not turn that uninitialized field into a loaded figure/id.
  The decoder creates no runtime object; later materialization remains separate.

## Where the next import belongs

**C:** `formats::insel::Island::records` retains the ordered raw chunks and
`hirsch_records()` exposes the first one as owned field values. Explicit
`decode_hirsch` can handle every preserved duplicate if a later loader needs it.
`sim::island_from_insel` in `scenario_formats.cpp` currently reports HIRSCH as
UnknownRecord; `ScenarioSpec::IslandSpec` contains no spawn inputs. World::create
therefore receives none. This packet intentionally leaves all of that unchanged.

**C:** `DrawPhase::Load` already exists. `DrawSite::WildlifeSweep` is an unported
advance-phase site, not a declaration for the load call at `0x10094808`.
Reusing that site or postponing the draw would mislabel the phase and shift the
world cursor. A distinct load-site contract is required before implementation.
The engine's sweep/event stamps do not by themselves prove equivalence to the
original initialization of island `+0x9a` on every import/reuse path.

A small next packet should establish and test, in this order:

1. Raw scenario/template import with widened coordinates, source ordering and
   explicit missing/present-empty policy. No species or figure id is inferred.
2. Apply the separately reviewed load-contract correction, then establish the original
   world-mode input and island-time input at that import point.
   Keep raw goods distinct from any mode-dependent effective goods decision.
3. Load-time jitter at its ordered place among existing load draws. Save/reload
   resumes persisted timestamps/PRNG state; it must not reroll them.
4. Stable per-chunk ordering derived from the established prepend semantics,
   retaining chronology when records are later added/reused. Materialization,
   occupancy release, animal life and population caps remain later units.

## Coordination needed before persistent code

Proposed semantic registrations, **not allocated here**:

| Section | Proposed name | Contract the next packet must settle |
|---|---|---|
| header | wildlife_rules | Default-off input, omitted at default, using ExtensionContext rather than an unhashed mirror |
| islands | wildlife_spawn_records | Per-island source presence/order and effective spawn state: coordinates, goods discriminator and computed timestamp; exact canonical fields/order reviewed before code |
| figures | later animal feature, separate packet | Only after creation/lifecycle proves which saved live fields and links are needed; no uninitialized source figure id |

Request coordinated feature tag value(s) under ADR 0006 and state.md sections
9.7–9.8 before writing these payloads. No numeric tag, schema field, timeline or
version is assigned by this document. A future implementation must demonstrate
feature-off byte identity and ordinary native/replay continuation with the flag on.

**U — bounded handoff gaps:** complete global order of existing versus reused
islands, templates/links, buildings/actors, player/city draws and script INIT;
world-mode provenance; upstream admissibility of duplicate HIRSCH headers; and
later callbacks/materialization. The local child-header loop and fresh initializer
do not settle those global questions. Stop that dependent implementation until
its contract is evidenced. This raw-decoder packet needs none of those decisions.
History Edition/running-original execution is NOT_RUN; no live wildlife claim.

**Resolved primary question / next dependency:** `/500` and base 0–131 are
independently confirmed. The orchestrator owns a separate correction of the
preserved wildlife read/helper/oracle boundary before persistent integration.
Those files are unchanged by this formats packet. Its decoder, strict-length
policy and corpus proofs compute no timestamp and need no rerun for this note.
No decoder policy question remains; the global ordering/provenance gaps above
still delimit the next persistent-state packet.
