#!/bin/bash
# Contract tests of the picture matrix runner and of bench_picture --aggregate (round dz, 1f). No
# game files: the runner drives a stub bench, and the aggregate reads hand-written traces.
#
#   tools/bench/picture_matrix_test.sh <bench_picture binary>
#
# Each check states the outcome it expects; the script exits 1 at the first one that differs.
set -u
if [ $# -ne 1 ]; then
    echo "usage: $0 <bench_picture binary>" >&2
    exit 2
fi
BENCH=$1
HERE=$(cd "$(dirname "$0")" && pwd)
RUNNER="${PICTURE_MATRIX_RUNNER:-$HERE/picture_matrix.sh}"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/picture_matrix_test.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

fail() {
    echo "FAIL: $*" >&2
    exit 1
}
expect_status() { # expected, actual, what
    [ "$2" -eq "$1" ] || fail "$3: exit $2, expected $1"
    echo "ok: $3 (exit $2)"
}

# A stub bench: records that it ran, then exits with the status its --exit option names.
STUB="$WORK/stub_bench"
cat > "$STUB" << 'EOF'
#!/bin/bash
out=""
code=0
while [ $# -gt 0 ]; do
    case "$1" in
    --out) out=$2; shift 2 ;;
    --exit) code=$2; shift 2 ;;
    *) shift ;;
    esac
done
echo ran > "$out/ran"
exit "$code"
EOF
chmod +x "$STUB"

run_matrix() { # cases text, out dir name; sets rc
    printf '%b' "$1" > "$WORK/cases"
    bash "$RUNNER" "$STUB" /no/game "$WORK/$2" "$WORK/cases" > "$WORK/$2.stdout" 2> "$WORK/$2.stderr"
    rc=$?
}

# An empty and a comment-only cases file select nothing: refused, nothing runs.
run_matrix "" empty
expect_status 2 "$rc" "empty cases file"
grep -q "selects no case" "$WORK/empty.stderr" || fail "empty cases file: no reason given"
run_matrix "# only a comment\n\n   \n" comments
expect_status 2 "$rc" "comment-only cases file"
[ ! -e "$WORK/comments/provenance.txt" ] || fail "comment-only cases file: the run started"

# A final case without a newline runs.
run_matrix "first --exit 0\nlast --exit 0" lastline
expect_status 0 "$rc" "final case without a newline"
[ -f "$WORK/lastline/last/ran" ] || fail "final case without a newline: it did not run"
grep -q "cases=2" "$WORK/lastline/provenance.txt" || fail "final case: the count is not 2"

# A failing case keeps its status and artifacts; the next case does not run.
run_matrix "bad --exit 3\nafter --exit 0\n" failing
expect_status 3 "$rc" "failing case followed by another"
[ -f "$WORK/failing/bad/ran" ] || fail "failing case: its artifacts are gone"
[ ! -e "$WORK/failing/after" ] || fail "failing case: the next case ran"
grep -q "^bad	3	" "$WORK/failing/results.tsv" || fail "failing case: results.tsv lacks its status"

# An out dir that already holds files is refused, and nothing in it is touched.
mkdir -p "$WORK/used"
echo earlier > "$WORK/used/keep.txt"
run_matrix "one --exit 0\n" used
expect_status 2 "$rc" "out dir with an earlier run"
[ "$(cat "$WORK/used/keep.txt")" = earlier ] || fail "out dir with an earlier run: overwritten"
[ ! -e "$WORK/used/one" ] || fail "out dir with an earlier run: a case ran"

# bench_picture --aggregate: a missing, an empty, a malformed trace (exit 2); a cut trace and a
# failed run (exit 6); a complete idle run and a complete productive run (exit 0).
HEADER=$'t_ms\tkind\tisland\tversion'
aggregate() { # file, expected status, what
    "$BENCH" --aggregate "$1" > "$WORK/agg.stdout" 2> "$WORK/agg.stderr"
    expect_status "$2" "$?" "$3"
}
aggregate "$WORK/missing.tsv" 2 "aggregate of a missing trace"
: > "$WORK/empty.tsv"
aggregate "$WORK/empty.tsv" 2 "aggregate of an empty trace"
printf '%s\n0.5\tdesired\t0\t1x\n' "$HEADER" > "$WORK/bad_number.tsv"
aggregate "$WORK/bad_number.tsv" 2 "aggregate of a trace with a partial number"
grep -q "not a whole number" "$WORK/agg.stderr" || fail "partial number: no reason given"
printf 'kind\tt_ms\tisland\tversion\n' > "$WORK/bad_header.tsv"
aggregate "$WORK/bad_header.tsv" 2 "aggregate of a trace with a wrong header"
printf '%s\n0.5\tdesired\t0\n' "$HEADER" > "$WORK/bad_columns.tsv"
aggregate "$WORK/bad_columns.tsv" 2 "aggregate of a trace with three columns"
printf '%s\n0.5\tdesired\t0\t1\n1.0\tadmitted\t0\t1\n' "$HEADER" > "$WORK/cut.tsv"
aggregate "$WORK/cut.tsv" 6 "aggregate of a trace without run_end"
grep -q "run=INCOMPLETE" "$WORK/agg.stdout" || fail "cut trace: not labelled incomplete"
printf '%s\n0.0\tdrain_start\t0\t0\n1.0\tdrain_end\t0\t0\n1.0\trun_end\t0\t1\n' "$HEADER" \
    > "$WORK/failed.tsv"
aggregate "$WORK/failed.tsv" 6 "aggregate of a failed run"
printf '%s\n0.0\tdrain_start\t0\t0\n1.0\tdrain_end\t0\t0\n1.0\trun_end\t0\t0\n' "$HEADER" \
    > "$WORK/idle.tsv"
aggregate "$WORK/idle.tsv" 0 "aggregate of a complete idle run"
grep -q "edits=0" "$WORK/agg.stdout" || fail "idle run: edits not 0"
printf '%s\n0.0\tdesired\t0\t1\n1.0\tadmitted\t0\t1\n2.0\tstarted\t0\t1\n5.0\tbuilt\t0\t1\n6.0\tcommitted\t0\t1\n6.0\tgeneration\t0\t1\n9.0\tchunks_ready\t0\t1\n9.0\tdrain_start\t0\t0\n9.0\tdrain_end\t0\t0\n9.0\trun_end\t0\t0\n' \
    "$HEADER" > "$WORK/productive.tsv"
aggregate "$WORK/productive.tsv" 0 "aggregate of a complete productive run"
grep -q "object_ready n=1 p50_ms=6.000" "$WORK/agg.stdout" || fail "productive run: object-ready"
echo "all picture matrix contract checks passed"
