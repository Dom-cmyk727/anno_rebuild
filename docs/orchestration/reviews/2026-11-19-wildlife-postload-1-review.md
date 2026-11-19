# WILDLIFE-POSTLOAD-1 — Mac orchestrator review, 2026-11-19

**CHANGES REQUIRED at the initial reviewed head: two focused documentation fixes.**
No engine correction or repeated test campaign requested by this review.

- Initial head `e05d9504f2907de1f6b8f2b4d52197f8767f35d4`;
  tree `44f6c04810c1664bead11398f19ee7f33faafd1e`.
- Research source `09614995c9e9b9c7f187e271b9ea6b3ebaf985bc`;
  tree `5f5dc9fb00adf4b23977be966e0bbef291075825`.
- Original-head [CI44446560786](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44446560786)
  pending at initial review; a correction requires its own exact-head CI identity.
- Contract `docs/sim/wildlife-postload.md`; evidence
  `docs/orchestration/evidence/wildlife-postload-1.md` on the task branch.

## Independent findings

1. **P2 — nondiscriminating boundary vector.** Tests/evidence reviewer
   `/root/review_town_ci_close` found V12 does not distinguish zero-padding from
   nearest-edge clamping: the nonnegative4x4 maximum sees the same legal boundary
   values, with clamping only repeating samples. Do not demand failure from this
   equivalent interpretation. Test inclusive W/H and excluded H+1 padding only
   where genuinely supported by the helper's input representation; otherwise
   narrow the claim. Proposed vectors remain NOT_EXECUTED.
2. **P2 — raster arithmetic overstated.** Primary reviewer
   `/root/review_docs_sim_dispatch` independently read a3e7d–a3e90:
   `(old &0xf0) | (((converted &0xff)+8)>>4)`, no final low-nibble mask.
   Converted byte248–255 can set bit4; old0xa0 becomes0xb0. Correct the contract
   and evidence's unconditional high-nibble-preservation statement. Upstream
   numeric domain remains unresolved; this is not an observed runtime overflow.

Both findings require focused prose correction/re-review only. Exact arithmetic
and genuinely distinguishing vectors belong in the existing contract, not a new
review campaign or generic test framework.

## Other reviewed evidence and remaining obligations

Primary review confirms first-call c2 gate then fill, second-call navigation plus
separate raster/generation, and the complete call-free third graphics leaf. V9
is a valid static mismatch:20+250=270, half135, versus wrapped14 leaving vertex
max18/half9. Future repair must examine both the sum mask and final min(mx,255)
clamp. No reproduction, shipped-map failure or compatibility disposition is
claimed or released here.

Root verified47 SHA-256 receipts, docs-only delta from218b838e, unchanged fixtures
and report/evidence-only source-to-head delta. Docs/link check passes. All12
original vectors are hand-derived NOT_EXECUTED. No C++/reference gate was run or
required for these docs; prior29/29/1 is unchanged reused evidence, not new passes.
Catalogue/type-array binding, c2-zero preprocessing and raster inputs/consumers
remain unresolved;4745 is still PRE-callback supplied consumption.

The proposed next implementation is a pure UNWIRED graphics finalizer, preserving
original fill→navigation/raster→cleanup publication order. No World/native/tag/
default change, navigation repair or global-cursor assumption is authorized by
this research result. Release follows focused correction approval.
