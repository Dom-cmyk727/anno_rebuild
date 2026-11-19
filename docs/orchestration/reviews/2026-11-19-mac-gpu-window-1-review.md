# MAC-GPU-WINDOW-1 — diagnostic review, 2026-11-19

**DIAGNOSTIC APPROVED AND CLOSED; candidate 10 remains FAIL.** Four requested
investigations executed, 0 passes / 4 failures. The original no-frame failure
was not reproduced and its hidden-versus-acquire cause remains unresolved.
No code, binary, timeout, registration, original file or harness change.

Candidate head `95897a55bc939a185766b7207e69d098997cdb1d`, tree
`d65099256a598f7ce82d37ea303b81ff87088542`, detached/clean before and after.
Existing Release executable SHA-256:
`92c8f901be4cbee26a9347f4bc30293a1899d51b0db82cf9ee3043fc246e4986`,
identical to the original gate receipt. Exact-head CI lookup found zero runs;
no hosted or canonical gate PASS is claimed for this diagnostic.
Kernel c9259c16 and its clean tree were preserved.

## Four results and precise failure boundary

| Condition | Exact case | Exit / elapsed | Successful present returns | First present return | Failure |
|---|---|---|---:|---:|---|
| A | viewer_wiring_ui_magnifier_range | 1 / 1.899 s | 17 | 281.670 ms | Script line 28: zoom.target 20000, expected 10000 after click_px 835 163 |
| A | viewer_wiring_ui_magnifier_range_stepped | 1 / 31.762 s | 3627 | 190.381 ms | Script line 29 waits 30000 ms for view.tile_w 42; remains 20 after click_px 805 145 |
| B | viewer_wiring_ui_magnifier_range | 1 / 1.682 s | 17 | 194.455 ms | Same line 28 zoom assertion |
| B | viewer_wiring_ui_magnifier_range_stepped | 1 / 31.739 s | 3626 | 198.779 ms | Same line 29 wait/assertion |

All four run script lines after successful frame progress. Across 7,287 recorded
frame attempts, every acquire is SuccessOptimal with a texture and successful
present-call return. No recorded acquire_skipped/retry/fatal/timeout/occluded
status; zero trace drops, terminal run_end status 1. This is not physical scanout.
The original artifacts 1084/1085 instead abort before the script at no frame within
30000 ms; those failures remain preserved and are not superseded by these results.

## Conditions and limits

The owner confirms the old full-gate display was **unlocked and awake**. The owner
also confirmed Terminal was arranged frontmost with a small footprint and the
Mac unlocked/awake before A. CUA Terminal control was refused and not bypassed.
During-launch viewer coverage/focus was not reliably observed; the app-only
Finder screenshot and missed short-lived window inventory do not establish it.

B preserved the ordinary automated exec/Python/subprocess launch and registered
app arguments, real assets/default adapter 0/four inner threads. A visible-versus-
invisible contrast was NOT established. Differences from the old full run include
serial invocation versus four-job campaign, direct wrapper versus CTest ancestry,
existing --trace/explicit retained outputs and an owner-arranged current Terminal
versus unknown old coverage. Other old inherited environment remains unknown.

The exact wrapper clears inherited ANNO_VIEW_TRACE unless --trace is given.
Existing tracing was enabled identically for A/B to preserve diagnostic evidence;
no assertion/watchdog was changed. Direct wrapper investigations are not canonical
acceptance gates. Four invocations only; no retries or whole-suite run.

The candidate SDL handler changes hidden_ without recording visibility events.
Successful acquires here do not reconstruct the old run. The causal verdict is
**insufficient evidence to distinguish hidden_ from acquire/back-off in the
original failure**, not proof of a locked display or proof of a renderer defect.

## Independent review and receipts

Reviewer `/root/review_town_ci_close` independently parsed all four raw frames
TSVs, old/new stderr/script boundaries and current source. Counts, timestamps,
acquire/present statuses and disclosed limits reconcile. APPROVED diagnostic,
no blocking evidence finding; candidate remains FAIL. No test rerun for review.
Root verified all 85 manifest artifact hashes and the handoff hash.

Relative to the unchanged candidate checkout:

- `cache/mac-gpu-window-1/handoff.md` SHA-256
  `2bc4c0c14d5ac45cb5f0dbd9bcb658a0dc4071bc3455b5c3225ee5daa62f8b4b`.
- `cache/mac-gpu-window-1/sha256.json` SHA-256
  `35d3cac8a86aa34ecb9fd1ab7db705bb1c7ef0c3ed464eec0c08d4328b6ab9c6`;
  85 paths relative to that evidence directory.
- A range child stderr: `b2bd6675eba32d9ee764777d5884a0316439cca1309376a934e6e48bb035be7e`.
- A stepped child stderr: `5bde4cc241c499dc7f0d0b882fa79698544f49fa0e7e33e0899c1be0843851fc`.
- B range child stderr: `39e0044651e8251077eb2c07dd4fdc7c056490042d459e7e1697488f68b02dde`.
- B stepped child stderr: `23ef0ed6bb41b404f626cfeb07c4eb1e1e23423ba55fde3b642004fe825e8a3c`.

Child stderr/scripts/session/traces and wrapper output are retained. Legitimately
empty stdout/stderr files keep their empty-file hashes; no missing text recreated.
Raw evidence remains Mac-local and ignored; Windows can read this review in Git,
not the private logs unless separately transferred through an authorized channel.

## Windows handoff and next Mac task

1. Original first-frame cause remains open. Existing observation cannot identify
   which gate blocked it. Size a narrow visibility/acquire diagnostic rather than
   asserting that unlocked/awake or an available adapter guarantees a drawable.
   No new display policy is approved merely by this inconclusive result.
2. Track the post-script zoom failures separately. Every run reports density 2.00;
   script resize 1024×768 yields drawable 2048×1536. Reviewer source inspection
   identifies a concrete hypothesis: click_px is described in drawable pixels,
   constructs SDL button coordinates, and UI handling applies density conversion.
   Trace script coordinate → SDL event → UI hit/reference → dispatched magnifier
   command and resulting zoom before fixing or attributing the failure. This
   hypothesis is NOT a diagnosis and cannot explain the earlier pre-script abort.
3. Keep the reconciled ten named-adapter GPU skips; DXGI platform and texture
   ceiling opt-in. No magnifier failure is waived and no new full gate is requested.

The executor returns to [WILDLIFE-GROUND-FACTS-1](../tasks/wildlife-ground-facts-1.md)
for the next bounded owned metadata binding. Windows owns UI/harness follow-up;
no Mac repair campaign is added. Main remains untouched.
