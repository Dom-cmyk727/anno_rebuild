# WIN-NAV-EDITIONS-1 — verify coarse navigation against GOG and History Edition

Owner-requested handoff from the Mac orchestrator, 2026-11-19.
**Research and differential verification only. No production repair, semantic
identity allocation, default change or integration is authorized by this packet.**
The owner asks Windows to check both original editions before deciding the fix.
The Mac continues its independent, unwired wildlife source-binding work.

## Objective and intake

Establish what each original edition actually computes for coarse navigation
obstacle heights; compare that with the rebuild; explain any effect on routes,
saves and replay; return a reviewed recommendation. Do not assume HE agrees with
GOG, or that the proposed correction is already approved.

Use a suitable owned research checkout at an ordinary checkpoint. Preserve all
ongoing Windows work and evidence. Fetch `origin/orch/mac-lane` and
`origin/task/WILDLIFE-NAV-INPUTS-1`; inspect the exact refs without merging the Mac
stack into the live Windows branch. Record your checkout head/tree and the current
production implementation before comparison. If the current Windows code already
changed, compare both the reviewed baseline and current code and explain the delta.

Read, in order:

1. `CLAUDE.md`, `docs/PLAN.md` §§0.1–0.4, current Windows task/owner rules,
   `docs/testing/evidence-contract.md`, `docs/testing/README.md`.
2. On `orch/mac-lane`, `docs/orchestration/mac-lane.md` and
   `docs/orchestration/reviews/2026-11-19-wildlife-nav-inputs-1-review.md`.
3. At research head `74d50bfeedf1a48ab7e140cc66e36859e0d0a9c3`,
   `docs/sim/wildlife-navigation-inputs.md`,
   `docs/orchestration/evidence/wildlife-nav-inputs-1.md`,
   `docs/re/ship-navigation.md` §9.2 and `docs/sim/wildlife-postload.md` §4.
4. The current `src/sim/src/route_world.cpp`, its header and tests,
   `route_coast.cpp`, `route_cache.cpp`/header, `route_plan.cpp`, `ship_mover.cpp`,
   `resource_import.cpp`, and the registry/terrain input code they actually use.
   Locate moved files with `rg --files`; do not invent replacement paths.
5. `docs/adr/0006-save-sections.md`, current state/replay contracts and the actual
   `NavMotion` field serialization/identity/refusal checks. Historical stream
   numbers are not authority to allocate a new one.

The accepted Mac research head above had all 12 jobs green in CI 44458725953.
Its unchanged production base was `889143ba1485a2fed72c03bd648c9c39878c4f06`.
This accepts the diagnosis/reproduction, not a repair or a shipped-map divergence.
Mac private probe logs are not in Git; do not claim you inspected them from the
committed summaries. Recreate the small controls locally if needed.

## N0 — inventory both editions and freeze the question

Record edition, release/patch version, architecture, executable/module SHA-256,
image base and relevant data-file hashes separately for GOG and HE. Locate the
actual installed HE code; its modules, offsets and implementation may differ.
Never transplant a GOG VA/RVA or function name into the HE column without tracing
it. Distinguish file RVA, preferred VA and runtime address after relocation.
Use read-only originals; decoded bytes, listings and original assets stay private.

GOG hashes from the Mac evidence, to verify against your files:

- AnnoWelt.dll: `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`
- AnnoGame.dll: `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`
- Anlagen.dat: `b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75`

A different hash means a separately identified build, not automatic failure and
not proof that the old addresses apply. If one edition is unavailable, finish the
other and report the precise missing prerequisite; never label both verified.

Write a short claim/proof/control table before executing probes. Label static
instruction proof, original-runtime observation, isolated original-code emulation,
authored reference and rebuild execution separately throughout.

## N1 — independently re-derive GOG and then locate the HE equivalent

Trace the producer, its data inputs and immediate consumers in each edition.
For GOG these are leads to verify, not an oracle to copy:

- Welt `0x10084150–0x10084360`: clipped update rectangle, local 8-tile block starts,
  9×9 unsigned vertex samples and 8×8 tile-anchor samples; min starts 255, max 0.
- Anchor eligibility: tile record byte 3 bit 0x80. The height accessor is virtual
  slot +0x58. Game tables `0x1002185c` and `0x10021a88` lead to `0x10007d50`,
  descriptor +8; `0x10009372–0x10009384` loads it from HEAD +0x14.
- Welt `0x100842a3`, constant `0x100c9c20` (f32 bits `0x3d23d70a`), conversion
  helper `0x100b5f90`: divide the metadata height by f32 0.04, then truncate.
- `0x100842b6` masks the CONVERTED COMPONENT to 8 bits BEFORE `0x100842bb` adds
  the unsigned tile base. Candidate range is therefore 0..510, not 0..255.
  Anchored-object candidates update only max; min comes from the vertex samples.
- `0x1008431f–0x10084331` halves the full extrema before storing bytes. Check
  there is neither a sum mask nor a max clamp at 255 before the halving.
- Fine navigation's byte addition at `0x100904e7` is a different operation.
  Do not generalize the coarse correction to that consumer.
- Coarse class at `0x100ab0b8–0x100ab0e7`; strict shore threshold at
  `0x100ab94b–0x100ab966`. Check the comparisons and their order, not just names.

For HE, locate the corresponding code through actual callers, data readers and
outputs. Re-derive widths, signedness, masks, rounding, loop extents, ordering and
store width. If HE uses a different algorithm or representation, document that
rather than forcing a line-for-line mapping. Inspect actual asset metadata and
placement/import behavior in each edition so the input domains are evidenced.

Deliver a side-by-side table: claim, GOG module/hash/address, HE
module/hash/address, result, confidence and remaining question. Review exact
function boundaries to avoid attributing a following function to the caller.
No proprietary source access is assumed: code evidence means the locally inspected
original executable instructions unless legitimately available source exists.

## N2 — reproduce the smallest discriminating cases before any broad run

Recreate authored inputs with expected values derived independently BEFORE
running the rebuild. Use the real registry build and resource import where
proving legal inputs. Assert the setup accepted, zero unexpected draws, and the
actual anchor/base before querying. Do not fabricate the decisive placement.

The legal Mac counterexample uses a 1×1 KLIPPE metadata profile, flags 0x12,
flat corners 18. Real mode-2 import gives base 16. A converted component 254
then yields 270. The earlier suggested flat-18/base-250 setup was NOT proved
legal; do not reuse it as evidence.

| Case | Original hypothesis: min/max halves | Wrong behavior it separates |
|---|---|---|
| component 2 + base 16, vertices 18 | 9 / 9 | ordinary legal control |
| component 239 + base 16 = 255 | 9 / 127 | pre-overflow control |
| component 240 + base 16 = 256 | 9 / 128 | wrapping the sum |
| component 254 + base 16 = 270 | 9 / 135 | wrapping or early saturation |
| component 255 + base 255 = 510, vertices 255 | 127 / 255 | early saturation even when class agrees |
| converted component 256, mask first, base 16, vertices 18 | 9 / 9 | incorrectly deleting the required component mask |

Use the exact f32 input bit patterns in the research table, and independently
check their conversion; avoid decimal-rounding substitutions. Its six finite
values were chosen away from ambiguous conversion boundaries. Do not claim a
universal x87/SSE profile from these six cases.

Also retain the seven ordinary/equality/far-corner controls (flat 19/20/31/32;
far-corner 32 versus 34) and separate geometry checks. The existing rig uses a
24×24 island at (8,8), world 40×40, avoiding a border-induced shore result for
the interior target. Keep full-border and unsupported misaligned/partial geometry
separate; no invented padding or edge clamp.

Compare exact min/max bytes AND classification AND own-cell shore. A classification
assertion alone misses the sum-only fix: it returns 9/127 rather than 9/135.
Expected diagnostic pattern on the reviewed rebuild: three exact-half mismatches;
removing only the sum mask still leaves three; the private proposed arithmetic
has zero; incorrectly removing the component mask breaks its legal wrap control.
If current code behaves differently, report the actual result and why.

Private temporary source variants are permitted only as isolated causal probes;
preserve source/binary identities and first failures, never use their output to
supply the expected answer. No repair is applied to the production branch.

## N3 — verify original execution where available; bound real-data claims

For BOTH editions, use an available debugger or bounded emulator to observe the
actual producer on the discriminating inputs. Confirm the complete relevant
instruction slice, caller inputs, stubs if any, FP state and returned/stored bytes.
Read-only runtime inspection on a private session is fine; do not patch installed
original files. A debugger/emulator-injected state is an isolated code experiment,
not proof that a shipped scenario naturally reaches it.

If a full original-game observation is practical, capture one ordinary legal case
per edition and one separating case only if it can be reached legitimately.
Otherwise label runtime NOT_RUN and retain the static/emulated result precisely.
Do not install a large new toolchain or spend the night chasing a cosmetic setup.

Perform one bounded metadata/scene check per edition to determine whether the
range that separates the algorithms occurs in shipped content. State the scanned
scope and actual placement predicates. A valid authored metadata profile proves
the supported import domain; it does not establish a shipped overflow. No-hit
results do not establish impossibility.

If cheap, carry a separating valid block through the production route query and
compare route result/cost/nodes before and in the isolated candidate arithmetic.
A successful exact-block reproduction is sufficient for the arithmetic diagnosis;
if no full route witness is obtained, report potential route impact rather than
inventing a demonstrated replay divergence.

## N4 — compatibility assessment and proposed next implementation packet

Trace coarse output → CoastIndex/RouteCache → ShipMover/plan_route → saved/hashed
NavMotion and later command outcomes in the current consolidated code. Derived
cache bytes being absent from saves does NOT establish semantic compatibility.
Stored routes can initially persist while subsequent replanning changes results.
No direct new PRNG draw in this calculation does not guarantee unchanged later
simulation timing or draw history.

Recommend one bounded remedy based on BOTH edition results. If the editions
agree and the rebuild differs, propose the faithful correction with exact scope
and discriminating regression tests. If editions differ, put the edition/rules
policy choice to the owner with consequences. Do not silently choose GOG or HE.
Identify the current affected semantic section/handshake/refusal boundary under
ADR 0006, and any save/replay pin work that would follow. Do not allocate a tag,
change a default, rebaseline a trajectory or add an optional rule in this packet.
Separate byte-layout compatibility from behavioral compatibility explicitly.

The secondary world raster's FP capture is a separate pending task, documented in
`docs/testing/wildlife-raster-fp-capture.md` at `aec8bcc8`. It must not be confused
with this coarse integer-extrema question or made a prerequisite for its verdict.

## N5 — independent review, finite closeout and reportback

Use the Windows orchestrator's independent read-only review at handover. The
reviewer must inspect BOTH actual original builds, derive at least the 270 and
component-256 answers independently, and verify a legal control plus each wrong
interpretation's intended assertion. If the reviewer lacks an edition's bytes,
that edition's primary evidence remains review-pending. Do not add another broad
review campaign or rerun unaffected suites.

For docs/private probes only, state C++ acceptance gates NOT_RUN/not required by
the diff. Any committed helper or test change needs its existing focused canonical
contracts, relevant target gates and exact-head CI; an empty or skipped comparison
is not a pass. Keep original data/listings/private extracted tables and absolute
host paths out of public commits. Publish only methods, authored fixtures where
appropriate, findings, hashes, limitations and the compact review record.

Target bounded substeps, roughly 2–4 hours for an initial decisive packet, with a
six-hour checkpoint if unfinished. At that checkpoint identify the single next
read/observation and estimate it. Preserve useful completed results rather than
expanding into full pathfinding, whole wildlife integration or broad engine repair.

Return to the owner and Mac orchestrator:

- Branch, full head/tree, tested/probe source/binary identities, exact CI or NOT_RUN,
  review-file path and reviewer verdict.
- Separate GOG/HE source hashes, address table and static/runtime/emulation status.
- Exact expected/observed extrema, class and shore; controls and first failures.
- Authored-domain versus shipped-data versus real-route claims kept distinct.
- Whether the defect is confirmed, contradicted or still unresolved for each
  edition; the proposed repair scope and explicit owner compatibility decision.

Stop at a reviewed investigation and recommendation. Preserve ongoing Windows
work and the Mac's independent task. Do not merge or push main for this handoff;
ordinary task-branch pushes follow the current owner rules. Update the Windows-owned
board/status to record intake without silently accepting or landing the Mac stack.

Dispatch scope independently reviewed by `/root/review_docs_sim_dispatch`: APPROVED
on 2026-11-19. This is packet review, not verification of the HE result.
