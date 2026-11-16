# UI-MINIMAP-1 against the History Edition: the orchestrator's own picture check (2026-11-16)

**Why:** the owner's instruction for this review: "just start the viewer maximised, screenshot it and check it
thoroughly against the HE minimap".

**What was compared:**
- **Ours:** `task/UI-MINIMAP-1` `8978a6a1`, built by the orchestrator in its own worktree; the viewer on the 2560 x 1440
  monitor, maximised (client 2560 x 1369), `--play` on `Tutorial/02Trade01` with `--player 2 --construction-menu on`,
  RTX 4090; screenshots by the dev script's `screenshot` at the start view and at the closest zoom on the fisher
  site.
- **The History Edition:** the owner's recording of the same tutorial (`ui_5`), frames at 0:30 (the start view) and
  6:12 (the fisher site). The recording is 1920 x 1080 of a 2560 x 1440 desktop, so it was scaled by 4/3; the two
  minimaps were then aligned on the chrome (mean difference 14 of 255 over the whole patch).
- All pictures stay outside git.

## What agrees

| Part | Ours | History Edition |
|---|---|---|
| Position of the minimap, its frame and its six buttons | the same after alignment | |
| Parchment colour at four points inside the diamond | (175, 155, 111), (128, 107, 72), (167, 147, 107), (172, 154, 111) | (173, 153, 107), (126, 105, 70), (164, 145, 105), (170, 153, 110) |
| Islands | land box x 137..249, y 45..112 | x 136..249, y 47..112; best overlap at a shift of (0, 0) |
| Land colour | (110, 105, 5) | (108, 99, 19) |
| Settlement colour | (0, 141, 123) | (24, 145, 119) |
| The own-figure marker | 3 x 3, green, at x 200..202, y 69..71 | 3 x 3, green, at x 201..203, y 69..71 |
| The view box | white, 1 px | white, 1 px (softened by the video) |
| The view box's size against the zoom | at two zooms, ours times the measured zoom ratio is within about 10 % of the History Edition's | |

The zoom ratio between the two views was measured by correlating the world pictures: at the start our view is 2.08
times farther out; at the fisher site our closest zoom is 1.42 times closer than the recording.

## Findings

- **F1, the scroll's left end is cut off (visible, to fix in this packet).** In the History Edition the rolled left
  end of the parchment hangs over the map with a round end, and the diamond's left tip with it. In ours it is cut by
  a vertical edge a few pixels left of the panel, with a rectangular notch. About 12 to 25 pixels of the chrome are
  missing there.
- **F2, the parchment is speckled.** Ours shows per-pixel grain; the History Edition's is smooth. Some of that is
  the video, but the grain is strong at full size. Find out how the History Edition draws that quad (filtering,
  colour depth) and match it; the History Edition is the reference.
- **F3, the settlement's coloured cells (minor, uncertain).** Ours has a ragged lower edge that reaches past the
  island's land; the History Edition's teal sits on the land. Check which cells count as owned when OQ-629 is read.
- **F4, the start view differs (not the minimap's fault, but it shows there).** Our tutorial starts 2.08 times
  farther out than the History Edition and at another camera position, so the box is larger and elsewhere. The
  scenario's script has a `[CAMARAPOS]` section; check whether the start camera and zoom come from it.
- **F5, the closest zoom (a question).** At our closest zoom the world is 1.42 times larger than in the recording
  at 6:12, which the UI lane took as the History Edition's closest. Either the recording was not at the closest
  zoom, or our closest is closer than the original's. The owner's clean run settles it; step C depends on it.
- **Seen beside the minimap, for the render lane:** shallow water over sand is a blue and yellow checkerboard in
  ours and a smooth blend in the History Edition.

## Verdict

The minimap's drawing matches the History Edition in position, islands, colours, marker and view box. **F1 and F2
are to be fixed before UI-MINIMAP-1 lands**; F3 with B2; F4 and F5 are questions for the UI lane and the owner.
