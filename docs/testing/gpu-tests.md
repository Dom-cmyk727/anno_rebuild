# Running the GPU tests on a software (lavapipe) adapter

CI's `linux-gcc` job has no physical GPU, so it runs the GPU parity tests of
`src/render/tests/gpu_test.cpp` on Mesa's software Vulkan driver, lavapipe
(`.github/workflows/ci.yml`, T13, OQ-089).

## What CI installs (linux-gcc only)

- `mesa-vulkan-drivers`, `libvulkan1`, `vulkan-tools` (the last for `vulkaninfo --summary` in the log)

## How the adapter is chosen

wgpu-native is pointed at lavapipe with three environment variables, set for that
job's test step:

- `WGPU_BACKEND=vulkan` — request the Vulkan backend (`src/render/src/wgpu.cpp`, `backend_from_env`)
- `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json` — the Vulkan loader opens only the lvp ICD
- `ANNO_REQUIRE_GPU=1` — a missing adapter fails the test instead of skipping (`device_or_skip`)

The test prints `adapter: <name> (...)` to the log, so the chosen adapter name is visible in the job log.

## Running locally on a software adapter

There is no lavapipe on this Windows host. To exercise the same code path (an
integrated/software adapter, i.e. the automatic-tier branch) locally, run the GPU
test executable on the Radeon iGPU:

```
cmake --preset msvc
cmake --build --preset msvc --target anno_render_gpu_tests
$env:ANNO_REQUIRE_GPU=1; $env:ANNO_GPU_ADAPTER=1; ctest --preset msvc -R "river flow on the GPU" --output-on-failure
```

`ANNO_GPU_ADAPTER=1` selects the second adapter (the iGPU on this PC; the RTX 4090 is
adapter 0 and takes the discrete-adapter branch, which skips the automatic-tier
logic). On a discrete adapter the automatic block is not entered (`High` without
measuring), so the iGPU run is the one that exercises the deterministic choice.

## Test time

lavapipe renders on the CPU, so the river cost measurement (a 1024 × 1024 synthetic
river view per tier) is slower than on real hardware. If a GPU test is too slow on
lavapipe to keep the `linux-gcc` job under 15 minutes, bound it by a size parameter
with a reason comment; do not skip it. Report the added test time in the task report.

## What must not happen

- Do not loosen any GPU = CPU tolerance without evidence.
- If a parity test differs on lavapipe, record the test name and output as BLOCKED or QUESTION in the report; do not "fix" the renderer for it.
