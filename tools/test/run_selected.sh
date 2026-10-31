#!/usr/bin/env bash
# Compatibility entry point. The portable Python gate owns all verdict rules.
set -eu
if [ "$#" -lt 5 ]; then
    echo 'usage: run_selected.sh <build-dir> <target|-> <regex> <timeout-s> <new-log> [jobs] [gate options...]' >&2
    exit 2
fi
PATH="${ANNO_TOOLS_BIN:+$ANNO_TOOLS_BIN:}$PATH"
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=$1; TARGET=$2; RE=$3; TMO=$4; LOG=$5
shift 5
JOBS=${1:-1}
if [ "$#" -gt 0 ]; then shift; fi
args=(--build-dir "$BUILD" --select "$RE" --timeout "$TMO" --log "$LOG" --jobs "$JOBS")
if [ "$TARGET" != - ]; then args+=(--target "$TARGET"); fi
exec python3 -I "$HERE/run_selected.py" "${args[@]}" "$@"
