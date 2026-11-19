# Owned descriptor facts and pure creation effects

WILDLIFE-CREATE-EFFECTS-1, 2026-11-19. Contract and hand expectations before C++.
Source: [creation closure](wildlife-create.md), [decisions](wildlife-decisions.md).
No manager execution, World, native state, tag, default or PRNG integration.

## API and source boundary

Use a narrow owned descriptor capability and pure projector in sim_load, alongside
its existing pure ground-preparation/fill adapters. This keeps original-profile
binding and explicit original-width projections out of persistent engine state,
without reversing library dependencies. Only the binder can construct facts;
callers cannot replace kind 50 with 49 or feed normalized registry kind 0.
Inputs: explicit Gog descriptor profile, requested widened type ids, parsed
FigurenHeader entries and AnnoGame image. Return sorted owned selected New facts
(id/kind/HP/yield and source hashes). No registry digest or production type repair.

Read name mappings with existing checked PE helpers and uniquely backed readable initialized
windows (the actual mappings are in writable .data, not read-only .rdata), not a new PE parser. Require the addressed GOG PE identity/layout and
semantic class labels at 49/50/51; this is a behavior-profile check, not a species
roster. Do not compile the decoded mapping tables. Names remain file-backed data.
Match whole first-NUL names, original first matching ordinal. Required needed
records/entries must be unique; duplicate or unsafe/unknown names refuse as a
conservative adapter boundary. Unrelated source entries do not impose a roster cap.

D: Game 0x1000ff6d–0x10010016 resolves BEWOHNTYPE (193 name ordinals);
0x100101b9–0x100101cf zero-fills present LOGIC's 44-byte buffer and HP store
0x10010387 reads offset 0x1e. Missing LOGIC retains constructor HP 5; present
empty/short LOGIC zero-extends. >44 refuses. Needed BEWOHNTYPE must be explicit,
terminated and fit before the original lookup-buffer overlap (84 bytes).
D: WARE 0x100103a4–0x1001045d matches 143 goods ordinals and stores the byte on
match; constructor goods is 0. Missing WARE preserves 0; unknown present names
conservatively refuse instead of guessing retained/default behavior. Present
WARE must terminate within the original 256-byte buffer. The original getter
0x10013be0–0x10013ce7 is SEGMENTED, not a flat table:
1–8/56–90/132–136 use base 0x100251f8; 9–40 base 0x10025234;
41–55 base 0x10025178; 96–131/142 base 0x100251e4;
137–141 base 0x100251d0; other ordinals/null pointers use 0x100261e8.
These are image layout/rule facts, not engine goods/class caps.

## Pure projection boundary

Input: sealed descriptor, explicit supplied manager success/null, assigned id,
creation time, three binary32 words, supported area and home facts, prior spawn
fields, sweep time and caller-owned previously consumed helper-draw count.
Output: owned optional defined animal fields, projected spawn fields, ordered
semantic effect stages and unchanged contextual draw count. Stages describe a
projection, never real registration/spatial links or actual callbacks.

Decode finite binary32 using integer sign/exponent/mantissa arithmetic. For
supported nonnegative x/z, check truncation toward zero and area bounds; reject
nonfinite/out-of-range values. Height must be finite and is retained bitwise.
Ordinary home must equal decoded x/z. Cave requires explicit resolved entrance
presence and separate in-bounds target anchor; no assertion that a supplied
entrance came from the original provider. No upstream/cross-host FP parity claim.
Keep widened type/assigned ids and full time separate from original u16 fields;
the original projection adapter refuses assigned ids >65535, not an engine cap.
Home low-word projections stay distinct from widened validated tile coordinates.

Null outcome returns no animal and unchanged four spawn fields; previously used
draw count is retained, not rolled back. Geometry/source validation is adapter
admission, not a claim that the original null branch evaluates a home callback.
No RNG input/generator, pointer image, uninitialized padding or mutable global.

## Hand-derived acceptance before implementation

| Requirement | Legal / distinguishing wrong sibling | Planned evidence |
|---|---|---|
| Real mapping not route/registry | HIRSCH kind 49, HASE kind 51 vs RAUBTIER kind 50; new type id 90001 | sealed binder, wrong source/class and unknown/duplicates refuse |
| Source defaults | absent LOGIC HP 5 vs present empty/zero HP 0; short one byte at 0x1e | exact HP facts, original file oracle |
| Numeric yield | absent WARE 0; FLEISCHTIER 137 vs carried goods 69 | segmented getter, alternate authored goods, unknown/refusal sibling |
| Initial/type quantities | ordinary 512, id 128 ->2048; descriptor capacity does not set quantity | literal expected effect fields and actual source mutants |
| Width/time | assigned 0/65535 legal; 65536 adapter refusal; time 65538 ->word 2/full time retained | separate engine id and original projections |
| Geometry | x1.25/z3.75 ->home1/3; home10/20 refuses; cave home5/6 legal | integer decoder, bounds/nonfinite controls; trusting-home mutant fails |
| Spawn success | sweep byte250 ->54, packed0x83 ->0xc3, descriptor yield !=carried goods | ordered stage/field expectations |
| Null | old id17/time9/packed3/goods255 unchanged, no animal | premature occupancy mutant fails |
| Ownership/determinism | destroy image/record owners, repeated result equal, refused inputs unchanged | complete owned fields/source hashes |
| Bounded original file | 368 entries, 29 kind49/0kind51; every HP/id/kind/yield independently parsed | required real-descriptor projections, not runtime creation |

All new C++/causal/gate checks NOT_RUN at this contract checkpoint. Existing
74-vector pure decisions stay unchanged; no full/heavy/pins campaign is needed.

## Implemented profile and verification boundary

`read_wildlife_names` reuses the checked PE view with a new initialized/readable
file-window query. The old readonly query retains identical checks; actual GOG
name tables live in writable .data, so copied-file reading must admit it.
The initial real-file mapping check exposed the erroneous readonly assumption;
that failed result is retained. There is no live writable mapping or weakened
kernel contract. Backing/overlap/raw-vs-virtual checks remain shared and strict.

`bind_wildlife_creation_descriptors` constructs sealed `WildlifeCreationDescriptor`
values. Only needed entry ids are indexed; requests sorted/distinct, unknown/missing
ids refuse. Original observed entry flag 0 is this bounded profile; other flags
refuse conservatively. Nonrelevant records/entries do not define a roster. Duplicate
needed fields refuse even if identical. `source_sha256` identifies id/flag followed
by needed record name/length/payload envelopes in source order, not authenticity
or the complete original file. Image hash identifies the supplied mapping bytes.
No unused modern registry field or digest becomes proof.

`project_wildlife_creation` consumes no draws and returns defined typed projections
only: widened type/id/full time/home and distinct original id/time/home words,
HP/quantity/carried goods/yield, flags/action/request/current/behavior/configuration,
retained position words and explicit projection stages. Null has one ManagerNull
stage, no animal and unchanged original spawn fields. No actual registration,
spatial links, callbacks or rollback occurs. Geometry admission precedes projection;
ordinary inconsistent home refuses, cave target is independently supplied and bounded.
Negative/nonfinite/out-of-int32 x/z refuse; subnormal/signed-zero truncation is checked
by integer arithmetic. Finite height is retained without arithmetic. HP 0 does not
close the later zero-HP prey/combat question.

Synthetic kind 51/new type 90001 are legal without production edits. Tests retain
caller owners only through binding, then inspect facts/effects after destruction.
Independent original-file oracle (written before the C++ real-data comparator)
finds 29 kind49/0kind51 among 368, four zero-HP and yields 0/137/138; all numeric
facts compared against raw-source rows in focused gates. No raw strings are used
as yield identities. This is real-descriptor projection under authored supplied
manager/time/position inputs, not original runtime creation or a World canary.

Actual private TU mutations challenge initial amount, type adjustment, cargo,
null occupancy, trusting home and flat goods mapping after a legal executable.
Canonical source and binary are preserved. Focused Debug/ASan receipts and final
review disposition are in the [evidence](../orchestration/evidence/wildlife-create-effects-1.md).
Original/HE runtime, upstream cross-host FP/cave provider, allocator handler,
persistent World/manager/save/tag/PRNG/default integration remain outside scope.

### Final source-name safety discriminator

D: the lookup writes its output name at stack +0x88 while WARE input starts
at +0x124 (Game 0x100103d8–0x10010400 / common copy 0x10003493). A mapped name
plus terminator must fit in that 156-byte nonoverlapping span for this adapter.
Authored name length 155 is legal; 156 refuses before binding. Apply the same
conservative name bound to this profile's mapping reads; it is not a global engine
text/content limit. The actual GOG mappings remain supported. Earlier 18-case
Debug/ASan gates are retained at c82aec30; this last reader-only refinement needs
only the six affected creation cases rerun, with the 12 unchanged regressions reused.

### Cave target's integer-to-binary32 boundary

The helper's cave branch also performs FILD target integer ->FSTP binary32 before
truncating the home (Welt 0x10092f84–0x10092f90). A supplied target integer must
therefore be exactly representable as binary32 in this profile; otherwise it needs
unproved rounding and refuses. Authored 16777216 is exact/legal, 16777217 refuses.
Integer significand-bit validation admits larger exact values without an engine
coordinate cap. This narrows only cave admission and introduces no FP rounding
policy. After the completed name-bound six-case gates, rerun only the five affected
effect/geometry/ownership/descriptor-projection cases; source-refusal and all prior
unchanged regressions keep their earlier receipts.
