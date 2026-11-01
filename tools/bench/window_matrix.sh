#!/bin/bash
# window_matrix.sh: the round-ec window matrix (docs/perf/README.md, "The play window, traced"), one
# window at a time: idle, one, burst and sustained at two drawables, a resize run, and the trace
# off/on overhead pairs. macOS or Linux (tools/bench/window_case.sh); needs ANNO1503_GAME_DIR.
#
#   tools/bench/window_matrix.sh <anno1503> <bench_picture> <new out dir> [<large drawable> [<cases>]]
#
# The large drawable defaults to 3456x1944 (the largest 16:9 that fits the round's Mac display); give
# 3840x2160 on a 4K display. <cases> (an extended regular expression that must match a whole case
# name, default all: `one-.*` selects both one cases, `one` none) runs a requested subset. plan.tsv,
# written before the first window, lists every case of the matrix, whether it was selected and its
# trace mode, so the table can tell a requested subset and an unfinished campaign from the full
# matrix (round ed). Then: tools/bench/window_table.py <bench_picture> <out dir>.
set -u
[ $# -ge 3 ] || { sed -n '2,13p' "$0" >&2; exit 2; }
APP=$1; BENCH=$2; M=$3; LARGE=${4:-3456x1944}; SELECT=${5:-.*}
HERE=$(cd "$(dirname "$0")" && pwd)
W="$HERE/window_workloads"
[ ! -e "$M" ] || { echo "window_matrix: $M exists" >&2; exit 2; }
printf '' | grep -Eq -- "$SELECT" 2> /dev/null
[ $? -ne 2 ] || { echo "window_matrix: '$SELECT' is not a regular expression" >&2; exit 2; }
# The cases, tab-separated (a path may hold spaces): name, commands, drawable, seconds, tile,
# trace, resize (- for none).
T=$'\t'
CASES=()
for S in 1920x1080 "$LARGE"; do
    CASES+=("idle-$S$T$W/idle.commands$T$S${T}12${T}1080,444${T}on$T-"
            "one-$S$T$W/one.commands$T$S${T}12${T}1052,444${T}on$T-"
            "burst-$S$T$W/burst.commands$T$S${T}14${T}1080,444${T}on$T-"
            "sustained-$S$T$W/sustained.commands$T$S${T}32${T}1140,444${T}on$T-")
done
CASES+=("resize-sustained-1920x1080$T$W/sustained.commands${T}1920x1080${T}32${T}1140,444${T}on${T}12:1600x900")
for i in 1 2; do
    CASES+=("overhead-off-$i$T$W/sustained.commands${T}1920x1080${T}32${T}1140,444${T}off$T-"
            "overhead-on-$i$T$W/sustained.commands${T}1920x1080${T}32${T}1140,444${T}on$T-")
done
selected() { printf '%s\n' "$1" | grep -Eqx -- "$SELECT"; }
N=0
for c in "${CASES[@]}"; do
    selected "${c%%$T*}" && N=$((N + 1))
done
[ "$N" -gt 0 ] || { echo "window_matrix: '$SELECT' selects no case" >&2; exit 2; }
mkdir -p "$M"
printf 'case\tselected\ttrace\n' > "$M/plan.tsv"
for c in "${CASES[@]}"; do
    IFS=$T read -r name commands size seconds at trace resize <<< "$c"
    printf '%s\t%s\t%s\n' "$name" "$(selected "$name" && echo 1 || echo 0)" "$trace" >> "$M/plan.tsv"
done
printf 'case\tstatus\twall_s\tload_after\n' > "$M/results.tsv"
for c in "${CASES[@]}"; do
    IFS=$T read -r name commands size seconds at trace resize <<< "$c"
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
