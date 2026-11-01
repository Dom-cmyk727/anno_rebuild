#!/bin/bash
# window_case.sh: one scripted, traced play window (round ec, D; docs/app/window-trace.md).
#
#   tools/bench/window_case.sh <anno1503> <bench_picture> <out dir> <commands file> <w>x<h> \
#       <seconds> <tile x>,<tile y> [trace on|off]
#
# Runs `anno1503 --play data/maps/lan5.txt --commands <file> --renderer gpu` for <seconds>, asking for
# a drawable of <w>x<h> pixels and centring the camera on the tile. With the trace on (default) the
# window writes <out>/trace (events, frames, sim, manifest, summary) and the trace is aggregated
# (<out>/aggregate.txt, its exit status in run.txt). With it off, only the window's own stderr lines
# remain (the paired overhead control). Provenance goes to <out>/run.txt first: host, adapter (from
# the run), binary and file hashes, revision and tree state, environment. Needs ANNO1503_GAME_DIR.
# macOS or Linux (bash, perl, /usr/bin/time, shasum); not Windows, where the window's own trace
# (ANNO_VIEW_TRACE) and bench_picture --aggregate work but this runner does not.
# Exit: the window's status, or 2 for bad arguments; the aggregate's status is recorded, not
# returned (window_table.py reads it).
set -u
[ $# -ge 7 ] || { sed -n '2,15p' "$0" >&2; exit 2; }
APP=$1; BENCH=$2; OUT=$3; COMMANDS=$4; SIZE=$5; SECONDS_=$6; AT=$7; TRACE=${8:-on}
HERE=$(cd "$(dirname "$0")/../.." && pwd)
MAP="$HERE/data/maps/lan5.txt"
[ -n "${ANNO1503_GAME_DIR:-}" ] || { echo "window_case: ANNO1503_GAME_DIR is not set" >&2; exit 2; }
[ ! -e "$OUT" ] || { echo "window_case: $OUT exists (use a new directory)" >&2; exit 2; }
mkdir -p "$OUT"
{
    echo "window_case started=$(date '+%Y-%m-%d %H:%M:%S')"
    if [ "$(uname)" = Darwin ]; then
        echo "host model=$(sysctl -n hw.model 2>/dev/null) cpu=\"$(sysctl -n machdep.cpu.brand_string 2>/dev/null)\" cores=$(sysctl -n hw.ncpu 2>/dev/null) mem_bytes=$(sysctl -n hw.memsize 2>/dev/null) os=\"$(sw_vers -productVersion 2>/dev/null) $(uname -r)\""
    else
        echo "host model=$(cat /sys/devices/virtual/dmi/id/product_name 2>/dev/null) cpu=\"$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //')\" cores=$(nproc) mem_bytes=$(awk '/MemTotal/ {print $2 * 1024}' /proc/meminfo) os=\"$(uname -sr)\""
    fi
    echo "load_before=\"$(uptime | sed 's/.*load averages*: //')\""
    # The tree's state: tracked changes and untracked files (their names and contents) both count;
    # clean=yes only when there are neither (round ec review: untracked sources were not counted).
    DIFF=$(git -C "$HERE" diff HEAD)
    UNTRACKED=$(cd "$HERE" && git ls-files --others --exclude-standard)
    STATE=$( { printf '%s' "$DIFF"; [ -z "$UNTRACKED" ] || (cd "$HERE" && printf '%s\n' "$UNTRACKED" | tr '\n' '\0' | xargs -0 shasum -a 256); } | shasum | cut -c1-12)
    CLEAN=no
    [ -z "$DIFF" ] && [ -z "$UNTRACKED" ] && CLEAN=yes
    echo "revision=$(git -C "$HERE" rev-parse HEAD) clean=$CLEAN state=$STATE untracked=$(printf '%s' "$UNTRACKED" | grep -c . || true)"
    echo "anno1503=$APP sha256=$(shasum -a 256 "$APP" | cut -c1-64)"
    echo "bench_picture=$BENCH sha256=$(shasum -a 256 "$BENCH" | cut -c1-64)"
    echo "map=$MAP sha256=$(shasum -a 256 "$MAP" | cut -c1-64)"
    echo "commands=$COMMANDS sha256=$(shasum -a 256 "$COMMANDS" | cut -c1-64) lines=$(wc -l < "$COMMANDS" | tr -d ' ')"
    echo "drawable_asked=$SIZE seconds=$SECONDS_ at=$AT trace=$TRACE"
    echo "env ANNO_VIEW_LIMITS=${ANNO_VIEW_LIMITS:-interactive} ANNO_VIEW_GPU_CHUNKS=${ANNO_VIEW_GPU_CHUNKS:-off} ANNO_GPU_ADAPTER=${ANNO_GPU_ADAPTER:-default} ANNO_VIEW_RESIZE=${ANNO_VIEW_RESIZE:-none}"
} > "$OUT/run.txt"
TRACE_ENV=()
[ "$TRACE" = on ] && TRACE_ENV=(ANNO_VIEW_TRACE="$OUT/trace")
# /usr/bin/time: the process's peak resident set (time.txt; -l on macOS, GNU -v on Linux); perl's
# alarm bounds a hung window.
TIME_FLAG=-l
[ "$(uname)" = Darwin ] || TIME_FLAG=-v
env ${TRACE_ENV[@]+"${TRACE_ENV[@]}"} ANNO_VIEW_SIZE="$SIZE" ANNO_VIEW_AT="$AT" \
    ANNO_VIEW_QUIT_S="$SECONDS_" /usr/bin/time "$TIME_FLAG" -o "$OUT/time.txt" \
    perl -e 'alarm shift; exec @ARGV' $((SECONDS_ + 120)) \
    "$APP" --play "$MAP" --commands "$COMMANDS" --renderer gpu > "$OUT/stdout.txt" 2> "$OUT/stderr.txt"
RC=$?
echo "window_exit=$RC ended=$(date '+%Y-%m-%d %H:%M:%S') load_after=\"$(uptime | sed 's/.*load averages*: //')\"" >> "$OUT/run.txt"
if [ "$TRACE" = on ]; then
    "$BENCH" --aggregate "$OUT/trace/events.tsv" > "$OUT/aggregate.txt" 2> "$OUT/aggregate.stderr"
    echo "aggregate_exit=$?" >> "$OUT/run.txt"
fi
exit $RC
