#!/bin/bash
# run_selected.sh: runs the mandatory preflight, builds one target, runs the preflight tests, then
# the CTest cases matching a regex, and writes one complete log with provenance, per-case output and
# a verdict read per test (tools/test/ctest_verdict.py; docs/testing/README.md).
#
#   tools/test/run_selected.sh <build-dir> <target|-> <ctest-regex> <case-timeout-s> <log> [jobs]
#
# Order (round ec): before anything is built, the source-level test-name check (cmake -P
# cmake/CheckTestNames.cmake: a name CTest splits runs no case and passes). After the build, each
# preflight test must be registered in <build-dir> and pass on its own, by its exact name:
#   test_names_are_selectable   the same name check, as registered
#   test_verdict_contract       the verdict reader's own contract
#   picture_matrix_contract     the evidence pipeline's contracts (runner, aggregate, table)
# A preflight that is missing or fails stops the run before the selection starts (exit 5). The
# preflights run whatever the selection is; a selector that also matches them does not replace them.
# What the preflights execute is built first, whatever <target> is (round ec review of A: the
# evidence-pipeline preflight ran whatever bench_picture was on disk): the targets in
# ANNO_RUN_SELECTED_PREFLIGHT_TARGETS (default bench_picture), each logged with its hash. A
# preflight target that does not build stops the run (exit 5).
#
# Exit status 0 only when every step passed and the verdict reader says PASS: something selected,
# every selected test with a terminal result, nothing failed, nothing empty, no sanitizer report, no
# required asset or capability absent. Platform, opt-in and other skips are listed apart and are not
# executed passes. The verdict is the stricter of CTest's own status and the reader's.
#   2 build failed  3 lock held  5 preflight failed or missing  1 a test failed  4 sanitizer
#   6 required prerequisite skipped  7 empty doctest child  8 nothing selected  9 no terminal result
# A second run on the same build directory is refused while the first holds the lock. The launcher
# should start this script as its own process group (ledger: the PGID), so cancellation can stop the
# whole owned tree with kill -TERM -PGID.
set -u
# Tool directory first on PATH (the Mac profile: cache/mac-tools/venv/bin), if given.
PATH="${ANNO_TOOLS_BIN:+$ANNO_TOOLS_BIN:}$PATH"
BUILD=$1; TARGET=$2; RE=$3; TMO=$4; LOG=$5; JOBS=${6:-1}
HERE=$(cd "$(dirname "$0")/../.." && pwd)
# The source tree the name check reads, and the preflight tests (the contract test,
# tools/test/run_selected_test.sh, points them at a fixture project).
SOURCES="${ANNO_RUN_SELECTED_SOURCES:-$HERE}"
PREFLIGHTS="${ANNO_RUN_SELECTED_PREFLIGHTS:-test_names_are_selectable test_verdict_contract picture_matrix_contract}"
# An empty value builds none (the contract test's fixture project has no targets).
PREFLIGHT_TARGETS="${ANNO_RUN_SELECTED_PREFLIGHT_TARGETS-bench_picture}"
LOCK="$BUILD.run.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
    echo "refused: another run holds $LOCK" >&2
    exit 3
fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
{
    echo "== run $$ pgid=$(ps -o pgid= -p $$ | tr -d ' ') started=$(date '+%Y-%m-%d %H:%M:%S')"
    echo "== revision $(git -C "$HERE" rev-parse HEAD) dirty=$(git -C "$HERE" diff HEAD | shasum | cut -c1-12) untracked=$(git -C "$HERE" ls-files --others --exclude-standard | wc -l | tr -d ' ')"
    echo "== build dir $BUILD target $TARGET; cache: $(grep '^CMAKE_BUILD_TYPE:' "$BUILD/CMakeCache.txt" 2>/dev/null)"
    echo "== selector '$RE' timeout ${TMO}s jobs $JOBS; ANNO1503_GAME_DIR=${ANNO1503_GAME_DIR:-unset} ANNO_TEST_THREADS=${ANNO_TEST_THREADS:-unset}"
    echo "== preflights: source names; $PREFLIGHTS (built first: ${PREFLIGHT_TARGETS:-none})"
} > "$LOG"
# The cheap check first: no build is spent on a tree whose test names cannot be selected.
echo "== preflight source test names ($SOURCES)" >> "$LOG"
if ! cmake -DSOURCE_DIR="$SOURCES" -P "$HERE/cmake/CheckTestNames.cmake" >> "$LOG" 2>&1; then
    echo "VERDICT: FAIL preflight source test names failed; nothing built or selected" >> "$LOG"
    exit 5
fi
if [ "$TARGET" != "-" ]; then
    echo "== build $TARGET started $(date '+%H:%M:%S')" >> "$LOG"
    if ! cmake --build "$BUILD" --target "$TARGET" >> "$LOG" 2>&1; then
        echo "VERDICT: FAIL build failed; no test executed (target $TARGET)" >> "$LOG"
        exit 2
    fi
    echo "== build ok $(date '+%H:%M:%S')" >> "$LOG"
fi
for PT in $PREFLIGHT_TARGETS; do
    echo "== build preflight target $PT started $(date '+%H:%M:%S')" >> "$LOG"
    if ! cmake --build "$BUILD" --target "$PT" >> "$LOG" 2>&1; then
        echo "VERDICT: FAIL preflight target $PT did not build; the selection did not start" >> "$LOG"
        exit 5
    fi
done
# Every test executable and the viewer, whatever was built (round ec integration review: with target
# "-" a binary left by an experiment was run without its identity in the log).
for f in "$BUILD"/src/*/* "$BUILD"/src/*/*/*; do
    case "$(basename "$f")" in
    *_tests | anno1503 | "$TARGET" | bench_picture)
        [ -f "$f" ] && [ -x "$f" ] && echo "== binary $f sha=$(shasum -a 256 "$f" | cut -c1-16) mtime=$(stat -f %Sm "$f")" >> "$LOG"
        ;;
    esac
done
for PRE in $PREFLIGHTS; do
    echo "== preflight $PRE" >> "$LOG"
    if ! ctest --test-dir "$BUILD" -R "^$PRE\$" --no-tests=error --timeout "$TMO" >> "$LOG" 2>&1; then
        echo "VERDICT: FAIL preflight $PRE failed or is not registered; the selection did not start" >> "$LOG"
        exit 5
    fi
done
echo "== selection" >> "$LOG"
ctest --test-dir "$BUILD" -R "$RE" --no-tests=ignore --timeout "$TMO" -j "$JOBS" -V >> "$LOG" 2>&1
RC=$?
echo "== selection ended $(date '+%H:%M:%S') ctest_rc=$RC" >> "$LOG"
python3 -I "$HERE/tools/test/ctest_verdict.py" "$LOG" > "$LOG.verdict" 2>&1
V=$?
cat "$LOG.verdict" >> "$LOG"
rm -f "$LOG.verdict"
# The verdict is the stricter of CTest's own status and the per-test reading.
if [ "$V" -ne 0 ]; then
    exit "$V"
fi
exit "$RC"
