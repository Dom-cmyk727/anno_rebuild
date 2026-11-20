# FISHER-1f, the sim follows the read for the closed hut: review (2026-11-16)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/FISHER-1f` `498785e8`, on `task/FISHER-RE-3` `5901b846`. **CI:** green, `gate` included.

## Verdict

**ACCEPTED as the sim's slice of the GOG read.** It is not History Edition parity, and the comparison with the
owner's recording below leaves two findings for the next steps. Landing waits for the owner's OK and the Windows
game-file run.

## What changed, and what was checked

- **Measured first:** on the base the worker spawned at tick 690 and ended at 720 without leaving. That did not
  match the read; the report says so before changing anything.
- **Two corrections only:**
  1. **The paired mode.** Point floods for the fisher use the all-free subcell bitmap (the original passes the cell
     mode plus one, and that mode's routine writes nine free subcells), while the coarse cells and the layer
     admission keep mode 3. So a closed start can leave, and a closed destination is still never admitted.
     Checked in the binary: `AnnoWelt 0x10003b07`–`0x10003b0f` reads the mode and increments it; the jump table at
     `0x10092190` entry 4 is `0x1009215e`, which writes nine bytes of 8.
  2. **The end outside the hut.** The failed home job credits nothing, keeps the cargo with the figure, frees the
     hut's slot at once, waits 7500 ticks in the existing `wait` field, and only then starts the registry expiry.
     An explicit end cancels the wait. A worker inside the footprint completes and credits as before.
- **No new state:** no field, tag, version or default changed; the loader accepts the new valid continuation.
- **The 7500 ticks** are the original's timed rule (15.0 at 0.01 per step, 0.2 steps per tick), named with its
  addresses and listed in the lint exceptions as a duration, not a limit. Accepted.
- **Tests:** the five `closed_home` vectors through the C++ sim; the real site on `02Trade01`; determinism, reloads in
  the middle of a cut and of the wait; the flag off equals the base's trace. The new tests fail on the base.
- **Type 3510 is untouched.** 0 pins moved (partial scope; the full run comes with the landing).

## Against the owner's History Edition recording (FISHER-HE-1 step A)

Times are after the build; the sim's are ticks at 2 ms, assuming the Build command at tick 0.

| Event | Sim | History Edition | |
|---|---|---|---|
| The worker appears | 690 ticks, 1.38 s (spawn) | 1.30 s (first visible at the hut's corner) | agrees |
| First catch ends | 7370, 14.74 s | 14.08 s | **the sim is 0.66 s later** |
| Second catch ends | 13580, 27.16 s | 26.48 s | 0.68 s later |
| Third catch ends, release | 19720, 39.44 s | 38.83 s | 0.61 s later |
| Catch to catch | 12.42 s, 12.28 s | 12.40 s, 12.35 s | agrees |
| A new worker | 170 ticks, 0.34 s after the release | 0.40 s and 0.58 s | agrees |
| Where the trip ends | tile (246, 225) | the third stop; GOG memory: (246, 225) | agrees |
| After the release | the figure stays in place 7500 ticks, then 7500 ticks of expiry | **nothing visible** | open |

## Findings

- **F1, the first leg is about 0.6 s slower than the recording.** Everything after the first catch keeps that
  offset, so the difference is between the spawn and the first fish: 2.16 s in the sim, 1.63 s in the recording.
  The sim walks 450 ticks inside the footprint before it leaves (exit at 1170); in the recording the boat is
  already outside the hut's corner at 1.30 s. This may be the build-time alignment, the launch position or the
  walk on the first leg (OQ-603). It is measured properly in FISHER-HE-1 step C on a clean run, not tuned here.
- **F2, what is visible after the release.** The sim keeps the waiting figure, as the GOG read and the GOG memory
  record say. The History Edition shows nothing. Before the viewer draws fishers, one small read settles whether
  the original hides a released figure: FISHER-RE-4.

## FISHER-RE-4 (released, a read, about 1 hour)

`AnnoWelt 0x1003af80`–`0x1003af92` rewrites the figure's flag word at the end (`and 0xfffe`, `or 0x202`) just before
the release call. Read what those bits mean to the fisher's display pass (`0x100384d0`–`0x10038689`) and to the
registry's draw list: is a released figure drawn during its wait, or not? Give addresses and the answer for
figure 142; say whether it holds for other released workers. Update `fisher.md` and, if the answer is "not drawn",
state the rule the viewer needs. No engine code.
