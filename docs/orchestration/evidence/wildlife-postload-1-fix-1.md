# WILDLIFE-POSTLOAD-1 — fix round 1 correction receipt

READY FOR FOCUSED RE-REVIEW, 2026-11-19. Docs only; no engine repair or campaign.
Correction base `e05d9504f2907de1f6b8f2b4d52197f8767f35d4`; research source
`09614995c9e9b9c7f187e271b9ea6b3ebaf985bc` remains frozen. The previous
[evidence](wildlife-postload-1.md), its raw logs/listings and code pins are unchanged.
Its former padding discriminator and unconditional secondary-raster summary are
historical and superseded by the [current contract](../../sim/wildlife-postload.md).
For the old contract SHA receipt use `git show 09614995:docs/sim/wildlife-postload.md`,
not today's corrected file. This preserves the old evidence rather than rehashing it.

## Corrections

- V12 is a 1x1 zero/nonzero sanity control. Nonnegative max over the 4x4 sees the
  boundary samples already; nearest-edge clamping repeats them. Zero padding and
  that clamping interpretation are observationally equivalent for this predicate.
  V12 does not distinguish them. V12a supplies a NONZERO logical W/H border only
  conditionally on a low-level validated grid admitting it; raw Terrain forces
  those logical borders to zero, so the case is NOT_EXPRESSIBLE through raw input.
  V12b similarly requires an exposed physical H+1 padding row with a sentinel;
  original bounds exclude it. Raw Terrain does not expose that row. Otherwise
  retain static bounds proof and narrow dynamic claims. No supported input or
  helper interface is widened just for a test. All cases remain NOT_EXECUTED.
- Re-read original AnnoWelt a3e7d–a3e90: converted is masked to a byte, then
  `(u+8)>>4`, then OR with `old&0xf0`, with NO mask of rounded to 0x0f. Thus
  u248..255 ->16 can set high bit 4. V13 old0/converted247,248,255 ->0x0f,0x10,0x10
  distinguishes a later low-nibble mask. High preservation for all old bytes
  requires a proved u<=247 bound; the runtime numeric-table domain remains U.
  No broad numeric-producer read or code fix was attempted.
- V9 remains STATIC/NOT_EXECUTED. Any future repair must audit BOTH the sum's
  `&0xff` and the separate `min(mx,255)` before halving. Removing only the sum
  mask yields127 instead of135 for the authored max270 example. Both classify
  mixed; a class-only test misses the remaining byte mismatch. No route_world
  repair or compatibility change is authorized or made.

## Verification and exact diff

`cache/wildlife-postload-1/fix-1/contract.diff` is the exact contract-only diff
against the frozen base. Full final commit diff/head/tree accompany direct handoff.
New docs/links/anchors/diff and ownership checks only; no C++/reference suite or
original-prefix rerun. V1–V13 plus conditional V12a/b remain HAND-DERIVED and
NOT_EXECUTED (13 numbered families /15 table rows). The original 12-family docs
receipt remains historical. The correction check receipt is separate.
No changes to source/tools/CMake/fixtures/native/state/tag/default/compatibility.
Original evidence and route source identities verified below. Exact-head CI is
pending at publication, and independent focused re-review remains pending.

Decisions and reasoning:
- Correct an observational-equivalence overclaim without fabricating raw border inputs.
- State literal mask/round/OR semantics; unknown reachability is not a bound proof.
- Require exact extrema bytes and preserve the separate clamp obligation.
- Preserve frozen evidence and append a correction receipt instead of repeating gates.

QUESTIONs: none. No new owner policy or implementation decision is taken.

## SHA-256 receipts

| Artifact | SHA-256 |
|---|---|
| `docs/sim/wildlife-postload.md` | `21358ec0c0060d244f771f202eff372605694717896d8150ebfd2df9b08ec348` |
| `docs/orchestration/evidence/wildlife-postload-1.md` | `13a16443482ef749731b3d4309b342954686f04572d4502127a98af96f46b4f6` |
| `cache/wildlife-postload-1/fix-1/raster-rounding.private` | `e9efc4dd714fc1c600b73c103fecaae182a35b644713d4ea6a2452d6c2aa3ac8` |
| `cache/wildlife-postload-1/fix-1/graphics-bounds.private` | `d46157f8e062790a8c18f38699f7f2622037e025e4052adbb7c96459faf3f7ff` |
| `cache/wildlife-postload-1/fix-1/contract.diff` | `74300f21f60e24c4823afe6bfb9693aafd8712761578ff61566d650c1743a045` |
| `src/sim/src/route_world.cpp` | `22a879c4a7bd14b241af13e07fd123dcff20bbe381d281890ba97b67ded12889` |
| `historical-contract@09614995` | `8dd22fbe821b7a2575dc4d4453da0f6b006998aa583b5e6a6f97bd9698c6d2ef` |
| `cache/wildlife-postload-1/fix-1/docs-check.log` | `fe0169f201bfc3a828ee7b172fc3cedc86c621c973554bb114b009fa31042d0d` |

Correction docs check (no execution verdict inferred):

```text
DOC_CHECK files=2 links=10 hand_vector_rows=15 hashes=7 missing=0 status=PASS
FROZEN_EVIDENCE unchanged; source/tools/CMake/fixtures unchanged
EXECUTION: C++/reference suites NOT_RUN; proposed vectors NOT_EXECUTED
```
