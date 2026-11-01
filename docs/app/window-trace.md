# Window trace: the event contract (W4.3 round ec, B1)

An opt-in trace of the play window's real path: from a scripted command to the frame whose present call
returned. It is enabled with `ANNO_VIEW_TRACE=<new directory>` and needs the GPU renderer. It is written
once, at shutdown. This file is the contract; `src/app/include/anno/app/window_trace.hpp` implements it,
and `bench_picture --aggregate` reads its event file.

## Boundaries, kept distinct

1. **Command outcome (sim thread).** For each turn, the runner records the commands it applied, and the
   rejections the sim reported (`SimEventKind::CommandRejected`: the issuer and the reason; no command
   index). A command is never "accepted" because it was submitted.
   - **accepted:** its turn reported no rejection;
   - **rejected:** every command of the turn was rejected;
   - **invalid:** the sim reported more rejections than the turn applied commands. This cannot be a
     recording, and `window_table.py` refuses it;
   - **ambiguous:** some, but not all, of the turn's commands were rejected, so which one is unknown.
   Scripted sessions put one command in a turn, so their outcomes are not ambiguous.
2. **Batch publication (sim thread).** A turn whose building diff is not empty pushes a batch into the
   mailbox (`batch_published`, version = the turn).
3. **Window consumption.** The frame boundary takes the batches (`batch_taken`, version = the turn of each
   batch taken). A collapsed resync carries the newest turn.
4. **Island-version commitment.** The bridge's own steps (`desired` … `committed`, picture_trace.hpp),
   then `generation` g for that commit.
5. **CPU readiness.** For the islands of a commit: `no_visible_change(i, g)` when none of the keys island
   i's change invalidated (in every zoom group) is in the view of the first attempt after the commit; the
   change is offscreen and never waited for. Otherwise the change is visible, and each rendered frame
   observes the change's keys within that frame's own view and zoom group: `chunks_ready(i, g)` the first
   time they are all cached and fresh at one observation.
6. **GPU usability.** `gpu_ready(i, g)` the first time the same frame's keys are all GPU-usable at one
   observation, after the frame's render (`GpuFrame::chunk_state`). It is timed at that frame's
   submission. `drawn(i, g)` at the submission of every frame whose keys of the change are all GPU-usable
   at its own observation (round ed), until the change is presented; the first is `gpu_ready`'s frame.
   A frame whose view holds none of the change's keys draws nothing of it.
7. **Frame submission.** `frame_submitted` (island = the surface epoch, version = the frame id) at the
   return of `GpuFrame::render`, after its queue submit. Every event of that frame carries the same time.
8. **Surface acquire.** In `frames.tsv`: the acquire's start and end, and its status. The statuses are
   those of `wgpuSurfaceGetCurrentTexture`, as webgpu.h and wgpu.h define them: `SuccessOptimal`,
   `SuccessSuboptimal`, `Timeout`, `Outdated`, `Lost`, `Error`, and wgpu-native's `Occluded`.
9. **Present-call result.** In `frames.tsv`: the present call's start and end, and the `WGPUStatus` it
   returned.
   - `frame_presented` (epoch, frame) is recorded when the call returned `WGPUStatus_Success`.
   - `frame_failed` (epoch, frame) is recorded when the acquire gave no usable texture, or the present call
     failed.
   - `presented(i, g)` is recorded by the first frame that both draws the change (`drawn(i, g)` of that
     frame) and returned success from its present call, with that frame's present time
     (`FrameTracer::presented`, round ed). Readiness seen in an earlier frame does not count: a failed
     present completes nothing, and neither does a later successful frame whose view is elsewhere, or
     in which one of the change's keys is stale or not GPU-usable at that frame's observation. The
     change then keeps waiting for a frame that draws it, and is `present_unfinished` if the session
     ends first; nothing is presented to empty the waiting set. One island's frame never completes
     another's, and a later generation of the same island is its own change. When a later commit of
     the same island made the keys stale first, the frame that presents the earlier change draws the
     newer content; the aggregate counts that edit as `present_coalesced`.
   - Round ec (before `drawn`) completed a change at the first successful frame at or after its
     `gpu_ready` frame, whatever that frame drew. Its traces have no `drawn` and the current aggregate
     refuses them (docs/perf/README.md, "Round ed: saved window evidence").

   This is the CPU-side return of the present call. It is not GPU completion, not scanout, and not
   display latency: none of those is observed.

## Identifiers and clocks

- **Session:** the trace directory and its `manifest.txt`. The manifest holds:
  - the session start (wall clock, for people);
  - the command line and the map's name;
  - the number of scripted commands;
  - the adapter and the drawable size;
  - the chunk builder;
  - the record counts, drops and bounds.

  The revision, the tree's state, and the binary, map and command-file hashes are not in it. The runner
  records them in `run.txt` (`tools/bench/window_case.sh`).
- **Times:** `t_ms` in every file is `std::chrono::steady_clock`, in milliseconds since the session
  start, on every thread. Sim turns are identifiers, never times.
- **Turn:** the index of the runner's turn (`batch_published`, `batch_taken` and `sim.tsv`).
- **Island and generation:** the picture's island index, and the generation of the commit.
- **Frame:** the id of each attempted GPU frame, counted from 1, failed attempts included. The commits of a
  frame boundary wait for their keys within the view of the first attempt after them, also when that
  attempt fails: it is the view the frame was going to draw. An attempt is a
  call of the viewer's GPU draw: it acquires a surface texture. A frame the window skips before that (a
  zero-sized drawable, hidden, backing off) is not an attempt and has no row.
- **Surface epoch:** the count of surface (re)configurations. It starts at 1 and changes on a resize, a
  suboptimal surface, or an outdated or lost one.

## Files

- **`events.tsv`:** picture_trace format (`t_ms kind island version`). It holds:
  - the bridge's steps (boundary 4) and `generation`;
  - the readiness events: `chunks_ready`, `no_visible_change`, `gpu_mode` and `gpu_ready`;
  - `batch_published`, `batch_taken`, `frame_submitted`, `drawn`, `frame_presented`, `frame_failed` and
    `presented`;
  - `run_end`, written only when the recording is complete.
- **`frames.tsv`:** one row per attempted GPU frame. Columns, in order:
  - identity: `frame`, `epoch`, `generation`;
  - view: `view_x`, `view_y`, `view_w`, `view_h`, `group`, `scale`, `drawable_w`, `drawable_h`;
  - timing: `t_start_ms`, `picture_ms`, `acquire_start_ms`, `acquire_end_ms`, `acquire_status`, `texture`,
    `submit_ms`, `present_start_ms`, `present_end_ms`, `present_status`;
  - result: `outcome`, which is `presented`, `present_failed`, `acquire_skipped`, `acquire_retry` or
    `fatal`.
  Fields that do not apply to a row are `-1` or `-`.
- **`sim.tsv`:** one row per turn. Columns: `turn`, `t_ms` (the publication), `applied`, `rejected`,
  `outcome` (`accepted`, `rejected`, `ambiguous` or `none`), `reasons`, `batch` (1 when a batch was
  pushed), `deltas`.
- **`manifest.txt`, `summary.txt`:** saved through `app::finalize_evidence`; the event file is saved last.

## Validity, completeness, scenario, target: four separate answers

- **Valid evidence:** the files parse, and the causal rules hold. These are `bench_picture --aggregate`'s
  causal checks, plus the presentation rules:
  - a `presented(i, g)` needs that pair's `gpu_ready` not after it;
  - it needs a `frame_presented` at the same time, whose frame is not below the frame of the `gpu_ready`;
  - that frame has a `drawn(i, g)` at its submission, and no earlier presented frame has one (round ed):
    a deferred completion is accepted, an unrelated frame is not;
  - a `drawn(i, g)` is at a `frame_submitted`'s time, not before that pair's `gpu_ready`, once per frame
    and never after the pair's `presented`; a `gpu_ready` has a `drawn` of its frame;
  - a `batch_taken` of a turn needs its `batch_published` before it;
  - every event of a frame carries one surface epoch, and a later frame never has a lower one;
  - a frame fails at most once, and never before its own `frame_submitted`;
  - in a window trace a `desired` needs a `batch_taken` at or before it, with no frame event between the
    two. The take and the apply are one frame boundary, so an edit is tied to the batch, and through its
    turn to the command, by that boundary. The island version itself does not name a turn.
- **Complete recording:** `run_end` is present. If the bounded buffers overflowed, or a file could not be
  written, there is no `run_end`, and the summary says why.
- **Scenario correctness:** the run's own outcome: commands accepted as scripted, the picture converged,
  no GPU validation error. It is recorded in `summary.txt`; the run status is in `run_end`.
- **Performance target:** never decided by the trace. PLAN §0.3 needs presented frames on target hardware.

## Bounds

The trace holds at most `ANNO_VIEW_TRACE_EVENTS` events (default 2 000 000), `ANNO_VIEW_TRACE_FRAMES`
frame rows (default 200 000) and `ANNO_VIEW_TRACE_TURNS` turn rows (default 200 000), reserved when the
trace is enabled. (The comment at `main.cpp`'s trace setup names only the first two variables; all three are
read.) A record past either bound is counted as dropped and not stored. So is a commit past the bound of the
changes waiting at once (`FrameTracer`, 100 000: a hidden window commits without drawing). A trace with
drops is incomplete.

The trace directory is created at the start, so that a directory that exists or cannot be made refuses the
run before the window opens. Nothing is written into it until the session ends. Recording takes one mutex, shared by the sim thread and the window thread, and copies a small record.

## Exit status and verdicts

`anno1503` exits with one of these statuses:
- 0: the window ended without an error, every trace file was saved and the trace is consistent;
- 5: the trace dropped records, or a file could not be saved;
- 7: the trace contradicts itself;
- 1: a window error;
- 2: refused before the window opened, for bad arguments, `ANNO_VIEW_TRACE` without `--play`, or a trace
  directory that exists or cannot be made.

In the trace:
- `run_end` carries 0 for a successful run;
- it carries 1 for a window error, with `window_error` in `summary.txt`;
- it carries 5 when a file was not saved;
- it carries 7 when the trace contradicts itself (`kEvidenceContradictory`).

A trace with drops has no `run_end`. `bench_picture --aggregate events.tsv` reads it like a bench trace:
exit 0 for a complete, consistent, successful trace, 6 for a failed or incomplete one, and 2 for a
contradictory or malformed one.

## Test hooks of the window (not for players)

- `ANNO_VIEW_SIZE=<w>x<h>`: asks for a drawable of that many pixels. The size the window gets is
  reported on stderr and in the manifest (`drawable`); a screen smaller than the request gives less.
- `ANNO_VIEW_AT=<x>,<y>`: centres the camera on that tile.
- `ANNO_VIEW_QUIT_S=<seconds>`: ends the loop after that time, as a close of the window would.
- `ANNO_VIEW_RESIZE=<seconds>:<w>x<h>`: resizes the window once, that long after the start. The surface is
  reconfigured, which starts a new epoch.

## Behaviour kept (round ec, B3)

With the trace off, the viewer runs the same code path as before:
- one owning scene handle per frame;
- the same idle demand while paused or hidden;
- the same surface recovery: `classify_acquire` keeps the earlier switch, including its retry, back-off
  and give-up counts;
- the same teardown order.

With the trace on:
- the frame waits for nothing new and reads nothing back;
- it does not touch the sim: the turn observer runs on the sim thread after the publication and only
  copies counts;
- the readiness polls run after the present call, so nothing is added between submission and present;
- the records are taken under one mutex into reserved memory, and nothing is written until the session
  ends.

The paired overhead control is in `docs/perf/README.md` (round ec).

## Running and reading it

- `tools/bench/window_case.sh` runs one scripted, traced window with its provenance (`run.txt`) and peak
  RSS (`time.txt`), then aggregates the trace.
- `tools/bench/window_table.py <matrix>` validates every case and tabulates frames, cadence, acquire and
  present calls, edit boundaries and the stderr control lines. Its contract is
  `tools/bench/window_table_test.sh` (CTest `window_table_contract`).
- The round-ec measurements are in `docs/perf/README.md`.
