#!/bin/bash
# picture_matrix.sh: runs bench_picture cases one after another (docs/perf/README.md, "Picture on a
# legal city workload").
#
#   tools/bench/picture_matrix.sh <bench_picture binary> <game_dir> <out_dir> <cases_file>
#
# The cases file has one case per line, `name option...` (blank lines and lines starting with #
# are skipped). Each case runs alone, in <out_dir>/<name> (its raw files, stdout.txt, stderr.txt).
# The runner stops at the first case that does not exit 0, keeps that case's artifacts, and exits
# with the case's own status (1 verification failed, 3 the drain did not converge, 4 the workload or
# scene could not be built, 2 bad options); a failing case is never followed by an echo that ends
# the run successfully. results.tsv lists every case run with its status and wall seconds;
# provenance.txt records the source revision, the binary's hash and the host.
set -u
if [ $# -ne 4 ]; then
    echo "usage: $0 <bench_picture binary> <game_dir> <out_dir> <cases_file>" >&2
    exit 2
fi
BIN=$1
GAME=$2
OUT=$3
CASES=$4
HERE=$(cd "$(dirname "$0")/../.." && pwd)
if [ ! -x "$BIN" ] || [ ! -f "$CASES" ]; then
    echo "picture_matrix: $BIN is not executable or $CASES is missing" >&2
    exit 2
fi
mkdir -p "$OUT" || exit 2
{
    echo "started=$(date '+%Y-%m-%d %H:%M:%S %z')"
    echo "revision=$(git -C "$HERE" rev-parse HEAD) dirty_diff_sha=$(git -C "$HERE" diff HEAD | shasum | cut -c1-12)"
    echo "binary=$BIN sha256=$(shasum -a 256 "$BIN" | cut -d' ' -f1)"
    echo "cases_file=$CASES sha256=$(shasum -a 256 "$CASES" | cut -d' ' -f1)"
    echo "game_dir=$GAME"
    echo "host=$(uname -srm)"
    if command -v sysctl > /dev/null; then
        echo "cpu=$(sysctl -n machdep.cpu.brand_string 2> /dev/null) cores=$(sysctl -n hw.ncpu 2> /dev/null) memory=$(sysctl -n hw.memsize 2> /dev/null)"
    fi
    if command -v memory_pressure > /dev/null; then
        echo "memory_free_percent_at_start=$(memory_pressure 2> /dev/null | sed -n 's/.*free percentage: //p')"
    fi
} > "$OUT/provenance.txt"
printf 'case\tstatus\twall_s\n' > "$OUT/results.tsv"
while read -r name args; do
    if [ -z "$name" ] || [ "${name#\#}" != "$name" ]; then
        continue
    fi
    dir="$OUT/$name"
    mkdir -p "$dir" || exit 2
    start=$(date +%s)
    # shellcheck disable=SC2086 # the case's options are words
    "$BIN" "$GAME" --out "$dir" $args > "$dir/stdout.txt" 2> "$dir/stderr.txt"
    rc=$?
    printf '%s\t%s\t%s\n' "$name" "$rc" "$(($(date +%s) - start))" >> "$OUT/results.tsv"
    if [ "$rc" -ne 0 ]; then
        echo "picture_matrix: case $name exited $rc; artifacts in $dir" >&2
        exit "$rc"
    fi
done < "$CASES"
echo "finished=$(date '+%Y-%m-%d %H:%M:%S %z')" >> "$OUT/provenance.txt"
