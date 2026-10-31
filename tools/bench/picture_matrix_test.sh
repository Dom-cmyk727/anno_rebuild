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

# picture_table.py (round ea, 3a/3b): the tables of a matrix whose cases have the evidence named
# by each case; every case keeps its row, and the exit status says whether the evidence was usable.
TABLE="$HERE/picture_table.py"
FRAMES=$'frame\tt_ms\tphase\tpublished\tapply_ms\tapply_list_ms\treindex_ms\tdispatch_ms\tcommit_ms\tcommitted\tupdate_ms\n0\t0.1\tloop\t1\t0.02\t0.01\t0.01\t0.01\t0.00\t0\t0.05\n1\t16.8\tloop\t0\t0\t0\t0\t0\t0.01\t1\t0.04\n'
SUMMARY='stats committed=1 discarded=0 coalesced=0 deferred=0 result_bytes_peak=10 estimate_shortfalls=0 version_requeues=0 rebased=0
memory sampled_peak_incremental_payload=1 exact_peak_incremental_payload=2 sampled_peak_unreferenced_payload=0 sampled_peak_reclaim_queue_bytes=0 exact_peak_reservation_bytes=3 sampled_peak_rss=4 sampled_peak_footprint=5 majflt_delta=0 sys_pageouts_delta=0 sys_swapouts_delta=0
VERIFY OK'
make_case() { # matrix, case, trace file ("" for none), [frames, summary: "none" leaves them out]
    mkdir -p "$1/$2"
    [ -z "$3" ] || cp "$3" "$1/$2/events.tsv"
    [ "${4:-}" = none ] || printf '%s' "$FRAMES" > "$1/$2/frames.tsv"
    [ "${5:-}" = none ] || printf '%s\n' "$SUMMARY" > "$1/$2/summary.txt"
    printf '%s\t0\t1\t-\n' "$2" >> "$1/results.tsv"
}
new_matrix() { # dir
    mkdir -p "$1"
    printf 'case\tstatus\twall_s\tload_after\n' > "$1/results.tsv"
}
table() { # binary, matrix, expected status, what
    python3 "$TABLE" "$1" "$2" > "$WORK/table.stdout" 2> "$WORK/table.stderr"
    expect_status "$3" "$?" "$4"
}
new_matrix "$WORK/m_good"
make_case "$WORK/m_good" good "$WORK/productive.tsv"
make_case "$WORK/m_good" quiet "$WORK/idle.tsv"
table "$BENCH" "$WORK/m_good" 0 "table of complete, successful cases"
grep -q "^| good | 0 | 1 | 1 / 6.0 " "$WORK/table.stdout" || fail "table: the good case's row"
grep -q "^| good | loop | 2 | " "$WORK/table.stdout" || fail "table: the good case's stage row"
for kind in missing malformed cut; do
    new_matrix "$WORK/m_$kind"
    make_case "$WORK/m_$kind" good "$WORK/productive.tsv"
    case $kind in
    missing) make_case "$WORK/m_$kind" bad "" ;;
    malformed) make_case "$WORK/m_$kind" bad "$WORK/bad_number.tsv" ;;
    cut) make_case "$WORK/m_$kind" bad "$WORK/cut.tsv" ;;
    esac
    table "$BENCH" "$WORK/m_$kind" 1 "table with a $kind trace"
    grep -q "^| good | 0 | 1 | " "$WORK/table.stdout" || fail "$kind trace: the good case lost its row"
    grep -q "^| bad | 0 | evidence not usable: " "$WORK/table.stdout" ||
        fail "$kind trace: no diagnostic row"
    grep -q "case bad: evidence not usable" "$WORK/table.stderr" || fail "$kind trace: no stderr line"
done
grep -q "trace incomplete: run=INCOMPLETE" "$WORK/table.stdout" || fail "cut trace: reason not given"
new_matrix "$WORK/m_nosummary"
make_case "$WORK/m_nosummary" bad "$WORK/productive.tsv" "" none
table "$BENCH" "$WORK/m_nosummary" 1 "table with a case without summary.txt"
grep -q "summary.txt unreadable" "$WORK/table.stdout" || fail "missing summary: reason not given"
new_matrix "$WORK/m_noframes"
make_case "$WORK/m_noframes" bad "$WORK/productive.tsv" none
table "$BENCH" "$WORK/m_noframes" 1 "table with a case without frames.tsv"
new_matrix "$WORK/m_failed"
make_case "$WORK/m_failed" bad "$WORK/failed.tsv"
table "$BENCH" "$WORK/m_failed" 6 "table with a complete, failed run"
grep -q "^| bad | 0 (run=complete status=1 (FAILED)) | " "$WORK/table.stdout" ||
    fail "failed run: its row does not say so"
# An aggregate process that fails in an unexpected way (a crash, no output) is not indexed.
CRASH="$WORK/crash_bench"
printf '#!/bin/bash\necho "crashed" >&2\nkill -SEGV $$\n' > "$CRASH"
chmod +x "$CRASH"
table "$CRASH" "$WORK/m_good" 1 "table whose aggregate crashes"
grep -q "aggregate failed (exit -11): crashed" "$WORK/table.stdout" ||
    fail "crashing aggregate: reason not given"
EMPTY="$WORK/empty_bench"
printf '#!/bin/bash\nexit 0\n' > "$EMPTY"
chmod +x "$EMPTY"
table "$EMPTY" "$WORK/m_good" 1 "table whose aggregate prints nothing and exits 0"
mkdir -p "$WORK/m_none"
table "$BENCH" "$WORK/m_none" 2 "table without results.tsv"

# The bench's own finalization on a real case (round ea, 3c), when a game directory is given: a
# directory named frames.tsv in a fresh out dir. The bench exits 5, the trace it leaves records 5,
# and neither the aggregate nor the table counts the case as successful.
if [ -n "${ANNO1503_GAME_DIR:-}" ]; then
    new_matrix "$WORK/m_bench"
    mkdir -p "$WORK/m_bench/case/frames.tsv"
    "$BENCH" "$ANNO1503_GAME_DIR" --scenario idle --duration-s 0.3 --out "$WORK/m_bench/case" \
        > "$WORK/bench.stdout" 2> "$WORK/bench.stderr"
    expect_status 5 "$?" "bench with an unwritable frames.tsv"
    grep -q "evidence not saved: .*frames.tsv" "$WORK/bench.stderr" || fail "bench: file not named"
    grep -q "	run_end	0	5$" "$WORK/m_bench/case/events.tsv" || fail "bench: run_end is not 5"
    aggregate "$WORK/m_bench/case/events.tsv" 6 "aggregate of the bench's incomplete evidence"
    printf 'case\t5\t1\t-\n' >> "$WORK/m_bench/results.tsv"
    table "$BENCH" "$WORK/m_bench" 1 "table of the bench's incomplete evidence"
else
    echo "skip: the bench finalization check needs ANNO1503_GAME_DIR (not set)"
fi
echo "all picture matrix contract checks passed"
