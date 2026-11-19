# FISHER-1e, the real-map fisher canary: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/FISHER-1e` `70ecba52` (test source `8b7bccef`), base main `816c6352`. **CI:** green, `gate` included.

## Verdict

**ACCEPTED.** The fisher works on two shipped maps, loaded normally, with nothing suppressed. This closes the proof
OQ-625 asked for, on other maps than `S01_02_01`, as the owner chose. Landing waits for the owner's OK and the
Windows game-file run, which is also this canary's first run on the target host.

## What the canary shows

| Map and hut | First catch (tick) | Raw received | First food | One trip |
|---|---|---|---|---|
| `01_1024_easy_256`, Venice type 3978 at 217, 326 | 6810 | 20660 | 20810 | 39.70 game s |
| the same map, at 212, 314 | 6860 | 20660 | 20850 | 39.62 game s |
| the same map, at 216, 301 | 6910 | 20760 | 20890 | 39.74 game s |
| `02Trade01`, the human type 810 at 243, 216 (one Build command) | 7520 | 22240 | 22290 | 43.10 game s |

## What was checked

- **Nothing suppressed:** the normal loader, `World::create` and `Recorder::start`; only the existing
  `fisher_rules` flag is set; the first map gets no command, the second exactly the one Build.
- **Each stage is asserted from sim state, not from elapsed time:** figure kind 142 on job 45; the open entrance,
  then the departure; a claimed mature fish tile cut from 11 to 10 with the cargo up by 256; three catches; the
  raw stock up by the figure's capacity; the worker back inside the hut; the first conversion using the type's own
  input and output steps (128 to 51 for Venice, 69 to 17 for the human hut).
- **A bound chosen before the run:** 60000 ticks, not changed after the observation.
- **Determinism:** two independent worlds match after every turn (2089 and 2229 turns); a native save taken in the
  middle of a cut continues to the same hashes and the same final bytes.
- **The flag off:** every hash and the saved bytes equal the default world's; workers, raw and food stay zero.
- **No engine change**, no pin moved, no fixture byte changed. FISHER-MAP-1's F1 is fixed (13 empty kernels).

## Limits, stated by the executor and kept

- Venice's hut type and rates on the first map; one human hut on the second.
- Food is the hut's own output; delivery to a warehouse is not part of this proof.
- A hut with every entrance closed is not covered (FISHER-RE-3 stays queued).
- OQ-603 (float and Y parity) and OQ-604 (the original's transitive draws) stay open.
- The macOS run is the only local run so far; the Windows game-file run comes with the landing.
