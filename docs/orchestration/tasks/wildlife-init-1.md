# WILDLIFE-INIT-1 — prove the first wildlife initialization profile

**RELEASED 2026-11-18** after independent local review of WILDLIFE-LOAD-FIX-1
at `533ee3fc96a560f1c3189f3a4d32ce113abff65a`. Exact-head
CI and acceptance remain separately recorded; this research/reference unit does
not consume a pending implementation. About 2–4 hours, EXECUTOR II MAC, direct
reportback. The goal is a sufficiently precise contract for the next packet to
implement actual default-off wildlife import and native persistence.

## Intake and read order

Use new branch `task/WILDLIFE-INIT-1` from the independently accepted
`2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`, preserving all active branches and
private evidence. Read the decoder boundary at exact
`f1e245d2769306451d4c2079aadf6a64be93defd` by ref. Read correction head `533ee3fc96a560f1c3189f3a4d32ce113abff65a`
by ref too; no source intake,
merge or rebase is needed for this packet. Ordinary-push only your task branch.

Read CLAUDE, PLAN0.1–0.3, TASKS owner rules/section1, testing/evidence-contract.md
and testing/README.md, then:

- `docs/re/wildlife.md` sections1–3 and11, with the reviewed /500 correction.
- `docs/sim/wildlife-load-boundary.md` from the decoder pin above.
- `docs/re/annoscene.md`, `docs/re/annowelt.md`, `docs/re/islands.md` sections
  reached by the initialization edges below; do not restart whole-module research.
- `src/sim/src/scenario_formats.cpp`, `src/sim/src/scenario.cpp`,
  `src/sim_load/src/scenario_load.cpp` and their public input types.
- `src/sim/include/anno/sim/draw.hpp`, `extension_context.hpp`, ADR0006,
  `docs/sim/state.md` sections9.7–9.8 and the current feature-tag registry.
- The small existing wildlife references and public-command contracts, to reuse
  their conventions rather than build another test framework.

## I0 — choose one achievable first profile

Write an acceptance table before tracing. Prefer **fresh original-scenario import
with embedded islands** as the first profile. Treat existing/reused islands,
linked templates and native-save restoration separately. An observable input
precondition must justify each exclusion; “not implemented” does not imply zero
draws. Inventory one representative original scenario against those conditions.
A tiny synthetic scenario can isolate ordering but cannot prove the real-file
profile by itself. Keep the production map and all defaults unchanged.

Current importer warnings explicitly omit some original draws: unnamed computer
names, linked ore and treasure placement, and template/native names. Establish
whether they can occur in the selected profile. Do not claim full original PRNG
alignment merely because wildlife's local draw count is correct. Name any other
unread intervening edge needed for the selected profile, with a finite call path.

## I1 — read only the necessary original edges

Pin module hashes and receiver adjustments; verify instructions against bytes.
Starting targets, to be confirmed rather than accepted by title:

1. AnnoWelt `0x100a3790–0x100a37bb`: query/set behavior of world `+0x69cc` bit0;
   distinguish argument `-1` queries from writes. Constructor initialization near
   `0x100a3acd`, reset `0x100af24b–0x100af257`, MAIN restoration
   `0x100ae30b–0x100ae32a`. Separate bits0,1,2 and initialization paths.
2. World property4 through `0x100a3380`, island constructor/initializer
   `0x1008c090`, stamp store `0x1008c2dc–0x1008c2e6`, corrected loader conversion
   `0x100946e9–0x10094705`. Pin width/truncation and when the value is sampled.
   Do not infer a clock unit from the divisor alone.
3. Fresh/reused island branch `0x100a2f53–0x100a2f83` and sequential child-header
   dispatch `0x100a2f8c–0x100a2fbc`. Account for absent/empty/repeated HIRSCH.
4. AnnoScene fresh-world/load entry `0x10002e40`, player/load path `0x100025a0`
   and dispatcher `0x10009cc0`. Follow template placement `0x10008490` only far
   enough to classify its relationship to the chosen profile and load cursor.

Keep file/jitter order separate from later reversed same-chunk traversal. The
original header comparator is not a global first-HIRSCH lookup. Do not trace all
animal movement, combat, callbacks, float routes or allocation-failure behavior.
Those do not become implicit successes through this bounded initialization read.

## I2 — executable ordered-load examples

Write a small independent Python reference and hand-derived synthetic vectors.
Explicit inputs supply raw island stamp, mode, ordered headers/records and raw
u32 draws. Output each effective goods value, timestamp, draws consumed and
per-chunk traversal order. Model successfully allocated records unless allocation
outcomes are explicit inputs; decoded record count alone does not prove total
original jitter draws. This model receives an order; caller evidence must
separately prove that an original profile supplies it. No hidden fixed seed or
unstated global draw phase. All synthetic values are authored, not extracted.

Include distinguishing examples for:

- Two islands whose stream order differs from numeric slot order.
- Two records in one chunk versus two chunks: jitter follows file order while
  same-chunk insertion reverses traversal, without reversing assigned draws.
- Absent, present-empty and repeated headers; no fabricated empty spawn.
- Mixed bit0 settings, unknown bits and a mode change that alters goods only.
- Raw stamps499/500 and65535, divided bases0/1/131 and jitter limit31.
- A separately evidenced intervening load draw, explicitly supplied to the model;
  if no such caller is established, label the example synthetic scheduling only.
- Reconstruction of persisted effective state without rerolling; describe this
  as the rebuild persistence contract until original save behavior is evidenced.

Reject zero executed vectors, malformed/unknown input and draw exhaustion with
clear bounded failures. Add a public-command contract. Show legal controls pass
and an intentional wrong order fails the exact timestamp/draw assignment, while
the corrected model passes. Also catch a wrong /1000 conversion with raw500.
No generic mutation campaign or reference that merely echoes expected fields.

## I3 — implementation-ready contract and remaining decisions

Write `docs/sim/wildlife-initialization.md` with the supported input profile,
original evidence, explicit adaptations and remaining unknowns separated. Identify
where initialization belongs relative to current link RNG cursor transfer,
existing actor/player/script work, and World::create's parallel island building.
No shared RNG draws inside parallel `build_island`. Explain how feature-off import
retains current bytes/cursor, and enabled native reload resumes rather than rerolls.

Describe proposed header/islands payload fields and their canonical order,
validations, presence/default rules and per-island source/traversal identity.
**Do not allocate numeric tags or write payloads.** The Mac orchestrator coordinates
reservations with the board owner before the later implementation packet.

Give an exact next-code checklist with files and positive/negative proofs. The
intended next package should implement one supported fresh-load path and native
round-trip without animal materialization. Do not silently generalize that proof
to templates, reused islands, original savegames or all scenarios.

If omitted original draws prevent the proposed faithful profile, stop the dependent
implementation and report the exact finite prerequisite, with its smallest witness,
effects and recommended next scope. Continuing the already authorized faithful port
through a bounded original-load correction is an orchestrator dispatch decision,
not a new owner-policy approval. Only proposing a divergent deterministic rebuild
initialization phase requires a concrete owner choice with changed ordering disclosed.
Do not implement an adaptation, begin the prerequisite in this packet, launch an
unbounded repair campaign or trace unrelated lifecycle code.

## I4 — focused verification, review and stopping criteria

Only docs, authored reference/vector/contract and their CTest registration are
in scope. No production src/, state/default/registry/draw-site change, original
assets or gameplay-pin regeneration. Register the new reference/contract; run
the canonical focused tests and required preflight. C++ sanitizer campaigns are
unaffected and need not be repeated for Python/docs-only work. Check reference
CLI controls, exact vector counts, relative links and no public host paths.

Freeze and ordinary-push your branch. Send READY FOR REVIEW directly to the Mac
orchestrator with head/tree, exact CI status, addressed evidence, counts, source/
log hashes, Decisions and reasoning, any owner question, and next implementation
boundary. Original bytes/listings/decoded records remain ignored; committed
summaries stay compact. Preserve failures and distinguish NOT_RUN from pass.
The orchestrator provides independent read-only review; no executor reviewer agent.

Stop at the reviewed contract/reference. No World integration, species creation,
fauna sweep, hunting, tag reservation or default flip. Success is one evidenced
profile plus executable distinguishing examples and a precise implementation
handoff, or an evidence-backed finite prerequisite when the proposed profile is not yet
supported; an owner decision is needed only for a proposed policy deviation.
At six hours, return a clean checkpoint with exact unread dependency rather than
expanding the investigation. Do not pause independent steps merely waiting for CI.

## Initialization witness, independently confirmed during execution

Tutorial/01Discover01's first nonempty GELAENDE header34 places type70/kind226
on a previously empty, flat tile. Original placement admission and commit precede
the group225–256 branch's world draw atAnnoWelt0x10089468, followed by catalogue
slot+0x88. HIRSCH header71 follows. The current importer has no corresponding
resource-load draw. Executor and independent reviewer checked the matching original
bytes and scenario/registry facts. This proves one missing predecessor, not4589
actual draws or the complete timeline. Keep the full-profile claim unestablished.
A bounded faithful GELAENDE-load prerequisite may follow this contract's review;
no renewed owner permission is needed for that existing faithful-port objective.
