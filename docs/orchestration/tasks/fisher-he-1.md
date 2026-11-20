# FISHER-HE-1: the closed-entrance fisher, pictures against the History Edition

**Status:**
- **Written and RELEASED** 2026-11-16 by the Claude Code orchestrator for **the UI Executor**. The owner was asked
  who makes the History Edition captures and answered "UI Executor" (2026-11-16).
- **The acceptance the owner set** (2026-11-15): "fisher hut with every entrance on land. this needs to be matching
  pixel perfect with HE as reference."

**Read first:** `docs/re/fisher.md` §10 on `origin/task/FISHER-RE-3` `5901b846` (accepted, not landed), above all
the capture card in §10.3; [the review](../reviews/2026-11-16-fisher-re-3-review.md);
`docs/testing/original-harness.md`; `docs/research/he-vs-gog-comparison.md`.

**Rules:** small steps, each reported to the orchestrator; no original pixels, frames or script text in git
(captures stay in `cache/` or outside the repository); silent runs; a desktop session of the History Edition needs
the owner to agree in your session; landing stays with the orchestrator.

## Changed 2026-11-16 by the owner

Reference tests against the running original are done on the **History Edition** (owner, confirmed to the
orchestrator; "Simply because its more stable"). For step A that means:
- the owner plays the History Edition himself from the UI lane's short recipe and records video; the UI lane
  analyses the videos;
- the GOG desktop runs are stopped; the partial GOG record already made (to game ms 79,307) is kept as
  supporting evidence, labelled partial;
- the card's closed site (type 810 at 242, 217, rotation 1) is kept, because the closed case exists only at
  such a spot; the owner's own, more natural sites are extra open controls;
- nothing can be read from the History Edition's memory, so every event is taken from pixels, with its frame
  and the analyst's confidence.

## Step A: the reference captures of the originals (first, about 2 to 3 hours)

It depends on no rebuild code, and it is the reference everything else is judged by.

1. **Both editions**, each from a fresh start of `Tutorial/02Trade01` (the card gives both paths), human player 2,
   the city on island 3: GOG 2.0.0.5 through the harness, and the History Edition.
2. **Two runs per edition**, never both huts in one run (their footprints overlap):
   - **closed:** type 810 at (242, 217), rotation 1; the entrance arrows on (245, 218) and (242, 218);
   - **open control:** type 810 at (243, 216), rotation 0.
3. **Record**, as the card says: a fixed viewport, rotation, zoom and client size, the same for both editions; from
   before the build is confirmed to 120 game seconds after it, at normal speed; the first appearance of the boat,
   its tile positions and headings, each catch, where it stops and when it disappears; the stock the hut shows at
   each of those moments.
4. **If an edition refuses the start or the build** (a tutorial lock, a placement refusal, a different coast
   rule), keep that refusal as the result and stop. Do not move the hut to make it fit.
5. **Report** to the orchestrator: an event table per run (game time, frame, tile), where the two editions agree
   and differ, and whether the prediction of the GOG read holds (the boat leaves and catches, cannot return, the
   hut gets no fish, the boat disappears after a wait). Say plainly what could not be captured.

## Step B: the rebuild can show it (small, after the report of step A)

`--play` creates the world with default extensions (`play.cpp`), so the fisher rule cannot be switched on in the
viewer. Add a developer switch for it in `--play` (off by default; the sim rule itself stays behind its existing
flag), and make the raw stock, output and workers out of the hovered hut readable together with the game time
(the developer inspection line may already do most of this). App code only.

## Step C: the comparison (after FISHER-1f has landed)

The rebuild at the same two sites with the switch on, captured the same way, compared with the History Edition
frame by frame at the matched events. "Pixel perfect" is judged on the positions and frames of the boat and on the
hut; state every difference with its frames. Not released until A and B are reviewed.
