# Conditional owned ordinary geometry — P0 contract before code

WILDLIFE-ORDINARY-PROJECT-1, 2026-11-19. Independent review pending.
Primary [arithmetic/provider contract](../re/wildlife-ordinary-geometry.md),
[creation effects](wildlife-create-effects.md), [import](wildlife-import-plan.md).
No original runtime CW, World, manager execution, state/default/identity or PRNG claim.

| Requirement | Primary / independent expected | Wrong sibling / distinguishing assertion |
|---|---|---|
| Ordered four offsets | Welt 0x10092da4–0x10092e6f; existing 29 hand vectors | high at tile 1: 0x3feaaaab, shortcut 0x3feaaaaa |
| Callback RC | b5390 first triangle; [0,1,2,4], low/low | up 0x3f000001 vs nearest 0x3f000000 |
| Callback PC | same, [0,3,0,0] | PC24 0x3f000001 vs PC53 0x3f000000 |
| Required spill | FSTP 0x100b5649, [1,0,2,0] | 0x3f7fffff vs omitted 0x3f800000 |
| Triangle admission | query-edge stored words; exact/ULP diagonal | inclusive first triangle vs second, output decision checked |
| Height source | island+0x14/W+1, not object height or mean | nonflat prepared patch: wrong corner/constant source must fail |
| Table profile | retained additions at 0x1007c760, own PC/RC | table6 PC24 0x3e75c28e vs PC53 0x3e75c28f |
| Explicit context | two independent profiles + nonempty provenance | absent/invalid profile refuses, no labelled-original default |
| Source/lifecycle | same completed source/geometry and immutable fresh HEIGHT | mismatched identity/missing columns/cave/border refuses |
| Owned composition | original cell419, figure528, corners48, X111+high/Z45+high | words 0x42dfaaab/0x3ff5c28f/0x42375555; explicit manager effects |
| Bounded access | table256 once/context, terrain validation once/batch | project reads exactly 4 required corners, no catalogue/source rescan |
| Original widths | origin65535/chunk255/component7 ->67582 | no narrowing/wrap; source-width adapter bound, not engine map limit |

## Admitted interface and arithmetic

Use a private checked multiword integer (fixed u32 limbs sufficient for this
bounded significand alignment/product/division), not host FP, __int128, a dependency
or a general x87 instruction emulator. Normal finite operations round explicitly
at supplied PC24/53/64 and RC nearest-even/down/up/toward-zero, with each proved
binary32 spill. Refuse alignment/range/underflow/unproved signed-zero endpoints
with typed diagnostics; never turn arithmetic failure into successful zero.
The public leaf height admits explicit four table words for independent tests;
that does not bind arbitrary arrays as original source. Source adapter obtains
corners only from the validated fresh completed prepared terrain, no border or
runtime/custom corner support in this first implementation.

Prepare numeric table entries once from the independently supplied initializer
profile; callback profile is separate. Both need explicit provenance. Own table
words; no global mutable state/cache. Source batch owns immutable terrain and
small per-chunk selection facts with source/terrain identities. Validate terrain
digest/geometry once at batch creation, not per animal. This cannot authenticate
arbitrary caller tampering of transient public prepared facts; accepted source
lifecycle remains part of the adapter admission contract, not a native validator.
Project only selected ordinary entries using the actual tile kind, existing
selection outcome and four required corners. Output owns X/Y/Z, home, source and
numeric qualification and unchanged named draw counts. No new draw or native
creation success. Existing creation projector receives explicit manager/id/time.

At P0 all new implementation/tests are NOT_RUN. Expected words above are from
reviewed hand derivations, not C++ output. Canonical reference and independent
word assertions remain distinct. Gates will be frozen focused Debug/ASan plus
the two reference checks; no broad/heavy/opening/pins campaign.

## Implemented admission and cheap proof checkpoint

`wildlife_geometry.hpp` exposes checked position/height leaves, a sealed numeric
context (table generated once from supplied initializer profile; no explicit observed
table-word input), and a sealed ordinary source batch. Context requires two profiles
and provenance; supported PC24/53/64 and four RC modes have no valid default.
Source batch compares scene/ordinal/chunk/actual tile/type identities, validates
Terrain columns/digest once, and owns immutable terrain plus selected requests.
This is accepted-source admission, not authentication of arbitrary mutable C++ facts.
Only ordinary selected entries are admitted; cave, missing or inconsistent facts
refuse. Projection reads four interior raw corners through a counted accessor,
uses the already selected position outcome, and owns words/home/corner/provenance/
profiles plus unchanged named draw counts. No original CW observation is inferred.

Private 8-u32 workspace handles checked significand alignment, exact <=128-bit
products and long division for up to 64-bit precision. Its add/subtract/multiply/
divide/store schedule is the addressed conditional model, not an x87 instruction
emulator. Normal arithmetic exponent range [-126,100] matches the reviewed reference;
alignment needing more than the checked workspace, underflow/nonfinite and the
reviewed downward-zero endpoint refuse with WildlifeGeometryError. Native FP and
compiler-specific integer extensions are absent. All source-domain coordinates and
generated height-table values lie far inside the workspace bounds. No engine limit
or production runtime rounding profile was introduced.

Three numeric cases compare all 29 independent JSON vectors and also assert literal
words separately. The direct-word case never reads reference code or fixture values:
corrupting the Python reference cannot supply its expected results. Include PC64
nearest/up/down/toward-zero literal controls, zero/domain/context errors and table
PC24/53 discrimination. Four authored cases execute 1280 assertions at this checkpoint.
Nonflat source patch (local9,9), corners [0,25,50,100], gives world33/49 low/low,
X=0x4204aaab, Z=0x4244aaab and Y=0x3f000040: local fraction 43691/2^18,
so height 3*t=131073/2^18. Source destruction/ownership, exact four reads and
explicit manager-success composition type60001 are checked.

Original unchanged Tutorial tile111/45 corners48 gives owned
[0x42dfaaab,0x3ff5c28f,0x42375555], then type528 creation projection under
supplied manager/id/time. Direct source canary executes 35 assertions. Draw domains
remain prefix4745/maintenance0/selector2; no new draw or native creation claim.

Preserved first failures: initial strict compilation had six bit_width signedness
warnings-as-errors, corrected with an explicit unsigned helper (no suppression).
First nonflat test reused flat prefix100; four resource quads at (8,8),(9,8),(10,8),
(8,9) now have means24/22/30/28 and reject below32. Thus95 resource draws+1 jitter=96.
Only that expectation corrected; resource behavior unchanged, failed test source/binary/log retained.
Four actual private C++ source mutants reject wrong corner, omitted spill, forced
callback profile and high shortcut after a legal control. Canonical source/binary
hashes unchanged; clean control rebuilt and rerun. Gate results follow in handoff.

### Final table-rounding discriminator and gate reuse

After the initial frozen focused gates, add test-only independent table RC controls:
248*c=166429979/2^24, binary32 grid step 16 numerator units. PC53/64 retain that
accumulator exactly; nearest/up store 166429984 ->0x411eb852, down/toward-zero
store 166429968 ->0x411eb851, with callback fixed separately to nearest.
An actual table-local forced-nearest C++ mutant fails four intended assertions;
legal context control passes 41 assertions. Only the changed context case is rerun
in Debug/ASan; the other 16 checks/profile reuse their original frozen evidence.
Production implementation is unchanged across the two tested epochs. No expected
fixture, reference, golden or pin value changed. The independent 29 JSON vectors
remain the accepted geometry reference input.
