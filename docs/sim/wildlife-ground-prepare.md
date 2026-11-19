# Supplied-fact wildlife ground leaf

WILDLIFE-GROUND-PREPARE-1, 2026-11-19. Contract before C++ implementation.
Source: [addressed preprocessing](wildlife-preprocess.md), GOG AnnoWelt
0x10084370–0x10084818. This is an UNWIRED numerical operation on supplied facts,
not an original-scene loader, city-ring proof or permission to call live fill.
The supported mathematical profile has no city prewalk. Callers supply that
contract; a Boolean or hash cannot certify an original island had no cities.

## Inputs and computation

One synchronous immutable view: full `sim::TileRect` geometry, u16 GFX column,
u32 whole object-word column (the engine TileLayer representation), byte effective
ground, supplied byte growth lookup, selected 25-byte kernel and object facts.
No rectangle update argument: partial/disjoint updates are unavailable, rather
than silently zero-padded. Origin does not change local row-major indexing.
Validate positive dimensions, representable last tile, product/length arithmetic,
exact column lengths, kernel shape and referenced GFX >> 2 coverage.

Facts distinguish N = explicit null descriptor, A = absent property, F = failed
query, V = successful byte. N requires kind/value 0; A/F require value 0; V may
hold zero. Every referenced whole object word, including 0, needs an explicit
fact. Missing or duplicate facts refuse. No id masking. Unknown enum states and
inconsistent facts refuse. Supplied descriptor kinds are widened u32.
Use deterministic once-bound lookup, not a full registry scan per tile.

Only V with raw kind 225–256 and nonzero value blends, using the two addressed
integer branches; others retain p. Scratch b=(p+5)/10, unsigned, no early clamp.
After the ENTIRE scratch pass, skip zero centres, sum each product/12 separately
over island-clipped 5×5 with fixed centre alignment, clamp at 10, merge low nibble.
Products and sums use u32, ample for 25*(26*255/12); geometry uses widened checks.
No FP, RNG, World, metadata write, original callback or provenance seal.

Result owns its byte plane; inputs must live only through the synchronous call.
All validation precedes numerical work and output publication; refusal leaves
inputs unchanged. Follow ordinary project vector allocation policy, no exception
wrapper or allocator framework. The selected Tutorial and the existing
FRUCHTBAR-required fill validator remain unchanged and uncalled.

## Acceptance table, before implementation

| Requirement | Positive / specific counterexample | Planned evidence | Review |
|---|---|---|---|
| Unsigned/blend/whole words | q255, both blend branches, ids 90001/70000, word 16384 distinct from 0 | authored vectors/reference, C++ public helper | pending |
| Explicit evidence states | N/A/F/V-zero retain p; missing fact refuses, including zero | vectors plus one-precondition refusal controls | pending |
| Exact smoothing | s2 floors per term, s3 skips centre zero, s10 distinguishes product 260 from wrapped 4 | reference-first expectations and actual C++ wrong-source controls | pending |
| Immutable scratch/merge | s7 [a4,b5]; whole-byte write loses high nibble | actual output comparison and ownership tests | pending |
| True 2D alignment | b rows 1,2,3 / 4,1,2 / 2,3,1; centre+2east+south -> 9,9,5 / 8,8,3 / 8,5,1 at origin (17,-11) | hand-derived corners/edges/interior; transpose/stride counterexample | pending |
| Validation and lifetime | legal control then wrong size/kernel/lookup/fact; large arithmetic without large allocation; output survives input destruction | C++ refusal and ownership tests | pending |
| Public reference is computation | changed expected value exits 1, malformed/zero exits 2; echo-only implementation fails contract | CLI contract and private echo control | pending |
| UNWIRED compatibility | only helper/test registrations; bounded existing fill/terrain ownership controls | canonical Debug/ASan, caller/scope audit | pending |

The 24 hand-derived vectors reuse all applicable positive examples from the
research contract. S8 partial and S9 empty-intersection proposals cannot be
invoked through this full-island-only API; refusals and no partial API cover that
boundary. The initial S5 width control remains honestly nondistinguishing after
saturation; S10 is the actual product-width discriminator. No historical failure
is rerun. Future growth-parser tests need NONZERO retained staging: the real
short row retains three zeros and alone cannot distinguish retention/zero-fill.

NOT_RUN at reference checkpoint: C++, original/HE, source binding and live
composition. No original kernel table, script, asset or binary in fixtures.

## Implemented leaf and finite evidence

`prepare_wildlife_ground(const WildlifeGroundInput&)` now returns an owned
`formats::Result<vector<uint8_t>, string>` from the focused sim_load header/source.
Fact validation uses the four named enum states, then sorts one private fact copy
and binds whole words by lower_bound. No input span survives the call. Geometry
checks the last TileXY, u64 product, vector representability, then exact lengths.
The no-rectangle API deliberately cannot express partial updates.

The public C++ vector test executes all 24 authored rows, translated origins and
reversed fact order. Three further cases cover single-precondition refusals,
large arithmetic without huge allocations and output lifetime/immutability.
Private actual-source variants all fail only intended output assertions: aggregate
1, guard 3, byte-product 2, whole-byte 22, in-place scratch 1, signed byte 3,
transposed indexing 1; the legal private build passes all 4,596 assertions.
Canonical source/binary hashes are unchanged by these builds. The echo-only
reference variant is rejected by the public command contract's wrong-expectation
control. These prove conditional computation, not original scene binding.

Final gate receipts and independent disposition accompany the REPORTS handoff.
Original/HE, growth parsing, original city provenance, source adapter and fill
composition remain NOT_RUN/out of scope. The pre-existing validator is unchanged.
