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
- `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json` — the Vulkan loader opens only the lvp ICD (filename can differ by distro; CI picks any `*lvp*.json` in `/usr/share/vulkan/icd.d`)
- `ANNO_REQUIRE_GPU=1` — a missing adapter fails the test instead of skipping (`device_or_skip`)

The test prints `adapter: <name> (...)` to the log, so the chosen adapter name is visible in the job log.

## Adapter branch taken

- A **discrete** adapter (RTX 4090 here): `High` without measuring; the automatic block is not entered.
- An **integrated** adapter (Radeon iGPU here): the measured automatic branch — the legacy shimmer is measured first, then tiers up to the stop gate, and the tier is `choose_river_quality(frame's own river_costs(), gate)`.
- A **software/CPU** adapter (lavapipe in CI; llvmpipe in WSL): `Low` unmeasured, no measurement at all.

## Running locally on a software adapter

There is no lavapipe on this Windows host. To exercise the automatic-tier branch locally,
run the GPU test executable on the integrated Radeon iGPU (adapter 1; adapter 0 is the
discrete RTX 4090 which takes the `High`-unmeasured branch):

```
cmake --preset msvc
cmake --build --preset msvc --target anno_render_gpu_tests
$env:ANNO_REQUIRE_GPU=1; $env:ANNO_GPU_ADAPTER=1; ctest --preset msvc -R "river flow on the GPU" --output-on-failure
```

To run on a true software Vulkan adapter locally, use WSL Ubuntu with Mesa lavapipe and the
same environment variables CI uses:

```
sudo apt-get install -y mesa-vulkan-drivers libvulkan1 vulkan-tools
export WGPU_BACKEND=vulkan
export VK_ICD_FILENAMES="/usr/share/vulkan/icd.d/lvp_icd.json"
export ANNO_REQUIRE_GPU=1
ctest --preset dev -R "river flow on the GPU" --output-on-failure
```

`vulkaninfo --summary` prints the chosen adapter name (lavapipe/llvmpipe) in the log; the
test itself also prints `adapter: <name> (...)`.

## Test time

lavapipe renders on the CPU, so the river cost measurement (a 1024 × 1024 synthetic
river view per tier) is slower than on real hardware. Added test time on linux-gcc
(run 39753758943): test step 75 s → 135 s, GPU case sum 62 s → 117 s, lavapipe
install about 21 s. The job stays well under the 15 minute cap. If a GPU test is too
slow on lavapipe, bound it by a size parameter with a reason comment; do not skip it.

**windows-msvc runs without the GPU cases (2026-10-23).** The hosted Windows runner's
only adapter is Microsoft's WARP software rasterizer, which wgpu-native picks through
DX12, so the cases did not skip there: they ran one at a time (`RESOURCE_LOCK gpu`) at
about 50 s each, 2 557 s of the job's 2 613 s of test time (run 39926764589, 852 tests,
43 of 54 minutes; the sim tests took 19 s). The GPU test executable carries the CTest
label `gpu` (`src/render/CMakeLists.txt`), and the windows-msvc test step runs
`ctest -LE gpu`. What Windows still checks: the MSVC build of `anno_render_gpu` and its
tests, and every other test. What it no longer checks: the GPU parity cases on WARP,
which linux-gcc checks on lavapipe and the owner's 4090 checks locally
(`ctest --preset msvc -L gpu`). The other jobs are unchanged. Reverting is one line in
`ci.yml`.

## What must not happen

- Do not loosen any GPU = CPU tolerance without evidence.
- If a parity test differs on lavapipe, record the test name and output as BLOCKED or QUESTION in the report; do not "fix" the renderer for it.
