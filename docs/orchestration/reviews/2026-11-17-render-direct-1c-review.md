# Review: RENDER-DIRECT-1c (the atlas pages on the GPU)

Reviewer: the orchestrator, 2026-11-17. Branch `task/RENDER-DIRECT-1c`, head `b3214e22`. Packet:
`tasks/render-direct-1c.md`.

**Verdict: CHANGES REQUESTED.** Two findings. The first starts with an error in the packet itself.

What holds:
- A separate unit (`atlas_upload.hpp/.cpp`, its own WGSL module) that touches no `GpuFrame` state.
- The texture: RGBA8Unorm, never sRGB, 2 mip levels, one `writeTexture` per layer and level.
- Refusal by a pure, tested function: no layers, layer limit, dimension limit, or budget.
- **The UV rule:** realised as `textureLoad` at origin plus pixel, in one shared `sample_placement` function,
  consistent with `gpu.wgsl`'s "never filtered" rule.
- **Exact readback and sampling on both named adapters,** on the synthetic set and on `lan5.txt` (all groups on
  the 4090, the pair on the Radeon).
- **The existing GPU suite** is unchanged: 129 of 129 on the 4090.
- **The idle check** before the timing is recorded.
- **The pair rule** `{g, g+1}` is derived from `iso.md`'s level choice and hysteresis. Accepted.

## F1: the worst pair was never uploaded (the packet named the wrong scene)

The packet's step 2 says "`02_1024_easy_128.sze`, groups 0+1, the worst pair, 340 MiB". That is the
orchestrator's error. RENDER-DIRECT-1b's F5 found the 340 MiB pair in **`Scenes/0010SingleEndlos/endless02.sze`**;
`02_1024_easy_128.sze` was 1a's worst *all-groups* scene.

The report repeats the label "worst adjacent pair, 340 MiB", while its own table shows that scene's pair at 40 MiB
(1+1 layers, uploaded separately). So the case that tests the integrated budget was never run.

**Fix:**
- Upload `endless02.sze` groups 0+1 **as one resident pair**, the way `resident_atlas_groups` names it, on the
  Radeon, and on the 4090 with all three groups.
- Prove the readback byte-exact.
- Add its VRAM, upload and readback times to the table.
- Correct the scene label in the report and in `atlas.md` §11.

## F2: the budget gate is never exercised with a real budget

Both upload tests pass `budget_bytes = atlas_pages_bytes(pages)` (`atlas_upload_test.cpp` lines 187 and 329): the
budget equals the need by construction. Whether a real page set fits the budget the compositor would grant is never
checked. That budget is `atlas_budget_bytes_before_sprite` minus the sprite atlas's footprint, the
`cb_atlas_budget_bytes` path.

**Fix:**
- In the F1 case, compute the budget as the compositor would on that adapter: the adapter type and limits from the
  opened device, and the sprite atlas's footprint for that scene, either the real one after its sprites are packed
  or a stated measured value.
- Report needed against allowed. If the 340 MiB pair does not fit the iGPU's 384 MiB once the sprite atlas is
  subtracted, that is a finding for the owner (OQ-096b), not something to fix here.
- Add one synthetic test where the budget is one byte short, through `upload_atlas_pages` itself, not only the pure
  function.

## Gates

CI must be green on the final head; run 44089922321 is still running on `b3214e22`. The F1 and F2 cases are
measurement-free correctness checks, so no quiet slot is needed. Only the new timing rows want one: ask for it, or
mark those rows indicative.
