# WILDLIFE-ORDINARY-PROJECT-1 — conditional owned creation geometry

RELEASED after corrected contract and independent scope review, 2026-11-19.
One bounded implementation packet, target 2–4 active hours; checkpoint at four.
The goal is to replace authored ordinary X/Y/Z words with source-bound calculated
words under an explicitly supplied numeric profile. This does not identify the
original runtime profile or authorize native/world integration.

## P0 — intake, supported profile and proof table

Start `task/WILDLIFE-ORDINARY-PROJECT-1` from corrected ORDINARY-GEOMETRY head
`7c91bd125cda8e1b6b43cac22688638062ea7b29`. Read CLAUDE, PLAN §§0.1–0.3, evidence-contract and testing README,
the exact review and this packet from orch/mac-lane without merging that branch.
Read docs/re/wildlife-ordinary-geometry.md and its reference/vectors, then
wildlife-import-plan.md, wildlife-create-effects.md and wildlife-raster-fp.md under
their existing docs directories. Read src/sim_load/include/anno/sim_load/
wildlife_create.hpp, wildlife_import_plan.hpp and wildlife_prefix.hpp and their
implementations before choosing the smallest interface.

Write one acceptance table before code: requirement, primary evidence, independent
expected word, plausible wrong behavior and intended distinguishing assertion.
Carry all review corrections forward. Separate supplied callback precision/rounding
from supplied table-initialization precision/rounding; neither has a default labelled
original. Name source geometry, current-corner provenance and supported numeric
endpoints. Do not silently narrow acceptance to the flat Tutorial patch.

## P1 — deterministic arithmetic needed by this path

Implement only the integer-based arithmetic required by the reviewed position,
height table and triangle schedules. No native float/double/long double, process
FPU modification or host-dependent intrinsics. Reuse an existing suitable facility
if one exists. Avoid a general x87 emulator, a broad numeric-library refactor or a
new dependency as a side project. Use checked operations and portable widths that
compile on MSVC as well as Clang/GCC; bare __int128 is not a portable public contract.

Preserve each proved operation rounding and binary32 spill, including cancellation,
subtraction sign, division and the two triangle schedules. Support the explicitly
reviewed normal finite profiles, with no guessed zero sign, underflow, exception or
nonfinite behavior. A retained unsupported endpoint must produce a typed diagnostic;
it must not silently return zero or a rounded host result. If the arithmetic cannot
be completed within this packet, freeze the smallest meaningful tested kernel and
report the exact remainder; do not invent a simplified interpolation to finish.

Do not accept a translated Python/C++ agreement as sole proof: retain hand-derived
words and check the specific wrong-shortcut, wrong-source, wrong-scale, wrong-spill
and branch-boundary alternatives where they differ on the admitted domain.

## P2 — source-bound owned ordinary adapter

Bind island origin/chunk/packed position and the four current terrain corners to
the same prepared/source identity as the imported plan. Ordinary is determined by
the actual tile kind, not the selected animal type. Explicitly reject the cave path.
Admit proved interior cells first. Any fresh border support must use the already
proved source lifecycle; no arbitrary post-load/custom corner inference.

Accept explicit numeric context through a typed interface. Compute needed table
entries once per context/batch, not an island-wide scan or per-animal catalogue
reparse. Distinguish caller-authored context from observed original words. If accepting
explicit stored table words, preserve their identity/provenance and validate their
admitted domain; do not call arbitrary arrays original source data.

Return owned X/Y/Z words plus home/source/numeric qualification. No borrowed lifetime
hazards, manager allocation or live World write. Consume only the already supplied
position outcome; do not draw again or conflate prefix, maintenance and selector
counts. Compose with the existing creation-effects projector in a focused test using
explicit manager-success context. A successful plan is not native animal creation.

## P3 — focused tests that distinguish the implementation

Cover four offsets, tile1 high-expression discriminator, nonflat triangles, exact
and one-ULP diagonal, largest admitted source-width coordinate, independent table
and callback profiles and refused missing/invalid context. Carry the corrected
callback RC, callback PC and first-triangle left-height spill discriminators into
C++ as independent expected-word assertions, not merely a reference comparison. Test zero/domain limits
according to the actual admitted contract, source mismatch, unavailable corners,
unsupported cave and input destruction after constructing an owned result.

Run the independent Python reference through its existing CLI and compare C++ on
its admitted vectors. Hand-derived assertions must run separately and survive a
wrong reference implementation. Add only missing decisive vectors; preserve old
failures and do not regenerate expected results from C++.

Use a legal control before a small set of actual-source mutants targeting the
new causal seams. Each must fail its intended semantic assertion, not compilation,
case discovery or malformed input. Include height-source substitution and a relevant
rounding/spill or triangle boundary. If a proposed mutant is equivalent on the admitted
inputs, say so and choose a meaningful counterexample; do not manufacture proof.

## P4 — one original-source composition and bounded cost

Reuse the unchanged Tutorial source witness (origin24,24, chunk10,2, packed47,
world111,45; independently checked raw terrain) with explicitly authored draws and
numeric profiles. Derive and assert words independently before invoking the adapter,
then feed the result through the existing creation-effects projector. Include the
same path on an authored nonflat patch so a flat-only implementation cannot pass.
Preserve all inputs and intermediate draw counts. No World, true manager, hunting,
opening replay or full original PRNG claim.

Demonstrate that selecting one cell/batch touches only its required source data after
context preparation; avoid a new scale campaign or claiming gameplay frame performance.
No fixture/golden/state/tag/default change should be needed. If one appears necessary,
stop that dependency and explain it before making the change.

## P5 — freeze, focused gates and direct review handoff

Run formatting/lint, affected strict builds and canonical Debug plus ASan/UBSan
selections covering the new adapter, arithmetic, composition and touched existing
creation paths. Require source cases with game files explicitly. Keep reference
contracts required. Run no heavy/full/opening campaign merely because the branch is
large. Record tool/source/binary hashes and actual unique executed cases, every
skip/failure and which source each result covers. Never edit tracked files during
a gate; rebuild clean controls after mutants.

Ordinary-push only the task branch and send the Mac orchestrator full head/tree,
tested source, CI, evidence, exact admitted numeric domain and next remaining blocker.
No executor review agents: the orchestrator conducts independent code/evidence review.
READY FOR REVIEW needs nonflat geometry and the source-bound creation composition,
not a wrapper that always refuses. At four active hours checkpoint without hiding
remaining work. No World/Rules/default/identity/PRNG change, Windows assignment,
original assets/listings publication, coordinator repair or interference with other
jobs and the owner's awake assertion.
