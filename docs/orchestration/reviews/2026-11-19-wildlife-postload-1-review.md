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


## Focused correction review — 2026-11-19

**LOCAL REVIEW APPROVED at corrected head `1628ba18a5b6eefb55684afc3091ba3be9fa10c8`,
tree `7681810bbf7c5d549583c65baeb68ae1a7430940`. Both P2 findings closed.**
Exact correction [CI44447642251](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44447642251)
remains in progress; the original-head run is not substituted for it.

Both original reviewers independently approved the focused docs delta. V12 is
sanity-only and explicitly equivalent under clamping; V12a/b require an already
admitted low-level representation and are not fabricated raw Terrain inputs.
Literal raster mask→round→OR and V13 are correct, with upstream domain still U.
V9 requires both sum-mask and final-clamp audit plus exact extrema bytes. All15
vector rows/13families remain NOT_EXECUTED; no new dynamic proof is claimed.
Root verified8 correction receipts (including the historical contract via git),
unchanged original evidence and docs-only diff. No test campaign repeated.

[WILDLIFE-GFX-FINALIZE-1](../tasks/wildlife-gfx-finalize-1.md) is independently
scope-reviewed and RELEASED from corrected1628ba18. Implement only the pure owned
third-call leaf and its independent reference/causal/digest checks. Do not wire it
before fill/navigation or change World/native/default/identity. The catalogue,
navigation discrepancy and raster inputs/consumers remain explicit follow-ups.
Executor reports directly for review and next release; main untouched.
