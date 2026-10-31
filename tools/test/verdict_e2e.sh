#!/bin/bash
# verdict_e2e.sh: the verdict reader against REAL corpus tests, end to end through run_selected.sh.
#
#   tools/test/verdict_e2e.sh <build-dir> <out-dir>
#
# 1. ANNO1503_GAME_DIR absent: the two Bigbrother corpus cases print "ANNO1503_GAME_DIR not set:
#    skipped" and pass in CTest. The run must exit 6 with executed=0 and required-asset=2.
# 2. ANNO1503_GAME_DIR set (taken from this script's environment; the step is reported as not run
#    when it is unset): the same cases must execute: exit 0, executed=2, skipped=0.
# Not run by CTest (it would start CTest inside CTest); the unit contract is test_verdict_contract.
set -u
BUILD=$1; OUT=$2; HERE=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$OUT"; GAME=${ANNO1503_GAME_DIR:-}; RC=0
env -u ANNO1503_GAME_DIR "$HERE/tools/test/run_selected.sh" "$BUILD" - 'bigbrother corpus' 300 "$OUT/e2e-absent.log" 2 > "$OUT/e2e-absent.txt" 2>&1
A=$?; tail -2 "$OUT/e2e-absent.txt"
if [ "$A" -ne 6 ] || ! grep -q 'executed=0 .*required-asset=2' "$OUT/e2e-absent.txt"; then echo "E2E FAIL (absent): exit $A"; RC=1; else echo "E2E ok (absent): exit 6, executed=0, required-asset=2"; fi
if [ -n "$GAME" ]; then
    ANNO1503_GAME_DIR=$GAME "$HERE/tools/test/run_selected.sh" "$BUILD" - 'bigbrother corpus' 300 "$OUT/e2e-present.log" 2 > "$OUT/e2e-present.txt" 2>&1
    P=$?; tail -2 "$OUT/e2e-present.txt"
    if [ "$P" -ne 0 ] || ! grep -q 'executed=2 .*skipped=0' "$OUT/e2e-present.txt"; then echo "E2E FAIL (present): exit $P"; RC=1; else echo "E2E ok (present): exit 0, executed=2, skipped=0"; fi
else
    echo "E2E (present): NOT RUN, ANNO1503_GAME_DIR is unset"
fi
exit $RC
