# Fast zooming on the current `main`: what shows, measured (2026-11-14)

**Requested by the owner** (2026-11-14): "start a test run in enhanced mode on one of our multi island maps; zoom in and
out quickly and multiple times in different sections of the map; pay very close attention to any artifacts and any
delayed rendering of textures and islands." The owner watched the scripted session live and "could really distinctly see
the delayed rendering and artifacts".

**Written by** the Claude Code orchestrator. This is evidence for [`zoom-smoothness.md`](zoom-smoothness.md) and for
ZOOM-SMOOTH-1. It measures the four root causes of §3 on today's build at the owner's window size. It changes no code.

## Setup

- **Build:** `f54e36f0`, the SAVE-SECTIONS-1a landing candidate. Its renderer and app code are `main` `d89d54a6`'s;
  SAVE-SECTIONS-1a changes only the sim stream. MSVC Release.
- **Map:** `--play data/maps/lan5.txt`, the fixed 5-player map: 21 islands, 1431 × 1431 tiles.
- **Display:** `--water enhanced`, continuous zoom with the defaults (1.1 per notch, 120 ms ease).
- **Window:** maximised to the owner's 2560 × 1369, on the RTX 4090 (Vulkan) and the 500 Hz primary monitor.
- **Machine:** otherwise idle. The test run had finished, and no build or test was running.

**The input** is the viewer's own dev-script harness (`ANNO_DEV_SCRIPT`, `docs/app/game-shell.md`). It pushes real SDL
wheel, pan and camera events into the window: 2,811 steps and 62 checkpoints. The script ran five map sections:
- **north**, the player-0 start;
- **middle**, the tundra island;
- **sea**, open water between islands;
- **southeast**, a large island;
- **west**, at the map edge.

Each section ran the same moves:

| Move | What the script does |
|---|---|
| jump | `look X Y`: an instant camera jump into the section, then 3 s of rest |
| osc | 3 × (20 notches out, then 20 in), one notch every 25 ms (40 notches/s) |
| fullout / fullin | 50 notches out to the limit, then 50 in, at 40 notches/s |
| flick | 4 × (8 notches in one wheel event out, then 8 in), 60 ms apart |
| human | 16 notches out, then 16 in, at 8 notches/s (north and sea only) |
| panzoom | 10 × (a pan of 250 px and 1 notch out), then back, 40 ms apart |

Every move is followed by 2 s of rest. The wheel anchor is the window centre. That is the harness's limit: it can't zoom
at an off-centre cursor.

**Three runs of the same script:**

| Run | Capture | What it is used for |
|---|---|---|
| 1 | GDI screen grab, about 15 frames/s | The pictures only. The grab itself stalled presentation: 2,154 frame gaps above 6 ms, against 52 and 100 in runs 2 and 3. Its timing is discarded. |
| 2 | ffmpeg Desktop Duplication (`ddagrab`), hardware-encoded, 9,416 real frames in 127 s (about 74 frames/s), resampled to a 120 fps timeline | The frame-accurate pictures |
| 3 | none | The clean numbers: readiness counters and frame times |

The readiness counters are the viewer's own report, printed every 2 s:
- how many frames drew a visible chunk not built yet, and the most such chunks at once;
- how many chunks dissolved in over a stand-in;
- how many new wave windows were started;
- the worst busy frame.

## Findings

### F1. A camera jump shows open sea, then a patchwork of chunks, for 0.10–0.17 s

The frame 0.02 s after a jump is **all open sea, with no island**. At 0.10 s the island is a **patchwork of chunk
rectangles** with straight edges:
- some opaque;
- some translucent, fading in over the sea;
- some **flat placeholder sea**, which visibly lacks the wave texture of the water around it.

Share of the picture still wrong after the jump (16 px blocks with mean |RGB| > 40 against the settled frame; run 2):

| Jump | until 0.06 s | 0.08 s | 0.10 s | 0.13 s | complete at |
|---|---|---|---|---|---|
| north | 70 % | 60 % | 18 % | 6 % | 0.17 s |
| middle | 98 % (still 94 % at 0.04 s) | 5 % | 0 % | 0 % | 0.10 s |
| southeast | 56–60 % | 45 % | 17 % | 0 % | 0.13 s |

The sea and west jumps land on water, so there was nothing to measure. Counters (run 3): the jumps drew 10–60
frames with unbuilt chunks, up to 24 at once, and 83–113 dissolves each. Evidence: `run2/shots/nj1.png` (0.02 s) and
`nj2.png` (0.10 s).

### F2. Zooming out, the islands the zoom-out reveals arrive late: missing, then cut, then translucent

The islands already on screen stay intact. Islands that enter the view at its edges during a zoom-out appear over about
**0.4 s**. On the north full zoom-out (run 2, 0.13 s steps, `run2/shots/sheet_nfo.png`), the eastern island goes
through these stages:
- **0.64 s:** a sliver only;
- **0.76 s:** the left half solid, the right half pale, **cut by a straight vertical line**, with only sea beyond it;
- **0.89 s:** almost complete, its right side still translucent;
- **1.02 s:** complete.

At 2× (`nfo2_east_x2.png`, 0.75 s), the same island is cut vertically at x ≈ 2404 px. Its northern lobes above a
straight horizontal chunk seam are **pale, desaturated and see-through**, and lack the beach rim that the opaque part
below has. A small neighbouring island is cut at the same vertical line.

Counters, in the report windows covering each full zoom-out (run 3):
- 14–47 % of frames drew unbuilt chunks: north 254/1900, middle 350/743, sea 335/1792, southeast 408/1798, west
  276/791;
- **up to 38–58 chunks missing at once**;
- 342–501 dissolves per zoom-out.

### F3. Translucent copies of islands while zooming out

Run 1 frame 294 (0.4 s into the north zoom-out) shows two artifacts:
- **A pale offset copy:** part of the lower-right island is drawn as a pale copy, offset from the island itself, with the
  real island visible through it.
- **A cut island:** at the right edge, only a vertical strip of the eastern island is drawn.

Both match RC2: a stand-in of another level fading out while the new chunk fades in. The same view 0.4 s later
(frames 298 and 306) is clean.

### F4. Zooming in fast also draws unbuilt chunks

At 40 notches/s to the closest zoom (run 3), the most chunks missing at once were:
- **77** at north;
- **74** at west;
- **66** at southeast;
- 40 at middle and 33 at sea.

On the video the zoom-in pictures look complete at the sheet's scale. At close zoom a missing chunk shows as a
stand-in: the same place drawn from a coarser level, so blurrier rather than empty. These counts are the viewer's.

### F5. Even the slow, human rate draws unbuilt chunks on the first visit

At 8 notches/s, north drew 52 of 2995 frames with unbuilt chunks, up to 29 at once, and 79 dissolves. Sea, on its
second visit to those levels, drew 6 frames. **Every first visit to a zoom level shows late content,** however slowly
you zoom.

### F6. Fast back-and-forth: the first pass is the worst

In the oscillation (3 × 20 notches out and in), north drew 444 of 1904 frames with unbuilt chunks (23 %), up to 32 at
once, and 592 dissolves. The later sections drew fewer (middle 160/2970, sea 26/1976), because they reuse what is
cached. Southeast still drew 383/2961, as it was further from what was built.

### F7. Single slow frames on the window thread, at maximum zoom-out

At 500 Hz every frame above 2 ms drops at least one refresh. Without capture (run 3):

| Frame | Spent on |
|---|---|
| 15.4 ms | recording commands 12.8 ms |
| 12.6 ms | recording 10.8 ms |
| 14.4 ms | recording 13.7 ms |
| 18.2 ms | `sync drops` 14.9 ms (chunks dropped on the window thread) |

They all come at or right after maximum zoom-out (zoom group 4). Run 2 adds `finish` 9.1–9.4 ms and `chunks` 17.0 ms
frames at the same moments. This is RC4.

No gap between frames reached 30 ms in runs 2 or 3. Single waits for the display of 10–19 ms appeared 2 times without
capture, and more often with the video capture, so they are mostly the capture's.

### F8. When the input stops, the picture is final within 0.1–0.26 s; the late changes happen during motion

After each move stopped, every frame was compared with the frame 1.85 s later, the last one before the next input
(run 2, 120 fps).
- **At wide zoom:** the picture was final 0.04–0.26 s after the input stopped, the 120 ms ease included.
- **At close zoom:** parts kept changing for up to 1.7 s, but the difference maps (`nfin_diff.png`,
  `nfin_shore_pair.png`) show only the enhanced water's swash running up and down the beach. The shore region changes
  steadily, about 0.4 per frame, with no step. That is animation, not late content.

So the owner's "delayed rendering" is in the motion and right after a jump, not after the camera stops.

### Not seen in this session

- **Flicks:** 8 notches in one event, at levels already built. They drew 0 unbuilt frames.
- **Panning while zooming:** 250 px steps inside the built area drew 0–37 unbuilt frames, up to 4 at once. A long pan
  into unbuilt areas was not tested.
- **Wave-window switches** (RC3): the counter rose (10 new windows in each human-rate move). The pictures here don't
  isolate their look.

## What this means for ZOOM-SMOOTH-1

The owner's acceptance rule (2026-11-13) is no late islands and no dissolves while zooming: the viewer's non-ready and
dissolve counters at 0. Today, against that rule:
- a jump shows nothing for about 60–80 ms, and is complete only after 0.10–0.17 s;
- a fast zoom-out misses up to 58 chunks at once;
- a fast zoom-in misses up to 77;
- even a slow first-visit zoom misses up to 29.

The numbers match RC1 and RC2:
- the prefetch does not cover the area a zoom-out reveals;
- missing chunks arrive by build, upload and a 150 ms dissolve;
- stand-ins and placeholders are visible.

F7's record, finish and drop spikes are RC4. The script and harness here can serve as the packet's repeatable owner-like
check, beside `bench_frame --zoom-sweep`.

## Evidence (outside git: rendered frames of the original's art are not published)

`E:\anno1503-worktrees\zoom-session-2026-11-14\`:
- **`zoom.script`:** the session.
- **`tools/`:** the generator, the drivers (GDI and ddagrab), the analyses and the frame extractor.
- **`run1/`:** `frames/*.jpg` (1,954 frames), `stderr.log`, `frames.csv`.
- **`run2/`:**
  - `capture.mkv` (1.5 GB);
  - `stderr.log`, `phases.csv`, `settle.csv`, `analysis.txt`;
  - `shots/`: `nj1`, `nj2`, `nfo2_east_x2`, `sheet_nfo`, `sheet_nosc`, `nfin_*`.
- **`run3/`:** `stderr.log`, `phases.csv`, `analysis.txt` (the clean numbers).

**Video sync:** video time = driver time − 3.525 s. It is fixed on the first jump's cut, and the checkpoints agree within 1 ms.
