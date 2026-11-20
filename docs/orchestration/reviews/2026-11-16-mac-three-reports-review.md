# The Mac's three reports: the first gate run, FISHER-RE-4, ANNOSCENE-ORE-1 (2026-11-16)

**Reviewer:** the Claude Code orchestrator.

## 1. GATE-RUN on `eb11f0c6` (FISHER-VIEW-1's landing candidate): accepted as evidence

The first run under the owner's landing rule of 2026-11-16.

- **The tree matches** the candidate's: `055ee30606bbae58c6a50c3f4485c5eae0582774`.
- **Result on the Mac** (M5 Max, 48 GiB, Apple Clang 21, 20 minutes): 2700 selected, 2656 executed, **2 failed**, 42
  skipped (platform 9, opt-in 28, gpu 2, other 3). No memory refusal, no timeout, no rerun. Gate log sha256
  `889751a7...`.
- **The two failures are GPU tests on Metal:**
  - "river flow on the GPU: the cost is measured per tier, GPU timestamps when the device has them...":
    `c.gpu_timestamps == ctx->timestamps` was false against true (`gpu_test.cpp:2385`);
  - "every pass of a frame is timed, and the timed frame is the untimed picture": `pass.has_value()` failed
    (`gpu_test.cpp:2861`).
- **Why this does not hold the landing:** both pass on the Windows PC's RTX 4090 in the same candidate's PC part (472
  executed, 0 failed); the candidate changes no render, sim, formats or assets code; and under the rule the GPU
  tests are the Windows PC's, on the target adapter. macOS is a development host, never a target.
- **A finding, not ignored:** on Metal the device reports timestamps but the timed pass returns none. It is recorded
  for the render lane as a Mac-only gap (GATE-MAC-1, to be written; low priority). Future Mac gate reports keep
  naming these two until it is fixed or the tests state the Metal case.

## 2. FISHER-RE-4, is a released fisher drawn: ACCEPTED

**Head:** `task/FISHER-RE-4` `ee507a8c`. A read; docs only. CI green.

- **The answer:** the GOG original does **not** draw figure 142 after the release. The figure stays in the registry
  and the display pass is still called, but the end selects animation 17, and figure 142 has no sprite set 17: the
  lookup fails and nothing is submitted.
- **Checked in our files:** `FigurenHeader.dat` figure 142 has exactly four sets, with keys 4, 7, 8 and 9. Figure
  108 does have a set 17, as the note says, so this is not a general hide-on-release rule.
- **What it settles:** the History Edition recording and the GOG read agree. The sim keeps the waiting figure
  (FISHER-1f); the viewer draws nothing for it. **The viewer rule:** a released figure-142 fisher in its end wait
  emits no sprite, because its animation has no sprite set; no fallback picture. This closes FISHER-1f's F2.
- The flag bits (`AnnoWelt 0x1003af80`-`0x1003af96`: bit 0 cleared, bits 1 and 9 set) are read by their consumers;
  none of them hides the figure. History Edition binaries: NOT_RUN.

## 3. ANNOSCENE-ORE-1, the ore and treasure types and `INFO` byte 5: ACCEPTED

**Head:** `task/ANNOSCENE-ORE-1` `ea023e18`. A read of files with a reference check. CI green.

- **The ids** (P, our files): ore kind 0 is figure 1661 (EISENERZ, goods 1), 1 is 1561 (GOLD, 2), 2 is 1562
  (SALZSTEIN, 3), 3 is 1563 (EDELSTEINE, 4), 4 has no figure, 5 is 1564 (MAMORSTEIN, 6), 6 is 1560 (EISENERZ, 1);
  1550 and 1551 are the treasure.
- **The objects** (D/P): the island's `OCCUR` `ENTRY2` markers; the loader takes the figure type from bits 14 to 25
  of the payload's first `u32` (`AnnoWelt 0x10095f8b`). **A correction to the co-developer's reading and to our own
  note:** the placement's slot `+0x84` returns the marker's **goods** (1 to 6), not the figure id.
- **`INFO` bytes 4 and 5** (P): on our copy 465 `.scp` files, 429 candidates; byte 4 is 0 in 429 of 429; byte 5
  equals the number of ore markers in **429 of 429**, 1,590 markers, no exception.
- Open, as the packet asked: a marker that gets no ore, the tons of a placed deposit, the treasure's spot scan, and
  which file the co-developer's copy lacks.

## Landing

FISHER-RE-4 and ANNOSCENE-ORE-1 are docs and reference tools. They land with the next candidate, with the owner's OK.
