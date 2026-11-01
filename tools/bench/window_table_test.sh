#!/bin/bash
# Contract test of window_table.py (round ec, D; round ed, 2): a synthetic window case that is
# complete and consistent is tabulated with its known numbers, and each kind of unusable evidence is
# refused with its reason while the other cases keep their rows. Every trace is aggregated by the
# given bench_picture when the table is made: a saved aggregate.txt or run.txt cannot vouch for a
# trace that was cut or contradicts itself. The inventory (plan.tsv, results.tsv) is checked: an
# empty campaign, a lost, duplicated or unplanned case, and an unfinished campaign are refused; a
# requested subset is labelled as one; an idle case with no command is valid.
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
    printf 't_ms\tkind\tisland\tversion\n0\tgpu_mode\t0\t1\n1\tbatch_published\t0\t3\n2\tbatch_taken\t0\t3\n2\tdesired\t0\t1\n3\tcommitted\t0\t1\n3\tgeneration\t0\t1\n4\tframe_submitted\t1\t1\n4\tchunks_ready\t0\t1\n4\tgpu_ready\t0\t1\n4\tdrawn\t0\t1\n5\tframe_presented\t1\t1\n5\tpresented\t0\t1\n6\tframe_failed\t1\t2\n10\trun_end\t0\t0\n' \
        > "$c/trace/events.tsv"
    printf '%s\n%s\n%s\n' "$H" "$F1" "$F2" > "$c/trace/frames.tsv"
    printf 'turn\tt_ms\tapplied\trejected\toutcome\treasons\tbatch\tdeltas\n3\t1.0\t1\t0\taccepted\t-\t1\t1\n' \
        > "$c/trace/sim.tsv"
    printf 'window_trace 1\nadapter Test adapter\ntrace events=14 frames=2 turns=1 dropped=0 bounds=1/1/1\n' \
        > "$c/trace/manifest.txt"
    printf 'zoom group 2, 8x8 px: 60.0 fps, frame avg 1.0 ms / worst 2.0 ms\nzoom group 2, 8x8 px: 120.0 fps, frame avg 0.5 ms / worst 1.0 ms\n' \
        > "$c/stderr.txt"
    printf '   2147483648  maximum resident set size\n' > "$c/time.txt"
    "$BENCH" --aggregate "$c/trace/events.tsv" > "$c/aggregate.txt" 2> "$c/aggregate.stderr"
    local agg=$?
    printf 'commands=x sha256=0 lines=1\ndrawable_asked=8x8 trace=on\nwindow_exit=0\naggregate_exit=%s\n' "$agg" > "$c/run.txt"
    printf '%s\t0\t1\t-\n' "$2" >> "$1/results.tsv"
    printf '%s\t1\ton\n' "$2" >> "$1/plan.tsv"
}
new_matrix() {
    mkdir -p "$1"
    printf 'case\tstatus\twall_s\tload_after\n' > "$1/results.tsv"
    printf 'case\tselected\ttrace\n' > "$1/plan.tsv"
}
table() { # matrix, expected, what
    python3 "$TABLE" "$BENCH" "$1" > "$WORK/out" 2> "$WORK/err"
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
grep -q "^Campaign: the full matrix, 1 of 1 planned cases" "$WORK/out" || fail "campaign line"
bench_sha=$(python3 -c 'import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest())' "$BENCH")
grep -q "^Aggregate: /.*bench_picture sha256 $bench_sha (every trace aggregated now)" "$WORK/out" ||
    fail "aggregate identity"
! grep -q "differs from the current aggregate" "$WORK/out" || fail "a fresh case named as stale"
# Known numbers: two attempts, one presented; the edit presented 3 ms after its desired event.
grep -q "^| good | 8x8 | 8x8 | 1 | Test adapter | 2 / 1 / 1 | 0 | run=complete status=0 (succeeded) |" \
    "$WORK/out" || fail "identity row"
grep -q "^| good | 1 / 2.50 / (n=1) / (n=1) / 2.50 | 2 / 0.30 / (n=2) / (n=2) / 0.40 | 1 / 0.50 / (n=1) / (n=1) / 0.50 | 1 / 1.70 / (n=1) / (n=1) / 1.70 |" \
    "$WORK/out" || fail "timing row"
grep -q "| 1 / 1 / 1 / 0 / 0 | 1 / 1.0 / (n=1) / (n=1) / 1.0 | 1 | 0 | 0 | .* | 1 / 3.0 / (n=1) / (n=1) / 3.0 | 0 / 0 / 0 |" "$WORK/out" ||
    fail "edits row"
grep -q "^| good | on | 1 / 120.0 / (n=1) / (n=1) / 120.0 | 1 / 0.50 / (n=1) / (n=1) / 0.50 | 2.00 GiB |" \
    "$WORK/out" || fail "stderr row"
# GNU time -v's peak RSS line (Linux) is read as well as macOS's.
new_matrix "$WORK/m_gnu"
make_case "$WORK/m_gnu" good
printf '\tMaximum resident set size (kbytes): 1572864\n' > "$WORK/m_gnu/good/time.txt"
table "$WORK/m_gnu" 0 "a case with GNU time's peak RSS"
grep -q "| 1.50 GiB |" "$WORK/out" || fail "GNU time: peak RSS not read"
# Each fault in its own matrix, beside a good case that keeps its row.
# The round-ec review's impossible rows (P2-3), each accepted before: a frame that starts before the
# previous one ended, an attempt without a texture that has submit times, an epoch going back,
# columns that are not numbers, an outcome the viewer never gives for the status, a present status
# that is not one, an acquire that ends before it starts, more rejections than commands, frames.tsv
# and events.tsv disagreeing on a frame, and more commands applied than the file holds.
for bad in gap mismatch count contradictory incomplete notime sim_outcome present_order overlap \
    failed_submit epoch_back junk_cols wrong_skip junk_present neg_acquire over_reject swapped \
    too_many drawable_no_epoch generation_back reasons_count no_drawn status; do
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
    # Round ed: the raw trace changed after the run; its saved aggregate.txt (a success) and run.txt
    # (aggregate_exit=0) are kept. Only the trace decides.
    contradictory) sed -i.orig 's/^5\tpresented\t0\t1$/3.5\tpresented\t0\t1/' "$c/trace/events.tsv" ;;
    incomplete) sed -i.orig '/run_end/d' "$c/trace/events.tsv" ;;
    no_drawn) sed -i.orig '/drawn/d' "$c/trace/events.tsv" ;;
    status) sed -i.orig 's/^bad\t0\t/bad\t5\t/' "$WORK/m_$bad/results.tsv" ;;
    notime) rm "$c/time.txt" ;;
    sim_outcome) sed -i.orig 's/\taccepted\t/\trejected\t/' "$c/trace/sim.tsv" ;;
    present_order) sed -i.orig 's/\t4.0\t4.5\t5.0\tSuccess/\t4.0\t5.5\t5.0\tSuccess/' "$c/trace/frames.tsv" ;;
    overlap) sed -i.orig 's/^2\t1\t1\t0\t0\t8\t8\t2\t1\t8\t8\t5.5\t/2\t1\t1\t0\t0\t8\t8\t2\t1\t8\t8\t4.8\t/' "$c/trace/frames.tsv" ;;
    failed_submit) sed -i.orig 's/\t0\t-1.000000\t-1.000000/\t0\t5.800000\t-1.000000/' "$c/trace/frames.tsv" ;;
    epoch_back) sed -i.orig 's/^1\t1\t1\t/1\t2\t1\t/' "$c/trace/frames.tsv" ;;
    junk_cols) sed -i.orig 's/^1\t1\t1\t0\t0\t/1\t1\tabc\tx\t0\t/' "$c/trace/frames.tsv" ;;
    wrong_skip) sed -i.orig 's/\tTimeout\t/\tOutdated\t/' "$c/trace/frames.tsv" ;;
    junk_present) sed -i.orig 's/\tSuccess\tpresented$/\tbanana\tpresent_failed/' "$c/trace/frames.tsv" ;;
    neg_acquire) sed -i.orig 's/\t5.6\t6.0\tTimeout/\t5.6\t5.2\tTimeout/' "$c/trace/frames.tsv" ;;
    over_reject) sed -i.orig 's/\t1\t0\taccepted\t-\t1\t1$/\t1\t2\tinvalid\tx,y\t1\t1/' "$c/trace/sim.tsv" ;;
    # Both present events moved, so the trace stays consistent and only the cross-check refuses it.
    swapped) sed -i.orig 's/^5\tframe_presented\t1\t1$/4.9\tframe_presented\t1\t1/; s/^5\tpresented\t0\t1$/4.9\tpresented\t0\t1/' "$c/trace/events.tsv" ;;
    too_many) sed -i.orig 's/lines=1/lines=0/' "$c/run.txt" ;;
    drawable_no_epoch) sed -i.orig 's/^2\t1\t1\t0\t0\t8\t8\t2\t1\t8\t8\t/2\t1\t1\t0\t0\t8\t8\t2\t1\t16\t16\t/' "$c/trace/frames.tsv" ;;
    generation_back) sed -i.orig 's/^2\t1\t1\t/2\t1\t0\t/' "$c/trace/frames.tsv" ;;
    reasons_count) printf 'turn\tt_ms\tapplied\trejected\toutcome\treasons\tbatch\tdeltas\n3\t1.0\t1\t0\taccepted\t-\t1\t1\n4\t1.5\t1\t1\trejected\ta,b,c\t0\t0\n' > "$c/trace/sim.tsv" ;;
    esac
    expected=1
    table "$WORK/m_$bad" "$expected" "a window case with $bad evidence"
    grep -q "^| good | 8x8 | 8x8 | " "$WORK/out" || fail "$bad: the good case lost its row"
    if [ "$expected" -eq 1 ]; then
        grep -q "^| bad | evidence not usable: " "$WORK/out" || fail "$bad: no diagnostic row"
        grep -q "case bad: evidence not usable" "$WORK/err" || fail "$bad: no stderr line"
    fi
done
reason() { # matrix, text
    python3 "$TABLE" "$BENCH" "$1" 2>&1 | grep -qF -- "$2"
}
reason "$WORK/m_gap" "frame 3, expected 2" || fail "gap: reason"
for pair in "overlap:starts before the previous one ended" \
    "failed_submit:an attempt without a texture has submit or present times" \
    "epoch_back:epoch 1 after epoch 2" "junk_cols:'abc' is not a whole number" \
    "wrong_skip:outcome acquire_skipped after Outdated" "junk_present:'banana' is not a status" \
    "neg_acquire:the acquire is out of order" "over_reject:2 rejections for 1 commands" \
    "swapped:frame 1: frames.tsv gives frame_presented" "too_many:1 commands applied for 0" \
    "drawable_no_epoch:drawable 16x16 in epoch 1 of 8x8" "generation_back:generation 0 after generation 1" \
    "reasons_count:reasons 'a,b,c' for 1 rejections" \
    "mismatch:outcome acquire_skipped with SuccessOptimal and texture 0" \
    "notime:time.txt unreadable" "sim_outcome:outcome rejected for 1 applied, 0 rejected" \
    "present_order:the frame's boundaries are out of order"; do
    reason "$WORK/m_${pair%%:*}" "${pair#*:}" || fail "${pair%%:*}: reason"
done
reason "$WORK/m_count" "frames_failed=1, the trace 0" || fail "count: reason"
# Round ed, 2A/2C: the saved success does not hide a cut or contradictory trace.
grep -q "^run=complete status=0 (succeeded)" "$WORK/m_incomplete/bad/aggregate.txt" &&
    grep -q "^aggregate_exit=0" "$WORK/m_incomplete/bad/run.txt" ||
    fail "incomplete: the saved success is not there"
reason "$WORK/m_incomplete" "case bad: evidence not usable: trace incomplete: run=INCOMPLETE (no run_end" ||
    fail "incomplete: reason"
reason "$WORK/m_contradictory" "case bad: evidence not usable: trace unreadable: " ||
    fail "contradictory: reason"
reason "$WORK/m_contradictory" "presented of island 0 generation 1 before its gpu_ready" ||
    fail "contradictory: the aggregate's reason"
reason "$WORK/m_no_drawn" "which has no drawn of it" || fail "no_drawn: reason"
reason "$WORK/m_status" "results.tsv status 5, run.txt window_exit 0" || fail "status: reason"
# A trace edited after its run is aggregated as it is now, and the stale saved summary is named.
new_matrix "$WORK/m_stale"
make_case "$WORK/m_stale" good
make_case "$WORK/m_stale" late
sed -i.orig 's/^6\tframe_failed\t1\t2$/6\tframe_failed\t1\t2\n7\tconverged\t0\t0/' \
    "$WORK/m_stale/late/trace/events.tsv"
table "$WORK/m_stale" 0 "a trace whose saved summary is older than its events"
grep -q "differs from the current aggregate (the current one is used): late\.$" "$WORK/out" ||
    fail "stale: the case is not named"
# An idle case: no command, no edit, every frame presented. Zero commands is a valid case.
new_matrix "$WORK/m_idle"
make_case "$WORK/m_idle" idle
c="$WORK/m_idle/idle"
printf 't_ms\tkind\tisland\tversion\n0\tgpu_mode\t0\t1\n4\tframe_submitted\t1\t1\n5\tframe_presented\t1\t1\n6\tframe_failed\t1\t2\n10\trun_end\t0\t0\n' \
    > "$c/trace/events.tsv"
printf 'turn\tt_ms\tapplied\trejected\toutcome\treasons\tbatch\tdeltas\n' > "$c/trace/sim.tsv"
sed -i.orig 's/lines=1/lines=0/' "$c/run.txt"
table "$WORK/m_idle" 0 "an idle case with no command"
grep -q "^| idle | 0 / 0 / 0 / 0 / 0 | n=0 | 0 | 0 | 0 | " "$WORK/out" || fail "idle: edits row"
# The inventory (2D, 2E).
mkdir -p "$WORK/m_none"
table "$WORK/m_none" 2 "a matrix without results.tsv"
new_matrix "$WORK/m_noplan"
make_case "$WORK/m_noplan" good
rm "$WORK/m_noplan/plan.tsv"
table "$WORK/m_noplan" 2 "a matrix without plan.tsv"
new_matrix "$WORK/m_empty"
table "$WORK/m_empty" 2 "a header-only results.tsv and a plan that selects nothing"
grep -q "plan.tsv selects no case" "$WORK/err" || fail "empty: reason"
new_matrix "$WORK/m_zero_of_one"
printf 'one\t1\ton\n' >> "$WORK/m_zero_of_one/plan.tsv"
table "$WORK/m_zero_of_one" 1 "a selected case that never ran (header-only results.tsv)"
grep -q "INCOMPLETE campaign: 0 of 1 selected cases ran; no result for one" "$WORK/err" ||
    fail "zero of one: reason"
new_matrix "$WORK/m_partial"
make_case "$WORK/m_partial" a
printf 'b\t1\ton\n' >> "$WORK/m_partial/plan.tsv"
table "$WORK/m_partial" 1 "a campaign that stopped after its first case"
grep -q "INCOMPLETE campaign: 1 of 2 selected cases ran; no result for b" "$WORK/err" ||
    fail "partial: reason"
new_matrix "$WORK/m_subset"
make_case "$WORK/m_subset" a
printf 'b\t0\ton\n' >> "$WORK/m_subset/plan.tsv"
table "$WORK/m_subset" 0 "a requested subset"
grep -q "^Campaign: a requested subset, 1 of 2 planned cases (a); not the full matrix\.$" \
    "$WORK/out" || fail "subset: campaign line"
new_matrix "$WORK/m_dup"
make_case "$WORK/m_dup" a
printf 'a\t0\t1\t-\n' >> "$WORK/m_dup/results.tsv"
table "$WORK/m_dup" 1 "a case listed twice"
grep -q "results.tsv lists case a twice" "$WORK/err" || fail "dup: reason"
new_matrix "$WORK/m_lost"
make_case "$WORK/m_lost" a
make_case "$WORK/m_lost" b
sed -i.orig '/^b\t/d' "$WORK/m_lost/results.tsv" "$WORK/m_lost/plan.tsv"
table "$WORK/m_lost" 1 "a case directory results.tsv does not list"
grep -q "case directory b is not listed in results.tsv" "$WORK/err" || fail "lost: reason"
new_matrix "$WORK/m_unplanned"
make_case "$WORK/m_unplanned" a
sed -i.orig 's/^a\t1\ton$/a\t0\ton/' "$WORK/m_unplanned/plan.tsv"
printf 'z\t1\ton\n' >> "$WORK/m_unplanned/plan.tsv"
table "$WORK/m_unplanned" 1 "a result for a case the plan does not select"
grep -q "results.tsv lists case a, which plan.tsv does not select" "$WORK/err" ||
    fail "unplanned: reason"
# Round ed review of package 2: the trace mode is the plan's, not run.txt's. A traced case whose
# run.txt claims trace=off (its trace cut and contradictory) is refused, not moved to the controls.
new_matrix "$WORK/m_claims_off"
make_case "$WORK/m_claims_off" good
make_case "$WORK/m_claims_off" bad
sed -i.orig '/run_end/d' "$WORK/m_claims_off/bad/trace/events.tsv"
sed -i.orig 's/trace=on/trace=off/' "$WORK/m_claims_off/bad/run.txt"
table "$WORK/m_claims_off" 1 "a traced case whose run.txt claims trace off"
grep -q "case bad: evidence not usable: run.txt trace=off, plan.tsv trace=on" "$WORK/err" ||
    fail "claims off: reason"
# A trace-off case (planned off) that has a trace directory.
new_matrix "$WORK/m_off_trace"
make_case "$WORK/m_off_trace" good
make_case "$WORK/m_off_trace" off
sed -i.orig 's/^off\t1\ton$/off\t1\toff/' "$WORK/m_off_trace/plan.tsv"
sed -i.orig 's/trace=on/trace=off/' "$WORK/m_off_trace/off/run.txt"
table "$WORK/m_off_trace" 1 "a trace-off case with a trace directory"
grep -q "case off: evidence not usable: a trace-off case with a trace directory" "$WORK/err" ||
    fail "off with trace: reason"
# The control: the same case without its trace directory is a valid trace-off control.
rm -rf "$WORK/m_off_trace/off/trace"
table "$WORK/m_off_trace" 0 "a trace-off control"
grep -q "^| off | off | " "$WORK/out" || fail "trace-off control: control row"
# A plan trace mode that is neither on nor off.
new_matrix "$WORK/m_mode"
make_case "$WORK/m_mode" a
sed -i.orig 's/^a\t1\ton$/a\t1\tmaybe/' "$WORK/m_mode/plan.tsv"
table "$WORK/m_mode" 1 "a plan trace mode that is not on or off"
grep -q "plan.tsv: case a trace 'maybe' (on or off)" "$WORK/err" || fail "mode: reason"
# Results in another order than the plan's.
new_matrix "$WORK/m_order"
make_case "$WORK/m_order" a
make_case "$WORK/m_order" b
{ head -1 "$WORK/m_order/results.tsv"; tail -1 "$WORK/m_order/results.tsv";
  sed -n 2p "$WORK/m_order/results.tsv"; } > "$WORK/m_order/r" && mv "$WORK/m_order/r" "$WORK/m_order/results.tsv"
table "$WORK/m_order" 1 "results in another order than the plan"
grep -q "results.tsv lists the cases in another order than plan.tsv" "$WORK/err" || fail "order: reason"
# A campaign stopped while its second case ran: its directory exists, it has no result.
new_matrix "$WORK/m_begun"
make_case "$WORK/m_begun" a
make_case "$WORK/m_begun" b
sed -i.orig '/^b\t/d' "$WORK/m_begun/results.tsv"
table "$WORK/m_begun" 1 "a campaign stopped during a case"
grep -q "INCOMPLETE campaign: 1 of 2 selected cases ran; no result for b (begun, not finished: b)" \
    "$WORK/err" || fail "begun: reason"
# A plan that selects nothing is refused before results.tsv is read (window_matrix.sh writes none).
mkdir -p "$WORK/m_none_selected"
printf 'case\tselected\ttrace\na\t0\ton\n' > "$WORK/m_none_selected/plan.tsv"
table "$WORK/m_none_selected" 2 "a plan that selects nothing and no results.tsv"
grep -q "plan.tsv selects no case" "$WORK/err" || fail "none selected: reason"
# The aggregator is resolved once: a bare name runs and hashes the bench_picture on PATH, not a
# file of that name in the current directory.
mkdir -p "$WORK/bin" "$WORK/cwd"
cp "$BENCH" "$WORK/bin/bench_picture"
printf '#!/bin/sh\nexit 0\n' > "$WORK/cwd/bench_picture"
chmod +x "$WORK/cwd/bench_picture"
(cd "$WORK/cwd" && PATH="$WORK/bin:$PATH" python3 "$TABLE" bench_picture "$WORK/m_good") > "$WORK/out" 2> "$WORK/err" ||
    fail "a bare bench_picture name: exit $?"
want=$(python3 -c 'import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest())' "$WORK/bin/bench_picture")
grep -q "^Aggregate: .*/bin/bench_picture sha256 $want" "$WORK/out" || fail "bare name: not the file on PATH"
# A case captured with another bench_picture is named.
sed -i.orig 's/^commands=x sha256=0 lines=1$/bench_picture=\/old sha256=00ff\ncommands=x sha256=0 lines=1/' \
    "$WORK/m_good/good/run.txt"
table "$WORK/m_good" 0 "a case captured with another bench_picture"
grep -q "^Captured with another bench_picture than the one aggregating now: good\.$" "$WORK/out" ||
    fail "other bench: not named"
echo "all window table contract checks passed"
