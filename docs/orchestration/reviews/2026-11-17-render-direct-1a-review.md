# RENDER-DIRECT-1a: the orchestrator's review (2026-11-17)

`task/RENDER-DIRECT-1a` `9773d4d0` (code `7d7c5e49`), Windows executor, base `main` `ac02d4a2`. Files:
`src/tools/bench_frame/main.cpp`, `src/render/tests/residency_test.cpp`, `docs/render/atlas.md` (new),
`docs/orchestration/REPORTS.md`. No render path touched (four files, checked). CI running at review time (run
44002213714).

## What holds

- **The tool** is a `bench_frame` mode that reuses `load_any_scene`, `prepare_objects`, `animation`/`frame_index`
  and `AtlasResidency`; no second implementation. Each page/padding combination is one `ensure()` call with a
  budget that never evicts, so the page counts come from real packing. 519 scenes, none refused, 7 min 12 s. The
  figure catalog, 241,808 frames per group, equals `sprite_fidelity_test.cpp`'s count.
- **The unit test** has hand-computed counts and needs no game files: four 30 px frames fill one 64 px layer; with
  2 px padding each 34 px frame needs a layer of its own; a frame that padding pushes past the page size is
  `Overflow`.
- **The enhanced water's copies** are counted from the code that makes them, with the surf and river exclusions.
- **The GPU limits** were read on both adapters; none of them binds any candidate.
- **The four points of the first pass are answered:**
  - the scene argument works ("-" for the corpus, a path for one scene);
  - the fill table for the worst scene is in `atlas.md` section 3;
  - the static frames' pixels are counted (but see F4);
  - `scene_reclaim_test`'s failure (`CHECK(12 == 11)` after `river_shade_test` in the default order) was shown on
    unchanged `main` `ac02d4a2`, so it is not this branch's.

## Findings (changes asked; document and summary output only)

**F1. The budget is checked against one group, not against what ADR 0007 keeps resident.**
- ADR 0007 keeps the atlases of all three sets resident and draws the finer set minified with mips (decision 2,
  decision 3). Its Consequences already expect "about 270–440 MiB" for that.
- `gpu-chunks.md` 4.3 rule 6 lets the discrete budget hold every group of a scene. The integrated budget may evict
  only other groups' frames.
- The earlier count, 196.7 MiB, was all three groups together. `atlas.md` compares group 0 alone (208 MiB) with
  256 MiB and concludes that the layout fits both GPUs.
- Its own per-group worst cases (208, 64 and 16 MiB) suggest about 288 MiB for all groups and 272 MiB for groups 0
  and 1, the pair a zoom crosses. Both are over 256 MiB, but the worst scene may differ per group, so this needs
  checking.

Asked:
- From the run already made (no new corpus run), and written by the command itself so it can be reproduced: per
  scene, the sum of groups 0+1+2 and of the pairs 0+1 and 1+2. Give the worst scene and its bytes for each, for the
  candidates below.
- Then say plainly:
  - whether the RTX 4090 holds all three groups with ADR 0007's mips;
  - whether the iGPU holds the pair a zoom crosses.
- If the pair does not fit, the iGPU would upload a group while the zoom is running, which the owner's "zoom must be
  seamless" rule forbids. That is the QUESTION the packet asks for: name the scene, the bytes and the options with
  their costs.

**F2. Dropping the mips is the owner's decision, not a layout choice.**
- ADR 0007 decision 3 draws the finer set minified with mips. Taking them out changes how a zoom looks (shimmer as a
  set nears the switch), so per the owner's rule the owner picks the default.
- `atlas.md` must call it a proposal, not "the layout", and carry the facts the decision needs:
  - **The groups already form a mip ladder** (tile widths 84, 42, 20, 10 and 5 px). The finer set is never shrunk
    much more than 2× before the next set takes over (2.1× between groups 1 and 2, plus the hysteresis), so one mip
    level does nearly all the work.
  - **A "full" chain is not a usable candidate with 2 px padding.** Level *n* needs 2^n px of padding or neighbours
    bleed in, so 2 px supports exactly one level. Replace the "full" rows with **one level** (+25 % per page). A full
    chain may stay listed, with the padding it would need.
  - **The cost of each option on both budgets:**
    - one level everywhere;
    - one level on the 4090 and none on the iGPU;
    - none.
- Add an open question for this (the owner), next to OQ-096 (the budget), unless one already covers it.

**F3. The figure needs are the scenario's start units only.**
- `scene.units` holds the ships and soldiers a scenario file places. A running game adds carriers, residents,
  animals and more ships, which the loader cannot tell.
- Remove "figures do not change the fit conclusion" and "a few hundred KB to a few MB". Instead, give the measured
  maximum with its scene, or nothing.
- Say that the figure class needs its own budget with eviction (`gpu-chunks.md` 4.2's hot class), sized later from a
  running game. The packet allows exactly that ("otherwise say it cannot").
- The bytes of the two player-colour options (section 6) rest on the same start units; mark them so.

**F4. The static pixel count is the green test matching green, not player colour.**
- `recolour`'s test is green dominance. Building frames contain trees, fields, grass and painted parts, so 40.6 % of
  frames matching shows that the test cannot be applied to static frames. It does not show that the files mark
  player colour on buildings.
- It is not a look question as worded ("a meaningful share of building pixels would be recoloured").
- Rephrase:
  - no player-colour marking that this test can separate from ordinary green;
  - whether any building in the original shows its owner's colour (a flag, for example) is open, for the History
    Edition or the owner. Add it to the open questions unless the flag entry (`Flagge01_01.PCX`, OQ near line 171)
    already covers it.

## Notes, no change needed

- The corpus mode finds `data/maps/...` relative to the current directory ("run it from the repo root").
  Acceptable for a tool; a `--repo` option at the next touch.
- The 4/3 and +25 % factors are formulas and are labelled as such. Keep it that way.
- `scene_reclaim_test` fails when `anno_render_tests` runs whole in its default order, on `main` too. CI runs every
  case as its own CTest test and never sees it. It goes on the board as a small, separate item, not into this
  packet.

## Verdict

**CHANGES REQUESTED**, about one hour: `atlas.md`, the summary output of `--atlas-report` (the sums of F1), and a
REPORTS addendum. If F1 or F2 ends in a QUESTION, the executor stops and reports it; the orchestrator takes it to
the owner. The measurement itself (tool, test, corpus run) is accepted as it is.

## Fix round (added 2026-11-17)

`6bfb78cd` on `9773d4d0`, CI running at review time (run 44006619634).

- **F1:** `--atlas-report` now writes the per-scene sums 0+1+2, 0+1 and 1+2 itself. The orchestrator counted the
  rows of the generated table at 2048 px pages and 2 px padding:

  | Resident set | No mips: worst / maps over 256 MiB | One mip level: worst / maps over 256 MiB |
  |---|---|---|
  | One group | 208 MiB / 0 | 260 MiB / 5 |
  | Groups 0+1, the pair a zoom crosses | 272 MiB / 1 | 340 MiB / 20 |
  | All three groups | 288 MiB / 5 | 360 MiB / 24 |

  - The worst scene is always `0010SingleEndlos/02_1024_easy_128.sze`. Without mips, five more scenes need exactly
    256 MiB for the pair.
  - No scene goes over 384 MiB in any row.
  - The LAN map (`lan5.txt`) needs 128 MiB for the pair without mips and 160 MiB with one level.
  - The RTX 4090 holds all three groups with one level in every scene (360 of 512 MiB).
- **F1 ends in the QUESTION** the packet provides for (`atlas.md` section 4, OQ-096b). It goes to the owner.
- **F2:** the "full" rows are replaced by one mip level (+25 %), and the padding limit is stated. `atlas.md` calls
  the mip level a proposal; OQ-096b holds the decision.
- **F3:** figures come from the start units only; the maximum is 3,401,840 bytes (74 frames, `single38.sze`, group
  0). The figure class gets its own evicting budget, sized later.
- **F4:** reworded. The green test cannot be applied to static frames. The real question is a new OQ.

**One fix before acceptance:** the new question is numbered OQ-641, which `task/UI-TABS-1` already uses (OQ-639 to
644; `task/UI-PANELS-PREP`, in the candidate waiting to land, uses up to OQ-689). Renumber it to **OQ-690**.

**Verdict: ACCEPTED once CI is green on the renumbered head.** The packet's result is a measurement and a QUESTION
for the owner (OQ-096b together with OQ-096). RENDER-DIRECT-1b is written once the owner has answered.

## Acceptance (added 2026-11-17)

`30a42f55` renumbered the new question to OQ-690. `5fb98192` records the owner's decision on OQ-096 and OQ-096b
in the OQs, `atlas.md` and `gpu-chunks.md` (docs only). The code's integrated default (`gpu.cpp`
`cb_atlas_budget_bytes`, 256) follows in RENDER-DIRECT-1b. **CI green on `5fb98192`** (run 44010395552).
**ACCEPTED.** It lands under the owner's standing rule in the candidate `4dfdd9b7`.
