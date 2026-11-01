#!/bin/bash
# window_matrix.sh: the round-ec window matrix (docs/perf/README.md, "The play window, traced"), one
# window at a time: idle, one, burst and sustained at two drawables, a resize run, and the trace
# off/on overhead pairs. macOS or Linux (tools/bench/window_case.sh); needs ANNO1503_GAME_DIR.
#
#   tools/bench/window_matrix.sh <anno1503> <bench_picture> <new out dir> [<large drawable> [<cases>]]
#
# The large drawable defaults to 3456x1944 (the largest 16:9 that fits the round's Mac display); give
# 3840x2160 on a 4K display. <cases> (an extended regular expression over the case names, default
# all) runs a requested subset. plan.tsv, written before the first window, lists every case of the
# matrix and whether it was selected, so the table can tell a requested subset and an unfinished
# campaign from the full matrix (round ed). Then: tools/bench/window_table.py <bench_picture> <out dir>.
set -u
[ $# -ge 3 ] || { sed -n '2,12p' "$0" >&2; exit 2; }
APP=$1; BENCH=$2; M=$3; LARGE=${4:-3456x1944}; SELECT=${5:-.*}
HERE=$(cd "$(dirname "$0")" && pwd)
W="$HERE/window_workloads"
[ ! -e "$M" ] || { echo "window_matrix: $M exists" >&2; exit 2; }
# The cases: name, commands, drawable, seconds, tile, trace, resize (- for none).
CASES=()
for S in 1920x1080 "$LARGE"; do
    CASES+=("idle-$S $W/idle.commands $S 12 1080,444 on -"
            "one-$S $W/one.commands $S 12 1052,444 on -"
            "burst-$S $W/burst.commands $S 14 1080,444 on -"
            "sustained-$S $W/sustained.commands $S 32 1140,444 on -")
done
CASES+=("resize-sustained-1920x1080 $W/sustained.commands 1920x1080 32 1140,444 on 12:1600x900")
for i in 1 2; do
    CASES+=("overhead-off-$i $W/sustained.commands 1920x1080 32 1140,444 off -"
            "overhead-on-$i $W/sustained.commands 1920x1080 32 1140,444 on -")
done
selected() { printf '%s\n' "$1" | grep -Eqx -- "$SELECT"; }
mkdir -p "$M"
N=0
printf 'case\tselected\n' > "$M/plan.tsv"
for c in "${CASES[@]}"; do
    name=${c%% *}
    if selected "$name"; then
        printf '%s\t1\n' "$name" >> "$M/plan.tsv"
        N=$((N + 1))
    else
        printf '%s\t0\n' "$name" >> "$M/plan.tsv"
    fi
done
[ "$N" -gt 0 ] || { echo "window_matrix: '$SELECT' selects no case (see $M/plan.tsv)" >&2; exit 2; }
printf 'case\tstatus\twall_s\tload_after\n' > "$M/results.tsv"
for c in "${CASES[@]}"; do
    read -r name commands size seconds at trace resize <<< "$c"
    selected "$name" || continue
    t0=$(date +%s)
    if [ "$resize" = - ]; then
        bash "$HERE/window_case.sh" "$APP" "$BENCH" "$M/$name" "$commands" "$size" "$seconds" "$at" \
            "$trace" > /dev/null 2>&1
    else
        ANNO_VIEW_RESIZE=$resize bash "$HERE/window_case.sh" "$APP" "$BENCH" "$M/$name" "$commands" \
            "$size" "$seconds" "$at" "$trace" > /dev/null 2>&1
    fi
    rc=$?
    printf '%s\t%s\t%s\t%s\n' "$name" "$rc" "$(($(date +%s) - t0))" \
        "$(uptime | sed 's/.*load averages*: //')" >> "$M/results.tsv"
done
