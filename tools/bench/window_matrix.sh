#!/bin/bash
# window_matrix.sh: the round-ec window matrix (docs/perf/README.md, "The play window, traced"), one
# window at a time: idle, one, burst and sustained at two drawables, a resize run, and the trace
# off/on overhead pairs. macOS or Linux (tools/bench/window_case.sh); needs ANNO1503_GAME_DIR.
#
#   tools/bench/window_matrix.sh <anno1503> <bench_picture> <new out dir> [<large drawable>]
#
# The large drawable defaults to 3456x1944 (the largest 16:9 that fits the round's Mac display); give
# 3840x2160 on a 4K display. Then: tools/bench/window_table.py <out dir>.
set -u
[ $# -ge 3 ] || { sed -n '2,10p' "$0" >&2; exit 2; }
APP=$1; BENCH=$2; M=$3; LARGE=${4:-3456x1944}
HERE=$(cd "$(dirname "$0")" && pwd)
W="$HERE/window_workloads"
[ ! -e "$M" ] || { echo "window_matrix: $M exists" >&2; exit 2; }
mkdir -p "$M"
printf 'case\tstatus\twall_s\tload_after\n' > "$M/results.tsv"
run() { # name commands size seconds at [trace]
    local t0
    t0=$(date +%s)
    bash "$HERE/window_case.sh" "$APP" "$BENCH" "$M/$1" "$2" "$3" "$4" "$5" "${6:-on}" > /dev/null 2>&1
    local rc=$?
    printf '%s\t%s\t%s\t%s\n' "$1" "$rc" "$(($(date +%s) - t0))" \
        "$(uptime | sed 's/.*load averages*: //')" >> "$M/results.tsv"
}
for S in 1920x1080 "$LARGE"; do
    run "idle-$S" "$W/idle.commands" "$S" 12 1080,444
    run "one-$S" "$W/one.commands" "$S" 12 1052,444
    run "burst-$S" "$W/burst.commands" "$S" 14 1080,444
    run "sustained-$S" "$W/sustained.commands" "$S" 32 1140,444
done
ANNO_VIEW_RESIZE=12:1600x900 run resize-sustained-1920x1080 "$W/sustained.commands" 1920x1080 32 1140,444
for i in 1 2; do
    run "overhead-off-$i" "$W/sustained.commands" 1920x1080 32 1140,444 off
    run "overhead-on-$i" "$W/sustained.commands" 1920x1080 32 1140,444 on
done
