#!/usr/bin/env bash
# The canonical WSL (Ubuntu, GCC 13) viewer-enabled ASan+UBSan gate (W4-MAIN-1 review F5 /
# W4-OPENING-CONTROLS-FLAKE step 0). Earlier gates (wsl_gate5.sh, wsl_gate_m1.sh, ...) lived only
# in gitignored cache/ directories, rewritten per gate, so a fix made in one script's lineage was
# invisible to the next (the autocrlf mismatch, independently rediscovered twice; see
# docs/testing/wsl-gate-root-cause-2026-11-05.md). This is the one copy, committed, used by every
# WSL lane from now on.
#
# What it does, in order: records provenance (workflow/source head, dirty count under the same
# Windows-git view the autocrlf override gives WSL, config file hashes), configures+builds an
# ASan/UBSan tree with the viewer and GPU targets compiled in, then runs the app/sim/formats-
# assets/test_support lanes through run_selected.py (which carries its own source-change guard:
# it fails VERDICT: FAIL tested source changed during selection if the tree changes mid-run).
#
# It never runs the GPU-adapter test suite (anno_render_gpu_tests) as part of the ASan lane: CI
# keeps "full ASan build" and "full GPU-adapter parity suite" apart on purpose
# (docs/testing/gpu-tests.md), and WSL's only adapter (llvmpipe) cannot pass the adapter-name
# checks those tests make, so including it here only burns hours for a guaranteed, permanent skip.
# Pass --gpu-lane to additionally run a lavapipe-backed GPU lane with ANNO_REQUIRE_GPU=1, the same
# setup linux-gcc CI uses; omit it (the default) for an ASan-only gate.
#
# Usage: tools/test/wsl_gate.sh <worktree-path> [--tag TAG] [--build-dir DIR] [--gpu-lane]
#   <worktree-path>  the WSL path (/mnt/e/...) of the git worktree to test. Must be a *separate*
#                     checkout from the one driving this script (never a live executor worktree,
#                     never the owner's E:\anno1503): see CLAUDE.md's merge-outside-worktree rule.
#   --tag TAG         distinguishes this run's gate files (default: date +%H%M%S).
#   --build-dir DIR   default: $HOME/anno-build/wsl-gate-<basename of worktree-path>.
#   --gpu-lane        also run anno_render_gpu_tests on lavapipe (ANNO_REQUIRE_GPU=1).
#
# Gate files land in <worktree-path>/cache/gates/ (gitignored), one $S summary file per run plus
# a .log/.out pair per lane, each line in $S timestamped so lane wall-clock time is directly
# readable (W4-OPENING-CONTROLS-FLAKE step 0's "lane times recorded").
set -u

WORKTREE=""
TAG="$(date +%H%M%S)"
BUILD_DIR=""
GPU_LANE=0
while [ $# -gt 0 ]; do
  case "$1" in
    --tag) TAG=$2; shift 2 ;;
    --build-dir) BUILD_DIR=$2; shift 2 ;;
    --gpu-lane) GPU_LANE=1; shift ;;
    *) if [ -z "$WORKTREE" ]; then WORKTREE=$1; else echo "unexpected argument: $1" >&2; exit 2; fi; shift ;;
  esac
done
if [ -z "$WORKTREE" ]; then
  echo "usage: $0 <worktree-path> [--tag TAG] [--build-dir DIR] [--gpu-lane]" >&2
  exit 2
fi

export PATH=$HOME/anno-tools-venv/bin:$PATH
export ANNO1503_GAME_DIR=/mnt/e/anno1503/game/app
export ANNO_TEST_THREADS=${ANNO_TEST_THREADS:-4}

W=$WORKTREE
B=${BUILD_DIR:-$HOME/anno-build/wsl-gate-$(basename "$W")}
G=$W/cache/gates
OLD=$HOME/anno-build/w4-asan-viewer/_deps
S=$G/wsl-$TAG.txt
mkdir -p "$G"

# The worktree's gitdir. For a git worktree, $W/.git is a file ("gitdir: <path>"); that path is
# written by Windows git as a drive-letter path (e.g. "E:/anno1503/.git/worktrees/name"), which
# WSL git cannot resolve (not absolute by its rules, so it is read as relative to CWD and fails
# "not a git repository"). Translate it to the /mnt/<drive> form before using it. Plain
# `git -C "$W" rev-parse --git-dir` fails the same way for the same reason, so it is not used here.
if [ -f "$W/.git" ]; then
  RAW=$(sed -n 's/^gitdir: //p' "$W/.git")
  case "$RAW" in
    [A-Za-z]:/*) DRIVE=$(printf '%s' "${RAW:0:1}" | tr 'A-Z' 'a-z'); GITDIR="/mnt/$DRIVE${RAW:2}" ;;
    /*) GITDIR=$RAW ;;
    *) GITDIR=$W/$RAW ;;
  esac
elif [ -d "$W/.git" ]; then
  GITDIR=$W/.git
else
  echo "cannot find a .git for $W" >&2
  exit 2
fi
SRC_GIT="GIT_DIR=$GITDIR GIT_WORK_TREE=$W"

echo "worktree $W build $B tag $TAG" > "$S"
echo "source head (raw) $(env $SRC_GIT git rev-parse HEAD) dirty (raw, no autocrlf override) $(env $SRC_GIT git status --short | wc -l)" >> "$S"

# The autocrlf override (wsl_gate5.sh, proven against the same bug before): Windows git checks
# tracked text files out as CRLF; WSL git, with no override, diffs those CRLF bytes byte-for-byte
# against the LF blobs in the index, so a byte-for-byte pristine checkout shows ~1000 files
# "modified". GIT_CONFIG_* here makes WSL git's view match Windows git's for this run only; it is
# never written to any .git/config.
export GIT_OPTIONAL_LOCKS=0 GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=core.autocrlf GIT_CONFIG_VALUE_0=true
DIRTY=$(env $SRC_GIT git status --short | wc -l)
echo "source head $(env $SRC_GIT git rev-parse HEAD) dirty (autocrlf=true view) $DIRTY" >> "$S"
if [ "$DIRTY" -ne 0 ]; then
  echo "VERDICT: INVALID autocrlf-overridden checkout is not clean ($DIRTY files); refusing to gate a dirty tree" >> "$S"
  cat "$S" >&2
  exit 1
fi

( cd "$W" && sha256sum tools/test/run_selected.py tools/test/ctest_verdict.py cmake/CheckTestNames.cmake ) >> "$S"
echo "start $(date -Is)" >> "$S"
uptime >> "$S"; free -m | sed -n 2p >> "$S"

if [ ! -f "$B/CMakeCache.txt" ]; then
  cmake -S "$W" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
    -DSDL_UNIX_CONSOLE_BUILD=ON -DANNO_SANITIZE=ON -DANNO_BUILD_VIEWER=ON -DANNO_BUILD_GPU=ON -DANNO_WARNINGS_AS_ERRORS=ON \
    -DFETCHCONTENT_SOURCE_DIR_DOCTEST="$OLD/doctest-src" -DFETCHCONTENT_SOURCE_DIR_SDL3="$OLD/sdl3-src" \
    -DFETCHCONTENT_SOURCE_DIR_WGPU_NATIVE="$OLD/wgpu_native-src" > "$G/wsl-$TAG-configure.log" 2>&1
  echo "configure rc=$? $(date -Is)" >> "$S"
fi
BUILD_START=$(date +%s)
cmake --build "$B" -j 12 > "$G/wsl-$TAG-build.log" 2>&1; RC=$?
echo "build rc=$RC elapsed=$(( $(date +%s) - BUILD_START ))s $(date -Is)" >> "$S"
if [ $RC -ne 0 ]; then echo "VERDICT: INVALID build failed, nothing tested" >> "$S"; cat "$S" >&2; exit 1; fi
sha256sum "$B/src/app/anno1503" "$B/src/app/anno_app_tests" "$B/src/app/anno_app_input_tests" >> "$S"
mkdir -p "$G/wsl-tmp-$TAG"; export TMPDIR="$G/wsl-tmp-$TAG"

run_lane() {
  local name=$1 select=$2 jobs=$3; shift 3
  local start=$(date +%s)
  env $SRC_GIT python3 -I tools/test/run_selected.py --source-root "$W" --build-dir "$B" \
    --select "$select" --exclude-label heavy --jobs "$jobs" --timeout 6000 \
    --log "$G/wsl-$TAG-$name.log" "$@" > "$G/wsl-$TAG-$name.out" 2>&1
  local rc=$?
  echo "$name lane rc=$rc elapsed=$(( $(date +%s) - start ))s $(grep -E 'COUNTS|VERDICT' "$G/wsl-$TAG-$name.out" | tr '\n' ' ') $(date -Is)" >> "$S"
}

cd "$W"
run_lane app '^anno_app_tests\.|^anno_app_input_tests\.|^anno_app_alloc_tests\.|^viewer_wiring_(contract|r1_held_publication|r4_occupancy|cpu_delivery_failures|opening_controls)$|^opening_session_replay$' 4 \
  --require-test viewer_wiring_contract --require-test viewer_wiring_opening_controls --require-test opening_session_replay
run_lane sim '^anno_sim_tests\.' 4
run_lane formats '^anno_formats_tests\.|^anno_assets_tests\.' 4
# anno_render_gpu_tests is deliberately excluded here: see the header comment.
run_lane render '^anno_render_tests\.' 4
run_lane tsupport '^anno_test_support_tests\.' 4

if [ "$GPU_LANE" -eq 1 ]; then
  ICD=$(ls /usr/share/vulkan/icd.d/*lvp*.json 2>/dev/null | head -1)
  if [ -z "$ICD" ]; then
    echo "gpu lane rc=skip no lavapipe ICD found under /usr/share/vulkan/icd.d (install mesa-vulkan-drivers)" >> "$S"
  else
    start=$(date +%s)
    env $SRC_GIT WGPU_BACKEND=vulkan VK_ICD_FILENAMES="$ICD" ANNO_REQUIRE_GPU=1 \
      python3 -I tools/test/run_selected.py --source-root "$W" --build-dir "$B" \
      --select '^anno_render_gpu_tests\.' --exclude-label heavy --jobs 1 --timeout 6000 \
      --log "$G/wsl-$TAG-gpu.log" > "$G/wsl-$TAG-gpu.out" 2>&1
    rc=$?
    echo "gpu lane (lavapipe, ANNO_REQUIRE_GPU=1) rc=$rc elapsed=$(( $(date +%s) - start ))s $(grep -E 'COUNTS|VERDICT' "$G/wsl-$TAG-gpu.out" | tr '\n' ' ') $(date -Is)" >> "$S"
  fi
fi

echo "done $(date -Is)" >> "$S"
cat "$S"
