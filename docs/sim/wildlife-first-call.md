# Owned wildlife first-call projection

WILDLIFE-FIRST-CALL-1, 2026-11-19. Contract before code. Source contract:
[provenance](wildlife-prep-provenance.md), [prefix](wildlife-prefix.md),
[completed-ground fill](wildlife-postload.md). Original wrapper Welt
0x100a1810–0x100a183c skips prep/prewalk after completed FRUCHTBAR and then fills.
This operation stops at that fill, before navigation/raster/graphics/World.

Proposed `prepare_wildlife_first_call(scene, selection, entries, annogame, types,
inputs, draws, roles)` synchronously constructs the prefix and returns an owned
`WildlifeFirstCallResult` (existing fill result, source SHA, ordinal and supplied
input provenance). Selection is a widened file ordinal and expected slot, never
caller-supplied prepared terrain or a completion Boolean. Only a single fresh
embedded INSEL5 is admitted. The restriction is an adapter profile, not an engine
island cap. Multiple-island prefix/fill ordering is not yet certified.

Require positive aligned source geometry: factory 0x100a2545–0x100a2584 moves
both edges by origin remainder modulo 8; this profile refuses a source needing
that adjustment. Use widened bounds and the exact full rectangle, no partial
area parameter. Construct prefix internally, require complete local headers,
and invoke accepted prepare/apply fill unchanged. Missing ground is a refusal,
not zero ground or an invocation of numerical preparation.

Failures identify source, prefix, fill-validation or apply stage. Prefix failure
may retain its existing diagnostic partial, explicitly not successful output or
a committed draw count. No external cursor is mutated; the supplied draws/stamps/
mode are conditional local inputs, not global original timing/PRNG evidence.
Source/catalogue buffers live through the call only; output owns tiles and retains
immutable Terrain. Modern transactional refusal is an explicit adaptation.

| Requirement | Independent positive / wrong control | Planned proof | Review |
|---|---|---|---|
| Whole completed source | aligned one-island source vs extra island/wrong slot/ordinal | exact stage/error; internal construction excludes prefix tampering | pending |
| Full rectangle | aligned control vs unaligned x/y, zero dimensions | intended geometry refusal before prefix | pending |
| Source-built field effects | hand 4x2 corners: means 0,32,64,32 / 0,16,32,16; occupied cell 2 | fish at 0,4; grass at 1,3,6; preserve occupied and neutral fields | pending |
| Completion semantics | all-zero completed vs absent/short; duplicate last payload | fish remains legal; no invented preparation; specific missing completion failure | pending |
| Draw accounting | GELAENDE then HIRSCH, supplied 4,7 plus suffix | variant 4, timestamp 8 at stamp 500; consumed 2, fill 0, suffix unchanged | pending |
| Refusal and ownership | valid roles then mismatch; unknown header after draw | exact stage and diagnostic; caller buffers unchanged; later legal call independent | pending |
| Real control | unchanged Tutorial and frozen action-map SHA/counts | all tile fields and chunk digests, 8486 fish/1559 grass/6339 unchanged | pending |
| Causal detection | legal executable then bypass fill/accept absent/skip alignment | actual implementation variants fail intended assertions | pending |

At contract checkpoint all new C++ tests/mutations/gates are NOT_RUN. Reuse existing
leaf/reference proof; manual equality to the same composed calls is insufficient.
No identity/default/tag/state/pin change, absent-ground preparation or runtime caller.

## Implemented boundary and cheap controls

`WildlifeFirstCallSelection` carries ordinal/expected slot. Errors carry
`WildlifeFirstCallStage` (Source, Prefix, FillValidation, FillApply), reason and
only an optional existing diagnostic prefix. `WildlifeFirstCallResult` owns the
existing fill result, exact source SHA, ordinal and copied/moved supplied context.
No successful result exposes a caller-supplied prefix as provenance.

The authored fixture's fresh TileLayer starts with walk code 2 (existing reset
contract); fish and neutral cells preserve it. Grass uses independently authored
HEAD code 3. Initial expectations incorrectly wrote code 0 in four cells; the
first failed run and per-channel diagnostic are preserved. Corrected expectations
trace to reset initialization, not to a changed fill rule. Resource variant 4
retains high nibble 0x40, hence effective cell 2 ground 0x46 after raw 6.

Cheap checks: 3 cases /117230 assertions pass, including the genuine Tutorial
with action-map SHA a66cff880b8ca148261ba7b61a9e288d9f4a4a90717546014386c4c2f0971487.
All 16384 output cells match independent expected fields and chunk digests;
8486 fish, 1559 grass, 6339 unchanged; prefix supplied draws 4745, fill draws 0.
Actual private implementation variants bypassing fill, accepting absent ground
and skipping alignment each fail intended assertions after a legal executable
control. Canonical source/binary remain unchanged. These direct controls do not
replace the canonical Debug/ASan gates recorded in the final evidence.

Source/registry/scene buffers are borrowed only during the synchronous call;
returned tiles and immutable Terrain survive caller-owner destruction. Existing
validator/prefix implementations remain unchanged. Unknown local headers retain
their diagnostic draw position but cannot publish first-call success. No external
RNG cursor exists. Multiple islands, unaligned factory input, missing/unsupported
ground, reused worlds and later callbacks remain explicitly outside scope.
