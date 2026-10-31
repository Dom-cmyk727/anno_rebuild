#!/bin/bash
# run_selected.sh: builds one target, runs the mandatory preflight, then the CTest cases that match
# a regex, and writes one complete log with provenance and a verdict (tools/test/ctest_verdict.py).
# Adapted from the W4 runner (3d2fb9d: zero-case children), with the checks MAP-2 added.
#
#   tools/test/run_selected.sh <build-dir> <target|-> <ctest-regex> <case-timeout-s> <log> [jobs]
#
# Exit status 0 only when: the build (when a target is given) succeeded; the preflight passed;
# at least one test was selected; every selected test passed; no doctest child executed zero
# cases; the log holds no sanitizer report; no test that needs the original files skipped for want
# of ANNO1503_GAME_DIR. Platform and opt-in skips are listed apart and do not fail the run.
#   2 build failed   3 lock held   5 preflight failed   1 a test failed   4 sanitizer
#   6 required-asset skip   7 empty doctest child   8 nothing selected
# The preflight is test_names_are_selectable: a test name CTest splits runs no case and passes.
set -u
PATH="${ANNO_TOOLS_BIN:+$ANNO_TOOLS_BIN:}$PATH"
BUILD=$1; TARGET=$2; RE=$3; TMO=$4; LOG=$5; JOBS=${6:-1}
HERE=$(cd "$(dirname "$0")/../.." && pwd)
LOCK="$BUILD.run.lock"
if ! mkdir "$LOCK" 2>/dev/null; then echo "refused: another run holds $LOCK" >&2; exit 3; fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
{
    echo "== run $$ started=$(date '+%Y-%m-%d %H:%M:%S')"
    echo "== revision $(git -C "$HERE" rev-parse HEAD) dirty=$(git -C "$HERE" diff HEAD | shasum | cut -c1-12)"
    echo "== build dir $BUILD target $TARGET; $(grep '^CMAKE_BUILD_TYPE:' "$BUILD/CMakeCache.txt" 2>/dev/null)"
    echo "== selector '$RE' timeout ${TMO}s jobs $JOBS; ANNO1503_GAME_DIR=${ANNO1503_GAME_DIR:-unset}"
} > "$LOG"
if [ "$TARGET" != "-" ]; then
    if ! cmake --build "$BUILD" --target "$TARGET" >> "$LOG" 2>&1; then
        echo "VERDICT: FAIL build failed; no test executed (target $TARGET)" >> "$LOG"; exit 2
    fi
fi
echo "== preflight test_names_are_selectable" >> "$LOG"
if ! ctest --test-dir "$BUILD" -R '^test_names_are_selectable$' --no-tests=error --timeout "$TMO" >> "$LOG" 2>&1; then
    echo "VERDICT: FAIL preflight test_names_are_selectable failed" >> "$LOG"; exit 5
fi
echo "== selection" >> "$LOG"
ctest --test-dir "$BUILD" -R "$RE" --no-tests=ignore --timeout "$TMO" -j "$JOBS" -V >> "$LOG" 2>&1
RC=$?
python3 -I "$HERE/tools/test/ctest_verdict.py" "$LOG" | tee -a "$LOG"
V=${PIPESTATUS[0]}
# The verdict is the stricter of CTest's own status and the log reading.
if [ "$V" -ne 0 ]; then exit "$V"; fi
exit "$RC"
