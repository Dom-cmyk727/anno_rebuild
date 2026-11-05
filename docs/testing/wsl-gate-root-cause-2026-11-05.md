# WSL ASan/UBSan gate: recurring false failures and excessive runtime (2026-11-05)

The W4-MAIN-1 merge gate's local WSL run (`wsl-m1`) took 6h17m, produced two false
failures, and left local hardware unusable for other work the whole time. Root-caused
below; the fix is to stop running the full gate locally and let the existing cloud CI
job do it (`.github/workflows/ci.yml`, `linux-clang-asan`), which does not have any of
these problems.

## 1. The giant "dirty" count and the false "source changed during selection"

Git for Windows ships with `core.autocrlf=true` machine-wide. WSL's git has no such
override (default `false`). Both read/write the *same* `.git` directory and NTFS
worktree (`/mnt/e/...`), which the dual Windows/WSL gate design requires. Windows git
checks every tracked text file out as CRLF; WSL git, with no conversion, compares those
CRLF bytes byte-for-byte against the LF blobs in the index — so ~1000 files show
"modified" under WSL git on a byte-for-byte pristine checkout. This is not new: the same
magnitude (523-545) appears in every prior WSL gate log back to 2026-11-02
(`anno1503-W4-opening-1/cache/gates/wsl-w*.txt`, `anno1503-W4-3/...`).

It was already solved once, in `cache/o1/wsl_gate5.sh` (used for W4-OPENING-1's
close-out), by forcing WSL git's view to match Windows' for the status check:
```
GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=core.autocrlf GIT_CONFIG_VALUE_0=true
```
`wsl_gate_m1.sh`, written fresh for this merge gate, used plain `git status --short |
wc -l` and regressed to the solved problem. Cause: these gate scripts live only in
gitignored `cache/` directories, rewritten per gate instead of coming from one
committed template — a fix in one script's lineage is invisible to the next.

Consequence this run: the ~1000-line noisy baseline buried a real mid-run edit (a CRLF
fixture fix applied to the worktree while the 6-hour script was still reading it,
against the plan to wait). `run_selected.py`'s own integrity guard correctly detected
the tree changed between lanes and failed the render lane
(`VERDICT: FAIL tested source changed during selection`) — the guard worked, but the
noisy baseline made the hazard easy to miss by eye, and the sim/formats lanes' results
can't be trusted either way because the edit's exact timing relative to their runs is
not reconstructable from the logs.

**Fix applied:** none needed beyond not repeating the mid-run edit; see §5.

## 2. The underlying fixture bug this exposed (fixed)

Separately, `src/sim/tests/founding_fixture.hpp`'s body-hash check and
`src/formats/tests/ship_manifest_test.cpp`'s exact-string field compare both choke on a
trailing `\r` (Windows checkout CRLF) that `operator>>`-based readers elsewhere silently
tolerate. Fixed: `.gitattributes` now forces `text eol=lf` on
`fixtures/synthetic/{founding,map2_manifest,sim}/**`, all 29 affected files were
renormalized, and `founding_fixture.hpp` strips a trailing `\r` defensively. This bug is
specific to a Windows-git-managed NTFS checkout read from WSL — a plain Linux clone
(cloud CI) never produces CRLF bytes in the first place, so it could never have
reproduced there.

## 3. Why the gate takes hours: GPU tests silently run on the wrong, slow adapter

`wsl_gate_m1.sh` builds with `-DANNO_BUILD_GPU=ON` and runs the full
`anno_render_gpu_tests` suite without setting `WGPU_BACKEND`, `VK_ICD_FILENAMES`, or
`ANNO_REQUIRE_GPU`. The log confirms the adapter actually used:
```
adapter: llvmpipe (LLVM 19.1.1, 256 bits) (Vulkan)
```
`llvmpipe` is Mesa's always-present CPU software rasterizer — not the RTX 4090, not the
Radeon iGPU, and not `lavapipe` either. `docs/testing/gpu-tests.md` already documents
the correct setup (install `mesa-vulkan-drivers`, then
`WGPU_BACKEND=vulkan`, `VK_ICD_FILENAMES=.../lvp_icd.json`, `ANNO_REQUIRE_GPU=1`), used
by CI's `linux-gcc` job; `wsl_gate_m1.sh` never applied it, so the Vulkan loader fell
back to whatever it found by default.

Measured wall-clock per lane this run: build 3.9 min, app **133.8 min**, sim 29.4 min,
formats/assets 20.5 min, **render 182.6 min**, test_support 6.3 min — render alone is
half the 6h17m total, and its GPU-tagged subset sums to 8316 CPU-seconds across 116
tests. `docs/testing/gpu-tests.md` records the same GPU case sum at **117 seconds** on
CI's properly-configured lavapipe — i.e. roughly 70x slower here, on a machine with a
9800X3D and an RTX 4090. The `app` lane's own outsized time is consistent with the same
cause: viewer/window tests built with `ANNO_BUILD_GPU=ON` also pay the software-render
cost per test.

The two failures that tripped `prerequisite skip outside the declared gate policy`
(`GPU matches the CPU path on one scene[, on the Radeon iGPU]`) are a direct, permanent
symptom, not a flake: both tests require matching a *named* real adapter, and the only
adapter WSL exposes here is `llvmpipe` — no name match is ever possible, so they skip
every single time this configuration runs.

Confirmed not the cause: no `.wslconfig` cap exists; WSL2 sees all 16 logical
processors (matches the host's 8-core/16-thread 9800X3D) and ~15 GB RAM with plenty free
throughout. ASan/UBSan's own ~2-3x instrumentation overhead is normal and not the
anomaly.

**Compounding factor:** the worktree, build dir inputs, and `game/app` all live on the
Windows NTFS volume (`E:\`), read from WSL across the `/mnt/e` 9p bridge — a well-known
slow path for many-small-file access (fixtures, scene corpus files, game assets),
stacking on top of the software-render and ASan overhead.

## 4. Why this keeps happening

The project's own CI already solved both problems correctly and keeps them apart:
`linux-gcc` runs GPU parity tests on a real, named adapter (lavapipe) with
`ANNO_REQUIRE_GPU=1` (missing adapter fails, never silently skips); `linux-clang-asan`
runs ASan/UBSan but does not run GPU-adapter tests at all. `wsl_gate_m1.sh` combined
"full ASan build" with "full GPU-adapter parity suite" in one local tree — a combination
CI deliberately avoids — and skipped the `.gitattributes`-level autocrlf fix already
proven in `wsl_gate5.sh`. Both regressions trace to the same gap: there is no single,
committed gate-script template; each gate is a fresh, gitignored script that has to
rediscover fixes already made by an earlier one.

## 5. Action taken / going forward

- The local `wsl-m2` rerun was stopped; no WSL build/test processes remain running.
- The CRLF fixture fix (§2) is committed and pushed on `task/W4-main-1`; the cloud
  `linux-clang-asan` CI job runs it instead of a local WSL gate — it has no autocrlf
  mismatch (plain Linux checkout) and never runs GPU-adapter tests, so neither failure
  mode in this note can occur there.
- Process rule: never edit a worktree a background gate script is still reading from;
  stop it first or wait for it to finish.
- Open follow-up (not done here): promote one canonical, committed gate script — with
  the `core.autocrlf` override from `wsl_gate5.sh` and the lavapipe/`ANNO_REQUIRE_GPU`
  setup from `gpu-tests.md`/`ci.yml` — out of gitignored `cache/` so future gates stop
  rediscovering both of these from scratch. Local WSL ASan is still fine for narrow,
  single-test debugging; it should not be the vehicle for a full merge gate.
