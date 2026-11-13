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
#                                [--keep-build] [--min-free-gib N]
#   <worktree-path>  the WSL path (/mnt/e/...) of the git worktree to test. Must be a *separate*
#                     checkout from the one driving this script (never a live executor worktree,
#                     never the owner's E:\anno1503): see CLAUDE.md's merge-outside-worktree rule.
#   --tag TAG         distinguishes this run's gate files (default: date +%H%M%S).
#   --build-dir DIR   default: $HOME/anno-build/wsl-gate (one folder, shared across every worktree
#                     and run — WSL disk rules, owner 2026-11-06, below). Resolved with realpath;
#                     refused unless it is $HOME/anno-build itself or a path under it.
#   --gpu-lane        also run anno_render_gpu_tests on lavapipe (ANNO_REQUIRE_GPU=1). Refusing to
#                     run it (no lavapipe ICD installed) fails the gate, since it was requested.
#   --keep-build      skip the end-of-run build-tree deletion, for an immediate rerun of the same
#                     head (configure+build is skipped next time when CMakeCache.txt is still there).
#   --min-free-gib N  the free-space floor checked before configuring (default: 20).
#
# The default build folder is shared: a second invocation against it queues behind a `flock` held
# by the first (bounded by ANNO_GATE_LOCK_TIMEOUT_S, default 1800s) rather than racing its build or
# its end-of-run deletion. Give concurrent runs distinct --build-dir values if that is ever
# genuinely needed.
#
# WSL disk rules (owner, 2026-11-06; docs/orchestration/TASKS.md, W4-OPENING-CONTROLS-FLAKE step 0):
# C: ran out of space once because Ubuntu's disk image lived there and every worktree's gate run
# left its own never-deleted build tree under $HOME/anno-build (47 GB of them). Ubuntu's image now
# lives on E: (`wsl --manage Ubuntu --move`), and this script itself must never recreate that
# problem: one reused build folder (not one per worktree), deleted when the run finishes (unless
# told to keep it), a pinned-dependency cache that lives outside every build tree so deleting a
# build never re-triggers a slow refetch, and a refusal before configuring if the build filesystem
# is nearly out of room. The image is not sparse (WSL disabled sparse images): space freed inside
# it is reused by a later build, not returned to the host, so deleting the build tree keeps the
# image itself lean, not just the host's view of it.
#
# Gate files land under $HOME/anno-gate-logs/, never inside the worktree under test (docs/testing/
# README.md documents the exact path): a worktree removed with `git worktree remove` takes its
# whole tree with it, gitignored files included, and that is exactly how the raw logs of an
# earlier run (`w7final2`) were lost before a reviewer could read them (W4-SETTLEMENT-1 W7 review,
# 2026-11-07). One $S summary file per run plus a .log/.out pair per lane, each line in $S
# timestamped so lane wall-clock time is directly readable (W4-OPENING-CONTROLS-FLAKE step 0's
# "lane times recorded"). A run never overwrites another run's summary (below); give --tag an
# explicit, distinct value for a rerun in the same minute, or let a collision fail loudly.
set -u

WORKTREE=""
TAG="$(date +%H%M%S)"
BUILD_DIR=""
GPU_LANE=0
KEEP_BUILD=0
MIN_FREE_GIB=20
while [ $# -gt 0 ]; do
  case "$1" in
    --tag) TAG=$2; shift 2 ;;
    --build-dir) BUILD_DIR=$2; shift 2 ;;
    --gpu-lane) GPU_LANE=1; shift ;;
    --keep-build) KEEP_BUILD=1; shift ;;
    --min-free-gib) MIN_FREE_GIB=$2; shift 2 ;;
    *) if [ -z "$WORKTREE" ]; then WORKTREE=$1; else echo "unexpected argument: $1" >&2; exit 2; fi; shift ;;
  esac
done
if [ -z "$WORKTREE" ]; then
  echo "usage: $0 <worktree-path> [--tag TAG] [--build-dir DIR] [--gpu-lane] [--keep-build] [--min-free-gib N]" >&2
  exit 2
fi

export PATH=$HOME/anno-tools-venv/bin:$PATH
export ANNO1503_GAME_DIR=/mnt/e/anno1503/game/app
export ANNO_TEST_THREADS=${ANNO_TEST_THREADS:-4}

W=$(realpath "$WORKTREE" 2>/dev/null) || { echo "cannot resolve worktree path $WORKTREE" >&2; exit 2; }

# --build-dir, resolved and required under ~/anno-build (W7 review finding 8): the shared default
# below is always there, but an explicit value used to be taken as-is, trusted through a later
# `cd "$W"` and then handed straight to `rm -rf` in cleanup() — $HOME, the repository itself or any
# other path a caller mistypes would be deleted on exit. `realpath -m` works even when the
# directory does not exist yet (the common case: it is created just below).
if [ -n "$BUILD_DIR" ]; then
  BUILD_DIR=$(realpath -m "$BUILD_DIR" 2>/dev/null) || { echo "cannot resolve --build-dir $BUILD_DIR" >&2; exit 2; }
  case "$BUILD_DIR" in
    "$HOME/anno-build"|"$HOME/anno-build"/*) ;;
    *) echo "refusing --build-dir $BUILD_DIR: must be $HOME/anno-build or a path under it" >&2; exit 2 ;;
  esac
fi
# One reused build folder by default (WSL disk rules above) — never one per worktree. An explicit
# --build-dir still works for a caller that genuinely wants its own (e.g. to keep two runs' builds
# apart deliberately); the default no longer does that implicitly.
B=${BUILD_DIR:-$HOME/anno-build/wsl-gate}
# Gate logs live outside every tested worktree (header comment above): a worktree removal takes
# its gitignored cache/ directory with it, which is exactly how an earlier run's raw logs were
# lost before review. Namespaced by the worktree's own basename so distinct worktrees under test
# do not share a log directory; TAG still distinguishes runs against the same worktree.
G=$HOME/anno-gate-logs/$(basename "$W")
# The pinned FetchContent sources, outside every build tree (WSL disk rules above) so deleting $B
# at the end of a run never forces a slow refetch next time. Seeded once (2026-11-06) from the
# W4-SETTLEMENT-1 `5a5de598` pins into $HOME/anno-build/w4-asan-viewer/_deps, then moved here.
OLD=$HOME/anno-deps
S=$G/wsl-$TAG.txt
mkdir -p "$G" "$B"

# Never silently overwrite another run's summary (W7 review finding 6): the default tag is a
# wall-clock HHMMSS, so two runs started in the same second, or a caller reusing an explicit --tag,
# used to clobber each other's evidence with no warning.
if [ -e "$S" ]; then
  echo "refusing to overwrite existing summary $S; pass a different --tag" >&2
  exit 2
fi

# A lock on the build folder, acquired before the cleanup trap is registered (W7 review finding
# 7): the trap used to run unconditionally on every exit, including the free-space, dirty-tree and
# stale-cache-mismatch refusals below, which happen before any work starts. A second invocation
# against the same (default, shared) $B that hit one of those refusals at once would still run
# `rm -rf "$B"` and destroy another gate's in-progress build. The lock file is a sibling of $B, not
# inside it, so the stale-cache wipe's `rm -rf "$B"` further down never touches the held fd. A
# bounded wait (not a bare failure) lets a second invocation queue behind a running one instead of
# racing it; raise ANNO_GATE_LOCK_TIMEOUT_S for a deliberately long first run sharing the default
# build dir.
exec 9>"$B.lock"
if ! flock -w "${ANNO_GATE_LOCK_TIMEOUT_S:-1800}" 9; then
  echo "VERDICT: INVALID could not lock build dir $B within ${ANNO_GATE_LOCK_TIMEOUT_S:-1800}s (another gate run holds it)" >&2
  exit 1
fi

# Delete the build tree when the run finishes (WSL disk rules above), pass or fail (including the
# free-space and dirty-checkout refusals below), unless the caller asked to keep it for an
# immediate rerun of the same head. Registered only once the lock above is held, so an invocation
# that never acquires the lock never deletes a build dir it does not own.
cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$B"
  fi
}
trap cleanup EXIT

# The free-space floor (WSL disk rules above): refuse before configuring rather than fail
# confusingly mid-build, or worse, mid-build on some *other* concurrent job sharing the same disk.
FREE_GIB=$(df -BG --output=avail "$B" | tail -1 | tr -dc '0-9')
if [ -z "$FREE_GIB" ] || [ "$FREE_GIB" -lt "$MIN_FREE_GIB" ]; then
  echo "VERDICT: INVALID only ${FREE_GIB:-unknown} GiB free on the filesystem under $B, below the ${MIN_FREE_GIB} GiB floor; refusing to configure" >&2
  exit 1
fi

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

# A shared build dir (WSL disk rules above) can be left over from a *different* worktree's run if
# that run was killed or raced before cleanup ran (this is exactly how gatecheck2 silently built
# the live executor worktree instead of the intended clean checkout on 2026-11-06: $B/CMakeCache.txt
# still pointed at the live worktree, so the "if cache missing, configure" check below skipped
# configure entirely and the stale tree's object files got relinked against $W's sources). Detect
# that by comparing the cache's own recorded source directory to $W, not just whether a cache file
# exists at all, and wipe+reconfigure on any mismatch.
if [ -f "$B/CMakeCache.txt" ]; then
  CACHED_HOME=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$B/CMakeCache.txt")
  REQ_HOME=$(realpath "$W" 2>/dev/null || printf '%s' "$W")
  CACHED_HOME_RESOLVED=$(realpath "$CACHED_HOME" 2>/dev/null || printf '%s' "$CACHED_HOME")
  if [ "$CACHED_HOME_RESOLVED" != "$REQ_HOME" ]; then
    echo "stale build dir: $B was configured for source '$CACHED_HOME' but this run tests '$W'; wiping $B and reconfiguring" >> "$S"
    rm -rf "$B"
    mkdir -p "$B"
  fi
fi

# GATE-1a: -O1 -g makes the ASan/UBSan tree 5.5-5.9x faster than unoptimised Debug with the same
# results (docs/orchestration/reviews/2026-11-12-project-analysis.md §2.1 D1). A cached build tree
# from before this change still has the old (empty) CMAKE_CXX_FLAGS_DEBUG, and the "cache exists,
# skip configure" check below would otherwise keep running it unoptimised; detect that the same way
# as the stale-source check above.
WANTED_CXX_FLAGS_DEBUG="-O1 -g"
if [ -f "$B/CMakeCache.txt" ]; then
  CACHED_CXXFLAGS_DEBUG=$(sed -n 's/^CMAKE_CXX_FLAGS_DEBUG:STRING=//p' "$B/CMakeCache.txt")
  if [ "$CACHED_CXXFLAGS_DEBUG" != "$WANTED_CXX_FLAGS_DEBUG" ]; then
    echo "stale build dir: $B was configured with CMAKE_CXX_FLAGS_DEBUG='$CACHED_CXXFLAGS_DEBUG' but this run wants '$WANTED_CXX_FLAGS_DEBUG'; wiping $B and reconfiguring" >> "$S"
    rm -rf "$B"
    mkdir -p "$B"
  fi
fi

if [ ! -f "$B/CMakeCache.txt" ]; then
  cmake -S "$W" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
    -DCMAKE_CXX_FLAGS_DEBUG="$WANTED_CXX_FLAGS_DEBUG" \
    -DSDL_UNIX_CONSOLE_BUILD=ON -DANNO_SANITIZE=ON -DANNO_BUILD_VIEWER=ON -DANNO_BUILD_GPU=ON -DANNO_WARNINGS_AS_ERRORS=ON \
    -DFETCHCONTENT_SOURCE_DIR_DOCTEST="$OLD/doctest-src" -DFETCHCONTENT_SOURCE_DIR_SDL3="$OLD/sdl3-src" \
    -DFETCHCONTENT_SOURCE_DIR_WGPU_NATIVE="$OLD/wgpu_native-src" > "$G/wsl-$TAG-configure.log" 2>&1
  echo "configure rc=$? $(date -Is)" >> "$S"
fi
# Provenance: record the source directory and the C++ Debug flags this build tree is actually
# configured with, read back from CMake's own cache rather than just echoing the inputs, so a
# future run can be audited for the same mismatch this fix (and the stale-source one above it) was
# written to catch.
echo "build dir source (from CMakeCache.txt) $(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$B/CMakeCache.txt")" >> "$S"
echo "build dir CMAKE_CXX_FLAGS_DEBUG (from CMakeCache.txt) $(sed -n 's/^CMAKE_CXX_FLAGS_DEBUG:STRING=//p' "$B/CMakeCache.txt")" >> "$S"
BUILD_START=$(date +%s)
cmake --build "$B" -j 12 > "$G/wsl-$TAG-build.log" 2>&1; RC=$?
echo "build rc=$RC elapsed=$(( $(date +%s) - BUILD_START ))s $(date -Is)" >> "$S"
if [ $RC -ne 0 ]; then echo "VERDICT: INVALID build failed, nothing tested" >> "$S"; cat "$S" >&2; exit 1; fi
sha256sum "$B/src/app/anno1503" "$B/src/app/anno_app_tests" "$B/src/app/anno_app_input_tests" >> "$S"
mkdir -p "$G/wsl-tmp-$TAG"; export TMPDIR="$G/wsl-tmp-$TAG"

# Tracks whether the run as a whole should fail (W7 review finding 5): the script used to end on
# `cat "$S"`, so a run with one or more failing lanes, or a requested lane that never actually
# executed, still exited 0. A reviewer or CI step that only checks the exit code would then read a
# broken gate as green.
OVERALL_RC=0

run_lane() {
  local name=$1 select=$2 jobs=$3; shift 3
  local start=$(date +%s)
  env $SRC_GIT python3 -I tools/test/run_selected.py --source-root "$W" --build-dir "$B" \
    --select "$select" --exclude-label heavy --jobs "$jobs" --timeout 6000 \
    --log "$G/wsl-$TAG-$name.log" "$@" > "$G/wsl-$TAG-$name.out" 2>&1
  local rc=$?
  echo "$name lane rc=$rc elapsed=$(( $(date +%s) - start ))s $(grep -E 'COUNTS|VERDICT' "$G/wsl-$TAG-$name.out" | tr '\n' ' ') $(date -Is)" >> "$S"
  if [ "$rc" -ne 0 ]; then
    OVERALL_RC=1
  fi
}

cd "$W"
# picture_matrix_contract (GATE-1b): run once per gate, as a required contract of the first lane
# only; every later lane passes --skip-auto-contract so it is neither re-run nor re-counted.
run_lane app '^anno_app_tests\.|^anno_app_input_tests\.|^anno_app_alloc_tests\.|^viewer_wiring_(contract|r1_held_publication|r4_occupancy|cpu_delivery_failures|opening_controls|settlement_activity|expand_live_load)$|^opening_session_replay$' 4 \
  --require-test viewer_wiring_contract --require-test viewer_wiring_opening_controls \
  --require-test viewer_wiring_settlement_activity --require-test opening_session_replay
run_lane sim '^anno_sim_tests\.' 4 --skip-auto-contract
run_lane formats '^anno_formats_tests\.|^anno_assets_tests\.' 4 --skip-auto-contract
# anno_render_gpu_tests is deliberately excluded here: see the header comment.
run_lane render '^anno_render_tests\.' 4 --skip-auto-contract
run_lane tsupport '^anno_test_support_tests\.' 4 --skip-auto-contract

if [ "$GPU_LANE" -eq 1 ]; then
  ICD=$(ls /usr/share/vulkan/icd.d/*lvp*.json 2>/dev/null | head -1)
  if [ -z "$ICD" ]; then
    # Requested but unable to run is a failure, not a quiet skip (W7 review finding 5): the caller
    # asked for the GPU lane specifically, so a missing lavapipe ICD must not pass silently.
    echo "gpu lane rc=skip no lavapipe ICD found under /usr/share/vulkan/icd.d (install mesa-vulkan-drivers)" >> "$S"
    OVERALL_RC=1
  else
    start=$(date +%s)
    # --allow-skip gpu (GATE-1c item 2): the two named-adapter play_gpu_test.cpp cases are a
    # classified gpu skip here (lavapipe has no "RTX 4090"/"Radeon" adapter), not a failure;
    # ANNO_REQUIRE_GPU still fails a case that finds no GPU device at all.
    env $SRC_GIT WGPU_BACKEND=vulkan VK_ICD_FILENAMES="$ICD" ANNO_REQUIRE_GPU=1 \
      python3 -I tools/test/run_selected.py --source-root "$W" --build-dir "$B" \
      --select '^anno_render_gpu_tests\.' --exclude-label heavy --jobs 1 --timeout 6000 \
      --allow-skip gpu --skip-auto-contract --log "$G/wsl-$TAG-gpu.log" > "$G/wsl-$TAG-gpu.out" 2>&1
    rc=$?
    echo "gpu lane (lavapipe, ANNO_REQUIRE_GPU=1) rc=$rc elapsed=$(( $(date +%s) - start ))s $(grep -E 'COUNTS|VERDICT' "$G/wsl-$TAG-gpu.out" | tr '\n' ' ') $(date -Is)" >> "$S"
    if [ "$rc" -ne 0 ]; then
      OVERALL_RC=1
    fi
  fi
fi

echo "done $(date -Is)" >> "$S"
echo "overall rc=$OVERALL_RC" >> "$S"
cat "$S"
exit "$OVERALL_RC"
