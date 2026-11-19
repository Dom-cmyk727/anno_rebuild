# GATE-MAC-1 and UNIX-WARN-1: the orchestrator's review (2026-11-16)

## GATE-MAC-1 (Mac executor; `task/GATE-MAC-1` `d4f2e592`, CI green, all 12 jobs, run 43940378105)

**What it found** (Apple M5 Max, Metal, wgpu-native 29.0.1.1; each test alone, 30 times):
- The two tests that failed every run failed 30 of 30; the sea-pass test failed 0 of 30 plainly and 1 of 30 with
  the trace on.
- With `ANNO_GPU_TIMESTAMP_TRACE=1` the raw ticks show the cause: **the end tick comes before the begin tick**
  (2,801 of 3,061 readings of the river's sea pass; 118 of 120 of the frame's resample pass; 112 of 120 of the
  sea test's objects pass). The period is 1 ns; equal ticks, spans over 10 s and failed maps were all 0.
- A separate WebGPU triangle program that never touches `GpuFrame` gives reversed pairs 27 of 30 (its first: begin
  429653162488541, end 0). So the fault is below our frame code. Which defect in wgpu's Metal backend or Apple's
  driver it is stays unproved, and the note says so.

**What it changed:**
- `gpu.cpp`: only the trace switch (silent by default). The engine still rejects reversed, equal and over-long
  spans and falls back to the wall clock; no reading is ever turned into a time.
- `gpu_test.cpp`: an allowance limited to a Metal backend whose device name starts with "Apple ". Only there may
  an individual pass or frame reading be missing. Every adapter still needs the mapped timing record when
  timestamps are supported, finite and positive values for every reading present (the old `>= 0` became `> 0`),
  the frame spanning its passes, byte-identical timed and untimed pictures, and no validation error.
- `docs/render/gpu-timestamp-evidence.md`: the measurements and sources.

**Checked on this PC (its NOT_RUN):** `anno_render_gpu_tests` whole on the **RTX 4090 (Vulkan)**: 118 of 118
executed and passed, none skipped. The three changed tests on the **Radeon iGPU (Vulkan)**, `ANNO_REQUIRE_GPU=1`,
trace on: 3 of 3 passed with the strict rules; of 271 readings 263 were valid and 8 had equal ticks, the AMD
driver's known empty first frame, which the tests already allow. So the tightened rules hold on both Windows
adapters.

**Notes, no change needed now:**
- The independent probe's source and logs stay in the Mac's git-ignored cache, so no one else can run it. If the
  question comes back (a wgpu upgrade), the probe should become a small opt-in test.
- The new helper was placed between `device_or_skip`'s doc comment and its function, so that comment now sits above
  the wrong function. Move it at the next touch of the file.
- The trace switch is read with `getenv` on every timestamp read-back. Cheap, but it belongs in the context.
- On Apple Metal the river tier choice runs on the wall-clock fallback. The Mac is a development host only, so
  this is accepted.
- The warning script's missing define, which the Mac also hit, is fixed by UNIX-WARN-1 below.

**Verdict: ACCEPTED.** It lands with the next candidate on the owner's OK. From then on the Mac's full run can say
PASS.

## UNIX-WARN-1 (Windows executor; `task/UNIX-WARN-1` `841af180`)

- `tools/check_unix_warnings.sh` reads the define and its generator script out of `src/sim/CMakeLists.txt` at run
  time and fails with exit 2 when it cannot find them; it generates the oracle file into its own scratch folder
  and passes the define only for `extensions_test.cpp`. No second hand-written copy.
- A negative control: a scratch file with a shadowed variable must fail under the same flags, once per compiler;
  if it compiles clean the script stops with exit 2. Shown working, and shown failing with `-Wshadow` removed.
- The report's run: all four sections clean under GCC 13 and Clang 18, exit 0. `docs/testing/README.md` has a
  section on the script.
- **Note:** the generator script is the first quoted `COMMAND` line in that CMake file; a second quoted custom
  command above it would be picked instead without a message. Tie the match to the `add_custom_command` of the
  oracle at the next touch.

**Verdict: ACCEPTED once CI is green on `841af180`** (running at review time). It lands with the next candidate.
