# FISHER-HE-1 step A, the History Edition at the closed site: review (2026-11-16)

**Reviewer:** the Claude Code orchestrator.
**Evidence:** the owner's recording of the History Edition (played by the owner, 2026-11-16) and the UI lane's
frame analysis of it; both outside git. A partial GOG record from before the owner's History Edition rule.
**Nothing is committed for this step.**

## Verdict

**ACCEPTED as the behaviour reference for the closed site. Not yet a frame reference:** the camera moved and a
statistics screen was opened during the run, the open control was not run, and the entrance markers are not visible
in the History Edition. Step B is released. Step C needs one clean History Edition run first (below).

## What the History Edition shows at the closed site

The hut is type 810 at the card's site on `Tutorial/02Trade01`; the site is matched to the GOG guide frame by
pixels (offset 0, 0), and the History Edition chose the orientation itself.

| Event | History Edition, after the build | Note |
|---|---|---|
| The boat appears at the hut's front corner | 1.30 s | the rebuild's canary spawns its worker at tick 690, 1.38 s |
| Three stops on the water, each further out | 11.15 s, 11.25 s, 11.3 s, with about 1 s of movement between | the rebuild's catch wait is about 5600 ticks, 11.2 s |
| The boat turns home | never | |
| The hut's stock | 0 and 0 % at every moment its panel was visible | |
| The boat vanishes where it stands, from one frame to the next | 38.85 s | seen twice; the second time with no user action near it |
| A new boat appears at the hut | 0.40 s and 0.58 s after a vanish | |

## Against the GOG read (FISHER-RE-3)

- **Confirmed in a running original:** the worker is sent although both entrances are on land; it leaves through a
  neighbouring water cell; it catches three times; it never comes home; the hut gets no fish; the worker slot is
  freed and a new worker is sent.
- **The timing agrees with the partial GOG record:** release about 40.6 s after the build there, 38.85 s here.
- **One visible difference from the read, to settle before the rebuild draws this:** the read has the released
  worker wait (15.0 at rate 0.01, about 15 s) before it is removed, and the GOG memory showed the released figure
  still present with that countdown. In the History Edition **nothing is visible after the release**. Either the
  waiting figure is not drawn, or the History Edition has no wait. Pixels cannot tell which. For the owner's
  acceptance the visible fact decides: **the boat is gone at the release.**
- The earlier "boat appeared about 12 s after the build" in the GOG note was a polling artefact, withdrawn by the
  UI lane.

## What follows

1. **For FISHER-1f's review (Mac):** what the sim keeps and what the viewer would draw after the release. If the sim
   keeps a waiting figure, it must not be visible. A small read may settle whether the original hides a released
   figure (the flags set at `AnnoWelt 0x1003af8d`) or the History Edition differs.
2. **Step B (released):** the viewer switch for the fisher rule and the hut's stock readout.
3. **Before step C, one clean History Edition run by the owner:** a fixed camera and zoom for the whole run, no
   other screen opened, 120 game seconds from the build, at the closed site and, in a second run, at the open
   control. The UI lane gives the exact settings so the rebuild's viewer can be set to the same view.

## Also in the recording, for the UI lane

The shore-building placement rules of the History Edition (a valid band along the shore, automatic orientation, road
connection points, the range highlight) and the statistics screen, which appears to pause the game.
