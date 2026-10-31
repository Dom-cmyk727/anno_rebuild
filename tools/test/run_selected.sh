#!/bin/bash
# run_selected.sh: builds one target, then runs the CTest cases matching a regex, and writes one
# complete log with provenance, per-case START/END and a verdict line (docs/testing/README.md).
#
#   tools/test/run_selected.sh <build-dir> <target|-> <ctest-regex> <case-timeout-s> <log> [jobs]
#
# Exit status: 0 only if the build (when a target is given) succeeded, at least one case was
# selected, and every selected case passed (CTest's own status: failed, crashed and timed-out
# children are nonzero; zero selected cases are an error). Cases that print "skipping:" (game files
# not set) pass in CTest and are counted separately in the verdict; they are not executed passes.
# Sanitizer reports are counted in the verdict; a count of zero does not change the exit status.
# A doctest child that ran no test case is counted (vacuous_cases) and fails the run (exit 7).
# A second run on the same build directory is refused while the first holds the lock. The launcher
# should start this script as its own process group (ledger: the PGID), so cancellation can stop the
# whole owned tree with kill -TERM -PGID.
set -u
# Tool directory first on PATH (the Mac profile: cache/mac-tools/venv/bin), if given.
PATH="${ANNO_TOOLS_BIN:+$ANNO_TOOLS_BIN:}$PATH"
BUILD=$1; TARGET=$2; RE=$3; TMO=$4; LOG=$5; JOBS=${6:-1}
HERE=$(cd "$(dirname "$0")/../.." && pwd)
LOCK="$BUILD.run.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
    echo "refused: another run holds $LOCK" >&2
    exit 3
fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
{
    echo "== run $$ pgid=$(ps -o pgid= -p $$ | tr -d ' ') started=$(date '+%Y-%m-%d %H:%M:%S')"
    echo "== revision $(git -C "$HERE" rev-parse HEAD) dirty=$(git -C "$HERE" diff HEAD | shasum | cut -c1-12)"
    echo "== build dir $BUILD target $TARGET; cache: $(grep '^CMAKE_BUILD_TYPE:' "$BUILD/CMakeCache.txt" 2>/dev/null)"
    echo "== selector '$RE' timeout ${TMO}s jobs $JOBS; ANNO1503_GAME_DIR=${ANNO1503_GAME_DIR:-unset} ANNO_TEST_THREADS=${ANNO_TEST_THREADS:-unset}"
} > "$LOG"
if [ "$TARGET" != "-" ]; then
    echo "== build $TARGET started $(date '+%H:%M:%S')" >> "$LOG"
    if ! cmake --build "$BUILD" --target "$TARGET" >> "$LOG" 2>&1; then
        echo "VERDICT: BUILD_FAILED no case executed (target $TARGET)" >> "$LOG"
        exit 2
    fi
    echo "== build ok $(date '+%H:%M:%S')" >> "$LOG"
fi
for f in "$BUILD"/src/*/"${TARGET}" "$BUILD"/src/*/*/"${TARGET}" "$BUILD"/src/*/"$TARGET"; do
    [ -x "$f" ] && echo "== binary $f sha=$(shasum -a 256 "$f" | cut -c1-16) mtime=$(stat -f %Sm "$f")" >> "$LOG"
done
ctest --test-dir "$BUILD" -R "$RE" --no-tests=error --timeout "$TMO" -j "$JOBS" -V >> "$LOG" 2>&1
RC=$?
SELECTED=$(grep -c '^ *Start *[0-9]*:' "$LOG")
PASSED=$(grep -c 'Test *#[0-9]*:.*Passed' "$LOG")
FAILED=$(grep -c 'Test *#[0-9]*:.*\(Failed\|Exception\|Timeout\|Not Run\|Interrupt\)' "$LOG")
SKIPPED=$(grep -c 'skipping:' "$LOG")
REQUIRED_SKIPS=$(grep -c 'skipping: ANNO1503_GAME_DIR not set' "$LOG")
SANITIZER=$(grep -c 'AddressSanitizer\|runtime error:' "$LOG")
# A doctest child that ran no test case (a name CTest split, or a filter that matches nothing)
# passes in CTest but executed nothing (round dz).
VACUOUS=$(grep -c '\[doctest\] test cases: 0 |' "$LOG")
# A sanitizer report is a failure whatever the child's exit status says: a sanitizer may print
# after the test function returned, and its exit path is not reliable (round dw, DW1).
if [ "$RC" -eq 0 ] && [ "$SANITIZER" -eq 0 ] && [ "$SELECTED" -gt 0 ]; then VERDICT=PASS; else VERDICT=FAIL; fi
# A case that needs the game files and was skipped (ANNO1503_GAME_DIR unset) is not an executed pass: the
# gate fails rather than reporting success for work it did not do (round dw, DW1).
if [ "$REQUIRED_SKIPS" -gt 0 ]; then VERDICT=FAIL; RC=6; fi
if [ "$RC" -eq 0 ] && [ "$SANITIZER" -gt 0 ]; then RC=4; fi
if [ "$VACUOUS" -gt 0 ]; then VERDICT=FAIL; [ "$RC" -eq 0 ] && RC=7; fi
echo "VERDICT: $VERDICT ctest_rc=$RC selected_starts=$SELECTED passed=$PASSED failed_or_timed_out=$FAILED skip_messages=$SKIPPED required_asset_skips=$REQUIRED_SKIPS sanitizer_lines=$SANITIZER vacuous_cases=$VACUOUS ended=$(date '+%H:%M:%S')" >> "$LOG"
exit "$RC"
