# Apple Metal timestamp evidence — GATE-MAC-1 (2026-11-16)

This is the measured Apple M5 Max / Metal slice, macOS 27.0.1, Apple Clang
21.0.0. Base: `ac02d4a2`; wgpu-native is pinned to **v29.0.1.1** in
`cmake/WgpuNative.cmake`. It uses wgpu-core/hal/types 29.0.1, as declared in
[the release's Cargo.toml](https://github.com/gfx-rs/wgpu-native/blob/v29.0.1.1/Cargo.toml).
No dependency upgrade or timestamp acceptance-rule change is part of this work.

## Reproduction and frequency

Each test ran alone 30 times, CTest `-j 1`, with no other build or test running.
The names below abbreviate the three full names in `gpu_test.cpp` and REPORTS.
All 90 baseline logs remain under `cache/gate-mac-1/baseline/`.

| Test | Baseline passes / failures | With raw trace, unchanged assertions | Corrected test, trace off |
|---|---|---|---|
| River cost per tier | 0 / 30 | 0 / 30 | 30 / 0 |
| Every frame pass / pixel equality | 0 / 30 | 0 / 30 | 30 / 0 |
| Sea pass timing | 30 / 0 | 29 / 1 | 30 / 0 |

The sea test passed in the first full gate and failed in the second. Its
isolation and trace results are separate measurements, not a claim that the
trace is timing-neutral or that its failure rate is fixed.

## Raw readings and independent control

Set `ANNO_GPU_TIMESTAMP_TRACE=1` to print each recorded pass's name, raw
u64 begin/end ticks, period in ns, and `ok`, `equal_ticks`,
`end_before_begin`, `over_10_s` (including the existing 10-second boundary)
or `map_failed`. Failed mapping has no accessible ticks and prints
`unavailable`, with `map_read` or `mapped_range` detail. Frame and
`sea_objects` rows identify the composite spans separately from real passes.
The switch also reports the positive wall-clock river-cost fallback. It is
silent by default, allocates no new saved state and controls no rendering rule.

All measured periods were **1 ns**. In 30 traced runs of each unchanged test:

| Test | Real pass | Valid readings | End before begin |
|---|---|---:|---:|
| River | Sea | 260 | 2801 |
| Frame | Fluid | 90 | 0 |
| Frame | Terrain | 120 | 0 |
| Frame | Under | 120 | 0 |
| Frame | Sea | 120 | 0 |
| Frame | Objects | 119 | 1 |
| Frame | Resample | 2 | 118 |
| Sea | Terrain | 120 | 0 |
| Sea | Under | 120 | 0 |
| Sea | Sea | 118 | 2 |
| Sea | Objects | 8 | 112 |

There were **0 equal-tick, 0 over-limit and 0 mapping failures** in this trace.
The river test also invokes warm-ups, budgeted and automatic quality selection,
so its observations exceed the three repeats of the first four-tier call.
Composite rows: river Frame 260 valid / 2801 reversed; frame Frame 2 / 118,
Sea+Objects 119 / 0; sea Frame 8 / 112, Sea+Objects 8 / 0. Missing component
readings prevent formation of some composite spans; they are not extra draws
or extra GPU passes. Full counts and raw rows: `trace-summary.json` alongside
`trace/`; repetition log hashes are in `repetition-sha256.json`.

A private independent WebGPU probe draws a full-screen triangle, writes query
indices 0/1 through the render-pass descriptor, resolves/copies 16 bytes, waits
and maps. It never constructs or calls `GpuFrame`. **27/30** pairs were
reversed, 3 valid; validation errors **0**, period **1 ns**. Its first pair was
begin `429653162488541`, end `0`. Probe source/logs remain in the ignored
`cache/gate-mac-1/` directory. A preliminary clear-only control had 1 equal
pair and 29 valid pairs; that is a different workload, not the triangle result.
This reproduces the failure below this engine's frame resolve/copy layout.

## Disposition and test contract

Choose packet option **(a)**. Retain `read_timestamps`' rejection of reversed,
equal and implausibly long spans, and its existing wall-clock cost fallback.
The diagnostic code does not turn invalid readings into successful times.

Only a device whose API metadata says **Metal** and whose device name begins
`Apple ` may omit an individual pass or frame timing in these three tests.
The observed vendor id was 0, so the exception does not infer a PCI vendor id.
Other backends, including Vulkan/D3D12 on Windows and Intel/AMD Metal, retain
their required readings. This is a test allowance, not a production device policy.

The tests still require finite positive values for every present reading;
a positive, correctly reported GPU-or-wall river cost and unchanged quality
selection behavior; an available frame timing-record object when queries are
supported; all available frame/pass span comparisons; one-shot sea reading;
no timing when disabled; no GPU validation errors; and byte-identical timed
and untimed pictures. They never interpret missing as zero or negative.

The pinned Metal implementation maps render timestamps to vertex-start and
fragment-end sample indices in
[wgpu-hal 29.0.1 command.rs](https://github.com/gfx-rs/wgpu/blob/v29.0.1/wgpu-hal/src/metal/command.rs#L926-L948).
Upstream [issue 9414](https://github.com/gfx-rs/wgpu/issues/9414) reports another
Metal counter failure on wgpu 29.0.1 (zero values despite advertised support).
That is context, **not proof that our reversed readings have its exact cause**.
The local triangle probe and raw buffer readings support this disposition;
the exact Apple/wgpu implementation defect remains outside this packet.

Windows adapters are NOT_RUN locally; the orchestrator checks them at review.
No state number, section version, pin, simulation or pixel rule changes.
