# FISHER-MAP-1, which shipped map can carry the fisher canary: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/FISHER-MAP-1` `21e373e7`, on `task/SO-1b` `d19c446a`. Docs only.
**CI:** green.

## Verdict

**ACCEPTED. The map is chosen:** `0010SingleEndlos/01_1024_easy_256.sze` first, `Tutorial/02Trade01.sze` second.
FISHER-1e is released on them.

## What the census says

- **50 maps:** 14 compile unchanged, 36 refuse. Of the 14, 11 create and start normally; 3 refuse in the unit import.
- **Fisher classes among the 11:** A 2, B 3, C 6.
- **The five small names (SL-1 and SL-2) alone unlock no map.** Every refused map has another blocker. The most
  common ones are WON (35 maps by the reviewer's own count with `tools/bigbrother.py`), the figure and building
  object definitions, QUESTLOG_SHOW, SHOWTEXT and INHABITANTCOUNT. Four single missions (`Single_05`, `_06`, `_08`,
  `_12`) need exactly QUESTLOG_SHOW, SHOWTEXT, INHABITANTCOUNT and WON. So the mission outcome is the name that
  opens the most maps. That is input for the owner's decision on the SL steps, not a release.

## The choice

1. **`01_1024_easy_256.sze`, class A.** Three fisher huts of type 3978 already stand, owned by Venice (player 5),
   each with one open entrance and fish in reach; the worker is figure 142 on route 45, the body FISHER-1d
   ported. The map loads, creates and starts with nothing changed, and needs no command. It is the purest
   "loaded normally" proof. Its limit, to be stated in the canary: Venice's hut type and its rates, not the
   human hut.
2. **`02Trade01.sze`, class B.** The human player's standard hut, type 810, built by one Build command
   (player 2, city on island 3, at 243, 216, rotation 0). It covers what the first map cannot.
- Not chosen: `endless03`, whose standing hut is type 3510 with figure 393 on route 44, not the ported body.
- FISHER-RE-3 (a hut with every entrance closed) is not needed for either map; it stays queued.

## Checked

- The kernel counts with `tools/bigbrother.py` over the 50 files.
- **F1 (small):** 13 kernels are empty, not 12. `Tutorial/03T_Combat_01.sze` has no kernel items and 31 object
  definitions; the table calls it "active". Its class is unaffected: it refuses at the figure objects. Correct
  the label in FISHER-1e's branch.
- The method: refusal ladders on private copies that are neither run nor committed; world facts from the normal
  loader with only the existing fisher flag on; no projection, no deleted actor, no injected stock.
- No engine code, no pins, no original script text.
