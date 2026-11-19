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

Read name mappings with existing checked PE helpers and uniquely backed read-only
windows, not a new PE parser. Require the addressed GOG PE identity/layout and
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
