#!/bin/bash
# Contract test of window_table.py (round ec, D): a synthetic window case that is complete and
# consistent is tabulated with its known numbers, and each kind of unusable evidence is refused with
# its reason while the other cases keep their rows.
#
#   tools/bench/window_table_test.sh <bench_picture binary>
set -u
BENCH=$1
HERE=$(cd "$(dirname "$0")" && pwd)
TABLE="$HERE/window_table.py"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/window_table_test.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
fail() {
    echo "FAIL: $*" >&2
    exit 1
}
H=$'frame\tepoch\tgeneration\tview_x\tview_y\tview_w\tview_h\tgroup\tscale\tdrawable_w\tdrawable_h\tt_start_ms\tpicture_ms\tacquire_start_ms\tacquire_end_ms\tacquire_status\ttexture\tsubmit_ms\tpresent_start_ms\tpresent_end_ms\tpresent_status\toutcome'
F1=$'1\t1\t1\t0\t0\t8\t8\t2\t1\t8\t8\t3.0\t0.1\t3.2\t3.5\tSuccessOptimal\t1\t4.0\t4.5\t5.0\tSuccess\tpresented'
F2=$'2\t1\t1\t0\t0\t8\t8\t2\t1\t8\t8\t5.5\t0.1\t5.6\t6.0\tTimeout\t0\t-1.000000\t-1.000000\t-1.000000\t-\tacquire_skipped'
make_case() { # matrix, case
    local c="$1/$2"
    mkdir -p "$c/trace"
    printf 't_ms\tkind\tisland\tversion\n0\tgpu_mode\t0\t1\n1\tbatch_published\t0\t3\n2\tbatch_taken\t0\t3\n2\tdesired\t0\t1\n3\tcommitted\t0\t1\n3\tgeneration\t0\t1\n4\tframe_submitted\t1\t1\n4\tchunks_ready\t0\t1\n4\tgpu_ready\t0\t1\n5\tframe_presented\t1\t1\n5\tpresented\t0\t1\n6\tframe_failed\t1\t2\n10\trun_end\t0\t0\n' \
        > "$c/trace/events.tsv"
    printf '%s\n%s\n%s\n' "$H" "$F1" "$F2" > "$c/trace/frames.tsv"
    printf 'turn\tt_ms\tapplied\trejected\toutcome\treasons\tbatch\tdeltas\n3\t1.0\t1\t0\taccepted\t-\t1\t1\n' \
        > "$c/trace/sim.tsv"
    printf 'window_trace 1\nadapter Test adapter\ntrace events=13 frames=2 turns=1 dropped=0 bounds=1/1/1\n' \
        > "$c/trace/manifest.txt"
    printf 'zoom group 2, 8x8 px: 60.0 fps, frame avg 1.0 ms / worst 2.0 ms\nzoom group 2, 8x8 px: 120.0 fps, frame avg 0.5 ms / worst 1.0 ms\n' \
        > "$c/stderr.txt"
    printf '   2147483648  maximum resident set size\n' > "$c/time.txt"
    "$BENCH" --aggregate "$c/trace/events.tsv" > "$c/aggregate.txt" 2> "$c/aggregate.stderr"
    local agg=$?
    printf 'drawable_asked=8x8 trace=on\nwindow_exit=0\naggregate_exit=%s\n' "$agg" > "$c/run.txt"
    printf '%s\t0\t1\t-\n' "$2" >> "$1/results.tsv"
}
new_matrix() {
    mkdir -p "$1"
    printf 'case\tstatus\twall_s\tload_after\n' > "$1/results.tsv"
}
table() { # matrix, expected, what
    python3 "$TABLE" "$1" > "$WORK/out" 2> "$WORK/err"
    local rc=$?
    [ "$rc" -eq "$2" ] || fail "$3: exit $rc, expected $2 ($(tail -2 "$WORK/err"))"
    ! grep -q Traceback "$WORK/err" || fail "$3: a traceback"
    echo "ok: $3 (exit $rc)"
}
new_matrix "$WORK/m_good"
make_case "$WORK/m_good" good
grep -q "^run=complete status=0 (succeeded)" "$WORK/m_good/good/aggregate.txt" ||
    fail "the synthetic case does not aggregate as a success: $(cat "$WORK/m_good/good/aggregate.stderr")"
table "$WORK/m_good" 0 "a complete, consistent window case"
# Known numbers: two attempts, one presented; the edit presented 3 ms after its desired event.
grep -q "^| good | 8x8 | 8x8 | Test adapter | 2 / 1 / 1 | 0 | run=complete status=0 (succeeded) |" \
    "$WORK/out" || fail "identity row"
grep -q "^| good | 1 / 2.50 / (n=1) / (n=1) / 2.50 | 2 / 0.30 / (n=2) / (n=2) / 0.40 | 1 / 0.50 / (n=1) / (n=1) / 0.50 | 1 / 1.70 / (n=1) / (n=1) / 1.70 |" \
    "$WORK/out" || fail "timing row"
grep -q "| 1 / 1 / 0 / 0 | 1 | 0 | 0 | .* | 1 / 3.0 / (n=1) / (n=1) / 3.0 | 0 / 0 / 0 |" "$WORK/out" ||
    fail "edits row"
grep -q "^| good | on | 1 / 120.0 / (n=1) / (n=1) / 120.0 | 1 / 0.50 / (n=1) / (n=1) / 0.50 | 2.00 GiB |" \
    "$WORK/out" || fail "stderr row"
# Each fault in its own matrix, beside a good case that keeps its row.
for bad in gap mismatch count contradictory incomplete notime sim_outcome present_order; do
    new_matrix "$WORK/m_$bad"
    make_case "$WORK/m_$bad" good
    make_case "$WORK/m_$bad" bad
    c="$WORK/m_$bad/bad"
    case $bad in
    gap) sed -i.orig 's/^2\t1\t1\t/3\t1\t1\t/' "$c/trace/frames.tsv" ;;
    mismatch) sed -i.orig 's/\tTimeout\t0\t/\tSuccessOptimal\t0\t/' "$c/trace/frames.tsv" ;;
    count) sed -i.orig 's/acquire_skipped$/acquire_retry/; s/\tTimeout\t/\tOutdated\t/' "$c/trace/frames.tsv" &&
        sed -i.orig '/frame_failed/d' "$c/trace/events.tsv" &&
        "$BENCH" --aggregate "$c/trace/events.tsv" > "$c/aggregate.txt" 2> /dev/null ;;
    contradictory) printf 'drawable_asked=8x8 trace=on\nwindow_exit=0\naggregate_exit=2\n' > "$c/run.txt" &&
        echo "bench_picture: contradiction: presented before its gpu_ready" > "$c/aggregate.stderr" ;;
    incomplete) printf 'drawable_asked=8x8 trace=on\nwindow_exit=5\naggregate_exit=6\n' > "$c/run.txt" &&
        sed -i.orig '/run_end/d' "$c/trace/events.tsv" &&
        "$BENCH" --aggregate "$c/trace/events.tsv" > "$c/aggregate.txt" 2> /dev/null ;;
    notime) rm "$c/time.txt" ;;
    sim_outcome) sed -i.orig 's/\taccepted\t/\trejected\t/' "$c/trace/sim.tsv" ;;
    present_order) sed -i.orig 's/\t4.0\t4.5\t5.0\tSuccess/\t4.0\t5.5\t5.0\tSuccess/' "$c/trace/frames.tsv" ;;
    esac
    expected=1
    table "$WORK/m_$bad" "$expected" "a window case with $bad evidence"
    grep -q "^| good | 8x8 | 8x8 | " "$WORK/out" || fail "$bad: the good case lost its row"
    if [ "$expected" -eq 1 ]; then
        grep -q "^| bad | evidence not usable: " "$WORK/out" || fail "$bad: no diagnostic row"
        grep -q "case bad: evidence not usable" "$WORK/err" || fail "$bad: no stderr line"
    fi
done
python3 "$TABLE" "$WORK/m_gap" 2>&1 | grep -q "frame 3, expected 2" || fail "gap: reason"
python3 "$TABLE" "$WORK/m_count" 2>&1 | grep -q "frames_failed=1, the trace 0" || fail "count: reason"
python3 "$TABLE" "$WORK/m_incomplete" 2>&1 | grep -q "trace incomplete: run=INCOMPLETE" ||
    fail "incomplete: reason"
python3 "$TABLE" "$WORK/m_contradictory" 2>&1 | grep -q "aggregate exit 2: .*before its gpu_ready" ||
    fail "contradictory: reason"
mkdir -p "$WORK/m_none"
table "$WORK/m_none" 2 "a matrix without results.tsv"
echo "all window table contract checks passed"
