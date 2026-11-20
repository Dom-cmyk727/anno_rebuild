# Review: UI-MINIMAP-2 (the magnifier reaches the wheel's zoom range)

Reviewer: the orchestrator, 2026-11-17. Branch `task/UI-MINIMAP-2`, head `c5b10c85`, which includes fix round R2.
Packet: `tasks/ui-minimap-2.md` on that branch.

**Verdict: ACCEPTED.** It lands under the owner's standing rule.

## The owner's decisions it implements

- **UI-MINIMAP-2 itself,** 2026-11-17. Asked "Go for the UI lane's UI-MINIMAP-2 (magnifier zooms out as far as
  the wheel)?", the owner answered "Yes, go". The wish came from `ui_6.mp4`: "one or two further zoom stages
  ... rather have one too many than too few".
- **R2,** from `ui_7.mp4`: the magnifier's + also reaches the wheel's closest zoom-in.

The original's behaviour stays in the stepped zoom (the 0..2 clamp, AnnoFrame `0x10007710` / `0x10007692`).
Only the continuous zoom, already a rebuild improvement, gets the extra stages.

## What was checked

- **`magnifier_stages`:**
  - closest first: the wheel's ceiling, then doublings of the widest built level below it, then the built
    levels, then halvings of the narrowest down to the floor, then the floor itself;
  - each side is skipped when there is no room, and on inputs that aren't positive and finite;
  - both loops end, since the factor is greater than 1 and the bounds are finite;
  - the stage factor of 2 matches the renderer's derived levels (`ZoomLevels::with_derived` halves the tile
    size).
- **`zoom_floor`/`zoom_ceiling`** are factored out of `zoom_target_after` with the same arithmetic, so the
  wheel is unchanged.
- **The stages are rebuilt at each press,** because the floor follows the window size.
- **The new script facts** (`zoom.target`, `min_tile`, `max_tile`, `at_min`, `at_max`) give the wiring scripts
  both ends, with failure proofs.
- **Unit tests** cover the stages and the target choice; the gate passed 62/62 with 0 skipped.

## Evidence

- CI green on `c5b10c85` (run 44058957987, head verified).
- **The owner's test,** `ui_8.mp4`, through the whole magnifier range: "perfect, that's it exactly like that ...
  works fine looks good". It ran on a local combined build of `main` `4dfdd9b7`, this head unchanged, and
  UI-STATUS-1 `c58dab37`, never pushed.
