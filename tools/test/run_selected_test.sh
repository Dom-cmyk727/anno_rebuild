#!/bin/bash
# Contract test of run_selected.sh (round ec): the mandatory preflights gate the selection.
#
#   tools/test/run_selected_test.sh
#
# A fixture CMake project (no compiler) registers two preflight tests, one passing and one failing,
# and a selected test that writes a marker file when it runs. Each check states the outcome it
# expects; the script exits 1 at the first one that differs.
#   - passing preflights: the selection runs (the marker exists), exit 0;
#   - a failing preflight, or one that is not registered: exit 5, the selection never starts;
#   - a test name CTest would split in the sources: exit 5 before any build is attempted;
#   - a selection without a terminal result is not a pass (the verdict reader's contract).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
RUNNER="$HERE/run_selected.sh"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/run_selected_test.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
fail() {
    echo "FAIL: $*" >&2
    exit 1
}
expect() { # expected status, actual, what
    [ "$2" -eq "$1" ] || fail "$3: exit $2, expected $1 (log: $(tail -3 "$WORK/run.log" 2>/dev/null))"
    echo "ok: $3 (exit $2)"
}
mkdir -p "$WORK/project/src" "$WORK/bad/src"
cat > "$WORK/project/CMakeLists.txt" << EOF
cmake_minimum_required(VERSION 3.20)
project(fixture NONE)
enable_testing()
add_test(NAME pre_ok COMMAND \${CMAKE_COMMAND} -E true)
add_test(NAME pre_fails COMMAND \${CMAKE_COMMAND} -E false)
add_test(NAME main_case COMMAND \${CMAKE_COMMAND} -E touch "$WORK/marker")
EOF
echo 'TEST_CASE("a fine name") {}' > "$WORK/project/src/fine_test.cpp"
echo 'TEST_CASE("a name; CTest splits it") {}' > "$WORK/bad/src/split_test.cpp"
cmake -S "$WORK/project" -B "$WORK/build" > "$WORK/configure.log" 2>&1 || fail "fixture configure"
run() { # preflights, sources, target; sets rc
    rm -f "$WORK/marker"
    ANNO_RUN_SELECTED_PREFLIGHTS="$1" ANNO_RUN_SELECTED_SOURCES="$2" \
        bash "$RUNNER" "$WORK/build" "$3" '^main_case$' 60 "$WORK/run.log" 1 > /dev/null 2>&1
    rc=$?
}
run "pre_ok" "$WORK/project" -
expect 0 "$rc" "passing preflights"
[ -f "$WORK/marker" ] || fail "passing preflights: the selection did not run"
grep -q "COUNTS: selected=1 executed=1" "$WORK/run.log" || fail "passing preflights: counts"
run "pre_ok pre_fails" "$WORK/project" -
expect 5 "$rc" "a failing preflight"
[ ! -e "$WORK/marker" ] || fail "a failing preflight: the selection ran"
grep -q "preflight pre_fails failed or is not registered" "$WORK/run.log" || fail "failing: reason"
run "pre_ok pre_missing" "$WORK/project" -
expect 5 "$rc" "a preflight that is not registered"
[ ! -e "$WORK/marker" ] || fail "a missing preflight: the selection ran"
# The source check runs before the build: a target that does not exist is never built.
run "pre_ok" "$WORK/bad" no_such_target
expect 5 "$rc" "a test name with a semicolon in the sources"
[ ! -e "$WORK/marker" ] || fail "a bad name: the selection ran"
grep -q "source test names failed; nothing built" "$WORK/run.log" || fail "bad name: reason"
! grep -q "== build no_such_target" "$WORK/run.log" || fail "bad name: the build was attempted"
# The default preflights are the round's three, by name.
grep -q 'test_names_are_selectable test_verdict_contract picture_matrix_contract' "$RUNNER" ||
    fail "the default preflights changed"
echo "all run_selected contract checks passed"
